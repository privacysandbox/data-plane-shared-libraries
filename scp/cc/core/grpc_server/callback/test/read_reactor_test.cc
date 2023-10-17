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

#include "core/grpc_server/callback/src/read_reactor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>
#include <utility>

#include "core/grpc_server/callback/test/callback_server.pb.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"

using grpc::ServerCallbackReader;
using grpc::ServerReadReactor;
using grpc::internal::ServerReactor;
using std::thread;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::NiceMock;
using testing::Not;
using testing::Return;

namespace {
std::atomic_bool finished = false;
}

namespace google::scp::core::test {

// This class is used to mock how the ReadReactor would actually be called.
// Mostly, Read should be mocked to populate the given request pointer.
// MockReader is also a friend class of ReadReactor, so it can call OnDone and
// OnReadDone.
// Generally a ServerCallbackReader has the following process:
// 1. Reader calls reactor->OnReadDone(true) some # of times
// 2. Reader calls reactor->OnReadDone(false) to indicate no more messages are
//    coming.
// 3. Reader::Finish gets called
// 4. Reader will call reactor->OnDone()
class MockReader : public ServerCallbackReader<SomeRequest> {
 public:
  explicit MockReader(ServerReadReactor<SomeRequest>* reactor) {
    BindReactor(reactor);
    ON_CALL(*this, reactor).WillByDefault(Return(reactor));
    ON_CALL(*this, CallOnDone()).WillByDefault([reactor]() {
      reactor->OnDone();
    });
  }

  MOCK_METHOD(void, Finish, (grpc::Status), (override));
  MOCK_METHOD(void, SendInitialMetadata, (), (override));
  MOCK_METHOD(void, Read, (SomeRequest*), (override));
  MOCK_METHOD(ServerReactor*, reactor, (), (override));
  MOCK_METHOD(void, CallOnDone, (), (override));
};

class TestReadReactor : public ReadReactor<SomeRequest, SomeResponse> {
 public:
  using ReadReactor<SomeRequest, SomeResponse>::ReadReactor;

  // Call this after setting expectations on the reader.
  void Start() {
    // This usually goes in the constructor but we want to delay reading until
    // we can set expectations.
    this->StartRead(&req_);
  }

  // We use this function object as a mock to avoid using gMock on the class we
  // want to test.
  std::function<ExecutionResult(
      ProducerStreamingContext<SomeRequest, SomeResponse>)>
      initiate_call_function;

 private:
  ExecutionResult InitiateCall(
      ProducerStreamingContext<SomeRequest, SomeResponse> context) noexcept
      override {
    return initiate_call_function(context);
  }
};

class ReadReactorTest : public testing::Test {
 protected:
  ReadReactorTest()
      : reactor_(new TestReadReactor(&resp_)),
        reader_(static_cast<ServerReadReactor<SomeRequest>*>(reactor_)) {
    finished = false;
  }

