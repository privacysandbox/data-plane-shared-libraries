# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Gazelle-generated dependencies

# To add a single golang package:
#   gazelle update-repos -to_macro=deps.bzl%go_dependencies github.com/pseudomuto/protoc-gen-doc@v1.5.1
#
# Transitive deps are not added. Therefore, you will often need to fetch the package's go.mod
# and add its deps:
#   curl -OL https://github.com/pseudomuto/protoc-gen-doc/blob/master/go.mod
#   gazelle update-repos -to_macro deps.bzl%go_dependencies -from_file go.mod

load("@bazel_gazelle//:deps.bzl", "go_repository")
load("@rules_buf//buf:defs.bzl", "buf_dependencies")

def go_dependencies():
    go_repository(
        name = "com_github_aokoli_goutils",
        importpath = "github.com/aokoli/goutils",
        sum = "h1:/hA+Ywo3AxoDZY5ZMnkiEkUvkK4BPp927ax110KCqqg=",
        version = "v1.1.1",
    )
    go_repository(
        name = "com_github_davecgh_go_spew",
        importpath = "github.com/davecgh/go-spew",
        sum = "h1:ZDRjVQ15GmhC3fiQ8ni8+OwkZQO4DARzQgrnXU1Liz8=",
        version = "v1.1.0",
    )
    go_repository(
        name = "com_github_envoyproxy_protoc_gen_validate",
        importpath = "github.com/envoyproxy/protoc-gen-validate",
        sum = "h1:bV5JGEB1ouEzZa0hgVDFFiClrUEuGWRaAc/3mxR2QK0=",
        version = "v0.3.0-java",
    )
    go_repository(
        name = "com_github_gogo_protobuf",
        importpath = "github.com/gogo/protobuf",
        sum = "h1:Ov1cvc58UF3b5XjBnZv7+opcTcQFZebYjWzi34vdm4Q=",
        version = "v1.3.2",
    )
    go_repository(
        name = "com_github_golang_protobuf",
        importpath = "github.com/golang/protobuf",
        sum = "h1:ROPKBNFfQgOUMifHyP+KYbvpjbdoFNs+aK7DXlji0Tw=",
        version = "v1.5.2",
    )
    go_repository(
        name = "com_github_google_go_cmp",
        importpath = "github.com/google/go-cmp",
        sum = "h1:BKbKCqvP6I+rmFHt06ZmyQtvB8xAkWdhFyr0ZUNZcxQ=",
        version = "v0.5.6",
    )
    # go_repository(
    #     name = "com_github_google_uuid",
    #     importpath = "github.com/google/uuid",
    #     sum = "h1:Gkbcsh/GbpXz7lPftLA3P6TYMwjCLYm83jiFQZF/3gY=",
    #     version = "v1.1.1",
    # )

    go_repository(
        name = "com_github_google_uuid",
        importpath = "github.com/google/uuid",
        sum = "h1:EVhdT+1Kseyi1/pUmXKaFxYsDNy9RQYkMWRH68J/W7Y=",
        version = "v1.1.2",
    )
    go_repository(
        name = "com_github_huandu_xstrings",
        importpath = "github.com/huandu/xstrings",
        sum = "h1:/Gcsuc1x8JVbJ9/rlye4xZnVAbEkGauT8lbebqcQws4=",
        version = "v1.3.3",
    )
    go_repository(
        name = "com_github_imdario_mergo",
        importpath = "github.com/imdario/mergo",
        sum = "h1:3tnifQM4i+fbajXKBHXWEH+KvNHqojZ778UH75j3bGA=",
        version = "v0.3.11",
    )
    go_repository(
        name = "com_github_kr_pretty",
        importpath = "github.com/kr/pretty",
        sum = "h1:L/CwN0zerZDmRFUapSPitk6f+Q3+0za1rQkzVuMiMFI=",
        version = "v0.1.0",
    )
    go_repository(
        name = "com_github_masterminds_goutils",
        importpath = "github.com/Masterminds/goutils",
        sum = "h1:5nUrii3FMTL5diU80unEVvNevw1nH4+ZV4DSLVJLSYI=",
        version = "v1.1.1",
    )

    go_repository(
        name = "com_github_masterminds_semver",
        importpath = "github.com/Masterminds/semver",
        sum = "h1:WBLTQ37jOCzSLtXNdoo8bNM8876KhNqOKvrlGITgsTc=",
        version = "v1.4.2",
    )
    go_repository(
        name = "com_github_masterminds_semver_v3",
        importpath = "github.com/Masterminds/semver/v3",
        sum = "h1:3MEsd0SM6jqZojhjLWWeBY+Kcjy9i6MQAeY7YgDP83g=",
        version = "v3.2.0",
    )

    # go_repository(
    #     name = "com_github_masterminds_sprig",
    #     importpath = "github.com/Masterminds/sprig",
    #     sum = "h1:0gSxPGWS9PAr7U2NsQ2YQg6juRDINkUyuvbb4b2Xm8w=",
    #     version = "v2.15.0+incompatible",
    # )
    go_repository(
        name = "com_github_masterminds_sprig",
        importpath = "github.com/Masterminds/sprig/v3",
        sum = "h1:eL2fZNezLomi0uOLqjQoN6BfsDD+fyLtgbJMAj9n6YA=",
        version = "v3.2.3",
    )
    go_repository(
        name = "com_github_mitchellh_copystructure",
        importpath = "github.com/mitchellh/copystructure",
        sum = "h1:Laisrj+bAB6b/yJwB5Bt3ITZhGJdqmxquMKeZ+mmkFQ=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_mitchellh_reflectwalk",
        importpath = "github.com/mitchellh/reflectwalk",
        sum = "h1:G2LzWKi524PWgd3mLHV8Y5k7s6XUvT0Gef6zxSIeXaQ=",
        version = "v1.0.2",
    )

    go_repository(
        name = "com_github_mwitkow_go_proto_validators",
        importpath = "github.com/mwitkow/go-proto-validators",
        sum = "h1:qRlmpTzm2pstMKKzTdvwPCF5QfBNURSlAgN/R+qbKos=",
        version = "v0.3.2",
    )
    go_repository(
        name = "com_github_pmezard_go_difflib",
        importpath = "github.com/pmezard/go-difflib",
        sum = "h1:4DBwDE0NGyQoBHbLQYPwSUPoCMWR5BEzIk/f1lZbAQM=",
        version = "v1.0.0",
    )
    go_repository(
        name = "com_github_pseudomuto_protoc_gen_doc",
        importpath = "github.com/pseudomuto/protoc-gen-doc",
        patch_args = ["-p1"],
        patches = [
            Label("//third_party:protoc-gen-doc.patch"),
        ],
        sum = "h1:Ah259kcrio7Ix1Rhb6u8FCaOkzf9qRBqXnvAufg061w=",
        version = "v1.5.1",
    )
    go_repository(
        name = "com_github_pseudomuto_protokit",
        importpath = "github.com/pseudomuto/protokit",
        sum = "h1:hlnBDcy3YEDXH7kc9gV+NLaN0cDzhDvD1s7Y6FZ8RpM=",
        version = "v0.2.0",
    )
    go_repository(
        name = "com_github_shopspring_decimal",
        importpath = "github.com/shopspring/decimal",
        sum = "h1:abSATXmQEYyShuxI4/vyW3tV1MrKAJzCZ/0zLUXYbsQ=",
        version = "v1.2.0",
    )
    go_repository(
        name = "com_github_spf13_cast",
        importpath = "github.com/spf13/cast",
        sum = "h1:nFm6S0SMdyzrzcmThSipiEubIDy8WEXKNZ0UOgiRpng=",
        version = "v1.3.1",
    )

    go_repository(
        name = "com_github_stretchr_testify",
        importpath = "github.com/stretchr/testify",
        sum = "h1:nOGnQDM7FYENwehXlg/kFVnos3rEvtKTjRvOWSzb6H4=",
        version = "v1.5.1",
    )
    go_repository(
        name = "in_gopkg_check_v1",
        importpath = "gopkg.in/check.v1",
        sum = "h1:qIbj1fsPNlZgppZ+VLlY7N33q108Sa+fhmuc+sWQYwY=",
        version = "v1.0.0-20180628173108-788fd7840127",
    )
    go_repository(
        name = "in_gopkg_yaml_v2",
        importpath = "gopkg.in/yaml.v2",
        sum = "h1:D8xgwECY7CYvx+Y2n4sBz93Jn9JRvxdiyyo8CTfuKaY=",
        version = "v2.4.0",
    )
    go_repository(
        name = "in_gopkg_yaml_v3",
        importpath = "gopkg.in/yaml.v3",
        sum = "h1:hjy8E9ON/egN1tAYqKb61G10WtihqetD4sz2H+8nIeA=",
        version = "v3.0.0",
    )
    go_repository(
        name = "org_golang_google_genproto",
        importpath = "google.golang.org/genproto",
        sum = "h1:ysnBoUyeL/H6RCvNRhWHjKoDEmguI+mPU+qHgK8qv/w=",
        version = "v0.0.0-20210917145530-b395a37504d4",
    )
    go_repository(
        name = "org_golang_google_protobuf",
        importpath = "google.golang.org/protobuf",
        sum = "h1:SnqbnDw1V7RiZcXPx5MEeqPv2s79L9i7BJUlG/+RurQ=",
        version = "v1.27.1",
    )
    go_repository(
        name = "org_golang_x_crypto",
        importpath = "golang.org/x/crypto",
        sum = "h1:a06MkbcxBrEFc0w0QIZWXrH/9cCX6KJyWbBOIwAn+7A=",
        version = "v0.3.0",
    )

def buf_deps():
    buf_dependencies(
        name = "buf_deps_apis",
        modules = [
            "buf.build/googleapis/googleapis:28151c0d0a1641bf938a7672c500e01d",
        ],
    )
