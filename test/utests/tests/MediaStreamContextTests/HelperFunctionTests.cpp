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
 * @file HelperFunctionTests.cpp
 * @brief L1 tests for Phase 2-REVISED helper functions:
 *        TransferFragmentBuffer, PopulateCommonMetadata, ProcessInitSegmentIfNeeded.
 *
 * These helpers were extracted from CacheFragment() and CacheFragmentChunk()
 * to reduce code duplication while preserving the separate buffer index
 * lifecycle contracts of each caching path.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MediaStreamContext.h"
#include "fragmentcollector_mpd.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"
#include "priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockMediaTrack.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockIsoBmffBuffer.h"

#include "StreamAbstractionAAMP.h"

using namespace testing;

// ============================================================================
// Test fixture
// ============================================================================

/**
 * @brief Test fixture for MediaStreamContext helper function tests.
 *
 * Sets up the minimal infrastructure required to test the static helper
 * methods: TransferFragmentBuffer, PopulateCommonMetadata, and
 * ProcessInitSegmentIfNeeded.
 */
class HelperFunctionTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(
			mPrivateInstanceAAMP, 123.45, 12.34);

		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockMediaTrack = new NiceMock<MockMediaTrack>();
		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();
		g_mockIsoBmffBuffer = new NiceMock<MockIsoBmffBuffer>();
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockMediaTrack;
		g_mockMediaTrack = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockIsoBmffBuffer;
		g_mockIsoBmffBuffer = nullptr;
	}

	PrivateInstanceAAMP *mPrivateInstanceAAMP{nullptr};
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD{nullptr};
};

// ============================================================================
// TransferFragmentBuffer tests
// ============================================================================

/**
 * @brief Chunk mode: payload is assigned into the cached fragment.
 *
 * Verifies that when isChunkMode is true, the chunk payload data is
 * assigned into the cached fragment buffer via assign().
 */
TEST_F(HelperFunctionTest, TransferFragmentBuffer_ChunkMode_AssignsData)
{
	CachedFragment cached;
	const uint8_t payload[] = {'c','h','u','n','k','-','d','a','t','a'};
	size_t payloadSize = sizeof(payload);

	MediaStreamContext::TransferFragmentBuffer(
		&cached, payload, nullptr, payloadSize, true);

	// Fake assign does pointer copy — verify size was recorded
	EXPECT_EQ(cached.fragment.size(), payloadSize);
}

/**
 * @brief Fragment mode: Replace is called to move download buffer data.
 *
 * Verifies that when isChunkMode is false and a download buffer is
 * provided, the data is moved via Replace() (zero-copy). The source
 * buffer is emptied and the destination receives the data.
 */
TEST_F(HelperFunctionTest, TransferFragmentBuffer_FragmentMode_ReplacesBuffer)
{
	CachedFragment cached;
	AampGrowableBuffer downloadBuffer("test-download");

	const char data[] = "fragment-data";
	downloadBuffer.assign(data, data + sizeof(data) - 1);

	MediaStreamContext::TransferFragmentBuffer(
		&cached, nullptr, &downloadBuffer, 0, false);

	EXPECT_EQ(downloadBuffer.size(), 0u);
	// Destination should have the data
	EXPECT_GT(cached.fragment.size(), 0u);
}

/**
 * @brief Fragment mode with null download buffer: No-op.
 *
 * Verifies that when isChunkMode is false but downloadBuffer is nullptr,
 * TransferFragmentBuffer does nothing (no crash, no change).
 */
TEST_F(HelperFunctionTest, TransferFragmentBuffer_FragmentMode_NullBuffer_NoOp)
{
	CachedFragment cached;

	MediaStreamContext::TransferFragmentBuffer(
		&cached, nullptr, nullptr, 0, false);

	EXPECT_EQ(cached.fragment.size(), 0u);
}

/**
 * @brief Chunk mode with zero-length payload: assign with size 0.
 *
 * Verifies that a zero-size chunk payload is handled gracefully.
 */
