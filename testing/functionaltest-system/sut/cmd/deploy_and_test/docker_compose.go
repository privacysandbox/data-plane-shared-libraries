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
	"context"
	"encoding/json"
	"fmt"
	"io"
	"io/fs"
	"os"
	"os/exec"
	"path"
	"time"

	dockerContainer "github.com/docker/docker/api/types/container"
	dockerImage "github.com/docker/docker/api/types/image"
	dockerClient "github.com/docker/docker/client"
	"github.com/docker/docker/pkg/jsonmessage"
	sutV1Pb "github.com/privacysandbox/functionaltest-system/sut/v1/pb"
)

type DockerCompose struct {
	Compose  *sutV1Pb.DockerCompose
	Stdout   io.Writer
	Stderr   io.Writer
	EventLog io.Writer

	cmdEvents *exec.Cmd
	cmdUp     *exec.Cmd
}

func newDockerCompose(compose *sutV1Pb.DockerCompose) (*DockerCompose, error) {
	composeLogOut, _eOut := os.OpenFile(path.Join(sutWorkdir, "docker-compose-log.out"), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if _eOut != nil {
		return nil, _eOut
	}
	composeLogErr, _eErr := os.OpenFile(path.Join(sutWorkdir, "docker-compose-log.err"), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if _eErr != nil {
		return nil, _eErr
	}
	composeEvents, _eErr := os.OpenFile(path.Join(sutWorkdir, "docker-compose-events.json"), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644)
	if _eErr != nil {
		return nil, _eErr
	}
	return &DockerCompose{
		Compose:  compose,
		Stdout:   composeLogOut,
		Stderr:   composeLogErr,
		EventLog: composeEvents,
	}, nil
}

func (d *DockerCompose) Validate() (err error) {
	cmd := exec.Command("docker", "compose", "version", "--short")
	fmt.Println("command:", cmd)
	runCmd(cmd)
	// 2.17.2
	return
}

func (d *DockerCompose) createCmd(commandArgs []string) (cmd *exec.Cmd, err error) {
	composeArgs := []string{
		"compose",
		"--file",
		d.Compose.YamlFilename,
	}
	for _, envFilename := range d.Compose.EnvFilenames {
		composeArgs = append(composeArgs,
			"--env-file",
			envFilename,
		)
	}
	cmd = exec.Command("docker", append(composeArgs, commandArgs...)...)
	cmd.Env = []string{
		"SUT_DATA_DIR=" + sutWorkdir + "/" + "sut_data",
	}
	cmd.Stdout = d.Stdout
	cmd.Stderr = d.Stderr
	return
}

func (d *DockerCompose) Up() (err error) {
	for _, imgPath := range d.Compose.DockerImageFiles {
		sutWorkFS := os.DirFS(sutWorkdir)
		if err = DockerLoad(sutWorkFS, imgPath); err != nil {
			return
		}
	}
	if d.cmdUp, err = d.createCmd([]string{"up"}); err != nil {
		return
	}
	if verbose {
		fmt.Printf("[%s] %v\n", sutName, d.cmdUp)
	}
	if err = d.cmdUp.Start(); err != nil {
		return
	}

	if d.cmdEvents, err = d.createCmd([]string{"events", "--json"}); err != nil {
		return
	}
	d.cmdEvents.Stdout = d.EventLog
	if err = d.cmdEvents.Start(); err != nil {
		return
	}

	// delay to allow services to all start
	// TODO: use event log to determine when all services are ready
	time.Sleep(2 * time.Second)

	// no particular need for a cmd.Wait() as dockerComposeDown() will actually shut down the docker-compose environment
	return
}

func (d *DockerCompose) Down() (err error) {
	fmt.Printf("[%s] docker compose down\n", sutName)
	cmd, _e := d.createCmd([]string{"down"})
	if _e != nil {
		return _e
	}
	//fmt.Println("command:", cmd)
	if err = cmd.Run(); err != nil {
		return
	}
	if err = d.cmdUp.Process.Kill(); err != nil {
		return
	}
	if err = d.cmdEvents.Process.Kill(); err != nil {
		return
	}
	return
}

func dockerImageList(cli *dockerClient.Client) error {
	images, contErr := cli.ImageList(context.Background(), dockerImage.ListOptions{All: true})
	if contErr != nil {
		return contErr
	}
	for _, img := range images {
		fmt.Printf("\t%v\n", img)
	}
	return nil
}

func dockerContainerList(cli *dockerClient.Client) error {
	containers, contErr := cli.ContainerList(context.Background(), dockerContainer.ListOptions{All: true})
	if contErr != nil {
		return contErr
	}
	for _, container := range containers {
		fmt.Printf("\t%s %s\n", container.ID[:10], container.Image)
	}
	return nil
}

func dockerLoad(cli *dockerClient.Client, img io.Reader) error {
	const quiet = false
	resp, err := cli.ImageLoad(context.Background(), img, quiet)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.Body != nil && resp.JSON {
		dec := json.NewDecoder(resp.Body)
		for {
			var jm jsonmessage.JSONMessage
			if err = dec.Decode(&jm); err != nil {
				if err == io.EOF {
					break
				}
				return err
			}
			if verbose {
				fmt.Printf("[dockerLoad] ")
				if jm.Progress != nil {
					fmt.Printf("%s %s\n", jm.Status, jm.Progress.String())
				} else if jm.Stream != "" {
					fmt.Printf("%s\n", jm.Stream)
				} else {
					fmt.Printf("%s\n", jm.Status)
				}
			}
		}
	}
	return nil
}

func DockerLoad(srcFS fs.FS, imageFile string) (err error) {
	const quiet = false
	cli, cliErr := dockerClient.NewClientWithOpts(
		dockerClient.WithAPIVersionNegotiation(),
		dockerClient.FromEnv,
	)
	if cliErr != nil {
		return cliErr
	}
	imgBytes, err := fs.ReadFile(srcFS, imageFile)
	if err != nil {
		return
	}
	img := bytes.NewReader(imgBytes)
	if verbose {
		fmt.Println("Loading image from fs:", imageFile)
	}
	return dockerLoad(cli, img)
}
