/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file aamp_kmp.h
 * @brief aamp_kmp header file
 */

#ifndef AAMPCLI_KMP_H
#define AAMPCLI_KMP_H

#ifdef __cplusplus
extern "C" {
#endif

void kmp_init(void);
void kmp_tune(const char *url);

#ifdef __cplusplus
}
#endif

#endif // AAMP_KMP_H
