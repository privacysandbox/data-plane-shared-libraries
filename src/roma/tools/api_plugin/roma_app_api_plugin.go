// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"embed"
	"fmt"
	"io"
	"io/fs"
	"log"
	"os"
	"path"
	"strings"

	pgdCmd "github.com/privacysandbox/data-plane-shared/src/roma/tools/api_plugin/cmd/cmd"
	romaApi "github.com/privacysandbox/data-plane-shared/apis/roma/v1"
	gendoc "github.com/pseudomuto/protoc-gen-doc"
	"github.com/pseudomuto/protoc-gen-doc/extensions"
	"github.com/pseudomuto/protokit"
)

var (
	tmplWorkdir string
)
var tmplSymlink = "redacted"

//go:embed tmpl/*
var embedFS embed.FS

func squoteEscape(input string) string {
	return fmt.Sprintf("'%s'", strings.ReplaceAll(input, "'", "''"))
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
			_, err := copyFile(srcFS, filePath, path.Join(_destDir, d.Name()))
			if err != nil {
				return err
			}
		}
		return nil
	})
}

func copyFile(srcFS fs.FS, srcPath string, destPath string) (int64, error) {
	if srcStatFS, ok := srcFS.(fs.StatFS); ok {
		srcStat, err := srcStatFS.Stat(srcPath)
		if err != nil {
			return 0, err
		}
		if !srcStat.Mode().IsRegular() {
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
	return fs.WalkDir(srcFS, ".", func(path string, d fs.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if !d.Type().IsDir() {
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

func extractTemplates() (err error) {
 	// tmplWorkdir is a temp dir into which embedded sut files and
	// workdir/zip content are copied
	if tmplWorkdir, err = os.MkdirTemp("", "tmpl"); err != nil {
		return
	}
	if err = os.Symlink(tmplWorkdir, tmplSymlink); os.IsExist(err) {
		if err = os.Remove(tmplSymlink); err != nil {
			err = fmt.Errorf("unable to remove symlink: %s", tmplSymlink, err)
			return
		}
		if err = os.Symlink(tmplWorkdir, tmplSymlink); err != nil {
			err = fmt.Errorf("unable to create symlink: %s", tmplSymlink, err)
			return
		}
	} else if err != nil {
		return
	}
	if err = copyToDir(embedFS, tmplWorkdir); err != nil {
		return
	}
	return
}


func init() {
	// src/roma/tools/api_plugin/cmd.AddFunctions("squote_esc", squoteEscape)
	extensions.SetTransformer(
		"privacysandbox.apis.roma.app_api.v1.roma_svc_annotation",
		func(payload interface{}) interface{} {
			if obj, ok := payload.(*romaApi.RomaServiceAnnotation); ok {
				return obj
			} else {
				return nil
			}
		},
	)
	extensions.SetTransformer(
		"privacysandbox.apis.roma.app_api.v1.roma_rpc_annotation",
		func(payload interface{}) interface{} {
			if obj, ok := payload.(*romaApi.RomaFunctionAnnotation); ok {
				return obj
			} else {
				return nil
			}
		},
	)
	extensions.SetTransformer(
		"privacysandbox.apis.roma.app_api.v1.roma_field_annotation",
		func(payload interface{}) interface{} {
			if obj, ok := payload.(*romaApi.RomaFieldAnnotation); ok {
				return obj
			} else {
				return nil
			}
		},
	)
}

// protoc-gen-doc is used to generate documentation from comments in your proto files.
//
// It is a protoc plugin, and can be invoked by passing `--doc_out` and `--doc_opt` arguments to protoc.
//
// Example: generate HTML documentation
//
//     protoc --doc_out=. --doc_opt=html,index.html protos/*.proto
//
// Example: use a custom template
//
//     protoc --doc_out=. --doc_opt=custom.tmpl,docs.txt protos/*.proto
//
// For more details, check out the README at https://github.com/pseudomuto/protoc-gen-doc

// HandleFlags checks if there's a match and returns true if it was "handled"
func HandleFlags(f *pgdCmd.Flags) bool {
	if !f.HasMatch() {
		return false
	}
	if f.ShowHelp() {
		f.PrintHelp()
	}
	if f.ShowVersion() {
		f.PrintVersion()
	}
	return true
}

func Cleanup() (err error) {
	err = os.Remove(tmplSymlink)
	return
}

func main() {
	if err := extractTemplates(); err != nil {
		log.Fatal(err)
	}
	defer os.RemoveAll(tmplWorkdir)
	defer Cleanup()
	if flags := pgdCmd.ParseFlags(os.Stdout, os.Args); HandleFlags(flags) {
		os.Exit(flags.Code())
	}
	if err := protokit.RunPlugin(new(gendoc.Plugin)); err != nil {
		log.Fatal(err)
	}
}
