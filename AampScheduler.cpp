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
	AAMPLOG_WARN("vk:: StartScheduler called, playerId:%d", playerId);
	//Turn on thread for processing async operations
	std::lock_guard<std::mutex>lock(mQMutex);
	mSchedulerThread = std::thread(std::bind(&AampScheduler::ExecuteAsyncTask, this));
	mSchedulerRunning = true;
	AAMPLOG_WARN("vk:: StartScheduler thread created Async Worker [%zx] mSchedulerRunning:%d", GetPrintableThreadID(mSchedulerThread), mSchedulerRunning);
}

/**
 * @brief To schedule a task to be executed later
 */
int AampScheduler::ScheduleTask(AsyncTaskObj obj)
{
	int id = AAMP_TASK_ID_INVALID;
	AAMPLOG_WARN("vk:: ScheduleTask entry, task:%s mSchedulerRunning:%d mState:%d mLockOut:%d mCurrentTaskId:%d",
		obj.mTaskName.c_str(), mSchedulerRunning, mState, mLockOut, mCurrentTaskId);
	if (mSchedulerRunning)
	{

		if( mState == eSTATE_ERROR || mState == eSTATE_RELEASED)
		{
			AAMPLOG_WARN("vk:: ScheduleTask REJECTED task:%s due to mState:%d (ERROR=%d RELEASED=%d)",
				obj.mTaskName.c_str(), mState, eSTATE_ERROR, eSTATE_RELEASED);
			return id;
		}

		std::lock_guard<std::mutex>lock(mQMutex);
		if (!mLockOut)
		{
			id = mNextTaskId++;
			// Upper limit check
			if (mNextTaskId >= AAMP_SCHEDULER_ID_MAX_VALUE)
			{
				mNextTaskId = AAMP_SCHEDULER_ID_DEFAULT;
			}
			obj.mId = id;
			if (obj.mTaskName == "SetRate")
			{
				// Remove any existing SetRate task from the queue
				auto it = std::find_if(mTaskQueue.begin(), mTaskQueue.end(),
									   [](const AsyncTaskObj& obj) { return obj.mTaskName == "SetRate"; });
				if (it != mTaskQueue.end()) {
					AAMPLOG_WARN("vk:: ScheduleTask Found queued SetRate task, removing old one. task:%s taskId:%d, new taskId:%d", it->mTaskName.c_str(), it->mId, id);
					mTaskQueue.erase(it);
				}
			}
			mTaskQueue.push_back(obj);
			AAMPLOG_WARN("vk:: ScheduleTask QUEUED task:%s taskId:%d queueSize:%zu, notifying worker thread",
				obj.mTaskName.c_str(), id, mTaskQueue.size());
			mQCond.notify_one();
		}
		else
		{
			AAMPLOG_WARN("vk:: ScheduleTask LOCKED OUT task:%s mLockOut:%d - task will NOT be queued!!", obj.mTaskName.c_str(), mLockOut);
		}
	}
	else
	{
		AAMPLOG_WARN("vk:: ScheduleTask SCHEDULER NOT RUNNING task:%s - task ignored!!", obj.mTaskName.c_str());
	}
	return id;
}

/**
 * @brief Executes scheduled tasks - invoked by thread
 */
