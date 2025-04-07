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

#ifndef ROMA_CONFIG_TYPE_CONVERTER
#define ROMA_CONFIG_TYPE_CONVERTER

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/map.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/struct.pb.h>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "include/v8.h"

namespace google::scp::roma {
template <typename T>
struct TypeConverter {};

template <>
struct TypeConverter<std::string> {
  static v8::Local<v8::Value> ToV8(absl::Nonnull<v8::Isolate*> isolate,
                                   std::string_view val) {
    return v8::String::NewFromUtf8(isolate, val.data(),
                                   v8::NewStringType::kNormal, val.size())
        .ToLocalChecked();
  }

  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val,
                     absl::Nonnull<std::string*> out) {
    if (val.IsEmpty() || !val->IsString()) {
      return false;
    }
    v8::HandleScope scope(isolate);
    v8::TryCatch trycatch(isolate);

    v8::Local<v8::String> str;
    if (!val->ToString(isolate->GetCurrentContext()).ToLocal(&str)) {
      return false;
    }

    const size_t len = str->Utf8Length(isolate);
    out->resize(len);
    str->WriteUtf8(isolate, &(*out)[0], len, nullptr,
                   v8::String::NO_NULL_TERMINATION);

    return true;
  }
};

template <>
struct TypeConverter<std::vector<std::string>> {
  static v8::Local<v8::Value> ToV8(
      absl::Nonnull<v8::Isolate*> isolate,
      const google::protobuf::RepeatedPtrField<std::string>& val) {
    v8::Local<v8::Array> array = v8::Array::New(isolate, val.size());

    // Return an empty result if there was an error creating the array.
    if (array.IsEmpty()) {
      return v8::Local<v8::Array>();
    }
    for (size_t i = 0; i < val.size(); i++) {
      const auto v8_str = TypeConverter<std::string>::ToV8(isolate, val.at(i));
      const auto result = array->Set(isolate->GetCurrentContext(), i, v8_str);
      result.Check();
    }
    return array;
  }

  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val,
                     absl::Nonnull<std::vector<std::string>*> out) {
    if (val.IsEmpty() || !val->IsArray()) {
      return false;
    }
    auto array = val.As<v8::Array>();
    out->reserve(array->Length());
    for (size_t i = 0; i < array->Length(); i++) {
      auto item = array->Get(isolate->GetCurrentContext(), i).ToLocalChecked();
      if (std::string str;
          TypeConverter<std::string>::FromV8(isolate, item, &str)) {
        out->push_back(std::move(str));
      } else {
        out->clear();
        return false;
      }
    }

    return true;
  }
};

template <>
struct TypeConverter<google::protobuf::Value> {
  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val,
                     absl::Nonnull<google::protobuf::Value*> out) {
    google::protobuf::Value proto_val;

    if (val->IsNull() || val->IsUndefined()) {
      proto_val.set_null_value(google::protobuf::NullValue::NULL_VALUE);
    } else if (val->IsBoolean()) {
      proto_val.set_bool_value(val->BooleanValue(isolate));
    } else if (val->IsNumber()) {
      double num;
      if (!val->NumberValue(isolate->GetCurrentContext()).To(&num)) {
        return false;
      }
      proto_val.set_number_value(num);
    } else if (val->IsString()) {
      std::string str_val;
      if (!TypeConverter<std::string>::FromV8(isolate, val, &str_val)) {
        return false;
      }
      proto_val.set_string_value(std::move(str_val));
    } else if (val->IsArray()) {
      auto arr = val.As<v8::Array>();
      auto list_val = proto_val.mutable_list_value();

      for (int i = 0; i < arr->Length(); i++) {
        v8::Local<v8::Value> element;
        if (!arr->Get(isolate->GetCurrentContext(), i).ToLocal(&element)) {
          return false;
        }
        google::protobuf::Value element_val;
        if (!FromV8(isolate, element, &element_val)) {
          return false;
        }
        *list_val->add_values() = std::move(element_val);
      }
    } else if (val->IsObject()) {
      auto obj = val.As<v8::Object>();
      auto nested_struct = proto_val.mutable_struct_value();

      v8::Local<v8::Array> nested_props;
      if (!obj->GetOwnPropertyNames(isolate->GetCurrentContext())
               .ToLocal(&nested_props)) {
        return false;
      }

      for (int i = 0; i < nested_props->Length(); i++) {
        v8::Local<v8::Value> prop_name;
        if (!nested_props->Get(isolate->GetCurrentContext(), i)
                 .ToLocal(&prop_name)) {
          return false;
        }

        v8::Local<v8::Value> prop_value;
        if (!obj->Get(isolate->GetCurrentContext(), prop_name)
                 .ToLocal(&prop_value)) {
          return false;
        }

        std::string key;
        if (!TypeConverter<std::string>::FromV8(isolate, prop_name, &key)) {
          continue;
        }

        google::protobuf::Value field_val;
        if (!FromV8(isolate, prop_value, &field_val)) {
          return false;
        }
        (*nested_struct->mutable_fields())[std::move(key)] =
            std::move(field_val);
      }
    } else {
      // Default to null for unsupported types
      proto_val.set_null_value(google::protobuf::NullValue::NULL_VALUE);
    }

    *out = std::move(proto_val);
    return true;
  }
};