TEST_F(HelperFunctionTest, TransferFragmentBuffer_ChunkMode_ZeroSize)
{
	CachedFragment cached;
	const uint8_t data[] = {'d','a','t','a'};

	MediaStreamContext::TransferFragmentBuffer(
		&cached, data, nullptr, 0, true);

	EXPECT_EQ(cached.fragment.size(), 0u);
}

// ============================================================================
// PopulateCommonMetadata tests
// ============================================================================

/**
 * @brief All common metadata fields are set correctly.
 *
 * Verifies that type, uri, initFragment, profileIndex, and discontinuity
 * are populated as specified by the caller.
 */
TEST_F(HelperFunctionTest, PopulateCommonMetadata_SetsAllFields)
{
	CachedFragment cached;
	std::string url = "http://example.com/segment.m4s";

	MediaStreamContext::PopulateCommonMetadata(
		&cached, url, eMEDIATYPE_VIDEO, 3, true, true);

	EXPECT_EQ(cached.type, eMEDIATYPE_VIDEO);
	EXPECT_EQ(cached.uri, url);
	EXPECT_TRUE(cached.initFragment);
	EXPECT_EQ(cached.profileIndex, 3);
	EXPECT_TRUE(cached.discontinuity);
}

/**
 * @brief Timing fields are NOT modified by PopulateCommonMetadata.
 *
 * Verifies that position, duration, and absPosition retain their
 * initial values (the helper respects lifecycle separation).
 */
TEST_F(HelperFunctionTest, PopulateCommonMetadata_DoesNotSetTimingFields)
{
	CachedFragment cached;
	// Set timing fields to known non-default values
	cached.position = 42.0;
	cached.duration = 2.5;
	cached.absPosition = 100.0;

	MediaStreamContext::PopulateCommonMetadata(
		&cached, "http://test.com/frag", eMEDIATYPE_AUDIO, 0, false, false);

	// Timing fields must be unchanged
	EXPECT_DOUBLE_EQ(cached.position, 42.0);
	EXPECT_DOUBLE_EQ(cached.duration, 2.5);
	EXPECT_DOUBLE_EQ(cached.absPosition, 100.0);
}

/**
 * @brief Default/false values are propagated correctly.
 *
 * Verifies that passing false for boolean flags results in false on the
 * cached fragment.
 */
TEST_F(HelperFunctionTest, PopulateCommonMetadata_DefaultFlags)
{
	CachedFragment cached;
	// Pre-set to true to verify they get overwritten
	cached.initFragment = true;
	cached.discontinuity = true;

	MediaStreamContext::PopulateCommonMetadata(
		&cached, "", eMEDIATYPE_AUDIO, 0, false, false);

	EXPECT_FALSE(cached.initFragment);
	EXPECT_FALSE(cached.discontinuity);
}

/**
 * @brief Empty URL is handled without issues.
 */
TEST_F(HelperFunctionTest, PopulateCommonMetadata_EmptyUrl)
{
	CachedFragment cached;

	MediaStreamContext::PopulateCommonMetadata(
		&cached, "", eMEDIATYPE_VIDEO, 0, false, false);

	EXPECT_TRUE(cached.uri.empty());
}

// ============================================================================
// ProcessInitSegmentIfNeeded tests
// ============================================================================

