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

#include <gtest/gtest.h>
#include <vector>
#include "SegmentIndex.h"

namespace
{
// sidx version 0, timescale 1000, first_offset 0, two entries:
// entry 0: 16384 bytes, 2000 ticks; entry 1: 12288 bytes, 2000 ticks.
const std::vector<uint8_t> kSidx = {
	0x00, 0x00, 0x00, 0x38,
	0x73, 0x69, 0x64, 0x78,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x03, 0xE8,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x02,
	0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x07, 0xD0, 0x90, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x07, 0xD0, 0x90, 0x00, 0x00, 0x00,
};
constexpr uint64_t kSize0 = 16384;
constexpr uint64_t kSize1 = 12288;
constexpr size_t kFirstOffsetLastByte = 27;
}

/** @brief A default-constructed index is empty with a zero base offset. */
TEST(SegmentIndexTests, Default_IsEmptyWithZeroBase)
{
	SegmentIndex index;
	EXPECT_TRUE(index.Empty());
	EXPECT_EQ(index.BaseOffset(), 0u);
	EXPECT_TRUE(index.Snapshot().empty());
}

/** @brief LoadIfEmpty stores the first index and rejects a second one. */
TEST(SegmentIndexTests, LoadIfEmpty_StoresOnlyTheFirstIndex)
{
	SegmentIndex index;
	EXPECT_TRUE(index.LoadIfEmpty(std::vector<uint8_t>(kSidx)));
	EXPECT_FALSE(index.Empty());

	std::vector<uint8_t> other(8, 0xAA);
	EXPECT_FALSE(index.LoadIfEmpty(std::move(other)));
	EXPECT_EQ(index.Snapshot(), kSidx);
}

/** @brief Lookup returns the size and duration of an entry and fails past the last one. */
TEST(SegmentIndexTests, Lookup_ReturnsEntrySizeAndDuration)
{
	SegmentIndex index;
	index.LoadIfEmpty(std::vector<uint8_t>(kSidx));

	unsigned int size = 0;
	float duration = 0.0f;
	EXPECT_TRUE(index.Lookup(1, &size, &duration, nullptr));
	EXPECT_EQ(size, kSize1);
	EXPECT_FLOAT_EQ(duration, 2.0f);
	EXPECT_FALSE(index.Lookup(2, &size, &duration, nullptr));
}

/** @brief Lookup on an empty index fails. */
TEST(SegmentIndexTests, Lookup_OnEmptyIndexFails)
{
	SegmentIndex index;
	unsigned int size = 0;
	float duration = 0.0f;
	EXPECT_FALSE(index.Lookup(0, &size, &duration, nullptr));
}

/** @brief With no index loaded, AnchorAndOffsetOf returns its input and leaves the base at 0. */
TEST(SegmentIndexTests, AnchorAndOffsetOf_EmptyIndexReturnsInputAndLeavesBase)
{
	SegmentIndex index;
	EXPECT_EQ(index.AnchorAndOffsetOf(1000, 3), 1000u);
	EXPECT_EQ(index.BaseOffset(), 0u);
}

/** @brief AnchorAndOffsetOf records segment 0's offset and returns the start of the requested segment. */
TEST(SegmentIndexTests, AnchorAndOffsetOf_RecordsBaseAndSumsPrecedingSizes)
{
	SegmentIndex index;
	index.LoadIfEmpty(std::vector<uint8_t>(kSidx));

	EXPECT_EQ(index.AnchorAndOffsetOf(1000, 0), 1000u);
	EXPECT_EQ(index.BaseOffset(), 1000u);
	EXPECT_EQ(index.AnchorAndOffsetOf(1000, 1), 1000u + kSize0);
	EXPECT_EQ(index.AnchorAndOffsetOf(1000, 2), 1000u + kSize0 + kSize1);
	EXPECT_EQ(index.BaseOffset(), 1000u);
}

/** @brief The sidx first_offset field is added to the recorded base. */
TEST(SegmentIndexTests, AnchorAndOffsetOf_AddsTheFirstOffsetToTheBase)
{
	std::vector<uint8_t> sidx(kSidx);
	sidx[kFirstOffsetLastByte] = 100;
	SegmentIndex index;
	index.LoadIfEmpty(std::move(sidx));

	EXPECT_EQ(index.AnchorAndOffsetOf(1000, 1), 1100u + kSize0);
	EXPECT_EQ(index.BaseOffset(), 1100u);
}

/** @brief RangeOf reports a segment's offset from the base, plus its size and duration. */
TEST(SegmentIndexTests, RangeOf_ReturnsOffsetSizeAndDurationRelativeToBase)
{
	SegmentIndex index;
	index.LoadIfEmpty(std::vector<uint8_t>(kSidx));
	index.SetBaseOffset(1000);

	uint64_t offset = 0;
	unsigned int size = 0;
	float duration = 0.0f;
	EXPECT_TRUE(index.RangeOf(1, offset, size, duration));
	EXPECT_EQ(offset, 1000u + kSize0);
	EXPECT_EQ(size, kSize1);
	EXPECT_FLOAT_EQ(duration, 2.0f);
}

/** @brief RangeOf for a missing entry returns false but still reports the summed offset. */
TEST(SegmentIndexTests, RangeOf_MissingEntryFailsButStillReportsOffset)
{
	SegmentIndex index;
	index.LoadIfEmpty(std::vector<uint8_t>(kSidx));
	index.SetBaseOffset(1000);

	uint64_t offset = 0;
	unsigned int size = 0;
	float duration = 0.0f;
	EXPECT_FALSE(index.RangeOf(2, offset, size, duration));
	EXPECT_EQ(offset, 1000u + kSize0 + kSize1);
}

/** @brief Clear empties the index and zeroes the base offset, and a new index can then be loaded. */
TEST(SegmentIndexTests, Clear_ReleasesTheBufferAndZeroesTheBase)
{
	SegmentIndex index;
	index.LoadIfEmpty(std::vector<uint8_t>(kSidx));
	index.SetBaseOffset(1000);

	index.Clear();

	EXPECT_TRUE(index.Empty());
	EXPECT_EQ(index.BaseOffset(), 0u);
	EXPECT_TRUE(index.LoadIfEmpty(std::vector<uint8_t>(kSidx)))
		<< "A cleared index must accept a new load";
}
