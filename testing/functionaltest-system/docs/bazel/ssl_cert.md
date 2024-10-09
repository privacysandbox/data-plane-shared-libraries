<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Macro for generating SSL certificates.

<a id="generate_ca_bundle"></a>

## generate_ca_bundle

<pre>
generate_ca_bundle(<a href="#generate_ca_bundle-name">name</a>, <a href="#generate_ca_bundle-certs">certs</a>, <a href="#generate_ca_bundle-test_tags">test_tags</a>)
</pre>

Generates a root CA bundle from a list of signed SSL certificates.

**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="generate_ca_bundle-name"></a>name |  name of bundle target   |  none |
| <a id="generate_ca_bundle-certs"></a>certs |  list of target certificates   |  none |
| <a id="generate_ca_bundle-test_tags"></a>test_tags |  optional list of test tags   |  <code>[]</code> |


<a id="generate_ssl_certificate"></a>

## generate_ssl_certificate

<pre>
generate_ssl_certificate(<a href="#generate_ssl_certificate-name">name</a>, <a href="#generate_ssl_certificate-domain">domain</a>, <a href="#generate_ssl_certificate-test_tags">test_tags</a>)
</pre>

Generates a set of files for a signed SSL certificate.

The generated target files are:
    root_ca.cert
    private.key
    signed.cert


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="generate_ssl_certificate-name"></a>name |  base name for underlying targets and name of generated filegroup   |  none |
| <a id="generate_ssl_certificate-domain"></a>domain |  certificate domain   |  none |
| <a id="generate_ssl_certificate-test_tags"></a>test_tags |  optional list of test tags   |  <code>[]</code> |


