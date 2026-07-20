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
 * @file AampCMCDCollectorTestCases.cpp
 * @brief Tests for AampCMCDCollector CTA-5004 header output.
 *
 * Asserts the exact header strings the collector emits through its public
 * API: quoted String keys, alphabetical key order within each header,
 * per-key rounding (bl/dl/mtp/rtp), the optional-key omission rule, the bs
 * latch, and the sourcing of the session keys (sf/st/cid/pr) and request
 * keys (d/mtp/su).
 */

#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AampCMCDCollector.h"
#include "AampMediaType.h"

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

namespace
{
	const std::string kTraceId{"test-trace-id"};
	// Session header before any session params are pushed: sid (quoted) and v=1 only.
	const std::string kBareSessionHeader{"CMCD-Session: sid=\"" + kTraceId + "\",v=1"};
}

class AampCMCDCollectorTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mCollector = std::make_unique<AampCMCDCollector>();
	}

	void InitEnabled(std::string traceId = kTraceId)
	{
		mCollector->Initialize(true, traceId);
	}

	std::vector<std::string> GetHeaders(AampMediaType mediaType)
	{
		std::vector<std::string> headers;
		mCollector->CMCDGetHeaders(mediaType, headers);
		return headers;
	}

	std::unique_ptr<AampCMCDCollector> mCollector;
};

TEST_F(AampCMCDCollectorTest, NotInitialized_EmitsNoHeaders)
{
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), IsEmpty());
}

TEST_F(AampCMCDCollectorTest, Disabled_EmitsNoHeaders)
{
	std::string traceId{kTraceId};
	mCollector->Initialize(false, traceId);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), IsEmpty());
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST), IsEmpty());
}

TEST_F(AampCMCDCollectorTest, UnknownMediaType_EmitsNoHeaders)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_DEFAULT), IsEmpty());
}

TEST_F(AampCMCDCollectorTest, Manifest_EmitsSessionAndObjectTypeM)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            UnorderedElementsAre("CMCD-Object: ot=m", kBareSessionHeader));
}

TEST_F(AampCMCDCollectorTest, PlaylistTypes_EmitManifestKeys)
{
	InitEnabled();

	for (AampMediaType mediaType : {eMEDIATYPE_PLAYLIST_VIDEO, eMEDIATYPE_PLAYLIST_AUDIO, eMEDIATYPE_PLAYLIST_SUBTITLE})
	{
		EXPECT_THAT(GetHeaders(mediaType),
		            UnorderedElementsAre("CMCD-Object: ot=m", kBareSessionHeader))
		    << "media type " << mediaType;
	}
}

TEST_F(AampCMCDCollectorTest, SubtitleTypes_EmitObjectTypeC)
{
	InitEnabled();

	// "c" = caption/subtitle per CTA-5004; the legacy "s" is not a defined ot token.
	for (AampMediaType mediaType : {eMEDIATYPE_SUBTITLE, eMEDIATYPE_INIT_SUBTITLE})
	{
		EXPECT_THAT(GetHeaders(mediaType),
		            UnorderedElementsAre("CMCD-Object: ot=c", kBareSessionHeader))
		    << "media type " << mediaType;
	}
}

TEST_F(AampCMCDCollectorTest, VideoDefaults_OmitUnavailableStandardKeys)
{
	InitEnabled();

	// No data collected yet: the standard keys (br/tb/bl and the other
	// request/status keys) are unknown and must be omitted rather than
	// reported as 0 (CTA-5004 optional-key rule). The vendor keys keep the
	// deployed always-present behaviour - downstream consumers may rely on it.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: ot=v",
	                "CMCD-Request: com.comcast-fb=0,com.comcast-lb=0",
	                kBareSessionHeader));
}

TEST_F(AampCMCDCollectorTest, VideoWithFullState_EmitsAllHeadersSortedAndTyped)
{
	InitEnabled();
	mCollector->CMCDSetSessionParams(eMEDIAFORMAT_DASH, "http://example.com/master.mpd?token=secret");
	mCollector->CMCDSetLiveStatus(true);
	mCollector->CMCDSetPlaybackRate(2.0f);
	mCollector->SetBitrates(eMEDIATYPE_VIDEO, {1000000, 5000000});
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2500000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 12);
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 3000, 2500);
	mCollector->CMCDSetFragmentDuration(eMEDIATYPE_VIDEO, 6006);
	mCollector->CMCDSetMeasuredThroughput(eMEDIATYPE_VIDEO, 4321);
	mCollector->CMCDSetStartupUrgent(eMEDIATYPE_VIDEO, true);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                // br/tb plain integers (no rounding clause); d plain ms; keys sorted
	                "CMCD-Object: br=2500,d=6006,ot=v,tb=5000",
	                // bl rounded to 100 ms; dl = bl / pr(2.0) rounded; mtp rounded to 100 kbps;
	                // nor quoted; su bare token; vendor keys plain unrounded
	                "CMCD-Request: bl=3000,com.comcast-dns=12,com.comcast-fb=34,com.comcast-lb=56,dl=1500,mtp=4300,nor=\"http://example.com/seg2.ts\",su",
	                // cid stripped of the query string and quoted; pr=2; sf=d; st=l
	                "CMCD-Session: cid=\"http://example.com/master.mpd\",pr=2,sf=d,sid=\"" + kTraceId + "\",st=l,v=1",
	                // bs latched from the starved SetTrackData; rtp = 2 x br rounded
	                "CMCD-Status: bs,rtp=5000"));
}

