#include "EventLoop.h"

#include "Clock.h"

namespace nmqe {

EventLoop::~EventLoop()
{
	stop();
}

void EventLoop::start()
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (mThread.joinable()) return;
	mStopping = false;
	mThread = std::thread(&EventLoop::run, this);
}

void EventLoop::stop()
{
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mStopping = true;
	}
	mCv.notify_all();
	if (mThread.joinable() && !isLoopThread()) mThread.join();
	std::lock_guard<std::mutex> lock(mMutex);
	mTasks.clear();
}

void EventLoop::postAt(int64_t dueMonoNs, Task task)
{
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mStopping) return;
		mTasks.emplace(dueMonoNs, std::move(task));
	}
	mCv.notify_one();
}

void EventLoop::postAfterMs(int64_t delayMs, Task task)
{
	postAt(monoNs() + delayMs * kNsPerMs, std::move(task));
}

void EventLoop::run()
{
	std::unique_lock<std::mutex> lock(mMutex);
	while (!mStopping)
	{
		if (mTasks.empty())
		{
			mCv.wait(lock);
			continue;
		}
		auto first = mTasks.begin();
		const int64_t now = monoNs();
		if (first->first > now)
		{
			mCv.wait_for(lock, std::chrono::nanoseconds(first->first - now));
			continue;
		}
		Task task = std::move(first->second);
		mTasks.erase(first);
		lock.unlock();
		task();
		lock.lock();
	}
}

} // namespace nmqe