void AampScheduler::ExecuteAsyncTask()
{
	AAMPLOG_WARN("vk:: ExecuteAsyncTask ENTERED, thread started. mPlayerId:%d mSchedulerRunning:%d", mPlayerId, mSchedulerRunning);
	UsingPlayerId playerId( mPlayerId );
	std::unique_lock<std::mutex>queueLock(mQMutex);
	AAMPLOG_WARN("vk:: ExecuteAsyncTask acquired queueLock, entering main loop. mSchedulerRunning:%d queueSize:%zu",
		mSchedulerRunning, mTaskQueue.size());
	while (mSchedulerRunning)
	{
		if (mTaskQueue.empty())
		{
			AAMPLOG_WARN("vk:: ExecuteAsyncTask queue is EMPTY, waiting on condition variable. mSchedulerRunning:%d mLockOut:%d mState:%d",
				mSchedulerRunning, mLockOut, mState);
			mQCond.wait(queueLock);
			AAMPLOG_WARN("vk:: ExecuteAsyncTask WOKE UP from condition wait. mSchedulerRunning:%d queueSize:%zu mLockOut:%d mState:%d",
				mSchedulerRunning, mTaskQueue.size(), mLockOut, mState);
		}
		else
		{
			AAMPLOG_WARN("vk:: ExecuteAsyncTask queue has %zu tasks, attempting to acquire execution lock (mExMutex). mLockOut:%d",
				mTaskQueue.size(), mLockOut);
			/*
			Take the execution lock before taking a task from the queue
			otherwise this function could hold a task, out of the queue,
			that cannot be deleted by RemoveAllTasks()!
			Allow the queue to be modified while waiting.*/
			queueLock.unlock();
			AAMPLOG_WARN("vk:: ExecuteAsyncTask queueLock RELEASED, now waiting for mExMutex (execution lock). If stuck here, SuspendScheduler is holding the lock!");
			std::lock_guard<std::mutex>executionLock(mExMutex);
			AAMPLOG_WARN("vk:: ExecuteAsyncTask mExMutex ACQUIRED, re-acquiring queueLock");
			queueLock.lock();
			AAMPLOG_WARN("vk:: ExecuteAsyncTask queueLock RE-ACQUIRED. queueSize:%zu mSchedulerRunning:%d mState:%d",
				mTaskQueue.size(), mSchedulerRunning, mState);

			//mTaskQueue could have been modified while waiting for execute permission
			if (!mTaskQueue.empty())
			{
				AsyncTaskObj obj = mTaskQueue.front();
				mTaskQueue.pop_front();
				AAMPLOG_WARN("vk:: ExecuteAsyncTask DEQUEUED task:%s taskId:%d remainingQueueSize:%zu",
					obj.mTaskName.c_str(), obj.mId, mTaskQueue.size());
				if (obj.mId != AAMP_TASK_ID_INVALID)
				{
					mCurrentTaskId = obj.mId;
					AAMPLOG_WARN("vk:: ExecuteAsyncTask task:%s taskId:%d mState:%d mCurrentTaskId:%d",
						obj.mTaskName.c_str(), obj.mId, mState, mCurrentTaskId);
					if( mState != eSTATE_ERROR && mState != eSTATE_RELEASED)
					{
						//Unlock so that new entries can be added to queue while function executes
						queueLock.unlock();

						AAMPLOG_WARN("vk:: ExecuteAsyncTask EXECUTING task:%s taskId:%d",obj.mTaskName.c_str(),obj.mId);
						//Execute function
						obj.mTask(obj.mData);
						AAMPLOG_WARN("vk:: ExecuteAsyncTask COMPLETED task:%s taskId:%d",obj.mTaskName.c_str(),obj.mId);
						//May be used in a wait() in future loops, it needs to be locked
						queueLock.lock();
					}
					else
					{
						AAMPLOG_WARN("vk:: ExecuteAsyncTask SKIPPED task:%s taskId:%d due to mState:%d (ERROR=%d RELEASED=%d)",
							obj.mTaskName.c_str(), obj.mId, mState, eSTATE_ERROR, eSTATE_RELEASED);
					}
				}
				else
				{
					AAMPLOG_WARN("vk:: ExecuteAsyncTask INVALID task ID, skipping task:%s", obj.mTaskName.c_str());
				}
			}
			else
			{
				AAMPLOG_WARN("vk:: ExecuteAsyncTask queue became EMPTY after re-acquiring locks (tasks were removed while waiting for mExMutex)");
			}
		}
	}
	AAMPLOG_WARN("vk:: ExecuteAsyncTask EXITING worker thread. mSchedulerRunning:%d", mSchedulerRunning);
}

/**
 * @brief To remove all scheduled tasks and prevent further tasks from scheduling
 */
void AampScheduler::RemoveAllTasks()
{
	std::lock_guard<std::mutex>lock(mQMutex);
	AAMPLOG_WARN("vk:: RemoveAllTasks called. queueSize:%zu mLockOut:%d mState:%d mCurrentTaskId:%d",
		mTaskQueue.size(), mLockOut, mState, mCurrentTaskId);
	if(!mLockOut)
	{
		AAMPLOG_WARN("vk:: RemoveAllTasks WARNING scheduler is NOT locked out, active task may continue executing");
	}
	if (!mTaskQueue.empty())
	{
		for (const auto& task : mTaskQueue)
		{
			AAMPLOG_WARN("vk:: RemoveAllTasks REMOVING task:%s taskId:%d", task.mTaskName.c_str(), task.mId);
		}
		AAMPLOG_WARN("vk:: RemoveAllTasks clearing %d entries from queue", (int)mTaskQueue.size());
		mTaskQueue.clear();
	}
}

/**
 * @brief To stop scheduler and associated resources
 */
