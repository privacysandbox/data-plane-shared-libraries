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

// This file has code that will be executed as part of benchmarking for both
// KV and BA servers.

#ifndef ROMA_BENCHMARK_TEST_CODE_H_
#define ROMA_BENCHMARK_TEST_CODE_H_

#include <string_view>

namespace google::scp::roma::benchmark {

constexpr std::string_view kCodeHelloWorld = "hello = () => 'Hello world!';";
constexpr std::string_view kHandlerNameHelloWorld = "hello";

// This JS function is deliberately something that's expensive to run.  The
// code is based on:
// https://www.tutorialspoint.com/using-sieve-of-eratosthenes-to-find-primes-javascript
constexpr std::string_view kCodePrimeSieve = R"(
  function sieve() {
    // Find all prime numbers less than this:
    const n = 100000;
    // Create a boolean array of size n+1
    const primes = new Array(n + 1).fill(true);
    // Set first two values to false
    primes[0] = false;
    primes[1] = false;
    // Loop through the elements
    for (let i = 2; i <= Math.sqrt(n); i++) {
       if (primes[i]) {
          for (let j = i * i; j <= n; j += i) {
             primes[j] = false;
          }
       }
    }

    const result = [];
    // Loop through the array from 2 to n
    for (let i = 2; i <= n; i++) {
       if (primes[i]) {
          result.push(i);
       }
    }

    return result;
  }
)";
constexpr std::string_view kHandlerNamePrimeSieve = "sieve";

constexpr std::string_view kCodeSerializeProtobuf = R"(
    function SerializeFunc(req) {
      const start_time = performance.now();
      BenchmarkServerPb.ObjectToProtoBytes_BenchmarkRequest(req);
      const end_time = performance.now();
      return end_time - start_time;
    };
)";
constexpr std::string_view kCodeSerializeJson = R"(
    function SerializeFunc(req) {
      const start_time = performance.now();
      JSON.stringify(req);
      const end_time = performance.now();
      return end_time - start_time;
    };
)";
constexpr std::string_view kHandlerNameSerializeFunc = "SerializeFunc";

constexpr std::string_view kCodeDeserializeProtobuf = R"(
    function DeserializeFunc(req) {
      const serializedRequest = BenchmarkServerPb.ObjectToProtoBytes_BenchmarkRequest(req);

      const start_time = performance.now();
      BenchmarkServerPb.ProtoBytesToObject_BenchmarkRequest(serializedRequest);
      const end_time = performance.now();
      return end_time - start_time;
    };
)";
constexpr std::string_view kCodeDeserializeJson = R"(
    function DeserializeFunc(req) {
      const serializedRequest = JSON.stringify(req);

      const start_time = performance.now();
      JSON.parse(serializedRequest);
      const end_time = performance.now();
      return end_time - start_time;
    };
)";
constexpr std::string_view kHandlerNameDeserializeFunc = "DeserializeFunc";

