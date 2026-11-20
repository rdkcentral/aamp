#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

namespace nmqe {

/**
 * Single-owner event loop. Mutable player state is touched only from tasks
 * run here; other threads post. Timers are monotonic.
 */
class EventLoop
{
public:
	using Task = std::function<void()>;

	EventLoop() = default;
	~EventLoop();
	EventLoop(const EventLoop &) = delete;
	EventLoop &operator=(const EventLoop &) = delete;

	void start();
	/** Stops after the current task; pending tasks are discarded. Idempotent. */
	void stop();

	void post(Task task) { postAt(0, std::move(task)); }
	/** Runs `task` once the monotonic clock reaches `dueMonoNs` (0 = now). */
	void postAt(int64_t dueMonoNs, Task task);
	void postAfterMs(int64_t delayMs, Task task);

	bool isLoopThread() const { return std::this_thread::get_id() == mThread.get_id(); }

private:
	void run();

	std::mutex mMutex;
	std::condition_variable mCv;
	std::multimap<int64_t, Task> mTasks; // equal keys keep posting order
	bool mStopping = false;
	std::thread mThread;
};

} // namespace nmqe
