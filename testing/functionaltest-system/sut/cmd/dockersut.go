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

package cmd

import (
	deployAndTest "github.com/privacysandbox/functionaltest-system/sut/cmd/deploy_and_test"
	"github.com/spf13/cobra"
)

var dockerSutCmd = &cobra.Command{
	Use:   "dockersut",
	Short: "Docker System-Under-Test commands",
	Long: `Collection of utilities related to functional testing for the
Privacy Sandbox servers.`,
}

func init() {
	dockerSutCmd.Flags().BoolP("toggle", "t", false, "Help message for toggle")
	dockerSutCmd.AddCommand(deployAndTest.DockerDeploySutCmd)
}
