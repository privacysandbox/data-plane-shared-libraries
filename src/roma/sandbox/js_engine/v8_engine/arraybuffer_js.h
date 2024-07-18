/*
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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_

#include <string_view>

inline constexpr std::string_view kModuleJs = R"JS_CODE(
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
)JS_CODE";

inline constexpr std::string_view kTextDecoderJs = R"JS_CODE(
// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/utils.js
var require_utils = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/utils.js"(exports, module) {
    function inRange(a, min, max) {
      return min <= a && a <= max;
    }
    var floor = Math.floor;
    function stringToCodePoints(string) {
      var s = String(string);
      var n = s.length;
      var i = 0;
      var u = [];
      while (i < n) {
        var c = s.charCodeAt(i);
        if (c < 55296 || c > 57343) {
          u.push(c);
        } else if (56320 <= c && c <= 57343) {
          u.push(65533);
        } else if (55296 <= c && c <= 56319) {
          if (i === n - 1) {
            u.push(65533);
          } else {
            var d = s.charCodeAt(i + 1);
            if (56320 <= d && d <= 57343) {
              var a = c & 1023;
              var b = d & 1023;
              u.push(65536 + (a << 10) + b);
              i += 1;
            } else {
              u.push(65533);
            }
          }
        }
        i += 1;
      }
      return u;
    }
    function codePointsToString(code_points) {
      var s = "";
      for (var i = 0; i < code_points.length; ++i) {
        var cp = code_points[i];
        if (cp <= 65535) {
          s += String.fromCharCode(cp);
        } else {
          cp -= 65536;
          s += String.fromCharCode(
            (cp >> 10) + 55296,
            (cp & 1023) + 56320
          );
        }
      }
      return s;
    }
    function decoderError(fatal, opt_code_point) {
      if (fatal)
        throw TypeError("Decoder error");
      return opt_code_point || 65533;
    }
    function encoderError(code_point) {
      throw TypeError("The code point " + code_point + " could not be encoded.");
    }
    function convertCodeUnitToBytes(code_unit, utf16be) {
      const byte1 = code_unit >> 8;
      const byte2 = code_unit & 255;
      if (utf16be)
        return [byte1, byte2];
      return [byte2, byte1];
    }
    function isASCIIByte(a) {
      return 0 <= a && a <= 127;
    }
    var isASCIICodePoint = isASCIIByte;
    var end_of_stream = -1;
    var finished = -1;
    module.exports.inRange = inRange;
    module.exports.floor = floor;
    module.exports.stringToCodePoints = stringToCodePoints;
    module.exports.codePointsToString = codePointsToString;
    module.exports.decoderError = decoderError;
    module.exports.encoderError = encoderError;
    module.exports.convertCodeUnitToBytes = convertCodeUnitToBytes;
    module.exports.isASCIIByte = isASCIIByte;
    module.exports.isASCIICodePoint = isASCIICodePoint;
    module.exports.end_of_stream = end_of_stream;
    module.exports.finished = finished;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/encodings.js
const require_encodings = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/encodings.js"(exports, module) {
    var encodings = [
      {
        encodings: [
          {
            labels: [
              "unicode-1-1-utf-8",
              "utf-8",
              "utf8"
            ],
            name: "UTF-8"
          }
        ],
        heading: "The Encoding"
      },
      {
        encodings: [
          {
            labels: [
              "866",
              "cp866",
              "csibm866",
              "ibm866"
            ],
            name: "IBM866"
          },
          {
            labels: [
              "csisolatin2",
              "iso-8859-2",
              "iso-ir-101",
              "iso8859-2",
              "iso88592",
              "iso_8859-2",
              "iso_8859-2:1987",
              "l2",
              "latin2"
            ],
            name: "ISO-8859-2"
          },
          {
            labels: [
              "csisolatin3",
              "iso-8859-3",
              "iso-ir-109",
              "iso8859-3",
              "iso88593",
              "iso_8859-3",
              "iso_8859-3:1988",
              "l3",
              "latin3"
            ],
            name: "ISO-8859-3"
          },
          {
            labels: [
              "csisolatin4",
              "iso-8859-4",
              "iso-ir-110",
              "iso8859-4",
              "iso88594",
              "iso_8859-4",
              "iso_8859-4:1988",
              "l4",
              "latin4"
            ],
            name: "ISO-8859-4"
          },
          {
            labels: [
              "csisolatincyrillic",
              "cyrillic",
              "iso-8859-5",
              "iso-ir-144",
              "iso8859-5",
              "iso88595",
              "iso_8859-5",
              "iso_8859-5:1988"
            ],
            name: "ISO-8859-5"
          },
          {
            labels: [
              "arabic",
              "asmo-708",
              "csiso88596e",
              "csiso88596i",
              "csisolatinarabic",
              "ecma-114",
              "iso-8859-6",
              "iso-8859-6-e",
              "iso-8859-6-i",
              "iso-ir-127",
              "iso8859-6",
              "iso88596",
              "iso_8859-6",
              "iso_8859-6:1987"
            ],
            name: "ISO-8859-6"
          },
          {
            labels: [
              "csisolatingreek",
              "ecma-118",
              "elot_928",
              "greek",
              "greek8",
              "iso-8859-7",
              "iso-ir-126",
              "iso8859-7",
              "iso88597",
              "iso_8859-7",
              "iso_8859-7:1987",
              "sun_eu_greek"
            ],
            name: "ISO-8859-7"
          },
          {
            labels: [
              "csiso88598e",
              "csisolatinhebrew",
              "hebrew",
              "iso-8859-8",
              "iso-8859-8-e",
              "iso-ir-138",
              "iso8859-8",
              "iso88598",
              "iso_8859-8",
              "iso_8859-8:1988",
              "visual"
            ],
            name: "ISO-8859-8"
          },
          {
            labels: [
              "csiso88598i",
              "iso-8859-8-i",
              "logical"
            ],
            name: "ISO-8859-8-I"
          },
          {
            labels: [
              "csisolatin6",
              "iso-8859-10",
              "iso-ir-157",
              "iso8859-10",
              "iso885910",
              "l6",
              "latin6"
            ],
            name: "ISO-8859-10"
          },
          {
            labels: [
              "iso-8859-13",
              "iso8859-13",
              "iso885913"
            ],
            name: "ISO-8859-13"
          },
          {
            labels: [
              "iso-8859-14",
              "iso8859-14",
              "iso885914"
            ],
            name: "ISO-8859-14"
          },
          {
            labels: [
              "csisolatin9",
              "iso-8859-15",
              "iso8859-15",
              "iso885915",
              "iso_8859-15",
              "l9"
            ],
            name: "ISO-8859-15"
          },
          {
            labels: [
              "iso-8859-16"
            ],
            name: "ISO-8859-16"
          },
          {
            labels: [
              "cskoi8r",
              "koi",
              "koi8",
              "koi8-r",
              "koi8_r"
            ],
            name: "KOI8-R"
          },
          {
            labels: [
              "koi8-ru",
              "koi8-u"
            ],
            name: "KOI8-U"
          },
          {
            labels: [
              "csmacintosh",
              "mac",
              "macintosh",
              "x-mac-roman"
            ],
            name: "macintosh"
          },
          {
            labels: [
              "dos-874",
              "iso-8859-11",
              "iso8859-11",
              "iso885911",
              "tis-620",
              "windows-874"
            ],
            name: "windows-874"
          },
          {
            labels: [
              "cp1250",
              "windows-1250",
              "x-cp1250"
            ],
            name: "windows-1250"
          },
          {
            labels: [
              "cp1251",
              "windows-1251",
              "x-cp1251"
            ],
            name: "windows-1251"
          },
          {
            labels: [
              "ansi_x3.4-1968",
              "ascii",
              "cp1252",
              "cp819",
              "csisolatin1",
              "ibm819",
              "iso-8859-1",
              "iso-ir-100",
              "iso8859-1",
              "iso88591",
              "iso_8859-1",
              "iso_8859-1:1987",
              "l1",
              "latin1",
              "us-ascii",
              "windows-1252",
              "x-cp1252"
            ],
            name: "windows-1252"
          },
          {
            labels: [
              "cp1253",
              "windows-1253",
              "x-cp1253"
            ],
            name: "windows-1253"
          },
          {
            labels: [
              "cp1254",
              "csisolatin5",
              "iso-8859-9",
              "iso-ir-148",
              "iso8859-9",
              "iso88599",
              "iso_8859-9",
              "iso_8859-9:1989",
              "l5",
              "latin5",
              "windows-1254",
              "x-cp1254"
            ],
            name: "windows-1254"
          },
          {
            labels: [
              "cp1255",
              "windows-1255",
              "x-cp1255"
            ],
            name: "windows-1255"
          },
          {
            labels: [
              "cp1256",
              "windows-1256",
              "x-cp1256"
            ],
            name: "windows-1256"
          },
          {
            labels: [
              "cp1257",
              "windows-1257",
              "x-cp1257"
            ],
            name: "windows-1257"
          },
          {
            labels: [
              "cp1258",
              "windows-1258",
              "x-cp1258"
            ],
            name: "windows-1258"
          },
          {
            labels: [
              "x-mac-cyrillic",
              "x-mac-ukrainian"
            ],
            name: "x-mac-cyrillic"
          }
        ],
        heading: "Legacy single-byte encodings"
      },
      {
        encodings: [
          {
            labels: [
              "chinese",
              "csgb2312",
              "csiso58gb231280",
              "gb2312",
              "gb_2312",
              "gb_2312-80",
              "gbk",
              "iso-ir-58",
              "x-gbk"
            ],
            name: "GBK"
          },
          {
            labels: [
              "gb18030"
            ],
            name: "gb18030"
          }
        ],
        heading: "Legacy multi-byte Chinese (simplified) encodings"
      },
      {
        encodings: [
          {
            labels: [
              "big5",
              "big5-hkscs",
              "cn-big5",
              "csbig5",
              "x-x-big5"
            ],
            name: "Big5"
          }
        ],
        heading: "Legacy multi-byte Chinese (traditional) encodings"
      },
      {
        encodings: [
          {
            labels: [
              "cseucpkdfmtjapanese",
              "euc-jp",
              "x-euc-jp"
            ],
            name: "EUC-JP"
          },
          {
            labels: [
              "csiso2022jp",
              "iso-2022-jp"
            ],
            name: "ISO-2022-JP"
          },
          {
            labels: [
              "csshiftjis",
              "ms932",
              "ms_kanji",
              "shift-jis",
              "shift_jis",
              "sjis",
              "windows-31j",
              "x-sjis"
            ],
            name: "Shift_JIS"
          }
        ],
        heading: "Legacy multi-byte Japanese encodings"
      },
      {
        encodings: [
          {
            labels: [
              "cseuckr",
              "csksc56011987",
              "euc-kr",
              "iso-ir-149",
              "korean",
              "ks_c_5601-1987",
              "ks_c_5601-1989",
              "ksc5601",
              "ksc_5601",
              "windows-949"
            ],
            name: "EUC-KR"
          }
        ],
        heading: "Legacy multi-byte Korean encodings"
      },
      {
        encodings: [
          {
            labels: [
              "csiso2022kr",
              "hz-gb-2312",
              "iso-2022-cn",
              "iso-2022-cn-ext",
              "iso-2022-kr"
            ],
            name: "replacement"
          },
          {
            labels: [
              "utf-16be"
            ],
            name: "UTF-16BE"
          },
          {
            labels: [
              "utf-16",
              "utf-16le"
            ],
            name: "UTF-16LE"
          },
          {
            labels: [
              "x-user-defined"
            ],
            name: "x-user-defined"
          }
        ],
        heading: "Legacy miscellaneous encodings"
      }
    ];
    module.exports = encodings;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/implementations/utf8.js
const require_utf8 = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/implementations/utf8.js"(exports, module) {
    var {
      inRange,
      decoderError,
      isASCIICodePoint,
      end_of_stream,
      finished
    } = require_utils();
    var UTF8Decoder = class {
      /**
       * @param {{fatal: boolean}} options
       */
      constructor(options) {
        const { fatal } = options;
        let utf8_code_point = 0, utf8_bytes_seen = 0, utf8_bytes_needed = 0, utf8_lower_boundary = 128, utf8_upper_boundary = 191;
        this.handler = function(stream, bite) {
          if (bite === end_of_stream && utf8_bytes_needed !== 0) {
            utf8_bytes_needed = 0;
            return decoderError(fatal);
          }
          if (bite === end_of_stream)
            return finished;
          if (utf8_bytes_needed === 0) {
            if (inRange(bite, 0, 127)) {
              return bite;
            } else if (inRange(bite, 194, 223)) {
              utf8_bytes_needed = 1;
              utf8_code_point = bite & 31;
            } else if (inRange(bite, 224, 239)) {
              if (bite === 224)
                utf8_lower_boundary = 160;
              if (bite === 237)
                utf8_upper_boundary = 159;
              utf8_bytes_needed = 2;
              utf8_code_point = bite & 15;
            } else if (inRange(bite, 240, 244)) {
              if (bite === 240)
                utf8_lower_boundary = 144;
              if (bite === 244)
                utf8_upper_boundary = 143;
              utf8_bytes_needed = 3;
              utf8_code_point = bite & 7;
            } else {
              return decoderError(fatal);
            }
            return null;
          }
          if (!inRange(bite, utf8_lower_boundary, utf8_upper_boundary)) {
            utf8_code_point = utf8_bytes_needed = utf8_bytes_seen = 0;
            utf8_lower_boundary = 128;
            utf8_upper_boundary = 191;
            stream.prepend(bite);
            return decoderError(fatal);
          }
          utf8_lower_boundary = 128;
          utf8_upper_boundary = 191;
          utf8_code_point = utf8_code_point << 6 | bite & 63;
          utf8_bytes_seen += 1;
          if (utf8_bytes_seen !== utf8_bytes_needed)
            return null;
          var code_point = utf8_code_point;
          utf8_code_point = utf8_bytes_needed = utf8_bytes_seen = 0;
          return code_point;
        };
      }
    };
    var UTF8Encoder = class {
      constructor() {
        this.handler = function(stream, code_point) {
          if (code_point === end_of_stream)
            return finished;
          if (isASCIICodePoint(code_point))
            return code_point;
          var count, offset;
          if (inRange(code_point, 128, 2047)) {
            count = 1;
            offset = 192;
          } else if (inRange(code_point, 2048, 65535)) {
            count = 2;
            offset = 224;
          } else if (inRange(code_point, 65536, 1114111)) {
            count = 3;
            offset = 240;
          }
          var bytes = [(code_point >> 6 * count) + offset];
          while (count > 0) {
            var temp = code_point >> 6 * (count - 1);
            bytes.push(128 | temp & 63);
            count -= 1;
          }
          return bytes;
        };
      }
    };
    module.exports.UTF8Decoder = UTF8Decoder;
    module.exports.UTF8Encoder = UTF8Encoder;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/implementations/utf16.js
