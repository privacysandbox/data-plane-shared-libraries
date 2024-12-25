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
	"archive/zip"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path"
	"path/filepath"
	"strings"

	bazeliskCore "github.com/bazelbuild/bazelisk/core"
	bazeliskRepos "github.com/bazelbuild/bazelisk/repositories"
	sutV1Pb "github.com/privacysandbox/functionaltest-system/sut/v1/pb"
)

type BazelTestError struct {
	err string
}

func (e BazelTestError) Error() string {
	return fmt.Sprintf("%v", e.err)
}

var (
	ErrTargetsUnspecified = errTargetsUnspecified()
	ErrInvalidLabel       = errInvalidLabel()
	ErrInvalidArgs        = errInvalidArgs()
	ErrNotDirectory       = errNotDirectory()
	ErrIsDirectory        = errIsDirectory()
	ErrExist              = errExist()
)

func errTargetsUnspecified() error {
	return BazelTestError{err: "bazel test targets unspecified"}
}

func errInvalidLabel() error {
	return BazelTestError{err: "invalid label"}
}

func errInvalidArgs() error {
	return BazelTestError{err: "invalid args"}
}

func errNotDirectory() error {
	return BazelTestError{err: "directory does not exist"}
}

func errIsDirectory() error {
	return BazelTestError{err: "path is an existing directory"}
}

func errExist() error {
	return os.ErrExist
}

func stringValFilter(strs []string, pred func(string) bool) (retval []string) {
	for _, str := range strs {
		if pred(str) {
			retval = append(retval, str)
		}
	}
	return
}

func stringValPredicate(s string) bool {
	s = strings.TrimSpace(s)
	if len(s) == 0 {
		return false
	}
	return true
}

func configureBazelisk() error {
	// Configure bazelisk's home path to be within the sut work dir, to ensure
	// hermeticity of the each execution.
	bazelHomeDir := path.Join(sutWorkdir, ".bazelhome")
	bazeliskHomeDir := path.Join(bazelHomeDir, "bazelisk")
	if err := os.MkdirAll(bazeliskHomeDir, 0755); err != nil {
		return err
	}
	os.Setenv("BAZELISK_HOME", bazeliskHomeDir)
	// per https://github.com/bazelbuild/bazel/issues/16937, bazel does not
	// respect XDG_CACHE_HOME prior to 7.2.0rc1, as fixed in github PR:
	// https://github.com/bazelbuild/bazel/pull/21817. In the meantime,
	// override the HOME variable
	// os.Setenv("XDG_CACHE_HOME", bazelHomeDir)
	os.Setenv("HOME", bazelHomeDir)
	sutTmpDir := path.Join(sutWorkdir, "tmp")
	if err := os.MkdirAll(sutTmpDir, 0755); err != nil {
		return err
	}
	os.Setenv("TMPDIR", sutTmpDir)
	return nil
}

func runBazelTest(bazelTestCmd *sutV1Pb.BazelTest, label string) (exitCode int, err error) {
	normalizeLabel := func(lbl string) string {
		return strings.TrimSpace(lbl)
	}
	label = normalizeLabel(label)
	if len(label) == 0 {
		err = ErrInvalidLabel
		return
	}
	outputBase := path.Join(sutWorkdir, "bazel-") + label
	bazelArgs := []string{
		"--output_base",
		outputBase,
		// "--output_user_base",
		// path.Join(sutWorkdir, ".cache/bazel"),
		"test",
	}
	testTargets := stringValFilter(bazelTestCmd.TestTargets, stringValPredicate)
	if len(testTargets) == 0 {
		err = ErrTargetsUnspecified
		return
	}
	bazelArgs = append(bazelArgs, testTargets...)
	exitCode, err = runBazelArgs(bazelArgs)
	if err != nil {
		return
	}
	err = captureBazelLogs("bazel-testlogs", path.Join(sutWorkdir, label+"-logs.zip"))
	return
}

func runBazelArgs(args []string) (exitCode int, err error) {
	args = stringValFilter(args, stringValPredicate)
	if len(args) == 0 {
		err = ErrInvalidArgs
		return
	}
	gcs := &bazeliskRepos.GCSRepo{}
	config := bazeliskCore.MakeDefaultConfig()
	gitHub := bazeliskRepos.CreateGitHubRepo(config.Get("BAZELISK_GITHUB_TOKEN"))
	// Fetch LTS releases, release candidates, rolling releases and Bazel-at-commits from GCS, forks from GitHub.
	repos := bazeliskCore.CreateRepositories(gcs, gcs, gitHub, gcs, gcs, true)
	fmt.Println("bazel commandline:", args)
	exitCode, err = bazeliskCore.RunBazeliskWithArgsFuncAndConfig(func(_ string) []string { return args }, repos, config)
	return
}

func findBazelLogFiles(logRoot string) (paths []string, err error) {
	logFS := os.DirFS(logRoot)
	fs.WalkDir(logFS, ".", func(path string, d fs.DirEntry, e error) error {
		if e != nil {
			err = e
			return err
		}
		f := d.Name()
		if f == "test.log" || f == "test.xml" {
			paths = append(paths, path)
		}
		return nil
	})
	return
}

func captureBazelLogs(logRoot, archiveFilename string) (err error) {
	if len(logRoot) == 0 || len(archiveFilename) == 0 {
		err = ErrInvalidArgs
		return
	}
	logRoot = filepath.Clean(logRoot)
	archiveFilename = filepath.Clean(archiveFilename)
	if verbose {
		fmt.Printf("Capturing bazel logs. logRoot: %s, archiveFilename: %s\n", logRoot, archiveFilename)
	}
	if stat, _err := os.Stat(logRoot); _err != nil || !stat.IsDir() {
		err = ErrNotDirectory
		return
	}
	if stat, _err := os.Stat(archiveFilename); _err == nil && stat.IsDir() {
		err = ErrIsDirectory
		return
	}
	archive, aErr := os.Create(archiveFilename)
	if aErr != nil {
		err = aErr
		return
	}
	defer archive.Close()
	writer := zip.NewWriter(archive)
	defer writer.Close()

	origWorkdir, _ := os.Getwd()
	os.Chdir(logRoot)
	defer os.Chdir(origWorkdir)
	d, _ := os.Getwd()
	if verbose {
		fmt.Println("working dir:", d)
	}
	filepaths, e := findBazelLogFiles(".")
	if e != nil {
		err = e
		return
	}
	for _, file := range filepaths {
		f, fErr := os.Open(file)
		if fErr != nil {
			err = fErr
			return
		}
		defer f.Close()
		w, wErr := writer.Create(file)
		if err != nil {
			err = wErr
			return
		}
		if _, e := io.Copy(w, f); e != nil {
			err = e
			return
		}
	}
	return
}
