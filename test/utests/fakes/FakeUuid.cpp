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

#include <uuid/uuid.h>
#include <iostream>
using namespace std;

#if 0
#define TRACE_FUNC() printf("%s\n" ,__func__)
#else
#define TRACE_FUNC() ((void)0)
#endif

/**
 * @brief To generate UUID
 */
void uuid_generate(uuid_t out)
{
	TRACE_FUNC();
}

/**
 * @brief To parse UUID
 */
void uuid_unparse(const uuid_t uu, char *out)
{
	TRACE_FUNC();
}