const require_utf16 = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/implementations/utf16.js"(exports, module) {
    var { inRange, decoderError, end_of_stream, finished, convertCodeUnitToBytes } = require_utils();
    var UTF16Decoder = class {
      /**
       * @param {boolean} utf16_be True if big-endian, false if little-endian.
       * @param {{fatal: boolean}} options
       */
      constructor(utf16_be, options) {
        const { fatal } = options;
        this.utf16_be = utf16_be;
        this.fatal = fatal;
        this.utf16_lead_byte = null;
        this.utf16_lead_surrogate = null;
      }
      /**
       * @param {Stream} stream The stream of bytes being decoded.
       * @param {number} bite The next byte read from the stream.
       */
      handler(stream, bite) {
        if (bite === end_of_stream && (this.utf16_lead_byte !== null || this.utf16_lead_surrogate !== null)) {
          return decoderError(this.fatal);
        }
        if (bite === end_of_stream && this.utf16_lead_byte === null && this.utf16_lead_surrogate === null) {
          return finished;
        }
        if (this.utf16_lead_byte === null) {
          this.utf16_lead_byte = bite;
          return null;
        }
        let code_unit;
        if (this.utf16_be) {
          code_unit = (this.utf16_lead_byte << 8) + bite;
        } else {
          code_unit = (bite << 8) + this.utf16_lead_byte;
        }
        this.utf16_lead_byte = null;
        if (this.utf16_lead_surrogate !== null) {
          const lead_surrogate = this.utf16_lead_surrogate;
          this.utf16_lead_surrogate = null;
          if (inRange(code_unit, 56320, 57343)) {
            return 65536 + (lead_surrogate - 55296) * 1024 + (code_unit - 56320);
          }
          stream.prepend(convertCodeUnitToBytes(code_unit, this.utf16_be));
          return decoderError(this.fatal);
        }
        if (inRange(code_unit, 55296, 56319)) {
          this.utf16_lead_surrogate = code_unit;
          return null;
        }
        if (inRange(code_unit, 56320, 57343))
          return decoderError(this.fatal);
        return code_unit;
      }
    };
    var UTF16Encoder = class {
      /**
       * @param {boolean} [utf16_be] True if big-endian, false if little-endian.
       */
      constructor(utf16_be = false) {
        this.utf16_be = utf16_be;
      }
      /**
       * @param {Stream} stream Input stream.
       * @param {number} code_point Next code point read from the stream.
       */
      handler(stream, code_point) {
        if (code_point === end_of_stream)
          return finished;
        if (inRange(code_point, 0, 65535))
          return convertCodeUnitToBytes(code_point, this.utf16_be);
        const lead = convertCodeUnitToBytes(
          (code_point - 65536 >> 10) + 55296,
          this.utf16_be
        );
        const trail = convertCodeUnitToBytes(
          (code_point - 65536 & 1023) + 56320,
          this.utf16_be
        );
        return lead.concat(trail);
      }
    };
    module.exports.UTF16Decoder = UTF16Decoder;
    module.exports.UTF16Encoder = UTF16Encoder;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/implementations/single-byte.js