// This code was fetched from this publicly available URL on 2023-10-16:
// https://storage.googleapis.com/buyer-bas-dev/generateBid.js
constexpr std::string_view kCodeGoogleAdManagerGenerateBid = R"(
(function(stc){/*

 Copyright The Closure Library Authors.
 SPDX-License-Identifier: Apache-2.0
*/
var ea={},fa=this||self;function ha(a,b){return Error(`Invalid wire type: ${a} (at position ${b})`)}function ia(){return Error("Failed to read varint, encoding is invalid.")}function ja(a,b){return Error(`Tried to read past the end of the data ${b} > ${a}`)};var ka,la;a:{for(var ma=["CLOSURE_FLAGS"],na=fa,oa=0;oa<ma.length;oa++)if(na=na[ma[oa]],null==na){la=null;break a}la=na}var pa=la&&la[610401301];ka=null!=pa?pa:!1;var ra;const ua=fa.navigator;ra=ua?ua.userAgentData||null:null;function va(a){return ka?ra?ra.brands.some(({brand:b})=>b&&-1!=b.indexOf(a)):!1:!1}function n(a){var b;a:{if(b=fa.navigator)if(b=b.userAgent)break a;b=""}return-1!=b.indexOf(a)};function wa(){return ka?!!ra&&0<ra.brands.length:!1}function xa(){return wa()?va("Chromium"):(n("Chrome")||n("CriOS"))&&!(wa()?0:n("Edge"))||n("Silk")};!n("Android")||xa();xa();n("Safari")&&(xa()||(wa()?0:n("Coast"))||(wa()?0:n("Opera"))||(wa()?0:n("Edge"))||(wa()?va("Microsoft Edge"):n("Edg/"))||wa()&&va("Opera"));var ya={},za=null,Ba=function(a){var b=[];Aa(a,function(c){b.push(c)});return b},Aa=function(a,b){function c(k){for(;d<a.length;){var m=a.charAt(d++),r=za[m];if(null!=r)return r;if(!/^[\s\xa0]*$/.test(m))throw Error("Unknown base64 encoding at char: "+m);}return k}Ca();for(var d=0;;){var e=c(-1),f=c(0),h=c(64),g=c(64);if(64===g&&-1===e)break;b(e<<2|f>>4);64!=h&&(b(f<<4&240|h>>2),64!=g&&b(h<<6&192|g))}},Ca=function(){if(!za){za={};for(var a="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789".split(""),b=["+/=","+/","-_=","-_.","-_"],c=0;5>c;c++){var d=a.concat(b[c].split(""));ya[c]=d;for(var e=0;e<d.length;e++){var f=d[e];void 0===za[f]&&(za[f]=e)}}}};function Da(a){let b="",c=0;const d=a.length-10240;for(;c<d;)b+=String.fromCharCode.apply(null,a.subarray(c,c+=10240));b+=String.fromCharCode.apply(null,c?a.subarray(c):a);return btoa(b)}const Ea=/[-_.]/g,Fa={"-":"+",_:"/",".":"="};function Ha(a){return Fa[a]||""}function Ia(a){Ea.test(a)&&(a=a.replace(Ea,Ha));a=atob(a);const b=new Uint8Array(a.length);for(let c=0;c<a.length;c++)b[c]=a.charCodeAt(c);return b}function Ja(a){return null!=a&&a instanceof Uint8Array}let Na;var Oa={};let Pa;function Qa(a){if(a!==Oa)throw Error("illegal external caller");}var Sa=function(){return Pa||(Pa=new Ra(null,Oa))},Ta=function(a){const b=a.g;return null==b?"":"string"===typeof b?b:a.g=Da(b)},Ra=class{constructor(a,b){Qa(b);this.g=a;if(null!=a&&0===a.length)throw Error("ByteString should be constructed with non-empty values");}};function Ua(a){return 0==a.length?Sa():new Ra(a,Oa)};function Va(a){if("string"===typeof a)return{buffer:Ia(a),K:!1};if(Array.isArray(a))return{buffer:new Uint8Array(a),K:!1};if(a.constructor===Uint8Array)return{buffer:a,K:!1};if(a.constructor===ArrayBuffer)return{buffer:new Uint8Array(a),K:!1};if(a.constructor===Ra){Qa(Oa);var b=a.g;b=null==b||Ja(b)?b:"string"===typeof b?Ia(b):null;return{buffer:(null==b?b:a.g=b)||Na||(Na=new Uint8Array(0)),K:!0}}if(a instanceof Uint8Array)return{buffer:new Uint8Array(a.buffer,a.byteOffset,a.byteLength),K:!1};throw Error("Type not convertible to a Uint8Array, expected a Uint8Array, an ArrayBuffer, a base64 encoded string, a ByteString or an Array of numbers");};var Wa=function(a,b,{ma:c=!1}={}){a.ma=c;b&&(b=Va(b),a.h=b.buffer,a.A=b.K,a.l=0,a.i=a.h.length,a.g=a.l)},Xa=function(a,b){a.g=b;if(b>a.i)throw ja(a.i,b);},Ya=function(a){const b=a.h;let c=a.g,d=b[c++],e=d&127;if(d&128&&(d=b[c++],e|=(d&127)<<7,d&128&&(d=b[c++],e|=(d&127)<<14,d&128&&(d=b[c++],e|=(d&127)<<21,d&128&&(d=b[c++],e|=d<<28,d&128&&b[c++]&128&&b[c++]&128&&b[c++]&128&&b[c++]&128&&b[c++]&128)))))throw ia();Xa(a,c);return e},Za=class{constructor(a,b){this.h=null;this.A=!1;this.g=this.i=this.l=0;Wa(this,a,b)}},$a=[];var ab=function(a,{ra:b=!1}={}){a.ra=b},bb=function(a){var b=a.g;if(b.g==b.i)return!1;a.i=a.g.g;var c=Ya(a.g)>>>0;b=c>>>3;c&=7;if(!(0<=c&&5>=c))throw ha(c,a.i);if(1>b)throw Error(`Invalid field number: ${b} (at position ${a.i})`);a.A=b;a.h=c;return!0},cb=function(a){switch(a.h){case 0:if(0!=a.h)cb(a);else a:{a=a.g;var b=a.g;const c=b+10,d=a.h;for(;b<c;)if(0===(d[b++]&128)){Xa(a,b);break a}throw ia();}break;case 1:a=a.g;Xa(a,a.g+8);break;case 2:2!=a.h?cb(a):(b=Ya(a.g)>>>0,a=a.g,Xa(a,a.g+b));break;case 5:a=a.g;Xa(a,a.g+4);break;case 3:b=a.A;do{if(!bb(a))throw Error("Unmatched start-group tag: stream EOF");if(4==a.h){if(a.A!=b)throw Error("Unmatched end-group tag");break}cb(a)}while(1);break;default:throw ha(a.h,a.i);}},db=class{constructor(a,b){if($a.length){const c=$a.pop();Wa(c,a,b);a=c}else a=new Za(a,b);this.g=a;this.i=this.g.g;this.h=this.A=-1;ab(this,b)}},eb=[];class fb{constructor(a){this.g=a}};const q=Symbol();function t(a,b){q?a[q]|=b:void 0!==a.g?a.g|=b:Object.defineProperties(a,{g:{value:b,configurable:!0,writable:!0,enumerable:!1}})}function gb(a){const b=u(a);1!==(b&1)&&(Object.isFrozen(a)&&(a=Array.prototype.slice.call(a)),x(a,b|1))}function hb(a,b){q?a[q]&&(a[q]&=~b):void 0!==a.g&&(a.g&=~b)}function u(a){let b;q?b=a[q]:b=a.g;return b|0}function x(a,b){q?a[q]=b:void 0!==a.g?a.g=b:Object.defineProperties(a,{g:{value:b,configurable:!0,writable:!0,enumerable:!1}});return a}function ib(a,b){Object.isFrozen(a)&&(a=Array.prototype.slice.call(a));x(a,b);return a}function jb(a){t(a,1);return a}function kb(a){t(a,16);return a}function lb(a,b){x(b,(a|0)&-51)}function mb(a,b){x(b,(a|18)&-41)};var nb={};function ob(a){return null!==a&&"object"===typeof a&&!Array.isArray(a)&&a.constructor===Object}function pb(a,b){if(null!=a)if("string"===typeof a)a=a?new Ra(a,Oa):Sa();else if(a.constructor!==Ra){var c;Ja(a)?c=b?Ua(a):a.length?new Ra(new Uint8Array(a),Oa):Sa():c=void 0;a=c}return a}var qb=Object.freeze(x([],23));function rb(a){if(u(a.j)&2)throw Error();};function sb(a){if("number"!==typeof a)throw Error(`Value of float/double field must be a number, found ${typeof a}: ${a}`);return a}function tb(a){if(null==a)return a;if("number"===typeof a||"NaN"===a||"Infinity"===a||"-Infinity"===a)return Number(a)}function ub(a){if(null==a)return a;switch(typeof a){case "string":return+a;case "number":return a}}function vb(a){if(null==a)return a;switch(typeof a){case "string":return+a;case "number":return a}}function wb(a){return a}function xb(a){return null==a?a:a}function yb(a){return a}function zb(a){return a}function Ab(a,b,c){var d=!1;if(null!=a&&"object"===typeof a&&!(d=Array.isArray(a))&&a.V===nb)return a;if(d){var e=d=u(a);0===e&&(e|=c&16);e|=c&2;e!==d&&x(a,e);return new b(a)}}const Bb=Symbol();var z=function(a,b,c){return-1===b?null:b>=a.O?a.A?a.A[b]:void 0:c&&a.A&&(c=a.A[b],null!=c)?c:a.j[b+-1]},A=function(a,b,c,d){a.pa&&(a.pa=void 0);if(b>=a.O||d)return d=a.O+-1,(a.A||(a.A=a.j[d]={}))[b]=c,a;a.j[b+-1]=c;(c=a.A)&&b in c&&delete c[b];return a};function Cb(a,b,c,d){let e=z(a,b);Array.isArray(e)||(e=qb);const f=u(e);f&1||jb(e);if(d)f&2||t(e,18),c&1||Object.freeze(e);else{d=!(c&2);const h=f&2;c&1||!h?d&&f&16&&!h&&hb(e,16):(e=jb(Array.prototype.slice.call(e)),A(a,b,e))}return e}function B(a,b,c){const d=!!(u(a.j)&2);let e=Cb(a,b,1,d),f=u(e);if(!(f&4)){Object.isFrozen(e)&&(e=jb(e.slice()),A(a,b,e));let h=0,g=0;for(;h<e.length;h++){const k=c(e[h]);null!=k&&(e[g++]=k)}g<h&&(e.length=g);f|=5;d&&(f|=18);x(e,f);f&2&&Object.freeze(e)}!d&&(f&2||Object.isFrozen(e))&&(e=Array.prototype.slice.call(e),t(e,5),A(a,b,e));return e}var C=function(a,b){return B(a,b,tb)};function Db(a){return pb(a,!0)}function Eb(a){return pb(a,!1)}function Fb(a,b,c,d){if(null==c)return rb(a),A(a,b);const e=u(c);if(!(e&4)){if(e&2||Object.isFrozen(c))c=Array.prototype.slice.call(c);for(let f=0;f<c.length;f++)c[f]=d(c[f]);x(c,e|5)}rb(a);return A(a,b,c)}function E(a,b,c,d){rb(a);c!==d?A(a,b,c):A(a,b,void 0,!1);return a}var F=function(a,b,c){if(null!=c&&"number"!==typeof c)throw Error(`Value of float/double field must be a number|null|undefined, found ${typeof c}: ${c}`);return E(a,b,c,0)},Nb=function(){var a=Gb,b=Hb;let c=0;for(let d=0;d<b.length;d++){const e=b[d];null!=z(a,e)&&(0!==c&&A(a,c,void 0,!1),c=e)}return c},Ob=function(a,b,c){const d=z(a,c,!1);b=Ab(d,b,u(a.j));b!==d&&null!=b&&A(a,c,b,!1);return b},G=function(a,b,c){(a=Ob(a,b,c))?b=a:(a=b[Bb])?b=a:(a=new b,t(a.j,18),b=b[Bb]=a);return b},H=function(a,b,c){b=Ob(a,b,c);if(null==b)return b;if(!(u(a.j)&2)){const d=Pb(b);d!==b&&(b=d,A(a,c,b,!1))}return b},Rb=function(a,b,c){return Qb(a,b,c,1,u(a.j))};function Qb(a,b,c,d,e){var f=!!(e&2),h=Cb(a,c,1,f);if(h===qb||!(u(h)&4)){var g=h;h=!!(e&2);var k=!!(u(g)&2);f=g;!h&&k&&(g=Array.prototype.slice.call(g));var m=e|(k?2:0);e=k||void 0;let r=k=0;for(;k<g.length;k++){const y=Ab(g[k],b,m);void 0!==y&&(e=e||!!(2&u(y.j)),g[r++]=y)}r<k&&(g.length=r);b=g;g=u(b);m=g|5;e=e?m&-9:m|8;g!=e&&(b=ib(b,e));g=b;f!==g&&A(a,c,g);(h&&2!==d||1===d)&&Object.freeze(g);return g}if(3===d)return h;f||(f=Object.isFrozen(h),1===d?f||Object.freeze(h):(d=u(h),b=d&-19,f&&(h=Array.prototype.slice.call(h),d=0,A(a,c,h)),d!==b&&x(h,b)));return h}var I=function(a,b,c){const d=u(a.j);var e=!!(d&2);a=Qb(a,b,c,e?1:2,d);if(!(e||u(a)&8)){for(e=0;e<a.length;e++)b=a[e],c=Pb(b),b!==c&&(a[e]=c);t(a,8)}return a},J=function(a,b,c){rb(a);null==c&&(c=void 0);return A(a,b,c)};function Sb(a,b,c,d){const e=u(a.j);if(e&2)throw Error();a=Qb(a,c,b,2,e);c=null!=d?d:new c;a.push(c);c.K()&&hb(a,8);return c}function L(a,b){return null==a?b:a}var M=function(a,b){a=z(a,b);return L(null==a?a:!!a,!1)},N=function(a,b){return L(ub(z(a,b)),0)},Tb=function(a,b){return L(vb(z(a,b)),0)},O=function(a,b){return L(z(a,b),"0")},P=function(a,b){const c=z(a,b),d=tb(c);null!=d&&d!==c&&A(a,b,d);return L(d,0)},Q=function(a,b){return L(z(a,b),"")},R=function(a,b){return L(xb(z(a,b)),0)};let Ub;function Vb(a,b){Ub=b;a=new a(b);Ub=void 0;return a};function Wb(a){switch(typeof a){case "number":return isFinite(a)?a:String(a);case "boolean":return a?1:0;case "object":if(a&&!Array.isArray(a)){if(Ja(a))return Da(a);if(a instanceof Ra)return Ta(a)}}return a};function Xb(a,b){const c=Array.prototype.slice.call(a.j),d=a.A;var e=c.length+(d?-1:0);let f=0;for(;f<e;f++)c[f]=b(c[f]);if(d){e=c[f]={};for(const h in d)Object.prototype.hasOwnProperty.call(d,h)&&(e[h]=b(d[h]))}b=Vb(a.constructor,kb(c));a.ha&&(b.ha=a.ha.slice());return b}function Yb(a,b,c,d,e,f){if(null!=a){if(Array.isArray(a))a=e&&0==a.length&&u(a)&1?void 0:f&&u(a)&2?a:Zb(a,b,c,void 0!==d,e,f);else if(ob(a)){const h={};for(let g in a)Object.prototype.hasOwnProperty.call(a,g)&&(h[g]=Yb(a[g],b,c,d,e,f));a=h}else a=b(a,d);return a}}function Zb(a,b,c,d,e,f){const h=d||c?u(a):0;d=d?!!(h&16):void 0;a=Array.prototype.slice.call(a);for(let g=0;g<a.length;g++)a[g]=Yb(a[g],b,c,d,e,f);c&&c(h,a);return a}function $b(a){return a.V===nb?a.toJSON():Wb(a)};function ac(a,b,c=mb){if(null!=a){if(a instanceof Uint8Array)return b?a:new Uint8Array(a);if(Array.isArray(a)){const d=u(a);if(d&2)return a;if(b&&!(d&32)&&(d&16||0===d))return x(a,d|18),a;a=Zb(a,ac,d&4?mb:c,!0,!1,!0);bc(a);return a}a.V===nb&&(u(a.j)&2||(a=cc(a,!0),t(a.j,18)));return a}}function cc(a,b){const c=u(a.j),d=b||c&2?mb:lb,e=!!(c&16);return Xb(a,f=>ac(f,e,d))}function dc(a){ec(a.j);return a}function ec(a){var b=u(a);if(!(b&2))for(x(a,b|2),b=0;b<a.length;b++){var c=a[b];if(c)if(Array.isArray(c))ec(c),bc(c);else if(c.V===nb)dc(c);else if(b===a.length-1&&ob(c))for(let d in c){if(!Object.prototype.hasOwnProperty.call(c,d))continue;const e=c[d];e&&(Array.isArray(e)?(ec(e),bc(e)):e.V===nb&&dc(e))}}}function Pb(a){if(!(u(a.j)&2))return a;const b=cc(a,!1);b.pa=a;return b}function bc(a){const b=u(a);b&4&&b&2&&Object.freeze(a)};var T=class{constructor(a){null==a&&(a=Ub);Ub=void 0;if(null==a)a=[],x(a,48);else{if(!Array.isArray(a))throw Error();t(a,32)}this.j=a;a:{var b=this.j.length;a=b-1;if(b&&(b=this.j[a],ob(b))){this.A=b;this.O=a- -1;break a}this.O=Number.MAX_VALUE}}toJSON(){var a=Zb(this.j,$b,void 0,void 0,!1,!1);return fc(this,a,!0)}K(){return!!(u(this.j)&2)}};T.prototype.V=nb;T.prototype.toString=function(){return fc(this,this.j,!1).toString()};function fc(a,b,c){const d=a?a.constructor.m:void 0;var e=a.O;if(d){if(!c){b=Array.prototype.slice.call(b);var f;if(b.length&&ob(f=b[b.length-1]))for(var h=0;h<d.length;h++)if(d[h]>=e){Object.assign(b[b.length-1]={},f);break}}e=b;c=!c;a=a.O;let k;for(f=0;f<d.length;f++)if(h=d[f],h<a){h+=-1;var g=e[h];null==g?e[h]=c?qb:jb([]):c&&g!==qb&&gb(g)}else{if(!k){let m;e.length&&ob(m=e[e.length-1])?k=m:e.push(k={})}g=k[h];null==k[h]?k[h]=c?qb:jb([]):c&&g!==qb&&gb(g)}}return b}function gc(a,b){if(null==b)return new a;if(!Array.isArray(b))throw Error("must be an array");if(Object.isFrozen(b)||Object.isSealed(b)||!Object.isExtensible(b))throw Error("arrays passed to jspb constructors must be mutable");t(b,64);return Vb(a,kb(b))};const hc=Symbol();function ic(a){let b=a[hc];if(!b){const c=jc(a),d=c.i;b=d?(e,f)=>d(e,f,c):(e,f)=>{for(;bb(f)&&4!=f.h;){var h=f.A,g=c[h];if(!g){var k=c.h;k&&(k=k[h])&&(g=c[h]=kc(k))}if(!g||!g(f,e,h))if(k=f,h=e,g=k.i,cb(k),!k.ra){var m=k.g.g-g;k.g.g=g;k=k.g;g=m;if(0==g)g=Sa();else{if(0>g)throw Error(`Tried to read a negative byte length: ${g}`);m=k.g;var r=m+g;if(r>k.i)throw ja(g,k.i-m);k.g=r;r=m;k.ma&&k.A?g=k.h.subarray(r,r+g):(k=k.h,m=r,g=r+g,g=m===g?Na||(Na=new Uint8Array(0)):k.slice(m,g));g=Ua(g)}(k=h.ha)?k.push(g):h.ha=[g]}}return e};a[hc]=b}return b}function lc(a){if(a=a.za)return ic(a)}function kc(a){const b=lc(a),c=a.bb.g;if(b){const d=jc(a.za).g;return(e,f,h)=>c(e,f,h,d,b)}return(d,e,f)=>c(d,e,f)}function mc(a,b){let c=a[b];"function"==typeof c&&0===c.length&&(c=c(),a[b]=c);return Array.isArray(c)&&(nc in c||oc in c||0<c.length&&"function"==typeof c[0])?c:void 0}const oc=Symbol(),nc=Symbol();function pc(a,b){const c=a.g;return b?(d,e,f)=>c(d,e,f,b):c}function qc(a,b,c){const d=a.g,e=ic(b),f=jc(b).g;return(h,g,k)=>d(h,g,k,f,e,c)}function jc(a){var b=a[nc];if(b)return b;a:{b=a[nc]={};var c=pc,d=qc;b.g=a[0];let g=1;if(a.length>g&&"number"!==typeof a[g]){var e=a[g++];if(Array.isArray(e)){b.i=e[0];b.h=e[1];break a}b.h=e}for(;g<a.length;){e=a[g++];for(var f=g+1;f<a.length&&"number"!==typeof a[f];)f++;const k=a[g++];f-=g;switch(f){case 0:b[e]=c(k);break;case 1:(f=mc(a,g))?(g++,b[e]=d(k,f)):b[e]=c(k,a[g++]);break;case 2:f=b;var h=g++;h=mc(a,h);f[e]=d(k,h,a[g++]);break;default:throw Error("unexpected number of binary field arguments: "+f);}}}nc in a&&oc in a&&(a.length=0);return b}var rc;rc=new fb(function(a,b,c){if(0!==a.h)return!1;{var d=a.g;let f=0,h=a=0;const g=d.h;let k=d.g;do{var e=g[k++];f|=(e&127)<<h;h+=7}while(32>h&&e&128);32<h&&(a|=(e&127)>>4);for(h=3;32>h&&e&128;h+=7)e=g[k++],a|=(e&127)<<h;Xa(d,k);if(128>e){d=f>>>0;e=a>>>0;if(a=e&2147483648)d=~d+1>>>0,e=~e>>>0,0==d&&(e=e+1>>>0);d=4294967296*e+(d>>>0);a=a?-d:d}else throw ia();}A(b,c,a);return!0});var sc;sc=new fb(function(a,b,c){if(5!==a.h)return!1;a=a.g;var d=a.h;const e=a.g,f=d[e],h=d[e+1],g=d[e+2];d=d[e+3];Xa(a,a.g+4);A(b,c,(f<<0|h<<8|g<<16|d<<24)>>>0);return!0});function tc(a,b){const c=uc();if(!b(a))throw b=c?c()+"\n":"",Error(b+String(a));}function vc(a){tc(a,wc);return a}let xc=void 0;function uc(){const a=xc;xc=void 0;return a};function yc(a){return b=>{if(null==b||""==b)b=new a;else{b=JSON.parse(b);if(!Array.isArray(b))throw Error(void 0);b=Vb(a,kb(b))}return b}};function zc(a){return JSON.stringify([a.map(b=>[{[b.La]:b.Ja}])])};function Ac(a,b=`unexpected value ${a}!`){throw Error(b);};var Bc=function(a,...b){var c=encodeURIComponent,d=zc(b);b=[];for(var e=0,f=0;f<d.length;f++){var h=d.charCodeAt(f);255<h&&(b[e++]=h&255,h>>=8);b[e++]=h}d=3;void 0===d&&(d=0);Ca();d=ya[d];e=Array(Math.floor(b.length/3));f=d[64]||"";let g=0;for(h=0;g<b.length-2;g+=3){var k=b[g],m=b[g+1],r=b[g+2],y=d[k>>2];k=d[(k&3)<<4|m>>4];m=d[(m&15)<<2|r>>6];r=d[r&63];e[h++]=y+k+m+r}y=0;r=f;switch(b.length-g){case 2:y=b[g+1],r=d[(y&15)<<2]||f;case 1:b=b[g],e[h]=d[b>>2]+d[(b&3)<<4|y>>4]+r+f}c=c(e.join(""));a.g(`${"https://pagead2.googlesyndication.com/pagead/ping"}?e=${3}&d=${c}`)},Cc=class{constructor(a){this.g=a}};var Dc=function(a){return(b,c)=>{a:{if(eb.length){const f=eb.pop();ab(f,c);Wa(f.g,b,c);b=f}else b=new db(b,c);try{var d=jc(a).g;var e=ic(a)(new d,b);break a}finally{d=b,b=d.g,b.h=null,b.A=!1,b.l=0,b.i=0,b.g=0,b.ma=!1,d.A=-1,d.h=-1,100>eb.length&&eb.push(d)}e=void 0}return e}}([class extends T{},1,rc,2,sc,3,sc]);var Ec=class extends T{};var Fc=class extends T{constructor(){super()}};var Gc=class extends T{constructor(){super()}};Gc.m=[6];var Hc=class extends T{};Hc.m=[2,3,4];var Ic=class extends T{};Ic.m=[1];var Jc=class extends T{};Jc.m=[2];var Kc=class extends T{};Kc.m=[2];var Lc=class extends T{i(){return I(this,Kc,1)}g(){return I(this,Kc,3)}h(){return P(this,6)}l(){return R(this,9)}};Lc.m=[1,3,10,7,8];var Mc=class extends T{};Mc.m=[3];var Nc=class extends T{};var Oc=class extends T{};Oc.m=[2,3,5];var Pc=class extends T{};Pc.m=[3,4,9];var Qc=class extends T{};Qc.m=[3];var Rc=class extends T{};Rc.m=[3,4,5,6,7];var Sc=class extends T{};Sc.m=[1,2];var Tc=class extends T{constructor(){super()}};Tc.m=[15];var Uc=class extends T{};var Vc=class extends T{};Vc.m=[1];var Wc=class extends T{};var Xc=class extends T{};var Yc=class extends T{g(){return H(this,Lc,4)}h(){return H(this,Sc,5)}B(){return B(this,14,yb)}C(){return B(this,15,yb)}l(){return M(this,16)}i(){return I(this,Xc,17)}};Yc.m=[9,10,11,12,13,14,15,17];var Zc=class extends T{};Zc.m=[1];var $c=class extends T{};var ad=class extends T{h(){return I(this,Kc,1)}i(){return I(this,Kc,2)}g(){return I(this,Kc,3)}};ad.m=[1,2,3];var bd=class extends T{h(){return Q(this,1)}l(){return O(this,2)}g(){return O(this,3)}B(){return P(this,5)}C(){return O(this,6)}i(){return M(this,7)}};var cd=class extends T{g(){return P(this,1)}};var dd=class extends T{i(){return P(this,1)}l(){return P(this,2)}h(){return P(this,3)}C(){return P(this,4)}B(){return P(this,9)}g(){return H(this,cd,5)}D(){return P(this,6)}P(){return P(this,7)}L(){return P(this,8)}};var ed=class extends T{g(){return B(this,1,xb)}h(){return P(this,2)}i(){return C(this,3)}};ed.m=[1,3];var td=class extends T{};var ud=class extends T{g(){return M(this,1)}ka(){return M(this,2)}L(){return H(this,dd,3)}i(){return H(this,dd,4)}P(){return H(this,ed,11)}h(){return M(this,5)}ja(){return M(this,6)}l(){return M(this,7)}la(){return M(this,8)}xa(){return N(this,9)}wa(){return M(this,10)}C(){return M(this,14)}D(){return M(this,15)}B(){return M(this,16)}};var vd=class extends T{h(){return H(this,ud,3)}g(){return G(this,ud,3)}l(){return Q(this,4)}P(){return H(this,Sc,5)}ka(){return H(this,dd,6)}ja(){return H(this,dd,7)}L(){return H(this,ed,18)}D(){return P(this,8)}C(){return M(this,11)}i(){return H(this,bd,16)}la(){return G(this,bd,16)}B(){return N(this,17)}};vd.m=[2];var wd=class extends T{constructor(){super()}};wd.m=[10];var xd=class extends T{g(){return O(this,2)}};var yd=class extends T{};var zd=class extends T{};zd.m=[1];var Ad=class extends T{};var Bd=class extends T{l(){return Q(this,2)}B(){return Q(this,6)}C(){return M(this,4)}h(){return H(this,xd,5)}g(){return Q(this,11)}i(){return Q(this,19)}D(){return M(this,21)}},Cd=yc(Bd);function Dd(a,...b){Bc(a,...b.map(c=>({La:9,Ja:c.toJSON()})))};var wc=a=>Array.isArray(a);var Ed=function(a){return(b,c)=>dc(a(b,c))}(Dc);function Fd(a,b){return!b||0>=b?a:Math.min(a,b)}function V(a,b,c){return b?b:c?c:a?1:0}function Gd(a,b){return a&&0<a.length?a:b&&0<b.length?b:[]}function Hd(a){a=a?.l();return void 0===a?!1:[61,51,52].includes(a)};function Id(a,b){if(!b||0>=b)return{o:0,F:2};var c=a?.L(),d=a?.h()?.P(),e=a?.ja();if(!c&&!d){var f=a?.h()?.i();a=V(!0,e?.i(),f?.i());var h=V(!1,e?.l(),f?.l());c=V(!1,e?.h(),f?.h());e=V(!1,e?.g()?.g(),f?.g()?.g());f=new dd;d=F(f,1,a);d=F(d,2,h);d=F(d,3,c);var g=new cd;g=F(g,1,e);J(d,5,g);return{o:b*a*(1-1/(1+Math.exp(-h*(Math.log(b/1E6)-e-c)))),F:1,N:4,qa:f}}if(c||d){a=V(!1,c?.h(),d?.h());h=Gd(c?.i(),d?.i());c=Gd(c?.g(),d?.g());d=[];for(f of c)switch(f){case 1:d.push(1E-6*b);break;case 2:g=e?.g()?.g(),d.push("number"===typeof g?Math.exp(g):0)}e=Jd(h,d);1===e.F?(b=new ed,b=Fb(b,3,h,sb),b=Fb(b,1,c,wb),b=F(b,2,a),e.qa=b,b=e):b=0>=a||1<a?e:{o:b*a,F:e.F,N:9}}else b={o:0,F:3};return b}function Jd(a,b){if(0===a.length||0>a[0])return{o:0,F:5};const c=b.length;if(a.length!==1+2*(1+c))return{o:0,F:6};const d=c+2;let e=a[1],f=a[d];for(let h=0;h<c;h++){const g=1+h;if(0>=b[h]){if(1E-9>Math.abs(a[1+g])&&1E-9>Math.abs(a[d+g]))continue;return{o:0,F:4}}const k=Math.log(b[h]);e+=a[1+g]*k;f+=a[d+g]*k}return{o:1E9*Math.exp(-.5*(-(e+f)+Math.sqrt((e-f)*(e-f)+4*a[0]))),F:1,N:8}};function Kd(a,b){var c=a?.ka();const d=a?.h()?.L();if(!b||0>=b)return{o:0,N:1};if(1===a?.B())return{o:b,N:2};if(!a?.h()?.ka())return{o:.85*b,N:2};var e=V(!0,c?.C(),d?.C());const f=V(!0,c?.i(),d?.i()),h=V(!1,c?.l(),d?.l()),g=V(!1,c?.h(),d?.h()),k=V(!1,c?.g()?.g(),d?.g()?.g());var m=V(!1,c?.B(),d?.B());const r=new dd;var y=F(r,1,f);y=F(y,2,h);y=F(y,3,g);y=F(y,4,e);y=F(y,9,m);var Ga=new cd;Ga=F(Ga,1,k);J(y,5,Ga);y=3;e=e*b*f*(1-1/(1+Math.exp(-h*(Math.log(e*b/1E6)-k-g))));e<m*b&&(e=m*b,y=6);m=1E6*a?.D();c=c?.L()??0;a?.h()?.ja()&&e<m&&m<b&&(a=d?.P()??0,e=m+a*(0===c?Math.log(b/m):(b-m)/(c-m))*1E6,y=7);return{o:e,N:y,qa:r}};function Ld(a,b,c){if(!(0<C(a,2).length&&C(a,2).length===B(a,3,xb).length&&C(a,2).length===C(a,4).length))return 0;let d=0,e=0,f=1;for(const h of B(a,3,xb)){let g=0;switch(h){case 1:g=C(a,2)[e]*(b.W?Math.pow(b.W,C(a,4)[e]):0);break;case 2:d=g=C(a,2)[e]*(b.ia?Math.pow(b.ia,C(a,4)[e]):0);break;case 3:g=C(a,2)[e]}if(0===g)return 0;f*=g;e+=1}0<P(a,7)&&(b=1,0<c&&(b=c),f=Math.min(f,P(a,7)*b*d*1E3));return 1E6*f}function Md(a,b,c){let d=0;c&&(0<I(c,Hc,7).length?d=Ld(I(c,Hc,7)[0],a,b):0<I(c,Hc,8).length&&(d=Ld(I(c,Hc,8)[0],a,b)));return d};var Nd={Za:0,Ua:1,Pa:2,Ra:3,Qa:4,Va:5,Xa:6,ab:7,Ta:8,Na:9,Ya:10,Sa:11,Oa:12,Wa:13};function Od(a,b,c){if(R(a,2)!==R(b,2))return c;let d=!1;switch(R(a,2)){case 1:a:{var e=new Set(B(a,3,vb)??[]);for(var f of B(b,3,vb))if(e.has(f)){d=!0;break a}d=!1}break;case 0:a:{f=new Set(B(a,4,zb)??[]);for(e of B(b,4,zb))if(f.has(e)){d=!0;break a}d=!1}break;case 2:b=new Pd(b);d=(f=H(a,Oc,5))?Qd(b,f):!1;break;case 3:a:{f=new Set;for(const h of B(a,9,u(a.j)&18?Db:Eb))f.add(Ta(h));if(0===f.size)d=!0;else{for(const h of B(b,6,u(b.j)&18?Db:Eb))if(f.has(Ta(h))){d=!0;break a}d=!1}}break;case 4:d=Rd(a,b)}return M(a,6)?d?null:c:d?c:null}function Rd(a,b){a=H(a,Mc,10);if(void 0===a)return!1;var c=I(b,Qc,7);if(0===c.length)return!1;b=!0;for(const e of c)if(N(e,1)===N(a,1)&&N(e,2)===N(a,2)){b&&(b=!1);var d=B(e,3,ub);if(0===d.length)return!1;c=!0;for(const f of d){d=f;const h=Math.floor(d/32),g=B(a,3,vb);if(!(h>=g.length||0!==(g[h]>>>d%32)%2)){c=!1;break}}if(c)return!0}return b}function Qd(a,b){const c=R(b,1),d=I(b,Nc,3),e=I(b,Oc,2);let f;switch(c){case 2:f=d.every(h=>Sd(a,h))&&e.every(h=>Qd(a,h));break;case 1:f=d.some(h=>Sd(a,h))||e.some(h=>Qd(a,h));break;default:Ac(c)}return M(b,4)?!f:f}var Sd=function(a,b){const c=Tb(b,2);return(a=a.Y.get(Tb(b,1)))?a.has(c):!1};class Pd{constructor(a){this.Y=new Map;for(const b of I(a,Nc,5)){a=Tb(b,1);const c=Tb(b,2);let d=this.Y.get(a);d||(d=new Set,this.Y.set(a,d));d.add(c)}}};function Td(a,b){for(const c of b?.get(a)||[])if(c.count+1>c.Ia)return!1;return!0};function Ud(a){a=a.split("!");let b="-1";for(let c=0;c<a.length;c++)a[c].match(/^\dm\d+/)?c+=+a[c].substring(2):a[c].match(/^\d{2}m\d+/)?c+=+a[c].substring(3):a[c].match(/^1j\d+/)&&(b=a[c].substring(2));return b}function Vd(a,b){if(!a.Ga)return 1;const c=a.sa?.some(e=>b.va?.includes(e,0));var d=b.interestGroupName??void 0;if(void 0===d)return 10;d=Ud(d);if("-1"===d)return 11;a=a.sa?.includes(d,0);return c&&!a?10:1};function Wd(a,b){return null==a.oa?!0:!a.oa.some(c=>b.va?.includes(c,0))};function Xd({R:a,v:b,G:c,U:d}={}){return Yd(0,a?.h()??[],b?.i()??[],c??new Map,d??!1)}function Zd({R:a,v:b,G:c,U:d}={}){return Yd(1,a?.i()??[],b?.g()??[],c??new Map,d??!1)}function $d({R:a,v:b,G:c,U:d}={}){return Yd(1,a?.g()??[],b?.g()??[],c??new Map,d??!1)}function Yd(a,b,c,d,e){let f=0;const h=new Map;for(var g of b)h.set(Q(g,1),g),f=P(g,3);g=null;for(const k of c)if(f=P(k,3),e&&(f=ae(f,k,d)),b=h.get(Q(k,1))){a:{c=a;g=k;b=C(b,2);const m=C(g,2);if(b.length===m.length){g=0;for(let r=0;r<b.length;r++)g+=b[r]*m[r];b=g}else b=void 0;if(void 0!==b)switch(c){case 0:c=1/(1+Math.exp(-1*b));break a;case 1:c=Math.exp(b);break a}c=void 0}if(void 0!==c)return e&&(c=ae(c,k,d)),c;g=f}return g??f}function ae(a,b,c){const d=N(G(b,Jc,4),1);c=c.get(d);if(void 0===c||c>=Rb(G(b,Jc,4),Ic,2).length)return a;b=Rb(G(b,Jc,4),Ic,2)[c];return C(b,1).length?a*C(b,1)[0]:a};function be(a,b,c){"0"===a||c.has(a)||c.set(a,b.filter(d=>0<N(d,3)).map(d=>{let e=N(d,3);switch(R(d,1)){case 6:d=60*N(d,2);break;case 1:d=3600*N(d,2);break;case 2:d=86400*N(d,2);break;case 3:d=604800*N(d,2);break;case 4:d=2592E3*N(d,2);break;case 5:d=null;break;default:e=d=0}return{ua:d,Ia:e,count:0}}))}function ce(a,b,c){if(b=c.get(b))for(const d of b)(null===d.ua||N(a,1)<=d.ua)&&d.count++};var de=class extends T{};de.m=[2];var ee=class extends T{constructor(){super()}};ee.m=[2];function fe(a,b,c){c=I(c,de,2);var d=0;for(let h=c.length-1;0<=h;h--){d*=C(c[h],2).length+1;a:switch(R(c[h],1)){case 1:var e=a;var f=Ed(Ba(b.l()));f&&z(f,1)?(f=z(f,1),e=L(z(e,3),0),e=0===f||0===e||f<e?-1:(f-e)/6E7):e=-1;break a;default:e=-1}f=0;const g=C(c[h],2);for(;f<g.length&&!(e<g[f]);f++);d+=f}return d}function ge(){var a=new ee;a=E(a,1,0,0);var b=new de;b=E(b,1,1,0);b=Fb(b,2,[20],sb);Sb(a,2,de,b);return[a]};var he=class extends T{};he.m=[1];const ie={ad:{},bid:0,render:"",allowComponentAuction:!0};function je(a,b,c,d){b=b?new vd(vc(b)):void 0;if(!b||!I(b,td,2).length&&!b.g().g()&&!b.g().h())return ie;const e=new Vc(vc(a.userBiddingSignals)),f=a.ads.map(k=>({renderUrl:k.renderUrl,metadata:new Ec(vc(k.metadata))})),h=a.trustedBiddingSignalsKeys?a.trustedBiddingSignalsKeys[0]:void 0;let g;c&&h&&c[h]&&(g=gc(Zc,vc(c[h])));c=d.prevWins.map(k=>{var m=new Fc;m=E(m,1,k[0],0);k=new Ec(vc(k[1].metadata));return J(m,2,k)});return ke(a.name,e,f,d,c,g,b)}function ke(a,b,c,d,e,f,h){let g=null,k=null,m=null;if(h&&(M(G(h,$c,12),1)||M(G(h,$c,12),2))){var r=new wd;var y=E(r,2,0,0);var Ga=E(y,5,a,"");var te=E(Ga,7,!1,!1);var ue=J(te,8,h);g=E(ue,9,h.l(),"");const l=globalThis.forDebuggingOnly;M(G(h,$c,12),1)&&(k=new Cc(l.reportAdAuctionWin));M(G(h,$c,12),2)&&(m=new Cc(l.reportAdAuctionLoss));if(M(G(h,$c,12),5)){var ve=J(g,6,b),we=new Gc;var xe=E(we,1,d.topWindowHostname,"");var ye=E(xe,2,d.seller,"");var ze=E(ye,3,d.topLevelSeller,"");var Ae=E(ze,4,d.joinCount,0);var fd=E(Ae,5,d.bidCount,0);var S=e;rb(fd);if(null!=S){let p=!!S.length;for(let D=0;D<S.length;D++){const X=S[D];p=p&&!(u(X.j)&2)}let v=u(S),K;var gd=v|1;K=(p?gd|8:gd&-9)|4;K!=v&&(S=ib(S,K))}null==S&&(S=void 0);var Be=A(fd,6,S);var Ce=F(Be,7,d.dataVersion);J(ve,11,Ce);f&&J(g,3,f)}}var qa=g;const w={ta:h?.h()??void 0,ea:new Map,Z:new Map,aa:new Map,ba:new Map,ga:new Map,interestGroupName:a??void 0,Ma:R(b,2),Aa:d.joinCount};if(h?.g().C()||h?.g().D()||h?.g().B()){const l=new Map,p=ge();for(const v of p){const K=fe(b,h,v);l.set(N(v,1),K)}w.G=l}if(h){const l=ge()[0];var De=fe(b,h,l);w.modelingSignals=De}const Y=new Map;if(f){for(const l of I(f,Yc,1)){const p=le(O(l,1),O(l,2),O(l,3));Y.set(p,l);be(O(l,2),I(l,Wc,9),w.ea);be(O(l,1),I(l,Wc,10),w.Z);be(O(l,6),I(l,Wc,11),w.aa);be(O(l,7),I(l,Wc,12),w.ba);be(O(l,8),I(l,Wc,13),w.ga)}for(const l of e)w.ea&&ce(l,O(G(l,Ec,2),2),w.ea),w.Z&&ce(l,O(G(l,Ec,2),1),w.Z),w.aa&&ce(l,O(G(l,Ec,2),4),w.aa),w.ba&&ce(l,O(G(l,Ec,2),5),w.ba),w.ga&&ce(l,O(G(l,Ec,2),6),w.ga)}const hd=new Map;if(h)for(const l of I(h,td,2)){const p=le(O(l,1),O(l,2),"");hd.set(p,P(l,3))}const Ib=[];for(const l of c){const p={renderUrl:l.renderUrl,H:O(l.metadata,1),J:O(l.metadata,2),T:O(l.metadata,3),da:O(l.metadata,4),ca:O(l.metadata,5),fa:O(l.metadata,6),na:O(l.metadata,7),Ba:O(l.metadata,8),Ca:O(l.metadata,9),Da:O(l.metadata,10),u:0,M:0},v=le(p.H,p.J,p.T);p.S=hd.get(le(p.H,p.J,""));if(!p.S){if(!h?.g().g()&&!h?.g().h()){qa&&W(qa,p,5,1);continue}else if(!Y.get(v)){qa&&W(qa,p,6,1);continue}p.v=Y.get(v)?.g()??void 0}p.I=Y.get(v)?.h()??void 0;if(p.na&&"0"!==p.na){const K=Y.get(v)?.i()??void 0;var Jb;if(!(Jb=!K)){a:{var Z=p,Ee=K;for(const D of Ee)if(Z.na===O(D,1)&&Z.Ba===O(D,2)&&Z.Ca===O(D,3)&&Z.Da===O(D,4)){Z.I||(Z.I=new Sc);const X=H(D,Sc,5);if(X){for(const sa of I(X,Pc,1))Sb(Z.I,1,Pc,sa);for(const sa of I(X,Rc,2))Sb(Z.I,2,Rc,sa)}var id=!0;break a}id=!1}Jb=!id}if(Jb){qa&&W(qa,p,13,1);continue}}p.oa=Y.get(v)?.B()??void 0;p.sa=Y.get(v)?.C()??void 0;p.Ga=Y.get(v)?.l()??!1;Ib.push(p)}const Kb=h?.P();if(Kb){const l=new Map,p=new Map;for(const D of I(Kb,Pc,1))l.set(R(D,1),D);for(const D of I(Kb,Rc,2))p.set(R(D,1),D);const {Ka:v,Y:K}={Ka:l,Y:p};w.Ea=v;w.Fa=K}w.va=B(b,1,yb)??void 0;const Lb=1!==w.Ma||w.ta?.la()&&!(w.Aa<w.ta?.xa())?1:8;if(1!==Lb&&null!==g)for(const l of Ib)W(g,l,Lb,2);let jd=ie;if(1===Lb){var U=g,kd=h?.h();const l=[],p=new Map;for(const v of Ib){if(void 0!==kd&&kd?.wa()&&(v.renderUrl.includes("/td/adfetch/dv3")||Hd(v.v))){U&&W(U,v,12,3);continue}var aa=v,Ka=w;if(!Td(aa.J,Ka.ea)||!Td(aa.H,Ka.Z)||aa.da&&!Td(aa.da,Ka.aa)||aa.ca&&!Td(aa.ca,Ka.ba)||aa.fa&&!Td(aa.fa,Ka.ga)){U&&W(U,v,3,3);continue}if(v.I){var ld=w;const D=ld.Ea,X=ld.Fa,sa=v.I;if(D&&X&&sa)a:{var Fe=D,Ge=X,md=sa,nd=p;for(const ba of I(md,Pc,1)){const ca=R(ba,1),ta=Ge.get(ca);if(!ta)continue;let da=null;if(M(ba,7)){let La=nd.get(ca),od=!0;const pd=Tb(ba,8);if(void 0===La)La=new Map,nd.set(ca,La);else{const Mb=La.get(pd);if(Mb){var Ma=Mb;break a}null===Mb&&(od=!1)}od&&(da=Od(ba,ta,ca),La.set(pd,da))}else da=Od(ba,ta,ca);if(da){Ma=da;break a}}for(const ba of I(md,Rc,2)){const ca=R(ba,1),ta=Fe.get(ca);if(!ta)continue;const da=Od(ta,ba,ca);if(da){Ma=da;break a}}Ma=null}else Ma=null;const qd=Ma;if(qd){U&&W(U,v,4,3,qd);continue}}if(!Wd(v,w)){U&&W(U,v,7,3);continue}const K=Vd(v,w);1!==K?U&&W(U,v,K,3):l.push(v)}var He={ads:l,X:w};jd=me(He,g,h).ya}if(void 0!==h&&null!==g){if(k){var rd=g,Je=k;E(rd,2,1,0);Dd(Je,rd)}if(m){var sd=g,Ke=m;E(sd,2,2,0);Dd(Ke,sd)}}return jd}function W(a,b,c,d,e){a=Sb(a,10,Uc);d=E(a,3,d,0);var f=new Tc;f=E(f,1,b.renderUrl,"");f=E(f,2,b.H,"");f=E(f,3,b.J,"");f=E(f,4,b.T,"");f=F(f,12,b.u);f=F(f,13,b.M);var h=b.Ha??!1;f=E(f,16,null==h?h:!!h,!1);void 0!==b.da&&E(f,5,b.da,"");void 0!==b.ca&&E(f,6,b.ca,"");void 0!==b.fa&&E(f,7,b.fa,"");void 0!==b.v&&J(f,8,b.v);void 0!==b.ia&&F(f,9,b.ia);void 0!==b.W&&F(f,10,b.W);void 0!==b.S&&F(f,11,b.S);void 0!==b.I&&J(f,14,b.I);for(const g of b.oa??[])b=f,h=g,rb(b),Cb(b,15,2,!1).push(h);J(d,1,f);void 0!==e&&E(a,5,e,0);Object.values(Nd).includes(c)&&E(a,2,c,0)}function me(a,b,c){var d=[],e=[];for(var f of a.ads)if(null!=f.S)f.u=f.S,f.Ha=!0,d.push(f);else{c?.g().g()?(Hd(f.v)?f.W=$d({R:H(c,ad,1),v:f.v,G:a.X.G,U:c.g().B()}):(f.ia=Xd({R:H(c,ad,1),v:f.v,G:a.X.G,U:c.g().C()}),f.W=Zd({R:H(c,ad,1),v:f.v,G:a.X.G,U:c.g().D()})),f.u=Md(f,P(c.g(),12),f.v),M(c.g(),13)&&!f.u&&(f.u=f.v?.h()??0)):c?.g().h()&&(f.u=f.v?.h()??0);if(c?.g().l())if(f.H!==c?.i()?.l()||0<(c?.i()?.g()??"").length&&0<(c?.i()?.h()??"").length&&(f.J!==c?.i()?.g()||a.X.interestGroupName!==c?.i()?.h()))f.u=0;else if(c?.i()?.i()){var h=c?.i()?.B()??0;f.u=0===h?1:h}e.push(f)}e={renderUrl:"",H:"",J:"",T:"",u:0,M:0};c?.g().l()&&c?.i()?.i()?(c=a.ads.reduce((g,k)=>g.u<k.u?k:g,e),c.M=c.u):(d=a.ads.reduce((g,k)=>!Hd(k.v)&&g.u<k.u?k:g,e),h=a.ads.reduce((g,k)=>Hd(k.v)&&g.u<k.u?k:g,e),e=Kd(c,d?.u),e.o=Fd(e.o,c?.h()?.L()?.D()),f=Id(c,h?.u),f.o=Fd(f.o,c?.h()?.i()?.D()),e.o>f.o?(c=d,c.M=e.o):(c=h,c.M=f.o));if(b)for(const g of a.ads)W(b,g,le(g.H,g.J,g.T)===le(c.H,c.J,c.T)?1:9,4);return{ya:{ad:{},bid:c.M/1E6,bidCurrency:"USD",render:c.renderUrl,allowComponentAuction:!0,modelingSignals:a.X.modelingSignals,adCost:42},debugInfo:void 0}}function le(a,b,c){return a.concat("+",b,"+",c)};const ne=globalThis;function oe(a,b){var c={id:"fledge_auction_winner"};c.winner_qid=b?.l()||-1;a=a.renderUrl.split("?")[1]?.split("&")??[];for(const d of a)if(a=d.split("="),"cr_id"===a[0]){c.winner_cid=a[1];break}c=Object.entries(c).map(([d,e])=>`${encodeURIComponent(d)}=${encodeURIComponent(e)}`).join("&");ne.sendReportTo(`${"https://pagead2.googlesyndication.com/pagead/gen_204"}?${c}`)};const pe=globalThis;function qe(a){let b=0;a=Rb(G(G(a,Ad,1),zd,3),yd,1);if(!a)return 0;for(const c of a)null!=L(z(c,2),0)&&b<L(z(c,2),0)&&(b=L(z(c,2),0));return b}function re(a,b,c,d,e,f){const h=c?Array.isArray(c)?new Bd(c):c:void 0;var g={};g.haveSellerSignals=h?1:0;g.haveBuyerSignals=e?1:0;g.winner_qid=(h||e)?.l()||-1;g.xfpQid=h?.B()||-1;g.is_plog=h?.C()?1:0;g.ecrs=h?.g()||"";g.cf=h?.h()?.g()||0;g.pf=h?qe(h):0;g.pubp=e?.B()||0;h?.D()||(g.turtlexTest=1);g.tdeid=B(a,1,ub).join(",");g.bid=b.winningBidCpmUsdMicros;g.hsobid=b.highestScoringOtherBidCpmUsdMicros;(a=e?.la().C())&&"0"!==a&&(g.ddid=a);d&&(g.desirability=d.desirability,g.igown=d.interestGroupOwner,g.rurl=d.renderUrl,g.topWindowHostname=d.topWindowHostname,g.bsig=JSON.stringify(d),g.sdv=d.dataVersion??-1);f&&(g.igown=f.interestGroupOwner,g.wign=f.interestGroupName,g.rurl=f.renderUrl,g.topWindowHostname=f.topWindowHostname,g.bdv=f.dataVersion??-1,g.modelingSignals=f.modelingSignals??-1,g.joinCount=f.joinCount??-1,g.recency=f.recency??-1,g.adCost=f.adCost??-1);g=`${"https://googleads.g.doubleclick.net/td/auctionwinner"}?${Object.entries(g).map(([k,m])=>`${encodeURIComponent(k)}=${encodeURIComponent(m)}`).join("&")}`;e?.C()?se(g,b,c,d,e,f):(pe.sendReportTo(g),d&&!["https://googleads.g.doubleclick.net","https://td.doubleclick.net"].includes(d.interestGroupOwner)&&Ie(b,c,d))}function Le(a,b,c,d,e){const f=b?Array.isArray(b)?Cd(JSON.stringify(b)):b:void 0;b={};b.bid=a.winningBidCpmUsdMicros;b.cid=f?.i()||"";b.ecrs=f?.g()||"";b.winner_qid=(d||f)?.l()||-1;c&&(b.rurl=c.renderUrl??"",b.igown=c.interestGroupOwner);e&&(b.wbid=1E6*e.bid,b.wign=e.interestGroupName??"");d&&((a=G(d,ad,1))&&(b.bqs=JSON.stringify(a.toJSON())),(a=G(d,dd,6))&&(b.gfpa=JSON.stringify(a.toJSON())),(a=d.g())&&(b.qf=JSON.stringify(a.toJSON())),b.mafprcu=d.D()||0,b.pubp=d.B()||0);return Object.entries(b).map(([h,g])=>`${encodeURIComponent(h)}=${encodeURIComponent(g)}`).join("&")}function Ie(a,b,c){a=`${"https://googleads.g.doubleclick.net/td/adclick"}?${Le(a,b,c)}`;pe.registerAdBeacon?.({click:a})}function se(a,b,c,d,e,f){b=Le(b,c,d,e,f);c=`${"https://googleads.g.doubleclick.net/td/adclick"}?${b}`;d=`${"https://www.googleadservices.com/td/adview"}?${b}`;e=`${"https://www.googleadservices.com/td/adclick"}?${b}`+"&navigation=1";f&&f.renderUrl.includes("/td/adfetch/dv3")&&(e=`${"https://ad.doubleclick.net/td/adclick"}?${b}`+"&navigation=1",d=`${"https://ad.doubleclick.net/td/adview"}?${b}`);pe.registerAdBeacon?.({click:c,impression:d,interaction:`${"https://googleads.g.doubleclick.net/td/adinteraction"}?${b}`,auctionwinner:a,"reserved.top_navigation":e,["active-view-viewable"]:`${"https://googleads.g.doubleclick.net/td/activeview"}?acvw=td_r%3Dviewable&${b}`,["active-view-time-on-screen"]:`${"https://googleads.g.doubleclick.net/td/activeview"}?acvw=td_r%3Dtos&${b}`,["active-view-unmeasurable"]:`${"https://googleads.g.doubleclick.net/td/activeview"}?acvw=td_r%3Dunmeasurable&${b}`,["active-view-begin-to-render"]:`${"https://googleads.g.doubleclick.net/td/activeview"}?acvw=td_r%3Db2r&${b}`,["active-view-error"]:"https://pagead2.googlesyndication.com/pagead/gen_204?id=av-js&type=error&bin=7&td=1"})};var Me=yc(class extends T{}),Hb=[3,4];const Ne=globalThis;var Gb,Oe=stc;tc(Oe,a=>"string"===typeof a);Gb=Me(Oe);var Pe,Qe=3===Nb();const Re=uc();if(!Qe)throw Error(Re&&Re()||String(Qe));var Se;Se=3===Nb();Pe=H(Gb,he,Se?3:-1);Ne.generateBid=(a,b,c,d,e)=>je(a,c,d,e);Ne.reportWin=function(a){return(b,c,d,e)=>{b=c?new vd(vc(c)):void 0;d=["https://securepubads.g.doubleclick.net","https://pubads.g.doubleclick.net"].includes(e.seller)?d:void 0;c={winningBidCpmUsdMicros:1E6*e.bid,highestScoringOtherBidCpmUsdMicros:1E6*e.highestScoringOtherBid};b?.C()&&oe(e,b);re(a,c,d?.sellerSignalsJspb,d?.sellerReportingBrowserSignals,b,e)}}(Pe);}).call(this,"[null,null,[]]");
)";
constexpr std::string_view kHandlerNameGoogleAdManagerGenerateBid =
    "generateBid";

