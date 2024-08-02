#include <gtest/gtest.h>
#include "AampSmartMutex.hpp"
#include <thread>
#include "AampLogManager.h"

AampLogManager gGlobalLogObj;
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
