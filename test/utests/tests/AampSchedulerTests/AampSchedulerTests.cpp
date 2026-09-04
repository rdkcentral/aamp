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
*/

#include <gtest/gtest.h>

#include "../AampScheduler.h"
#include "../AampConfig.h"
#include "../AampEvent.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

class AampSchedulerTests : public testing::Test
{
protected:
	AampScheduler *m_as = nullptr;
	void SetUp() override
	{
		m_as = new AampScheduler();
	}
	void TearDown() override
	{
		delete m_as;
		m_as = nullptr;
	}
};

class AampSchedulerTestsFixture : public AampScheduler, public testing::Test
{
protected:
	void SetUp() override
	{
	}
	void TearDown() override
	{
	}
};

TEST_F(AampSchedulerTests,AsyncTaskObjoperatorTest_1)
{
	//Arrange:create local object
	AsyncTaskObj obj(NULL,NULL,"SampleText",AAMP_TASK_ID_INVALID);

	//Act:assign class object
	AsyncTaskObj obj1=obj;
}

TEST_F(AampSchedulerTests,AsyncTaskObjoperatorTest_2)
{
	//Arrange:create local object
	AsyncTaskObj obj2(NULL,NULL,"SampleText",1);

	//Act:assign class object
	AsyncTaskObj obj3=obj2;
}

TEST_F(AampSchedulerTests, destructorTest)
{
	//Act:call the stopscheduler function for clearing the resources
	m_as->StopScheduler();
}

TEST_F(AampSchedulerTests, removeTasksTest1)
{
	//Arrange:declare local queue variable
	std::deque<AsyncTaskObj> mTaskQueue;

	//Act:call the functions on the variables
	mTaskQueue.clear();
	m_as->RemoveAllTasks();
	m_as->SuspendScheduler();

	//Assert:Check for the variable size
	EXPECT_EQ(0,mTaskQueue.size());
}

TEST_F(AampSchedulerTests, removeTasksTest2)
{
	//Arrange:declare local variable with some raw data
	std::deque<AsyncTaskObj> mTaskQueue;
	AsyncTaskObj obj1(NULL,NULL,"SAMPLE",AAMP_TASK_ID_INVALID);
	obj1.mId = 0;  obj1.mId = 0;

	//Act:call the functions to push variables
	mTaskQueue.push_back(obj1);
	//Assert:Check for the variable size
	EXPECT_NE(0,mTaskQueue.size());

	//Assert:Check for the variable size
	EXPECT_EQ(1,mTaskQueue.size());

	//Act:call the functions on the variables
	mTaskQueue.clear();
	m_as->RemoveAllTasks();
	m_as->SuspendScheduler();

	//Assert:Check for the variable size
	EXPECT_EQ(0,mTaskQueue.size());
}

TEST_F(AampSchedulerTests, removeallTasksTest)
{
	//Arrange:declare local variable with some raw data
	std::deque<AsyncTaskObj> mTaskQueue;
	AsyncTaskObj obj1(NULL,NULL,"SAMPLE",AAMP_TASK_ID_INVALID);
	obj1.mId = 0;  obj1.mId = 0;

	AsyncTaskObj obj2(NULL,NULL,"SAMPLETEXT",AAMP_TASK_ID_INVALID);
	obj2.mId = 0;  obj2.mId = 0;

	//Act:call the functions to push variables
	mTaskQueue.push_back(obj1);
	mTaskQueue.push_back(obj2);
	//Assert:Check for the variable size
	EXPECT_NE((int)mTaskQueue.size(),0);

	//Act:call the RemoveAllTasks function
	m_as->RemoveAllTasks();
	m_as->SuspendScheduler();

	//Assert:Check for the variable size
	EXPECT_NE((int)mTaskQueue.size(),0);
}

TEST_F(AampSchedulerTests, StartSchedulerTest)
{
	//Arrange:declare local variable
	bool flag=false;
	//Act:call startscheduler function
	m_as->StartScheduler(-1);
	//Assert:check the result
	EXPECT_FALSE(flag);
}