constexpr std::string_view kCodeJetstreamUniPoker = R"JS_CODE(
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"use strict";


class CardDeck
{
    constructor()
    {
        this.newDeck();
    }

    newDeck()
    {
        // Make a shallow copy of a new deck
        this._cards = CardDeck._newDeck.slice(0);
    }

    shuffle()
    {
        this.newDeck();

        for (let index = 52; index !== 0;) {
            // Select a random card
            let randomIndex = Math.floor(Math.random() * index);
            index--;

            // Swap the current card with the random card
            let tempCard = this._cards[index];
            this._cards[index] = this._cards[randomIndex];
            this._cards[randomIndex] = tempCard;
        }
    }

    dealOneCard()
    {
        return this._cards.shift();
    }

    static cardRank(card)
    {
        // This returns a numeric value for a card.
        // Ace is highest.

        let rankOfCard = card.codePointAt(0) & 0xf;
        if (rankOfCard == 0x1) // Make Aces higher than Kings
            rankOfCard = 0xf;

        return rankOfCard;
    }

    static cardName(card)
    {
        if (typeof(card) == "string")
            card = card.codePointAt(0);
        return this._rankNames[card & 0xf];
    }
}

CardDeck._rankNames = [
    "", "Ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "", "Queen", "King"
];

CardDeck._newDeck = [
    // Spades
    "\u{1f0a1}", "\u{1f0a2}",  "\u{1f0a3}",  "\u{1f0a4}",  "\u{1f0a5}",
    "\u{1f0a6}", "\u{1f0a7}",  "\u{1f0a8}",  "\u{1f0a9}",  "\u{1f0aa}",
    "\u{1f0ab}", "\u{1f0ad}",  "\u{1f0ae}",
    // Hearts
    "\u{1f0b1}", "\u{1f0b2}",  "\u{1f0b3}",  "\u{1f0b4}",  "\u{1f0b5}",
    "\u{1f0b6}", "\u{1f0b7}",  "\u{1f0b8}",  "\u{1f0b9}",  "\u{1f0ba}",
    "\u{1f0bb}", "\u{1f0bd}",  "\u{1f0be}",
    // Clubs
    "\u{1f0d1}", "\u{1f0d2}",  "\u{1f0d3}",  "\u{1f0d4}",  "\u{1f0d5}",
    "\u{1f0d6}", "\u{1f0d7}",  "\u{1f0d8}",  "\u{1f0d9}",  "\u{1f0da}",
    "\u{1f0db}", "\u{1f0dd}",  "\u{1f0de}",
    // Diamonds
    "\u{1f0c1}", "\u{1f0c2}",  "\u{1f0c3}",  "\u{1f0c4}",  "\u{1f0c5}",
    "\u{1f0c6}", "\u{1f0c7}",  "\u{1f0c8}",  "\u{1f0c9}",  "\u{1f0ca}",
    "\u{1f0cb}", "\u{1f0cd}",  "\u{1f0ce}"
];

