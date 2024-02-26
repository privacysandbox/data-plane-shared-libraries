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

variable "environment" {
  description = "Assigned environment name to group related resources."
  type        = string
  validation {
    condition     = length(var.environment) <= 10
    error_message = "Due to current naming scheme limitations, environment must not be longer than 10."
  }
}

variable "aws_account" {
  description = "AWS account number, 12 digits long."
  type        = string
}

variable "aws_s3_bucket_name" {
  description = "Name of the AWS S3 bucket to be created."
  type        = string
}

variable "aws_s3_blob_name" {
  description = "Name of the AWS S3 blob to be created."
  type        = string
}

variable "aws_s3_blob_content" {
  description = "String content to put into aws_s3_blob_name AWS S3 blob."
  type        = string
}