const require_single_byte = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/implementations/single-byte.js"(exports, module) {
    var { end_of_stream, finished, isASCIIByte, decoderError, encoderError, isASCIICodePoint } = require_utils();
    var { indexPointerFor } = require_indexes();
    var SingleByteDecoder = class {
      /**
       * @param {!Array.<number>} index The encoding index.
       * @param {{fatal: boolean}} options
       */
      constructor(index, options) {
        const { fatal } = options;
        this.fatal = fatal;
        this.index = index;
      }
      /**
       * @param {Stream} stream The stream of bytes being decoded.
       * @param {number} bite The next byte read from the stream.
       */
      handler(stream, bite) {
        if (bite === end_of_stream)
          return finished;
        if (isASCIIByte(bite))
          return bite;
        var code_point = this.index[bite - 128];
        if (code_point === null)
          return decoderError(this.fatal);
        return code_point;
      }
    };
    var SingleByteEncoder = class {
      /**
       * @param {!Array.<?number>} index The encoding index.
       */
      constructor(index) {
        this.index = index;
      }
      /**
       * @param {Stream} stream Input stream.
       * @param {number} code_point Next code point read from the stream.
       * @return {(number|!Array.<number>)} Byte(s) to emit.
       */
      handler(stream, code_point) {
        if (code_point === end_of_stream)
          return finished;
        if (isASCIICodePoint(code_point))
          return code_point;
        const pointer = indexPointerFor(code_point, this.index);
        if (pointer === null)
          encoderError(code_point);
        return pointer + 128;
      }
    };
    module.exports.SingleByteDecoder = SingleByteDecoder;
    module.exports.SingleByteEncoder = SingleByteEncoder;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/encoding-indexes.js
