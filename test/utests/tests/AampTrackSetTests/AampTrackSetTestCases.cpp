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

/**
 * @file AampTrackSetTestCases.cpp
 * @brief Unit tests for AampTrackSet and AampAtomicTrackSet.
 *
 * Contract under test:
 *  - A set holds any subset of the three track types (video, audio, subtitle).
 *  - Iteration visits the members in ascending media-type order (video, audio,
 *    subtitle), and Reversed() visits them in descending order.
 *  - Media types that are not track types are never members.
 *  - AampAtomicTrackSet hands out snapshots, and concurrent Add() calls for
 *    different types never lose an update.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>
#include "AampTrackSet.h"

/**
 * @brief Prints a set as its member media types, e.g. {0, 2}, in failure messages.
 * @param[in] set The set to print
 * @param[out] os Stream to print to
 */
inline void PrintTo(const AampTrackSet &set, std::ostream *os)
{
	*os << "{";
	const char *separator = "";
	for (const AampMediaType type : set)
	{
		*os << separator << static_cast<int>(type);
		separator = ", ";
	}
	*os << "}";
}

namespace
{
	/** Track types in the order iteration must visit them. */
	const std::vector<AampMediaType> TRACK_TYPES{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE};

	/** Media types that are not tracks and so must never be set members. 31 is the largest value the enum can hold. */
	const std::vector<AampMediaType> NON_TRACK_TYPES{eMEDIATYPE_RESERVED, eMEDIATYPE_MANIFEST, eMEDIATYPE_LICENCE,
		eMEDIATYPE_IFRAME, eMEDIATYPE_INIT_VIDEO, eMEDIATYPE_PLAYLIST_VIDEO, eMEDIATYPE_DEFAULT,
		static_cast<AampMediaType>(31)};

	/** @return the track types selected by the low bits of selection (bit n selects TRACK_TYPES[n]) in ascending order */
	std::vector<AampMediaType> TypesSelectedBy(unsigned selection)
	{
		std::vector<AampMediaType> selected;
		for (size_t index = 0; index < TRACK_TYPES.size(); ++index)
		{
			if (0u != (selection & (1u << index)))
			{
				selected.push_back(TRACK_TYPES[index]);
			}
		}
		return selected;
	}

	AampTrackSet SetOf(const std::vector<AampMediaType> &types)
	{
		AampTrackSet set;
		for (const AampMediaType type : types)
		{
			set = set.With(type);
		}
		return set;
	}

	std::vector<AampMediaType> Visited(const AampTrackSet &set)
	{
		std::vector<AampMediaType> visited;
		for (const AampMediaType type : set)
		{
			visited.push_back(type);
		}
		return visited;
	}

	std::vector<AampMediaType> VisitedReversed(const AampTrackSet &set)
	{
		std::vector<AampMediaType> visited;
		for (const AampMediaType type : set.Reversed())
		{
			visited.push_back(type);
		}
		return visited;
	}
}

/**
 * @brief Every one of the 8 possible subsets of the track types.
 */
class AampTrackSetSubsetTests : public ::testing::TestWithParam<unsigned>
{
};

/**
 * @brief Membership, size, emptiness and both iteration orders agree with the selected subset.
 */
TEST_P(AampTrackSetSubsetTests, Subset_QueriesAndIterationMatchSelection)
{
	const std::vector<AampMediaType> expected = TypesSelectedBy(GetParam());
	std::vector<AampMediaType> expectedReversed{expected};
	std::reverse(expectedReversed.begin(), expectedReversed.end());

	const AampTrackSet set = SetOf(expected);

	EXPECT_EQ(set.Size(), expected.size());
	EXPECT_EQ(set.Empty(), expected.empty());
	for (const AampMediaType type : TRACK_TYPES)
	{
		const bool selected = (std::find(expected.begin(), expected.end(), type) != expected.end());
		EXPECT_EQ(set.Contains(type), selected) << "type " << static_cast<int>(type);
	}
	EXPECT_EQ(Visited(set), expected);
	EXPECT_EQ(VisitedReversed(set), expectedReversed);
}

INSTANTIATE_TEST_SUITE_P(AllSubsets, AampTrackSetSubsetTests, ::testing::Range(0u, 8u));

/**
 * @brief The order the types are listed in does not change the set or its iteration order.
 */
TEST(AampTrackSetTests, InitializerList_OrderOfListingDoesNotMatter)
{
	const AampTrackSet shuffled{eMEDIATYPE_SUBTITLE, eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO};
	const AampTrackSet sorted{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE};

	EXPECT_EQ(shuffled, sorted);
	EXPECT_EQ(Visited(shuffled), TRACK_TYPES);
}

/**
 * @brief Listing a type more than once counts it once.
 */
TEST(AampTrackSetTests, InitializerList_DuplicateTypeCountsOnce)
{
	const AampTrackSet set{eMEDIATYPE_AUDIO, eMEDIATYPE_AUDIO, eMEDIATYPE_AUDIO};

	EXPECT_EQ(set.Size(), 1u);
	EXPECT_EQ(Visited(set), std::vector<AampMediaType>{eMEDIATYPE_AUDIO});
}

/**
 * @brief Media types that are not tracks are never members, however they are offered.
 */