class Hand
{
    constructor()
    {
        this.clear();
    }

    clear()
    {
        this._cards = [];
        this._rank = 0;
    }

    takeCard(card)
    {
        this._cards.push(card);
    }

    score()
    {
        // Sort highest rank to lowest
        this._cards.sort((a, b) => {
            return CardDeck.cardRank(b) - CardDeck.cardRank(a);
        });

        let handString = this._cards.join("");

        let flushResult = handString.match(Hand.FlushRegExp);
        let straightResult = handString.match(Hand.StraightRegExp);
        let ofAKindResult = handString.match(Hand.OfAKindRegExp);

        if (flushResult) {
            if (straightResult) {
                if (straightResult[1])
                    this._rank = Hand.RoyalFlush;
                else
                    this._rank = Hand.StraightFlush
            } else
                this._rank = Hand.Flush;

            this._rank |= CardDeck.cardRank(this._cards[0]) << 16 | CardDeck.cardRank(this._cards[1]) << 12;
        } else if (straightResult)
            this._rank = Hand.Straight | CardDeck.cardRank(this._cards[0]) << 16 | CardDeck.cardRank(this._cards[1]) << 12;
        else if (ofAKindResult) {
            // When comparing lengths, a matched unicode character has a length of 2.
            // Therefore expected lengths are doubled, e.g a pair will have a match length of 4.
            if (ofAKindResult[0].length == 8)
                this._rank = Hand.FourOfAKind | CardDeck.cardRank(this._cards[0]);
            else {
                // Found pair or three of a kind.  Check for two pair or full house.
                let firstOfAKind = ofAKindResult[0];
                let remainingCardsIndex = handString.indexOf(firstOfAKind) + firstOfAKind.length;
                let secondOfAKindResult;
                if (remainingCardsIndex <= 6
                    && (secondOfAKindResult = handString.slice(remainingCardsIndex).match(Hand.OfAKindRegExp))) {
                    if ((firstOfAKind.length == 6 && secondOfAKindResult[0].length == 4)
                        || (firstOfAKind.length == 4 && secondOfAKindResult[0].length == 6)) {
                        let threeOfAKindCardRank;
                        let twoOfAKindCardRank;
                        if (firstOfAKind.length == 6) {
                            threeOfAKindCardRank = CardDeck.cardRank(firstOfAKind.slice(0,2));
                            twoOfAKindCardRank = CardDeck.cardRank(secondOfAKindResult[0].slice(0,2));
                        } else {
                            threeOfAKindCardRank = CardDeck.cardRank(secondOfAKindResult[0].slice(0,2));
                            twoOfAKindCardRank = CardDeck.cardRank(firstOfAKind.slice(0,2));
                        }
                        this._rank = Hand.FullHouse | threeOfAKindCardRank << 16 | threeOfAKindCardRank < 12 | threeOfAKindCardRank << 8 | twoOfAKindCardRank << 4 | twoOfAKindCardRank;
                    } else if (firstOfAKind.length == 4 && secondOfAKindResult[0].length == 4) {
                        let firstPairCardRank = CardDeck.cardRank(firstOfAKind.slice(0,2));
                        let SecondPairCardRank = CardDeck.cardRank(secondOfAKindResult[0].slice(0,2));
                        let otherCardRank;
                        // Due to sorting, the other card is at index 0, 4 or 8
                        if (firstOfAKind.codePointAt(0) == handString.codePointAt(0)) {
                            if (secondOfAKindResult[0].codePointAt(0) == handString.codePointAt(4))
                                otherCardRank = CardDeck.cardRank(handString.slice(8,10));
                            else
                                otherCardRank = CardDeck.cardRank(handString.slice(4,6));
                        } else
                            otherCardRank = CardDeck.cardRank(handString.slice(0,2));

                        this._rank = Hand.TwoPair | firstPairCardRank << 16 | firstPairCardRank << 12 | SecondPairCardRank << 8 | SecondPairCardRank << 4 | otherCardRank;
                    }
                } else {
                    let ofAKindCardRank = CardDeck.cardRank(firstOfAKind.slice(0,2));
                    let otherCardsRank = 0;
                    for (let card of this._cards) {
                        let cardRank = CardDeck.cardRank(card);
                        if (cardRank != ofAKindCardRank)
                            otherCardsRank = (otherCardsRank << 4) | cardRank;
                    }

                    if (firstOfAKind.length == 6)
                        this._rank = Hand.ThreeOfAKind | ofAKindCardRank << 16 | ofAKindCardRank << 12 | ofAKindCardRank << 8 | otherCardsRank;
                    else
                        this._rank = Hand.Pair | ofAKindCardRank << 16 | ofAKindCardRank << 12 | otherCardsRank;
                }
            }
        } else {
            this._rank = 0;
            for (let card of this._cards) {
                let cardRank = CardDeck.cardRank(card);
                this._rank = (this._rank << 4) | cardRank;
            }
        }
    }

    get rank()
    {
        return this._rank;
    }

    toString()
    {
        return this._cards.join("");
    }
}

Hand.FlushRegExp = new RegExp("([\u{1f0a1}-\u{1f0ae}]{5})|([\u{1f0b1}-\u{1f0be}]{5})|([\u{1f0c1}-\u{1f0ce}]{5})|([\u{1f0d1}-\u{1f0de}]{5})", "u");

Hand.StraightRegExp = new RegExp("([\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}][\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}][\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}][\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}])|[\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}][\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}][\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}]|[\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}][\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}]|[\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}][\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}]|[\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}][\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}]|[\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}][\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}]|[\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}][\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}]|[\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}][\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}][\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}]|[\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}][\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}][\u{1f0a2}\u{1f0b2}\u{1f0d2}\u{1f0c2}]|[\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}][\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}][\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}][\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}][\u{1f0a2}\u{1f0b2}\u{1f0d2}\u{1f0c2}]", "u");

