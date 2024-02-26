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
# Security Groups.
################################################################################

# Control ingress and egress traffic for the public subnet ssh ec2 instance.
resource "aws_security_group" "ssh_security_group" {
  name   = "${var.service}-${var.environment}-ssh-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-ssh-sg"
  }
}

# Control ingress and egress traffic for the private subnet EC2 instance.
resource "aws_security_group" "instance_security_group" {
  name   = "${var.service}-${var.environment}-instance-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-instance-sg"
  }
}

# Security group to control ingress and egress traffic to VPC endpoints.
resource "aws_security_group" "vpc_endpoint_security_group" {
  name   = "${var.service}-${var.environment}-vpc-endpoint-sg"
  vpc_id = var.vpc_id

  tags = {
    Name = "${var.service}-${var.environment}-vpc-endpoint-sg"
  }
}

################################################################################
# Ingress and egress rules for SSH.
################################################################################
resource "aws_security_group_rule" "allow_all_ssh_ingress" {
  protocol          = "TCP"
  type              = "ingress"
  from_port         = 22
  to_port           = 22
  security_group_id = aws_security_group.ssh_security_group.id
  cidr_blocks       = ["0.0.0.0/0"]
}

resource "aws_security_group_rule" "allow_ssh_to_ec2_egress" {
  protocol                 = "TCP"
  type                     = "egress"
  from_port                = 22
  to_port                  = 22
  security_group_id        = aws_security_group.ssh_security_group.id
  source_security_group_id = aws_security_group.instance_security_group.id
}

resource "aws_security_group_rule" "allow_ssh_secure_tcp_egress" {
  protocol          = "TCP"
  type              = "egress"
  from_port         = 443
  to_port           = 443
  cidr_blocks       = ["0.0.0.0/0"]
  security_group_id = aws_security_group.ssh_security_group.id
}

# # Egress security group rule for ec2 instance to use Proxy.
# resource "aws_security_group_rule" "allow_ec2_to_proxify" {
#   from_port                = 8888
#   protocol                 = "TCP"
#   security_group_id        = aws_security_group.instance_security_group.id
#   to_port                  = 8888
#   type                     = "egress"
#   cidr_blocks              = ["0.0.0.0/0"]
# }

################################################################################
# Ingress and egress rules for private subnet ec2 instances.
################################################################################
resource "aws_security_group_rule" "allow_ssh_to_ec2_ingress" {
  protocol                 = "TCP"
  type                     = "ingress"
  from_port                = 22
  to_port                  = 22
  security_group_id        = aws_security_group.instance_security_group.id
  source_security_group_id = aws_security_group.ssh_security_group.id
}

# Egress security group rules for ec2 instance to VPC endpoints.
resource "aws_security_group_rule" "allow_ec2_to_vpc_endpoint_egress" {
  protocol                 = "TCP"
  type                     = "egress"
  from_port                = 443
  to_port                  = 443
  security_group_id        = aws_security_group.instance_security_group.id
  source_security_group_id = aws_security_group.vpc_endpoint_security_group.id
}

resource "aws_security_group_rule" "allow_ec2_to_vpc_ge_egress" {
  protocol          = "TCP"
  type              = "egress"
  from_port         = 443
  to_port           = 443
  security_group_id = aws_security_group.instance_security_group.id
  prefix_list_ids   = [for id in var.vpc_endpoint_gateway_endpoints_prefix_list_ids : id]
}

# Ingress and egress rules for backend vpc interface endpoints.
resource "aws_security_group_rule" "allow_ec2_to_vpce_ingress" {
  protocol                 = "TCP"
  type                     = "ingress"
  from_port                = 443
  to_port                  = 443
  security_group_id        = aws_security_group.vpc_endpoint_security_group.id
  source_security_group_id = aws_security_group.instance_security_group.id
}

resource "aws_security_group_rule" "allow_ssh_instance_to_vpce_ingress" {
  protocol                 = "TCP"
  type                     = "ingress"
  from_port                = 0
  to_port                  = 65535
  security_group_id        = aws_security_group.vpc_endpoint_security_group.id
  source_security_group_id = aws_security_group.ssh_security_group.id
}

resource "aws_security_group_rule" "allow_ec2_to_ec2_endpoint_egress" {
  protocol                 = "TCP"
  type                     = "egress"
  from_port                = 50100
  to_port                  = 50100
  security_group_id        = aws_security_group.instance_security_group.id
  source_security_group_id = aws_security_group.instance_security_group.id
}

resource "aws_security_group_rule" "allow_ec2_to_ec2_endpoint_ingress" {
  protocol                 = "TCP"
  type                     = "ingress"
  from_port                = 50100
  to_port                  = 50100
  security_group_id        = aws_security_group.instance_security_group.id
  source_security_group_id = aws_security_group.instance_security_group.id
}

# end

data "aws_ip_ranges" "ec2_instance_connect_ip_ranges" {
  regions  = [var.region]
  services = ["ec2_instance_connect"]
}

resource "aws_security_group_rule" "allow_ec2_instance_connect_ingress" {
  protocol          = "TCP"
  type              = "ingress"
  from_port         = 22
  to_port           = 22
  cidr_blocks       = data.aws_ip_ranges.ec2_instance_connect_ip_ranges.cidr_blocks
  security_group_id = aws_security_group.instance_security_group.id
}