const require_encoding_indexes = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/encoding-indexes.js"(exports, module) {
    var Indexes = {
      "iso-8859-2": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 260, 728, 321, 164, 317, 346, 167, 168, 352, 350, 356, 377, 173, 381, 379, 176, 261, 731, 322, 180, 318, 347, 711, 184, 353, 351, 357, 378, 733, 382, 380, 340, 193, 194, 258, 196, 313, 262, 199, 268, 201, 280, 203, 282, 205, 206, 270, 272, 323, 327, 211, 212, 336, 214, 215, 344, 366, 218, 368, 220, 221, 354, 223, 341, 225, 226, 259, 228, 314, 263, 231, 269, 233, 281, 235, 283, 237, 238, 271, 273, 324, 328, 243, 244, 337, 246, 247, 345, 367, 250, 369, 252, 253, 355, 729],
      "iso-8859-3": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 294, 728, 163, 164, null, 292, 167, 168, 304, 350, 286, 308, 173, null, 379, 176, 295, 178, 179, 180, 181, 293, 183, 184, 305, 351, 287, 309, 189, null, 380, 192, 193, 194, null, 196, 266, 264, 199, 200, 201, 202, 203, 204, 205, 206, 207, null, 209, 210, 211, 212, 288, 214, 215, 284, 217, 218, 219, 220, 364, 348, 223, 224, 225, 226, null, 228, 267, 265, 231, 232, 233, 234, 235, 236, 237, 238, 239, null, 241, 242, 243, 244, 289, 246, 247, 285, 249, 250, 251, 252, 365, 349, 729],
      "iso-8859-4": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 260, 312, 342, 164, 296, 315, 167, 168, 352, 274, 290, 358, 173, 381, 175, 176, 261, 731, 343, 180, 297, 316, 711, 184, 353, 275, 291, 359, 330, 382, 331, 256, 193, 194, 195, 196, 197, 198, 302, 268, 201, 280, 203, 278, 205, 206, 298, 272, 325, 332, 310, 212, 213, 214, 215, 216, 370, 218, 219, 220, 360, 362, 223, 257, 225, 226, 227, 228, 229, 230, 303, 269, 233, 281, 235, 279, 237, 238, 299, 273, 326, 333, 311, 244, 245, 246, 247, 248, 371, 250, 251, 252, 361, 363, 729],
      "iso-8859-5": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 1025, 1026, 1027, 1028, 1029, 1030, 1031, 1032, 1033, 1034, 1035, 1036, 173, 1038, 1039, 1040, 1041, 1042, 1043, 1044, 1045, 1046, 1047, 1048, 1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063, 1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 1086, 1087, 1088, 1089, 1090, 1091, 1092, 1093, 1094, 1095, 1096, 1097, 1098, 1099, 1100, 1101, 1102, 1103, 8470, 1105, 1106, 1107, 1108, 1109, 1110, 1111, 1112, 1113, 1114, 1115, 1116, 167, 1118, 1119],
      "iso-8859-6": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, null, null, null, 164, null, null, null, null, null, null, null, 1548, 173, null, null, null, null, null, null, null, null, null, null, null, null, null, 1563, null, null, null, 1567, null, 1569, 1570, 1571, 1572, 1573, 1574, 1575, 1576, 1577, 1578, 1579, 1580, 1581, 1582, 1583, 1584, 1585, 1586, 1587, 1588, 1589, 1590, 1591, 1592, 1593, 1594, null, null, null, null, null, 1600, 1601, 1602, 1603, 1604, 1605, 1606, 1607, 1608, 1609, 1610, 1611, 1612, 1613, 1614, 1615, 1616, 1617, 1618, null, null, null, null, null, null, null, null, null, null, null, null, null],
      "iso-8859-7": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 8216, 8217, 163, 8364, 8367, 166, 167, 168, 169, 890, 171, 172, 173, null, 8213, 176, 177, 178, 179, 900, 901, 902, 183, 904, 905, 906, 187, 908, 189, 910, 911, 912, 913, 914, 915, 916, 917, 918, 919, 920, 921, 922, 923, 924, 925, 926, 927, 928, 929, null, 931, 932, 933, 934, 935, 936, 937, 938, 939, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949, 950, 951, 952, 953, 954, 955, 956, 957, 958, 959, 960, 961, 962, 963, 964, 965, 966, 967, 968, 969, 970, 971, 972, 973, 974, null],
      "iso-8859-8": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, null, 162, 163, 164, 165, 166, 167, 168, 169, 215, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 247, 187, 188, 189, 190, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, null, 8215, 1488, 1489, 1490, 1491, 1492, 1493, 1494, 1495, 1496, 1497, 1498, 1499, 1500, 1501, 1502, 1503, 1504, 1505, 1506, 1507, 1508, 1509, 1510, 1511, 1512, 1513, 1514, null, null, 8206, 8207, null],
      "iso-8859-10": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 260, 274, 290, 298, 296, 310, 167, 315, 272, 352, 358, 381, 173, 362, 330, 176, 261, 275, 291, 299, 297, 311, 183, 316, 273, 353, 359, 382, 8213, 363, 331, 256, 193, 194, 195, 196, 197, 198, 302, 268, 201, 280, 203, 278, 205, 206, 207, 208, 325, 332, 211, 212, 213, 214, 360, 216, 370, 218, 219, 220, 221, 222, 223, 257, 225, 226, 227, 228, 229, 230, 303, 269, 233, 281, 235, 279, 237, 238, 239, 240, 326, 333, 243, 244, 245, 246, 361, 248, 371, 250, 251, 252, 253, 254, 312],
      "iso-8859-13": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 8221, 162, 163, 164, 8222, 166, 167, 216, 169, 342, 171, 172, 173, 174, 198, 176, 177, 178, 179, 8220, 181, 182, 183, 248, 185, 343, 187, 188, 189, 190, 230, 260, 302, 256, 262, 196, 197, 280, 274, 268, 201, 377, 278, 290, 310, 298, 315, 352, 323, 325, 211, 332, 213, 214, 215, 370, 321, 346, 362, 220, 379, 381, 223, 261, 303, 257, 263, 228, 229, 281, 275, 269, 233, 378, 279, 291, 311, 299, 316, 353, 324, 326, 243, 333, 245, 246, 247, 371, 322, 347, 363, 252, 380, 382, 8217],
      "iso-8859-14": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 7682, 7683, 163, 266, 267, 7690, 167, 7808, 169, 7810, 7691, 7922, 173, 174, 376, 7710, 7711, 288, 289, 7744, 7745, 182, 7766, 7809, 7767, 7811, 7776, 7923, 7812, 7813, 7777, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 372, 209, 210, 211, 212, 213, 214, 7786, 216, 217, 218, 219, 220, 221, 374, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 373, 241, 242, 243, 244, 245, 246, 7787, 248, 249, 250, 251, 252, 253, 375, 255],
      "iso-8859-15": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 8364, 165, 352, 167, 353, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 381, 181, 182, 183, 382, 185, 186, 187, 338, 339, 376, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255],
      "iso-8859-16": [128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 260, 261, 321, 8364, 8222, 352, 167, 353, 169, 536, 171, 377, 173, 378, 379, 176, 177, 268, 322, 381, 8221, 182, 183, 382, 269, 537, 187, 338, 339, 376, 380, 192, 193, 194, 258, 196, 262, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 272, 323, 210, 211, 212, 336, 214, 346, 368, 217, 218, 219, 220, 280, 538, 223, 224, 225, 226, 259, 228, 263, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 273, 324, 242, 243, 244, 337, 246, 347, 369, 249, 250, 251, 252, 281, 539, 255],
    };
    module.exports = Indexes;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/indexes.js
