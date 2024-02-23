# CPIO deployment example

Provides an example about how to deploy a binary using CPIO to Nitro enclave.

## Create AMI

Config AWS credential in ~/.aws/credentials.

Run the following command to create the AMI:

        bazel run --//:platform=aws src/public/cpio/examples/deploy/aws/enclave:cpio_test

## Bring up EC2 instance

1.  In this example, we are bringing up the EC2 instance through terraform. Follow the instructions
    [here](https://developer.hashicorp.com/terraform/tutorials/aws-get-started) to setup terraform.
2.  Create an AWS Service IAM role in AWS console and grant proper access (depends on what AWS
    services the test binary talks to). Or find an existing IAM role.
3.  Create a key pair in the region us-east-1 which is used for this test by default. This is needed
    to SSH to the EC2 instance. Or find an exsiting key pair
4.  Create a subnet or use the existing default subnet in the region.
5.  Find the ami_id from the result of the previous "Create AMI" step. Then run the following
    commands:

        cd cc/public/cpio/examples/deploy/aws/enclave/terraform

        terraform init

        terraform apply -var "instance_name=$name" -var "iam_role=$iam_role_name" -var "key_pair_name=$key_pair_name" -var "subnet_id=$subnet_id" -var "ami=$ami_id"

## Run binary inside Nitro enclave

1.  Go to the instance details page in AWS console, find the command to SSH to the instance under
    Connect category.
2.  SSH to the instance as ec2-user. An example command is:

        ssh -i "key_pair.pem" ec2-user@ec2-54-91-11-108.compute-1.amazonaws.com

3.  The binary is already running inside the Enclave when the instance is up. Run the following
    command to see the log.

        nitro-cli console --enclave-id $(nitro-cli describe-enclaves | jq .[0].EnclaveID -r)

4.  If need to rerun the enclave, terminate the enclave first by running:

         nitro-cli terminate-enclave --enclave-id $(nitro-cli describe-enclaves | jq .[0].EnclaveID -r)

    Then run the following command to rerun the enclave:

        nitro-cli run-enclave --cpu-count=2 --memory=7168 --eif-path /opt/google/scp/enclave.eif

## Destroy resource

After testing, make sure to run the following command to destroy what are created through the
terraform before you make any changes to the terraform:

        terraform destroy -var "instance_name=$name" -var "iam_role=$iam_role_name" -var "key_pair_name=$key_pair_name" -var "subnet_id=$subnet_id" -var "ami=$ami_id"

If forget to destroy resource, you will encounter entity already exists issue in future test. To
solve them:

1. Delete the network_interface attached with the created security group.
2. Delete the security group.
3. Detach role from the instance profile by running

    aws iam remove-role-from-instance-profile --instance-profile-name profile-name --role-name
    role-name

4. Delete the instance profile by running

    aws iam delete-instance-profile --instance-profile-name profile-name
