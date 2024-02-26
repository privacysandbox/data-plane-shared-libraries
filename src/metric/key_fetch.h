//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#ifndef METRIC_KEY_REFRESH_H_
#define METRIC_KEY_REFRESH_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "src/public/core/interface/cloud_platform.h"

// Defines instruments for key fetching from the coordinator.
namespace privacy_sandbox::server_common {

// Records the result of fetching keys from the coordinator.
// Uses the Increment methods to counts and use the Get methods to retrieve
// states. Note that this implementation assumes that the frequency of key
// refresh is low enough that every update on the result counter is exported and
// that type int doesn't overflow in a reasonable amount of time.
class KeyFetchResultCounter {
 public:
  static absl::flat_hash_map<std::string, double> GetKeyFetchFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return absl::flat_hash_map<std::string, double>{
        {"public key sync", public_key_fetch_sync_failure_count_},
        {"public key async", public_key_fetch_async_failure_count_},
        {"private key sync", private_key_fetch_async_failure_count_},
        {"private key async", private_key_async_failure_count_},
    };
  }

  static absl::flat_hash_map<std::string, double> GetNumPrivateKeysFetched()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return absl::flat_hash_map<std::string, double>{
        {"private key", num_private_keys_fetched_},
    };
  }

  static absl::flat_hash_map<std::string, double> GetNumKeysParsed()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return absl::flat_hash_map<std::string, double>{
        {"GCP public key", num_public_keys_parsed_[CloudPlatform::kGcp]},
        {"AWS public key", num_public_keys_parsed_[CloudPlatform::kAws]},
        {"private key", num_private_keys_parsed_},
    };
  }

  static absl::flat_hash_map<std::string, double> GetNumKeysCached()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return absl::flat_hash_map<std::string, double>{
        {"GCP public key", num_public_keys_cached_[CloudPlatform::kGcp]},
        {"AWS public key", num_public_keys_cached_[CloudPlatform::kAws]},
        {"private key", num_private_keys_cached_},
    };
  }

  static void IncrementPublicKeyFetchSyncFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++public_key_fetch_sync_failure_count_;
  }

  static void IncrementPublicKeyFetchAsyncFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++public_key_fetch_async_failure_count_;
  }

  static void IncrementPrivateKeyFetchSyncFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++private_key_fetch_async_failure_count_;
  }

  static void IncrementPrivateKeyFetchAsyncFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++private_key_async_failure_count_;
  }

  static void SetNumPrivateKeysFetched(int num) ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_private_keys_fetched_ = num;
  }

  static void SetNumPublicKeysParsed(CloudPlatform platform, int num)
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_public_keys_parsed_[platform] = num;
  }

  static void SetNumPrivateKeysParsed(int num) ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_private_keys_parsed_ = num;
  }

  static void SetNumPublicKeysCached(CloudPlatform platform, int num)
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_public_keys_cached_[platform] = num;
  }

  static void SetNumPrivateKeysCached(int num) ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_private_keys_cached_ = num;
  }

 private:
  ABSL_CONST_INIT static inline absl::Mutex mu_{absl::kConstInit};

  static inline int public_key_fetch_sync_failure_count_ ABSL_GUARDED_BY(mu_){
      0};
  static inline int public_key_fetch_async_failure_count_ ABSL_GUARDED_BY(mu_){
      0};
  static inline int private_key_fetch_async_failure_count_ ABSL_GUARDED_BY(mu_){
      0};
  static inline int private_key_async_failure_count_ ABSL_GUARDED_BY(mu_){0};
  static inline int num_private_keys_fetched_ ABSL_GUARDED_BY(mu_){0};

  static inline absl::flat_hash_map<CloudPlatform, int> num_public_keys_parsed_
      ABSL_GUARDED_BY(mu_){{CloudPlatform::kGcp, 0}, {CloudPlatform::kAws, 0}};
  static inline int num_private_keys_parsed_ ABSL_GUARDED_BY(mu_){0};

  static inline absl::flat_hash_map<CloudPlatform, int> num_public_keys_cached_
      ABSL_GUARDED_BY(mu_){{CloudPlatform::kGcp, 0}, {CloudPlatform::kAws, 0}};
  static inline int num_private_keys_cached_ ABSL_GUARDED_BY(mu_){0};
};

}  // namespace privacy_sandbox::server_common

#endif  // METRIC_KEY_REFRESH_H_
