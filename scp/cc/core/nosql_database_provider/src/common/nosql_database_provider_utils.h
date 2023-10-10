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

#ifndef CORE_NOSQL_DATABASE_PROVIDER_SRC_COMMON_NOSQL_DATABASE_PROVIDER_UTILS_H_
#define CORE_NOSQL_DATABASE_PROVIDER_SRC_COMMON_NOSQL_DATABASE_PROVIDER_UTILS_H_

#include <string>
#include <string_view>
#include <variant>

#include "absl/strings/numbers.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/src/common/error_codes.h"

namespace google::scp::core::nosql_database_provider {
class NoSQLDatabaseProviderUtils {
 public:
  /**
   * @brief Creates a NoSQLDatabaseValidAttributeValueTypes from string object
   * and assigns the value.
   *
   * @tparam T The type into which to parse the specified string value.
   * @param value The input char array value.
   * @param length The length of the char array.
   * @param no_sql_database_valid_attribute The output object to be set.
   * @return ExecutionResult The execution result of the operation.
   */
  template <typename T>
  static ExecutionResult FromString(
      const char* value, const size_t length,
      NoSQLDatabaseValidAttributeValueTypes& no_sql_database_valid_attribute) {
    const FailureExecutionResult error_invalid_param(
        errors::SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE);
    if (value == nullptr) {
      return error_invalid_param;
    }

    if (std::is_same<T, int>::value) {
      int int_value = 0;
      if (absl::SimpleAtoi(std::string_view(value, length), &int_value)) {
        no_sql_database_valid_attribute = int_value;
        return SuccessExecutionResult();
      }
    } else if (std::is_same<T, double>::value) {
      double dbl_value = 0;
      if (absl::SimpleAtod(std::string_view(value, length), &dbl_value)) {
        no_sql_database_valid_attribute = dbl_value;
        return SuccessExecutionResult();
      }
    } else if (std::is_same<T, float>::value) {
      float float_value = 0;
      if (absl::SimpleAtof(std::string_view(value, length), &float_value)) {
        no_sql_database_valid_attribute = float_value;
        return SuccessExecutionResult();
      }
    } else if (std::is_same<T, std::string>::value) {
      no_sql_database_valid_attribute = std::string(value, length);
      return SuccessExecutionResult();
    }

    return error_invalid_param;
  }
};
}  // namespace google::scp::core::nosql_database_provider

#endif  // CORE_NOSQL_DATABASE_PROVIDER_SRC_COMMON_NOSQL_DATABASE_PROVIDER_UTILS_H_
