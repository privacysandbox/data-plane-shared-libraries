# Copyright 2022 Google LLC
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

"""Macros for rpc diff testing and performance testing"""

load("@aspect_bazel_lib//lib:jq.bzl", "jq")

def functional_test_files_for(
        glob_spec,
        request_suffix = ".request.json",
        reply_suffix = ".reply.json",
        pre_filter_suffix = ".pre-filter.jq",
        post_filter_suffix = ".filter.jq",
        post_filter_slurp_suffix = ".filter.slurp.jq"):
    file_types = ("request", "reply", "pre-filter", "post-filter", "post-filter-slurp")
    suffixes = (request_suffix, reply_suffix, pre_filter_suffix, post_filter_suffix, post_filter_slurp_suffix)
    files = {
        (test_name, filetype): fullpath
        for filetype, suffix in zip(file_types, suffixes)
        for fullpath in native.glob(["{}{}".format(glob_spec, suffix)])
        for test_name in [fullpath.removesuffix(suffix).rpartition("/")[2]]
    }
    test_files = {
        test_name: {
            file_type: files.get((test_name, file_type), "")
            for file_type in file_types
        }
        for test_name in {tname: 0 for tname, _ in files}
    }
    return test_files

def rpc_diff_test(
        name,
        request,
        golden_reply,
        endpoint,
        rpc,
        protoset = "",
        custom_rpc_invoker_tarball = "",
        jq_pre_filter = "",
        jq_post_filter = "",
        jq_post_slurp = False,
        tags = [],
        plaintext = False,
        client_type = "",
        **kwargs):
    """Generates a diff test for a grpc request/reply.

    Args:
      name: test suite name
      request: label of request file
      golden_reply: label of reply file
      endpoint: struct for endpoint defining the protocol, host, port etc
      rpc: gRPC qualified rpc name
      protoset: protobuf descriptor set label or file
      custom_rpc_invoker_tarball: label for an image tarball used to invoke rpc requests
      jq_pre_filter: jq filter program as string to apply to the rpc request
      jq_post_filter: jq filter program as string to apply to the rpc response
      jq_post_slurp: boolean to indicate use of jq --slurp for the rpc response
      tags: tag list for the tests
      plaintext: boolean to indicate plaintext request
      client_type: client type to use for the rpc request
      **kwargs: additional test args
    """
    runner = Label("//bazel:rpc_diff_test_runner")
    rpc_invoker_type = "binary"

    if custom_rpc_invoker_tarball:
        rpc_invoker = custom_rpc_invoker_tarball
        rpc_invoker_type = "image"
    elif endpoint.endpoint_type == "grpc":
        rpc_invoker = Label("//bazel:grpcurl_rpc_invoker")
    elif endpoint.endpoint_type == "http":
        rpc_invoker = Label("//bazel:curl_rpc_invoker")
    else:
        fail("[rpc_diff_test] unsupported endpoint type:", endpoint.endpoint_type)
    args = [
        "--endpoint-hostport",
        "{}:{}".format(endpoint.host, endpoint.port),
        "--rpc",
        rpc,
        "--protoset",
        "$(rootpath {})".format(protoset),
        "--request",
        "$(execpath {})".format(request),
        "--reply",
        "$(execpath {})".format(golden_reply),
        "--rpc-invoker",
        "$(rootpath {})".format(rpc_invoker),
        "--rpc-invoker-type",
        rpc_invoker_type,
    ]
    if endpoint.docker_network:
        args.extend([
            "--docker-network",
            endpoint.docker_network,
        ])

    if endpoint.http_path and endpoint.endpoint_type == "http":
        args.extend([
            "--http-path",
            endpoint.http_path,
        ])

    data = [
        request,
        golden_reply,
        protoset,
        rpc_invoker,
    ]

    if plaintext:
        args.extend(["--plaintext"])
    if client_type:
        args.extend(["--client-type", client_type])
    if jq_pre_filter:
        args.extend(["--jq-pre-filter", "$(execpath {})".format(jq_pre_filter)])
        data.append(jq_pre_filter)
    if jq_post_filter:
        args.extend(["--jq-post-filter", "$(execpath {})".format(jq_post_filter)])
        data.append(jq_post_filter)
    if jq_post_slurp:
        args.extend(["--jq-post-slurp"])
    native.sh_test(
        name = name,
        srcs = [runner],
        args = args,
        data = data,
        tags = tags,
        **kwargs
    )