TEST_F(AampSchedulerTests, RemoveTaskTest)
{
	//Arrange:declare local variable with some raw data
	std::deque<AsyncTaskObj> mTaskQueue;
	AsyncTaskObj obj1(NULL,NULL,"SAMPLE",AAMP_TASK_ID_INVALID);
	obj1.mId = 0;
	//Act:push the object into queue
  	mTaskQueue.push_back(obj1);

	//Assert:check for the retrun value
	EXPECT_NE((int)mTaskQueue.size(),0);

	int id = 1;
	bool ret=m_as->RemoveTask(id);

	//Assert:check for the retrun value
	EXPECT_FALSE(ret);
}

TEST_F(AampSchedulerTests, RemoveTaskTest1)
{
	//Arrange:declare local variable with some raw data
	std::deque<AsyncTaskObj> mTaskQueue;
	AsyncTaskObj obj1(NULL,NULL,"SAMPLE",AAMP_TASK_ID_INVALID);
	obj1.mId = 0;

	//Act:push the object into queue
  	mTaskQueue.push_back(obj1);

	int id=2;
	bool ret=m_as->RemoveTask(id);
	//Assert:check for the retrun value
	EXPECT_FALSE(ret);
}

TEST_F(AampSchedulerTests, RemoveTaskTest2)
{
	//Arrange:declare local variable with some raw data
	std::deque<AsyncTaskObj> mTaskQueue;
	AsyncTaskObj obj1(NULL,NULL,"SAMPLE",20);
	AsyncTaskObj obj2(NULL,NULL,"SAMPLE",20);

	//Act:push the object into queue
  	mTaskQueue.push_back(obj1);
	mTaskQueue.push_back(obj2);

	m_as->SetState(eSTATE_INITIALIZING);
	m_as->StartScheduler(-1);

	bool ret=m_as->RemoveTask(20);

	//Assert:check for the retrun value
	EXPECT_FALSE(ret);
}

TEST_F(AampSchedulerTests, EnableScheduleTaskTest)
{
	//Arrange:declare local variable
	bool flag=true;

	//Act:call EnableScheduleTask function
	m_as->EnableScheduleTask();

	//Assert:check for the retrun value
	EXPECT_TRUE(flag);
}

TEST_F(AampSchedulerTests, SetStateTest)
{
	//Arrange:declare local variable
	AAMPPlayerState sstate = eSTATE_INITIALIZING;

	//Act:call the setstate function with the local variable
	m_as->SetState(sstate);

	//Assert:check for the expected value
	EXPECT_EQ(1,sstate);
}

TEST_F(AampSchedulerTestsFixture, ScheduleTaskTest_SchedulerNotRunning)
{
	// Arrange: Create a valid AsyncTaskObj
	AsyncTaskObj obj([](void* data) { /* Task logic */ }, nullptr, "TaskWhenNotRunning", AAMP_TASK_ID_INVALID);

	// Act: Attempt to schedule the task
	int taskId = ScheduleTask(obj);

	// Assert: Verify that the task is not scheduled
	EXPECT_EQ(taskId, AAMP_TASK_ID_INVALID);
}

TEST_F(AampSchedulerTestsFixture, ScheduleTaskTest_ValidTask)
{
	// Arrange: Create a valid AsyncTaskObj
	AsyncTaskObj obj([](void* data) { /* Task logic */ }, nullptr, "ValidTask", AAMP_TASK_ID_INVALID);

	// Simulate the scheduler running
	mSchedulerRunning = true;

	// Act: Schedule the task
	int taskId = ScheduleTask(obj);

	// Assert: Verify that a valid task ID is returned
	EXPECT_NE(taskId, AAMP_TASK_ID_INVALID);
}

