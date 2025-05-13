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

#include <systemd/sd-journal.h>
#include "MockSdJournal.h"

#include <stdio.h>

#include <cstdarg>

MockSdJournal *g_mockSdJournal = nullptr;

int sd_journal_printv_with_location(int priority, const char *file, const char *line, const char *func, const char *format, va_list arg )
{ // truncated to LINE_MAX - 8
    int ret_val = -1;
    if (g_mockSdJournal != nullptr)
    {
		int buffer_len = 2040;
		char *buffer_ptr = (char *)calloc(buffer_len,1);
		if( buffer_ptr )
		{
			buffer_len = vsnprintf(buffer_ptr, buffer_len, format, arg);
			assert( buffer_len>0 );
			printf( "%s\n", buffer_ptr );
			ret_val = g_mockSdJournal->sd_journal_print_mock(priority, buffer_ptr);
			free( buffer_ptr );
		}
    }
    return ret_val;
}
