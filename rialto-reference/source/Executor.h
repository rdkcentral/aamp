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

#ifndef RIALTO_REFERENCE_EXECUTOR_H
#define RIALTO_REFERENCE_EXECUTOR_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace rialto_reference
{

/**
 * @brief One thread running posted tasks in order; delayed tasks run when due.
 *
 * Tasks posted for the same due time run in posting order. The queue state is
 * shared with the thread, so stop() may be called from a task (for example a
 * client destroying the pipeline from inside a callback): the thread is then
 * detached and exits after that task returns.
 */
class Executor
{
public:
	Executor() : m_state(std::make_shared<State>()), m_thread(&Executor::run, m_state) {}

	~Executor() { stop(); }

	Executor(const Executor &) = delete;
	Executor &operator=(const Executor &) = delete;

	void post(std::function<void()> task) { postDelayed(std::chrono::milliseconds(0), std::move(task)); }

	void postDelayed(std::chrono::milliseconds delay, std::function<void()> task)
	{
		{
			std::lock_guard<std::mutex> lock(m_state->mutex);
			if (m_state->stopping)
			{
				return;
			}
			m_state->tasks.emplace(std::chrono::steady_clock::now() + delay, std::move(task));
		}
		m_state->cv.notify_one();
	}

	/** Discards pending tasks and waits for the running one, unless called from the executor's own thread. */
	void stop()
	{
		{
			std::lock_guard<std::mutex> lock(m_state->mutex);
			m_state->stopping = true;
			m_state->tasks.clear();
		}
		m_state->cv.notify_one();
		if (!m_thread.joinable())
		{
			return;
		}
		if (isCurrentThread())
		{
			m_thread.detach();
		}
		else
		{
			m_thread.join();
		}
	}

	bool isCurrentThread() const { return std::this_thread::get_id() == m_thread.get_id(); }

private:
	struct State
	{
		std::mutex mutex;
		std::condition_variable cv;
		std::multimap<std::chrono::steady_clock::time_point, std::function<void()>> tasks;
		bool stopping{false};
	};

	static void run(std::shared_ptr<State> state)
	{
		std::unique_lock<std::mutex> lock(state->mutex);
		while (!state->stopping)
		{
			if (state->tasks.empty())
			{
				state->cv.wait(lock);
				continue;
			}
			auto first = state->tasks.begin();
			// Copied: the node can be erased by stop() while waiting.
			const auto due = first->first;
			if (due > std::chrono::steady_clock::now())
			{
				state->cv.wait_until(lock, due);
				continue;
			}
			auto task = std::move(first->second);
			state->tasks.erase(first);
			lock.unlock();
			task();
			task = nullptr;
			lock.lock();
		}
	}

	std::shared_ptr<State> m_state;
	std::thread m_thread;
};

} // namespace rialto_reference

#endif // RIALTO_REFERENCE_EXECUTOR_H
