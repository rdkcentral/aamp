/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#include <cstddef>

// Minimal fake structs
struct EVP_CIPHER_CTX {};
struct EVP_CIPHER {};

// Stubbed functions
extern "C" {
    EVP_CIPHER_CTX* EVP_CIPHER_CTX_new() { return new EVP_CIPHER_CTX(); }
    void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX* ctx) { delete ctx; }

    const EVP_CIPHER* EVP_aes_128_ctr() { return nullptr; }
    const EVP_CIPHER* EVP_aes_128_cbc() { return nullptr; }

    int EVP_DecryptInit_ex(EVP_CIPHER_CTX*, const EVP_CIPHER*, void*, const unsigned char*, const unsigned char*) { return 1; }
    int EVP_DecryptUpdate(EVP_CIPHER_CTX*, unsigned char*, int*, const unsigned char*, int) { return 1; }
    int EVP_DecryptFinal_ex(EVP_CIPHER_CTX*, unsigned char*, int*) { return 1; }
}