void AampScheduler::StopScheduler()
{
	AAMPLOG_WARN("vk:: StopScheduler called. mSchedulerRunning:%d mLockOut:%d mState:%d mCurrentTaskId:%d",
		mSchedulerRunning, mLockOut, mState, mCurrentTaskId);
	// Clean up things in queue
	mSchedulerRunning = false;

	//allow StopScheduler() to be called without warning from a nonsuspended state and
	//not cause an error in ResumeScheduler() below due to trying to unlock an unlocked lock
	if(!mLockOut)
	{
		AAMPLOG_WARN("vk:: StopScheduler calling SuspendScheduler");
		SuspendScheduler();
	}

	RemoveAllTasks();

	//prevent possible deadlock where mSchedulerThread is waiting for mExLock/mExMutex
	AAMPLOG_WARN("vk:: StopScheduler calling ResumeScheduler to prevent deadlock");
	ResumeScheduler();
	mQCond.notify_one();
    if (mSchedulerThread.joinable())
	{
		AAMPLOG_WARN("vk:: StopScheduler joining worker thread");
        mSchedulerThread.join();
		AAMPLOG_WARN("vk:: StopScheduler worker thread joined successfully");
	}
}

/**
 * @brief To acquire execution lock for synchronization purposes
 */
void AampScheduler::SuspendScheduler()
{
	AAMPLOG_WARN("vk:: SuspendScheduler called, acquiring mExLock (execution lock). mLockOut:%d mState:%d", mLockOut, mState);
	mExLock.lock();
	AAMPLOG_WARN("vk:: SuspendScheduler mExLock ACQUIRED, setting mLockOut=true");
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = true;
	AAMPLOG_WARN("vk:: SuspendScheduler DONE. mLockOut:%d queueSize:%zu", mLockOut, mTaskQueue.size());
}

/**
 * @brief To release execution lock
 */
void AampScheduler::ResumeScheduler()
{
	AAMPLOG_WARN("vk:: ResumeScheduler called, releasing mExLock. mLockOut:%d mState:%d", mLockOut, mState);
	mExLock.unlock();
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
	AAMPLOG_WARN("vk:: ResumeScheduler DONE. mLockOut:%d queueSize:%zu", mLockOut, mTaskQueue.size());
}

/**
 * @brief To remove a scheduled tasks with ID
 */
bool AampScheduler::RemoveTask(int id)
{
	bool ret = false;
	AAMPLOG_WARN("vk:: RemoveTask called. id:%d mCurrentTaskId:%d mLockOut:%d mState:%d", id, mCurrentTaskId, mLockOut, mState);
	std::lock_guard<std::mutex>lock(mQMutex);
	// Make sure its not currently executing/executed task
	if (id != AAMP_TASK_ID_INVALID && mCurrentTaskId != id)
	{
		for (auto it = mTaskQueue.begin(); it != mTaskQueue.end(); )
		{
			if (it->mId == id)
			{
				AAMPLOG_WARN("vk:: RemoveTask FOUND and REMOVING task:%s taskId:%d queueSize:%zu", it->mTaskName.c_str(), it->mId, mTaskQueue.size());
				mTaskQueue.erase(it);
				ret = true;
				break;
			}
			else
			{
				it++;
			}
		}
		if (!ret)
		{
			AAMPLOG_WARN("vk:: RemoveTask id:%d NOT FOUND in queue (queueSize:%zu)", id, mTaskQueue.size());
		}
	}
	else
	{
		AAMPLOG_WARN("vk:: RemoveTask SKIPPED id:%d (invalid or currently executing, mCurrentTaskId:%d)", id, mCurrentTaskId);
	}
	return ret;
}

/**
 * @brief To enable scheduler to queue new tasks
 */
void AampScheduler::EnableScheduleTask()
{
	AAMPLOG_WARN("vk:: EnableScheduleTask called. mLockOut:%d mState:%d mSchedulerRunning:%d", mLockOut, mState, mSchedulerRunning);
	std::lock_guard<std::mutex>lock(mQMutex);
	mLockOut = false;
	AAMPLOG_WARN("vk:: EnableScheduleTask DONE. mLockOut:%d", mLockOut);
}

/**
 * @brief To player state to Scheduler
 */
void AampScheduler::SetState(AAMPPlayerState sstate)
{
	AAMPLOG_WARN("vk:: SetState called. oldState:%d newState:%d mSchedulerRunning:%d mLockOut:%d",
		mState, sstate, mSchedulerRunning, mLockOut);
	mState = sstate;
}