const require_indexes = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/indexes.js"(exports, module) {
    var { inRange } = require_utils();
    var Indexes = require_encoding_indexes();
    function indexCodePointFor(pointer, i) {
      if (!i) return null;
      return i[pointer] || null;
    }
    function indexPointerFor(code_point, i) {
      var pointer = i.indexOf(code_point);
      return pointer === -1 ? null : pointer;
    }
    function index(name) {
      return Indexes[name];
    }
    function indexGB18030RangesCodePointFor(pointer) {
      if (pointer > 39419 && pointer < 189e3 || pointer > 1237575)
        return null;
      if (pointer === 7457) return 59335;
      var offset = 0;
      var code_point_offset = 0;
      var idx = index("gb18030-ranges");
      var i;
      for (i = 0; i < idx.length; ++i) {
        var entry = idx[i];
        if (entry[0] <= pointer) {
          offset = entry[0];
          code_point_offset = entry[1];
        } else {
          break;
        }
      }
      return code_point_offset + pointer - offset;
    }
    function indexGB18030RangesPointerFor(code_point) {
      if (code_point === 59335) return 7457;
      var offset = 0;
      var pointer_offset = 0;
      var idx = index("gb18030-ranges");
      var i;
      for (i = 0; i < idx.length; ++i) {
        var entry = idx[i];
        if (entry[1] <= code_point) {
          offset = entry[1];
          pointer_offset = entry[0];
        } else {
          break;
        }
      }
      return pointer_offset + code_point - offset;
    }
    function indexShiftJISPointerFor(code_point) {
      shift_jis_index = shift_jis_index || index("jis0208").map((cp, pointer) => {
        return inRange(pointer, 8272, 8835) ? null : cp;
      });
      const index_ = shift_jis_index;
      return index_.indexOf(code_point);
    }
    var shift_jis_index;
    function indexBig5PointerFor(code_point) {
      big5_index_no_hkscs = big5_index_no_hkscs || index("big5").map((cp, pointer) => {
        return pointer < (161 - 129) * 157 ? null : cp;
      });
      var index_ = big5_index_no_hkscs;
      if (code_point === 9552 || code_point === 9566 || code_point === 9569 || code_point === 9578 || code_point === 21313 || code_point === 21317) {
        return index_.lastIndexOf(code_point);
      }
      return indexPointerFor(code_point, index_);
    }
    var big5_index_no_hkscs;
    module.exports = index;
    module.exports.indexCodePointFor = indexCodePointFor;
    module.exports.indexPointerFor = indexPointerFor;
    module.exports.indexGB18030RangesCodePointFor = indexGB18030RangesCodePointFor;
    module.exports.indexGB18030RangesPointerFor = indexGB18030RangesPointerFor;
    module.exports.indexShiftJISPointerFor = indexShiftJISPointerFor;
    module.exports.indexBig5PointerFor = indexBig5PointerFor;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/table.js
