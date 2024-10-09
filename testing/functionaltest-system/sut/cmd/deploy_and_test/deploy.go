// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package deploy_and_test

import (
	"bytes"
	"embed"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"strconv"

	sutV1Pb "github.com/privacysandbox/functionaltest-system/sut/v1/pb"
	"github.com/spf13/cobra"
	"google.golang.org/protobuf/encoding/prototext"
)

const testRunnerFilename = "testrunner.textproto"

var (
	sutWorkdir      string
	sutZipFile      string
	sutDockerImages []string
	sutDir          string
	sutName         string
	verbose         bool = false
)

//go:embed embedFS.zip
var sutEmbedFS embed.FS

var DockerDeploySutCmd = &cobra.Command{
	Use:   "deploy-and-test",
	Short: "Deploy System-Under-Test commands",
	Long: `Collection of utilities related to functional testing for the
Privacy Sandbox servers.`,
	RunE: func(cmd *cobra.Command, args []string) (err error) {
		if err = deploy(); err != nil {
			log.Fatalln("deploy-sut", err)
		}
		return
	},
}

func copyToDir(srcFS fs.FS, destDir string) error {
	return fs.WalkDir(srcFS, ".", func(filePath string, d fs.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if !d.Type().IsDir() {
			_destDir := path.Join(destDir, path.Dir(filePath))
			if dirErr := os.MkdirAll(_destDir, 0755); dirErr != nil {
				return dirErr
			}
			len, err := copyFile(srcFS, filePath, path.Join(_destDir, d.Name()))
			if err != nil {
				return err
			}
			if verbose {
				fmt.Printf("copyToDir: %s [%d bytes]\n", filePath, len)
			}
		}
		return nil
	})
}

func copyFile(srcFS fs.FS, srcPath string, destPath string) (int64, error) {
	if srcStatFS, ok := srcFS.(fs.StatFS); ok {
		if srcStat, err := srcStatFS.Stat(srcPath); err != nil {
			fmt.Printf("unable to stat %s, assuming it's a file path\n", srcPath)
			//return 0, err
		} else if !srcStat.Mode().IsRegular() {
			fmt.Printf("%s is not a regular file, skipping\n", srcPath)
			return 0, nil
		}
	}
	source, err := srcFS.Open(srcPath)
	if err != nil {
		return 0, err
	}
	defer source.Close()
	destination, err := os.Create(destPath)
	if err != nil {
		return 0, err
	}
	sourceFileInfo, err := source.Stat()
	if err != nil {
		return 0, err
	}
	if err = destination.Chmod(sourceFileInfo.Mode()); err != nil {
		return 0, err
	}
	defer destination.Close()
	return io.Copy(destination, source)
}

func listFS(srcFS fs.FS) error {
	if verbose {
		fmt.Printf("file listing\n")
	}
	return fs.WalkDir(srcFS, ".", func(path string, d fs.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if verbose && !d.Type().IsDir() {
			if info, e := d.Info(); e != nil {
				fmt.Printf("%s [unknown]\n", path)
				return e
			} else {
				fmt.Printf("%s [%d]\n", path, info.Size())
			}
		}
		return nil
	})
}

type StageError struct {
	err string
}

func (e StageError) Error() string {
	return fmt.Sprintf("%v", e.err)
}

func shellCommandToCmd(shellCommand *sutV1Pb.ShellCommand) (cmd *exec.Cmd, err error) {
	if len(shellCommand.Command) == 0 {
		err = StageError{err: "command not specified"}
		return
	}
	if len(shellCommand.Command[0]) == 0 {
		err = StageError{err: "executable not specified"}
		return
	}
	cmd = exec.Command(shellCommand.Command[0], shellCommand.Command[1:]...)
	return
}

func runCmd(cmd *exec.Cmd) (err error) {
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err = cmd.Run()
	return
}

func runStage(stage *sutV1Pb.Stage) (err error) {
	switch x := stage.Command.Command.(type) {
	case *sutV1Pb.Command_Shell:
		fmt.Printf(" Shell command: %v\n", stage.Command.Command)
		cmd, _err := shellCommandToCmd(stage.Command.GetShell())
		if _err != nil {
			err = _err
			return
		}
		err = runCmd(cmd)
		break
	case *sutV1Pb.Command_Bazel:
		fmt.Printf(" Bazel command: %v\n", stage.Command.Command)
		exitCode, e := runBazelTest(stage.Command.GetBazel(), stage.Label)
		if e != nil {
			return
		}
		if exitCode != 0 {
			err = fmt.Errorf("bazel non-zero exit code: %d", exitCode)
		}
		break
	default:
		err = fmt.Errorf("unsupported command type: %T", x)
	}
	return
}

// Generic file errors.
// Errors returned by file systems can be tested against these errors
// using [errors.Is].
var (
	ErrInvalid    = fs.ErrInvalid
	ErrPermission = fs.ErrPermission
	ErrNotExist   = fs.ErrNotExist
)

func readTestRunner(runnerConfigFile string) (tr *sutV1Pb.TestRunner, err error) {
	if len(runnerConfigFile) == 0 {
		err = ErrInvalid
		return
	}
	if verbose {
		fmt.Println("reading textproto from file:", runnerConfigFile)
	}
	runnerConfig, err := os.ReadFile(runnerConfigFile)
	if err != nil {
		// err = fmt.Errorf("reading file: %s -- %w", runnerConfigFile, err)
		return
	}
	tr = &sutV1Pb.TestRunner{}
	if err = prototext.Unmarshal(runnerConfig, tr); err != nil {
		log.Fatalln("Failed to parse test runner proto:", err)
		err = fmt.Errorf("parsing test runner proto: %s -- %w", runnerConfigFile, err)
		return
	}
	// assign a label if not specified
	for i, stage := range tr.Stages {
		if len(stage.Label) == 0 {
			stage.Label = "stage" + strconv.FormatInt(int64(i+1), 10)
		}
	}
	return
}

