#include "BoundedTrackQueue.h"

#include <algorithm>

namespace nmqe {

namespace {
uint64_t sizeOf(const ReceivedObject &object)
{
	return object.payload ? object.payload->size() : 0;
}
} // namespace

BoundedTrackQueue::Dropped BoundedTrackQueue::push(ReceivedObject object)
{
	std::lock_guard<std::mutex> lock(mMutex);
	Dropped dropped;
	const uint64_t incoming = sizeOf(object);
	const uint64_t incomingGroup = object.id.group;
	mBytes += incoming;
	mQueue.push_back(std::move(object));

	// Evict whole Groups from the front, never the Group the new Object belongs to.
	while (mBytes > mMaxBytes && mQueue.front().id.group != incomingGroup)
	{
		const uint64_t group = mQueue.front().id.group;
		dropped.groups.push_back(group);
		while (!mQueue.empty() && mQueue.front().id.group == group)
		{
			const uint64_t size = sizeOf(mQueue.front());
			mBytes -= size;
			dropped.bytes += size;
			++dropped.objects;
			mQueue.pop_front();
		}
	}
	mHighWater = std::max(mHighWater, mBytes);
	return dropped;
}

std::vector<ReceivedObject> BoundedTrackQueue::drain()
{
	std::lock_guard<std::mutex> lock(mMutex);
	std::vector<ReceivedObject> out(std::make_move_iterator(mQueue.begin()), std::make_move_iterator(mQueue.end()));
	mQueue.clear();
	mBytes = 0;
	return out;
}

uint64_t BoundedTrackQueue::bytes() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return mBytes;
}

size_t BoundedTrackQueue::size() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return mQueue.size();
}

uint64_t BoundedTrackQueue::highWaterBytes() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return mHighWater;
}

} // namespace nmqe
