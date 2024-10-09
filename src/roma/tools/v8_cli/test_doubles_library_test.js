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

goog.module('google.scp.roma.tools.v8_cli.test_doubles_library_test');
goog.setTestOnly('google.scp.roma.tools.v8_cli.test_doubles_library_test');

const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testSequentialAccess() {
    const data = [1, 2, 3];
    const callback = RegisterCallbackData(data);

    // Assert sequential access
    assertEquals(1, callback());
    assertEquals(2, callback());
    assertEquals(3, callback());
    assertNull(callback()); // No repeat
  },

  testSequentialAccessWithRepeat() {
    const data = [1, 2, 3];
    const callback = RegisterCallbackData(data, { repeatLast: true });

    // Assert sequential access with repeat
    assertEquals(1, callback());
    assertEquals(2, callback());
    assertEquals(3, callback());
    assertEquals(3, callback()); // Repeats last
  },

  testObjectLookup() {
    const data = { a: 10, b: 20 };
    const callback = RegisterCallbackData(data);

    // Assert object lookup
    assertEquals(10, callback('a'));
    assertEquals(20, callback('b'));
    assertNull(callback('c')); // Key not found
  },

  testArrayLookup() {
    const data = [1, 2, 3];
    const callback = RegisterCallbackData(data, { accessSequential: false });

    // Assert array lookup (using joined keys)
    assertEquals(1, callback(0));
    assertEquals(2, callback(1));
    assertEquals(3, callback(2));
    assertNull(callback(3)); // Index out of bounds
  },

  testObjectLookupFirstArgMatch() {
    const data = { a: 10, b: 20 };
    const callback = RegisterCallbackData(data, { firstArgMatch: true });

    assertEquals(10, callback('a', 'b')); // Only first arg considered
  },

  testArrayLookupFirstArgMatch() {
    const data = [1, 2, 3];
    const callback = RegisterCallbackData(data, { accessSequential: false, firstArgMatch: true });

    assertEquals(1, callback(0));
    assertEquals(1, callback(0, 1)); // Only first arg considered
  },

  testArrayLookupArgCountMatch() {
    const data = [1, 2, 3];
    const callback = RegisterCallbackData(data, { accessSequential: false, argCountMatch: true });

    assertEquals(1, callback('a')); // 1 arg, return 1st element
    assertEquals(2, callback('a', 'b')); // 2 args, return 2nd element
    assertEquals(3, callback('a', 'b', 'c')); // 3 args, return 3rd element
    assertNull(callback('a', 'b', 'c', 'd')); // more args than data, return null
  },

  testNullData() {
    const callback = RegisterCallbackData(null);
    assertNull(callback());
  },
});
