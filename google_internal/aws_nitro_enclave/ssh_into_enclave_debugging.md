# SSH into Nitro Enclave (Debugging)

This document discusses how to SSH into a Nitro Enclave for debugging.

## Setting Up AWS EC2 Instance

1. Create an
   [AWS Account](https://docs.google.com/document/d/19CEJRhr5KokUrTC6LpiEw9M6nmGI4_lkzW9nTnSwN9Y/edit#heading=h.ox30xgy5qgel).
1. On the left hand side of your AWS account, go to click on `Key Pairs`, you should see keys with
   familiar names.
1. Create your own and download the `.pem` file locally.
1. Next, go to `EC2` and click `Launch Instances` on the top right corner.
1. Name your instance, select Amazon Linux as Machine Image, and change AMI to Amazon Linux 2 AMI.
1. Set instance type to be `m5.xlarge`.
1. Set the key pair to be the key pair you previously created.
1. Create security group and set it to allow SSH traffic from your IP
1. Under advanced details, set `Nitro Enclave` to enable
1. Create the instance
1. Navigate back to the table of instances and click the instance ID of your new instance.
1. Click connect on the top right corner.
1. Follow the instructions from SSH client tab to ensure you can SSH into your instance.
1. Once you are able to SSH, go to
   [Installing the Nitro Enclaves CLI on Linux - AWS](https://docs.aws.amazon.com/enclaves/latest/user/nitro-enclave-cli-install.html)
   and follow instructions to install the nitro-cli on your EC2 instance.

## Building Proxy

1. Use `scripts/build-proxy` to build proxy.
1. Look for `dist/aws/proxy-al2-amd64.zip` and unzip it, this is the binary needed by the EC2
   instance created. Example -

```shell
unzip dist/aws/proxy-al2-amd64.zip
```

## (Optional) Building `resolv_conf_getter` Server

The `resolv_conf_getter` is a tool for copying the host network configuration into the enclave.

```shell
builders/tools/bazel-debian run //src/aws/proxy:copy_to_dist
```

## Creating an EIF from Docker Image

1. Add the layers you want to debug to `aws_nitro_enclaves_debug_debian_image` in
   [BUILD.bazel](./BUILD.bazel).
1. Build the image.

    ```shell
    builders/tools/bazel-debian build //google_internal/aws_nitro_enclave:aws_nitro_enclaves_debug_debian_image.tar
    ```

1. Use `builders/tools/convert-docker-to-nitro` to convert your Docker image to an EIF. Example -

    ```shell
    builders/tools/convert-docker-to-nitro --docker-image-tar bazel-bin/google_internal/aws_nitro_enclave/aws_nitro_enclaves_debug_debian_image.tar --docker-image-tag aws_nitro_enclaves_debug_debian_image --outdir google_internal/aws_nitro_enclave/ --eif-name aws_nitro_enclaves_debug_debian_image --docker-image-uri bazel/google_internal/aws_nitro_enclave
    ```

## Copying Over Required Files to EC2

Finally, upload all the needed files to your EC2 instance using SCP.

Files to be uploaded include -

-   `ssh_host_rsa_key` (generated during [building Docker image](#creating-an-eif-from-docker-image)
    phase)
-   `eif` (generated during [building .eif](#creating-an-eif-from-docker-image) phase)
-   `proxy` (generated during [building proxy](#building-proxy) phase).

Optionally, also upload -

-   `resolv_conf_getter_server` (generated during
    [building `resolv_conf_getter` server](#optional-building-resolv_conf_getter-server) phase).

Example -

```shell
EC2_ADDR=<address of your EC2 instance>
EC2_PEM=<.pem file with path>
scp -i ${EC2_PEM} \
  google_internal/aws_nitro_enclave/aws_nitro_enclaves_debug_debian_image.eif \
  bazel-bin/google_internal/aws_nitro_enclave/ssh_host_rsa_key proxy \
  dist/aws/resolv_conf_getter_server_debian_image.tar \
  ec2-user@${EC2_ADDR}:/home/ec2-user/
```

## Setup from EC2

1. SSH into your EC2 instance. Example -

    ```shell
    ssh -i ${EC2_PEM} ec2-user@${EC2_ADDR}
    ```

    Optionally, check if all the files uploaded are available.

1. Restrict access to your SSH key.

    ```shell
    chmod 0400 ssh_host_rsa_key
    ```

1. Install socat in your EC2 instance. Note that the version needs to be (>=1.7.4). Example -

    ```shell
    sudo yum update -y
    sudo yum install gcc make curl -y
    mkdir -p /tmp/socat-build
    cd /tmp/socat-build
    curl -L http://www.dest-unreach.org/socat/download/socat-1.7.4.4.tar.gz \
      | tar -xz --strip-components=1
    ./configure
    make -j socat
    sudo mv /tmp/socat-build/socat /
    cd /
    rm -rf /tmp/socat-build
    ```

    Optionally, check if socat was installed successfully.

    ```shell
    /socat -V
    ```

1. Run the Enclave.

    ```shell
    nitro-cli run-enclave --cpu-count 2 --memory 1684 --eif-path aws_nitro_enclaves_debug_debian_image.eif --enclave-cid 10 --debug-mode && nitro-cli console --enclave-name aws_nitro_enclaves_debug_debian_image
    ```

1. From a new terminal SSH-ed into your EC2 instance, run proxy.

    ```shell
    ~/proxy
    ```

1. From a new terminal SSH-ed into your EC2 instance and start up socat, backgrounding it.

    ```shell
    /socat TCP4-LISTEN:2222,reuseaddr,fork VSOCK-CONNECT:10:5006 &
    ```

1. Finally, you should be able to SSH into the enclave.

    ```shell
    ssh -i ssh_host_rsa_key -p 2222 -v -v root@localhost
    ```

1. (Optional) From a new terminal SSH-ed into your EC2 instance, load and run the resolv_conf_getter
   server.

```shell
docker load -i resolv_conf_getter_server_debian_image.tar
docker run -p 1600:1600 bazel/src/aws/proxy:resolv_conf_getter_server_debian_image
```

1. (Optional) From a new terminal SSH-ed into the enclave in your EC2 instance, run the
   resolv_conf_getter client.

```shell
/proxify /resolv_conf_getter_client
```

## SSH-ing into the Enclave from your Cloudtop

For SSH-ing into the running Enclave from your Cloudtop -

```shell
ssh -i bazel-bin/google_internal/aws_nitro_enclave/ssh_host_rsa_key root@${EC2_ADDR} -p 2222
```

If you face any issues with the `ssh_host_rsa_key`, consider retricting access using `chmod`.

For copying files into the running Enclave -

```shell
scp -i bazel-bin/google_internal/aws_nitro_enclave/ssh_host_rsa_key -P 2222 <files to be copied> root@${EC2_ADDR}:
```

## Frequently Asked Questions

1. Are you having trouble SSH-ing into EC2 instance?

    Take a look at your security group. For SSH-ing from your Cloudtop or for smoother SSH
    experience, open up your inbound rules to allow SSH-ing from Anywhere-IPv4 temporarily.

Need help debugging? Reach out to <knuts-and-bolts@google.com>.
