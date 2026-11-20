#pragma once

#include "common/ReceivedObject.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace nmqe {

/**
 * Hand-off queue between an Object source thread and the player loop for one
 * track. Bounded by bytes: an overflowing push never blocks; it evicts the
 * oldest whole Groups (keeping the newest) so what remains starts at a Group
 * boundary, and reports what it dropped.
 */
class BoundedTrackQueue
{
public:
	struct Dropped
	{
		uint64_t objects = 0;
		uint64_t bytes = 0;
		std::vector<uint64_t> groups;
	};

	explicit BoundedTrackQueue(uint64_t maxBytes) : mMaxBytes(maxBytes) {}

	/** Enqueues; returns what was evicted to stay within the byte cap. */
	Dropped push(ReceivedObject object);
	/** Takes every queued Object, oldest first. */
	std::vector<ReceivedObject> drain();

	uint64_t bytes() const;
	size_t size() const;
	uint64_t maxBytes() const { return mMaxBytes; }
	uint64_t highWaterBytes() const;

private:
	mutable std::mutex mMutex;
	std::deque<ReceivedObject> mQueue;
	uint64_t mBytes = 0;
	uint64_t mHighWater = 0;
	const uint64_t mMaxBytes;
};

} // namespace nmqe
