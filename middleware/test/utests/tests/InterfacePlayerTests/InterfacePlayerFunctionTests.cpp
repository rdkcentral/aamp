/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
#include "InterfacePlayerRDK.h"
#include "InterfacePlayerPriv.h"
#include "PlayerLogManager.h"
#include "MockGStreamer.h"
#include "MockGLib.h"
#include "MockGstHandlerControl.h"
#include "MockPlayerScheduler.h"
#include <gst/gstplugin.h>
#include <gst/gstpluginfeature.h>

using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Eq;
using ::testing::_;
using ::testing::Address;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::NotNull;
using ::testing::SaveArgPointee;
using ::testing::SaveArg;
using ::testing::Pointer;
using ::testing::Matcher;
using ::testing::AnyNumber;

#define GST_NORMAL_PLAY_RATE		1

class InterfacePlayerTests : public ::testing::Test
{

protected:
	bool isPipelineSetup = false;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"Pipeline"}};
	GstBus bus = {};
	GstQuery query = {};
	InterfacePlayerRDK *mInterfaceGstPlayer;
	InterfacePlayerPriv *mInterfacePrivatePlayer;
	
	GstPlayerPriv* mPlayerContext;
	Configs* mPlayerConfigParams;
	int cbResponse = 0;

	void SetUp() override
	{
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockGLib = new NiceMock<MockGLib>();
		g_mockPlayerScheduler = new NiceMock<MockPlayerScheduler>();

		ConstructAMPGstPlayer();
		PlayerLogManager::lockLogLevel(false);
		PlayerLogManager::disableLogRedirection = true; //required for mwlog output in utest
		PlayerLogManager::setLogLevel(mLOGLEVEL_TRACE);
	}

	void TearDown() override
	{
		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;

		DestroyAMPGstPlayer();

		delete g_mockGLib;
		g_mockGLib = nullptr;

		delete g_mockPlayerScheduler;
		g_mockPlayerScheduler = nullptr;
	}

public:
	void ConstructAMPGstPlayer()
	{
		mInterfaceGstPlayer = new InterfacePlayerRDK();
		mInterfacePrivatePlayer  = mInterfaceGstPlayer->GetPrivatePlayer();

		mPlayerContext = mInterfacePrivatePlayer->gstPrivateContext;
		
		mPlayerConfigParams = mInterfaceGstPlayer->m_gstConfigParam;
		//init callback to avoid bad_function_call error
		mInterfaceGstPlayer->TearDownCallback([this](bool status, int mediaType) {
		if (status) {
			cbResponse = 5;
		}});
		mInterfaceGstPlayer->StopCallback([this](bool status)
			{
				printf("StopCallback status: %d\n", status);
			});
	}

	void DestroyAMPGstPlayer()
	{
		delete mInterfaceGstPlayer;
		mPlayerContext = nullptr;
		mInterfaceGstPlayer = nullptr;
	}

};

TEST_F(InterfacePlayerTests, ConfigurePipeline_DefaultSettings)
{
	g_mockGStreamer = nullptr;
	EXPECT_EQ(mPlayerContext->firstTuneWithWesterosSinkOff, false);
	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, false, false, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");
	EXPECT_EQ(mPlayerContext->forwardAudioBuffers, false);

	//with realtek flag
	//mPlayerConfigParams->platformType = eGST_PLATFORM_REALTEK;
	mInterfacePrivatePlayer->socInterface->SetWesterosSinkState(true);
	
	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, false, false, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");
	EXPECT_EQ(mPlayerContext->firstTuneWithWesterosSinkOff, true);
}

TEST_F(InterfacePlayerTests, ConfigurePipeline_WithAudioForwardToAux)
{
	g_mockGStreamer = nullptr;
	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, false, true, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");
	EXPECT_EQ(mPlayerContext->forwardAudioBuffers, true);
}

TEST_F(InterfacePlayerTests, ConfigurePipeline_WithWesterosAndRealtoSink)
{
	g_mockGStreamer = nullptr;
	mPlayerConfigParams->useWesterosSink = true;
	EXPECT_EQ(mPlayerContext->using_westerossink, false);

	mPlayerConfigParams->useRialtoSink = true;
	EXPECT_EQ(mPlayerContext->usingRialtoSink, false);

	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, false, false, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");
	EXPECT_EQ(mPlayerContext->using_westerossink, true);
	EXPECT_EQ(mPlayerContext->usingRialtoSink, true);

}

TEST_F(InterfacePlayerTests, ConfigurePipeline_WithSubtitlesEnabled)
{
	g_mockGStreamer = nullptr;
	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, false, false, false, true, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");

	EXPECT_EQ(mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].format, GST_FORMAT_INVALID);
}



TEST_F(InterfacePlayerTests, ConfigurePipeline_WithBufferingEnabled)
{
	g_mockGStreamer = nullptr;
	mPlayerContext->buffering_enabled = true;
	mPlayerContext->rate = GST_NORMAL_PLAY_RATE;

	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_MPEGTS, GST_FORMAT_INVALID, GST_FORMAT_INVALID, GST_FORMAT_INVALID, false, false, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");

	EXPECT_EQ(mPlayerContext->buffering_in_progress, true);
	EXPECT_EQ(mPlayerContext->buffering_target_state, GST_STATE_PLAYING);
}


TEST_F(InterfacePlayerTests, ConfigurePipeline_StreamConfiguration)
{
	g_mockGStreamer = nullptr;
	mPlayerContext->NumberOfTracks = 0;
	mPlayerContext->rate = 1.0;

	EXPECT_EQ(mPlayerContext->NumberOfTracks, 0);

	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_ISO_BMFF, GST_FORMAT_AUDIO_ES_AC3, GST_FORMAT_AUDIO_ES_AC3, GST_FORMAT_SUBTITLE_MP4, false, false, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");

	EXPECT_EQ(mPlayerContext->NumberOfTracks, 3);
	EXPECT_EQ(cbResponse, 5); //callback was called
}

TEST_F(InterfacePlayerTests, ConfigurePipeline_ESChange)
{
	g_mockGStreamer = nullptr;
	mPlayerContext->NumberOfTracks = 0;
	mPlayerContext->rate = 1.0;
	mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].format = GST_FORMAT_AUDIO_ES_AC3;

	EXPECT_EQ(mPlayerContext->NumberOfTracks, 0);

	mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_ISO_BMFF, GST_FORMAT_AUDIO_ES_AC3, GST_FORMAT_AUDIO_ES_AC3, GST_FORMAT_SUBTITLE_MP4, true, false, false, false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");

	EXPECT_EQ(mPlayerContext->NumberOfTracks, 2);
	EXPECT_EQ(cbResponse, 5);
}