func extractEmbeddedFS() (err error) {
	zipBytes, e := fs.ReadFile(sutEmbedFS, "embedFS.zip")
	if e != nil {
		err = e
		return
	}
	if verbose {
		fmt.Println("embedFS.zip size:", len(zipBytes))
	}
	embedReader := bytes.NewReader(zipBytes)
	if err = unzipToDir(embedReader, int64(len(zipBytes)), "", sutWorkdir); err != nil {
		err = fmt.Errorf("unzipping embedFS.zip -- %w", err)
	}
	return
}

func deploy() (err error) {
	// sutWorkdir is a temp dir into which embedded sut files and
	// workdir/zip content are copied
	if sutWorkdir, err = os.MkdirTemp("", "sut"); err != nil {
		return
	}
	//defer os.RemoveAll(sutWorkdir)
	if verbose {
		fmt.Println("sut workdir:", sutWorkdir)
	}
	if err = extractEmbeddedFS(); err != nil {
		err = fmt.Errorf("extracting embedFS.zip -- %w", err)
		return
	}

	sutWorkFS := os.DirFS(sutWorkdir)
	if err = DockerLoad(sutWorkFS, "test-tools.tar"); err != nil {
		err = fmt.Errorf("docker load test-tools image -- %w", err)
		return
	}

	if len(sutDir) > 0 {
		if _, err = os.Stat(sutDir); errors.Is(err, os.ErrNotExist) {
			return
		}
		if err = copyToDir(os.DirFS(sutDir), sutWorkdir); err != nil {
			return
		}
	} else if len(sutZipFile) > 0 {
		zipFile, e := os.Open(sutZipFile)
		if e != nil {
			err = e
			return
		}
		defer zipFile.Close()
		s, e := zipFile.Stat()
		if e != nil {
			err = e
			return
		}
		var basedir string
		if basedir, err = findFileInZip(zipFile, s.Size(), "testrunner.textproto"); err != nil {
			err = fmt.Errorf("searching %s -- %w", sutZipFile, err)
			return
		}
		if err = unzipToDir(zipFile, s.Size(), basedir, sutWorkdir); err != nil {
			err = fmt.Errorf("unzipping %s to %s -- %w", sutZipFile, sutWorkdir, err)
			return
		}
	}
	if len(sutDockerImages) > 0 {
		if verbose {
			fmt.Printf("copying %d images to docker_images/\n", len(sutDockerImages))
		}
		const rootPath = "/"
		srcFS := os.DirFS(rootPath)
		destDir := path.Join(sutWorkdir, "docker_images")
		if err = os.MkdirAll(destDir, 0755); err != nil {
			return
		}
		for _, fPath := range sutDockerImages {
			if fPath, err = filepath.Abs(fPath); err != nil {
				return
			}
			if fPath, err = filepath.Rel(rootPath, fPath); err != nil {
				return
			}
			fName := path.Base(fPath)
			destPath := path.Join(destDir, fName)
			if verbose {
				fmt.Printf("copying docker image [%s] to [%s]\n", fPath, destPath)
			}
			if _, err = copyFile(srcFS, fPath, destPath); err != nil {
				return
			}
		}
	}
	if verbose {
		listFS(os.DirFS(sutWorkdir))
	}
	if err = configureBazelisk(); err != nil {
		return
	}
	runnerConfigFile := filepath.Join(sutWorkdir, testRunnerFilename)
	tr, e := readTestRunner(runnerConfigFile)
	if e != nil {
		err = e
		return
	}
	sutName = tr.Name
	if verbose {
		fmt.Println("sut:", sutName)
	}

	os.Chdir(sutWorkdir)
	if sutWorkdir, err = os.Getwd(); err != nil {
		return
	}
	totalStageCount := len(tr.Stages)
	exitCode, e := runBazelArgs([]string{"version"})
	if e != nil {
		err = e
		return
	}
	if exitCode != 0 {
		err = fmt.Errorf("bazel non-zero exit code: %d", exitCode)
		return
	}
	if len(tr.Deploy.Compose.YamlFilename) > 0 {
		dc, dcErr := newDockerCompose(tr.Deploy.Compose)
		if dcErr != nil {
			err = dcErr
			return
		}
		if err = dc.Up(); err != nil {
			return
		}
		defer dc.Down()
	}
	for i, stage := range tr.Stages {
		fmt.Printf("\nstage %d/%d", i+1, totalStageCount)
		if err = runStage(stage); err != nil {
			return
		}
	}
	return
}

func init() {
	DockerDeploySutCmd.Flags().BoolVar(&verbose, "verbose", false, "verbose output")
	DockerDeploySutCmd.Flags().StringVar(&sutZipFile, "sut-zip", "", "zip archive containing the data and configs for the SUT")
	DockerDeploySutCmd.Flags().StringArrayVar(&sutDockerImages, "docker-image", []string{}, "docker image archive for use in the SUT, copied to the docker_images/ SUT dir")
	DockerDeploySutCmd.Flags().StringVar(&sutDir, "sut-dir", "", "directory containing the data and configs for the SUT")
	DockerDeploySutCmd.Flags().MarkDeprecated("sut-dir", "prefer use of sut-zip")
	DockerDeploySutCmd.MarkFlagsOneRequired("sut-zip", "sut-dir")
	DockerDeploySutCmd.MarkFlagsMutuallyExclusive("sut-zip", "sut-dir")
}