Hand.OfAKindRegExp = new RegExp("(?:[\u{1f0a1}\u{1f0b1}\u{1f0d1}\u{1f0c1}]{2,4})|(?:[\u{1f0ae}\u{1f0be}\u{1f0de}\u{1f0ce}]{2,4})|(?:[\u{1f0ad}\u{1f0bd}\u{1f0dd}\u{1f0cd}]{2,4})|(?:[\u{1f0ab}\u{1f0bb}\u{1f0db}\u{1f0cb}]{2,4})|(?:[\u{1f0aa}\u{1f0ba}\u{1f0da}\u{1f0ca}]{2,4})|(?:[\u{1f0a9}\u{1f0b9}\u{1f0d9}\u{1f0c9}]{2,4})|(?:[\u{1f0a8}\u{1f0b8}\u{1f0d8}\u{1f0c8}]{2,4})|(?:[\u{1f0a7}\u{1f0b7}\u{1f0d7}\u{1f0c7}]{2,4})|(?:[\u{1f0a6}\u{1f0b6}\u{1f0d6}\u{1f0c6}]{2,4})|(?:[\u{1f0a5}\u{1f0b5}\u{1f0d5}\u{1f0c5}]{2,4})|(?:[\u{1f0a4}\u{1f0b4}\u{1f0d4}\u{1f0c4}]{2,4})|(?:[\u{1f0a3}\u{1f0b3}\u{1f0d3}\u{1f0c3}]{2,4})|(?:[\u{1f0a2}\u{1f0b2}\u{1f0d2}\u{1f0c2}]{2,4})", "u");

Hand.RoyalFlush = 0x900000;
Hand.StraightFlush = 0x800000;
Hand.FourOfAKind = 0x700000;
Hand.FullHouse = 0x600000;
Hand.Flush = 0x500000;
Hand.Straight = 0x400000;
Hand.ThreeOfAKind = 0x300000;
Hand.TwoPair = 0x200000;
Hand.Pair = 0x100000;

class Player extends Hand
{
    constructor(name)
    {
        super();
        this._name = name;
        this._wins = 0;
        this._handTypeCounts = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    }

    scoreHand()
    {
        this.score();
        let handType = this.rank >> 20;
        this._handTypeCounts[handType]++;
    }

    wonHand()
    {
        this._wins++
    }

    get name()
    {
        return this._name;
    }

    get hand()
    {
        return super.toString();
    }

    get wins()
    {
        return this._wins;
    }

    get handTypeCounts()
    {
        return this._handTypeCounts;
    }
}

function playHands() {
    const n_players = 10;
    const players = Array.from({length: n_players}, i => new Player(`foo{i}`));
    let cardDeck = new CardDeck();
    let handsPlayed = 0;
    let highestRank = 0;

    do {
        cardDeck.shuffle();

        for (let player of players)
            player.clear();

        for (let i = 0; i < 5; i++) {
            for (let player of players)
                player.takeCard(cardDeck.dealOneCard());
        }

        for (let player of players)
            player.scoreHand();

        handsPlayed++;

        highestRank = 0;

        for (let player of players) {
            if (player.rank > highestRank)
                highestRank = player.rank;
        }

        for (let player of players) {
            // We count ties as wins for each player.
            if (player.rank == highestRank)
                player.wonHand();
        }
    } while (handsPlayed < 2000);
}
)JS_CODE";

constexpr std::string_view kHandlerNameJetstreamUniPoker = "playHands";

constexpr std::string_view kCodeJetstreamSplay = R"JS_CODE(
// Copyright 2009 the V8 project authors. All rights reserved.
// Copyright (C) 2015 Apple Inc. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This benchmark is based on a JavaScript log processing module used
// by the V8 profiler to generate execution time profiles for runs of
// JavaScript applications, and it effectively measures how fast the
// JavaScript engine is at allocating nodes and reclaiming the memory
// used for old nodes. Because of the way splay trees work, the engine
// also has to deal with a lot of changes to the large tree object
// graph.

// Configuration.
var kSplayTreeSize = 8000;
var kSplayTreeModifications = 80;
var kSplayTreePayloadDepth = 5;

var splayTree = null;
var splaySampleTimeStart = 0.0;

function GeneratePayloadTree(depth, tag) {
  if (depth == 0) {
    return {
      array  : [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ],
      string : 'String for key ' + tag + ' in leaf node'
    };
  } else {
    return {
      left:  GeneratePayloadTree(depth - 1, tag),
      right: GeneratePayloadTree(depth - 1, tag)
    };
  }
}


function GenerateKey() {
  // The benchmark framework guarantees that Math.random is
  // deterministic; see base.js.
  return Math.random();
}

var splaySamples = [];

function SplayLatency() {
  return splaySamples;
}

function SplayUpdateStats(time) {
  var pause = time - splaySampleTimeStart;
  splaySampleTimeStart = time;
  splaySamples.push(pause);
}

function InsertNewNode() {
  // Insert new node with a unique key.
  var key;
  do {
    key = GenerateKey();
  } while (splayTree.find(key) != null);
  var payload = GeneratePayloadTree(kSplayTreePayloadDepth, String(key));
  splayTree.insert(key, payload);
  return key;
}


function SplaySetup() {
  // Check if the platform has the performance.now high resolution timer.
  // If not, throw exception and quit.
  if (!performance.now) {
    throw "PerformanceNowUnsupported";
  }

  splayTree = new SplayTree();
  splaySampleTimeStart = performance.now()
  for (var i = 0; i < kSplayTreeSize; i++) {
    InsertNewNode();
    if ((i+1) % 20 == 19) {
      SplayUpdateStats(performance.now());
    }
  }
}


function SplayTearDown() {
  // Allow the garbage collector to reclaim the memory
  // used by the splay tree no matter how we exit the
  // tear down function.
  var keys = splayTree.exportKeys();
  splayTree = null;

  splaySamples = [];

  // Verify that the splay tree has the right size.
  var length = keys.length;
  if (length != kSplayTreeSize) {
    throw new Error("Splay tree has wrong size");
  }

  // Verify that the splay tree has sorted, unique keys.
  for (var i = 0; i < length - 1; i++) {
    if (keys[i] >= keys[i + 1]) {
      throw new Error("Splay tree not sorted");
    }
  }
}


function SplayRun() {
  // Replace a few nodes in the splay tree.
  for (var i = 0; i < kSplayTreeModifications; i++) {
    var key = InsertNewNode();
    var greatest = splayTree.findGreatestLessThan(key);
    if (greatest == null) splayTree.remove(key);
    else splayTree.remove(greatest.key);
  }
  SplayUpdateStats(performance.now());
}


/**
 * Constructs a Splay tree.  A splay tree is a self-balancing binary
 * search tree with the additional property that recently accessed
 * elements are quick to access again. It performs basic operations
 * such as insertion, look-up and removal in O(log(n)) amortized time.
 *
 * @constructor
 */
function SplayTree() {
};


/**
 * Pointer to the root node of the tree.
 *
 * @type {SplayTree.Node}
 * @private
 */
SplayTree.prototype.root_ = null;


/**
 * @return {boolean} Whether the tree is empty.
 */
SplayTree.prototype.isEmpty = function() {
  return !this.root_;
};


/**
 * Inserts a node into the tree with the specified key and value if
 * the tree does not already contain a node with the specified key. If
 * the value is inserted, it becomes the root of the tree.
 *
 * @param {number} key Key to insert into the tree.
 * @param {*} value Value to insert into the tree.
 */
SplayTree.prototype.insert = function(key, value) {
  if (this.isEmpty()) {
    this.root_ = new SplayTree.Node(key, value);
    return;
  }
  // Splay on the key to move the last node on the search path for
  // the key to the root of the tree.
  this.splay_(key);
  if (this.root_.key == key) {
    return;
  }
  var node = new SplayTree.Node(key, value);
  if (key > this.root_.key) {
    node.left = this.root_;
    node.right = this.root_.right;
    this.root_.right = null;
  } else {
    node.right = this.root_;
    node.left = this.root_.left;
    this.root_.left = null;
  }
  this.root_ = node;
};


/**
 * Removes a node with the specified key from the tree if the tree
 * contains a node with this key. The removed node is returned. If the
 * key is not found, an exception is thrown.
 *
 * @param {number} key Key to find and remove from the tree.
 * @return {SplayTree.Node} The removed node.
 */
SplayTree.prototype.remove = function(key) {
  if (this.isEmpty()) {
    throw Error('Key not found: ' + key);
  }
  this.splay_(key);
  if (this.root_.key != key) {
    throw Error('Key not found: ' + key);
  }
  var removed = this.root_;
  if (!this.root_.left) {
    this.root_ = this.root_.right;
  } else {
    var right = this.root_.right;
    this.root_ = this.root_.left;
    // Splay to make sure that the new root has an empty right child.
    this.splay_(key);
    // Insert the original right child as the right child of the new
    // root.
    this.root_.right = right;
  }
  return removed;
};


/**
 * Returns the node having the specified key or null if the tree doesn't contain
 * a node with the specified key.
 *
 * @param {number} key Key to find in the tree.
 * @return {SplayTree.Node} Node having the specified key.
 */
SplayTree.prototype.find = function(key) {
  if (this.isEmpty()) {
    return null;
  }
  this.splay_(key);
  return this.root_.key == key ? this.root_ : null;
};


/**
 * @return {SplayTree.Node} Node having the maximum key value.
 */
SplayTree.prototype.findMax = function(opt_startNode) {
  if (this.isEmpty()) {
    return null;
  }
  var current = opt_startNode || this.root_;
  while (current.right) {
    current = current.right;
  }
  return current;
};


/**
 * @return {SplayTree.Node} Node having the maximum key value that
 *     is less than the specified key value.
 */
SplayTree.prototype.findGreatestLessThan = function(key) {
  if (this.isEmpty()) {
    return null;
  }
  // Splay on the key to move the node with the given key or the last
  // node on the search path to the top of the tree.
  this.splay_(key);
  // Now the result is either the root node or the greatest node in
  // the left subtree.
  if (this.root_.key < key) {
    return this.root_;
  } else if (this.root_.left) {
    return this.findMax(this.root_.left);
  } else {
    return null;
  }
};


/**
 * @return {Array<*>} An array containing all the keys of tree's nodes.
 */
SplayTree.prototype.exportKeys = function() {
  var result = [];
  if (!this.isEmpty()) {
    this.root_.traverse_(function(node) { result.push(node.key); });
  }
  return result;
};


/**
 * Perform the splay operation for the given key. Moves the node with
 * the given key to the top of the tree.  If no node has the given
 * key, the last node on the search path is moved to the top of the
 * tree. This is the simplified top-down splaying algorithm from:
 * "Self-adjusting Binary Search Trees" by Sleator and Tarjan
 *
 * @param {number} key Key to splay the tree on.
 * @private
 */
SplayTree.prototype.splay_ = function(key) {
  if (this.isEmpty()) {
    return;
  }
  // Create a dummy node.  The use of the dummy node is a bit
  // counter-intuitive: The right child of the dummy node will hold
  // the L tree of the algorithm.  The left child of the dummy node
  // will hold the R tree of the algorithm.  Using a dummy node, left
  // and right will always be nodes and we avoid special cases.
  var dummy, left, right;
  dummy = left = right = new SplayTree.Node(null, null);
  var current = this.root_;
  while (true) {
    if (key < current.key) {
      if (!current.left) {
        break;
      }
      if (key < current.left.key) {
        // Rotate right.
        var tmp = current.left;
        current.left = tmp.right;
        tmp.right = current;
        current = tmp;
        if (!current.left) {
          break;
        }
      }
      // Link right.
      right.left = current;
      right = current;
      current = current.left;
    } else if (key > current.key) {
      if (!current.right) {
        break;
      }
      if (key > current.right.key) {
        // Rotate left.
        var tmp = current.right;
        current.right = tmp.left;
        tmp.left = current;
        current = tmp;
        if (!current.right) {
          break;
        }
      }
      // Link left.
      left.right = current;
      left = current;
      current = current.right;
    } else {
      break;
    }
  }
  // Assemble.
  left.right = current.left;
  right.left = current.right;
  current.left = dummy.right;
  current.right = dummy.left;
  this.root_ = current;
};


/**
 * Constructs a Splay tree node.
 *
 * @param {number} key Key.
 * @param {*} value Value.
 */
SplayTree.Node = function(key, value) {
  this.key = key;
  this.value = value;
};


/**
 * @type {SplayTree.Node}
 */
SplayTree.Node.prototype.left = null;


/**
 * @type {SplayTree.Node}
 */
SplayTree.Node.prototype.right = null;


/**
 * Performs an ordered traversal of the subtree starting at
 * this SplayTree.Node.
 *
 * @param {function(SplayTree.Node)} f Visitor function.
 * @private
 */
SplayTree.Node.prototype.traverse_ = function(f) {
  var current = this;
  while (current) {
    var left = current.left;
    if (left) left.traverse_(f);
    f(current);
    current = current.right;
  }
};

SplaySetup();

function SplayTest() {
  for (let i = 0; i < 50; ++i) {
    SplayRun();
  }
}
)JS_CODE";

constexpr std::string_view kHandlerNameJetstreamSplay = "SplayTest";

const std::string_view kCodeJetstreamDeltaBlue = R"JS_CODE(
// Copyright 2008 the V8 project authors. All rights reserved.
// Copyright 1996 John Maloney and Mario Wolczko.

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


// This implementation of the DeltaBlue benchmark is derived
// from the Smalltalk implementation by John Maloney and Mario
// Wolczko. Some parts have been translated directly, whereas
// others have been modified more aggressively to make it feel
// more like a JavaScript program.



/**
 * A JavaScript implementation of the DeltaBlue constraint-solving
 * algorithm, as described in:
 *
 * "The DeltaBlue Algorithm: An Incremental Constraint Hierarchy Solver"
 *   Bjorn N. Freeman-Benson and John Maloney
 *   January 1990 Communications of the ACM,
 *   also available as University of Washington TR 89-08-06.
 *
 * Beware: this benchmark is written in a grotesque style where
 * the constraint model is built by side-effects from constructors.
 * I've kept it this way to avoid deviating too much from the original
 * implementation.
 */


/* --- O b j e c t   M o d e l --- */

Object.defineProperty(Object.prototype, "inheritsFrom", {

  value: function (shuper) {
    function Inheriter() { }
    Inheriter.prototype = shuper.prototype;
    this.prototype = new Inheriter();
    this.superConstructor = shuper;
  }
});

function OrderedCollection() {
  this.elms = new Array();
}

OrderedCollection.prototype.add = function (elm) {
  this.elms.push(elm);
}

OrderedCollection.prototype.at = function (index) {
  return this.elms[index];
}

OrderedCollection.prototype.size = function () {
  return this.elms.length;
}

OrderedCollection.prototype.removeFirst = function () {
  return this.elms.pop();
}

OrderedCollection.prototype.remove = function (elm) {
  var index = 0, skipped = 0;
  for (var i = 0; i < this.elms.length; i++) {
    var value = this.elms[i];
    if (value != elm) {
      this.elms[index] = value;
      index++;
    } else {
      skipped++;
    }
  }
  for (var i = 0; i < skipped; i++)
    this.elms.pop();
}

/* --- *
 * S t r e n g t h
 * --- */

/**
 * Strengths are used to measure the relative importance of constraints.
 * New strengths may be inserted in the strength hierarchy without
 * disrupting current constraints.  Strengths cannot be created outside
 * this class, so pointer comparison can be used for value comparison.
 */
function Strength(strengthValue, name) {
  this.strengthValue = strengthValue;
  this.name = name;
}

Strength.stronger = function (s1, s2) {
  return s1.strengthValue < s2.strengthValue;
}

Strength.weaker = function (s1, s2) {
  return s1.strengthValue > s2.strengthValue;
}

Strength.weakestOf = function (s1, s2) {
  return this.weaker(s1, s2) ? s1 : s2;
}

Strength.strongest = function (s1, s2) {
  return this.stronger(s1, s2) ? s1 : s2;
}

Strength.prototype.nextWeaker = function () {
  switch (this.strengthValue) {
    case 0: return Strength.WEAKEST;
    case 1: return Strength.WEAK_DEFAULT;
    case 2: return Strength.NORMAL;
    case 3: return Strength.STRONG_DEFAULT;
    case 4: return Strength.PREFERRED;
    case 5: return Strength.REQUIRED;
  }
}

// Strength constants.
Strength.REQUIRED        = new Strength(0, "required");
Strength.STONG_PREFERRED = new Strength(1, "strongPreferred");
Strength.PREFERRED       = new Strength(2, "preferred");
Strength.STRONG_DEFAULT  = new Strength(3, "strongDefault");
Strength.NORMAL          = new Strength(4, "normal");
Strength.WEAK_DEFAULT    = new Strength(5, "weakDefault");
Strength.WEAKEST         = new Strength(6, "weakest");

/* --- *
 * C o n s t r a i n t
 * --- */

/**
 * An abstract class representing a system-maintainable relationship
 * (or "constraint") between a set of variables. A constraint supplies
 * a strength instance variable; concrete subclasses provide a means
 * of storing the constrained variables and other information required
 * to represent a constraint.
 */
function Constraint(strength) {
  this.strength = strength;
}

/**
 * Activate this constraint and attempt to satisfy it.
 */
Constraint.prototype.addConstraint = function () {
  this.addToGraph();
  planner.incrementalAdd(this);
}

/**
 * Attempt to find a way to enforce this constraint. If successful,
 * record the solution, perhaps modifying the current dataflow
 * graph. Answer the constraint that this constraint overrides, if
 * there is one, or nil, if there isn't.
 * Assume: I am not already satisfied.
 */
Constraint.prototype.satisfy = function (mark) {
  this.chooseMethod(mark);
  if (!this.isSatisfied()) {
    if (this.strength == Strength.REQUIRED)
      alert("Could not satisfy a required constraint!");
    return null;
  }
  this.markInputs(mark);
  var out = this.output();
  var overridden = out.determinedBy;
  if (overridden != null) overridden.markUnsatisfied();
  out.determinedBy = this;
  if (!planner.addPropagate(this, mark))
    alert("Cycle encountered");
  out.mark = mark;
  return overridden;
}