TEST_F(AampCMCDCollectorTest, BufferStarvation_IsLatchedAndReportedOnce)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 0, 0);
	// A later non-starved update must not clear the pending latch.
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 0, 0);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Status: bs"));
	// Consumed by the first report; the next request must not repeat it.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Not(Contains("CMCD-Status: bs")));
}

TEST_F(AampCMCDCollectorTest, DnsUnavailable_OmitsDnsKey)
{
	InitEnabled();
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2500000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 0);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: com.comcast-fb=34,com.comcast-lb=56,nor=\"http://example.com/seg2.ts\""));
}

TEST_F(AampCMCDCollectorTest, NextRange_EmitsNrrInsteadOfNor)
{
	InitEnabled();
	mCollector->CMCDSetNextRangeRequest("100-200", 2000000, eMEDIATYPE_VIDEO);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: br=2000,ot=v",
	                "CMCD-Request: com.comcast-fb=0,com.comcast-lb=0,nrr=\"100-200\"",
	                kBareSessionHeader,
	                "CMCD-Status: rtp=4000"));
}

TEST_F(AampCMCDCollectorTest, EmptyNextRange_IsIgnored)
{
	InitEnabled();
	mCollector->CMCDSetNextRangeRequest("", 2000000, eMEDIATYPE_VIDEO);

	// An empty next range is ignored entirely (no bitrate capture, no nrr);
	// nor stays omitted because the next URL is unknown, while the vendor
	// keys remain per the deployed behaviour.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: ot=v",
	                "CMCD-Request: com.comcast-fb=0,com.comcast-lb=0",
	                kBareSessionHeader));
}

TEST_F(AampCMCDCollectorTest, ObjectTypeTokens_PerMediaType)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_VIDEO), Contains("CMCD-Object: ot=i"));
	EXPECT_THAT(GetHeaders(eMEDIATYPE_IFRAME), Contains("CMCD-Object: ot=v"));
	EXPECT_THAT(GetHeaders(eMEDIATYPE_AUDIO), Contains("CMCD-Object: ot=a"));
	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_AUDIO), Contains("CMCD-Object: ot=i"));
}

TEST_F(AampCMCDCollectorTest, MuxedStream_LatchesObjectTypeAv)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 0, 0, true);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Object: ot=av"));

	// The muxed label is a one-way latch: reporting IsMuxed=false later does not restore ot=v.
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 0, 0, false);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Object: ot=av"));
}

TEST_F(AampCMCDCollectorTest, AudioTrackData_UpdatesBufferKeysOnly)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_AUDIO, true, 1500, 999);

	// The audio path ignores currentBitrate, so br/rtp stay omitted. bl is rounded
	// and dl = bl at the default 1x playback rate.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_AUDIO),
	            UnorderedElementsAre(
	                "CMCD-Object: ot=a",
	                "CMCD-Request: bl=1500,com.comcast-fb=0,com.comcast-lb=0,dl=1500",
	                kBareSessionHeader,
	                "CMCD-Status: bs"));
}

TEST_F(AampCMCDCollectorTest, BufferLength_RoundedToNearest100)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 3049, 0);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=3000,com.comcast-fb=0,com.comcast-lb=0,dl=3000"));

	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 3050, 0);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=3100,com.comcast-fb=0,com.comcast-lb=0,dl=3100"));
}

TEST_F(AampCMCDCollectorTest, Deadline_ScalesWithPlaybackRate)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 3000, 0);

	// 2x drains the buffer twice as fast: dl = bl / 2.
	mCollector->CMCDSetPlaybackRate(2.0f);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=3000,com.comcast-fb=0,com.comcast-lb=0,dl=1500"));

	// 0.5x drains half as fast: dl = 2 * bl.
	mCollector->CMCDSetPlaybackRate(0.5f);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=3000,com.comcast-fb=0,com.comcast-lb=0,dl=6000"));

	// Not playing (pr=0): the buffer is not draining, so no deadline exists.
	mCollector->CMCDSetPlaybackRate(0.0f);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=3000,com.comcast-fb=0,com.comcast-lb=0"));
}

TEST_F(AampCMCDCollectorTest, PlaybackRate_OmittedAtNormalRate)
{
	InitEnabled();

	// pr defaults to 1 (normal play) and is omitted per CTA-5004.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST), Contains(kBareSessionHeader));

	// Any non-1 rate is emitted, including 0 ("not playing").
	mCollector->CMCDSetPlaybackRate(0.5f);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: pr=0.5,sid=\"" + kTraceId + "\",v=1"));

	mCollector->CMCDSetPlaybackRate(0.0f);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: pr=0,sid=\"" + kTraceId + "\",v=1"));

	mCollector->CMCDSetPlaybackRate(1.0f);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST), Contains(kBareSessionHeader));
}

