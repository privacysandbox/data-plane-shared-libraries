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
	_ "embed"
	"fmt"
	"strings"

	"github.com/spf13/cobra"
)

//go:embed embed/version-functionaltest.txt
var versionFunctionalTest string

//go:embed embed/version-buildsystem.txt
var versionBuildSystem string

//go:embed embed/.bazelversion
var versionBazel string

var versionCmd = &cobra.Command{
	Use:   "version",
	Short: "Version info",
	RunE: func(cmd *cobra.Command, args []string) (err error) {
		printVersion()
		return
	},
}

func printVersion() {
	fmt.Printf("functionaltest-system: %s\n", strings.TrimSpace(versionFunctionalTest))
	fmt.Printf("build-system: %s\n", strings.TrimSpace(versionBuildSystem))
	fmt.Printf("bazel: %s\n", strings.TrimSpace(versionBazel))
}
