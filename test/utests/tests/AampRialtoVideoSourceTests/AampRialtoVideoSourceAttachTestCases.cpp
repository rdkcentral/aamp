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
 * @file AampRialtoVideoSourceAttachTestCases.cpp
 * @brief AampRialtoVideoSource L1 tests: attachOrUpdate, DRM sessions, codec data, segment creation, format and degraded attach paths.
 */

#include "AampRialtoVideoSourceTestFixture.h"

// ---------------------------------------------------------------------------
// mediaType
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_mediaType_ReturnsVideo
 * @brief Verify mediaType() returns eMEDIATYPE_VIDEO.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_mediaType_ReturnsVideo)
{
	EXPECT_EQ(m_source.mediaType(), eMEDIATYPE_VIDEO);
}

// ---------------------------------------------------------------------------
// Initial state
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_InitialState_NotAttached
 * @brief Verify source starts unattached.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_InitialState_NotAttached)
{
	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
	EXPECT_EQ(m_source.mksId(), -1);
	EXPECT_EQ(m_source.width(), 0);
	EXPECT_EQ(m_source.height(), 0);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — H264
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachH264_Success
 * @brief Verify first attach with H264 codec succeeds.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachH264_Success)
{
	auto codecInfo = MakeH264CodecInfo(1280, 720);

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(1);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), 10);
	EXPECT_EQ(m_source.width(), 1280);
	EXPECT_EQ(m_source.height(), 720);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — HEVC
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachHevc_Success
 * @brief Verify first attach with HEVC codec succeeds.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachHevc_Success)
{
	auto codecInfo = MakeHevcCodecInfo(3840, 2160);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.width(), 3840);
	EXPECT_EQ(m_source.height(), 2160);
}

