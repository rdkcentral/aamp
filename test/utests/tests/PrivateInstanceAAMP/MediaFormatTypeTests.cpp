/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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
#include <gmock/gmock.h>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::An;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::Return;

class MediaFormatTypeTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mPrivateInstanceAAMP{};

    void SetUp() override
    {
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

        mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampGstPlayer = new MockAAMPGstPlayer(mLogObj, mPrivateInstanceAAMP);
        g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mLogObj, mPrivateInstanceAAMP);

        mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer;
        mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;
    }

    void TearDown() override
    {
        delete mPrivateInstanceAAMP;
        mPrivateInstanceAAMP = nullptr;

        delete g_mockStreamAbstractionAAMP;
        g_mockStreamAbstractionAAMP = nullptr;

        delete g_mockAampGstPlayer;
        g_mockAampGstPlayer = nullptr;

        delete gpGlobalConfig;
        gpGlobalConfig = nullptr;
    }

public:

};

//Most test urls taken from aampcli.csv aattached to rdk-30657
TEST_F(MediaFormatTypeTests, GetType)
{
const char* dashUrl[] =
{
"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd",
"http://172.27.223.96:8880/TRUTV_MIMIC.mpd",
"https://pqi-ppe-cdn-deluxe.enwd.co.sa.charterlab.com/DASH_DRM/SMTH0211604149942001/index.ism/manifest.mpd?adId=fa13bd6e-b4a4-404f-a0cf-0ed6b83ab953",
//Test recordedUrl code path. Replaced http://localhost with http://127.0.0.1 to cause this.
"http://127.0.0.1:9080/tsb?clientId=testharness&recordedUrl=http%3A%2F%2Fodol-atsec-pan-04.linear-nat-pil.xcr.comcast.net%2FCNNHD_HD_NAT_16141_0_5646493630829879163.mpd%3Ftrred%3Dfalse&analyticsUrl=https://collector.hdw.r53.deap.tv/raw.mirrored.cpe.fog.event&money=%7B%22TRACE_ID%22%3A%20%228d09139d-9924-4b20-8610-bfa7b0ab63f0%22%2C%22PARENT_ID%22%3A%20-4007834485039980500%2C%22SPAN_ID%22%3A%20-4007834485039980500%7D%0A&ses=%7B%22PSI%22%3A1438824946275%2C%22PBI%22%3A%221456515706520%22%7D&asset=%7B%22CLASS%22%3A%22Linear%22%2C%22ID_TYPE%22%3A%22StreamId%22%2C%22IDS%22%3A%7B%22STRID%22%3A%228621049422542808163%22%7D%7D&appName=STB-XI3&appVer=2.6p1&acId=2760425345202996426&devName=iptsb&devID=12:BF:60:1E:21:9C&phyID=7739922214635090063",
};

const char* progUrl[] =
{
"http://127.0.0.1:8080/WTXFDT29.1-first.mp4",
"http://127.0.0.1:8080/WTXFDT29.1-first.mkv",
"http://127.0.0.1:8080/WTXFDT29.1-first.ts",
"srt:http://127.0.0.1:8080/WTXFDT29.1-first.?xxxx"
};

const char* unknownUrl[] =
{
"http://127.0.0.1:8080/1080pMorta001.h264",
"http://localhost:9080/tsb?clientId=testharness&recordedUrl=http%3A%2F%2Fodol-atsec-pan-04.linear-nat-pil.xcr.comcast.net%2FCNNHD_HD_NAT_16141_0_5646493630829879163.mpd%3Ftrred%3Dfalse&analyticsUrl=https://collector.hdw.r53.deap.tv/raw.mirrored.cpe.fog.event&money=%7B%22TRACE_ID%22%3A%20%228d09139d-9924-4b20-8610-bfa7b0ab63f0%22%2C%22PARENT_ID%22%3A%20-4007834485039980500%2C%22SPAN_ID%22%3A%20-4007834485039980500%7D%0A&ses=%7B%22PSI%22%3A1438824946275%2C%22PBI%22%3A%221456515706520%22%7D&asset=%7B%22CLASS%22%3A%22Linear%22%2C%22ID_TYPE%22%3A%22StreamId%22%2C%22IDS%22%3A%7B%22STRID%22%3A%228621049422542808163%22%7D%7D&appName=STB-XI3&appVer=2.6p1&acId=2760425345202996426&devName=iptsb&devID=12:BF:60:1E:21:9C&phyID=7739922214635090063",
};

const char* hlsUrl[] =
{
"http://172.27.223.96:8880/TRUTV_MIMIC.m3u8",
"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main_mp4.m3u8",
"https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.m3u8",
"https://dai2.xumo.com/amagi_hls_data_xumo1212A-testfood52/CDN/playlist.m3u8?p=test",
"https://vex-hills-stg.mm-col-jitp2-s.xcr.comcast.net/test_ipvod6/VIPP2019092300001020/movie/1571803639146/manifest.m3u8?StreamType=VOD_T6&ProviderId=comcastqa.com&AssetId=VIPT2019092300001020&sid=77777ffdgmmacadfdda0ccdf-7hgdh9876677756s-1892232&PartnerId=comcast&DeviceId=10.169.123.12"
};

    	MediaFormat mediaType = mPrivateInstanceAAMP->GetMediaFormatType("unknown");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_UNKNOWN);
    	mediaType = mPrivateInstanceAAMP->GetMediaFormatType("hdmiin:2");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_HDMI);
    	mediaType = mPrivateInstanceAAMP->GetMediaFormatType("cvbsin:2");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_COMPOSITE);
    	mediaType = mPrivateInstanceAAMP->GetMediaFormatType("live:123");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_OTA);
    	mediaType = mPrivateInstanceAAMP->GetMediaFormatType("tune:2");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_OTA);
    	mediaType = mPrivateInstanceAAMP->GetMediaFormatType("mr:2");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_OTA);
    	mediaType = mPrivateInstanceAAMP->GetMediaFormatType("ocap://");
	EXPECT_EQ(mediaType, eMEDIAFORMAT_RMF);

	for(int i=0; i < ARRAY_SIZE(hlsUrl); i++)
	{
		mediaType = mPrivateInstanceAAMP->GetMediaFormatType(hlsUrl[i]);
		EXPECT_EQ(mediaType, eMEDIAFORMAT_HLS);
	}

	for(int i=0; i < ARRAY_SIZE(dashUrl); i++)
	{
		mediaType = mPrivateInstanceAAMP->GetMediaFormatType(dashUrl[i]);
		EXPECT_EQ(mediaType, eMEDIAFORMAT_DASH);
	}
	
	for(int i=0; i < ARRAY_SIZE(progUrl); i++)
	{
		mediaType = mPrivateInstanceAAMP->GetMediaFormatType(progUrl[i]);
		EXPECT_EQ(mediaType, eMEDIAFORMAT_PROGRESSIVE);
	}

	for(int i=0; i < ARRAY_SIZE(unknownUrl); i++)
	{
		mediaType = mPrivateInstanceAAMP->GetMediaFormatType(unknownUrl[i]);
		EXPECT_EQ(mediaType, eMEDIAFORMAT_UNKNOWN);
	}
}

