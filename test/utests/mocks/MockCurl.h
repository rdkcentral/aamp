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

#ifndef AAMP_MOCK_CURL_H
#define AAMP_MOCK_CURL_H

#include <gmock/gmock.h>
#include <curl/curl.h>


typedef int (*curl_progress_callback_t)(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
typedef int (*curl_write_func_t)(void *buffer, size_t sz, size_t nmemb, void *userdata);

class MockCurl
{
public:
	MOCK_METHOD(void, curl_easy_cleanup, (CURL *curl));
	MOCK_METHOD(CURLcode, curl_easy_getinfo_int, (CURL *curl, CURLINFO info, int *rc));
	MOCK_METHOD(CURL *, curl_easy_init, ());
	MOCK_METHOD(CURLcode, curl_easy_perform, (CURL *curl));
	MOCK_METHOD(CURLcode, curl_easy_setopt_func_write, (CURL *handle, CURLoption option, curl_write_func_t write_func));
	MOCK_METHOD(CURLcode, curl_easy_setopt_func_xferinfo, (CURL *handle, CURLoption option, curl_progress_callback_t progress_callback));
	MOCK_METHOD(CURLcode, curl_easy_setopt_ptr, (CURL *handle, CURLoption option, const void *ptr));
	MOCK_METHOD(CURLcode, curl_easy_setopt_str, (CURL *handle, CURLoption option, const char *str));
	MOCK_METHOD(CURLcode, curl_easy_setopt_long, (CURL *handle, CURLoption option, long value));
	MOCK_METHOD(CURLcode, curl_easy_setopt_slist, (CURL *handle, CURLoption option, struct curl_slist *list));
	MOCK_METHOD(char *, curl_easy_unescape, (CURL *curl, const char *url, int inlength, int *outlength));
	MOCK_METHOD(void, curl_free, (void *ptr));
};
extern MockCurl *g_mockCurl;

#endif /* AAMP_MOCK_CURL_H */