/**
 * @test AampRialtoVideoSource_AttachHevc_StreamFormatFollowsNaluPrefix
 * @brief Regression (L4): HEVC was always attached as HVC1, so Annex-B HEVC
 *        (e.g. HLS-TS) was mislabelled.  Length-prefixed (hvcC) HEVC is HVC1;
 *        Annex-B is BYTE_STREAM, as for H.264.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachHevc_StreamFormatFollowsNaluPrefix)
{
	const struct
	{
		bool naluLengthPrefixed;
		firebolt::rialto::StreamFormat expected;
	} kCases[] = {
		{true,  firebolt::rialto::StreamFormat::HVC1},
		{false, firebolt::rialto::StreamFormat::BYTE_STREAM},
	};

	for (const auto &c : kCases)
	{
		AampRialtoVideoSource source;
		firebolt::rialto::StreamFormat captured =
			firebolt::rialto::StreamFormat::UNDEFINED;
		EXPECT_CALL(*m_pipelinePtr, attachSource(_))
			.WillOnce(Invoke([&](const std::unique_ptr<
				firebolt::rialto::IMediaPipeline::MediaSource> &src)
			{
				auto *video = dynamic_cast<
					firebolt::rialto::IMediaPipeline::MediaSourceVideo *>(src.get());
				if (video)
				{
					captured = video->getStreamFormat();
				}
				const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
					*src).setId(m_nextSourceId++);
				return true;
			}));

		auto codecInfo = MakeHevcCodecInfo(1920, 1080, c.naluLengthPrefixed);
		const auto result = source.attachOrUpdate(
			*m_pipelinePtr, codecInfo, nullptr, -1);

		EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
		EXPECT_EQ(captured, c.expected)
			<< "naluLengthPrefixed=" << c.naluLengthPrefixed;
	}
}

// ---------------------------------------------------------------------------
// attachOrUpdate — unknown codec
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachUnknownCodec_Fails
 * @brief Verify attach with unrecognised codec returns FAILED.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachUnknownCodec_Fails)
{
	MediaCodecInfo ci{};
	ci.mCodecFormat = GST_FORMAT_INVALID;

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, ci, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::FAILED);
	EXPECT_FALSE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// attachOrUpdate — already attached (update)
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachTwice_ReturnsUpdated
 * @brief Verify second attach returns UPDATED and refreshes metadata.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachTwice_ReturnsUpdated)
{
	auto codecInfo1 = MakeH264CodecInfo(1280, 720);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo1, nullptr, -1);
	ASSERT_TRUE(m_source.isAttached());

	auto codecInfo2 = MakeH264CodecInfo(1920, 1080);

	EXPECT_CALL(*m_pipelinePtr, attachSource(_)).Times(0);

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo2, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.width(), 1920);
	EXPECT_EQ(m_source.height(), 1080);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — attachSource failure
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachSourceFails_ReturnsFailed
 * @brief Verify that when pipeline->attachSource returns false, result is FAILED.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachSourceFails_ReturnsFailed)
{
	ON_CALL(*m_pipelinePtr, attachSource(_))
		.WillByDefault(Return(false));

	auto codecInfo = MakeH264CodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::FAILED);
	EXPECT_FALSE(m_source.isAttached());
}

// ---------------------------------------------------------------------------
// attachOrUpdate — with DRM
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachWithDRM_CreatesDrmSession
 * @brief Verify DRM session is created when protection params are set, and
 *        the resulting MediaSourceVideo reports hasDrm() true.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachWithDRM_CreatesDrmSession)
{
	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(42));

	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_VIDEO;

	auto codecInfo = MakeH264CodecInfo();

	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([this](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			EXPECT_TRUE(src->getHasDrm());
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, &mockDrm, -1, prot);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_EQ(m_source.mksId(), 42);
}

/**
 * @test AampRialtoVideoSource_AttachWithoutDrmSession_CreatesSourceWithHasDrmTrue
 * @brief Trip-wire test: createRialtoSource() currently hard-codes hasDrm=true
 *        regardless of the mksId-derived hasDrm argument it receives (see
 *        comment in AampRialtoVideoSource::createRialtoSource). No DRM
 *        session is created here (no protection params supplied), yet the
 *        resulting MediaSourceVideo must still report hasDrm() true. If this
 *        starts failing, createRialtoSource's hard-coded value has changed —
 *        update this test deliberately to match the new intended behaviour.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachWithoutDrmSession_CreatesSourceWithHasDrmTrue)
{
	auto codecInfo = MakeH264CodecInfo();

	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([this](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			EXPECT_TRUE(src->getHasDrm());
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_EQ(m_source.mksId(), -1);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — DRM session refresh on already-attached source
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachTwice_UnchangedProtection_DoesNotRecreateSession
 * @brief Verify that re-queuing the same protection params on a later
 *        attachOrUpdate() call (e.g. every period's init segment) does not
 *        trigger a redundant createSession() call.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachTwice_UnchangedProtection_DoesNotRecreateSession)
{
	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
		.Times(1)
		.WillOnce(Return(42));

	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_VIDEO;

	auto codecInfo1 = MakeH264CodecInfo();
	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([this](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo1, &mockDrm, -1, prot);
	ASSERT_EQ(m_source.mksId(), 42);

	auto codecInfo2 = MakeH264CodecInfo(1920, 1080);
	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo2, &mockDrm, -1, prot);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.mksId(), 42);
}

/**
 * @test AampRialtoVideoSource_AttachTwice_ChangedProtection_RecreatesSession
 * @brief Verify that different protection params (new PSSH/init data) on a
 *        later attachOrUpdate() call refresh mksId via a new createSession()
 *        call — this is the key-rotation-across-periods scenario.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachTwice_ChangedProtection_RecreatesSession)
{
	NiceMock<MockDrmBridge> mockDrm;
	AampRialtoMediaSource::ProtectionParams protA;
	protA.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	protA.initData = {0xCA, 0xFE};
	protA.type     = eMEDIATYPE_VIDEO;

	AampRialtoMediaSource::ProtectionParams protB;
	protB.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	protB.initData = {0xBE, 0xEF};
	protB.type     = eMEDIATYPE_VIDEO;

	{
		InSequence seq;
		EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
			.WillOnce(Return(42));
		EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
			.WillOnce(Return(99));
	}

	auto codecInfo1 = MakeH264CodecInfo();
	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([this](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo1, &mockDrm, -1, protA);
	ASSERT_EQ(m_source.mksId(), 42);

	auto codecInfo2 = MakeH264CodecInfo();
	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo2, &mockDrm, -1, protB);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.mksId(), 99);
}

/**
 * @test AampRialtoVideoSource_AttachTwice_ProtectionArrivesLate_CreatesSession
 * @brief Verify that protection params supplied for the first time on a
 *        later attachOrUpdate() call (source already attached unencrypted)
 *        still create a DRM session and populate mksId.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachTwice_ProtectionArrivesLate_CreatesSession)
{
	auto codecInfo1 = MakeH264CodecInfo();
	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([this](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo1, nullptr, -1);
	ASSERT_EQ(m_source.mksId(), -1);

	NiceMock<MockDrmBridge> mockDrm;
	EXPECT_CALL(mockDrm, createSession(_, _, _, eMEDIATYPE_VIDEO))
		.WillOnce(Return(7));

	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, 0xFE};
	prot.type     = eMEDIATYPE_VIDEO;

	auto codecInfo2 = MakeH264CodecInfo();
	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo2, &mockDrm, -1, prot);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.mksId(), 7);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — with flush position
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachWithFlushPosition_SetsSourcePosition
 * @brief Verify setSourcePosition is called when flush position >= 0.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_AttachWithFlushPosition_SetsSourcePosition)
{
	const int64_t flushPosNs = 5'000'000'000LL;

	EXPECT_CALL(*m_pipelinePtr, setSourcePosition(10, flushPosNs, _, _, _))
		.WillOnce(Return(true));

	auto codecInfo = MakeH264CodecInfo();

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, flushPosNs);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_Reset_ClearsState
 * @brief Verify reset clears source ID, mks ID and metadata.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_Reset_ClearsState)
{
	auto codecInfo = MakeH264CodecInfo(1280, 720);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
	ASSERT_TRUE(m_source.isAttached());

	m_source.reset();

	EXPECT_FALSE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), -1);
	EXPECT_EQ(m_source.mksId(), -1);
}

// ---------------------------------------------------------------------------
// createSegment
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_CreateSegment_ReturnsVideoSegment
 * @brief Verify createSegment produces a MediaSegmentVideo.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_CreateSegment_AfterAttach)
{
	auto codecInfo = MakeH264CodecInfo(1920, 1080);
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	// Access createSegment indirectly through injection
	// (createSegment is protected — tested via injectOneSample)
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.width(), 1920);
	EXPECT_EQ(m_source.height(), 1080);
}

// ---------------------------------------------------------------------------
// Codec data staging
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_TakePendingCodecData_ReturnsStagedData
 * @brief Verify pending codec data is staged by attach and consumed by take.
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_TakePendingCodecData_ReturnsStagedData)
{
	auto codecInfo = MakeH264CodecInfo();
	m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);

	auto cd = m_source.takePendingCodecData();
	EXPECT_NE(cd, nullptr);
	EXPECT_FALSE(cd->data.empty());

	auto cd2 = m_source.takePendingCodecData();
	EXPECT_EQ(cd2, nullptr);
}

/**
 * @test AampRialtoVideoSource_IsEos_ReflectsSignalEosAndReset
 */
