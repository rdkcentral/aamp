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
 * @brief Characterization tests for AampCMCDCollector header output.
 *
 * These tests pin the exact header strings the collector emits so that the
 * serialization behaviour is verified end to end through the public API.
 * The expected values intentionally document current (pre-CTA-5004-compliance)
 * output, including the unquoted sid and unsorted key order.
 */

#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "AampCMCDCollector.h"
#include "AampMediaType.h"

using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

namespace
{
	const std::string kTraceId{"test-trace-id"};
	const std::string kSessionHeader{"CMCD-Session: sid=" + kTraceId};
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

TEST_F(AampCMCDCollectorTest, Manifest_EmitsSessionIdAndObjectTypeM)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST),
	            UnorderedElementsAre("CMCD-Object: ot=m", kSessionHeader));
}

TEST_F(AampCMCDCollectorTest, PlaylistTypes_EmitManifestKeys)
{
	InitEnabled();

	for (AampMediaType mediaType : {eMEDIATYPE_PLAYLIST_VIDEO, eMEDIATYPE_PLAYLIST_AUDIO, eMEDIATYPE_PLAYLIST_SUBTITLE})
	{
		EXPECT_THAT(GetHeaders(mediaType),
		            UnorderedElementsAre("CMCD-Object: ot=m", kSessionHeader))
		    << "media type " << mediaType;
	}
}

TEST_F(AampCMCDCollectorTest, SubtitleTypes_EmitObjectTypeS)
{
	InitEnabled();

	for (AampMediaType mediaType : {eMEDIATYPE_SUBTITLE, eMEDIATYPE_INIT_SUBTITLE})
	{
		EXPECT_THAT(GetHeaders(mediaType),
		            UnorderedElementsAre("CMCD-Object: ot=s", kSessionHeader))
		    << "media type " << mediaType;
	}
}

TEST_F(AampCMCDCollectorTest, VideoDefaults_EmitZeroedKeysAndNoStatus)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: br=0,ot=v,tb=0",
	                "CMCD-Request: bl=0,nor=,com.comcast-fb=0,com.comcast-lb=0",
	                kSessionHeader));
}

TEST_F(AampCMCDCollectorTest, VideoWithFullState_EmitsAllHeaders)
{
	InitEnabled();
	mCollector->SetBitrates(eMEDIATYPE_VIDEO, {1000000, 5000000});
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2500000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 12);
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 3000, 2500);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: br=2500,ot=v,tb=5000",
	                "CMCD-Request: bl=3000,nor=http://example.com/seg2.ts,com.comcast-dns=12,com.comcast-fb=34,com.comcast-lb=56",
	                kSessionHeader,
	                "CMCD-Status: bs"));
}

TEST_F(AampCMCDCollectorTest, DnsUnavailable_OmitsDnsKey)
{
	InitEnabled();
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2500000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 0);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=0,nor=http://example.com/seg2.ts,com.comcast-fb=34,com.comcast-lb=56"));
}

TEST_F(AampCMCDCollectorTest, NextRange_EmitsNrrInsteadOfNor)
{
	InitEnabled();
	mCollector->CMCDSetNextRangeRequest("100-200", 2000000, eMEDIATYPE_VIDEO);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: br=2000,ot=v,tb=0",
	                "CMCD-Request: bl=0,nrr=100-200,com.comcast-fb=0,com.comcast-lb=0",
	                kSessionHeader));
}

TEST_F(AampCMCDCollectorTest, DnsAvailable_TakesPrecedenceOverNextRange)
{
	InitEnabled();
	mCollector->CMCDSetNextRangeRequest("100-200", 2000000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 12);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            Contains("CMCD-Request: bl=0,nor=,com.comcast-dns=12,com.comcast-fb=34,com.comcast-lb=56"));
}

TEST_F(AampCMCDCollectorTest, EmptyNextRange_IsIgnored)
{
	InitEnabled();
	mCollector->CMCDSetNextRangeRequest("", 2000000, eMEDIATYPE_VIDEO);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Object: br=0,ot=v,tb=0"));
}

