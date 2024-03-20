/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include "aampgstplayer.h"
#include "MockGStreamer.h"
#include "MockGLib.h"
#include "MockAampConfig.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Eq;
using ::testing::_;
using ::testing::Address;
using ::testing::DoAll;
using ::testing::SetArgPointee;

AampLogManager *mLogObj{nullptr};
AampConfig *gpGlobalConfig{nullptr};

class AAMPGstPlayerTests : public ::testing::Test
{

protected:
	AAMPGstPlayer *mAAMPGstPlayer;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;

	void SetUp() override
	{
		mLogObj = new AampLogManager();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockGLib = new NiceMock<MockGLib>();
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP{};
		mPrivateInstanceAAMP->mLogObj = mLogObj;
	}

	void TearDown() override
	{
		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockGLib;
		g_mockGLib = nullptr;

		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete mLogObj;
		mLogObj = nullptr;

	}

public:
	static gboolean ProgressCallbackOnTimeout(gpointer user_data)
	{
		return FALSE;
	}
};

TEST_F(AAMPGstPlayerTests, Constructor)
{
	// Setup
	std::string debug_level{"test_level"};
	gboolean reset{TRUE};

	// Expectations
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
				.WillOnce(Return(debug_level));
	EXPECT_CALL(*g_mockGStreamer,
				gst_debug_set_threshold_from_string(StrEq(debug_level.c_str()), reset));

	// Code under test
	AAMPGstPlayer player{mPrivateInstanceAAMP,nullptr};
}

/* Table with different parameter sets to be passed into mAAMPGstPlayer->Configure(...) */
typedef struct
{
	StreamOutputFormat auxFormat;
	bool bESChangeStatus;
	bool forwardAudioToAux;
	bool setReadyAfterPipelineCreation;

} Config_Params;

Config_Params tbl[] = {
	{FORMAT_INVALID, false, false, false},
	{FORMAT_AUDIO_ES_AC3, true, true, true}};

// Parameter test class, for running same tests with different rates

class AAMPGstPlayerTestsP : public AAMPGstPlayerTests,
							public testing::WithParamInterface<int>
{
};

TEST_P(AAMPGstPlayerTestsP, Configure)
{
	// Initialise name field otherwise get a seg fault
	// Allocating memory but not using gst methods, might cause problems when de-allocated
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"hello"}};
	GstElement gst_element_bin1 = {.object = {.name = (gchar *)"bin1"}};
	GstElement gst_element_bin2 = {.object = {.name = (gchar *)"bin2"}};
	GstElement gst_element_bin3 = {.object = {.name = (gchar *)"bin3"}};
	GstElement gst_element_audsrvsink = {.object = {.name = (gchar *)"audosrv"}};
	GstBus bus = {};
	GstPipeline *pipeline = GST_PIPELINE(&gst_element_pipeline);
	GstQuery query = {};

	int idx = GetParam();

	EXPECT_TRUE(idx < (sizeof(tbl) / sizeof(tbl[0])));

	Config_Params *setup = &tbl[idx];

	mAAMPGstPlayer = new AAMPGstPlayer{mPrivateInstanceAAMP, nullptr};
	// Expectations
	// CreatePipeline()
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("AAMPGstPlayerPipeline")))
		.WillOnce(Return(&gst_element_pipeline));

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(pipeline))
		.WillOnce(Return(&bus));

	EXPECT_CALL(*g_mockGStreamer, gst_bus_add_watch(&bus, _, mAAMPGstPlayer))
		.WillOnce(Return(0));

	EXPECT_CALL(*g_mockGStreamer, gst_bus_set_sync_handler(&bus, _, mAAMPGstPlayer, NULL))
		.Times(1);

	EXPECT_CALL(*g_mockGStreamer, gst_query_new_position(GST_FORMAT_TIME))
		.WillOnce(Return(&query));
	// End CreatePipeline()

	// setReadyAfterPipelineCreation == true
	//  calling SetStateWithWarnings()

	if (setup->setReadyAfterPipelineCreation)
	{
		EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
			.WillOnce(DoAll(
				SetArgPointee<1>(GST_STATE_VOID_PENDING),
				SetArgPointee<2>(GST_STATE_NULL),
				Return(GST_STATE_CHANGE_SUCCESS)))
			.WillOnce(DoAll(
				SetArgPointee<1>(GST_STATE_READY),
				SetArgPointee<2>(GST_STATE_READY),
				Return(GST_STATE_CHANGE_SUCCESS)));

		EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_READY))
			.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));
	}
	else
	{

		EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
			.WillOnce(DoAll(
				SetArgPointee<1>(GST_STATE_VOID_PENDING),
				SetArgPointee<2>(GST_STATE_NULL),
				Return(GST_STATE_CHANGE_SUCCESS)));
	}

	//[AAMP-PLAYER][-1][INFO][AAMPGstPlayer_SetupStream][2437]AAMPGstPlayer_SetupStream - using playbin

	if (setup->forwardAudioToAux)
	{
		EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("playbin"), NULL))
			.WillOnce(Return(&gst_element_bin1))
			.WillOnce(Return(&gst_element_bin2))
			.WillOnce(Return(&gst_element_bin3));
		EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("audsrvsink"), NULL))
			.WillOnce(Return(&gst_element_audsrvsink));
	}
	else
	{
		EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("playbin"), NULL))
			.WillOnce(Return(&gst_element_bin1))
			.WillOnce(Return(&gst_element_bin2));
	}
	// Associate sinks with pipeline
	EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), &gst_element_bin1))
		.WillOnce(Return(TRUE));

	EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), &gst_element_bin2))
		.WillOnce(Return(TRUE));

	if (setup->forwardAudioToAux)
	{
		EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), &gst_element_bin3))
			.WillOnce(Return(TRUE));
		// Calls after [AAMPGstPlayer_SetupStream][2511]playbin flags1: 0x63
	}
	// Calls after [AAMPGstPlayer_SetupStream][2511]playbin flags1: 0x63

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PLAYING))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));

	// Code under test
	mAAMPGstPlayer->Configure(FORMAT_VIDEO_ES_H264,
							  FORMAT_AUDIO_ES_AAC,
							  setup->auxFormat,
							  FORMAT_SUBTITLE_WEBVTT,
							  setup->bESChangeStatus,
							  setup->forwardAudioToAux,
							  setup->setReadyAfterPipelineCreation);

	// AAMPGstPlayer::DestroyPipeline()
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&gst_element_pipeline))
		.Times(1);
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&bus))
		.Times(1);
	EXPECT_CALL(*g_mockGStreamer, gst_mini_object_unref(GST_MINI_OBJECT_CAST(&query)))
		.Times(1); /* AKA gst_query_unref()*/
	delete mAAMPGstPlayer;
	mAAMPGstPlayer = nullptr;
}

