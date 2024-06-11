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
	"errors"
	"testing"

	sutV1Pb "github.com/privacysandbox/functionaltest-system/sut/v1/pb"
)

func TestShellCommandToCmd(t *testing.T) {
	tests := []struct {
		shellCommand *sutV1Pb.ShellCommand
		desc         string
		wantErr      error
	}{
		{
			shellCommand: &sutV1Pb.ShellCommand{},
			desc:         "stage without command specified",
			wantErr:      StageError{"command not specified"},
		},
		{
			shellCommand: &sutV1Pb.ShellCommand{Command: []string{}},
			desc:         "stage without command specified",
			wantErr:      StageError{"command not specified"},
		},
		{
			shellCommand: &sutV1Pb.ShellCommand{Command: []string{""}},
			desc:         "stage without executable specified",
			wantErr:      StageError{"executable not specified"},
		},
		{
			shellCommand: &sutV1Pb.ShellCommand{Command: []string{"abc"}},
			desc:         "stage with executable",
			wantErr:      nil,
		},
		{
			shellCommand: &sutV1Pb.ShellCommand{Command: []string{"abc", "123"}},
			desc:         "stage with executable and ars",
			wantErr:      nil,
		},
	}
	for _, tc := range tests {
		if _, err := shellCommandToCmd(tc.shellCommand); !errors.Is(err, tc.wantErr) {
			t.Errorf("shellCommandToCmd(%q) error: [%v], want [%v] -- test description: %s", tc.shellCommand, err, tc.wantErr, tc.desc)
		}
	}
}

func TestReadTestRunner(t *testing.T) {
	tests := []struct {
		runnerConfigFile string
		desc             string
		isErr            bool
		wantErr          error
	}{
		{
			runnerConfigFile: "",
			desc:             "empty path",
			wantErr:          ErrInvalid,
		},
		{
			runnerConfigFile: "abc123.txt",
			desc:             "invalid path",
			isErr:            true,
		},
	}
	for _, tc := range tests {
		_, err := readTestRunner(tc.runnerConfigFile)
		if err == nil {
			if tc.wantErr != nil || tc.isErr {
				t.Errorf("readTestRunner(%q) expecting error: [%v] -- test description: %s", tc.runnerConfigFile, err, tc.desc)
			}
		} else {
			if tc.wantErr == nil && !tc.isErr {
				t.Errorf("readTestRunner(%q) error: [%v] -- test description: %s", tc.runnerConfigFile, err, tc.desc)
			} else if tc.wantErr != nil && !errors.Is(err, tc.wantErr) {
				t.Errorf("readTestRunner(%q) error: [%v], want [%v] -- test description: %s", tc.runnerConfigFile, err, tc.wantErr, tc.desc)
			}
		}
	}
}
