#include "player/BoundedTrackQueue.h"

#include <gtest/gtest.h>

using nmqe::BoundedTrackQueue;
using nmqe::ReceivedObject;

namespace {
ReceivedObject object(uint64_t group, uint64_t id, size_t size)
{
	ReceivedObject o;
	o.id.track = "video0.m4s";
	o.id.group = group;
	o.id.object = id;
	o.payload = std::make_shared<const nmqe::ByteBuffer>(size, 0);
	return o;
}
} // namespace

TEST(BoundedTrackQueue, KeepsEverythingWithinTheCap)
{
	BoundedTrackQueue queue(300);
	EXPECT_EQ(queue.push(object(1, 0, 100)).objects, 0u);
	EXPECT_EQ(queue.push(object(2, 0, 100)).objects, 0u);
	EXPECT_EQ(queue.bytes(), 200u);
	EXPECT_EQ(queue.drain().size(), 2u);
	EXPECT_EQ(queue.bytes(), 0u);
}

TEST(BoundedTrackQueue, EvictsOldestWholeGroups)
{
	BoundedTrackQueue queue(250);
	queue.push(object(1, 0, 100));
	queue.push(object(1, 1, 100));
	auto dropped = queue.push(object(2, 0, 100));
	ASSERT_EQ(dropped.groups, std::vector<uint64_t>({1}));
	EXPECT_EQ(dropped.objects, 2u);
	EXPECT_EQ(dropped.bytes, 200u);
	auto remaining = queue.drain();
	ASSERT_EQ(remaining.size(), 1u);
	EXPECT_EQ(remaining.front().id.group, 2u);
}

TEST(BoundedTrackQueue, NeverEvictsTheIncomingGroup)
{
	BoundedTrackQueue queue(50);
	auto dropped = queue.push(object(7, 0, 100));
	EXPECT_EQ(dropped.objects, 0u);
	EXPECT_EQ(queue.size(), 1u);
	EXPECT_EQ(queue.highWaterBytes(), 100u);
}
