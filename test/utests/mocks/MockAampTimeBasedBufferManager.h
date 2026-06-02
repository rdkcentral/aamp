/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#ifndef AAMP_MOCK_TIME_BASED_BUFFER_MANAGER_H
#define AAMP_MOCK_TIME_BASED_BUFFER_MANAGER_H

#include <gmock/gmock.h>
#include <memory>

namespace aamp
{
	class MockAampTimeBasedBufferManager
	{
	public:
		MOCK_METHOD(void, PopulateBuffer, (double fragmentDuration));
		MOCK_METHOD(void, ConsumeBuffer, (double timeToConsume));
		MOCK_METHOD(bool, IsFull, (), (const));
		MOCK_METHOD(void, ClearBuffer, ());
	};
}

extern std::shared_ptr<aamp::MockAampTimeBasedBufferManager> g_mockAampTimeBasedBufferManager;

#endif /* AAMP_MOCK_TIME_BASED_BUFFER_MANAGER_H */