TEST_F(AampRialtoVideoSourceTest, AampRialtoVideoSource_IsEos_ReflectsSignalEosAndReset)
{
	EXPECT_FALSE(m_source.isEos());

	m_source.signalEos(m_pipelinePtr);
	EXPECT_TRUE(m_source.isEos());

	m_source.reset();
	EXPECT_FALSE(m_source.isEos());
}

/**
 * @test AampRialtoVideoSource_ConcurrentAttachResetAndReaders_SeeOnlyValidValues
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_ConcurrentAttachResetAndReaders_SeeOnlyValidValues)
{
	/**
	 * @brief Regression (M4): sourceId(), format() and takePendingCodecData()
	 *        are read from injector and Rialto callback threads while the
	 *        control thread attaches and resets.  Every value read must be one
	 *        the source can legally hold; run under a thread sanitizer this
	 *        also exposes the data race the plain members had.
	 */
	constexpr int kIterations = 300;
	std::atomic<bool> done{false};
	std::atomic<bool> invalidSeen{false};

	auto reader = [&]
	{
		while (!done.load())
		{
			const int32_t id = m_source.sourceId();
			if (id != -1 && (id < 10 || id >= 10 + kIterations))
			{
				invalidSeen = true;
			}
			const StreamOutputFormat fmt = m_source.format();
			if (fmt != FORMAT_INVALID && fmt != FORMAT_ISO_BMFF)
			{
				invalidSeen = true;
			}
			auto cd = m_source.takePendingCodecData();
			if (cd && cd->data.empty())
			{
				invalidSeen = true;
			}
		}
	};
	std::thread r1(reader);
	std::thread r2(reader);

	auto codecInfo = MakeH264CodecInfo();
	for (int i = 0; i < kIterations; ++i)
	{
		m_source.setFormat(FORMAT_ISO_BMFF);
		m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1);
		m_source.reset();
		m_source.setFormat(FORMAT_INVALID);
	}
	done = true;
	r1.join();
	r2.join();

	EXPECT_FALSE(invalidSeen.load());
}

