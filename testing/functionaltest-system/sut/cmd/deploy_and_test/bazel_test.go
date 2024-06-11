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
	"os"
	"strings"
	"testing"

	sutV1Pb "github.com/privacysandbox/functionaltest-system/sut/v1/pb"
)

func TestRunBazelTest(t *testing.T) {
	errTests := []struct {
		bazelTestCmd *sutV1Pb.BazelTest
		label        string
		desc         string
		wantErr      error
	}{
		{
			bazelTestCmd: &sutV1Pb.BazelTest{},
			label:        "",
			desc:         "label not specified",
			wantErr:      ErrInvalidLabel,
		},
		{
			bazelTestCmd: &sutV1Pb.BazelTest{},
			label:        "\t",
			desc:         "label not specified",
			wantErr:      ErrInvalidLabel,
		},
		{
			bazelTestCmd: &sutV1Pb.BazelTest{},
			label:        "t",
			desc:         "test targets not specified",
			wantErr:      ErrTargetsUnspecified,
		},
		{
			bazelTestCmd: &sutV1Pb.BazelTest{TestTargets: []string{}},
			label:        "t",
			desc:         "no test targets specified",
			wantErr:      ErrTargetsUnspecified,
		},
		{
			bazelTestCmd: &sutV1Pb.BazelTest{TestTargets: []string{""}},
			label:        "t",
			desc:         "empty test target specified",
			wantErr:      ErrTargetsUnspecified,
		},
		{
			bazelTestCmd: &sutV1Pb.BazelTest{TestTargets: []string{" "}},
			label:        "t",
			desc:         "whitespace-only test target specified",
			wantErr:      ErrTargetsUnspecified,
		},
	}
	for _, tc := range errTests {
		if _, err := runBazelTest(tc.bazelTestCmd, tc.label); !errors.Is(err, tc.wantErr) {
			t.Errorf("runBazelTest(%q) error: [%v], want [%v] for test: %s", tc.bazelTestCmd, err, tc.wantErr, tc.desc)
		}
	}
	expectErrorTests := []struct {
		bazelTestCmd *sutV1Pb.BazelTest
		label        string
		desc         string
	}{
		{
			bazelTestCmd: &sutV1Pb.BazelTest{TestTargets: []string{"/////....."}},
			label:        "t",
			desc:         "invalid test target specified",
		},
	}
	for _, tc := range expectErrorTests {
		if _, err := runBazelTest(tc.bazelTestCmd, tc.label); err == nil {
			t.Errorf("runBazelTest(%q) expecting error for test: %s", tc.desc)
		}
	}
}

func TestRunBazelArgs(t *testing.T) {
	errTests := []struct {
		args    []string
		desc    string
		wantErr error
	}{
		{
			args:    []string{},
			desc:    "args not specified",
			wantErr: ErrInvalidArgs,
		},
		{
			args:    []string{" ", "\t"},
			desc:    "only empty string args",
			wantErr: ErrInvalidArgs,
		},
	}
	for _, tc := range errTests {
		if _, err := runBazelArgs(tc.args); !errors.Is(err, tc.wantErr) {
			t.Errorf("runBazelArgs(%q) error: [%v], want [%v] for test: %s", tc.args, err, tc.wantErr, tc.desc)
		}
	}
}
func TestCaptureBazelLogs(t *testing.T) {
	logPath := "xyz"
	os.Mkdir(logPath, 0755)
	errTests := []struct {
		logRoot         string
		archiveFilename string
		desc            string
		wantErr         error
	}{
		{
			logRoot:         "",
			archiveFilename: "",
			desc:            "empty args",
			wantErr:         ErrInvalidArgs,
		},
		{
			logRoot:         "",
			archiveFilename: "a",
			desc:            "empty logRoot",
			wantErr:         ErrInvalidArgs,
		},
		{
			logRoot:         logPath,
			archiveFilename: "",
			desc:            "empty archiveFilename",
			wantErr:         ErrInvalidArgs,
		},
		{
			logRoot:         "a/b/c/../../d/../../",
			archiveFilename: "b",
			desc:            "non-file path",
			wantErr:         nil,
		},
		{
			// deliberately avoid filepath.Join() here, as that also cleans the path
			logRoot:         strings.Join([]string{"a/b/c/../../d/../..", logPath}, string(os.PathSeparator)),
			archiveFilename: "c",
			desc:            "valid args",
			wantErr:         nil,
		},
		{
			logRoot:         "a",
			archiveFilename: "d",
			desc:            "logRoot dir does not exist",
			wantErr:         ErrNotDirectory,
		},
		{
			logRoot:         logPath,
			archiveFilename: logPath,
			desc:            "archiveFilename is existing dir",
			wantErr:         ErrIsDirectory,
		},
	}
	for _, tc := range errTests {
		if err := captureBazelLogs(tc.logRoot, tc.archiveFilename); !errors.Is(err, tc.wantErr) {
			t.Errorf("captureBazelLogs(%s, %s) error: [%v], want [%v] for test: %s", tc.logRoot, tc.archiveFilename, err, tc.wantErr, tc.desc)
		}
	}
}
