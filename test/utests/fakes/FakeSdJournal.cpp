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


#define MAX_DEBUG_LOG_BUFF_SIZE 1024


MockSdJournal *g_mockSdJournal = nullptr;


int sd_journal_print_with_location(int priority, const char *file, const char *line, const char *func, const char *format, ...)
{
    int ret_val = -1;

    if (g_mockSdJournal != nullptr)
    {
        va_list arg;
        char buffer[MAX_DEBUG_LOG_BUFF_SIZE];

        va_start(arg, format);
        if (vsnprintf(buffer, MAX_DEBUG_LOG_BUFF_SIZE, format, arg) >= 0)
        {
            /* Print the message on the console before calling the mock. */
            printf("%s\n", buffer);
            ret_val = g_mockSdJournal->sd_journal_print_mock(priority, buffer);
        }
        va_end(arg);
    }

    return ret_val;
}