const require_table = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/table.js"(exports, module) {
    var Encodings = require_encodings();
    var { UTF8Decoder, UTF8Encoder } = require_utf8();
    var { UTF16Decoder, UTF16Encoder } = require_utf16();
    var { SingleByteDecoder, SingleByteEncoder } = require_single_byte();
    var index = require_indexes();
    var label_to_encoding = {};
    Encodings.forEach(({ encodings }) => {
      encodings.forEach((encoding) => {
        encoding.labels.forEach((label) => {
          label_to_encoding[label] = encoding;
        });
      });
    });
    var encoders = {
      "UTF-8"() {
        return new UTF8Encoder();
      },
      "UTF-16BE"() {
        return new UTF16Encoder(true);
      },
      "UTF-16LE"() {
        return new UTF16Encoder();
      },
    };
    var decoders = {
      "UTF-8"(options) {
        return new UTF8Decoder(options);
      },
      "UTF-16BE"(options) {
        return new UTF16Decoder(true, options);
      },
      "UTF-16LE"(options) {
        return new UTF16Decoder(false, options);
      },
    };
    Encodings.forEach(({ heading, encodings }) => {
      if (heading != "Legacy single-byte encodings")
        return;
      encodings.forEach((encoding) => {
        const name = encoding.name;
        const idx = index(name.toLowerCase());
        decoders[name] = (options) => {
          return new SingleByteDecoder(idx, options);
        };
        encoders[name] = (options) => {
          return new SingleByteEncoder(idx, options);
        };
      });
    });
    module.exports.label_to_encoding = label_to_encoding;
    module.exports.encoders = encoders;
    module.exports.decoders = decoders;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/lib/index.js
