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

variable "region" {
  description = "Cloud region."
  type        = string
}

variable "service" {
  description = "Assigned name of the CPIO Validator."
  type        = string
}

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
}

variable "aws_username" {
  description = "AWS username."
  type        = string
}

variable "aws_ec2_instance_ami" {
  description = "ID of AWS AMI to create a EC2 instance."
  type        = string
}

variable "aws_ec2_instance_type" {
  description = "EC2 instance type."
  type        = string
}

variable "aws_ec2_key_pair_name" {
  description = "Key pair to assign to EC2 instance at launch."
  type        = string
}

variable "aws_ssm_parameter_arn" {
  description = "ARN of created SSM Parameter Store parameter."
  type        = string
}

variable "ssh_instance_security_group_id" {
  description = "Security group ID for public subnet SSH instance."
  type        = string
}

variable "instance_security_group_id" {
  description = "Security group ID for private subnet instance."
  type        = string
}

variable "aws_s3_bucket_name" {
  description = "Name of the AWS S3 bucket to be created."
  type        = string
}

variable "vpc_public_subnet_ids" {
  description = "A list of public subnet ids to launch the ssh instance in."
  type        = list(string)
}

variable "vpc_private_subnet_ids" {
  description = "A list of private subnet ids to launch the instance in."
  type        = list(string)
}
