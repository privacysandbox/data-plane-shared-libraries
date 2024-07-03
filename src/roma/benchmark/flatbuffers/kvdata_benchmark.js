/**
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function Base64Uint8Decode(input) {
  const TABLE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  const REGEX_SPACE_CHARACTERS = /[\t\n\f\r ]/g;
  input = String(input).replace(REGEX_SPACE_CHARACTERS, '');
  var bitCounter = 0;
  var bitStorage;
  var output = new Uint8Array(input.length); // allocate sufficient space
  var output_position = 0;
  var position = -1;
  while (++position < input.length) {
    const buffer = TABLE.indexOf(input.charAt(position));
    bitStorage = bitCounter % 4 ? bitStorage * 64 + buffer : buffer;
    if (bitCounter++ % 4) {
      output[output_position++] = 255 & (bitStorage >> ((-2 * bitCounter) & 6));
    }
  }
  return new Uint8Array(output.buffer, 0, output_position);
}

function PrintObjKeys(obj) {
  const keys = [];
  for (x in obj) {
    keys.push(x);
  }
  return keys.join(' ');
}

function PrintObj(obj) {
  let str = '{';
  for (x in obj) {
    str += `${x}: ${obj[x]}\n`;
  }
  str += '}';
  return str;
}

kv_fbs_data = (function () {
  if (kv_fbs_data != null) {
    return kv_fbs_data;
  }
  const fbs_data_b64 = '';
  return Base64Uint8Decode(fbs_data_b64);
})();

function Handler() {
  const kv_data = data_structures.privacysandbox_roma_benchmarks.KVData.getRootAsKVData(
    new flatbuffers.ByteBuffer(kv_fbs_data)
  );
  console.log('kv_data: ', PrintObj(kv_data.kv(10).unpack()));
  console.log('kv_data: ', PrintObj(kv_data.kv(12).unpack()));
  console.log('kv_data: ', PrintObj(kv_data.kv(820).unpack()));
  console.log('FlatBuffers bazel repository test: completed successfully');
}