const require_lib = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/lib/index.js"(exports, module) {
    var { end_of_stream } = require_utils();
    var { label_to_encoding } = require_table();
    var Stream = class {
      /**
       * A stream represents an ordered sequence of tokens.
       * @param {!(Array.<number>|Uint8Array)} tokens Array of tokens that provide
       * the stream.
       */
      constructor(tokens) {
        this.tokens = [...tokens];
        this.tokens.reverse();
      }
      /**
       * @returns True if end-of-stream has been hit.
       */
      endOfStream() {
        return !this.tokens.length;
      }
      /**
       * When a token is read from a stream, the first token in the
       * stream must be returned and subsequently removed, and
       * end-of-stream must be returned otherwise.
       *
       * @return Get the next token from the stream, or end_of_stream.
       */
      read() {
        if (!this.tokens.length)
          return end_of_stream;
        return this.tokens.pop();
      }
      /**
       * When one or more tokens are prepended to a stream, those tokens
       * must be inserted, in given order, before the first token in the
       * stream.
       *
       * @param {(number|!Array.<number>)} token The token(s) to prepend to the
       * stream.
       */
      prepend(token) {
        if (Array.isArray(token)) {
          var tokens = (
            /**@type {!Array.<number>}*/
            token
          );
          while (tokens.length)
            this.tokens.push(tokens.pop());
        } else {
          this.tokens.push(token);
        }
      }
      /**
       * When one or more tokens are pushed to a stream, those tokens
       * must be inserted, in given order, after the last token in the
       * stream.
       *
       * @param {(number|!Array.<number>)} token The tokens(s) to push to the
       * stream.
       */
      push(token) {
        if (Array.isArray(token)) {
          const tokens = (
            /**@type {!Array.<number>}*/
            token
          );
          while (tokens.length)
            this.tokens.unshift(tokens.shift());
        } else {
          this.tokens.unshift(token);
        }
      }
    };
    var DEFAULT_ENCODING = "utf-8";
    function getEncoding(label) {
      label = String(label).trim().toLowerCase();
      if (Object.prototype.hasOwnProperty.call(label_to_encoding, label)) {
        return label_to_encoding[label];
      }
      return null;
    }
    module.exports = Stream;
    module.exports.DEFAULT_ENCODING = DEFAULT_ENCODING;
    module.exports.getEncoding = getEncoding;
  }
});

// node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/lib/TextDecoder.js
const require_TextDecoder = __commonJS({
  "node_modules/.aspect_rules_js/text-decoding@1.0.0/node_modules/text-decoding/build/lib/TextDecoder.js"(exports, module) {
    var Stream = require_lib();
    var { DEFAULT_ENCODING, getEncoding } = Stream;
    var { end_of_stream, finished, codePointsToString } = require_utils();
    var { decoders } = require_table();
    var TextDecoder = class {
      /**
       * @param {string=} label The label of the encoding; defaults to 'utf-8'.
       * @param {Object=} options
       */
      constructor(label = DEFAULT_ENCODING, options = {}) {
        this._encoding = null;
        this._decoder = null;
        this._ignoreBOM = false;
        this._BOMseen = false;
        this._error_mode = "replacement";
        this._do_not_flush = false;
        const encoding = getEncoding(label);
        if (encoding === null || encoding.name == "replacement")
          throw RangeError("Unknown encoding: " + label);
        if (!decoders[encoding.name]) {
          throw Error("Decoder not present. Did you forget to include encoding-indexes.js first?");
        }
        this._encoding = encoding;
        if (options["fatal"])
          this._error_mode = "fatal";
        if (options["ignoreBOM"])
          this._ignoreBOM = true;
      }
      get encoding() {
        return this._encoding.name.toLowerCase();
      }
      get fatal() {
        return this._error_mode === "fatal";
      }
      get ignoreBOM() {
        return this._ignoreBOM;
      }
      /**
       * @param {BufferSource=} input The buffer of bytes to decode.
       * @param {Object=} options
       * @return The decoded string.
       */
      decode(input, options = {}) {
        let bytes;
        if (typeof input === "object" && input instanceof ArrayBuffer) {
          bytes = new Uint8Array(input);
        } else if (typeof input === "object" && "buffer" in input && input.buffer instanceof ArrayBuffer) {
          bytes = new Uint8Array(
            input.buffer,
            input.byteOffset,
            input.byteLength
          );
        } else {
          bytes = new Uint8Array(0);
        }
        if (!this._do_not_flush) {
          this._decoder = decoders[this._encoding.name]({
            fatal: this._error_mode === "fatal"
          });
          this._BOMseen = false;
        }
        this._do_not_flush = Boolean(options["stream"]);
        const input_stream = new Stream(bytes);
        const output = [];
        let result;
        while (true) {
          const token = input_stream.read();
          if (token === end_of_stream)
            break;
          result = this._decoder.handler(input_stream, token);
          if (result === finished)
            break;
          if (result !== null) {
            if (Array.isArray(result))
              output.push.apply(
                output,
                /**@type {!Array.<number>}*/
                result
              );
            else
              output.push(result);
          }
        }
        if (!this._do_not_flush) {
          do {
            result = this._decoder.handler(input_stream, input_stream.read());
            if (result === finished)
              break;
            if (result === null)
              continue;
            if (Array.isArray(result))
              output.push.apply(
                output,
                /**@type {!Array.<number>}*/
                result
              );
            else
              output.push(result);
          } while (!input_stream.endOfStream());
          this._decoder = null;
        }
        return this.serializeStream(output);
      }
      // A TextDecoder object also has an associated serialize stream
      // algorithm...
      /**
       * @param {!Array.<number>} stream
       */
      serializeStream(stream) {
        if (["UTF-8", "UTF-16LE", "UTF-16BE"].includes(this._encoding.name) && !this._ignoreBOM && !this._BOMseen) {
          if (stream.length > 0 && stream[0] === 65279) {
            this._BOMseen = true;
            stream.shift();
          } else if (stream.length > 0) {
            this._BOMseen = true;
          } else {
          }
        }
        return codePointsToString(stream);
      }
    };
    module.exports = TextDecoder;
  }
});