/**
 * @brief Non-init segment: early return, no parsing.
 *
 * Verifies that when isInitSegment is false, no ISO BMFF parsing occurs
 * and no timescale methods are called.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_NotInitSegment_NoOp)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_VIDEO;

	// When isInitSegment is false, we expect an early return and no ISO BMFF
	// parsing. Note: g_mockIsoBmffBuffer is a NiceMock, so this test verifies
	// the return value but does not use strict expectations to catch calls.
	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, false);

	EXPECT_EQ(result, 0u);
}

/**
 * @brief Init segment with non-init media type: early return, no parsing.
 *
 * Verifies that regular (non-init) media types cause early return even
 * when isInitSegment flag is true.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_NonInitType_NoOp)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_VIDEO;  // Not an INIT type

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, 0u);
}

/**
 * @brief Video init segment: timescale extracted and returned.
 *
 * Verifies the full happy path for video init segments: ISO BMFF buffer
 * is parsed, timescale is extracted, and the correct value is returned.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_VideoInit_SetsTimescale)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_INIT_VIDEO;
	// Put some dummy data in the fragment buffer so setBuffer has something
	const char data[] = "fake-init-data";
	cached.fragment.assign(data, data + sizeof(data) - 1);

	constexpr uint32_t expectedTimeScale = 90000;

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(A<const uint8_t*>(), A<size_t>())).Times(1);
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment())
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_))
		.WillOnce(DoAll(SetArgReferee<0>(expectedTimeScale), Return(true)));

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, expectedTimeScale);
}

/**
 * @brief Audio init segment: timescale extracted and returned.
 *
 * Verifies the happy path for audio init segments.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_AudioInit_SetsTimescale)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_INIT_AUDIO;
	const char data[] = "fake-init-data";
	cached.fragment.assign(data, data + sizeof(data) - 1);

	constexpr uint32_t expectedTimeScale = 48000;

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(A<const uint8_t*>(), A<size_t>())).Times(1);
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment())
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_))
		.WillOnce(DoAll(SetArgReferee<0>(expectedTimeScale), Return(true)));

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, expectedTimeScale);
}

/**
 * @brief Subtitle init segment: timescale extracted and returned.
 *
 * Verifies the happy path for subtitle init segments.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_SubtitleInit_SetsTimescale)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_INIT_SUBTITLE;
	const char data[] = "fake-init-data";
	cached.fragment.assign(data, data + sizeof(data) - 1);

	constexpr uint32_t expectedTimeScale = 1000;

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(A<const uint8_t*>(), A<size_t>())).Times(1);
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment())
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_))
		.WillOnce(DoAll(SetArgReferee<0>(expectedTimeScale), Return(true)));

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, expectedTimeScale);
}

/**
 * @brief Init segment but ISO BMFF reports not-init: no timescale set.
 *
 * Verifies that when buffer.isInitSegment() returns false, no timescale
 * method is called even though the caller flagged it as init.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_BufferNotInit_NoTimescale)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_INIT_VIDEO;
	const char data[] = "not-really-init";
	cached.fragment.assign(data, data + sizeof(data) - 1);

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(A<const uint8_t*>(), A<size_t>())).Times(1);
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment())
		.WillOnce(Return(false));
	// No getTimeScale calls expected

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, 0u);
}

/**
 * @brief Init segment but getTimeScale fails: no timescale set.
 *
 * Verifies graceful handling when timescale extraction fails.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_GetTimeScaleFails_NoTimescale)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_INIT_AUDIO;
	const char data[] = "corrupt-init";
	cached.fragment.assign(data, data + sizeof(data) - 1);

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(A<const uint8_t*>(), A<size_t>())).Times(1);
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment())
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_))
		.WillOnce(Return(false));
	// Returns 0, caller won't set timescale

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, 0u);
}

/**
 * @brief ISO BMFF parseBuffer fails: graceful degradation, no timescale set.
 *
 * Verifies that when parseBuffer() returns false, ProcessInitSegmentIfNeeded
 * returns 0 and does not attempt to inspect the buffer further.
 */
TEST_F(HelperFunctionTest, ProcessInitSegmentIfNeeded_ParseBufferFails_GracefulNoTimescale)
{
	CachedFragment cached;
	cached.type = eMEDIATYPE_INIT_VIDEO;
	const char data[] = "bad-init";
	cached.fragment.assign(data, data + sizeof(data) - 1);

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(A<const uint8_t*>(), A<size_t>())).Times(1);
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).Times(0);
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).Times(0);

	uint32_t result = MediaStreamContext::ProcessInitSegmentIfNeeded(
		&cached, true);

	EXPECT_EQ(result, 0u);
}
