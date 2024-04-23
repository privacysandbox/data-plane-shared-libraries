// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/util/protoutil.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "src/core/test/utils/proto_test_utils.h"

using google::scp::core::test::EqualsProto;

namespace privacy_sandbox::server_common {
namespace {

google::protobuf::Duration MakeGoogleApiDuration(int64_t s, int32_t ns) {
  google::protobuf::Duration proto;
  proto.set_seconds(s);
  proto.set_nanos(ns);
  return proto;
}

google::protobuf::Timestamp MakeGoogleApiTimestamp(int64_t s, int32_t ns) {
  google::protobuf::Timestamp proto;
  proto.set_seconds(s);
  proto.set_nanos(ns);
  return proto;
}

// Helper function that tests the EncodeGoogleApiProto() and
// DecodeGoogleApiProto() functions. Both variants of the EncodeGoogleApiProto
// are tested to ensure they return the proto result. The templated first
// argument may be either a absl::Duration or a absl::Time. The template P
// represents a google::protobuf::Duration or google::protobuf::Timestamp.
template <typename T, typename P>
void RoundTripGoogleApi(T v, int64_t expected_sec, int32_t expected_nsec) {
  const auto sor_proto = EncodeGoogleApiProto(v);
  ASSERT_TRUE(sor_proto.ok());
  const auto& proto = *sor_proto;
  EXPECT_EQ(proto.seconds(), expected_sec);
  EXPECT_EQ(proto.nanos(), expected_nsec);

  P out_proto;
  const auto status = EncodeGoogleApiProto(v, &out_proto);
  ASSERT_TRUE(status.ok());
  EXPECT_EQ(out_proto.seconds(), expected_sec);
  EXPECT_EQ(out_proto.nanos(), expected_nsec);
  EXPECT_THAT(proto, EqualsProto(out_proto));

  // Complete the round-trip by decoding the proto back to a absl::Duration.
  const auto sor_duration = DecodeGoogleApiProto(proto);
  ASSERT_TRUE(sor_duration.ok());
  const auto& duration = *sor_duration;
  EXPECT_EQ(duration, v);
}

TEST(ProtoUtilGoogleApi, RoundTripDuration) {
  // Shorthand to make the test cases readable.
  const auto& s = [](int64_t n) { return absl::Seconds(n); };
  const auto& ns = [](int64_t n) { return absl::Nanoseconds(n); };

  const struct {
    absl::Duration d;

    struct {
      int64_t sec;
      int32_t nsec;
    } expected;
  } kTestCases[] = {
      {s(0), {0, 0}},
      {s(123) + ns(456), {123, 456}},
      {ns(-5), {0, -5}},
      {s(-10) - ns(5), {-10, -5}},
      {s(-315576000000), {-315576000000, 0}},
      {s(315576000000), {315576000000, 0}},
      {MakeGoogleApiDurationMin(), {-315576000000, -999999999}},
      {MakeGoogleApiDurationMax(), {315576000000, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    RoundTripGoogleApi<absl::Duration, google::protobuf::Duration>(
        tc.d, tc.expected.sec, tc.expected.nsec);
  }
}

TEST(ProtoUtilGoogleApi, DurationTruncTowardZero) {
  // Shorthand to make the test cases readable.
  const absl::Duration tick = absl::Nanoseconds(1) / 4;
  const auto& s = [](int64_t n) { return absl::Seconds(n); };

  const struct {
    absl::Duration d;

    struct {
      int64_t sec;
      int32_t nsec;
    } expected;
  } kTestCases[] = {
      {tick, {0, 0}},
      {-tick, {0, 0}},
      {s(2) + tick, {2, 0}},
      {s(2) - tick, {1, 999999999}},
      {s(1) + tick, {1, 0}},
      {s(1) - tick, {0, 999999999}},
      {s(-1) + tick, {0, -999999999}},
      {s(-1) - tick, {-1, 0}},
      {s(-2) + tick, {-1, -999999999}},
      {s(-2) - tick, {-2, 0}},
      {MakeGoogleApiDurationMin() - tick, {-315576000000, -999999999}},
      {MakeGoogleApiDurationMin() + tick, {-315576000000, -999999998}},
      {MakeGoogleApiDurationMax() - tick, {315576000000, 999999998}},
      {MakeGoogleApiDurationMax() + tick, {315576000000, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    const auto sor = EncodeGoogleApiProto(tc.d);
    ASSERT_TRUE(sor.ok());
    const auto& proto = *sor;
    EXPECT_EQ(proto.seconds(), tc.expected.sec) << "d=" << tc.d;
    EXPECT_EQ(proto.nanos(), tc.expected.nsec) << "d=" << tc.d;
  }
}

TEST(ProtoUtilGoogleApi, EncodeDurationError) {
  const absl::Duration kTestCases[] = {
      MakeGoogleApiDurationMin() - absl::Nanoseconds(1),  //
      MakeGoogleApiDurationMax() + absl::Nanoseconds(1),  //
      -absl::InfiniteDuration(),                          //
      absl::InfiniteDuration()};                          //
  for (const auto& d : kTestCases) {
    const auto sor = EncodeGoogleApiProto(d);
    EXPECT_FALSE(sor.ok()) << "d=" << d;

    google::protobuf::Duration proto;
    const auto status = EncodeGoogleApiProto(d, &proto);
    EXPECT_FALSE(status.ok()) << "d=" << d;
  }
}

TEST(ProtoUtilGoogleApi, DecodeDurationError) {
  const google::protobuf::Duration kTestCases[] = {
      MakeGoogleApiDuration(1, -1),                                   //
      MakeGoogleApiDuration(-1, 1),                                   //
      MakeGoogleApiDuration(0, 999999999 + 1),                        //
      MakeGoogleApiDuration(0, -999999999 - 1),                       //
      MakeGoogleApiDuration(-315576000000 - 1, 0),                    //
      MakeGoogleApiDuration(315576000000 + 1, 0),                     //
      MakeGoogleApiDuration(std::numeric_limits<int64_t>::min(), 0),  //
      MakeGoogleApiDuration(std::numeric_limits<int64_t>::max(), 0),  //
      MakeGoogleApiDuration(0, std::numeric_limits<int32_t>::min()),  //
      MakeGoogleApiDuration(0, std::numeric_limits<int32_t>::max()),  //
      MakeGoogleApiDuration(std::numeric_limits<int64_t>::min(),
                            std::numeric_limits<int32_t>::min()),  //
      MakeGoogleApiDuration(std::numeric_limits<int64_t>::max(),
                            std::numeric_limits<int32_t>::max()),  //
  };
  for (const auto& d : kTestCases) {
    const auto sor = DecodeGoogleApiProto(d);
    EXPECT_FALSE(sor.ok()) << "d=" << d.DebugString();
  }
}

TEST(ProtoUtilGoogleApi, DurationProtoMax) {
  const auto proto_max = MakeGoogleApiDurationProtoMax();
  const absl::Duration duration_max = MakeGoogleApiDurationMax();
  auto decoded = DecodeGoogleApiProto(proto_max);
  EXPECT_TRUE(decoded.ok());
  EXPECT_EQ(*decoded, duration_max);
  auto encoded = EncodeGoogleApiProto(duration_max);
  EXPECT_TRUE(encoded.ok());
  EXPECT_THAT(*encoded, EqualsProto(proto_max));
}

TEST(ProtoUtilGoogleApi, DurationMaxIsMax) {
  const absl::Duration duration_max = MakeGoogleApiDurationMax();
  EXPECT_EQ(
      EncodeGoogleApiProto(duration_max + absl::Nanoseconds(1)).status().code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(ProtoUtilGoogleApi, DurationProtoMin) {
  const auto proto_min = MakeGoogleApiDurationProtoMin();
  const absl::Duration duration_min = MakeGoogleApiDurationMin();
  auto decoded = DecodeGoogleApiProto(proto_min);
  EXPECT_TRUE(decoded.ok());
  EXPECT_EQ(*decoded, duration_min);
  auto encoded = EncodeGoogleApiProto(duration_min);
  EXPECT_TRUE(encoded.ok());
  EXPECT_THAT(*encoded, EqualsProto(proto_min));
}

TEST(ProtoUtilGoogleApi, DurationMinIsMin) {
  const absl::Duration duration_min = MakeGoogleApiDurationMin();
  EXPECT_EQ(
      EncodeGoogleApiProto(duration_min - absl::Nanoseconds(1)).status().code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(ProtoUtilGoogleApi, RoundTripTime) {
  // Shorthand to make the test cases readable.
  const absl::Time epoch = absl::UnixEpoch();  // The protobuf epoch.
  const auto& s = [](int64_t n) { return absl::Seconds(n); };
  const auto& ns = [](int64_t n) { return absl::Nanoseconds(n); };

  const struct {
    absl::Time t;

    struct {
      int64_t sec;
      int32_t nsec;
    } expected;
  } kTestCases[] = {
      {epoch, {0, 0}},
      {epoch - ns(1), {-1, 999999999}},
      {epoch + ns(1), {0, 1}},
      {epoch + s(123) + ns(456), {123, 456}},
      {epoch - ns(5), {-1, 999999995}},
      {epoch - s(10) - ns(5), {-11, 999999995}},
      {MakeGoogleApiTimeMin(), {-62135596800, 0}},
      {MakeGoogleApiTimeMax(), {253402300799, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    RoundTripGoogleApi<absl::Time, google::protobuf::Timestamp>(
        tc.t, tc.expected.sec, tc.expected.nsec);
  }
}

TEST(ProtoUtilGoogleApi, TimeTruncTowardInfPast) {
  const absl::Duration tick = absl::Nanoseconds(1) / 4;
  const absl::Time before_epoch = absl::FromUnixSeconds(-1234567890);
  const absl::Time epoch = absl::UnixEpoch();
  const absl::Time after_epoch = absl::FromUnixSeconds(1234567890);

  const struct {
    absl::Time t;

    struct {
      int64_t sec;
      int32_t nsec;
    } expected;
  } kTestCases[] = {
      {before_epoch + tick, {-1234567890, 0}},
      {before_epoch - tick, {-1234567890 - 1, 999999999}},
      {epoch + tick, {0, 0}},
      {epoch - tick, {-1, 999999999}},
      {after_epoch + tick, {1234567890, 0}},
      {after_epoch - tick, {1234567890 - 1, 999999999}},
      {MakeGoogleApiTimeMin() + tick, {-62135596800, 0}},
      {MakeGoogleApiTimeMax() - tick, {253402300799, 999999998}},
      {MakeGoogleApiTimeMax() + tick, {253402300799, 999999999}},
  };

  for (const auto& tc : kTestCases) {
    const auto sor = EncodeGoogleApiProto(tc.t);
    ASSERT_TRUE(sor.ok());
    const auto& proto = *sor;
    EXPECT_EQ(proto.seconds(), tc.expected.sec) << "t=" << tc.t;
    EXPECT_EQ(proto.nanos(), tc.expected.nsec) << "t=" << tc.t;
  }
}

TEST(ProtoUtilGoogleApi, EncodeTimeError) {
  const absl::Time kTestCases[] = {
      MakeGoogleApiTimeMin() - absl::Nanoseconds(1),  //
      MakeGoogleApiTimeMax() + absl::Nanoseconds(1),  //
      absl::InfinitePast(),                           //
      absl::InfiniteFuture(),                         //
  };

  for (const auto& t : kTestCases) {
    const auto sor = EncodeGoogleApiProto(t);
    EXPECT_FALSE(sor.ok()) << "t=" << t;

    google::protobuf::Timestamp proto;
    const auto status = EncodeGoogleApiProto(t, &proto);
    EXPECT_FALSE(status.ok()) << "t=" << t;
  }
}

TEST(ProtoUtilGoogleApi, DecodeTimeError) {
  const google::protobuf::Timestamp kTestCases[] = {
      MakeGoogleApiTimestamp(1, -1),             //
      MakeGoogleApiTimestamp(1, 999999999 + 1),  //
      MakeGoogleApiTimestamp(
          absl::ToUnixSeconds(MakeGoogleApiTimeMin() - absl::Seconds(1)),
          0),  //
      MakeGoogleApiTimestamp(
          absl::ToUnixSeconds(MakeGoogleApiTimeMax() + absl::Seconds(1)),
          0),                                                          //
      MakeGoogleApiTimestamp(std::numeric_limits<int64_t>::min(), 0),  //
      MakeGoogleApiTimestamp(std::numeric_limits<int64_t>::max(), 0),  //
      MakeGoogleApiTimestamp(0, std::numeric_limits<int32_t>::min()),  //
      MakeGoogleApiTimestamp(0, std::numeric_limits<int32_t>::max()),  //
      MakeGoogleApiTimestamp(std::numeric_limits<int64_t>::min(),
                             std::numeric_limits<int32_t>::min()),  //
      MakeGoogleApiTimestamp(std::numeric_limits<int64_t>::max(),
                             std::numeric_limits<int32_t>::max()),  //
  };

  for (const auto& d : kTestCases) {
    const auto sor = DecodeGoogleApiProto(d);
    EXPECT_FALSE(sor.ok()) << "d=" << d.DebugString();
  }
}

TEST(ProtoUtilGoogleApi, TimestampProtoMax) {
  const auto proto_max = MakeGoogleApiTimestampProtoMax();
  const absl::Time time_max = MakeGoogleApiTimeMax();
  auto decode = DecodeGoogleApiProto(proto_max);
  EXPECT_TRUE(decode.ok());
  EXPECT_EQ(*decode, time_max);
  auto encode = EncodeGoogleApiProto(time_max);
  EXPECT_TRUE(encode.ok());
  EXPECT_THAT(*encode, EqualsProto(proto_max));
}

TEST(ProtoUtilGoogleApi, TimeMaxIsOK) {
  const absl::Time time_max = MakeGoogleApiTimeMax();
  const absl::TimeZone utc = absl::UTCTimeZone();
  const absl::Time other_time_max =
      absl::FromCivil(absl::CivilSecond(9999, 12, 31, 23, 59, 59), utc) +
      absl::Nanoseconds(999999999);
  EXPECT_EQ(time_max, other_time_max);
}

TEST(ProtoUtilGoogleApi, TimeMaxIsMax) {
  const absl::Time time_max = MakeGoogleApiTimeMax();
  EXPECT_EQ(
      EncodeGoogleApiProto(time_max + absl::Nanoseconds(1)).status().code(),
      absl::StatusCode::kInvalidArgument);
}

TEST(ProtoUtilGoogleApi, TimestampProtoMin) {
  const auto proto_min = MakeGoogleApiTimestampProtoMin();
  const absl::Time time_min = MakeGoogleApiTimeMin();
  auto decode = DecodeGoogleApiProto(proto_min);
  EXPECT_TRUE(decode.ok());
  EXPECT_EQ(*decode, time_min);
  auto encode = EncodeGoogleApiProto(time_min);
  EXPECT_TRUE(encode.ok());
  EXPECT_THAT(*encode, EqualsProto(proto_min));
}

TEST(ProtoUtilGoogleApi, TimeMinIsOK) {
  const absl::Time time_min = MakeGoogleApiTimeMin();
  const absl::TimeZone utc = absl::UTCTimeZone();
  const absl::Time other_time_min =
      absl::FromCivil(absl::CivilSecond(1, 1, 1, 0, 0, 0), utc);
  EXPECT_EQ(time_min, other_time_min);
}

TEST(ProtoUtilGoogleApi, TimeMinIsMin) {
  const absl::Time time_min = MakeGoogleApiTimeMin();
  EXPECT_EQ(
      EncodeGoogleApiProto(time_min - absl::Nanoseconds(1)).status().code(),
      absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace privacy_sandbox::server_common
