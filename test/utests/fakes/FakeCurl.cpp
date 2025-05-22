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
*
* Fake implementations of APIs from Curl which is:
* Copyright (c) 1996 - 2022, Daniel Stenberg, daniel@haxx.se, and many contributors
* Licensed under the CURL License
*/

#include <curl/curl.h>
#include "MockCurl.h"

#include <cstdarg>


MockCurl *g_mockCurl = nullptr;


void curl_easy_cleanup(CURL *curl)
{
    if (g_mockCurl != nullptr)
    {
        g_mockCurl->curl_easy_cleanup(curl);
    }
}

CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...)
{
    CURLcode curl_code = CURLE_OK;

    if (g_mockCurl != nullptr)
    {
        va_list arg;
        int *rc;

        va_start(arg, info);
        rc = va_arg(arg, int *);

        switch (info)
        {
            case CURLINFO_RESPONSE_CODE:
                curl_code = g_mockCurl->curl_easy_getinfo_int(curl, info, rc);
                break;

            default:
                // New cases might be required in this switch
                break;
        }

        va_end(arg);
    }

    return curl_code;
}

CURL *curl_easy_init(void)
{
    CURL *curl_handle = nullptr;

    if (g_mockCurl != nullptr)
    {
        curl_handle = g_mockCurl->curl_easy_init();
    }

    return curl_handle;
}

CURLcode curl_easy_perform(CURL *curl)
{
    CURLcode curl_code = CURLE_OK;

    if (g_mockCurl != nullptr)
    {
        curl_code = g_mockCurl->curl_easy_perform(curl);
    }

    return curl_code;
}

CURLcode curl_easy_setopt(CURL *handle, CURLoption option, ...)
{
    CURLcode curl_code = CURLE_OK;

    if (g_mockCurl != nullptr)
    {
        va_list arg;

        va_start(arg, option);

        switch (option)
        {
            case CURLOPT_PROGRESSDATA:
            case CURLOPT_WRITEDATA:
            {
                const void *ptr = va_arg(arg, void *);
                curl_code = g_mockCurl->curl_easy_setopt_ptr(handle, option, ptr);
            }
            break;

            case CURLOPT_URL:
            {
                const char *str = va_arg(arg, char *);
                curl_code = g_mockCurl->curl_easy_setopt_str(handle, option, str);
            }
            break;

            case CURLOPT_WRITEFUNCTION:
            {
                typedef int (*write_func_t)(void *buffer, size_t sz, size_t nmemb, void *userdata);
                write_func_t func_ptr = va_arg(arg, write_func_t);
                curl_code = g_mockCurl->curl_easy_setopt_func_write(handle, option, func_ptr);
            }
            break;

            case CURLOPT_XFERINFOFUNCTION:
            {
                curl_progress_callback_t func_ptr = va_arg(arg, curl_progress_callback_t);
                curl_code = g_mockCurl->curl_easy_setopt_func_xferinfo(handle, option, func_ptr);
            }
            break;

            default:
                // New cases might be required in this switch
                break;
        }

        va_end(arg);
    }

    return curl_code;
}

const char *curl_easy_strerror(CURLcode errornum)
{
    return nullptr;
}

char *curl_easy_unescape(CURL *curl, const char *url,
                         int inlength, int *outlength)
{
    char *unescaped_url = nullptr;

    if (g_mockCurl != nullptr)
    {
        unescaped_url = g_mockCurl->curl_easy_unescape(curl, url, inlength, outlength);
    }

    return unescaped_url;
}

void curl_free(void *ptr)
{
    if (g_mockCurl != nullptr)
    {
        g_mockCurl->curl_free(ptr);
    }
}

CURLSHcode curl_share_cleanup(CURLSH *)
{
    return CURLSHE_OK;
}

struct curl_slist *curl_slist_append(struct curl_slist *,
                                     const char *)
{
    return nullptr;
}

void curl_slist_free_all(struct curl_slist *)
{
}
