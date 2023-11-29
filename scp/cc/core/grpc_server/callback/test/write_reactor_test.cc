// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/grpc_server/callback/src/write_reactor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "core/grpc_server/callback/test/callback_server.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "src/cpp/util/duration.h"

using google::scp::core::common::ConcurrentQueue;
using grpc::ServerCallbackWriter;
using grpc::ServerWriteReactor;
using grpc::internal::ServerReactor;
using privacy_sandbox::server_common::ExpiringFlag;
using testing::_;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::IsTrue;
using testing::NiceMock;
using testing::Not;
using testing::Return;

namespace {
ABSL_CONST_INIT absl::Mutex finished_mu(absl::kConstInit);
bool finished ABSL_GUARDED_BY(finished_mu) = false;
}  // namespace

namespace google::scp::core::test {

// This class is used to mock how the WriteReactor would actually be called.
// Mostly, Write should be mocked to call OnWriteDone.
// MockWriter is also a friend class of WriteReactor, so it can call OnDone and
// OnWriteDone.
// Generally a ServerCallbackWriter has the following process:
// 1. Writer calls reactor->OnWriteDone(true) for each Write call that comes in.
// 2. Writer::Finish/WriteAndFinish gets called
// 3. Writer will call reactor->OnDone()
class MockWriter : public ServerCallbackWriter<SomeResponse> {
 public:
  explicit MockWriter(ServerWriteReactor<SomeResponse>* reactor) {
    BindReactor(reactor);
    ON_CALL(*this, reactor).WillByDefault(Return(reactor));
    ON_CALL(*this, CallOnDone()).WillByDefault([reactor]() {
      reactor->OnDone();
    });
  }

  MOCK_METHOD(void, Finish, (grpc::Status), (override));
  MOCK_METHOD(void, SendInitialMetadata, (), (override));
  MOCK_METHOD(void, Write, (const SomeResponse*, grpc::WriteOptions),
              (override));
  MOCK_METHOD(void, WriteAndFinish,
              (const SomeResponse*, grpc::WriteOptions, grpc::Status),
              (override));
  MOCK_METHOD(ServerReactor*, reactor, (), (override));
  MOCK_METHOD(void, CallOnDone, (), (override));
};

class TestWriteReactor : public WriteReactor<SomeRequest, SomeResponse> {
 public:
  using WriteReactor<SomeRequest, SomeResponse>::WriteReactor;

  // We use this function object as a mock to avoid using gMock on the class we
  // want to test.
  std::function<ExecutionResult(
      ConsumerStreamingContext<SomeRequest, SomeResponse>)>
      initiate_call_function;

 private:
  ExecutionResult InitiateCall(
      ConsumerStreamingContext<SomeRequest, SomeResponse> context) noexcept
      override {
    return initiate_call_function(context);
  }
};

class WriteReactorTest : public testing::Test {
 protected:
  WriteReactorTest()
      : reactor_(new TestWriteReactor()),
        writer_(static_cast<ServerWriteReactor<SomeResponse>*>(reactor_)) {
    absl::MutexLock l(&finished_mu);
    finished = false;
  }

  SomeRequest req_;
  TestWriteReactor* reactor_;
  NiceMock<MockWriter> writer_;
};

MATCHER_P(StatusCodeIs, code, "") {
  return ExplainMatchResult(Eq(code), arg.error_code(), result_listener);
}

MATCHER_P(SomeResponseHasField, expected, "") {
  return ExplainMatchResult(IsSuccessful(), ExecutionResult(arg->result()),
                            result_listener) &&
         ExplainMatchResult(Eq(expected), arg->field(), result_listener);
}

MATCHER_P(SomeResponseHasResult, result, "") {
  return ExplainMatchResult(ResultIs(result), ExecutionResult(arg->result()),
                            result_listener);
}

TEST_F(WriteReactorTest, BasicSequenceWorks) {
  std::thread async_thread;
  req_.set_field(5);
  reactor_->initiate_call_function = [this, &async_thread](auto context) {
    EXPECT_EQ(context.request->field(), req_.field());
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = std::thread([context]() mutable {
      for (int i = 0; i < 3; i++) {
        SomeResponse resp;
        resp.set_field(i);
        context.TryPushResponse(std::move(resp));
        context.ProcessNextMessage();
      }
      context.MarkDone();
      context.result = SuccessExecutionResult();
      context.Finish();
      absl::MutexLock l(&finished_mu);
      finished = true;
    });
    return SuccessExecutionResult();
  };
  InSequence seq;

  auto on_write = [this](auto, auto) {
    reactor_->OnWriteDone(true /*write_performed*/);
  };
  EXPECT_CALL(writer_, Write(SomeResponseHasField(0), _)).WillOnce(on_write);
  EXPECT_CALL(writer_, Write(SomeResponseHasField(1), _)).WillOnce(on_write);
  EXPECT_CALL(writer_, Write(SomeResponseHasField(2), _)).WillOnce(on_write);
  EXPECT_CALL(writer_,
              WriteAndFinish(SomeResponseHasResult(SuccessExecutionResult()), _,
                             StatusCodeIs(grpc::StatusCode::OK)));

  reactor_->Start(req_);

  {
    absl::MutexLock l(&finished_mu);
    finished_mu.Await(absl::Condition(&finished));
  }
  writer_.CallOnDone();
  async_thread.join();
}

TEST_F(WriteReactorTest, FailureOnInitiationWorks) {
  reactor_->initiate_call_function = [](auto context) {
    context.MarkDone();
    context.result = FailureExecutionResult(SC_UNKNOWN);
    context.Finish();
    {
      absl::MutexLock l(&finished_mu);
      finished = true;
    }
    return FailureExecutionResult(SC_UNKNOWN);
  };
  EXPECT_CALL(
      writer_,
      WriteAndFinish(SomeResponseHasResult(FailureExecutionResult(SC_UNKNOWN)),
                     _, StatusCodeIs(grpc::StatusCode::OK)));

  reactor_->Start(req_);

  {
    absl::MutexLock l(&finished_mu);
    finished_mu.Await(absl::Condition(&finished));
  }
  writer_.CallOnDone();
}

TEST_F(WriteReactorTest, FailureOnAsyncOperationWorks) {
  std::thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = std::thread([context]() mutable {
      SomeResponse resp;
      resp.set_field(0);
      context.TryPushResponse(std::move(resp));
      context.ProcessNextMessage();
      context.MarkDone();
      context.result = FailureExecutionResult(SC_UNKNOWN);
      context.Finish();
      absl::MutexLock l(&finished_mu);
      finished = true;
    });
    return SuccessExecutionResult();
  };
  InSequence seq;

