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
 * @file PlayerScheduler.h
 * @brief Class to schedule commands for async execution
 */

#ifndef __PLAYER_SCHEDULER_H__
#define __PLAYER_SCHEDULER_H__

#include <functional>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <utility>
#include <glib.h>
#include <string>

#define PLAYER_SCHEDULER_ID_MAX_VALUE INT_MAX  // 10000
#define PLAYER_SCHEDULER_ID_DEFAULT 1		//ID ranges from DEFAULT to MAX
#define PLAYER_TASK_ID_INVALID 0

typedef std::function<void (void *)> AsyncTask;

/**
 * @brief Async task operations
 */
struct PlayerAsyncTaskObj
{
	AsyncTask mTask;
	void * mData;
	int mId;
	std::string mTaskName;

	PlayerAsyncTaskObj(AsyncTask task, void *data, std::string tskName="", int id = PLAYER_TASK_ID_INVALID) :
				mTask(task), mData(data), mId(id),mTaskName(tskName)
	{
	}

	PlayerAsyncTaskObj(const PlayerAsyncTaskObj &other) : mTask(other.mTask), mData(other.mData), mId(other.mId),mTaskName(other.mTaskName)
	{
	}

	PlayerAsyncTaskObj& operator=(const PlayerAsyncTaskObj& other)
	{
		mTask = other.mTask;
		mData = other.mData;
		mId = other.mId;
		mTaskName = other.mTaskName;
		return *this;
	}
};

/**
 * @brief Scheduler class for asynchronous operations
 */
class PlayerScheduler
{
public:
	/**
	 * @fn PlayerScheduler
	 */
	PlayerScheduler();

	PlayerScheduler(const PlayerScheduler&) = delete;
	PlayerScheduler& operator=(const PlayerScheduler&) = delete;
	/**
	 * @fn ~PlayerScheduler
	 */
	virtual ~PlayerScheduler();

	/**
	 * @fn ScheduleTask
	 *
	 * @param[in] obj - object to be scheduled
	 * @return int - scheduled task id
	 */
	int ScheduleTask(PlayerAsyncTaskObj obj);

	/**
	 * @fn RemoveAllTasks
	 *
	 * @return void
	 */
	void RemoveAllTasks();

	/**
	 * @fn RemoveTask
	 *
	 * @param[in] id - ID of task to be removed
	 * @return bool true if removed, false otherwise
	 */
	bool RemoveTask(int id);

	/**
	 * @fn StartScheduler
	 *
	 * @return void
	 */
	void StartScheduler();

	/**
	 * @fn StopScheduler
	 *
	 * @return void
	 */
	void StopScheduler();

	/**
	 * @fn SuspendScheduler
	 *
	 * @return void
	 */
	void SuspendScheduler();

	/**
	 * @fn ResumeScheduler
	 *
	 * @return void
	 */
	void ResumeScheduler();

	/**
	 * @fn EnableScheduleTask
	 *
	 * @return void
	 */
	void EnableScheduleTask();

protected:
	/**
	 * @fn ExecuteAsyncTask
	 *
	 * @return void
	 */
	void ExecuteAsyncTask();

	std::deque<PlayerAsyncTaskObj> mTaskQueue;	/**< Queue for storing scheduled tasks */
	std::mutex mQMutex;			/**< Mutex for accessing mTaskQueue */
	std::condition_variable mQCond;		/**< To notify when a task is queued in mTaskQueue */
	bool mSchedulerRunning;			/**< Flag denotes if scheduler thread is running */
	std::thread mSchedulerThread;		/**< Scheduler thread */
	std::mutex mExMutex;			/**< Execution mutex for synchronization */
	std::unique_lock<std::mutex> mExLock;	/**< Lock to be used by SuspendScheduler and ResumeScheduler */
	int mNextTaskId;			/**< counter that holds ID value of next task to be scheduled */
	int mCurrentTaskId;			/**< ID of current executed task */
	bool mLockOut;				/**< flag indicates if the queue is locked out or not */
};

#endif /* __PLAYER_SCHEDULER_H__ */
