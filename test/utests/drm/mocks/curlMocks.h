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

#ifndef CURL_MOCKS_H
#define CURL_MOCKS_H

#include <curl/curl.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef size_t (*MockCurlWriteCallback)(char *ptr, size_t size, size_t nmemb, void *userdata);
	typedef void (*MockCurlPerformCallback)(CURL *curl, MockCurlWriteCallback writeFunction,
											void *writeData, void *userData);

#define MOCK_CURL_MAX_HEADERS (10u)

	typedef struct _MockCurlOpts
	{
		MockCurlWriteCallback writeFunction;	  /* CURLOPT_WRITEFUNCTION */
		void *writeData;						  /* CURLOPT_WRITEDATA */
		char url[200];							  /* CURLOPT_URL */
		long httpGet;							  /* CURLOPT_HTTPGET */
		long postFieldSize;						  /* CURLOPT_POSTFIELDSIZE */
		char postFields[500];					  /* CURLOPT_POSTFIELDS */
		char headers[MOCK_CURL_MAX_HEADERS][200]; /* CURLOPT_HTTPHEADERS */
		unsigned int headerCount;
	} MockCurlOpts;

	void MockCurlSetPerformCallback(MockCurlPerformCallback mockPerformCallback, void *userData);

	void MockCurlReset(void);

	const MockCurlOpts *MockCurlGetOpts(void);

#ifdef __cplusplus
}
#endif

#endif /* CURL_MOCKS_H */
