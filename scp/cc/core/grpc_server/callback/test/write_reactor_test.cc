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

#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include "core/grpc_server/callback/test/callback_server.pb.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::ConcurrentQueue;
using grpc::ServerCallbackWriter;
using grpc::ServerWriteReactor;
using grpc::internal::ServerReactor;
using std::atomic_bool;
using std::atomic_int;
using std::function;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::thread;
using testing::_;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::IsTrue;
using testing::NiceMock;
using testing::Not;
using testing::Return;

namespace {
atomic_bool finished = false;
}

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
  function<ExecutionResult(ConsumerStreamingContext<SomeRequest, SomeResponse>)>
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
  thread async_thread;
  req_.set_field(5);
  reactor_->initiate_call_function = [this, &async_thread](auto context) {
    EXPECT_EQ(context.request->field(), req_.field());
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = thread([context]() mutable {
      for (int i = 0; i < 3; i++) {
        SomeResponse resp;
        resp.set_field(i);
        context.TryPushResponse(move(resp));
        context.ProcessNextMessage();
      }
      context.MarkDone();
      context.result = SuccessExecutionResult();
      context.Finish();
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

  WaitUntil([]() { return finished.load(); });
  writer_.CallOnDone();
  async_thread.join();
}

TEST_F(WriteReactorTest, FailureOnInitiationWorks) {
  reactor_->initiate_call_function = [](auto context) {
    context.MarkDone();
    context.result = FailureExecutionResult(SC_UNKNOWN);
    context.Finish();
    finished = true;
    return FailureExecutionResult(SC_UNKNOWN);
  };
  EXPECT_CALL(
      writer_,
      WriteAndFinish(SomeResponseHasResult(FailureExecutionResult(SC_UNKNOWN)),
                     _, StatusCodeIs(grpc::StatusCode::OK)));

  reactor_->Start(req_);

  WaitUntil([]() { return finished.load(); });
  writer_.CallOnDone();
}

TEST_F(WriteReactorTest, FailureOnAsyncOperationWorks) {
  thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = thread([context]() mutable {
      SomeResponse resp;
      resp.set_field(0);
      context.TryPushResponse(move(resp));
      context.ProcessNextMessage();
      context.MarkDone();
      context.result = FailureExecutionResult(SC_UNKNOWN);
      context.Finish();
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

  WaitUntil([]() { return finished.load(); });
  writer_.CallOnDone();
  async_thread.join();
}

// Generally we expect the operations from ProcessNextMessage to strictly come
// before Finish. When Finish is called, then context.result contains the true
// result.
TEST_F(WriteReactorTest, CapturesExecutionResultWhenCalledOutOfOrder) {
  thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = thread([context]() mutable {
      context.result = FailureExecutionResult(SC_UNKNOWN);
      SomeResponse resp;
      resp.set_field(0);
      context.TryPushResponse(move(resp));
      // We call finish before ProcessNextMessage to simulate the async
      // operations executing in an unexpected order.
      context.Finish();

      // Reset result to ensure the correct one is captured.
      context.result = ExecutionResult();
      // Enqueue more messages to ensure they aren't written.
      context.TryPushResponse(move(resp));
      context.TryPushResponse(move(resp));
      context.MarkDone();
      context.ProcessNextMessage();
      context.ProcessNextMessage();
      context.ProcessNextMessage();
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

  WaitUntil([]() { return finished.load(); });
  writer_.CallOnDone();
  async_thread.join();
}

TEST_F(WriteReactorTest, FailureOnWriteWorks) {
  thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = thread([context]() mutable {
      SomeResponse resp;
      resp.set_field(1);
      context.TryPushResponse(move(resp));
      context.ProcessNextMessage();
      context.MarkDone();
      context.result = SuccessExecutionResult();
      context.Finish();
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

  WaitUntil([]() { return finished.load(); });
  writer_.CallOnDone();
  async_thread.join();
}

TEST_F(WriteReactorTest, CancellationWorks) {
  thread async_thread;
  reactor_->initiate_call_function = [&async_thread](auto context) {
    // Spawn a thread to push elements onto the queue and then finish.
    async_thread = thread([context]() mutable {
      SomeResponse resp;
      resp.set_field(0);
      context.TryPushResponse(resp);
      context.ProcessNextMessage();
      context.TryPushResponse(move(resp));
      context.ProcessNextMessage();
      WaitUntil([&context]() { return context.IsCancelled(); });
      context.MarkDone();
      context.result = FailureExecutionResult(SC_UNKNOWN);
      context.Finish();
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

  WaitUntil([]() { return finished.load(); });
  writer_.CallOnDone();
  async_thread.join();
}

}  // namespace google::scp::core::test
