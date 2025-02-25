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

#include "PlayerUtils.h"
#include "MockPlayerUtils.h"
#include <thread>

MockPlayerUtils *g_mockPlayerUtils = nullptr;

char *base64_URL_Encode(const unsigned char *src, size_t len)
{
        return NULL;
}

unsigned char *base64_URL_Decode(const char *src, size_t *len, size_t srcLen)
{
        return NULL;
}
long long GetCurrentTimeMS(void)
{
        long long timeMS = 0;

        if (g_mockPlayerUtils)
        {
                timeMS = g_mockPlayerUtils->GetCurrentTimeMS();
        }

        return timeMS;
}

static std::hash<std::thread::id> std_thread_hasher;

std::size_t GetThreadID( const std::thread &t )
{
	return std_thread_hasher( t.get_id() );
}
std::size_t GetThreadID( void )
{
	return std_thread_hasher( std::this_thread::get_id() );
}
/**
 * @brief support for POSIX threads
 */
std::size_t GetThreadID( const pthread_t &t )
{
	static std::hash<pthread_t> pthread_hasher;
	return pthread_hasher( t );
}
