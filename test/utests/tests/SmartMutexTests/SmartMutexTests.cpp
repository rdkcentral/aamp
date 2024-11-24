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

#include <gtest/gtest.h>
#include "AampSmartMutex.hpp"
#include <thread>

static AampSmartMutex mutex("gtest");
static std::string result;

static void task1(void)
{
	LOCK_SMARTMUTEX(mutex);
	result += "[task1 lock]";
	sleep(2);
	result += "[task1 unlock]";
	mutex.unlock();
}
static void task2(void)
{
	sleep(1);
	LOCK_SMARTMUTEX(mutex);
	result += "[task2 lock]";
	sleep(2);
	result += "[task2 unlock]";
	mutex.unlock();
}

static void task3(void)
{
	LOCK_SMARTMUTEX(mutex);
	result += "[task3 lock]";
	mutex.unlock();
}

TEST(AampSmartMutex, fastLock)
{
	result.clear();
	std::thread t3a(task3);
	std::thread t3b(task3);
	t3a.join();
	t3b.join();
	ASSERT_EQ( result, "[task3 lock][task3 lock]" );
}

TEST(AampSmartMutex, delayedLock)
{
	result.clear();
	std::thread t1(task1);
	std::thread t2(task2);
	t1.join();
	t2.join();
	ASSERT_EQ( result, "[task1 lock][task1 unlock][task2 lock][task2 unlock]" );
}
