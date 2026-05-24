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

/**
 * @file AampScheduler.cpp
 * @brief Class to schedule commands for async execution
 */

#include "AampScheduler.h"
#include "AampUtils.h"

/**
 * @brief AampScheduler Constructor
 */
AampScheduler::AampScheduler() : mTaskQueue(), mQMutex(), mQCond(),
	mSchedulerRunning(false), mSchedulerThread(), mExMutex(),
	mExLock(mExMutex, std::defer_lock), mNextTaskId(AAMP_SCHEDULER_ID_DEFAULT),
	mCurrentTaskId(AAMP_TASK_ID_INVALID), mLockOut(false), mState(eSTATE_IDLE),mPlayerId(-1)
{
}

/**
 * @brief AampScheduler Destructor
 */
AampScheduler::~AampScheduler()
{
	if (mSchedulerRunning)
	{
		StopScheduler();
	}
}

/**
 * @brief To start scheduler thread
 */
void AampScheduler::StartScheduler( int playerId )
{
	 mPlayerId = playerId;
	//Turn on thread for processing async operations
	std::lock_guard<std::mutex>lock(mQMutex);
	mSchedulerThread = std::thread(std::bind(&AampScheduler::ExecuteAsyncTask, this));
	mSchedulerRunning = true;
	AAMPLOG_INFO("Thread created Async Worker [%zx]", GetPrintableThreadID(mSchedulerThread));
}

/**
 * @brief To schedule a task to be executed later
 */
int AampScheduler::ScheduleTask(AsyncTaskObj obj)
{
    int id = AAMP_TASK_ID_INVALID;

    AAMPLOG_INFO("[ScheduleTask] ENTER task=%s thread=%ld", obj.mTaskName.c_str(), std::this_thread::get_id());

    if (mSchedulerRunning)
    {
        AAMPLOG_INFO("[ScheduleTask] Scheduler running");

        if (mState == eSTATE_ERROR || mState == eSTATE_RELEASED)
        {
            AAMPLOG_WARN("[ScheduleTask] Scheduler in invalid state=%d", mState);
            return id;
        }

        AAMPLOG_INFO("[ScheduleTask] BEFORE mutex lock");

        // 🔴 Potential hang point
        std::lock_guard<std::mutex> lock(mQMutex);

        AAMPLOG_INFO("[ScheduleTask] AFTER mutex lock acquired");

        if (!mLockOut)
        {
            id = mNextTaskId++;
            AAMPLOG_INFO("[ScheduleTask] Assigned TaskId=%d", id);

            // Upper limit check
            if (mNextTaskId >= AAMP_SCHEDULER_ID_MAX_VALUE)
            {
                mNextTaskId = AAMP_SCHEDULER_ID_DEFAULT;
                AAMPLOG_INFO("[ScheduleTask] TaskId rolled over");
            }

            obj.mId = id;

            if (obj.mTaskName == "SetRate")
            {
                AAMPLOG_INFO("[ScheduleTask] Checking existing SetRate task");

                auto it = std::find_if(mTaskQueue.begin(), mTaskQueue.end(),
                                       [](const AsyncTaskObj& obj) { return obj.mTaskName == "SetRate"; });

                if (it != mTaskQueue.end())
                {
                    AAMPLOG_INFO("[ScheduleTask] Removing old SetRate task. taskId=%d", it->mId);
                    mTaskQueue.erase(it);
                }
            }

            AAMPLOG_INFO("[ScheduleTask] Pushing task to queue. current size=%zu", mTaskQueue.size());

            mTaskQueue.push_back(obj);

            AAMPLOG_INFO("[ScheduleTask] Task pushed. new size=%zu", mTaskQueue.size());

            mQCond.notify_one();
            AAMPLOG_INFO("[ScheduleTask] Condition notified");
        }
        else
        {
            AAMPLOG_WARN("[ScheduleTask] Scheduler locked out. Skipping task=%s", obj.mTaskName.c_str());
        }
    }
    else
    {
        AAMPLOG_ERR("[ScheduleTask] Scheduler NOT running. Task ignored=%s", obj.mTaskName.c_str());
    }

    AAMPLOG_INFO("[ScheduleTask] EXIT taskId=%d", id);

    return id;
}
/**
 * @brief Executes scheduled tasks - invoked by thread
 */