TEST_F(AampCMCDCollectorTest, InitVideo_EmitsObjectTypeI)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_VIDEO), Contains("CMCD-Object: br=0,ot=i,tb=0"));
}

TEST_F(AampCMCDCollectorTest, Iframe_EmitsObjectTypeV)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_IFRAME), Contains("CMCD-Object: br=0,ot=v,tb=0"));
}

TEST_F(AampCMCDCollectorTest, Audio_EmitsObjectTypeA)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_AUDIO), Contains("CMCD-Object: br=0,ot=a,tb=0"));
}

TEST_F(AampCMCDCollectorTest, InitAudio_EmitsObjectTypeI)
{
	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_AUDIO), Contains("CMCD-Object: br=0,ot=i,tb=0"));
}

TEST_F(AampCMCDCollectorTest, MuxedStream_LatchesObjectTypeAv)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 0, 0, true);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Object: br=0,ot=av,tb=0"));

	// The muxed label is a one-way latch: reporting IsMuxed=false later does not restore ot=v.
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, false, 0, 0, false);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), Contains("CMCD-Object: br=0,ot=av,tb=0"));
}

TEST_F(AampCMCDCollectorTest, AudioTrackData_UpdatesBufferKeysOnly)
{
	InitEnabled();
	mCollector->SetTrackData(eMEDIATYPE_AUDIO, true, 1500, 999);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_AUDIO),
	            UnorderedElementsAre(
	                "CMCD-Object: br=0,ot=a,tb=0",
	                "CMCD-Request: bl=1500,nor=,com.comcast-fb=0,com.comcast-lb=0",
	                kSessionHeader,
	                "CMCD-Status: bs"));
}

TEST_F(AampCMCDCollectorTest, SetBitrates_OnlyAppliesToVideoAndAudio)
{
	InitEnabled();
	mCollector->SetBitrates(eMEDIATYPE_INIT_VIDEO, {5000000});

	EXPECT_THAT(GetHeaders(eMEDIATYPE_INIT_VIDEO), Contains("CMCD-Object: br=0,ot=i,tb=0"));
}

TEST_F(AampCMCDCollectorTest, UnknownTraceId_GeneratesUuidSessionId)
{
	std::string traceId{"unknown"};
	mCollector->Initialize(true, traceId);

	EXPECT_NE(traceId, "unknown");
	EXPECT_EQ(traceId.length(), 36u);
	EXPECT_THAT(GetHeaders(eMEDIATYPE_MANIFEST), Contains("CMCD-Session: sid=" + traceId));
}

TEST_F(AampCMCDCollectorTest, Reinitialize_ResetsCollectedState)
{
	InitEnabled();
	mCollector->SetBitrates(eMEDIATYPE_VIDEO, {5000000});
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 3000, 2500, true);

	InitEnabled();

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO),
	            UnorderedElementsAre(
	                "CMCD-Object: br=0,ot=v,tb=0",
	                "CMCD-Request: bl=0,nor=,com.comcast-fb=0,com.comcast-lb=0",
	                kSessionHeader));
}

TEST_F(AampCMCDCollectorTest, DisabledCollector_IgnoresDataSetters)
{
	std::string traceId{kTraceId};
	mCollector->Initialize(false, traceId);

	mCollector->SetBitrates(eMEDIATYPE_VIDEO, {5000000});
	mCollector->CMCDSetNextObjectRequest("http://example.com/seg2.ts", 2500000, eMEDIATYPE_VIDEO);
	mCollector->CMCDSetNetworkMetrics(eMEDIATYPE_VIDEO, 34, 56, 12);
	mCollector->SetTrackData(eMEDIATYPE_VIDEO, true, 3000, 2500);

	EXPECT_THAT(GetHeaders(eMEDIATYPE_VIDEO), IsEmpty());
}
