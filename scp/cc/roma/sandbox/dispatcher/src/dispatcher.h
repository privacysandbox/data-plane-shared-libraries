/*
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

#ifndef ROMA_SANDBOX_DISPATCHER_SRC_DISPATCHER_H_
#define ROMA_SANDBOX_DISPATCHER_SRC_DISPATCHER_H_

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "core/async_executor/src/async_executor.h"
#include "core/common/lru_cache/src/lru_cache.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/interface/roma.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/worker_api/src/worker_api.h"
#include "roma/sandbox/worker_pool/src/worker_pool.h"

#include "request_converter.h"
#include "request_validator.h"

namespace google::scp::roma::sandbox::dispatcher {
class Dispatcher {
 public:
  Dispatcher(core::AsyncExecutor* async_executor,
             worker_pool::WorkerPool* worker_pool, size_t max_pending_requests,
             size_t code_version_cache_size)
      : async_executor_(async_executor),
        worker_pool_(worker_pool),
        worker_index_(0),
        pending_requests_(0),
        max_pending_requests_(max_pending_requests),
        code_object_cache_(code_version_cache_size) {
    CHECK(max_pending_requests > 0) << "max_pending_requests cannot be zero";
    CHECK(code_version_cache_size > 0)
        << "code_version_cache_size cannot be zero";
  }

  // Block until scheduled requests are complete.
  ~Dispatcher() {
    auto fn = [this] {
      pending_requests_mu_.AssertReaderHeld();
      return pending_requests_ == 0;
    };
    absl::MutexLock l(&pending_requests_mu_);
    pending_requests_mu_.Await(absl::Condition(&fn));
  }

  /**
   * @brief Dispatch a set of requests. This function will block until all the
   * requests have been dispatched. This uses Dispatch.
   *
   * @tparam RequestT The type of the request.
   * @param batch The input batch of request to enqueue.
   * @param batch_callback The callback to invoke once the batch is done.
   * @return absl::Status Whether the dispatch batch operation
   * succeeded or failed.
   */
  template <typename RequestT>
  absl::Status DispatchBatch(std::vector<RequestT>& batch,
                             BatchCallback batch_callback) noexcept {
    auto batch_size = batch.size();
    auto batch_response =
        std::make_shared<std::vector<absl::StatusOr<ResponseObject>>>(
            batch_size, absl::StatusOr<ResponseObject>());
    auto finished_counter = std::make_shared<std::atomic<size_t>>(0);

    auto batch_callback_ptr =
        std::make_shared<BatchCallback>(std::move(batch_callback));
    for (size_t index = 0; index < batch_size; ++index) {
      auto callback =
          [batch_response, finished_counter, batch_callback_ptr, index](
              std::unique_ptr<absl::StatusOr<ResponseObject>> obj_response) {
            batch_response->at(index) = *std::move(obj_response);
            auto finished_value = finished_counter->fetch_add(1);
            if (finished_value + 1 == batch_response->size()) {
              (*batch_callback_ptr)(*batch_response);
            }
          };

      auto request = std::make_unique<RequestT>(batch[index]);
      absl::Status result;
      while ((result = Dispatcher::Dispatch(std::move(request), callback)) !=
             absl::OkStatus()) {
        // If the first request from the batch got a failure, return failure
        // without waiting.
        if (index == 0) {
          return result;
        }
        request = std::make_unique<RequestT>(batch[index]);
      }
    }

    return absl::OkStatus();
  }

  /**
   * @brief Execute a "load" request against all worker in the pool.
   *
   * @param code_object The code object to load.
   * @param broadcast_callback The callback to invoke once the operation has
   * completed.
   * @return absl::Status Whether the broadcast succeeded or failed.
   */
  absl::Status Broadcast(std::unique_ptr<CodeObject> code_object,
                         Callback broadcast_callback) noexcept;

  /**
   * @brief Enqueues a request to be handled by the workers. Can return failure
   * before scheduling the callback function.
   *
   * @tparam RequestT The type of the request.
   * @param request The request.
   * @param callback The function to call once the request completes.
   * @param worker_index Specific worker to allocate request to.
   * @return absl::Status Whether the enqueue operation succeeded or
   * not.
   */
  template <typename RequestT>
  absl::Status Dispatch(std::unique_ptr<RequestT> request, Callback callback,
                        int32_t worker_index = -1) noexcept
      ABSL_LOCKS_EXCLUDED(pending_requests_mu_, worker_index_mu_, cache_mu_) {
    if (absl::MutexLock l(&pending_requests_mu_);
        pending_requests_ >= max_pending_requests_) {
      return absl::ResourceExhaustedError(
          "Dispatch is disallowed since the number of unfinished requests is "
          "at capacity.");
    }

    // We accept empty request IDs, but we will replace them with a placeholder.
    if (request->id.empty()) {
      request->id = constants::kDefaultRomaRequestId;
    }

    auto validation_status =
        request_validator::RequestValidator<RequestT>::Validate(*request);
    if (!validation_status.ok()) {
      return validation_status;
    }

    size_t index = 0;

    if (worker_index != -1) {
      index = worker_index;
    } else {
      absl::MutexLock l(&worker_index_mu_);
      index = worker_index_;
      worker_index_ = (worker_index_ + 1) % worker_pool_->GetPoolSize();
    }

    if constexpr (std::is_same<RequestT, CodeObject>::value) {
      absl::MutexLock l(&cache_mu_);
      code_object_cache_.Set(request->version_string, *request);
    }

    {
      absl::MutexLock l(&pending_requests_mu_);
      pending_requests_++;
    }
    const auto request_id = request->id;
    auto schedule_result = async_executor_->Schedule(
        [this, index, request = std::move(request),
         callback = std::move(callback)]() mutable {
          std::unique_ptr<absl::StatusOr<ResponseObject>> response_or;

          auto worker_or = worker_pool_->GetWorker(index);
          if (!worker_or.status().ok()) {
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                worker_or.status());
            callback(std::move(response_or));
            absl::MutexLock l(&pending_requests_mu_);
            pending_requests_--;
            return;
          }

          std::string request_type;
          cache_mu_.Lock();
          if (!code_object_cache_.Contains(request->version_string)) {
            cache_mu_.Unlock();
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                absl::InternalError("Could not find code version in cache."));
            callback(std::move(response_or));
            absl::MutexLock l(&pending_requests_mu_);
            pending_requests_--;
            return;
          } else {
            // Double lookup necessary to support above not found error.
            const CodeObject& code_object =
                code_object_cache_.Get(request->version_string);

            // TODO(b/317791484): Verify this is WAI.
            if (!code_object.wasm_bin.empty()) {
              request_type = constants::kRequestTypeJavascriptWithWasm;
            } else if (code_object.js.empty()) {
              request_type = constants::kRequestTypeWasm;
            } else {
              request_type = constants::kRequestTypeJavascript;
            }
          }
          cache_mu_.Unlock();

          auto run_code_request =
              RequestConverter::FromUserProvided(*request, request_type);
          auto run_code_response_or = (*worker_or)->RunCode(run_code_request);
          if (!run_code_response_or.result().Successful()) {
            auto err_msg = core::errors::GetErrorMessage(
                run_code_response_or.result().status_code);
            response_or = std::make_unique<absl::StatusOr<ResponseObject>>(
                absl::InternalError(err_msg));
            LOG(ERROR) << "The worker " << index
                       << " execute the request failed due to " << err_msg;

            if (run_code_response_or.result().Retryable()) {
              // This means that the worker crashed and the request could be
              // retried, however, we need to reload the worker with the
              // cached code.
              if (auto reload_result = ReloadCachedCodeObjects(**worker_or);
                  !reload_result.ok()) {
                LOG(ERROR) << "Reloading the worker cache failed with "
                           << reload_result;
              }
              ROMA_VLOG(1)
                  << "Successfully reload all cached code objects to the worker"
                  << index;
            }

            callback(std::move(response_or));
            absl::MutexLock l(&pending_requests_mu_);
            pending_requests_--;
            return;
          }

          ResponseObject response_object;
          response_or =
              std::make_unique<absl::StatusOr<ResponseObject>>(response_object);
          response_or->value().id = request->id;
          response_or->value().resp =
              std::move(*run_code_response_or->response);
          for (auto& kv : run_code_response_or->metrics) {
            response_or->value().metrics[kv.first] = kv.second;
          }
          callback(std::move(response_or));
          absl::MutexLock l(&pending_requests_mu_);
          pending_requests_--;
        },
        core::AsyncPriority::Normal);

    if (schedule_result.Successful()) {
      ROMA_VLOG(1) << "Successfully schedule the execution for request "
                   << request_id << " in worker " << index;
      return absl::OkStatus();
    } else {
      absl::MutexLock l(&pending_requests_mu_);
      pending_requests_--;
      return absl::InternalError(
          absl::StrCat("Dispatch failed due to: ",
                       google::scp::core::errors::GetErrorMessage(
                           schedule_result.status_code)));
    }
  }

 private:
  absl::Status ReloadCachedCodeObjects(worker_api::WorkerApi& worker);

  core::AsyncExecutor* async_executor_;
  worker_pool::WorkerPool* worker_pool_;
  absl::Mutex worker_index_mu_;
  // The next worker index for Dispatch calls with `worker_index == -1`
  int worker_index_ ABSL_GUARDED_BY(worker_index_mu_);
  absl::Mutex pending_requests_mu_;
  int pending_requests_ ABSL_GUARDED_BY(pending_requests_mu_);
  const size_t max_pending_requests_;
  absl::Mutex cache_mu_;
  core::common::LruCache<std::string, CodeObject> code_object_cache_
      ABSL_GUARDED_BY(cache_mu_);
};
}  // namespace google::scp::roma::sandbox::dispatcher

#endif  // ROMA_SANDBOX_DISPATCHER_SRC_DISPATCHER_H_