Constraint.prototype.destroyConstraint = function () {
  if (this.isSatisfied()) planner.incrementalRemove(this);
  else this.removeFromGraph();
}

/**
 * Normal constraints are not input constraints.  An input constraint
 * is one that depends on external state, such as the mouse, the
 * keyboard, a clock, or some arbitrary piece of imperative code.
 */
Constraint.prototype.isInput = function () {
  return false;
}

/* --- *
 * U n a r y   C o n s t r a i n t
 * --- */

/**
 * Abstract superclass for constraints having a single possible output
 * variable.
 */
function UnaryConstraint(v, strength) {
  UnaryConstraint.superConstructor.call(this, strength);
  this.myOutput = v;
  this.satisfied = false;
  this.addConstraint();
}

UnaryConstraint.inheritsFrom(Constraint);

/**
 * Adds this constraint to the constraint graph
 */
UnaryConstraint.prototype.addToGraph = function () {
  this.myOutput.addConstraint(this);
  this.satisfied = false;
}

/**
 * Decides if this constraint can be satisfied and records that
 * decision.
 */
UnaryConstraint.prototype.chooseMethod = function (mark) {
  this.satisfied = (this.myOutput.mark != mark)
    && Strength.stronger(this.strength, this.myOutput.walkStrength);
}

/**
 * Returns true if this constraint is satisfied in the current solution.
 */
UnaryConstraint.prototype.isSatisfied = function () {
  return this.satisfied;
}

UnaryConstraint.prototype.markInputs = function (mark) {
  // has no inputs
}

/**
 * Returns the current output variable.
 */
UnaryConstraint.prototype.output = function () {
  return this.myOutput;
}

/**
 * Calculate the walkabout strength, the stay flag, and, if it is
 * 'stay', the value for the current output of this constraint. Assume
 * this constraint is satisfied.
 */
UnaryConstraint.prototype.recalculate = function () {
  this.myOutput.walkStrength = this.strength;
  this.myOutput.stay = !this.isInput();
  if (this.myOutput.stay) this.execute(); // Stay optimization
}

/**
 * Records that this constraint is unsatisfied
 */
UnaryConstraint.prototype.markUnsatisfied = function () {
  this.satisfied = false;
}

UnaryConstraint.prototype.inputsKnown = function () {
  return true;
}

UnaryConstraint.prototype.removeFromGraph = function () {
  if (this.myOutput != null) this.myOutput.removeConstraint(this);
  this.satisfied = false;
}

/* --- *
 * S t a y   C o n s t r a i n t
 * --- */

/**
 * Variables that should, with some level of preference, stay the same.
 * Planners may exploit the fact that instances, if satisfied, will not
 * change their output during plan execution.  This is called "stay
 * optimization".
 */
function StayConstraint(v, str) {
  StayConstraint.superConstructor.call(this, v, str);
}

StayConstraint.inheritsFrom(UnaryConstraint);

StayConstraint.prototype.execute = function () {
  // Stay constraints do nothing
}

/* --- *
 * E d i t   C o n s t r a i n t
 * --- */

/**
 * A unary input constraint used to mark a variable that the client
 * wishes to change.
 */
function EditConstraint(v, str) {
  EditConstraint.superConstructor.call(this, v, str);
}

EditConstraint.inheritsFrom(UnaryConstraint);

/**
 * Edits indicate that a variable is to be changed by imperative code.
 */
EditConstraint.prototype.isInput = function () {
  return true;
}

EditConstraint.prototype.execute = function () {
  // Edit constraints do nothing
}

/* --- *
 * B i n a r y   C o n s t r a i n t
 * --- */

var Direction = new Object();
Direction.NONE     = 0;
Direction.FORWARD  = 1;
Direction.BACKWARD = -1;

/**
 * Abstract superclass for constraints having two possible output
 * variables.
 */
function BinaryConstraint(var1, var2, strength) {
  BinaryConstraint.superConstructor.call(this, strength);
  this.v1 = var1;
  this.v2 = var2;
  this.direction = Direction.NONE;
  this.addConstraint();
}

BinaryConstraint.inheritsFrom(Constraint);

/**
 * Decides if this constraint can be satisfied and which way it
 * should flow based on the relative strength of the variables related,
 * and record that decision.
 */
BinaryConstraint.prototype.chooseMethod = function (mark) {
  if (this.v1.mark == mark) {
    this.direction = (this.v2.mark != mark && Strength.stronger(this.strength, this.v2.walkStrength))
      ? Direction.FORWARD
      : Direction.NONE;
  }
  if (this.v2.mark == mark) {
    this.direction = (this.v1.mark != mark && Strength.stronger(this.strength, this.v1.walkStrength))
      ? Direction.BACKWARD
      : Direction.NONE;
  }
  if (Strength.weaker(this.v1.walkStrength, this.v2.walkStrength)) {
    this.direction = Strength.stronger(this.strength, this.v1.walkStrength)
      ? Direction.BACKWARD
      : Direction.NONE;
  } else {
    this.direction = Strength.stronger(this.strength, this.v2.walkStrength)
      ? Direction.FORWARD
      : Direction.BACKWARD
  }
}

/**
 * Add this constraint to the constraint graph
 */
BinaryConstraint.prototype.addToGraph = function () {
  this.v1.addConstraint(this);
  this.v2.addConstraint(this);
  this.direction = Direction.NONE;
}

/**
 * Answer true if this constraint is satisfied in the current solution.
 */
BinaryConstraint.prototype.isSatisfied = function () {
  return this.direction != Direction.NONE;
}

/**
 * Mark the input variable with the given mark.
 */
BinaryConstraint.prototype.markInputs = function (mark) {
  this.input().mark = mark;
}

/**
 * Returns the current input variable
 */
BinaryConstraint.prototype.input = function () {
  return (this.direction == Direction.FORWARD) ? this.v1 : this.v2;
}

/**
 * Returns the current output variable
 */
BinaryConstraint.prototype.output = function () {
  return (this.direction == Direction.FORWARD) ? this.v2 : this.v1;
}

/**
 * Calculate the walkabout strength, the stay flag, and, if it is
 * 'stay', the value for the current output of this
 * constraint. Assume this constraint is satisfied.
 */
BinaryConstraint.prototype.recalculate = function () {
  var ihn = this.input(), out = this.output();
  out.walkStrength = Strength.weakestOf(this.strength, ihn.walkStrength);
  out.stay = ihn.stay;
  if (out.stay) this.execute();
}

/**
 * Record the fact that this constraint is unsatisfied.
 */
BinaryConstraint.prototype.markUnsatisfied = function () {
  this.direction = Direction.NONE;
}

BinaryConstraint.prototype.inputsKnown = function (mark) {
  var i = this.input();
  return i.mark == mark || i.stay || i.determinedBy == null;
}

BinaryConstraint.prototype.removeFromGraph = function () {
  if (this.v1 != null) this.v1.removeConstraint(this);
  if (this.v2 != null) this.v2.removeConstraint(this);
  this.direction = Direction.NONE;
}

/* --- *
 * S c a l e   C o n s t r a i n t
 * --- */

/**
 * Relates two variables by the linear scaling relationship: "v2 =
 * (v1 * scale) + offset". Either v1 or v2 may be changed to maintain
 * this relationship but the scale factor and offset are considered
 * read-only.
 */
function ScaleConstraint(src, scale, offset, dest, strength) {
  this.direction = Direction.NONE;
  this.scale = scale;
  this.offset = offset;
  ScaleConstraint.superConstructor.call(this, src, dest, strength);
}

ScaleConstraint.inheritsFrom(BinaryConstraint);

/**
 * Adds this constraint to the constraint graph.
 */
ScaleConstraint.prototype.addToGraph = function () {
  ScaleConstraint.superConstructor.prototype.addToGraph.call(this);
  this.scale.addConstraint(this);
  this.offset.addConstraint(this);
}

ScaleConstraint.prototype.removeFromGraph = function () {
  ScaleConstraint.superConstructor.prototype.removeFromGraph.call(this);
  if (this.scale != null) this.scale.removeConstraint(this);
  if (this.offset != null) this.offset.removeConstraint(this);
}

ScaleConstraint.prototype.markInputs = function (mark) {
  ScaleConstraint.superConstructor.prototype.markInputs.call(this, mark);
  this.scale.mark = this.offset.mark = mark;
}

/**
 * Enforce this constraint. Assume that it is satisfied.
 */
ScaleConstraint.prototype.execute = function () {
  if (this.direction == Direction.FORWARD) {
    this.v2.value = this.v1.value * this.scale.value + this.offset.value;
  } else {
    this.v1.value = (this.v2.value - this.offset.value) / this.scale.value;
  }
}

/**
 * Calculate the walkabout strength, the stay flag, and, if it is
 * 'stay', the value for the current output of this constraint. Assume
 * this constraint is satisfied.
 */
ScaleConstraint.prototype.recalculate = function () {
  var ihn = this.input(), out = this.output();
  out.walkStrength = Strength.weakestOf(this.strength, ihn.walkStrength);
  out.stay = ihn.stay && this.scale.stay && this.offset.stay;
  if (out.stay) this.execute();
}

/* --- *
 * E q u a l i t  y   C o n s t r a i n t
 * --- */

/**
 * Constrains two variables to have the same value.
 */
function EqualityConstraint(var1, var2, strength) {
  EqualityConstraint.superConstructor.call(this, var1, var2, strength);
}

EqualityConstraint.inheritsFrom(BinaryConstraint);

/**
 * Enforce this constraint. Assume that it is satisfied.
 */
EqualityConstraint.prototype.execute = function () {
  this.output().value = this.input().value;
}

/* --- *
 * V a r i a b l e
 * --- */

/**
 * A constrained variable. In addition to its value, it maintain the
 * structure of the constraint graph, the current dataflow graph, and
 * various parameters of interest to the DeltaBlue incremental
 * constraint solver.
 **/
function Variable(name, initialValue) {
  this.value = initialValue || 0;
  this.constraints = new OrderedCollection();
  this.determinedBy = null;
  this.mark = 0;
  this.walkStrength = Strength.WEAKEST;
  this.stay = true;
  this.name = name;
}

/**
 * Add the given constraint to the set of all constraints that refer
 * this variable.
 */
Variable.prototype.addConstraint = function (c) {
  this.constraints.add(c);
}

/**
 * Removes all traces of c from this variable.
 */
Variable.prototype.removeConstraint = function (c) {
  this.constraints.remove(c);
  if (this.determinedBy == c) this.determinedBy = null;
}

/* --- *
 * P l a n n e r
 * --- */

/**
 * The DeltaBlue planner
 */
function Planner() {
  this.currentMark = 0;
}

/**
 * Attempt to satisfy the given constraint and, if successful,
 * incrementally update the dataflow graph.  Details: If satisfying
 * the constraint is successful, it may override a weaker constraint
 * on its output. The algorithm attempts to resatisfy that
 * constraint using some other method. This process is repeated
 * until either a) it reaches a variable that was not previously
 * determined by any constraint or b) it reaches a constraint that
 * is too weak to be satisfied using any of its methods. The
 * variables of constraints that have been processed are marked with
 * a unique mark value so that we know where we've been. This allows
 * the algorithm to avoid getting into an infinite loop even if the
 * constraint graph has an inadvertent cycle.
 */
Planner.prototype.incrementalAdd = function (c) {
  var mark = this.newMark();
  var overridden = c.satisfy(mark);
  while (overridden != null)
    overridden = overridden.satisfy(mark);
}

/**
 * Entry point for retracting a constraint. Remove the given
 * constraint and incrementally update the dataflow graph.
 * Details: Retracting the given constraint may allow some currently
 * unsatisfiable downstream constraint to be satisfied. We therefore collect
 * a list of unsatisfied downstream constraints and attempt to
 * satisfy each one in turn. This list is traversed by constraint
 * strength, strongest first, as a heuristic for avoiding
 * unnecessarily adding and then overriding weak constraints.
 * Assume: c is satisfied.
 */
Planner.prototype.incrementalRemove = function (c) {
  var out = c.output();
  c.markUnsatisfied();
  c.removeFromGraph();
  var unsatisfied = this.removePropagateFrom(out);
  var strength = Strength.REQUIRED;
  do {
    for (var i = 0; i < unsatisfied.size(); i++) {
      var u = unsatisfied.at(i);
      if (u.strength == strength)
        this.incrementalAdd(u);
    }
    strength = strength.nextWeaker();
  } while (strength != Strength.WEAKEST);
}

/**
 * Select a previously unused mark value.
 */
Planner.prototype.newMark = function () {
  return ++this.currentMark;
}

/**
 * Extract a plan for resatisfaction starting from the given source
 * constraints, usually a set of input constraints. This method
 * assumes that stay optimization is desired; the plan will contain
 * only constraints whose output variables are not stay. Constraints
 * that do no computation, such as stay and edit constraints, are
 * not included in the plan.
 * Details: The outputs of a constraint are marked when it is added
 * to the plan under construction. A constraint may be appended to
 * the plan when all its input variables are known. A variable is
 * known if either a) the variable is marked (indicating that has
 * been computed by a constraint appearing earlier in the plan), b)
 * the variable is 'stay' (i.e. it is a constant at plan execution
 * time), or c) the variable is not determined by any
 * constraint. The last provision is for past states of history
 * variables, which are not stay but which are also not computed by
 * any constraint.
 * Assume: sources are all satisfied.
 */
Planner.prototype.makePlan = function (sources) {
  var mark = this.newMark();
  var plan = new Plan();
  var todo = sources;
  while (todo.size() > 0) {
    var c = todo.removeFirst();
    if (c.output().mark != mark && c.inputsKnown(mark)) {
      plan.addConstraint(c);
      c.output().mark = mark;
      this.addConstraintsConsumingTo(c.output(), todo);
    }
  }
  return plan;
}

/**
 * Extract a plan for resatisfying starting from the output of the
 * given constraints, usually a set of input constraints.
 */
Planner.prototype.extractPlanFromConstraints = function (constraints) {
  var sources = new OrderedCollection();
  for (var i = 0; i < constraints.size(); i++) {
    var c = constraints.at(i);
    if (c.isInput() && c.isSatisfied())
      // not in plan already and eligible for inclusion
      sources.add(c);
  }
  return this.makePlan(sources);
}

/**
 * Recompute the walkabout strengths and stay flags of all variables
 * downstream of the given constraint and recompute the actual
 * values of all variables whose stay flag is true. If a cycle is
 * detected, remove the given constraint and answer
 * false. Otherwise, answer true.
 * Details: Cycles are detected when a marked variable is
 * encountered downstream of the given constraint. The sender is
 * assumed to have marked the inputs of the given constraint with
 * the given mark. Thus, encountering a marked node downstream of
 * the output constraint means that there is a path from the
 * constraint's output to one of its inputs.
 */
Planner.prototype.addPropagate = function (c, mark) {
  var todo = new OrderedCollection();
  todo.add(c);
  while (todo.size() > 0) {
    var d = todo.removeFirst();
    if (d.output().mark == mark) {
      this.incrementalRemove(c);
      return false;
    }
    d.recalculate();
    this.addConstraintsConsumingTo(d.output(), todo);
  }
  return true;
}


/**
 * Update the walkabout strengths and stay flags of all variables
 * downstream of the given constraint. Answer a collection of
 * unsatisfied constraints sorted in order of decreasing strength.
 */
Planner.prototype.removePropagateFrom = function (out) {
  out.determinedBy = null;
  out.walkStrength = Strength.WEAKEST;
  out.stay = true;
  var unsatisfied = new OrderedCollection();
  var todo = new OrderedCollection();
  todo.add(out);
  while (todo.size() > 0) {
    var v = todo.removeFirst();
    for (var i = 0; i < v.constraints.size(); i++) {
      var c = v.constraints.at(i);
      if (!c.isSatisfied())
        unsatisfied.add(c);
    }
    var determining = v.determinedBy;
    for (var i = 0; i < v.constraints.size(); i++) {
      var next = v.constraints.at(i);
      if (next != determining && next.isSatisfied()) {
        next.recalculate();
        todo.add(next.output());
      }
    }
  }
  return unsatisfied;
}

Planner.prototype.addConstraintsConsumingTo = function (v, coll) {
  var determining = v.determinedBy;
  var cc = v.constraints;
  for (var i = 0; i < cc.size(); i++) {
    var c = cc.at(i);
    if (c != determining && c.isSatisfied())
      coll.add(c);
  }
}

/* --- *
 * P l a n
 * --- */

/**
 * A Plan is an ordered list of constraints to be executed in sequence
 * to resatisfy all currently satisfiable constraints in the face of
 * one or more changing inputs.
 */
function Plan() {
  this.v = new OrderedCollection();
}

Plan.prototype.addConstraint = function (c) {
  this.v.add(c);
}

Plan.prototype.size = function () {
  return this.v.size();
}

Plan.prototype.constraintAt = function (index) {
  return this.v.at(index);
}

Plan.prototype.execute = function () {
  for (var i = 0; i < this.size(); i++) {
    var c = this.constraintAt(i);
    c.execute();
  }
}

