/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include <openssl/evp.h>

EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void)
{
	return NULL;
}

void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
{
}

#if OPENSSL_API_COMPAT < 0x10100000L
int EVP_CIPHER_CTX_reset(EVP_CIPHER_CTX *ctx)
{
	return 0;
}
#else
void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *a)
{
}

int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *a)
{
	return 0;
}
#endif

const EVP_CIPHER *EVP_aes_128_ctr(void)
{
	return NULL;
}

int EVP_DecryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, const unsigned char *key,
					const unsigned char *iv)
{
	return 0;
}

int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher, ENGINE *impl,
					   const unsigned char *key, const unsigned char *iv)
{
	return 0;
}

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl, const unsigned char *in,
					  int inl)
{
	return 0;
}

int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl)
{
	return 0;
}
