/*
 * Copyright 2022 Google LLC
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

#ifndef SCP_CPIO_INTERFACE_JOB_CLIENT_TYPE_DEF_H_
#define SCP_CPIO_INTERFACE_JOB_CLIENT_TYPE_DEF_H_

#include <optional>
#include <string>

namespace google::scp::cpio {
/// Configurations for JobClient.
struct JobClientOptions {
  virtual ~JobClientOptions() = default;

  JobClientOptions() = default;

  JobClientOptions(const JobClientOptions& options)
      : job_queue_name(options.job_queue_name),
        job_table_name(options.job_table_name),
        gcp_spanner_instance_name(options.gcp_spanner_instance_name),
        gcp_spanner_database_name(options.gcp_spanner_database_name) {}

  // The name of the queue to store job message.
  std::string job_queue_name;

  // The name of the table to store job data.
  std::string job_table_name;

  // The Spanner Instance to use for GCP. Unused for AWS.
  std::string gcp_spanner_instance_name;

  // The Spanner Database to use for GCP. Unused for AWS.
  std::string gcp_spanner_database_name;
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_JOB_CLIENT_TYPE_DEF_H_
