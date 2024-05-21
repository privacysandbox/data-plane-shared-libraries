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
	romaApi "github.com/privacysandbox/data-plane-shared/apis/roma/v1"
	"github.com/pseudomuto/protoc-gen-doc/extensions"
)

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