  SomeResponse resp_;
  TestReadReactor* reactor_;
  NiceMock<MockReader> reader_;
};

MATCHER_P(StatusCodeIs, code, "") {
  return ExplainMatchResult(Eq(code), arg.error_code(), result_listener);
}

TEST_F(ReadReactorTest, BasicSequenceWorks) {
  thread finisher_thread;
  reactor_->initiate_call_function = [&finisher_thread](auto context) {
    // Spawn a thread to listen for when to finish context.
    finisher_thread = thread([context]() mutable {
      int val;
      std::unique_ptr<SomeRequest> req = context.TryGetNextRequest();
      for (val = 11; req != nullptr; val++) {
        EXPECT_EQ(req->field(), val);
        req = context.TryGetNextRequest();
      }
      EXPECT_EQ(val, 13);
      context.result = SuccessExecutionResult();
      context.response = std::make_shared<SomeResponse>();
      context.Finish();
      finished = true;
    });
    return SuccessExecutionResult();
  };
  InSequence seq;

  // Simulate 3 objects coming in with field (10, 11, 12). The first request
  // will not be on the queue - it is used to initiate the request.
  int val = 10;
  EXPECT_CALL(reader_, Read)
      .Times(3)
      .WillRepeatedly([this, &val](auto* request) {
        SomeRequest req;
        req.set_field(val++);
        *request = std::move(req);
        reactor_->OnReadDone(true /*read_performed*/);
      });
  // Tell the reactor this is the final call.
  EXPECT_CALL(reader_, Read).WillOnce([this](auto* request) {
    reactor_->OnReadDone(false /*read_performed*/);
  });
  EXPECT_CALL(reader_, Finish(StatusCodeIs(grpc::StatusCode::OK)));
  reactor_->Start();

  WaitUntil([]() { return finished.load(); });
  reader_.CallOnDone();
  finisher_thread.join();
  EXPECT_SUCCESS(ExecutionResult(resp_.result()));
}

TEST_F(ReadReactorTest, FailureOnInitiationWorks) {
  thread finisher_thread;
  reactor_->initiate_call_function = [&finisher_thread](auto context) {
    finisher_thread = thread([context]() mutable {
      context.result = FailureExecutionResult(SC_UNKNOWN);
      context.Finish();
      finished = true;
    });
    return FailureExecutionResult(SC_UNKNOWN);
  };
  InSequence seq;

  EXPECT_CALL(reader_, Read).WillOnce([this](auto* request) {
    SomeRequest req;
    req.set_field(10);
    *request = std::move(req);
    reactor_->OnReadDone(true /*read_performed*/);
  });
  EXPECT_CALL(reader_, Finish(StatusCodeIs(grpc::StatusCode::OK)));
  reactor_->Start();

  WaitUntil([]() { return finished.load(); });
  reader_.CallOnDone();
  finisher_thread.join();
  EXPECT_THAT(ExecutionResult(resp_.result()),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST_F(ReadReactorTest, FailureOnInitialReadWorks) {
  InSequence seq;

  // Tell the reactor the call failed.
  EXPECT_CALL(reader_, Read).WillOnce([this](auto* request) {
    reactor_->OnReadDone(false /*read_performed*/);
  });
  EXPECT_CALL(reader_, Finish(Not(StatusCodeIs(grpc::StatusCode::OK))));
  reactor_->Start();

  reader_.CallOnDone();
  EXPECT_EQ(finished, false);
}

TEST_F(ReadReactorTest, MakesNewResponseOnFailure) {
  std::atomic_int read_count = 0;
  thread finisher_thread;
  reactor_->initiate_call_function = [&finisher_thread,
                                      &read_count](auto context) {
    finisher_thread = thread([context, read_count = &read_count]() mutable {
      // Once the second read goes through, they will try to enqueue a
      // message but the queue is marked as done, so it will fail.
      while (*read_count < 2) {}
      // Spawn a thread to listen for when to finish context.
      // Return error and don't populate context.response.
      context.result = FailureExecutionResult(SC_UNKNOWN);
      context.MarkDone();
      context.Finish();
      finished = true;
    });
    return SuccessExecutionResult();
  };
  InSequence seq;

  auto normal_read = [this, &read_count](auto* request) {
    SomeRequest req;
    req.set_field(10);
    *request = std::move(req);
    read_count++;
    reactor_->OnReadDone(true /*read_performed*/);
  };
  EXPECT_CALL(reader_, Read)
      .WillOnce(normal_read)
      .WillOnce(normal_read)
      .WillRepeatedly(Return());
  EXPECT_CALL(reader_, Finish(StatusCodeIs(grpc::StatusCode::OK)));
  reactor_->Start();

  WaitUntil([]() { return finished.load(); });
  reader_.CallOnDone();
  finisher_thread.join();
  EXPECT_THAT(ExecutionResult(resp_.result()),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
}

TEST_F(ReadReactorTest, CancellationWorks) {
  thread finisher_thread;
  reactor_->initiate_call_function = [&finisher_thread](auto context) {
    // Spawn a thread to listen for when to finish context.
    finisher_thread = thread([context]() mutable {
      // Empty the queue and then move on.
      auto queue_is_done = [context = context]() mutable {
        if (context.TryGetNextRequest() == nullptr &&
            (context.IsMarkedDone() || context.IsCancelled())) {
          return context.TryGetNextRequest() == nullptr;
        }
        return false;
      };
      while (!queue_is_done()) {}
      EXPECT_TRUE(context.IsCancelled());
      context.result = SuccessExecutionResult();
      context.response = std::make_shared<SomeResponse>();
      context.Finish();
      finished = true;
    });
    return SuccessExecutionResult();
  };

  InSequence seq;

  EXPECT_CALL(reader_, Read)
      .WillOnce([this](auto* request) {
        request->set_field(1);
        reactor_->OnReadDone(true /*read_performed*/);
      })
      .WillOnce([this](auto* request) { reactor_->OnCancel(); });
  EXPECT_CALL(reader_, Finish(StatusCodeIs(grpc::StatusCode::OK)));
  reactor_->Start();

  WaitUntil([]() { return finished.load(); });
  reader_.CallOnDone();
  finisher_thread.join();
  EXPECT_SUCCESS(ExecutionResult(resp_.result()));
}

}  // namespace google::scp::core::test