// RecordedUrl tests
TEST_F(MediaFormatTypeTests, RecordedUrl)
{
	MediaFormat mediaFormat;
	static const struct
	{
		const char *url;
		MediaFormat expectedMediaFormat;
	} test_cases[] = 
	{
		// DELIA-62753
		{ "http://127.0.0.1:9080/adrec?clientId=FOG_AAMP&recordedUrl=https%3A%2F%2Fads.com%2Fad.mpd",
			eMEDIAFORMAT_DASH
		},
		{ "http://127.0.0.1:9080/adrec?clientId=FOG_AAMP&recordedUrl=https%3A%2F%2Fads.com%2Fad.m3u8",
			eMEDIAFORMAT_HLS
		},
		{ "http://127.0.0.1:9080/adrec?clientId=FOG_AAMP&recordedUrl=https%3A%2F%2Fads.com%2Fad.mpd&analyticsUrl=https://analytics.com/log",
			eMEDIAFORMAT_DASH
		},
		{ "http://127.0.0.1:9080/adrec?clientId=FOG_AAMP&recordedUrl=https%3A%2F%2Fads.com%2Fad.m3u8&analyticsUrl=https://analytics.com/log",
			eMEDIAFORMAT_HLS
		},
	};

	for (int i = 0; i < ARRAY_SIZE(test_cases); i++)
	{
		mediaFormat = mPrivateInstanceAAMP->GetMediaFormatType(test_cases[i].url);
		EXPECT_EQ(mediaFormat, test_cases[i].expectedMediaFormat);
	}
}

