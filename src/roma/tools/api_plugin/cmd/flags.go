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

//
// Copied from (with edits) https://github.com/pseudomuto/protoc-gen-doc/blob/v1.5.1/cmd/protoc-gen-doc/flags.go
//
// MIT License
//
// Copyright (c) 2022 David Muto (pseudomuto)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//

package cmd

import (
	"flag"
	"fmt"
	"io"

	gendoc "github.com/pseudomuto/protoc-gen-doc"
)

const helpMessage = `
This is a protoc plugin that is used to generate documentation from your protobuf files. Invocation is controlled by
using the doc_opt and doc_out options for protoc.

EXAMPLE: Generate HTML docs
protoc --doc_out=. --doc_opt=html,index.html protos/*.proto

EXAMPLE: Exclude file patterns
protoc --doc_out=. --doc_opt=html,index.html:google/*,somedir/* protos/*.proto

EXAMPLE: Use a custom template
protoc --doc_out=. --doc_opt=custom.tmpl,docs.txt protos/*.proto

EXAMPLE: Generate docs relative to source protos
protoc --doc_out=. --doc_opt=html,index.html,source_relative protos/*.proto

See https://github.com/pseudomuto/protoc-gen-doc for more details.
`

// Version returns the currently running version of protoc-gen-doc
func Version() string {
	return gendoc.VERSION
}

// Flags contains details about the CLI invocation of protoc-gen-doc
type Flags struct {
	appName     string
	flagSet     *flag.FlagSet
	err         error
	showHelp    bool
	showVersion bool
	writer      io.Writer
}

// Code returns the status code to exit with after handling the supplied flags
func (f *Flags) Code() int {
	if f.err != nil {
		return 1
	}

	return 0
}

// HasMatch returns whether or not the supplied args are matches. For example, passing `--help` will match, or some
// unknown parameter, but passing nothing will not.
func (f *Flags) HasMatch() bool {
	return f.ShowHelp() || f.ShowVersion()
}

// ShowHelp determines whether or not to show the help message
func (f *Flags) ShowHelp() bool {
	return f.err != nil || f.showHelp
}

// ShowVersion determines whether or not to show the version message
func (f *Flags) ShowVersion() bool {
	return f.showVersion
}

// PrintHelp prints the usage string including all flags to the `io.Writer` that was supplied to the `Flags` object.
func (f *Flags) PrintHelp() {
	fmt.Fprintf(f.writer, "Usage of %s:\n", f.appName)
	fmt.Fprintf(f.writer, "%s\n", helpMessage)
	fmt.Fprintf(f.writer, "FLAGS\n")
	f.flagSet.PrintDefaults()
}

// PrintVersion prints the version string to the `io.Writer` that was supplied to the `Flags` object.
func (f *Flags) PrintVersion() {
	fmt.Fprintf(f.writer, "%s version %s\n", f.appName, Version())
}

// ParseFlags parses the supplied options are returns a `Flags` object to the caller.
//
// Parameters:
//   - `w` - the `io.Writer` to use for printing messages (help, version, etc.)
//   - `args` - the set of args the program was invoked with (typically `os.Args`)
func ParseFlags(w io.Writer, args []string) *Flags {
	f := Flags{appName: args[0], writer: w}

	f.flagSet = flag.NewFlagSet(args[0], flag.ContinueOnError)
	f.flagSet.BoolVar(&f.showHelp, "help", false, "Show this help message")
	f.flagSet.BoolVar(&f.showVersion, "version", false, fmt.Sprintf("Print the current version (%v)", Version()))
	f.flagSet.SetOutput(w)

	// prevent showing help on parse error
	f.flagSet.Usage = func() {}
	f.err = f.flagSet.Parse(args[1:])
	return &f
}
