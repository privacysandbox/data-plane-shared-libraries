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

variable "instance_role_arn" {
  description = "The IAM role for the EC2 instance."
  type        = string
}

variable "ssh_instance_role_arn" {
  description = "The IAM role for the SSH EC2 instance."
  type        = string
}

variable "vpc_id" {
  description = "VPC ID where security groups will be created."
  type        = string
}

variable "vpc_private_route_table_ids" {
  description = "Private route tables where gateway endpoint routes will be added."
  type        = set(string)
}

variable "vpc_private_subnet_ids" {
  description = "Private subnets associated with VPC endpoints."
  type        = set(string)
}

variable "vpc_endpoint_security_group_id" {
  description = "Security group for interface endpoints."
  type        = string
}