TEST_F(AampCMCDCollectorTest, SessionParams_MapFormatAndStripQueryFromCid)
{
	InitEnabled();
	mCollector->CMCDSetSessionParams(eMEDIAFORMAT_DASH, "http://example.com/master.mpd?token=secret#t=10");

	// The query string and fragment must not leak into cid.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: cid=\"http://example.com/master.mpd\",sf=d,sid=\"" + kTraceId + "\",v=1"));
}

TEST_F(AampCMCDCollectorTest, SessionParams_MapHlsToSfH)
{
	InitEnabled();
	mCollector->CMCDSetSessionParams(eMEDIAFORMAT_HLS, "http://example.com/master.m3u8");

	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: cid=\"http://example.com/master.m3u8\",sf=h,sid=\"" + kTraceId + "\",v=1"));
}

TEST_F(AampCMCDCollectorTest, LiveStatus_EmitsStreamTypeToken)
{
	InitEnabled();
	mCollector->CMCDSetLiveStatus(true);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: sid=\"" + kTraceId + "\",st=l,v=1"));

	mCollector->CMCDSetLiveStatus(false);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: sid=\"" + kTraceId + "\",st=v,v=1"));
}

TEST_F(AampCMCDCollectorTest, MeasuredThroughput_RoundedTo100Kbps)
{
	InitEnabled();
	mCollector->CMCDSetMeasuredThroughput(eMEDIATYPE_VIDEO, 4321);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: com.comcast-fb=0,com.comcast-lb=0,mtp=4300"));
}

TEST_F(AampCMCDCollectorTest, RequestedThroughput_IsTwiceBitrateRounded)
{
	InitEnabled();
	// 2549 kbps encoded -> rtp = 2 * 2549 = 5098 -> rounded 5100.
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2549000, eMEDIATYPE_VIDEO);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Status: rtp=5100"));
}

TEST_F(AampCMCDCollectorTest, StartupUrgent_EmitsSuOnMediaAndInitInstances)
{
	InitEnabled();
	mCollector->CMCDSetStartupUrgent(eMEDIATYPE_VIDEO, true);
	mCollector->CMCDSetStartupUrgent(eMEDIATYPE_INIT_VIDEO, true);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: com.comcast-fb=0,com.comcast-lb=0,su"));
	// Init instances carry su too (fetched at startup/seek/rebuffer), with the
	// other standard segment metrics still omitted.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: ot=i",
	                "CMCD-Request: com.comcast-fb=0,com.comcast-lb=0,su",
	                kBareSessionHeader));

	// Level-triggered: the engine recomputes su per request, so false clears it.
	mCollector->CMCDSetStartupUrgent(eMEDIATYPE_VIDEO, false);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: com.comcast-fb=0,com.comcast-lb=0"));
}

TEST_F(AampCMCDCollectorTest, FragmentDuration_EmittedAsPlainObjectKey)
{
	InitEnabled();
	mCollector->CMCDSetFragmentDuration(eMEDIATYPE_VIDEO, 6006);

	// d is a plain integer ms value - CTA-5004 defines no rounding for it.
	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Object: d=6006,ot=v"));
}

TEST_F(AampCMCDCollectorTest, SetBitrates_OnlyAppliesToVideoAndAudio)
{
	InitEnabled();
	mCollector->SetBitrates(eMEDIATYPE_INIT_VIDEO, {5000000});

	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_VIDEO), Contains("CMCD-Object: ot=i"));
}

TEST_F(AampCMCDCollectorTest, UnknownTraceId_GeneratesUuidSessionId)
{
	std::string traceId{"unknown"};
	mCollector->Initialize(true, traceId);

	EXPECT_NE(traceId, "unknown");
	EXPECT_EQ(traceId.length(), 36u);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            Contains("CMCD-Session: sid=\"" + traceId + "\",v=1"));
}

TEST_F(AampCMCDCollectorTest, Reinitialize_ResetsCollectedState)
{
	InitEnabled();
	mCollector->SetBitrates(eMEDIATYPE_VIDEO, {5000000});
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 3000, 2500, true);
	mCollector->CMCDSetLiveStatus(true);

	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: ot=v",
	                "CMCD-Request: com.comcast-fb=0,com.comcast-lb=0",
	                kBareSessionHeader));
}

TEST_F(AampCMCDCollectorTest, DisabledCollector_IgnoresDataSetters)
{
	std::string traceId{kTraceId};
	mCollector->Initialize(false, traceId);

	mCollector->SetBitrates(eMEDIATYPE_VIDEO, {5000000});
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2500000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 12);
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 3000, 2500);
	mCollector->CMCDSetSessionParams(eMEDIAFORMAT_DASH, "http://example.com/master.mpd");
	mCollector->CMCDSetLiveStatus(true);
	mCollector->CMCDSetPlaybackRate(2.0f);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), IsEmpty());
}