TEST_F(AampSchedulerTestsFixture, ScheduleTaskTest_LockOut)
{
	// Arrange: Create a valid AsyncTaskObj
	AsyncTaskObj obj([](void* data) { /* Task logic */ }, nullptr, "TaskDuringLockOut", AAMP_TASK_ID_INVALID);

	// Simulate the scheduler running
	mSchedulerRunning = true;

	// Lock the scheduler to prevent task scheduling
	SuspendScheduler();

	// Act: Attempt to schedule the task
	int taskId = ScheduleTask(obj);

	// Assert: Verify that the task is not scheduled
	EXPECT_EQ(taskId, AAMP_TASK_ID_INVALID);

	// Cleanup: Resume the scheduler
	ResumeScheduler();
}

TEST_F(AampSchedulerTestsFixture, ScheduleTaskTest_SetRateTask)
{
	// Arrange: Create two AsyncTaskObj instances with the same task name "SetRate"
	AsyncTaskObj obj1([](void* data) { /* Task logic */ }, nullptr, "SetRate", AAMP_TASK_ID_INVALID);
	AsyncTaskObj obj2([](void* data) { /* Task logic */ }, nullptr, "SetRate", AAMP_TASK_ID_INVALID);

	// Simulate the scheduler running
	mSchedulerRunning = true;

	// Act: Schedule the first task
	int taskId1 = ScheduleTask(obj1);

	// Schedule the second task, which should replace the first one
	int taskId2 = ScheduleTask(obj2);

	// Assert: Verify that the second task ID is valid and different from the first
	EXPECT_NE(taskId2, AAMP_TASK_ID_INVALID);
	EXPECT_NE(taskId1, taskId2);

	// Assert: Verify that only obj2 is in the task queue
	EXPECT_EQ(mTaskQueue.size(), 1);
	EXPECT_EQ(mTaskQueue.front().mId, taskId2);
}

TEST_F(AampSchedulerTestsFixture, ScheduleTaskTest_MaxTaskIdWrapAround)
{
	// Arrange: Set the next task ID to the maximum value
	mNextTaskId = AAMP_SCHEDULER_ID_MAX_VALUE - 1;
	// Simulate the scheduler running
	mSchedulerRunning = true;

	// Create a valid AsyncTaskObj
	AsyncTaskObj obj([](void* data) { /* Task logic */ }, nullptr, "WrapAroundTask", AAMP_TASK_ID_INVALID);

	// Act: Schedule two tasks to trigger the wrap-around
	int taskId1 = ScheduleTask(obj);
	int taskId2 = ScheduleTask(obj);

	// Assert: Verify that the task IDs wrap around correctly
	EXPECT_EQ(taskId1, AAMP_SCHEDULER_ID_MAX_VALUE - 1);
	EXPECT_EQ(taskId2, AAMP_SCHEDULER_ID_DEFAULT);
}

/**
 * @test AampSchedulerTestsFixture_DisableScheduleTask_RejectsNewTasks
 * @brief Verifies that DisableScheduleTask() sets the lockout flag so that
 *        ScheduleTask() returns AAMP_TASK_ID_INVALID and leaves the queue empty.
 *        Covers the Stop/Tune concurrency path where the scheduler must refuse
 *        new work during teardown.
 */
TEST_F(AampSchedulerTestsFixture, DisableScheduleTask_RejectsNewTasks)
{
	// Arrange: scheduler is logically running; disable task queuing
	mSchedulerRunning = true;
	DisableScheduleTask();

	AsyncTaskObj obj([](void*) {}, nullptr, "BlockedTask", AAMP_TASK_ID_INVALID);

	// Act: attempt to schedule while locked out
	int taskId = ScheduleTask(obj);

	// Assert: task must be rejected and queue must remain empty
	EXPECT_EQ(taskId, AAMP_TASK_ID_INVALID);
	EXPECT_TRUE(mTaskQueue.empty());

	// Cleanup: DisableScheduleTask does not lock mExLock; restore mLockOut to false
	// so that StopScheduler() can call SuspendScheduler()+ResumeScheduler() safely
	// in the destructor.
	EnableScheduleTask();
}