  auto on_write = [this](auto, auto) {
    reactor_->OnWriteDone(true /*write_performed*/);
  };
  EXPECT_CALL(writer_, Write(SomeResponseHasField(0), _)).WillOnce(on_write);
  EXPECT_CALL(
      writer_,
      WriteAndFinish(SomeResponseHasResult(FailureExecutionResult(SC_UNKNOWN)),
                     _, StatusCodeIs(grpc::StatusCode::OK)));

  reactor_->Start(req_);

  {
    absl::MutexLock l(&finished_mu);
    finished_mu.Await(absl::Condition(&finished));
  }
  writer_.CallOnDone();
  async_thread.join();
}

// Generally we expect the operations from ProcessNextMessage to strictly come
// before Finish. When Finish is called, then context.result contains the true
// result.
TEST_F(WriteReactorTest, CapturesExecutionResultWhenCalledOutOfOrder) {
  std::thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = std::thread([context]() mutable {
      context.result = FailureExecutionResult(SC_UNKNOWN);
      SomeResponse resp;
      resp.set_field(0);
      context.TryPushResponse(std::move(resp));
      // We call finish before ProcessNextMessage to simulate the async
      // operations executing in an unexpected order.
      context.Finish();

      // Reset result to ensure the correct one is captured.
      context.result = ExecutionResult();
      // Enqueue more messages to ensure they aren't written.
      context.TryPushResponse(std::move(resp));
      context.TryPushResponse(std::move(resp));
      context.MarkDone();
      context.ProcessNextMessage();
      context.ProcessNextMessage();
      context.ProcessNextMessage();
      absl::MutexLock l(&finished_mu);
      finished = true;
    });
    return SuccessExecutionResult();
  };

  EXPECT_CALL(writer_, Write).Times(0);
  EXPECT_CALL(
      writer_,
      WriteAndFinish(SomeResponseHasResult(FailureExecutionResult(SC_UNKNOWN)),
                     _, StatusCodeIs(grpc::StatusCode::OK)));

  reactor_->Start(req_);

  {
    absl::MutexLock l(&finished_mu);
    finished_mu.Await(absl::Condition(&finished));
  }
  writer_.CallOnDone();
  async_thread.join();
}

TEST_F(WriteReactorTest, FailureOnWriteWorks) {
  std::thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = std::thread([context]() mutable {
      SomeResponse resp;
      resp.set_field(1);
      context.TryPushResponse(std::move(resp));
      context.ProcessNextMessage();
      context.MarkDone();
      context.result = SuccessExecutionResult();
      context.Finish();
      absl::MutexLock l(&finished_mu);
      finished = true;
    });
    return SuccessExecutionResult();
  };
  InSequence seq;

  auto on_write = [this](auto, auto) {
    reactor_->OnWriteDone(false /*write_performed*/);
  };
  EXPECT_CALL(writer_, Write(SomeResponseHasField(1), _)).WillOnce(on_write);
  EXPECT_CALL(writer_, Finish(StatusCodeIs(grpc::StatusCode::INTERNAL)));

  reactor_->Start(req_);

  {
    absl::MutexLock l(&finished_mu);
    finished_mu.Await(absl::Condition(&finished));
  }
  writer_.CallOnDone();
  async_thread.join();
}

TEST_F(WriteReactorTest, CancellationWorks) {
  std::thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = std::thread([context]() mutable {
      SomeResponse resp;
      resp.set_field(0);
      context.TryPushResponse(resp);
      context.ProcessNextMessage();
      context.TryPushResponse(std::move(resp));
      context.ProcessNextMessage();
      {
        ExpiringFlag expiring_flag;
        expiring_flag.Set(absl::Seconds(5));
        while (!context.IsCancelled() && !expiring_flag.Get()) {}
      }
      ASSERT_TRUE(context.IsCancelled())
          << "Context was cancelled, or exceeded timeout.";
      context.MarkDone();
      context.result = FailureExecutionResult(SC_UNKNOWN);
      context.Finish();
      absl::MutexLock l(&finished_mu);
      finished = true;
    });
    return SuccessExecutionResult();
  };
  InSequence seq;

  EXPECT_CALL(writer_, Write).WillOnce([this](auto, auto) {
    reactor_->OnWriteDone(true /*write_performed*/);
  });
  EXPECT_CALL(writer_, Write).WillOnce([this](auto, auto) {
    reactor_->OnCancel();
  });
  EXPECT_CALL(
      writer_,
      WriteAndFinish(SomeResponseHasResult(FailureExecutionResult(SC_UNKNOWN)),
                     _, StatusCodeIs(grpc::StatusCode::OK)));

  reactor_->Start(req_);

  {
    absl::MutexLock l(&finished_mu);
    finished_mu.Await(absl::Condition(&finished));
  }
  writer_.CallOnDone();
  async_thread.join();
}

}  // namespace google::scp::core::test