/* --- *
 * M a i n
 * --- */

/**
 * This is the standard DeltaBlue benchmark. A long chain of equality
 * constraints is constructed with a stay constraint on one end. An
 * edit constraint is then added to the opposite end and the time is
 * measured for adding and removing this constraint, and extracting
 * and executing a constraint satisfaction plan. There are two cases.
 * In case 1, the added constraint is stronger than the stay
 * constraint and values must propagate down the entire length of the
 * chain. In case 2, the added constraint is weaker than the stay
 * constraint so it cannot be accommodated. The cost in this case is,
 * of course, very low. Typical situations lie somewhere between these
 * two extremes.
 */
function chainTest(n) {
  planner = new Planner();
  var prev = null, first = null, last = null;

  // Build chain of n equality constraints
  for (var i = 0; i <= n; i++) {
    var name = "v" + i;
    var v = new Variable(name);
    if (prev != null)
      new EqualityConstraint(prev, v, Strength.REQUIRED);
    if (i == 0) first = v;
    if (i == n) last = v;
    prev = v;
  }

  new StayConstraint(last, Strength.STRONG_DEFAULT);
  var edit = new EditConstraint(first, Strength.PREFERRED);
  var edits = new OrderedCollection();
  edits.add(edit);
  var plan = planner.extractPlanFromConstraints(edits);
  for (var i = 0; i < 100; i++) {
    first.value = i;
    plan.execute();
    if (last.value != i)
      alert("Chain test failed.");
  }
}

/**
 * This test constructs a two sets of variables related to each
 * other by a simple linear transformation (scale and offset). The
 * time is measured to change a variable on either side of the
 * mapping and to change the scale and offset factors.
 */
function projectionTest(n) {
  planner = new Planner();
  var scale = new Variable("scale", 10);
  var offset = new Variable("offset", 1000);
  var src = null, dst = null;

  var dests = new OrderedCollection();
  for (var i = 0; i < n; i++) {
    src = new Variable("src" + i, i);
    dst = new Variable("dst" + i, i);
    dests.add(dst);
    new StayConstraint(src, Strength.NORMAL);
    new ScaleConstraint(src, scale, offset, dst, Strength.REQUIRED);
  }

  change(src, 17);
  if (dst.value != 1170) alert("Projection 1 failed");
  change(dst, 1050);
  if (src.value != 5) alert("Projection 2 failed");
  change(scale, 5);
  for (var i = 0; i < n - 1; i++) {
    if (dests.at(i).value != i * 5 + 1000)
      alert("Projection 3 failed");
  }
  change(offset, 2000);
  for (var i = 0; i < n - 1; i++) {
    if (dests.at(i).value != i * 5 + 2000)
      alert("Projection 4 failed");
  }
}

function change(v, newValue) {
  var edit = new EditConstraint(v, Strength.PREFERRED);
  var edits = new OrderedCollection();
  edits.add(edit);
  var plan = planner.extractPlanFromConstraints(edits);
  for (var i = 0; i < 10; i++) {
    v.value = newValue;
    plan.execute();
  }
  edit.destroyConstraint();
}

// Global variable holding the current planner.
var planner = null;

function deltaBlue() {
  for (let i = 0; i < 20; ++i) {
    chainTest(100);
    projectionTest(100);
  }
}
)JS_CODE";

constexpr std::string_view kHandlerNameJetstreamDeltaBlue = "deltaBlue";

constexpr std::string_view kCodeJetstreamCryptoAes = R"JS_CODE(
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/*
 * AES Cipher function: encrypt 'input' with Rijndael algorithm
 *
 *   takes   byte-array 'input' (16 bytes)
 *           2D byte-array key schedule 'w' (Nr+1 x Nb bytes)
 *
 *   applies Nr rounds (10/12/14) using key schedule w for 'add round key' stage
 *
 *   returns byte-array encrypted value (16 bytes)
 */
function Cipher(input, w) {    // main Cipher function [Â§5.1]
  var Nb = 4;               // block size (in words): no of columns in state (fixed at 4 for AES)
  var Nr = w.length/Nb - 1; // no of rounds: 10/12/14 for 128/192/256-bit keys

  var state = [[],[],[],[]];  // initialise 4xNb byte-array 'state' with input [Â§3.4]
  for (var i=0; i<4*Nb; i++) state[i%4][Math.floor(i/4)] = input[i];

  state = AddRoundKey(state, w, 0, Nb);

  for (var round=1; round<Nr; round++) {
    state = SubBytes(state, Nb);
    state = ShiftRows(state, Nb);
    state = MixColumns(state, Nb);
    state = AddRoundKey(state, w, round, Nb);
  }

  state = SubBytes(state, Nb);
  state = ShiftRows(state, Nb);
  state = AddRoundKey(state, w, Nr, Nb);

  var output = new Array(4*Nb);  // convert state to 1-d array before returning [Â§3.4]
  for (var i=0; i<4*Nb; i++) output[i] = state[i%4][Math.floor(i/4)];
  return output;
}


function SubBytes(s, Nb) {    // apply SBox to state S [Â§5.1.1]
  for (var r=0; r<4; r++) {
    for (var c=0; c<Nb; c++) s[r][c] = Sbox[s[r][c]];
  }
  return s;
}


function ShiftRows(s, Nb) {    // shift row r of state S left by r bytes [Â§5.1.2]
  var t = new Array(4);
  for (var r=1; r<4; r++) {
    for (var c=0; c<4; c++) t[c] = s[r][(c+r)%Nb];  // shift into temp copy
    for (var c=0; c<4; c++) s[r][c] = t[c];         // and copy back
  }          // note that this will work for Nb=4,5,6, but not 7,8 (always 4 for AES):
  return s;  // see fp.gladman.plus.com/cryptography_technology/rijndael/aes.spec.311.pdf
}


function MixColumns(s, Nb) {   // combine bytes of each col of state S [Â§5.1.3]
  for (var c=0; c<4; c++) {
    var a = new Array(4);  // 'a' is a copy of the current column from 's'
    var b = new Array(4);
    for (var i=0; i<4; i++) {
      a[i] = s[i][c];
      b[i] = s[i][c]&0x80 ? s[i][c]<<1 ^ 0x011b : s[i][c]<<1;
    }
    s[0][c] = b[0] ^ a[1] ^ b[1] ^ a[2] ^ a[3]; // 2*a0 + 3*a1 + a2 + a3
    s[1][c] = a[0] ^ b[1] ^ a[2] ^ b[2] ^ a[3]; // a0 * 2*a1 + 3*a2 + a3
    s[2][c] = a[0] ^ a[1] ^ b[2] ^ a[3] ^ b[3]; // a0 + a1 + 2*a2 + 3*a3
    s[3][c] = a[0] ^ b[0] ^ a[1] ^ a[2] ^ b[3]; // 3*a0 + a1 + a2 + 2*a3
  }
  return s;
}


function AddRoundKey(state, w, rnd, Nb) {  // xor Round Key into state S [Â§5.1.4]
  for (var r=0; r<4; r++) {
    for (var c=0; c<Nb; c++) state[r][c] ^= w[rnd*4+c][r];
  }
  return state;
}


function KeyExpansion(key) {  // generate Key Schedule (byte-array Nr+1 x Nb) from Key [Â§5.2]
  var Nb = 4;            // block size (in words): no of columns in state (fixed at 4 for AES)
  var Nk = key.length/4  // key length (in words): 4/6/8 for 128/192/256-bit keys
  var Nr = Nk + 6;       // no of rounds: 10/12/14 for 128/192/256-bit keys

  var w = new Array(Nb*(Nr+1));
  var temp = new Array(4);

  for (var i=0; i<Nk; i++) {
    var r = [key[4*i], key[4*i+1], key[4*i+2], key[4*i+3]];
    w[i] = r;
  }

  for (var i=Nk; i<(Nb*(Nr+1)); i++) {
    w[i] = new Array(4);
    for (var t=0; t<4; t++) temp[t] = w[i-1][t];
    if (i % Nk == 0) {
      temp = SubWord(RotWord(temp));
      for (var t=0; t<4; t++) temp[t] ^= Rcon[i/Nk][t];
    } else if (Nk > 6 && i%Nk == 4) {
      temp = SubWord(temp);
    }
    for (var t=0; t<4; t++) w[i][t] = w[i-Nk][t] ^ temp[t];
  }

  return w;
}

function SubWord(w) {    // apply SBox to 4-byte word w
  for (var i=0; i<4; i++) w[i] = Sbox[w[i]];
  return w;
}

function RotWord(w) {    // rotate 4-byte word w left by one byte
  w[4] = w[0];
  for (var i=0; i<4; i++) w[i] = w[i+1];
  return w;
}


// Sbox is pre-computed multiplicative inverse in GF(2^8) used in SubBytes and KeyExpansion [Â§5.1.1]
var Sbox =  [0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
             0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
             0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
             0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
             0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
             0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
             0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
             0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
             0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
             0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
             0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
             0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
             0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
             0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
             0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
             0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16];

// Rcon is Round Constant used for the Key Expansion [1st col is 2^(r-1) in GF(2^8)] [Â§5.2]
var Rcon = [ [0x00, 0x00, 0x00, 0x00],
             [0x01, 0x00, 0x00, 0x00],
             [0x02, 0x00, 0x00, 0x00],
             [0x04, 0x00, 0x00, 0x00],
             [0x08, 0x00, 0x00, 0x00],
             [0x10, 0x00, 0x00, 0x00],
             [0x20, 0x00, 0x00, 0x00],
             [0x40, 0x00, 0x00, 0x00],
             [0x80, 0x00, 0x00, 0x00],
             [0x1b, 0x00, 0x00, 0x00],
             [0x36, 0x00, 0x00, 0x00] ];


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/*
 * Use AES to encrypt 'plaintext' with 'password' using 'nBits' key, in 'Counter' mode of operation
 *                           - see http://csrc.nist.gov/publications/nistpubs/800-38a/sp800-38a.pdf
 *   for each block
 *   - outputblock = cipher(counter, key)
 *   - cipherblock = plaintext xor outputblock
 */
function AESEncryptCtr(plaintext, password, nBits) {
  if (!(nBits==128 || nBits==192 || nBits==256)) return '';  // standard allows 128/192/256 bit keys

  // for this example script, generate the key by applying Cipher to 1st 16/24/32 chars of password;
  // for real-world applications, a more secure approach would be to hash the password e.g. with SHA-1
  var nBytes = nBits/8;  // no bytes in key
  var pwBytes = new Array(nBytes);
  for (var i=0; i<nBytes; i++) pwBytes[i] = password.charCodeAt(i) & 0xff;
  var key = Cipher(pwBytes, KeyExpansion(pwBytes));
  key = key.concat(key.slice(0, nBytes-16));  // key is now 16/24/32 bytes long

  // initialise counter block (NIST SP800-38A Â§B.2): millisecond time-stamp for nonce in 1st 8 bytes,
  // block counter in 2nd 8 bytes
  var blockSize = 16;  // block size fixed at 16 bytes / 128 bits (Nb=4) for AES
  var counterBlock = new Array(blockSize);  // block size fixed at 16 bytes / 128 bits (Nb=4) for AES
  var nonce = (new Date()).getTime();  // milliseconds since 1-Jan-1970

  // encode nonce in two stages to cater for JavaScript 32-bit limit on bitwise ops
  for (var i=0; i<4; i++) counterBlock[i] = (nonce >>> i*8) & 0xff;
  for (var i=0; i<4; i++) counterBlock[i+4] = (nonce/0x100000000 >>> i*8) & 0xff;

  // generate key schedule - an expansion of the key into distinct Key Rounds for each round
  var keySchedule = KeyExpansion(key);

  var blockCount = Math.ceil(plaintext.length/blockSize);
  var ciphertext = new Array(blockCount);  // ciphertext as array of strings

  for (var b=0; b<blockCount; b++) {
    // set counter (block #) in last 8 bytes of counter block (leaving nonce in 1st 8 bytes)
    // again done in two stages for 32-bit ops
    for (var c=0; c<4; c++) counterBlock[15-c] = (b >>> c*8) & 0xff;
    for (var c=0; c<4; c++) counterBlock[15-c-4] = (b/0x100000000 >>> c*8)

    var cipherCntr = Cipher(counterBlock, keySchedule);  // -- encrypt counter block --

    // calculate length of final block:
    var blockLength = b<blockCount-1 ? blockSize : (plaintext.length-1)%blockSize+1;

    var ct = '';
    for (var i=0; i<blockLength; i++) {  // -- xor plaintext with ciphered counter byte-by-byte --
      var plaintextByte = plaintext.charCodeAt(b*blockSize+i);
      var cipherByte = plaintextByte ^ cipherCntr[i];
      ct += String.fromCharCode(cipherByte);
    }
    // ct is now ciphertext for this block

    ciphertext[b] = escCtrlChars(ct);  // escape troublesome characters in ciphertext
  }

  // convert the nonce to a string to go on the front of the ciphertext
  var ctrTxt = '';
  for (var i=0; i<8; i++) ctrTxt += String.fromCharCode(counterBlock[i]);
  ctrTxt = escCtrlChars(ctrTxt);

  // use '-' to separate blocks, use Array.join to concatenate arrays of strings for efficiency
  return ctrTxt + '-' + ciphertext.join('-');
}


/*
 * Use AES to decrypt 'ciphertext' with 'password' using 'nBits' key, in Counter mode of operation
 *
 *   for each block
 *   - outputblock = cipher(counter, key)
 *   - cipherblock = plaintext xor outputblock
 */
