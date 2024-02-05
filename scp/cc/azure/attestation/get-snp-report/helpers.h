/*
 * Portions Copyright (c) Microsoft Corporation
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

#pragma once

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "snp-attestation.h"

#define PRINT_VAL(ptr, field) \
  printBytes(#field, (const uint8_t*)&(ptr->field), sizeof(ptr->field), true)
#define PRINT_BYTES(ptr, field) \
  printBytes(#field, (const uint8_t*)&(ptr->field), sizeof(ptr->field), false)

// Helper functions
uint8_t* decodeHexString(const char* hexstring, size_t padTo);

char* encodeHexToString(uint8_t byte_array[], size_t len);

void printBytes(const char* desc, const uint8_t* data, size_t len, bool swap);

void printReport(const snp_attestation_report* r);
