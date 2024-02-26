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

terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 4.5"
    }
  }

  required_version = "1.2.3"
}

locals {
  service = "cpio-validator"
}

provider "aws" {
  region = var.region
  default_tags {
    tags = {
      service     = local.service
      environment = var.environment
    }
  }
}

####################################################
# S3.
####################################################
module "blob_storage" {
  source              = "./modules/blob_storage"
  environment         = var.environment
  aws_account         = var.aws_account
  aws_s3_bucket_name  = var.aws_s3_bucket_name
  aws_s3_blob_name    = var.aws_s3_blob_name
  aws_s3_blob_content = var.aws_s3_blob_content
}

####################################################
# KMS.
####################################################
module "kms" {
  source       = "./modules/kms"
  region       = var.region
  service      = local.service
  environment  = var.environment
  aws_username = var.aws_username
}

####################################################
# SSM Parameter Store.
####################################################
module "parameter" {
  source      = "./modules/parameter"
  service     = local.service
  environment = var.environment
}

####################################################
# EC2.
####################################################
module "instance" {
  source                         = "./modules/instance"
  region                         = var.region
  service                        = local.service
  environment                    = var.environment
  aws_username                   = var.aws_username
  aws_ssm_parameter_arn          = module.parameter.aws_ssm_parameter_arn
  aws_ec2_instance_ami           = var.aws_ec2_instance_ami
  aws_ec2_instance_type          = var.aws_ec2_instance_type
  aws_ec2_key_pair_name          = var.aws_ec2_key_pair_name
  ssh_instance_security_group_id = module.security.ssh_instance_security_group_id
  instance_security_group_id     = module.security.instance_security_group_id
  aws_s3_bucket_name             = var.aws_s3_bucket_name
  vpc_public_subnet_ids          = module.vpc.vpc_public_subnet_ids
  vpc_private_subnet_ids         = module.vpc.vpc_private_subnet_ids
}

####################################################
# Internal Modules.
####################################################
module "vpc" {
  source      = "./modules/vpc"
  region      = var.region
  service     = local.service
  environment = var.environment
}

module "security" {
  source                                         = "./modules/security"
  region                                         = var.region
  service                                        = local.service
  environment                                    = var.environment
  vpc_id                                         = module.vpc.vpc_id
  vpc_endpoint_gateway_endpoints_prefix_list_ids = module.vpc_endpoints.vpc_endpoint_gateway_endpoints_prefix_list_ids
}

module "vpc_endpoints" {
  source                         = "./modules/vpc_endpoints"
  region                         = var.region
  service                        = local.service
  environment                    = var.environment
  vpc_id                         = module.vpc.vpc_id
  vpc_private_subnet_ids         = module.vpc.vpc_private_subnet_ids
  vpc_private_route_table_ids    = module.vpc.vpc_private_route_table_ids
  vpc_endpoint_security_group_id = module.security.vpc_endpoint_security_group_id
  instance_role_arn              = module.instance.instance_role_arn
  ssh_instance_role_arn          = module.instance.ssh_instance_role_arn
}
