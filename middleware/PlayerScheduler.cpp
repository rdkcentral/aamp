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

/**
 * @file PlayerScheduler.cpp
 * @brief Class to schedule commands for async execution
 */

#include "PlayerScheduler.h"
#include "PlayerLogManager.h"

/**
 * @brief PlayerScheduler Constructor
 */
PlayerScheduler::PlayerScheduler() : mTaskQueue(), mQMutex(), mQCond(),
	mSchedulerRunning(false), mSchedulerThread(), mExMutex(),
	mExLock(mExMutex, std::defer_lock), mNextTaskId(PLAYER_SCHEDULER_ID_DEFAULT),
	mCurrentTaskId(PLAYER_TASK_ID_INVALID), mLockOut(false) 
{
}

/**
 * @brief PlayerScheduler Destructor
 */
PlayerScheduler::~PlayerScheduler()
{
	if (mSchedulerRunning)
	{
		StopScheduler();
	}
}

static std::hash<std::thread::id> std_thread_hasher;

std::size_t GetPlayerThreadID( const std::thread &t )
{
	return std_thread_hasher( t.get_id() );
}

/**
 * @brief To start scheduler thread
 */
void PlayerScheduler::StartScheduler()
{
	//Turn on thread for processing async operations
	std::lock_guard<std::mutex>lock(mQMutex);
	mSchedulerThread = std::thread(std::bind(&PlayerScheduler::ExecuteAsyncTask, this));
	mSchedulerRunning = true;
	MW_LOG_INFO("Thread created Async Worker [%zx]", GetPlayerThreadID(mSchedulerThread));
}

/**
 * @brief To schedule a task to be executed later
 */
int PlayerScheduler::ScheduleTask(PlayerAsyncTaskObj obj)
{
	int id = PLAYER_TASK_ID_INVALID;
	if (mSchedulerRunning)
	{
		std::lock_guard<std::mutex>lock(mQMutex);
		if (!mLockOut)
		{
			id = mNextTaskId++;
			// Upper limit check
			if (mNextTaskId >= PLAYER_SCHEDULER_ID_MAX_VALUE)
			{
				mNextTaskId = PLAYER_SCHEDULER_ID_DEFAULT;
			}
			obj.mId = id;
			mTaskQueue.push_back(obj);
			mQCond.notify_one();
		}
		else
		{
			// Operation is skipped here, this might happen due to race conditions during normal operation, hence setting as info log
			MW_LOG_INFO("Warning: Attempting to schedule a task when scheduler is locked out, skipping operation %s!!", obj.mTaskName.c_str());
		}
	}
	else
	{
		MW_LOG_ERR("Attempting to schedule a task when scheduler is not running, undefined behavior, task ignored:%s",obj.mTaskName.c_str());
	}
	return id;
}

/**
 * @brief Executes scheduled tasks - invoked by thread
 */
void PlayerScheduler::ExecuteAsyncTask()
{
	std::unique_lock<std::mutex>queueLock(mQMutex);
	while (mSchedulerRunning)
	{
		if (mTaskQueue.empty())
		{
			mQCond.wait(queueLock);
		}
		else
		{
			/* 
			Take the execution lock before taking a task from the queue
			otherwise this function could hold a task, out of the queue,
			that cannot be deleted by RemoveAllTasks()!
			Allow the queue to be modified while waiting.*/
			queueLock.unlock();
			std::lock_guard<std::mutex>executionLock(mExMutex);
			queueLock.lock();

			//note: mTaskQueue could have been modified while waiting for execute permission
			if (!mTaskQueue.empty())
			{
				PlayerAsyncTaskObj obj = mTaskQueue.front();
				mTaskQueue.pop_front();
				if (obj.mId != PLAYER_TASK_ID_INVALID)
				{
					mCurrentTaskId = obj.mId;
					MW_LOG_INFO("Found entry in function queue!!, task:%s. CurrentTaskId:%d ",obj.mTaskName.c_str(),mCurrentTaskId);
					//Unlock so that new entries can be added to queue while function executes
					queueLock.unlock();

					MW_LOG_WARN("SchedulerTask Execution:%s taskId:%d",obj.mTaskName.c_str(),obj.mId);
					//Execute function
					obj.mTask(obj.mData);
					//May be used in a wait() in future loops, it needs to be locked
					queueLock.lock();
				}
				else
				{
					MW_LOG_INFO("Scheduler found a task with invalid ID, skip task!");
				}
			}
		}
	}
	MW_LOG_INFO("Exited Async Worker Thread");
}

/**
 * @brief To remove all scheduled tasks and prevent further tasks from scheduling
 */
void PlayerScheduler::RemoveAllTasks()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	if(!mLockOut)
	{
		MW_LOG_WARN("The scheduler is active.  An active task may continue to execute after this function exits.  Call SuspendScheduler() prior to this function to prevent this.");
	}
	if (!mTaskQueue.empty())
	{
		MW_LOG_WARN("Clearing up %d entries from mFuncQueue", (int)mTaskQueue.size());
		mTaskQueue.clear();
	}
}

/**
 * @brief To stop scheduler and associated resources
 */
void PlayerScheduler::StopScheduler()
{
	MW_LOG_WARN("Stopping Async Worker Thread");
	// Clean up things in queue
	mSchedulerRunning = false;

	//allow StopScheduler() to be called without warning from a nonsuspended state and
	//not cause an error in ResumeScheduler() below due to trying to unlock an unlocked lock
	if(!mLockOut)
	{
		SuspendScheduler();
	}

	RemoveAllTasks();

	//prevent possible deadlock where mSchedulerThread is waiting for mExLock/mExMutex
	ResumeScheduler();
	mQCond.notify_one();
    if (mSchedulerThread.joinable())
        mSchedulerThread.join();
}

/**
 * @brief To acquire execution lock for synchronisation purposes
 */
void PlayerScheduler::SuspendScheduler()
{
	mExLock.lock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = true;
}

/**
 * @brief To release execution lock
 */
void PlayerScheduler::ResumeScheduler()
{
	mExLock.unlock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
}

/**
 * @brief To remove a scheduled tasks with ID
 */
bool PlayerScheduler::RemoveTask(int id)
{
	bool ret = false;
	std::lock_guard<std::mutex>lock(mQMutex);
	// Make sure its not currently executing/executed task
	if (id != PLAYER_TASK_ID_INVALID && mCurrentTaskId != id)
	{
		for (auto it = mTaskQueue.begin(); it != mTaskQueue.end(); )
		{
			if (it->mId == id)
			{
				mTaskQueue.erase(it);
				ret = true;
				break;
			}
			else
			{
				it++;
			}
		}
	}
	return ret;
}

/**
 * @brief To enable scheduler to queue new tasks
 */
void PlayerScheduler::EnableScheduleTask()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
}