def rpc_diff_test_suite(
        name,
        endpoint,
        rpc,
        test_files_glob_spec,
        protoset = "",
        custom_rpc_invoker_tarball = "",
        test_tags = [],
        plaintext = False,
        **kwargs):
    """Generate a test suite for test cases within the specified directory tree.

    Args:
      name: test suite name
      test_files_glob_spec: glob spec for test files, passed to function functional_test_files_for()
      protoset: protobuf descriptor set label or file
      custom_rpc_invoker_tarball: label for an image tarball used to invoke rpc requests
      test_tags: tag list for the tests
      plaintext: boolean to indicate plaintext requests
      **kwargs: additional args
    """
    test_files = functional_test_files_for(glob_spec = test_files_glob_spec)
    if not test_files:
        print("no test files found for glob spec: ", test_files_glob_spec)
        return

    test_labels = []
    for test_name, testcase_files in test_files.items():
        qual_test_name = "{}-{}".format(name, test_name)
        test_labels.append(":{}".format(qual_test_name))
        extra_kwargs = dict(kwargs)
        if testcase_files["pre-filter"]:
            extra_kwargs["jq_pre_filter"] = ":{}".format(testcase_files["pre-filter"])
        if testcase_files["post-filter"]:
            extra_kwargs["jq_post_filter"] = ":{}".format(testcase_files["post-filter"])
        elif testcase_files["post-filter-slurp"]:
            extra_kwargs["jq_post_filter"] = ":{}".format(testcase_files["post-filter-slurp"])
            extra_kwargs["jq_post_slurp"] = True
        rpc_diff_test(
            name = qual_test_name,
            endpoint = endpoint,
            golden_reply = ":{}".format(testcase_files["reply"]),
            protoset = protoset,
            request = ":{}".format(testcase_files["request"]),
            rpc = rpc,
            tags = test_tags,
            custom_rpc_invoker_tarball = custom_rpc_invoker_tarball,
            plaintext = plaintext,
            **extra_kwargs
        )
    native.test_suite(
        name = name,
        tests = test_labels,
        tags = test_tags,
    )

def rpc_perf_test(
        name,
        request,
        endpoint,
        rpc,
        protoset,
        jq_pre_filter = "",
        plaintext = False,
        tags = [],
        **kwargs):
    """Generate a ghz report for a grpc request.

    Args:
      name: test suite name
      request: label of request file
      endpoint: struct for endpoint defining the protocol, host, port etc
      rpc: gRPC qualified rpc name
      protoset: protobuf descriptor set label or file
      jq_pre_filter: jq filter program as string to apply to the rpc request
      plaintext: boolean to indicate plaintext request
      tags: tag list for the tests
      **kwargs: additional test args
    """
    if endpoint.endpoint_type == "grpc":
        runner = Label("//bazel:ghz_test_runner")
        #elif endpoint.endpoint_type == "http":
        #    runner = Label("//bazel:ab_test_runner")

    else:
        fail("[rpc_perf_test] unsupported endpoint type:", endpoint.endpoint_type)

    args = [
        "--endpoint-hostport",
        "{}:{}".format(endpoint.host, endpoint.port),
        "--rpc",
        rpc,
        "--protoset",
        "$(rootpath {})".format(protoset),
    ]
    data = [protoset]

    if endpoint.docker_network:
        args.extend(["--docker-network", endpoint.docker_network])

    if plaintext:
        args.extend(["--plaintext"])

    if endpoint.http_path and endpoint.endpoint_type == "http":
        args.extend([
            "--http-path",
            endpoint.http_path,
        ])

    if jq_pre_filter:
        filtered_request = "{}-filtered_request.json".format(name)
        jq(
            name = "{}-filtered_request".format(name),
            srcs = [request],
            filter_file = jq_pre_filter,
            out = filtered_request,
        )
        args.extend(["--request", "$(rootpath :{})".format(filtered_request)])
        data.append(":{}".format(filtered_request))
    else:
        args.extend(["--request", "$(execpath {})".format(request)])
        data.append(request)

    native.sh_test(
        name = name,
        srcs = [runner],
        args = args,
        data = data,
        tags = tags,
        **kwargs
    )

def rpc_perf_test_suite(
        name,
        endpoint,
        rpc,
        test_files_glob_spec,
        protoset,
        test_tags = [],
        plaintext = False,
        **kwargs):
    """Generates a test suite for test cases within the specified directory tree.

    Args:
      name: test suite name
      test_files_glob_spec: glob spec for test files, passed to function functional_test_files_for()
      protoset: protobuf descriptor set label or file
      test_tags: tag list for the tests
      plaintext: boolean to indicate plaintext requests
      **kwargs: additional args
    """
    test_files = functional_test_files_for(glob_spec = test_files_glob_spec)
    if not test_files:
        print("no test files found for glob spec: ", test_files_glob_spec)
        return

    test_labels = []
    for test_name, testcase_files in test_files.items():
        qual_test_name = "{}-{}".format(name, test_name)
        test_labels.append(":{}".format(qual_test_name))
        extra_kwargs = dict(kwargs)
        if testcase_files["pre-filter"]:
            extra_kwargs["jq_pre_filter"] = ":{}".format(testcase_files["pre-filter"])
        rpc_perf_test(
            name = qual_test_name,
            endpoint = endpoint,
            protoset = protoset,
            request = ":{}".format(testcase_files["request"]),
            rpc = rpc,
            tags = test_tags,
            plaintext = plaintext,
            **extra_kwargs
        )

    native.test_suite(
        name = name,
        tests = test_labels,
        tags = test_tags,
    )