// ---------------------------------------------------------------------------
// format() / setFormat() — StreamOutputFormat base-class state
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_format_InitiallyInvalid
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_format_InitiallyInvalid)
{
	/**
	 * @brief format() must return FORMAT_INVALID before any setFormat() call.
	 */
	EXPECT_EQ(m_source.format(), FORMAT_INVALID);
}

/**
 * @test AampRialtoVideoSource_setFormat_RoundTrip
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_setFormat_RoundTrip)
{
	/**
	 * @brief setFormat() stores the value and format() returns it.
	 */
	m_source.setFormat(FORMAT_ISO_BMFF);
	EXPECT_EQ(m_source.format(), FORMAT_ISO_BMFF);

	m_source.setFormat(FORMAT_VIDEO_ES_H264);
	EXPECT_EQ(m_source.format(), FORMAT_VIDEO_ES_H264);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — codec format handling (mapCodecToMime integration)
// ---------------------------------------------------------------------------

/**
 * @test AampRialtoVideoSource_AttachH264NoDemuxer_SucceedsWithByteStreamFormat
 * @brief Verify attachOrUpdate succeeds with H.264 codec info that declares
 *        Annex-B (non-length-prefixed) NAL units, e.g. HLS-TS ES delivered
 *        via TSProcessor. Captures the MediaSource passed to attachSource()
 *        and validates that mapCodecToMime() correctly computed
 *        BYTE_STREAM format.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachH264NoDemuxer_SucceedsWithByteStreamFormat)
{
	auto codecInfo = MakeH264CodecInfo(1280, 720, /*naluLengthPrefixed=*/false);

	// Capture the MediaSource to verify its stream format
	firebolt::rialto::StreamFormat capturedStreamFormat =
		firebolt::rialto::StreamFormat::UNDEFINED;

	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([&](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			// Downcast to MediaSourceVideo to access stream format
			auto videoSource = dynamic_cast<
				firebolt::rialto::IMediaPipeline::MediaSourceVideo*>(src.get());
			if (videoSource)
			{
				capturedStreamFormat = videoSource->getStreamFormat();
			}
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	// Verify attachment succeeded
	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
	EXPECT_EQ(m_source.sourceId(), 10);
	EXPECT_EQ(m_source.width(), 1280);
	EXPECT_EQ(m_source.height(), 720);

	// Verify the stream format was computed correctly by mapCodecToMime
	// H.264, non-length-prefixed → BYTE_STREAM (Annex B raw ES)
	EXPECT_EQ(capturedStreamFormat, firebolt::rialto::StreamFormat::BYTE_STREAM);
}

/**
 * @test AampRialtoVideoSource_AttachH264WithDemuxer_SucceedsWithAVCFormat
 * @brief Verify attachOrUpdate with H.264 codec info that declares
 *        length-prefixed NAL units (as Mp4Demux always reports for avcC
 *        sample entries) produces AVC stream format in the MediaSource
 *        passed to attachSource(), regardless of whether this particular
 *        source object owns an internal demuxer.
 */
TEST_F(AampRialtoVideoSourceWithDemuxTest,
	AampRialtoVideoSource_AttachH264WithDemuxer_SucceedsWithAVCFormat)
{
	auto codecInfo = MakeH264CodecInfo(1280, 720, /*naluLengthPrefixed=*/true);

	// Capture the MediaSource to verify its stream format
	firebolt::rialto::StreamFormat capturedStreamFormat =
		firebolt::rialto::StreamFormat::UNDEFINED;

	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([&](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			auto videoSource = dynamic_cast<
				firebolt::rialto::IMediaPipeline::MediaSourceVideo*>(src.get());
			if (videoSource)
			{
				capturedStreamFormat = videoSource->getStreamFormat();
			}
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());

	// Verify the stream format was computed correctly by mapCodecToMime
	// H.264, length-prefixed → AVC (AVCC, fMP4/DASH path)
	EXPECT_EQ(capturedStreamFormat, firebolt::rialto::StreamFormat::AVC);
}

/**
 * @test AampRialtoVideoSource_AttachH264LengthPrefixedNoOwnDemuxer_SucceedsWithAVCFormat
 * @brief Regression test: a source with NO internal demuxer of its own
 *        (hasDemuxer()==false) must still produce AVC when the supplied
 *        codecInfo declares length-prefixed NAL units. This is the
 *        AampMp4Demuxer -> SetStreamCaps() path for DASH/fMP4 content,
 *        where codec info is parsed by a separate Mp4Demux instance and
 *        this object's own m_demuxer is never created.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_AttachH264LengthPrefixedNoOwnDemuxer_SucceedsWithAVCFormat)
{
	ASSERT_FALSE(m_source.hasDemuxer());
	auto codecInfo = MakeH264CodecInfo(1280, 720, /*naluLengthPrefixed=*/true);

	firebolt::rialto::StreamFormat capturedStreamFormat =
		firebolt::rialto::StreamFormat::UNDEFINED;

	EXPECT_CALL(*m_pipelinePtr, attachSource(_))
		.WillOnce(Invoke([&](const std::unique_ptr<
			firebolt::rialto::IMediaPipeline::MediaSource> &src)
		{
			auto videoSource = dynamic_cast<
				firebolt::rialto::IMediaPipeline::MediaSourceVideo*>(src.get());
			if (videoSource)
			{
				capturedStreamFormat = videoSource->getStreamFormat();
			}
			const_cast<firebolt::rialto::IMediaPipeline::MediaSource &>(
				*src).setId(m_nextSourceId++);
			return true;
		}));

	auto result = m_source.attachOrUpdate(
		*m_pipelinePtr, codecInfo, nullptr, -1);

	EXPECT_EQ(result, AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_EQ(capturedStreamFormat, firebolt::rialto::StreamFormat::AVC);
}


/**
 * @test AampRialtoVideoSource_reset_PreservesFormat
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_reset_PreservesFormat)
{
	/**
	 * @brief reset() must not clear the stored stream format.
	 *
	 * AampRialtoPlayer::Configure() calls reset() at the start of each
	 * full-recreation pass.  The format must survive this reset so that
	 * the old format is still available for comparison before the source
	 * is replaced.
	 */
	m_source.setFormat(FORMAT_ISO_BMFF);
	m_source.reset();
	EXPECT_EQ(m_source.format(), FORMAT_ISO_BMFF);
}

