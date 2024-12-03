/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

#ifndef __TSB_SEM__
#define __TSB_SEM__

#include <mutex>
#include <condition_variable>

namespace TSB
{

/**
 * @brief A simple counting semaphore implementation.
 *        If using C++20, this could be replaced with std::counting_semaphore
 */
class Sem
{
public:
	void Wait();
	void Post();

private:
	std::mutex mMutex{};
	std::condition_variable mCondVar{};
	uint32_t mCount{0};
};

} // namespace TSB

#endif // __TSB_SEM__