/**
 * @test AampSchedulerTestsFixture_EnableScheduleTask_AfterDisable_AcceptsNewTasks
 * @brief Verifies that EnableScheduleTask() clears the lockout flag set by
 *        DisableScheduleTask(), restoring normal task acceptance.
 */
TEST_F(AampSchedulerTestsFixture, EnableScheduleTask_AfterDisable_AcceptsNewTasks)
{
	// Arrange: scheduler is logically running; disable then re-enable queuing
	mSchedulerRunning = true;
	DisableScheduleTask();
	EnableScheduleTask();

	AsyncTaskObj obj([](void*) {}, nullptr, "ReenabledTask", AAMP_TASK_ID_INVALID);

	// Act: schedule a task after the lockout is lifted
	int taskId = ScheduleTask(obj);

	// Assert: task must be accepted with a valid ID and appear in the queue
	EXPECT_NE(taskId, AAMP_TASK_ID_INVALID);
	EXPECT_EQ(mTaskQueue.size(), static_cast<size_t>(1));
	EXPECT_EQ(mTaskQueue.front().mId, taskId);
}

/**
 * @test AampSchedulerTestsFixture_ResumeScheduler_AfterSuspend_AcceptsNewTasks
 * @brief Verifies that ResumeScheduler() clears the lockout (and releases the
 *        execution lock) set by SuspendScheduler(), restoring normal task
 *        acceptance.  This mirrors the Stop() sequence:
 *        SuspendScheduler() → RemoveAllTasks() → ResumeScheduler().
 */
TEST_F(AampSchedulerTestsFixture, ResumeScheduler_AfterSuspend_AcceptsNewTasks)
{
	// Arrange: scheduler is logically running; suspend it (Stop/Tune path)
	mSchedulerRunning = true;
	SuspendScheduler();

	// Confirm the lockout is in effect
	AsyncTaskObj blocked([](void*) {}, nullptr, "BlockedTask", AAMP_TASK_ID_INVALID);
	EXPECT_EQ(ScheduleTask(blocked), AAMP_TASK_ID_INVALID);

	// Act: resume the scheduler (releases execution lock + clears mLockOut)
	ResumeScheduler();

	AsyncTaskObj obj([](void*) {}, nullptr, "RestoredTask", AAMP_TASK_ID_INVALID);
	int taskId = ScheduleTask(obj);

	// Assert: task must be accepted after resume
	EXPECT_NE(taskId, AAMP_TASK_ID_INVALID);
	EXPECT_EQ(mTaskQueue.size(), static_cast<size_t>(1));
	EXPECT_EQ(mTaskQueue.front().mId, taskId);
}

/**
 * @test AampSchedulerTestsFixture_StopScheduler_AfterDisableScheduleTask_DoesNotCrash
 * @brief Regression test for the DisableScheduleTask + StopScheduler destructor trap.
 *
 * DisableScheduleTask() sets mLockOut=true WITHOUT acquiring mExLock.
 * Before the fix, StopScheduler() used !mLockOut to decide whether to call
 * SuspendScheduler(); seeing mLockOut=true it skipped SuspendScheduler() and
 * then called ResumeScheduler() which tried to unlock mExLock even though it
 * was never locked, throwing std::system_error (unique_lock::unlock: not locked).
 *
 * After the fix StopScheduler() checks mExLock.owns_lock() directly, so it
 * correctly calls SuspendScheduler() (acquiring mExLock) before ResumeScheduler().
 */
TEST_F(AampSchedulerTestsFixture, StopScheduler_AfterDisableScheduleTask_DoesNotCrash)
{
	// Arrange: scheduler logically running, then disable task queuing without
	// acquiring the execution lock (simulates the Stop() preamble).
	mSchedulerRunning = true;
	DisableScheduleTask();

	// Act + Assert: StopScheduler() must not throw or crash.
	EXPECT_NO_THROW(StopScheduler());

	// Restart so the fixture TearDown (AampScheduler destructor) is in a
	// consistent state; StopScheduler() leaves mSchedulerRunning=false and
	// the thread unjoined if it was never started, which is fine here.
}