function AESDecryptCtr(ciphertext, password, nBits) {
  if (!(nBits==128 || nBits==192 || nBits==256)) return '';  // standard allows 128/192/256 bit keys

  var nBytes = nBits/8;  // no bytes in key
  var pwBytes = new Array(nBytes);
  for (var i=0; i<nBytes; i++) pwBytes[i] = password.charCodeAt(i) & 0xff;
  var pwKeySchedule = KeyExpansion(pwBytes);
  var key = Cipher(pwBytes, pwKeySchedule);
  key = key.concat(key.slice(0, nBytes-16));  // key is now 16/24/32 bytes long

  var keySchedule = KeyExpansion(key);

  ciphertext = ciphertext.split('-');  // split ciphertext into array of block-length strings

  // recover nonce from 1st element of ciphertext
  var blockSize = 16;  // block size fixed at 16 bytes / 128 bits (Nb=4) for AES
  var counterBlock = new Array(blockSize);
  var ctrTxt = unescCtrlChars(ciphertext[0]);
  for (var i=0; i<8; i++) counterBlock[i] = ctrTxt.charCodeAt(i);

  var plaintext = new Array(ciphertext.length-1);

  for (var b=1; b<ciphertext.length; b++) {
    // set counter (block #) in last 8 bytes of counter block (leaving nonce in 1st 8 bytes)
    for (var c=0; c<4; c++) counterBlock[15-c] = ((b-1) >>> c*8) & 0xff;
    for (var c=0; c<4; c++) counterBlock[15-c-4] = ((b/0x100000000-1) >>> c*8) & 0xff;

    var cipherCntr = Cipher(counterBlock, keySchedule);  // encrypt counter block

    ciphertext[b] = unescCtrlChars(ciphertext[b]);

    var pt = '';
    for (var i=0; i<ciphertext[b].length; i++) {
      // -- xor plaintext with ciphered counter byte-by-byte --
      var ciphertextByte = ciphertext[b].charCodeAt(i);
      var plaintextByte = ciphertextByte ^ cipherCntr[i];
      pt += String.fromCharCode(plaintextByte);
    }
    // pt is now plaintext for this block

    plaintext[b-1] = pt;  // b-1 'cos no initial nonce block in plaintext
  }

  return plaintext.join('');
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

function escCtrlChars(str) {  // escape control chars which might cause problems handling ciphertext
  return str.replace(/[\0\t\n\v\f\r\xa0'"!-]/g, function(c) { return '!' + c.charCodeAt(0) + '!'; });
}  // \xa0 to cater for bug in Firefox; include '-' to leave it free for use as a block marker

function unescCtrlChars(str) {  // unescape potentially problematic control characters
  return str.replace(/!\d\d?\d?!/g, function(c) { return String.fromCharCode(c.slice(1,-1)); });
}
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

/*
 * if escCtrlChars()/unescCtrlChars() still gives problems, use encodeBase64()/decodeBase64() instead
 */
var b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

function encodeBase64(str) {  // http://tools.ietf.org/html/rfc4648
   var o1, o2, o3, h1, h2, h3, h4, bits, i=0, enc='';

   str = encodeUTF8(str);  // encode multi-byte chars into UTF-8 for byte-array

   do {  // pack three octets into four hexets
      o1 = str.charCodeAt(i++);
      o2 = str.charCodeAt(i++);
      o3 = str.charCodeAt(i++);

      bits = o1<<16 | o2<<8 | o3;

      h1 = bits>>18 & 0x3f;
      h2 = bits>>12 & 0x3f;
      h3 = bits>>6 & 0x3f;
      h4 = bits & 0x3f;

      // end of string? index to '=' in b64
      if (isNaN(o3)) h4 = 64;
      if (isNaN(o2)) h3 = 64;

      // use hexets to index into b64, and append result to encoded string
      enc += b64.charAt(h1) + b64.charAt(h2) + b64.charAt(h3) + b64.charAt(h4);
   } while (i < str.length);

   return enc;
}

function decodeBase64(str) {
   var o1, o2, o3, h1, h2, h3, h4, bits, i=0, enc='';

   do {  // unpack four hexets into three octets using index points in b64
      h1 = b64.indexOf(str.charAt(i++));
      h2 = b64.indexOf(str.charAt(i++));
      h3 = b64.indexOf(str.charAt(i++));
      h4 = b64.indexOf(str.charAt(i++));

      bits = h1<<18 | h2<<12 | h3<<6 | h4;

      o1 = bits>>16 & 0xff;
      o2 = bits>>8 & 0xff;
      o3 = bits & 0xff;

      if (h3 == 64)      enc += String.fromCharCode(o1);
      else if (h4 == 64) enc += String.fromCharCode(o1, o2);
      else               enc += String.fromCharCode(o1, o2, o3);
   } while (i < str.length);

   return decodeUTF8(enc);  // decode UTF-8 byte-array back to Unicode
}

function encodeUTF8(str) {  // encode multi-byte string into utf-8 multiple single-byte characters
  str = str.replace(
      /[\u0080-\u07ff]/g,  // U+0080 - U+07FF = 2-byte chars
      function(c) {
        var cc = c.charCodeAt(0);
        return String.fromCharCode(0xc0 | cc>>6, 0x80 | cc&0x3f); }
    );
  str = str.replace(
      /[\u0800-\uffff]/g,  // U+0800 - U+FFFF = 3-byte chars
      function(c) {
        var cc = c.charCodeAt(0);
        return String.fromCharCode(0xe0 | cc>>12, 0x80 | cc>>6&0x3F, 0x80 | cc&0x3f); }
    );
  return str;
}

function decodeUTF8(str) {  // decode utf-8 encoded string back into multi-byte characters
  str = str.replace(
      /[\u00c0-\u00df][\u0080-\u00bf]/g,                 // 2-byte chars
      function(c) {
        var cc = (c.charCodeAt(0)&0x1f)<<6 | c.charCodeAt(1)&0x3f;
        return String.fromCharCode(cc); }
    );
  str = str.replace(
      /[\u00e0-\u00ef][\u0080-\u00bf][\u0080-\u00bf]/g,  // 3-byte chars
      function(c) {
        var cc = (c.charCodeAt(0)&0x0f)<<12 | (c.charCodeAt(1)&0x3f<<6) | c.charCodeAt(2)&0x3f;
        return String.fromCharCode(cc); }
    );
  return str;
}


function byteArrayToHexStr(b) {  // convert byte array to hex string for displaying test vectors
  var s = '';
  for (var i=0; i<b.length; i++) s += b[i].toString(16) + ' ';
  return s;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

function run() {
    var plainText =
"ROMEO: But, soft! what light through yonder window breaks?\n\
It is the east, and Juliet is the sun.\n\
Arise, fair sun, and kill the envious moon,\n\
Who is already sick and pale with grief,\n\
That thou her maid art far more fair than she:\n\
Be not her maid, since she is envious;\n\
Her vestal livery is but sick and green\n\
And none but fools do wear it; cast it off.\n\
It is my lady, O, it is my love!\n\
O, that she knew she were!\n\
She speaks yet she says nothing: what of that?\n\
Her eye discourses; I will answer it.\n\
I am too bold, 'tis not to me she speaks:\n\
Two of the fairest stars in all the heaven,\n\
Having some business, do entreat her eyes\n\
To twinkle in their spheres till they return.\n\
What if her eyes were there, they in her head?\n\
The brightness of her cheek would shame those stars,\n\
As daylight doth a lamp; her eyes in heaven\n\
Would through the airy region stream so bright\n\
That birds would sing and think it were not night.\n\
See, how she leans her cheek upon her hand!\n\
O, that I were a glove upon that hand,\n\
That I might touch that cheek!\n\
JULIET: Ay me!\n\
ROMEO: She speaks:\n\
O, speak again, bright angel! for thou art\n\
As glorious to this night, being o'er my head\n\
As is a winged messenger of heaven\n\
Unto the white-upturned wondering eyes\n\
Of mortals that fall back to gaze on him\n\
When he bestrides the lazy-pacing clouds\n\
And sails upon the bosom of the air.";

    var password = "O Romeo, Romeo! wherefore art thou Romeo?";

    var cipherText = AESEncryptCtr(plainText, password, 256);
    var decryptedText = AESDecryptCtr(cipherText, password, 256);

    if (decryptedText != plainText)
        throw "ERROR: bad result: expected " + plainText + " but got " + decryptedText;
}


function codec() {
  for (let i = 0; i < 8; ++i)
      run();
}
)JS_CODE";

constexpr std::string_view kHandlerNameJetstreamCryptoAes = "codec";

constexpr std::string_view kCodeJetstreamNavierStokes = R"JS_CODE(
/**
 * Copyright 2013 the V8 project authors. All rights reserved.
 * Copyright 2009 Oliver Hunt <http://nerget.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Update 10/21/2013: fixed loop variables at line 119
 */

var solver = null;
var nsFrameCounter = 0;

function runNavierStokes()
{
    solver.update();
    nsFrameCounter++;

    if(nsFrameCounter==15)
        checkResult(solver.getDens());
}

function checkResult(dens) {

    this.result = 0;
    for (var i=7000;i<7100;i++) {
        this.result+=~~((dens[i]*10));
    }

    if (this.result!=77) {
        throw(new Error("checksum failed"));
    }
}

function setupNavierStokes()
{
    solver = new FluidField(null);
    solver.setResolution(128, 128);
    solver.setIterations(20);
    solver.setDisplayFunction(function(){});
    solver.setUICallback(prepareFrame);
    solver.reset();
}

function tearDownNavierStokes()
{
    solver = null;
}

function addPoints(field) {
    var n = 64;
    for (var i = 1; i <= n; i++) {
        field.setVelocity(i, i, n, n);
        field.setDensity(i, i, 5);
        field.setVelocity(i, n - i, -n, -n);
        field.setDensity(i, n - i, 20);
        field.setVelocity(128 - i, n + i, -n, -n);
        field.setDensity(128 - i, n + i, 30);
    }
}

var framesTillAddingPoints = 0;
var framesBetweenAddingPoints = 5;

function prepareFrame(field)
{
    if (framesTillAddingPoints == 0) {
        addPoints(field);
        framesTillAddingPoints = framesBetweenAddingPoints;
        framesBetweenAddingPoints++;
    } else {
        framesTillAddingPoints--;
    }
}

// Code from Oliver Hunt (http://nerget.com/fluidSim/pressure.js) starts here.
function FluidField(canvas) {
    function addFields(x, s, dt)
    {
        for (var i=0; i<size ; i++ ) x[i] += dt*s[i];
    }

    function set_bnd(b, x)
    {
        if (b===1) {
            for (var i = 1; i <= width; i++) {
                x[i] =  x[i + rowSize];
                x[i + (height+1) *rowSize] = x[i + height * rowSize];
            }

            for (var j = 1; j <= height; j++) {
                x[j * rowSize] = -x[1 + j * rowSize];
                x[(width + 1) + j * rowSize] = -x[width + j * rowSize];
            }
        } else if (b === 2) {
            for (var i = 1; i <= width; i++) {
                x[i] = -x[i + rowSize];
                x[i + (height + 1) * rowSize] = -x[i + height * rowSize];
            }

            for (var j = 1; j <= height; j++) {
                x[j * rowSize] =  x[1 + j * rowSize];
                x[(width + 1) + j * rowSize] =  x[width + j * rowSize];
            }
        } else {
            for (var i = 1; i <= width; i++) {
                x[i] =  x[i + rowSize];
                x[i + (height + 1) * rowSize] = x[i + height * rowSize];
            }

            for (var j = 1; j <= height; j++) {
                x[j * rowSize] =  x[1 + j * rowSize];
                x[(width + 1) + j * rowSize] =  x[width + j * rowSize];
            }
        }
        var maxEdge = (height + 1) * rowSize;
        x[0]                 = 0.5 * (x[1] + x[rowSize]);
        x[maxEdge]           = 0.5 * (x[1 + maxEdge] + x[height * rowSize]);
        x[(width+1)]         = 0.5 * (x[width] + x[(width + 1) + rowSize]);
        x[(width+1)+maxEdge] = 0.5 * (x[width + maxEdge] + x[(width + 1) + height * rowSize]);
    }

    function lin_solve(b, x, x0, a, c)
    {
        if (a === 0 && c === 1) {
            for (var j=1 ; j<=height; j++) {
                var currentRow = j * rowSize;
                ++currentRow;
                for (var i = 0; i < width; i++) {
                    x[currentRow] = x0[currentRow];
                    ++currentRow;
                }
            }
            set_bnd(b, x);
        } else {
            var invC = 1 / c;
            for (var k=0 ; k<iterations; k++) {
                for (var j=1 ; j<=height; j++) {
                    var lastRow = (j - 1) * rowSize;
                    var currentRow = j * rowSize;
                    var nextRow = (j + 1) * rowSize;
                    var lastX = x[currentRow];
                    ++currentRow;
                    for (var i=1; i<=width; i++)
                        lastX = x[currentRow] = (x0[currentRow] + a*(lastX+x[++currentRow]+x[++lastRow]+x[++nextRow])) * invC;
                }
                set_bnd(b, x);
            }
        }
    }

    function diffuse(b, x, x0, dt)
    {
        var a = 0;
        lin_solve(b, x, x0, a, 1 + 4*a);
    }

    function lin_solve2(x, x0, y, y0, a, c)
    {
        if (a === 0 && c === 1) {
            for (var j=1 ; j <= height; j++) {
                var currentRow = j * rowSize;
                ++currentRow;
                for (var i = 0; i < width; i++) {
                    x[currentRow] = x0[currentRow];
                    y[currentRow] = y0[currentRow];
                    ++currentRow;
                }
            }
            set_bnd(1, x);
            set_bnd(2, y);
        } else {
            var invC = 1/c;
            for (var k=0 ; k<iterations; k++) {
                for (var j=1 ; j <= height; j++) {
                    var lastRow = (j - 1) * rowSize;
                    var currentRow = j * rowSize;
                    var nextRow = (j + 1) * rowSize;
                    var lastX = x[currentRow];
                    var lastY = y[currentRow];
                    ++currentRow;
                    for (var i = 1; i <= width; i++) {
                        lastX = x[currentRow] = (x0[currentRow] + a * (lastX + x[currentRow] + x[lastRow] + x[nextRow])) * invC;
                        lastY = y[currentRow] = (y0[currentRow] + a * (lastY + y[++currentRow] + y[++lastRow] + y[++nextRow])) * invC;
                    }
                }
                set_bnd(1, x);
                set_bnd(2, y);
            }
        }
    }

    function diffuse2(x, x0, y, y0, dt)
    {
        var a = 0;
        lin_solve2(x, x0, y, y0, a, 1 + 4 * a);
    }

    function advect(b, d, d0, u, v, dt)
    {
        var Wdt0 = dt * width;
        var Hdt0 = dt * height;
        var Wp5 = width + 0.5;
        var Hp5 = height + 0.5;
        for (var j = 1; j<= height; j++) {
            var pos = j * rowSize;
            for (var i = 1; i <= width; i++) {
                var x = i - Wdt0 * u[++pos];
                var y = j - Hdt0 * v[pos];
                if (x < 0.5)
                    x = 0.5;
                else if (x > Wp5)
                    x = Wp5;
                var i0 = x | 0;
                var i1 = i0 + 1;
                if (y < 0.5)
                    y = 0.5;
                else if (y > Hp5)
                    y = Hp5;
                var j0 = y | 0;
                var j1 = j0 + 1;
                var s1 = x - i0;
                var s0 = 1 - s1;
                var t1 = y - j0;
                var t0 = 1 - t1;
                var row1 = j0 * rowSize;
                var row2 = j1 * rowSize;
                d[pos] = s0 * (t0 * d0[i0 + row1] + t1 * d0[i0 + row2]) + s1 * (t0 * d0[i1 + row1] + t1 * d0[i1 + row2]);
            }
        }
        set_bnd(b, d);
    }

    function project(u, v, p, div)
    {
        var h = -0.5 / Math.sqrt(width * height);
        for (var j = 1 ; j <= height; j++ ) {
            var row = j * rowSize;
            var previousRow = (j - 1) * rowSize;
            var prevValue = row - 1;
            var currentRow = row;
            var nextValue = row + 1;
            var nextRow = (j + 1) * rowSize;
            for (var i = 1; i <= width; i++ ) {
                div[++currentRow] = h * (u[++nextValue] - u[++prevValue] + v[++nextRow] - v[++previousRow]);
                p[currentRow] = 0;
            }
        }
        set_bnd(0, div);
        set_bnd(0, p);

        lin_solve(0, p, div, 1, 4 );
        var wScale = 0.5 * width;
        var hScale = 0.5 * height;
        for (var j = 1; j<= height; j++ ) {
            var prevPos = j * rowSize - 1;
            var currentPos = j * rowSize;
            var nextPos = j * rowSize + 1;
            var prevRow = (j - 1) * rowSize;
            var currentRow = j * rowSize;
            var nextRow = (j + 1) * rowSize;

            for (var i = 1; i<= width; i++) {
                u[++currentPos] -= wScale * (p[++nextPos] - p[++prevPos]);
                v[currentPos]   -= hScale * (p[++nextRow] - p[++prevRow]);
            }
        }
        set_bnd(1, u);
        set_bnd(2, v);
    }

    function dens_step(x, x0, u, v, dt)
    {
        addFields(x, x0, dt);
        diffuse(0, x0, x, dt );
        advect(0, x, x0, u, v, dt );
    }

    function vel_step(u, v, u0, v0, dt)
    {
        addFields(u, u0, dt );
        addFields(v, v0, dt );
        var temp = u0; u0 = u; u = temp;
        var temp = v0; v0 = v; v = temp;
        diffuse2(u,u0,v,v0, dt);
        project(u, v, u0, v0);
        var temp = u0; u0 = u; u = temp;
        var temp = v0; v0 = v; v = temp;
        advect(1, u, u0, u0, v0, dt);
        advect(2, v, v0, u0, v0, dt);
        project(u, v, u0, v0 );
    }
    var uiCallback = function(d,u,v) {};

    function Field(dens, u, v) {
        // Just exposing the fields here rather than using accessors is a measurable win during display (maybe 5%)
        // but makes the code ugly.
        this.setDensity = function(x, y, d) {
             dens[(x + 1) + (y + 1) * rowSize] = d;
        }
        this.getDensity = function(x, y) {
             return dens[(x + 1) + (y + 1) * rowSize];
        }
        this.setVelocity = function(x, y, xv, yv) {
             u[(x + 1) + (y + 1) * rowSize] = xv;
             v[(x + 1) + (y + 1) * rowSize] = yv;
        }
        this.getXVelocity = function(x, y) {
             return u[(x + 1) + (y + 1) * rowSize];
        }
        this.getYVelocity = function(x, y) {
             return v[(x + 1) + (y + 1) * rowSize];
        }
        this.width = function() { return width; }
        this.height = function() { return height; }
    }
    function queryUI(d, u, v)
    {
        for (var i = 0; i < size; i++)
            u[i] = v[i] = d[i] = 0.0;
        uiCallback(new Field(d, u, v));
    }

    this.update = function () {
        queryUI(dens_prev, u_prev, v_prev);
        vel_step(u, v, u_prev, v_prev, dt);
        dens_step(dens, dens_prev, u, v, dt);
        displayFunc(new Field(dens, u, v));
    }
    this.setDisplayFunction = function(func) {
        displayFunc = func;
    }

    this.iterations = function() { return iterations; }
    this.setIterations = function(iters) {
        if (iters > 0 && iters <= 100)
           iterations = iters;
    }
    this.setUICallback = function(callback) {
        uiCallback = callback;
    }
    var iterations = 10;
    var visc = 0.5;
    var dt = 0.1;
    var dens;
    var dens_prev;
    var u;
    var u_prev;
    var v;
    var v_prev;
    var width;
    var height;
    var rowSize;
    var size;
    var displayFunc;
    function reset()
    {
        rowSize = width + 2;
        size = (width+2)*(height+2);
        dens = new Array(size);
        dens_prev = new Array(size);
        u = new Array(size);
        u_prev = new Array(size);
        v = new Array(size);
        v_prev = new Array(size);
        for (var i = 0; i < size; i++)
            dens_prev[i] = u_prev[i] = v_prev[i] = dens[i] = u[i] = v[i] = 0;
    }
    this.reset = reset;
    this.getDens = function()
    {
        return dens;
    }
    this.setResolution = function (hRes, wRes)
    {
        var res = wRes * hRes;
        if (res > 0 && res < 1000000 && (wRes != width || hRes != height)) {
            width = wRes;
            height = hRes;
            reset();
            return true;
        }
        return false;
    }
    this.setResolution(64, 64);
}

function NavierStokes() {
  setupNavierStokes();
  runNavierStokes();
}
)JS_CODE";

constexpr std::string_view kHandlerNameJetstreamNavierStokes = "NavierStokes";
}  // namespace google::scp::roma::benchmark

#endif  // ROMA_BENCHMARK_TEST_CODE_H_