TEST(AampTrackSetTests, NonTrackTypes_AreNeverMembers)
{
	for (const AampMediaType type : NON_TRACK_TYPES)
	{
		SCOPED_TRACE(static_cast<int>(type));

		const AampTrackSet fromList{type};
		EXPECT_TRUE(fromList.Empty());
		EXPECT_FALSE(fromList.Contains(type));

		const AampTrackSet withVideo{eMEDIATYPE_VIDEO};
		EXPECT_EQ(withVideo.With(type), withVideo);
		EXPECT_EQ(withVideo.Without(type), withVideo);
		EXPECT_FALSE(withVideo.With(type).Contains(type));
	}
}

/**
 * @brief With() returns a set that also holds the type and leaves the original untouched.
 */
TEST(AampTrackSetTests, With_AddsTypeToCopyAndLeavesOriginalUnchanged)
{
	const AampTrackSet original{eMEDIATYPE_VIDEO};

	const AampTrackSet extended = original.With(eMEDIATYPE_SUBTITLE);

	EXPECT_EQ(extended, (AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_SUBTITLE}));
	EXPECT_EQ(original, AampTrackSet{eMEDIATYPE_VIDEO});
}

/**
 * @brief Adding a type that is already a member changes nothing.
 */
TEST(AampTrackSetTests, With_ExistingType_IsIdempotent)
{
	const AampTrackSet set{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO};

	EXPECT_EQ(set.With(eMEDIATYPE_AUDIO), set);
}

/**
 * @brief Without() removes only the named type and leaves the original untouched.
 */
TEST(AampTrackSetTests, Without_RemovesOnlyThatTypeAndLeavesOriginalUnchanged)
{
	const AampTrackSet original{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE};

	const AampTrackSet reduced = original.Without(eMEDIATYPE_AUDIO);

	EXPECT_EQ(reduced, (AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_SUBTITLE}));
	EXPECT_EQ(original.Size(), 3u);
}

/**
 * @brief Removing a type that is not a member changes nothing.
 */
TEST(AampTrackSetTests, Without_AbsentType_IsNoOp)
{
	const AampTrackSet set{eMEDIATYPE_VIDEO};

	EXPECT_EQ(set.Without(eMEDIATYPE_SUBTITLE), set);
}

/**
 * @brief Sets with different members are not equal.
 */
TEST(AampTrackSetTests, Equality_DifferentMembers_AreNotEqual)
{
	EXPECT_NE((AampTrackSet{eMEDIATYPE_VIDEO}), (AampTrackSet{eMEDIATYPE_AUDIO}));
	EXPECT_NE((AampTrackSet{eMEDIATYPE_VIDEO}), (AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO}));
	EXPECT_NE(AampTrackSet{}, (AampTrackSet{eMEDIATYPE_VIDEO}));
}

/**
 * @brief A newly constructed atomic set is empty.
 */
TEST(AampAtomicTrackSetTests, DefaultConstructed_IsEmpty)
{
	const AampAtomicTrackSet shared;

	EXPECT_TRUE(shared.Load().Empty());
}

/**
 * @brief Store() replaces the whole contents.
 */
TEST(AampAtomicTrackSetTests, Store_ReplacesContents)
{
	AampAtomicTrackSet shared;
	shared.Store({eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO});

	shared.Store({eMEDIATYPE_SUBTITLE});

	EXPECT_EQ(shared.Load(), AampTrackSet{eMEDIATYPE_SUBTITLE});
}

/**
 * @brief Add() accumulates types and adding a member again changes nothing.
 */
TEST(AampAtomicTrackSetTests, Add_AccumulatesTypesAndIsIdempotent)
{
	AampAtomicTrackSet shared;

	shared.Add(eMEDIATYPE_AUDIO);
	shared.Add(eMEDIATYPE_VIDEO);
	shared.Add(eMEDIATYPE_AUDIO);

	EXPECT_EQ(shared.Load(), (AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO}));
}

/**
 * @brief Clear() removes every member.
 */
TEST(AampAtomicTrackSetTests, Clear_RemovesEveryMember)
{
	AampAtomicTrackSet shared;
	shared.Store({eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE});

	shared.Clear();

	EXPECT_TRUE(shared.Load().Empty());
}

/**
 * @brief A snapshot taken by Load() is not changed by later updates, so a loop over it is stable.
 */
TEST(AampAtomicTrackSetTests, Load_SnapshotIsUnaffectedByLaterUpdates)
{
	AampAtomicTrackSet shared;
	shared.Store({eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO});
	const AampTrackSet snapshot = shared.Load();

	shared.Clear();
	shared.Add(eMEDIATYPE_SUBTITLE);

	EXPECT_EQ(snapshot, (AampTrackSet{eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO}));
	EXPECT_EQ(shared.Load(), AampTrackSet{eMEDIATYPE_SUBTITLE});
}

/**
 * @brief Threads adding different types at the same time never lose each other's update.
 *
 * A lost update needs unlucky timing, so a broken Add() can still pass a given run, but a
 * correct Add() can never fail it.
 */
TEST(AampAtomicTrackSetTests, Add_ConcurrentDistinctTypes_NoUpdateIsLost)
{
	constexpr int ITERATIONS = 2000;

	for (int iteration = 0; iteration < ITERATIONS; ++iteration)
	{
		AampAtomicTrackSet shared;
		std::atomic<bool> start{false};
		std::vector<std::thread> workers;
		for (const AampMediaType type : TRACK_TYPES)
		{
			workers.emplace_back([&shared, &start, type]()
			{
				while (!start.load())
				{
					std::this_thread::yield();
				}
				shared.Add(type);
			});
		}
		start.store(true);
		for (std::thread &worker : workers)
		{
			worker.join();
		}

		ASSERT_EQ(shared.Load().Size(), TRACK_TYPES.size()) << "iteration " << iteration;
	}
}