// src/roma/benchmark/text_decoder.js
const Text_Decoder = __toESM(require_TextDecoder());
const TextDecoder = require_TextDecoder();
)JS_CODE";

inline constexpr std::string_view kUtilJs = R"JS_CODE(

const KVStr = function(obj) {
  if (obj == null) {
    return "null";
  }
  return "{key: " + obj.key + ", val: " + obj.val + "}";
};

const FB_KVStruct = class {
  constructor(kvdata) {
    this._kvdata = kvdata;
  }

  Length() {
    return this._kvdata.kvLength();
  }

  KeyAt(offset) {
    return this._kvdata.kv(offset).key();
  }

  ValueAt(offset) {
    return this._kvdata.kv(offset).val();
  }

  GetOffsetKVUnchecked(offset) {
    return this._kvdata.kv(offset);
  }

  GetOffsetKV(offset) {
    return (offset >= 0 && offset < this._kvdata.kvLength())
      ? this.GetOffsetKVUnchecked(offset)
      : null;
  }

  OffsetLookup(offset) {
    const v = this.GetOffsetKV(offset);
    if (v !== null) {
      return {key: v.key(), val: v.val(), num_lookups: 1};
    }
    return {key: null, val: null, num_lookups: 0};
   }

  BinarySearch(lookup) {
    let left = 0;
    let right = this._kvdata.kvLength() - 1;
    let num_lookups = 0;
    const FindMid = function(l, r) {
      num_lookups++;
      return l + Math.floor((r - l) / 2);
    };
    while (left <= right) {
      const mid = FindMid(left, right);
      const mid_key = this.KeyAt(mid);
      if (mid_key === lookup) {
        return {val: this.ValueAt(mid), num_lookups: num_lookups + 1};
      }
      if (mid_key < lookup) {
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }
    return {val: null, num_lookups};
  }
};

const FB_KVParallelArrays = class {
  constructor(kvdata) {
    this._kvdata = kvdata;
  }

  Length() {
    return this._kvdata.keysLength();
  }

  KeyAt(offset) {
    return this._kvdata.keys(offset);
  }

  ValueAt(offset) {
    return this._kvdata.vals(offset);
  }

  OffsetLookup(offset) {
    if (!(offset >= 0 && offset < this._kvdata.keysLength())) {
      return null;
    }
    const k = this.KeyAt(offset);
    if (k !== null) {
      return {key: k, val: this.ValueAt(offset), num_lookups: 2};
    }
    return {key: null, val: null, num_lookups: 0};
  }

  BinarySearch(lookup) {
    let left = 0;
    let right = this._kvdata.keysLength() - 1;
    let num_lookups = 0;
    const FindMid = function(l, r) {
      num_lookups++;
      return l + Math.floor((r - l) / 2);
    };
    while (left <= right) {
      const mid = FindMid(left, right);
      const mid_key = this.KeyAt(mid);
      if (mid_key === lookup) {
        return {val: this.ValueAt(mid), num_lookups: num_lookups + 1};
      }
      if (mid_key < lookup) {
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }
    return {val: null, num_lookups};
  }
};

const CreateFbufferKVStruct = function(data_layout, array_buf, buf_length) {
  switch (data_layout) {
    case "KVStrData":
      return new FB_KVStruct(data_structures.privacysandbox_roma_benchmarks.KVStrData.getRootAsKVStrData(
        new flatbuffers.ByteBuffer(new Uint8Array(array_buf, 0, buf_length))
      ));
    case "KVKeyData":
      return new FB_KVStruct(data_structures.privacysandbox_roma_benchmarks.KVKeyData.getRootAsKVKeyData(
        new flatbuffers.ByteBuffer(new Uint8Array(array_buf, 0, buf_length))
      ));
    case "KVDataParallel":
      return new FB_KVParallelArrays(data_structures.privacysandbox_roma_benchmarks.KVDataParallel.getRootAsKVDataParallel(
        new flatbuffers.ByteBuffer(new Uint8Array(array_buf, 0, buf_length))
      ));
  }
  return null;
}

)JS_CODE";

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_