TEST_F(AAMPGstPlayerTests, TimerAdd)
{
	// Setup
	gpointer user_data = nullptr;
	gboolean reset{TRUE};
	int repeatTimeout = 100;
	guint taskId = 0;
	GstElement dummyelement; 

	std::string debug_level{"test_level"};

	// Expectations
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel)).WillOnce(Return(debug_level));

	EXPECT_CALL(*g_mockGStreamer,gst_debug_set_threshold_from_string(StrEq(debug_level.c_str()), reset));

	mAAMPGstPlayer = new AAMPGstPlayer{mPrivateInstanceAAMP, nullptr};

	// Code under test - Callback Pointer = Null, user_data = Null
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .Times(0);
	mAAMPGstPlayer->TimerAdd(nullptr, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(0,taskId);

	// Code under test - user_data = Null
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .Times(0);
	mAAMPGstPlayer->TimerAdd(ProgressCallbackOnTimeout, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(0,taskId);

	user_data = &dummyelement;
	taskId = 1;

	// Code under test - taskId = 1 timer already added
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .Times(0);
	mAAMPGstPlayer->TimerAdd(ProgressCallbackOnTimeout, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(1,taskId);

	taskId = 0;

	// Code under test - Success Path
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .WillOnce(Return(1));
	mAAMPGstPlayer->TimerAdd(ProgressCallbackOnTimeout, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(1,taskId);

	//Tidy Up
	delete mAAMPGstPlayer;
}

TEST_F(AAMPGstPlayerTests, TimerRemove)
{
	// Setup
	std::string debug_level{"test_level"};
	gboolean reset{TRUE};
	guint taskId = 0;

	// Expectations
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstDebugLevel))
				.WillOnce(Return(debug_level));
	EXPECT_CALL(*g_mockGStreamer, gst_debug_set_threshold_from_string(StrEq(debug_level.c_str()), reset));

	mAAMPGstPlayer = new AAMPGstPlayer{mPrivateInstanceAAMP, nullptr};
	EXPECT_CALL(*g_mockGLib, g_source_remove(_)) .Times(0);

	// Code under test - taskId = 0 timer not added to be removed
	mAAMPGstPlayer->TimerRemove(taskId, "TimerRemove");
	EXPECT_EQ(0,taskId);

	taskId = 1;

	// Code under test - Success Path
	EXPECT_CALL(*g_mockGLib, g_source_remove(_)) .WillOnce(Return(TRUE));
	mAAMPGstPlayer->TimerRemove(taskId, "TimerRemove");
	EXPECT_EQ(0,taskId);

	//Tidy Up
	delete mAAMPGstPlayer;
}

INSTANTIATE_TEST_SUITE_P(AAMPGstPlayer,AAMPGstPlayerTestsP, testing::Values(0,1));
