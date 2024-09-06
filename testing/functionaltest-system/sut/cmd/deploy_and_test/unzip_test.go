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
	"errors"
	"fmt"
	"io/fs"
	"strings"
	"testing"
)

// generate zip archive (headers only)
func generateZip(filenames []string) (str string, err error) {
	buf := new(strings.Builder)
	z := zip.NewWriter(buf)
	const nFiles = (1 << 16) + 42
	for _, filename := range filenames {
		if _, err = z.CreateHeader(&zip.FileHeader{Name: filename, Method: zip.Store}); err != nil {
			err = fmt.Errorf("creating file header %s: %v", filename, err)
			return
		}
	}
	if err = z.Close(); err != nil {
		err = fmt.Errorf("error closing zip writer: %v", err)
		return
	}
	str = buf.String()
	return
}

func TestFindFileInZip(t *testing.T) {
	filenames := []string{"a", "b", "c/d"}
	zipStr, err := generateZip(filenames)
	if err != nil {
		t.Error(err)
	}
	zipLen := int64(len(zipStr))
	zipReader := strings.NewReader(zipStr)
	tests := []struct {
		filename string
		desc     string
		wantErr  error
	}{
		{
			filename: "",
			desc:     "file not specified",
			wantErr:  fs.ErrInvalid,
		},
		{
			filename: "a/b/c",
			desc:     "filename includes path",
			wantErr:  fs.ErrInvalid,
		},
		{
			filename: "ab",
			desc:     "file not found",
			wantErr:  fs.ErrNotExist,
		},
		{
			filename: "b",
			desc:     "file found",
			wantErr:  nil,
		},
		{
			filename: "c",
			desc:     "directory specified instead of file",
			wantErr:  fs.ErrNotExist,
		},
		{
			filename: "d",
			desc:     "file found in subdirectory",
			wantErr:  nil,
		},
	}
	for _, tc := range tests {
		if _, err := findFileInZip(zipReader, zipLen, tc.filename); !errors.Is(err, tc.wantErr) {
			t.Errorf("findFileInZip(%d, %s) error: [%v], want [%v] -- test description: %s", zipLen, tc.filename, err, tc.wantErr, tc.desc)
		}
	}
}
