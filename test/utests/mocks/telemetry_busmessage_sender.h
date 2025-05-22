/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

typedef enum
{
    T2ERROR_SUCCESS,
    T2ERROR_FAILURE,
    T2ERROR_INVALID_PROFILE,
    T2ERROR_PROFILE_NOT_FOUND,
    T2ERROR_PROFILE_NOT_SET,
    T2ERROR_MAX_PROFILES_REACHED,
    T2ERROR_MEMALLOC_FAILED,
    T2ERROR_INVALID_ARGS,
    T2ERROR_INTERNAL_ERROR,
    T2ERROR_NO_RBUS_METHOD_PROVIDER,
    T2ERROR_COMPONENT_NULL
}T2ERROR;


void t2_init(char *component) {}

T2ERROR t2_event_s(char* marker, char* value) { return T2ERROR_SUCCESS;} 

T2ERROR t2_event_f(char* marker, double value) { return T2ERROR_SUCCESS;} 

T2ERROR t2_event_d(char* marker, int value) { return T2ERROR_SUCCESS;} 

void t2_uninit(void) {}
