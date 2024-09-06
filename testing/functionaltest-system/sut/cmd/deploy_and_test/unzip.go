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
	"path/filepath"
)

func unzipToDir(zipFile io.ReaderAt, size int64, basedir, dest string) (err error) {
	basedir = filepath.Join("/", basedir)
	dest = filepath.Clean(dest)

	// Closure to address file descriptors issue with all the deferred .Close() methods
	extractAndWriteFile := func(f *zip.File, destName string) (fileErr error) {
		if f.FileInfo().IsDir() {
			return
		}
		if len(destName) == 0 || destName == "." || destName == "/" {
			return fmt.Errorf("illegal destination: %s", destName)
		}
		destPath := filepath.Join(dest, destName)
		if _, _err := filepath.Rel(dest, destPath); _err != nil {
			return fmt.Errorf("illegal file destination path: %s", destPath)
		}

		rc, _fileErr := f.Open()
		if _fileErr != nil {
			fileErr = _fileErr
			return
		}
		defer func() {
			if _err := rc.Close(); _err != nil {
				fileErr = _err
			}
			return
		}()

		if fileErr = os.MkdirAll(filepath.Dir(destPath), 0755); fileErr != nil && !os.IsExist(fileErr) {
			return
		}
		dest, _err := os.OpenFile(destPath, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0555)
		if _err != nil {
			fileErr = _err
			return
		}
		defer func() {
			if _err := dest.Close(); _err != nil {
				fileErr = _err
			}
			return
		}()
		if verbose {
			fmt.Printf("copying %s\n", f.Name)
		}
		if _, fileErr = io.Copy(dest, rc); fileErr != nil {
			return
		}
		fileErr = dest.Chmod(f.Mode())
		return
	}

	r, _err := zip.NewReader(zipFile, size)
	if _err != nil {
		err = fmt.Errorf("reading zip file -- %w", _err)
		return
	}
	validDestName := func(destName string) bool {
		return !(len(destName) == 0 || destName == "." || destName == "/")
	}
	os.MkdirAll(dest, 0755)
	for _, f := range r.File {
		destName := filepath.Join("/", f.Name)
		if destName, _err = filepath.Rel(basedir, destName); _err == nil && validDestName(destName) {
			if err = extractAndWriteFile(f, destName); err != nil {
				panic(err)
				return
			}
		} else {
			fmt.Printf("skipping zip file %s per as outside basedir %s\n", f.Name, basedir)
		}
	}
	return
}

func findFileInZip(zipFile io.ReaderAt, size int64, filename string) (path string, err error) {
	if !fs.ValidPath(filename) {
		err = &fs.PathError{Op: "findFileInZip", Path: filename, Err: fs.ErrInvalid}
		return
	}
	if filepath.Dir(filename) != "." {
		err = &fs.PathError{Op: "findFileInZip", Path: filename, Err: fs.ErrInvalid}
		return
	}
	r, _err := zip.NewReader(zipFile, size)
	if _err != nil {
		err = fmt.Errorf("reading zip file -- %w", _err)
		return
	}
	for _, f := range r.File {
		// if filepath.Base(f.Name) == filename && !f.FileInfo.IsDir() {
		if filepath.Base(f.Name) == filename {
			path = filepath.Dir(f.Name)
			return
		}
	}
	err = &fs.PathError{Op: "findFileInZip", Path: filename, Err: fs.ErrNotExist}
	return
}
