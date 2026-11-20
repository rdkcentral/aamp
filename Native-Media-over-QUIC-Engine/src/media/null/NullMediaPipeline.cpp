// Null Media Pipeline: consumes access units against a simulated presentation
// clock, with the same pull/need-data contract as a real backend. Used for
// timing and ordering validation and for replay -> null integration tests.

#include "NullMediaPipeline.h"

#include "common/Clock.h"
#include "common/Telemetry.h"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace nmqe::media {

namespace {

constexpr int64_t kTickMs = 10;
constexpr size_t kFramesPerRequest = 32;

class NullMediaPipeline final : public IMediaPipeline
{
public:
	NullMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, uint64_t maxQueuedBytes)
		: mClient(std::move(client)), mMaxQueuedBytes(maxQueuedBytes ? maxQueuedBytes : 4 * 1024 * 1024)
	{
	}

	~NullMediaPipeline() override { stop(); }

	bool load(bool) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mThread.joinable()) return true;
		mStopping = false;
		mThread = std::thread(&NullMediaPipeline::run, this);
		return true;
	}

	bool attachSource(MediaSource &source) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		source.id = mNextId++;
		mSources[source.id].type = source.type;
		return true;
	}

	bool removeSource(int32_t id) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mSources.erase(id) > 0;
	}

	bool allSourcesAttached() override { return true; }

	bool play() override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mWantPlaying = true;
		return true;
	}

	bool pause() override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		freezeLocked();
		mWantPlaying = false;
		return true;
	}

	bool stop() override
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			mStopping = true;
		}
		mCv.notify_all();
		if (mThread.joinable()) mThread.join();
		return true;
	}

	bool setPlaybackRate(double) override { return false; }

	bool setPosition(int64_t positionNs) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mBaseNs = positionNs;
		mFrozenNs = 0;
		return true;
	}

	bool getPosition(int64_t &positionNs) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		positionNs = positionLocked();
		return true;
	}

	bool getRenderedTimestamp(int32_t sourceId, int64_t &timeStampNs) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mSources.find(sourceId);
		if (it == mSources.end() || !it->second.rendered) return false;
		timeStampNs = it->second.lastRenderedNs;
		return true;
	}

	bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mSources.find(sourceId);
		if (it == mSources.end()) return false;
		renderedFrames = it->second.rendered;
		droppedFrames = 0;
		return true;
	}

	bool getQueuedBytes(int32_t sourceId, uint64_t &queuedBytes) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mSources.find(sourceId);
		if (it == mSources.end()) return false;
		queuedBytes = it->second.queuedBytes;
		return true;
	}

	bool haveData(MediaSourceStatus, uint32_t needDataRequestId) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		for (auto &[id, source] : mSources)
		{
			if (source.outstanding == needDataRequestId) source.outstanding = 0;
		}
		return true;
	}

	AddSegmentStatus addSegment(uint32_t, const MediaSegment &segment) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mSources.find(segment.sourceId);
		if (it == mSources.end()) return AddSegmentStatus::ERROR;
		it->second.queue.push_back({segment.timeStampNs, segment.size});
		it->second.queuedBytes += segment.size;
		return AddSegmentStatus::OK;
	}

	bool flush(int32_t sourceId, bool) override
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			auto it = mSources.find(sourceId);
			if (it == mSources.end()) return false;
			it->second.queue.clear();
			it->second.queuedBytes = 0;
		}
		if (auto client = mClient.lock()) client->notifySourceFlushed(sourceId);
		return true;
	}

	std::string describe() override
	{
		Json out;
		out.add("backend", "null").add("max_queued_bytes", mMaxQueuedBytes).add("tick_ms", kTickMs);
		return out.object();
	}

private:
	struct Unit
	{
		int64_t ptsNs;
		size_t size;
	};
	struct Source
	{
		MediaSourceType type = MediaSourceType::UNKNOWN;
		std::deque<Unit> queue;
		uint64_t queuedBytes = 0;
		uint64_t rendered = 0;
		int64_t lastRenderedNs = 0;
		uint32_t outstanding = 0;
		bool underflow = false;
	};

	int64_t positionLocked() const
	{
		if (!mClockRunning) return mBaseNs + mFrozenNs;
		return mBaseNs + mFrozenNs + (monoNs() - mClockStartMonoNs);
	}

	void freezeLocked()
	{
		if (!mClockRunning) return;
		mFrozenNs += monoNs() - mClockStartMonoNs;
		mClockRunning = false;
	}

	void run()
	{
		auto client = mClient.lock();
		std::unique_lock<std::mutex> lock(mMutex);
		while (!mStopping)
		{
			mCv.wait_for(lock, std::chrono::milliseconds(kTickMs), [this] { return mStopping; });
			if (mStopping) break;

			// Preroll: the clock starts once playing is requested and every source has data.
			if (mWantPlaying && !mClockRunning && !mSources.empty())
			{
				bool ready = true;
				for (auto &[id, source] : mSources) ready = ready && !source.queue.empty();
				if (ready)
				{
					mClockRunning = true;
					mClockStartMonoNs = monoNs();
					lock.unlock();
					if (client) client->notifyPlaybackState(PlaybackState::PLAYING);
					lock.lock();
				}
			}

			std::vector<std::pair<int32_t, uint32_t>> requests;
			std::vector<int32_t> firstFrames;
			std::vector<int32_t> underflows;
			const int64_t position = positionLocked();
			for (auto &[id, source] : mSources)
			{
				while (mClockRunning && !source.queue.empty() && source.queue.front().ptsNs <= position)
				{
					if (source.rendered++ == 0) firstFrames.push_back(id);
					source.lastRenderedNs = source.queue.front().ptsNs;
					source.queuedBytes -= source.queue.front().size;
					source.queue.pop_front();
				}
				const bool empty = source.queue.empty();
				if (mClockRunning && empty && !source.underflow) underflows.push_back(id);
				source.underflow = mClockRunning && empty;
				if (source.outstanding == 0 && source.queuedBytes < mMaxQueuedBytes)
				{
					source.outstanding = ++mRequestId;
					requests.emplace_back(id, source.outstanding);
				}
			}
			lock.unlock();
			if (client)
			{
				for (int32_t id : firstFrames) client->notifyFirstFrameReceived(id);
				for (int32_t id : underflows) client->notifyBufferUnderflow(id);
				for (auto [id, request] : requests) client->notifyNeedMediaData(id, kFramesPerRequest, request);
				client->notifyPosition(position);
			}
			lock.lock();
		}
	}

	std::weak_ptr<IMediaPipelineClient> mClient;
	const uint64_t mMaxQueuedBytes;
	std::mutex mMutex;
	std::condition_variable mCv;
	std::thread mThread;
	std::map<int32_t, Source> mSources;
	int32_t mNextId = 1;
	uint32_t mRequestId = 0;
	bool mStopping = false;
	bool mWantPlaying = false;
	bool mClockRunning = false;
	int64_t mClockStartMonoNs = 0;
	int64_t mBaseNs = 0;
	int64_t mFrozenNs = 0;
};

} // namespace

std::unique_ptr<IMediaPipeline> createNullMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, uint64_t maxQueuedBytes)
{
	return std::make_unique<NullMediaPipeline>(std::move(client), maxQueuedBytes);
}

} // namespace nmqe::media