def wrk2_perf_test(
        name,
        endpoint,
        rpc,
        request_rate,
        request = None,
        connections = None,
        duration = None,
        threads = None,
        latency = False,
        timeout = None,
        lua_script = None,
        jq_pre_filter = "",
        plaintext = False,
        tags = [],
        **kwargs):
    """Generate a ghz report for a grpc request.

    Args:
      name: test suite name
      endpoint: struct for endpoint defining the protocol, host, port etc
      rpc: gRPC qualified rpc name
      request_rate: number of requests per second (throughput)
      request: label of request file
      connections: number of connections to keep open
      duration: duration of test (e.g. 2s, 2m, 2h)
      threads: number of threads to use
      latency:  boolean to enable printing of latency statistics
      timeout:  request timeout (e.g. 2s, 2m, 2h)
      lua_script: label of lua script
      jq_pre_filter: jq filter program as string to apply to the rpc request
      plaintext: boolean to indicate plaintext request
      tags: tag list for the tests
      **kwargs: additional test args
    """
    if endpoint.endpoint_type == "http":
        runner = Label("//bazel:wrk2_test_runner")
    else:
        fail("[wrk2_perf_test] unsupported endpoint type:", endpoint.endpoint_type)

    if not request and not lua_script:
        fail("[wrk2_perf_test] request must be supplied for default lua script")

    args = [
        "--endpoint-hostport",
        "{}:{}".format(endpoint.host, endpoint.port),
        "--rpc",
        rpc,
        "--request-rate",
        request_rate,
    ]
    data = []

    if connections:
        args.extend(["--connections", connections])

    if duration:
        args.extend(["--duration", duration])

    if threads:
        args.extend(["--threads", threads])

    if latency:
        args.extend(["--latency"])

    if timeout:
        args.extend(["--timeout", timeout])

    if lua_script:
        args.extend(["--lua-script", "$(execpath {})".format(lua_script)])
        data.append(lua_script)

    if endpoint.docker_network:
        args.extend(["--docker-network", endpoint.docker_network])

    if plaintext:
        args.extend(["--plaintext"])

    if endpoint.http_path and endpoint.endpoint_type == "http":
        args.extend([
            "--http-path",
            endpoint.http_path,
        ])

    if request:
        if not jq_pre_filter:
            args.extend(["--request", "$(execpath {})".format(request)])
            data.append(request)
        else:
            filtered_request = "{}-filtered_request.json".format(name)
            jq(
                name = "{}-filtered_request".format(name),
                srcs = [request],
                filter_file = jq_pre_filter,
                out = filtered_request,
            )
            args.extend(["--request", "$(rootpath :{})".format(filtered_request)])
            data.append(":{}".format(filtered_request))

    native.sh_test(
        name = name,
        srcs = [runner],
        args = args,
        data = data,
        tags = tags,
        **kwargs
    )

def wrk2_perf_test_suite(
        name,
        endpoint,
        rpc,
        test_files_glob_spec,
        request_rate,
        connections = None,
        duration = None,
        threads = None,
        latency = False,
        timeout = None,
        lua_script = None,
        plaintext = False,
        test_tags = [],
        **kwargs):
    """Generates a test suite for test cases within the specified directory tree.

    Args:
      name: test suite name
      endpoint: struct for endpoint defining the protocol, host, port etc
      rpc: gRPC qualified rpc name
      test_files_glob_spec: glob spec for test files, passed to function functional_test_files_for()
      request_rate: number of requests per second (throughput)
      connections: number of connections to keep open
      duration: duration of test (e.g. 2s, 2m, 2h)
      threads: number of threads to use
      latency:  boolean to enable printing of latency statistics
      timeout:  request timeout (e.g. 2s, 2m, 2h)
      lua_script: label of lua script
      test_tags: tag list for the tests
      plaintext: boolean to indicate plaintext requests
      **kwargs: additional args
    """
    test_files = functional_test_files_for(glob_spec = test_files_glob_spec)
    if not test_files:
        print("no test files found for glob spec: ", test_files_glob_spec)
        return

    test_labels = []
    for test_name, testcase_files in test_files.items():
        qual_test_name = "{}-{}".format(name, test_name)
        test_labels.append(":{}".format(qual_test_name))
        extra_kwargs = dict(kwargs)
        if testcase_files["pre-filter"]:
            extra_kwargs["jq_pre_filter"] = ":{}".format(testcase_files["pre-filter"])
        wrk2_perf_test(
            name = qual_test_name,
            request = ":{}".format(testcase_files["request"]),
            endpoint = endpoint,
            rpc = rpc,
            request_rate = request_rate,
            connections = connections,
            duration = duration,
            threads = threads,
            latency = latency,
            timeout = timeout,
            lua_script = lua_script,
            plaintext = plaintext,
            tags = test_tags,
            **extra_kwargs
        )

    native.test_suite(
        name = name,
        tests = test_labels,
        tags = test_tags,
    )