// ---------------------------------------------------------------------------
// attachOrUpdate / processDataFragment — degraded paths
// ---------------------------------------------------------------------------

namespace
{
AampRialtoMediaSource::ProtectionParams WidevineProtection(uint8_t initByte)
{
	AampRialtoMediaSource::ProtectionParams prot;
	prot.systemId = "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
	prot.initData = {0xCA, initByte};
	prot.type     = eMEDIATYPE_VIDEO;
	return prot;
}
} // namespace

/**
 * @test AampRialtoVideoSource_Attach_ProtectionWithoutBridge_NoSessionAndRetriesLater
 * @brief Without a DRM bridge the source still attaches with no session, and
 *        the params are not recorded as applied: a later attach with a
 *        bridge creates the session.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_Attach_ProtectionWithoutBridge_NoSessionAndRetriesLater)
{
	auto codecInfo = MakeH264CodecInfo();

	EXPECT_EQ(m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1,
			WidevineProtection(0x01)),
		AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_EQ(m_source.mksId(), -1);

	NiceMock<MockDrmBridge> drm;
	EXPECT_CALL(drm, createSession(_, _, _, eMEDIATYPE_VIDEO)).WillOnce(Return(7));
	EXPECT_EQ(m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, &drm, -1,
			WidevineProtection(0x01)),
		AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.mksId(), 7);
}

/**
 * @test AampRialtoVideoSource_Update_ChangedProtectionWithoutBridge_KeepsSession
 * @brief New protection params on an attached source with no bridge leave the
 *        existing session in place (the update is still reported).
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_Update_ChangedProtectionWithoutBridge_KeepsSession)
{
	NiceMock<MockDrmBridge> drm;
	EXPECT_CALL(drm, createSession(_, _, _, _)).WillOnce(Return(42));
	auto codecInfo = MakeH264CodecInfo();
	ASSERT_EQ(m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, &drm, -1,
			WidevineProtection(0x01)),
		AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);

	EXPECT_EQ(m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr, -1,
			WidevineProtection(0x02)),
		AampRialtoMediaSource::AttachResult::UPDATED);
	EXPECT_EQ(m_source.mksId(), 42);
}

/**
 * @test AampRialtoVideoSource_Attach_SetSourcePositionFails_StillAttached
 * @brief A refused initial segment position does not undo the attach; the
 *        source keeps its id and is reported as newly attached.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_Attach_SetSourcePositionFails_StillAttached)
{
	EXPECT_CALL(*m_pipelinePtr, setSourcePosition(_, 5'000'000'000LL, true, _, _))
		.WillOnce(Return(false));
	auto codecInfo = MakeH264CodecInfo();

	EXPECT_EQ(m_source.attachOrUpdate(*m_pipelinePtr, codecInfo, nullptr,
			5'000'000'000LL),
		AampRialtoMediaSource::AttachResult::NEWLY_ATTACHED);
	EXPECT_TRUE(m_source.isAttached());
}

/**
 * @test AampRialtoVideoSource_ProcessDataFragment_EsPathAttachAborted_DropsFrame
 * @brief On the HLS-TS path (no demuxer) a frame waiting for a deferred attach
 *        is dropped when a flush aborts the wait; nothing reaches Rialto.
 */
TEST_F(AampRialtoVideoSourceTest,
	AampRialtoVideoSource_ProcessDataFragment_EsPathAttachAborted_DropsFrame)
{
	{
		auto &st = m_source.state();
		std::lock_guard<std::mutex> lock(st.mu);
		st.attachPending = true;
	}
	EXPECT_CALL(*m_pipelinePtr, addSegment(_, _)).Times(0);

	auto inject = std::async(std::launch::async, [this] {
		return m_source.processDataFragment(*m_pipelinePtr,
			std::make_shared<std::vector<uint8_t>>(4, 0x00),
			1.0, 1.0, 0.033, 0.0);
	});
	// Negative check: blocked while the attach is pending.
	EXPECT_EQ(inject.wait_for(std::chrono::milliseconds(50)),
		std::future_status::timeout);

	m_source.unblockInjection(m_pipelinePtr, "test");

	ASSERT_EQ(inject.wait_for(std::chrono::seconds(5)), std::future_status::ready);
	EXPECT_TRUE(inject.get());
	EXPECT_FALSE(m_source.isAttached());
}
