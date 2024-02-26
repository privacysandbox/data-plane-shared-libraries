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

################################################################################
# Restrict VPC endpoint access only to instances in this environment and service.
################################################################################
data "aws_iam_policy_document" "vpc_endpoint_policy_doc" {
  statement {
    sid       = "AllowVpcEndpointsToBeUsedByAllInstances"
    actions   = ["*"]
    effect    = "Allow"
    resources = ["*"]
    principals {
      type        = "AWS"
      identifiers = ["*"]
    }
    condition {
      test     = "ArnEquals"
      variable = "aws:PrincipalArn"
      values   = [var.instance_role_arn, var.ssh_instance_role_arn]
    }
  }
  statement {
    sid     = "AllowVpcEndpointsYumAccess"
    actions = ["s3:*"]
    effect  = "Allow"
    resources = [
      "arn:aws:s3:::packages.*.amazonaws.com/*",
      "arn:aws:s3:::repo.*.amazonaws.com/*",
      "arn:aws:s3:::amazonlinux-2-repos-${var.region}/*",
      "arn:aws:s3:::amazonlinux-2-repos-${var.region}*",
      "arn:aws:s3:::amazonlinux.*.amazonaws.com/*",
      "arn:aws:s3:::*.amazonaws.com",
      "arn:aws:s3:::*.amazonaws.com/*",
      "arn:aws:s3:::*.${var.region}.amazonaws.com/*",
      "arn:aws:s3:::*.${var.region}.amazonaws.com/",
      "arn:aws:s3:::*repos.${var.region}-.amazonaws.com",
      "arn:aws:s3:::*repos.${var.region}.amazonaws.com/*",
      "arn:aws:s3:::repo.${var.region}-.amazonaws.com",
      "arn:aws:s3:::repo.${var.region}.amazonaws.com/*"
    ]
    principals {
      type        = "AWS"
      identifiers = ["*"]
    }
  }
}

################################################################################
# Create endpoints for accessing AWS services.
################################################################################

# Create gateway endpoints for accessing AWS services.
resource "aws_vpc_endpoint" "vpc_gateway_endpoint" {
  for_each = toset([
    "s3"
  ])
  service_name      = "com.amazonaws.${var.region}.${each.key}"
  vpc_id            = var.vpc_id
  vpc_endpoint_type = "Gateway"
  # Private route tables.
  route_table_ids = var.vpc_private_route_table_ids
  policy          = data.aws_iam_policy_document.vpc_endpoint_policy_doc.json

  tags = {
    Name = "${var.service}-${var.environment}-${each.key}-endpoint"
  }
}

# Create interface endpoints for accessing AWS services.
resource "aws_vpc_endpoint" "vpc_interface_endpoint" {
  for_each = toset([
    "ec2",
    "ssm",
    "sqs",
    "kms",
    # "sns",
    # "autoscaling",
    # "xray",
    # "logs",
  ])
  service_name        = "com.amazonaws.${var.region}.${each.key}"
  vpc_id              = var.vpc_id
  vpc_endpoint_type   = "Interface"
  subnet_ids          = var.vpc_private_subnet_ids
  private_dns_enabled = true
  policy              = data.aws_iam_policy_document.vpc_endpoint_policy_doc.json

  security_group_ids = [var.vpc_endpoint_security_group_id]

  tags = {
    Name = "${var.service}-${var.environment}-${each.key}-endpoint"
  }
}
