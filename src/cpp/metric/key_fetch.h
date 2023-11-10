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

#ifndef SRC_CPP_METRIC_KEY_REFRESH_H_
#define SRC_CPP_METRIC_KEY_REFRESH_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "src/cpp/encryption/key_fetcher/interface/public_key_fetcher_interface.h"

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
        {"public key dispatch", public_key_dispatch_failure_count_},
        {"public key async", public_key_async_failure_count_},
        {"private key dispatch", private_key_dispatch_failure_count_},
        {"private key async", private_key_async_failure_count_},
    };
  }
  static absl::flat_hash_map<std::string, double>
  GetNumKeysParsedOnRecentFetch() ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return absl::flat_hash_map<std::string, double>{
        {"public key GCP",
         num_public_keys_parsed_recent_fetch_[CloudPlatform::GCP]},
        {"public key AWS",
         num_public_keys_parsed_recent_fetch_[CloudPlatform::AWS]},
        {"private key", num_private_keys_parsed_recent_fetch_},
    };
  }
  static absl::flat_hash_map<std::string, double>
  GetNumKeysCachedAfterRecentFetch() ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return absl::flat_hash_map<std::string, double>{
        {"public key GCP",
         num_public_keys_cached_recent_fetch_[CloudPlatform::GCP]},
        {"public key AWS",
         num_public_keys_cached_recent_fetch_[CloudPlatform::AWS]},
        {"private key", num_private_keys_cached_recent_fetch_},
    };
  }

  static void IncrementPublicKeyFetchDispatchFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++public_key_dispatch_failure_count_;
  }
  static void IncrementPublicKeyFetchAsyncFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++public_key_async_failure_count_;
  }
  static void IncrementPrivateKeyFetchDispatchFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++private_key_dispatch_failure_count_;
  }
  static void IncrementPrivateKeyFetchAsyncFailureCount()
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    ++private_key_async_failure_count_;
  }

  static void SetNumPublicKeysParsedOnRecentFetch(CloudPlatform platform,
                                                  int num)
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_public_keys_parsed_recent_fetch_[platform] = num;
  }
  static void SetNumPrivateKeysParsedOnRecentFetch(int num)
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_private_keys_parsed_recent_fetch_ = num;
  }

  static void SetNumPublicKeysCachedAfterRecentFetch(CloudPlatform platform,
                                                     int num)
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_public_keys_cached_recent_fetch_[platform] = num;
  }
  static void SetNumPrivateKeysCachedAfterRecentFetch(int num)
      ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    num_private_keys_cached_recent_fetch_ = num;
  }

 private:
  ABSL_CONST_INIT static inline absl::Mutex mu_{absl::kConstInit};

  static inline int public_key_dispatch_failure_count_ ABSL_GUARDED_BY(mu_){0};
  static inline int public_key_async_failure_count_ ABSL_GUARDED_BY(mu_){0};
  static inline int private_key_dispatch_failure_count_ ABSL_GUARDED_BY(mu_){0};
  static inline int private_key_async_failure_count_ ABSL_GUARDED_BY(mu_){0};

  static inline absl::flat_hash_map<CloudPlatform, int>
      num_public_keys_parsed_recent_fetch_ ABSL_GUARDED_BY(mu_){
          {CloudPlatform::GCP, 0}, {CloudPlatform::AWS, 0}};
  static inline int num_private_keys_parsed_recent_fetch_ ABSL_GUARDED_BY(mu_){
      0};

  static inline absl::flat_hash_map<CloudPlatform, int>
      num_public_keys_cached_recent_fetch_ ABSL_GUARDED_BY(mu_){
          {CloudPlatform::GCP, 0}, {CloudPlatform::AWS, 0}};
  static inline int num_private_keys_cached_recent_fetch_ ABSL_GUARDED_BY(mu_){
      0};
};

}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_METRIC_KEY_REFRESH_H_
