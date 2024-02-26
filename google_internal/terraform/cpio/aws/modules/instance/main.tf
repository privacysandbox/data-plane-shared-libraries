/**
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

####################################################
# Create EC2 IAM policy document to share with SSH instance and server instance.
####################################################
data "aws_iam_policy_document" "ec2_assume_role_policy_doc" {
  statement {
    actions = ["sts:AssumeRole"]

    principals {
      type        = "Service"
      identifiers = ["ec2.amazonaws.com"]
    }
  }
}

####################################################
# Create SSH instance role, policy, and profile.
####################################################
data "aws_iam_policy_document" "ssh_instance_policy_doc" {
  statement {
    sid       = "AllowSSHInstanceToSendSSHPublicKey"
    actions   = ["ec2-instance-connect:SendSSHPublicKey"]
    resources = ["arn:aws:ec2:*:*:instance/*"]
    condition {
      test     = "StringEquals"
      variable = "aws:ResourceTag/environment"
      values   = [var.environment]
    }
    condition {
      test     = "StringEquals"
      variable = "ec2:osuser"
      values   = ["ec2-user"]
    }
  }
  statement {
    sid       = "AllowSSHInstanceToDescribeInstances"
    actions   = ["ec2:DescribeInstances"]
    resources = ["*"]
  }
}

resource "aws_iam_role" "ssh_instance_role" {
  name               = format("%s-%s-sshInstanceRole", var.service, var.environment)
  assume_role_policy = data.aws_iam_policy_document.ec2_assume_role_policy_doc.json

  tags = {
    Name = format("%s-%s-sshInstanceRole", var.service, var.environment)
  }
}

resource "aws_iam_instance_profile" "ssh_instance_profile" {
  name = format("%s-%s-sshInstanceProfile", var.service, var.environment)
  role = aws_iam_role.ssh_instance_role.name

  tags = {
    Name = format("%s-%s-sshInstanceProfile", var.service, var.environment)
  }
}

resource "aws_iam_policy" "ssh_instance_policy" {
  name   = format("%s-%s-sshInstancePolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.ssh_instance_policy_doc.json
}

resource "aws_iam_role_policy_attachment" "ssh_instance_access" {
  policy_arn = aws_iam_policy.ssh_instance_policy.arn
  role       = aws_iam_role.ssh_instance_role.name
}

####################################################
# Create an public subnet EC2 instance for SSH into private subnet.
####################################################
data "aws_ami" "amazon_linux" {
  most_recent = true
  owners = [
    "amazon"
  ]

  filter {
    name = "name"

    values = [
      "amzn2-ami-hvm*-x86_64-gp2",
    ]
  }

  filter {
    name = "owner-alias"

    values = [
      "amazon",
    ]
  }
}

resource "aws_instance" "cpio_ssh_instance" {
  ami                  = data.aws_ami.amazon_linux.id
  instance_type        = "t2.micro"
  key_name             = var.aws_ec2_key_pair_name
  iam_instance_profile = aws_iam_instance_profile.ssh_instance_profile.name

  vpc_security_group_ids = [var.ssh_instance_security_group_id]
  subnet_id              = var.vpc_public_subnet_ids[0]

  tags = {
    Name = "${var.service}-${var.environment}-ssh-instance"
  }

  # Enforce IMDSv2.
  metadata_options {
    http_endpoint          = "enabled"
    http_tokens            = "required"
    instance_metadata_tags = "enabled"
  }

  user_data = <<EOF
    #!/bin/bash

    pip3 install ec2instanceconnectcli
  EOF
}

####################################################
# Set up policy group for SSH users to use EC2 instance connect.
####################################################
resource "aws_iam_group" "ssh_users_group" {
  name = format("%s-%s-ssh-users", var.service, var.environment)
}

data "aws_iam_policy_document" "ssh_users_group_policy_doc" {
  statement {
    sid       = "AllowSSHUsersToSendSSHPublicKey"
    actions   = ["ec2-instance-connect:SendSSHPublicKey"]
    resources = [aws_instance.cpio_ssh_instance.arn]
    condition {
      test     = "StringEquals"
      variable = "ec2:osuser"
      values   = ["ec2-user"]
    }
  }
  statement {
    sid       = "AllowSSHUsersToDescribeInstances"
    actions   = ["ec2:DescribeInstances"]
    resources = ["*"]
  }
}

resource "aws_iam_policy" "ssh_users_group_policy" {
  name   = format("%s-%s-sshUsersGroupPolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.ssh_users_group_policy_doc.json
}

resource "aws_iam_group_policy_attachment" "ssh_users_group_access" {
  policy_arn = aws_iam_policy.ssh_users_group_policy.arn
  group      = aws_iam_group.ssh_users_group.name
}

####################################################
# Create instance role, policy, and profile for EC2 instance.
####################################################

data "aws_iam_policy_document" "instance_policy_doc" {
  statement {
    sid       = "AllowInstancesToReadAllInstancesData"
    actions   = ["ec2:Describe*"]
    resources = ["*"]
    effect    = "Allow"
  }
  statement {
    sid       = "AllowInstancesToListS3DataFiles"
    actions   = ["s3:ListBucket"]
    resources = ["arn:aws:s3:::${var.aws_s3_bucket_name}"]
    effect    = "Allow"
  }
  statement {
    sid       = "AllowInstancesToGetListPutS3DataFiles"
    actions   = ["s3:GetObject", "s3:ListBucket", "s3:PutObject"]
    resources = ["arn:aws:s3:::${var.aws_s3_bucket_name}/*"]
    effect    = "Allow"
  }
  statement {
    sid       = "AllowInstancesToReadParameters"
    actions   = ["ssm:GetParameter"]
    resources = ["${var.aws_ssm_parameter_arn}"]
    effect    = "Allow"
  }
  statement {
    sid       = "AllowInstancesToAssumeRole"
    actions   = ["sts:AssumeRole"]
    effect    = "Allow"
    resources = ["*"]
  }
  statement {
    sid = "AllowInstancesToManageSqsQueues"
    actions = [
      "sqs:ChangeMessageVisibility",
      "sqs:DeleteMessage",
      "sqs:GetQueueUrl",
      "sqs:ReceiveMessage",
      "sqs:SendMessage"
    ]
    resources = ["*"]
  }
  statement {
    sid       = "AllowSSHInstanceToSendSSHPublicKey"
    actions   = ["ec2-instance-connect:SendSSHPublicKey"]
    resources = ["arn:aws:ec2:*:*:instance/*"]
    condition {
      test     = "StringEquals"
      variable = "aws:ResourceTag/environment"
      values   = [var.environment]
    }
    condition {
      test     = "StringEquals"
      variable = "ec2:osuser"
      values   = ["ec2-user"]
    }
  }
  statement {
    sid       = "AllowSSHInstanceToDescribeInstances"
    actions   = ["ec2:DescribeInstances"]
    resources = ["*"]
  }
}

resource "aws_iam_role" "instance_role" {
  name               = format("%s-%s-InstanceRole", var.service, var.environment)
  assume_role_policy = data.aws_iam_policy_document.ec2_assume_role_policy_doc.json

  tags = {
    Name = format("%s-%s-InstanceRole", var.service, var.environment)
  }
}

resource "aws_iam_policy" "instance_policy" {
  name   = format("%s-%s-InstancePolicy", var.service, var.environment)
  policy = data.aws_iam_policy_document.instance_policy_doc.json
}

resource "aws_iam_role_policy_attachment" "instance_role_policy_attachment" {
  policy_arn = aws_iam_policy.instance_policy.arn
  role       = aws_iam_role.instance_role.name
}

resource "aws_iam_instance_profile" "instance_profile" {
  name = format("%s-%s-InstanceProfile", var.service, var.environment)
  role = aws_iam_role.instance_role.name

  tags = {
    Name = format("%s-%s-InstanceProfile", var.service, var.environment)
  }
}

####################################################
# Create the EC2 instance in private subnet.
####################################################
resource "aws_instance" "cpio_instance" {
  depends_on = [
    aws_iam_role_policy_attachment.instance_role_policy_attachment
  ]
  ami                  = var.aws_ec2_instance_ami
  instance_type        = var.aws_ec2_instance_type
  iam_instance_profile = aws_iam_instance_profile.instance_profile.name

  vpc_security_group_ids = [var.instance_security_group_id]
  subnet_id              = var.vpc_private_subnet_ids[0]

  tags = {
    Name = "${var.service}-${var.environment}-instance"
  }

  # Enable Nitro enclaves.
  enclave_options {
    enabled = true
  }

  # Enforce IMDSv2.
  metadata_options {
    http_endpoint          = "enabled"
    http_tokens            = "required"
    instance_metadata_tags = "enabled"
  }
}