template <>
struct TypeConverter<google::protobuf::Struct> {
  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val,
                     absl::Nonnull<google::protobuf::Struct*> out) {
    if (!val->IsObject()) {
      return false;
    }
    auto v8_obj = val.As<v8::Object>();
    auto context = isolate->GetCurrentContext();

    v8::Local<v8::Array> property_names;
    if (!v8_obj->GetOwnPropertyNames(context).ToLocal(&property_names)) {
      return false;
    }
    for (int i = 0; i < property_names->Length(); i++) {
      v8::Local<v8::Value> prop;
      if (!property_names->Get(context, i).ToLocal(&prop)) {
        return false;
      }
      std::string prop_str;
      if (!TypeConverter<std::string>::FromV8(isolate, prop, &prop_str)) {
        out->clear_fields();
        return false;
      }

      v8::Local<v8::Value> field;
      if (!v8_obj->Get(context, prop).ToLocal(&field)) {
        return false;
      }

      google::protobuf::Value field_val;
      if (!TypeConverter<google::protobuf::Value>::FromV8(isolate, field,
                                                          &field_val)) {
        out->clear_fields();
        return false;
      }
      (*out->mutable_fields())[std::move(prop_str)] = std::move(field_val);
    }

    return true;
  }
};

template <>
struct TypeConverter<absl::flat_hash_map<std::string, std::string>> {
  static v8::Local<v8::Value> ToV8(
      absl::Nonnull<v8::Isolate*> isolate,
      const google::protobuf::Map<std::string, std::string>& val) {
    auto map = v8::Map::New(isolate);
    for (const auto& [k, v] : val) {
      // void-cast to ignore v8::Map::Set failure.
      (void)map->Set(isolate->GetCurrentContext(),
                     TypeConverter<std::string>::ToV8(isolate, k),
                     TypeConverter<std::string>::ToV8(isolate, v));
    }
    return map;
  }

  // Populates out parameter, `out`, with the contents of a V8 object, `val`,
  // for use in converting base JS objects to absl::flat_hash_map<std::string,
  // std::string>
  static bool FromV8Object(
      absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Value> val,
      absl::Nonnull<absl::flat_hash_map<std::string, std::string>*> out) {
    if (!val->IsObject()) {
      return false;
    }
    auto v8_obj = val.As<v8::Object>();

    auto property_names =
        v8_obj->GetOwnPropertyNames(isolate->GetCurrentContext())
            .ToLocalChecked();
    for (auto i = 0; i < property_names->Length(); i++) {
      auto prop =
          property_names->Get(isolate->GetCurrentContext(), i).ToLocalChecked();
      std::string prop_str;
      bool prop_conversion =
          TypeConverter<std::string>::FromV8(isolate, prop, &prop_str);

      auto field =
          v8_obj->Get(isolate->GetCurrentContext(), prop).ToLocalChecked();
      std::string field_str;
      bool field_conversion =
          TypeConverter<std::string>::FromV8(isolate, field, &field_str);

      if (!prop_conversion || !field_conversion) {
        out->clear();
        return false;
      }
      (*out)[prop_str] = field_str;
    }

    return true;
  }

  static bool FromV8(
      absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Value> val,
      absl::Nonnull<absl::flat_hash_map<std::string, std::string>*> out) {
    if (!out || val.IsEmpty() || !val->IsMap()) {
      return false;
    }
    auto v8_map = val.As<v8::Map>();
    // This turns the map into an array of size Size()*2, where index N is a
    // key, and N+1 is the value for the given key.
    const auto& m_arr = v8_map->AsArray();
    out->reserve(m_arr->Length());
    for (auto i = 0; i < m_arr->Length(); i += 2) {
      auto k = m_arr->Get(isolate->GetCurrentContext(), i).ToLocalChecked();
      auto v = m_arr->Get(isolate->GetCurrentContext(), i + 1).ToLocalChecked();
      if (std::string k_str, v_str;
          TypeConverter<std::string>::FromV8(isolate, k, &k_str) &&
          TypeConverter<std::string>::FromV8(isolate, v, &v_str)) {
        (*out)[k_str] = v_str;
      } else {
        out->clear();
        return false;
      }
    }

    return true;
  }
};

template <>
struct TypeConverter<uint32_t> {
  static v8::Local<v8::Value> ToV8(absl::Nonnull<v8::Isolate*> isolate,
                                   const uint32_t& val) {
    return v8::Integer::NewFromUnsigned(isolate, val);
  }

  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val, absl::Nonnull<uint32_t*> out) {
    if (val.IsEmpty() || !val->IsUint32()) {
      return false;
    }
    *out = val.As<v8::Uint32>()->Value();
    return true;
  }
};

template <>
struct TypeConverter<uint8_t*> {
  static v8::Local<v8::Value> ToV8(absl::Nonnull<v8::Isolate*> isolate,
                                   absl::Nonnull<const uint8_t*> data,
                                   size_t data_size) {
    auto buffer = v8::ArrayBuffer::New(isolate, data_size);
    memcpy(buffer->Data(), data, data_size);
    return v8::Uint8Array::New(buffer, 0, data_size);
  }

  static v8::Local<v8::Value> ToV8(absl::Nonnull<v8::Isolate*> isolate,
                                   std::string_view data) {
    return ToV8(isolate, reinterpret_cast<const uint8_t*>(data.data()),
                data.size());
  }

  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val, absl::Nonnull<uint8_t*> out,
                     size_t out_buffer_size) {
    if (val.IsEmpty() || !val->IsUint8Array() || out == nullptr) {
      return false;
    }
    auto val_array = val.As<v8::Uint8Array>();
    // The buffer size needs to match the size of the data we're trying to
    // write.
    if (val_array->Length() != out_buffer_size) {
      return false;
    }
    memcpy(out, val_array->Buffer()->Data(), val_array->Length());
    return true;
  }

  static bool FromV8(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::Value> val, std::string& data) {
    return FromV8(isolate, val, reinterpret_cast<uint8_t*>(data.data()),
                  data.size());
  }
};
}  // namespace google::scp::roma

#endif  // ROMA_CONFIG_TYPE_CONVERTER
