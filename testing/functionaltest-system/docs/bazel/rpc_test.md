<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Macros for rpc diff testing and performance testing

<a id="functional_test_files_for"></a>

## functional_test_files_for

<pre>
functional_test_files_for(<a href="#functional_test_files_for-glob_spec">glob_spec</a>, <a href="#functional_test_files_for-request_suffix">request_suffix</a>, <a href="#functional_test_files_for-reply_suffix">reply_suffix</a>, <a href="#functional_test_files_for-pre_filter_suffix">pre_filter_suffix</a>,
                          <a href="#functional_test_files_for-post_filter_suffix">post_filter_suffix</a>, <a href="#functional_test_files_for-post_filter_slurp_suffix">post_filter_slurp_suffix</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="functional_test_files_for-glob_spec"></a>glob_spec |  <p align="center"> - </p>   |  none |
| <a id="functional_test_files_for-request_suffix"></a>request_suffix |  <p align="center"> - </p>   |  <code>".request.json"</code> |
| <a id="functional_test_files_for-reply_suffix"></a>reply_suffix |  <p align="center"> - </p>   |  <code>".reply.json"</code> |
| <a id="functional_test_files_for-pre_filter_suffix"></a>pre_filter_suffix |  <p align="center"> - </p>   |  <code>".pre-filter.jq"</code> |
| <a id="functional_test_files_for-post_filter_suffix"></a>post_filter_suffix |  <p align="center"> - </p>   |  <code>".filter.jq"</code> |
| <a id="functional_test_files_for-post_filter_slurp_suffix"></a>post_filter_slurp_suffix |  <p align="center"> - </p>   |  <code>".filter.slurp.jq"</code> |


<a id="rpc_diff_test"></a>

## rpc_diff_test

<pre>
rpc_diff_test(<a href="#rpc_diff_test-name">name</a>, <a href="#rpc_diff_test-request">request</a>, <a href="#rpc_diff_test-golden_reply">golden_reply</a>, <a href="#rpc_diff_test-endpoint">endpoint</a>, <a href="#rpc_diff_test-rpc">rpc</a>, <a href="#rpc_diff_test-protoset">protoset</a>, <a href="#rpc_diff_test-custom_rpc_invoker_tarball">custom_rpc_invoker_tarball</a>,
              <a href="#rpc_diff_test-jq_pre_filter">jq_pre_filter</a>, <a href="#rpc_diff_test-jq_post_filter">jq_post_filter</a>, <a href="#rpc_diff_test-jq_post_slurp">jq_post_slurp</a>, <a href="#rpc_diff_test-tags">tags</a>, <a href="#rpc_diff_test-plaintext">plaintext</a>, <a href="#rpc_diff_test-client_type">client_type</a>, <a href="#rpc_diff_test-kwargs">kwargs</a>)
</pre>

Generates a diff test for a grpc request/reply.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rpc_diff_test-name"></a>name |  test suite name   |  none |
| <a id="rpc_diff_test-request"></a>request |  label of request file   |  none |
| <a id="rpc_diff_test-golden_reply"></a>golden_reply |  label of reply file   |  none |
| <a id="rpc_diff_test-endpoint"></a>endpoint |  struct for endpoint defining the protocol, host, port etc   |  none |
| <a id="rpc_diff_test-rpc"></a>rpc |  gRPC qualified rpc name   |  none |
| <a id="rpc_diff_test-protoset"></a>protoset |  protobuf descriptor set label or file   |  <code>""</code> |
| <a id="rpc_diff_test-custom_rpc_invoker_tarball"></a>custom_rpc_invoker_tarball |  label for an image tarball used to invoke rpc requests   |  <code>""</code> |
| <a id="rpc_diff_test-jq_pre_filter"></a>jq_pre_filter |  jq filter program as string to apply to the rpc request   |  <code>""</code> |
| <a id="rpc_diff_test-jq_post_filter"></a>jq_post_filter |  jq filter program as string to apply to the rpc response   |  <code>""</code> |
| <a id="rpc_diff_test-jq_post_slurp"></a>jq_post_slurp |  boolean to indicate use of jq --slurp for the rpc response   |  <code>False</code> |
| <a id="rpc_diff_test-tags"></a>tags |  tag list for the tests   |  <code>[]</code> |
| <a id="rpc_diff_test-plaintext"></a>plaintext |  boolean to indicate plaintext request   |  <code>False</code> |
| <a id="rpc_diff_test-client_type"></a>client_type |  client type to use for the rpc request   |  <code>""</code> |
| <a id="rpc_diff_test-kwargs"></a>kwargs |  additional test args   |  none |


<a id="rpc_diff_test_suite"></a>

## rpc_diff_test_suite

<pre>
rpc_diff_test_suite(<a href="#rpc_diff_test_suite-name">name</a>, <a href="#rpc_diff_test_suite-endpoint">endpoint</a>, <a href="#rpc_diff_test_suite-rpc">rpc</a>, <a href="#rpc_diff_test_suite-test_files_glob_spec">test_files_glob_spec</a>, <a href="#rpc_diff_test_suite-protoset">protoset</a>, <a href="#rpc_diff_test_suite-custom_rpc_invoker_tarball">custom_rpc_invoker_tarball</a>,
                    <a href="#rpc_diff_test_suite-test_tags">test_tags</a>, <a href="#rpc_diff_test_suite-plaintext">plaintext</a>, <a href="#rpc_diff_test_suite-kwargs">kwargs</a>)
</pre>

Generate a test suite for test cases within the specified directory tree.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rpc_diff_test_suite-name"></a>name |  test suite name   |  none |
| <a id="rpc_diff_test_suite-endpoint"></a>endpoint |  <p align="center"> - </p>   |  none |
| <a id="rpc_diff_test_suite-rpc"></a>rpc |  <p align="center"> - </p>   |  none |
| <a id="rpc_diff_test_suite-test_files_glob_spec"></a>test_files_glob_spec |  glob spec for test files, passed to function functional_test_files_for()   |  none |
| <a id="rpc_diff_test_suite-protoset"></a>protoset |  protobuf descriptor set label or file   |  <code>""</code> |
| <a id="rpc_diff_test_suite-custom_rpc_invoker_tarball"></a>custom_rpc_invoker_tarball |  label for an image tarball used to invoke rpc requests   |  <code>""</code> |
| <a id="rpc_diff_test_suite-test_tags"></a>test_tags |  tag list for the tests   |  <code>[]</code> |
| <a id="rpc_diff_test_suite-plaintext"></a>plaintext |  boolean to indicate plaintext requests   |  <code>False</code> |
| <a id="rpc_diff_test_suite-kwargs"></a>kwargs |  additional args   |  none |


<a id="rpc_perf_test"></a>

## rpc_perf_test

<pre>
rpc_perf_test(<a href="#rpc_perf_test-name">name</a>, <a href="#rpc_perf_test-request">request</a>, <a href="#rpc_perf_test-endpoint">endpoint</a>, <a href="#rpc_perf_test-rpc">rpc</a>, <a href="#rpc_perf_test-protoset">protoset</a>, <a href="#rpc_perf_test-jq_pre_filter">jq_pre_filter</a>, <a href="#rpc_perf_test-plaintext">plaintext</a>, <a href="#rpc_perf_test-tags">tags</a>, <a href="#rpc_perf_test-kwargs">kwargs</a>)
</pre>

Generate a ghz report for a grpc request.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rpc_perf_test-name"></a>name |  test suite name   |  none |
| <a id="rpc_perf_test-request"></a>request |  label of request file   |  none |
| <a id="rpc_perf_test-endpoint"></a>endpoint |  struct for endpoint defining the protocol, host, port etc   |  none |
| <a id="rpc_perf_test-rpc"></a>rpc |  gRPC qualified rpc name   |  none |
| <a id="rpc_perf_test-protoset"></a>protoset |  protobuf descriptor set label or file   |  none |
| <a id="rpc_perf_test-jq_pre_filter"></a>jq_pre_filter |  jq filter program as string to apply to the rpc request   |  <code>""</code> |
| <a id="rpc_perf_test-plaintext"></a>plaintext |  boolean to indicate plaintext request   |  <code>False</code> |
| <a id="rpc_perf_test-tags"></a>tags |  tag list for the tests   |  <code>[]</code> |
| <a id="rpc_perf_test-kwargs"></a>kwargs |  additional test args   |  none |


<a id="rpc_perf_test_suite"></a>

## rpc_perf_test_suite

<pre>
rpc_perf_test_suite(<a href="#rpc_perf_test_suite-name">name</a>, <a href="#rpc_perf_test_suite-endpoint">endpoint</a>, <a href="#rpc_perf_test_suite-rpc">rpc</a>, <a href="#rpc_perf_test_suite-test_files_glob_spec">test_files_glob_spec</a>, <a href="#rpc_perf_test_suite-protoset">protoset</a>, <a href="#rpc_perf_test_suite-test_tags">test_tags</a>, <a href="#rpc_perf_test_suite-plaintext">plaintext</a>,
                    <a href="#rpc_perf_test_suite-kwargs">kwargs</a>)
</pre>

Generates a test suite for test cases within the specified directory tree.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="rpc_perf_test_suite-name"></a>name |  test suite name   |  none |
| <a id="rpc_perf_test_suite-endpoint"></a>endpoint |  <p align="center"> - </p>   |  none |
| <a id="rpc_perf_test_suite-rpc"></a>rpc |  <p align="center"> - </p>   |  none |
| <a id="rpc_perf_test_suite-test_files_glob_spec"></a>test_files_glob_spec |  glob spec for test files, passed to function functional_test_files_for()   |  none |
| <a id="rpc_perf_test_suite-protoset"></a>protoset |  protobuf descriptor set label or file   |  none |
| <a id="rpc_perf_test_suite-test_tags"></a>test_tags |  tag list for the tests   |  <code>[]</code> |
| <a id="rpc_perf_test_suite-plaintext"></a>plaintext |  boolean to indicate plaintext requests   |  <code>False</code> |
| <a id="rpc_perf_test_suite-kwargs"></a>kwargs |  additional args   |  none |


<a id="wrk2_perf_test"></a>

## wrk2_perf_test

<pre>
wrk2_perf_test(<a href="#wrk2_perf_test-name">name</a>, <a href="#wrk2_perf_test-endpoint">endpoint</a>, <a href="#wrk2_perf_test-rpc">rpc</a>, <a href="#wrk2_perf_test-request_rate">request_rate</a>, <a href="#wrk2_perf_test-request">request</a>, <a href="#wrk2_perf_test-connections">connections</a>, <a href="#wrk2_perf_test-duration">duration</a>, <a href="#wrk2_perf_test-threads">threads</a>, <a href="#wrk2_perf_test-latency">latency</a>,
               <a href="#wrk2_perf_test-timeout">timeout</a>, <a href="#wrk2_perf_test-lua_script">lua_script</a>, <a href="#wrk2_perf_test-jq_pre_filter">jq_pre_filter</a>, <a href="#wrk2_perf_test-plaintext">plaintext</a>, <a href="#wrk2_perf_test-tags">tags</a>, <a href="#wrk2_perf_test-kwargs">kwargs</a>)
</pre>

Generate a ghz report for a grpc request.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="wrk2_perf_test-name"></a>name |  test suite name   |  none |
| <a id="wrk2_perf_test-endpoint"></a>endpoint |  struct for endpoint defining the protocol, host, port etc   |  none |
| <a id="wrk2_perf_test-rpc"></a>rpc |  gRPC qualified rpc name   |  none |
| <a id="wrk2_perf_test-request_rate"></a>request_rate |  number of requests per second (throughput)   |  none |
| <a id="wrk2_perf_test-request"></a>request |  label of request file   |  <code>None</code> |
| <a id="wrk2_perf_test-connections"></a>connections |  number of connections to keep open   |  <code>None</code> |
| <a id="wrk2_perf_test-duration"></a>duration |  duration of test (e.g. 2s, 2m, 2h)   |  <code>None</code> |
| <a id="wrk2_perf_test-threads"></a>threads |  number of threads to use   |  <code>None</code> |
| <a id="wrk2_perf_test-latency"></a>latency |  boolean to enable printing of latency statistics   |  <code>False</code> |
| <a id="wrk2_perf_test-timeout"></a>timeout |  request timeout (e.g. 2s, 2m, 2h)   |  <code>None</code> |
| <a id="wrk2_perf_test-lua_script"></a>lua_script |  label of lua script   |  <code>None</code> |
| <a id="wrk2_perf_test-jq_pre_filter"></a>jq_pre_filter |  jq filter program as string to apply to the rpc request   |  <code>""</code> |
| <a id="wrk2_perf_test-plaintext"></a>plaintext |  boolean to indicate plaintext request   |  <code>False</code> |
| <a id="wrk2_perf_test-tags"></a>tags |  tag list for the tests   |  <code>[]</code> |
| <a id="wrk2_perf_test-kwargs"></a>kwargs |  additional test args   |  none |


<a id="wrk2_perf_test_suite"></a>

## wrk2_perf_test_suite

<pre>
wrk2_perf_test_suite(<a href="#wrk2_perf_test_suite-name">name</a>, <a href="#wrk2_perf_test_suite-endpoint">endpoint</a>, <a href="#wrk2_perf_test_suite-rpc">rpc</a>, <a href="#wrk2_perf_test_suite-test_files_glob_spec">test_files_glob_spec</a>, <a href="#wrk2_perf_test_suite-request_rate">request_rate</a>, <a href="#wrk2_perf_test_suite-connections">connections</a>, <a href="#wrk2_perf_test_suite-duration">duration</a>,
                     <a href="#wrk2_perf_test_suite-threads">threads</a>, <a href="#wrk2_perf_test_suite-latency">latency</a>, <a href="#wrk2_perf_test_suite-timeout">timeout</a>, <a href="#wrk2_perf_test_suite-lua_script">lua_script</a>, <a href="#wrk2_perf_test_suite-plaintext">plaintext</a>, <a href="#wrk2_perf_test_suite-test_tags">test_tags</a>, <a href="#wrk2_perf_test_suite-kwargs">kwargs</a>)
</pre>

Generates a test suite for test cases within the specified directory tree.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="wrk2_perf_test_suite-name"></a>name |  test suite name   |  none |
| <a id="wrk2_perf_test_suite-endpoint"></a>endpoint |  struct for endpoint defining the protocol, host, port etc   |  none |
| <a id="wrk2_perf_test_suite-rpc"></a>rpc |  gRPC qualified rpc name   |  none |
| <a id="wrk2_perf_test_suite-test_files_glob_spec"></a>test_files_glob_spec |  glob spec for test files, passed to function functional_test_files_for()   |  none |
| <a id="wrk2_perf_test_suite-request_rate"></a>request_rate |  number of requests per second (throughput)   |  none |
| <a id="wrk2_perf_test_suite-connections"></a>connections |  number of connections to keep open   |  <code>None</code> |
| <a id="wrk2_perf_test_suite-duration"></a>duration |  duration of test (e.g. 2s, 2m, 2h)   |  <code>None</code> |
| <a id="wrk2_perf_test_suite-threads"></a>threads |  number of threads to use   |  <code>None</code> |
| <a id="wrk2_perf_test_suite-latency"></a>latency |  boolean to enable printing of latency statistics   |  <code>False</code> |
| <a id="wrk2_perf_test_suite-timeout"></a>timeout |  request timeout (e.g. 2s, 2m, 2h)   |  <code>None</code> |
| <a id="wrk2_perf_test_suite-lua_script"></a>lua_script |  label of lua script   |  <code>None</code> |
| <a id="wrk2_perf_test_suite-plaintext"></a>plaintext |  boolean to indicate plaintext requests   |  <code>False</code> |
| <a id="wrk2_perf_test_suite-test_tags"></a>test_tags |  tag list for the tests   |  <code>[]</code> |
| <a id="wrk2_perf_test_suite-kwargs"></a>kwargs |  additional args   |  none |


