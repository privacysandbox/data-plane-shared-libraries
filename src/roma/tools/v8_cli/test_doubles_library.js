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

/**
 * Registers a callback function based on the provided data and configuration flags.
 *
 * @param {*} data - The data source (array or object) for the callback.
 * @param {{ accessSequential: (boolean|undefined), repeatLast: (boolean|undefined), firstArgMatch: (boolean|undefined), argCountMatch: (boolean|undefined) }=} config - Configuration options.
 * @return {function(): *} A callback function that retrieves data based on the configuration.
 */
function RegisterCallbackData(
  data,
  { accessSequential = true, repeatLast = false, firstArgMatch = false, argCountMatch = false } = {}
) {
  if (!data) {
    return () => null;
  }

  let count = 0;

  if (Array.isArray(data) && accessSequential) {
    return function () {
      count++;
      if (count <= data.length) {
        return data[count - 1];
      } else if (repeatLast) {
        return data[data.length - 1];
      } else {
        return null;
      }
    };
  }

  return function () {
    let key = '';
    if (firstArgMatch) {
      key = arguments[0];
    } else if (argCountMatch) {
      key = arguments.length - 1;
    } else {
      key = [...arguments].join();
    }
    return data[key] ?? null;
  };
}
