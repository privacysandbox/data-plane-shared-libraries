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

# S3
resource "aws_s3_bucket" "bucket" {
  bucket = var.aws_s3_bucket_name

  # Delete all files in bucket during `terraform destory`.
  force_destroy = true

  tags = {
    Name = var.aws_s3_bucket_name
  }
}

resource "aws_s3_object" "blob" {
  depends_on = [
    aws_s3_bucket.bucket
  ]
  bucket  = var.aws_s3_bucket_name
  key     = var.aws_s3_blob_name
  content = var.aws_s3_blob_content

  tags = {
    Name = var.aws_s3_blob_name
  }
}

resource "aws_s3_bucket_policy" "cpio-validator" {
  bucket = aws_s3_bucket.bucket.id
  policy = data.aws_iam_policy_document.cpio-validator.json
}

data "aws_iam_policy_document" "cpio-validator" {
  statement {
    actions   = ["s3:ListBucket"]
    resources = ["arn:aws:s3:::${var.aws_s3_bucket_name}"]
    effect    = "Allow"
    principals {
      type        = "AWS"
      identifiers = ["arn:aws:iam::${var.aws_account}:root"]
    }
  }
  statement {
    actions   = ["s3:GetObject", "s3:PutObject"]
    resources = ["arn:aws:s3:::${var.aws_s3_bucket_name}/*"]
    effect    = "Allow"
    principals {
      type        = "AWS"
      identifiers = ["arn:aws:iam::${var.aws_account}:root"]
    }
  }
}