void AampScheduler::ExecuteAsyncTask()
{
    UsingPlayerId playerId(mPlayerId);

    AAMPLOG_INFO("[ExecuteAsyncTask] ENTER thread=%ld", std::this_thread::get_id());

    AAMPLOG_INFO("[ExecuteAsyncTask] BEFORE acquiring mQMutex");
    std::unique_lock<std::mutex> queueLock(mQMutex);
    AAMPLOG_INFO("[ExecuteAsyncTask] AFTER acquiring mQMutex");

    while (mSchedulerRunning)
    {
        AAMPLOG_INFO("[ExecuteAsyncTask] Loop start. Queue size=%zu", mTaskQueue.size());

        if (mTaskQueue.empty())
        {
            AAMPLOG_INFO("[ExecuteAsyncTask] Queue empty -> waiting on condition");

            // 🔴 Possible wait hang point
            mQCond.wait(queueLock);

            AAMPLOG_INFO("[ExecuteAsyncTask] Woke up from condition wait");
        }
        else
        {
            AAMPLOG_INFO("[ExecuteAsyncTask] Queue NOT empty");

            // Step 1: Unlock queue mutex
            AAMPLOG_INFO("[ExecuteAsyncTask] Unlocking mQMutex before acquiring mExMutex");
            queueLock.unlock();

            // 🔴 Possible hang point (execution mutex)
            AAMPLOG_INFO("[ExecuteAsyncTask] BEFORE acquiring mExMutex");
            std::lock_guard<std::mutex> executionLock(mExMutex);
            AAMPLOG_INFO("[ExecuteAsyncTask] AFTER acquiring mExMutex");

            // Step 2: Re-lock queue mutex
            AAMPLOG_INFO("[ExecuteAsyncTask] Re-locking mQMutex");
            queueLock.lock();
            AAMPLOG_INFO("[ExecuteAsyncTask] Re-acquired mQMutex");

            // Re-check queue
            if (!mTaskQueue.empty())
            {
                AsyncTaskObj obj = mTaskQueue.front();
                mTaskQueue.pop_front();

                AAMPLOG_INFO("[ExecuteAsyncTask] Popped task: %s id=%d remaining=%zu",
                             obj.mTaskName.c_str(), obj.mId, mTaskQueue.size());

                if (obj.mId != AAMP_TASK_ID_INVALID)
                {
                    mCurrentTaskId = obj.mId;

                    AAMPLOG_INFO("[ExecuteAsyncTask] Processing task=%s state=%d currentId=%d",
                                 obj.mTaskName.c_str(), mState, mCurrentTaskId);

                    if (mState != eSTATE_ERROR && mState != eSTATE_RELEASED)
                    {
                        // Unlock queue while executing task
                        AAMPLOG_INFO("[ExecuteAsyncTask] Unlocking mQMutex before task execution");
                        queueLock.unlock();



/**
 * @brief To remove all scheduled tasks and prevent further tasks from scheduling
 */
void AampScheduler::RemoveAllTasks()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	if(!mLockOut)
	{
		AAMPLOG_WARN("The scheduler is active.  An active task may continue to execute after this function exits.  Call SuspendScheduler() prior to this function to prevent this.");
	}
	if (!mTaskQueue.empty())
	{
		AAMPLOG_WARN("Clearing up %d entries from mFuncQueue", (int)mTaskQueue.size());
		mTaskQueue.clear();
	}
}

/**
 * @brief To stop scheduler and associated resources
 */
void AampScheduler::StopScheduler()
{
    AAMPLOG_WARN("[StopScheduler] ENTER thread=%ld", std::this_thread::get_id());

    AAMPLOG_WARN("[StopScheduler] Stopping Async Worker Thread");

    // Step 1: Mark scheduler as stopped
    mSchedulerRunning = false;
    AAMPLOG_INFO("[StopScheduler] mSchedulerRunning set to false");

    // Step 2: Check lockout state
    if (!mLockOut)
    {
        AAMPLOG_INFO("[StopScheduler] Calling SuspendScheduler()");
        SuspendScheduler();
        AAMPLOG_INFO("[StopScheduler] Returned from SuspendScheduler()");
    }
    else
    {
        AAMPLOG_INFO("[StopScheduler] Scheduler already in lockout state");
    }

    // 🔴 Possible hang source if this uses mQMutex / mExMutex
    AAMPLOG_INFO("[StopScheduler] BEFORE RemoveAllTasks()");
    RemoveAllTasks();
    AAMPLOG_INFO("[StopScheduler] AFTER RemoveAllTasks()");

    // prevent deadlock where worker waits for mExMutex
    AAMPLOG_INFO("[StopScheduler] BEFORE ResumeScheduler()");
    ResumeScheduler();
    AAMPLOG_INFO("[StopScheduler] AFTER ResumeScheduler()");

    // Wake up worker thread
    AAMPLOG_INFO("[StopScheduler] Notifying condition variable");
    mQCond.notify_one();

    // 🔴 VERY IMPORTANT: possible hang point
    if (mSchedulerThread.joinable())
    {
        AAMPLOG_WARN("[StopScheduler] BEFORE thread join");

        mSchedulerThread.join();

        AAMPLOG_WARN("[StopScheduler] AFTER thread join");
    }
    else
    {
        AAMPLOG_WARN("[StopScheduler] Thread not joinable");
    }

    AAMPLOG_WARN("[StopScheduler] EXIT");
}
/**
 * @brief To acquire execution lock for synchronization purposes
 */
void AampScheduler::SuspendScheduler()
{
    AAMPLOG_INFO("[SuspendScheduler] ENTER thread=%ld", std::this_thread::get_id());

    // 🔴 Possible hang point: mExLock
    AAMPLOG_INFO("[SuspendScheduler] BEFORE acquiring mExLock");
    mExLock.lock();
    AAMPLOG_INFO("[SuspendScheduler] AFTER acquiring mExLock");

    // 🔴 Possible hang point: mQMutex
    AAMPLOG_INFO("[SuspendScheduler] BEFORE acquiring mQMutex");
    std::lock_guard<std::mutex> lock(mQMutex);
    AAMPLOG_INFO("[SuspendScheduler] AFTER acquiring mQMutex");

    mLockOut = true;
    AAMPLOG_INFO("[SuspendScheduler] mLockOut set to true");

    AAMPLOG_INFO("[SuspendScheduler] EXIT");
}

/**
 * @brief To release execution lock
 */
void AampScheduler::ResumeScheduler()
{
    AAMPLOG_INFO("[ResumeScheduler] ENTER thread=%ld", std::this_thread::get_id());

    // 🔴 Unlock mExLock (can crash if not locked properly)
    AAMPLOG_INFO("[ResumeScheduler] BEFORE releasing mExLock");
    mExLock.unlock();
    AAMPLOG_INFO("[ResumeScheduler] AFTER releasing mExLock");

    // 🔴 Possible hang point: mQMutex
    AAMPLOG_INFO("[ResumeScheduler] BEFORE acquiring mQMutex");
    std::lock_guard<std::mutex> lock(mQMutex);
    AAMPLOG_INFO("[ResumeScheduler] AFTER acquiring mQMutex");

    mLockOut = false;
    AAMPLOG_INFO("[ResumeScheduler] mLockOut set to false");

    AAMPLOG_INFO("[ResumeScheduler] EXIT");
}

/**
 * @brief To remove a scheduled tasks with ID
 */
bool AampScheduler::RemoveTask(int id)
{
    bool ret = false;

    AAMPLOG_INFO("[RemoveTask] ENTER thread=%ld id=%d", std::this_thread::get_id(), id);

    // 🔴 Possible hang point: mQMutex
    AAMPLOG_INFO("[RemoveTask] BEFORE acquiring mQMutex");
    std::lock_guard<std::mutex> lock(mQMutex);
    AAMPLOG_INFO("[RemoveTask] AFTER acquiring mQMutex");

    AAMPLOG_INFO("[RemoveTask] Current queue size=%zu, currentTaskId=%d",
                 mTaskQueue.size(), mCurrentTaskId);

    // Make sure it's not currently executing task
    if (id != AAMP_TASK_ID_INVALID && mCurrentTaskId != id)
    {
        AAMPLOG_INFO("[RemoveTask] Searching for task id=%d", id);

        for (auto it = mTaskQueue.begin(); it != mTaskQueue.end(); )
        {
            AAMPLOG_INFO("[RemoveTask] Inspecting task id=%d name=%s",
                         it->mId, it->mTaskName.c_str());

            if (it->mId == id)
            {
                AAMPLOG_INFO("[RemoveTask] Found task id=%d, removing", id);

                mTaskQueue.erase(it);
                ret = true;

                AAMPLOG_INFO("[RemoveTask] Task removed. New size=%zu", mTaskQueue.size());
                break;
            }
            else
            {
                ++it;
            }
        }

        if (!ret)
        {
            AAMPLOG_WARN("[RemoveTask] Task id=%d not found in queue", id);
        }
    }
    else
    {
        AAMPLOG_WARN("[RemoveTask] Skipping removal. Invalid id or currently executing task. id=%d current=%d",
                     id, mCurrentTaskId);
    }

    AAMPLOG_INFO("[RemoveTask] EXIT ret=%d", ret);

    return ret;
}

/**
 * @brief To enable scheduler to queue new tasks
 */
void AampScheduler::EnableScheduleTask()
{
    AAMPLOG_INFO("[EnableScheduleTask] ENTER thread=%ld", std::this_thread::get_id());

    // 🔴 Possible hang point: mQMutex
    AAMPLOG_INFO("[EnableScheduleTask] BEFORE acquiring mQMutex");
    std::lock_guard<std::mutex> lock(mQMutex);
    AAMPLOG_INFO("[EnableScheduleTask] AFTER acquiring mQMutex");

    AAMPLOG_INFO("[EnableScheduleTask] Previous mLockOut=%d", mLockOut);

    mLockOut = false;

    AAMPLOG_INFO("[EnableScheduleTask] Updated mLockOut=%d", mLockOut);

    AAMPLOG_INFO("[EnableScheduleTask] EXIT");
}

/**
 * @brief To player state to Scheduler
 */
void AampScheduler::SetState(AAMPPlayerState sstate)
{
	mState = sstate;
}
