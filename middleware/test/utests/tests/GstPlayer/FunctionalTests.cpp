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
#include "InterfacePlayerRDK.h"
#include "InterfacePlayerPriv.h"
#include "GstUtils.h"
#include "MockGStreamer.h"
#include "MockGLib.h"
#include "MockGstHandlerControl.h"
#include "MockPlayerUtils.h"

#define GST_NORMAL_PLAY_RATE		1

using ::testing::NiceMock;
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

class GstPlayerTests : public ::testing::Test
{

protected:
	bool isPipelineSetup = false;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"Pipeline"}};
	GstBus bus = {};
	GstQuery query = {};
	InterfacePlayerRDK *mInterfaceGstPlayer;

	void SetUp() override
	{
		g_mockPlayerUtils = new MockPlayerUtils();
		g_mockGStreamer = new NiceMock<MockGStreamer>();
		g_mockGLib = new MockGLib();
		g_mockGstHandlerControl= new MockGstHandlerControl();
	}

	void TearDown() override
	{
		delete g_mockGstHandlerControl;
		 g_mockGstHandlerControl= nullptr;

		delete g_mockGLib;
		g_mockGLib = nullptr;

		delete g_mockGStreamer;
		g_mockGStreamer = nullptr;

		delete g_mockPlayerUtils;
		g_mockPlayerUtils = nullptr;

	}

public:
	typedef struct
	{
		GstStreamOutputFormat auxFormat;
		bool bESChangeStatus;
		bool forwardAudioToAux;
		bool setReadyAfterPipelineCreation;
		bool enableRectangleProperty;
		bool usingWesteros; 
		bool usingRialto;
	} Config_Params;

	static gboolean ProgressCallbackOnTimeout(gpointer user_data)
	{
		return FALSE;
	}

	void ConstructAMPGstPlayer()
	{
		std::string debug_level{"test_level"};
		gboolean reset{TRUE};

		mInterfaceGstPlayer = new InterfacePlayerRDK();

		//init callback to avoid bad_function_call error
		mInterfaceGstPlayer->TearDownCallback([this](bool status, int mediaType) {
			if (status) {
			}});
		mInterfaceGstPlayer->StopCallback([this](bool status)
		{
			printf("StopCallback status: %d\n", status);
		});

		mInterfaceGstPlayer->RegisterBusEvent([this](const BusEventData& event) {
			printf("busMessageCallback called\n");
		});
	}

	void DestroyAMPGstPlayer()
	{
		if (isPipelineSetup)
		{
			EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&gst_element_pipeline))
				.Times(1);
			EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&bus))
				.Times(1);
			EXPECT_CALL(*g_mockGStreamer, gst_mini_object_unref(GST_MINI_OBJECT_CAST(&query)))
				.Times(1); /* AKA gst_query_unref()*/
		}

		delete mInterfaceGstPlayer;
		mInterfaceGstPlayer = nullptr;
	}

	void SetupPipeline(Config_Params *setup)
	{
		GstElement gst_element_bin = {.object = {.name = (gchar *)"bin"}};
		GstElement gst_element_audsrvsink = {.object = {.name = (gchar *)"audosrv"}};
		GstElement westeros_video_sink = {.object = {.name = (gchar *)"westerossink0"}};
		GstElement rialto_video_sink = {.object = {.name = (gchar *)"rialtomsevideosink0"}};
		GstElement brcm_video_sink = {.object = {.name = (gchar *)"brcmvideosink0"}};
		GstElement audio_sink = {.object = {.name = (gchar *)"amlhalasink0"}};
		GstElement *p_video_sink = nullptr;
		GstElement *p_audio_sink = &audio_sink;
		GstPipeline *pipeline = GST_PIPELINE(&gst_element_pipeline);

		GstBusFunc bus_message_func = nullptr;
		GstBusSyncHandler bus_sync_func = nullptr;

		isPipelineSetup = true;
		if (setup->usingRialto)
		{
			p_video_sink = &rialto_video_sink;
		}
		else if (setup->usingWesteros)
		{
			p_video_sink = &westeros_video_sink;
		}
		else
		{
			p_video_sink = &brcm_video_sink;
		}

		mInterfaceGstPlayer->m_gstConfigParam->useWesterosSink = setup->usingWesteros;
		mInterfaceGstPlayer->m_gstConfigParam->useRialtoSink = setup->usingRialto;
		mInterfaceGstPlayer->m_gstConfigParam->enableRectPropertyCfg = setup->enableRectangleProperty;

		// CreatePipeline()
		
		EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("testPipeline")))
			.WillOnce(Return(&gst_element_pipeline));

		EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(pipeline))
			.WillOnce(Return(&bus));

		// Save the bus_message function for later use
		EXPECT_CALL(*g_mockGStreamer, gst_bus_add_watch(&bus, NotNull(), mInterfaceGstPlayer))
			.WillOnce(DoAll(
				SaveArgPointee<1>(&bus_message_func),
				Return(0)));

		// Save the bus_sync_handler function for later use
		EXPECT_CALL(*g_mockGStreamer, gst_bus_set_sync_handler(&bus, NotNull(), mInterfaceGstPlayer, NULL))
			.WillOnce(SaveArgPointee<1>(&bus_sync_func));

		EXPECT_CALL(*g_mockGStreamer, gst_query_new_position(GST_FORMAT_TIME))
			.WillOnce(Return(&query));
		// End CreatePipeline()
		
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
		EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(_, NULL))
			.WillRepeatedly(Return(&gst_element_bin));
		if (setup->forwardAudioToAux)
		{
			EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("audsrvsink"), NULL))
				.WillOnce(Return(&gst_element_audsrvsink));
		}

		if (setup->usingRialto)
		{
			EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("rialtomsevideosink"), NULL))
				.WillRepeatedly(Return(p_video_sink));
			// IsGstreamerSubsEnabled fake impl returns false and hence EXPECT_CALL for subtitle pipeline is not required
		}
		else if (setup->usingWesteros)
		{
			EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("westerossink"), NULL))
				.WillRepeatedly(Return(p_video_sink));
		}
		else
		{
			EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("brcmvideosink"), NULL))
				.WillRepeatedly(Return(p_video_sink));
		}

		EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), NotNull()))
			.WillRepeatedly(Return(TRUE));

		EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PLAYING))
			.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));

		mInterfaceGstPlayer->ConfigurePipeline(GST_FORMAT_VIDEO_ES_H264,
										GST_FORMAT_AUDIO_ES_AAC,
										setup->auxFormat,
										GST_FORMAT_SUBTITLE_WEBVTT,
										setup->bESChangeStatus,
										setup->forwardAudioToAux,
										  setup->setReadyAfterPipelineCreation,
										  false, 0, GST_NORMAL_PLAY_RATE, "testPipeline", 0, false, "testManifest");

		ASSERT_TRUE(bus_sync_func != nullptr);
		ASSERT_TRUE(bus_message_func != nullptr);

		GstMessage sync_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(p_video_sink) };

		EXPECT_CALL(*g_mockGstHandlerControl, isEnabled())
			.WillRepeatedly(Return(true));

		EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&sync_message),NotNull(),NotNull(),_))
			.WillOnce(DoAll(
				SetArgPointee<1>(GST_STATE_READY),
				SetArgPointee<2>(GST_STATE_PAUSED)));

		if (setup->enableRectangleProperty)
		{
			EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("rectangle"), Matcher<char *>(_))).Times(1);
		}
		else
		{
			EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("rectangle"), Matcher<char *>(_))).Times(0);
		}
		if (setup->usingRialto)
		{
			EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("zoom-mode"), Matcher<int>(_))).Times(0);
		}
		else
		{
			EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("zoom-mode"), Matcher<int>(GST_VIDEO_ZOOM_NONE))).Times(1);
		}
		EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("show-video-window"), Matcher<int>(true))).Times(1);

		EXPECT_CALL(*g_mockGStreamer, gst_object_replace(NotNull(),NotNull()))
			.WillOnce(DoAll(
				SetArgPointee<0>(GST_OBJECT(p_video_sink)),
				Return(1)));

		// Call the bus_sync_handler function with video sink READY -> PAUSED
		bus_sync_func(&bus, &sync_message, mInterfaceGstPlayer);

		sync_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(p_audio_sink) };

		EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&sync_message),NotNull(),NotNull(),_))
			.WillOnce(DoAll(
				SetArgPointee<1>(GST_STATE_READY),
				SetArgPointee<2>(GST_STATE_PAUSED)));

		EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(1.0))).Times(1);

		EXPECT_CALL(*g_mockGStreamer, gst_object_replace(NotNull(),NotNull()))
			.WillOnce(DoAll(
				SetArgPointee<0>(GST_OBJECT(p_audio_sink)),
				Return(1)));

		// Call the bus_sync_handler function with audio sink READY -> PAUSED
		bus_sync_func(&bus, &sync_message, mInterfaceGstPlayer);

		GstMessage bus_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(&gst_element_pipeline) };

		EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&bus_message),NotNull(),NotNull(),NotNull()))
			.WillOnce(DoAll(
				SetArgPointee<1>(GST_STATE_READY),
				SetArgPointee<2>(GST_STATE_PAUSED),
				SetArgPointee<3>(GST_STATE_NULL)));

		// Call the bus_message function
		bus_message_func(&bus, &bus_message, mInterfaceGstPlayer);
	}

};

TEST_F(GstPlayerTests, Constructor)
{
	ConstructAMPGstPlayer();
	DestroyAMPGstPlayer();
}


//	typedef struct
//	{
//		GstStreamOutputFormat auxFormat;
//		bool bESChangeStatus;
//		bool forwardAudioToAux;
//		bool setReadyAfterPipelineCreation;
//		bool enableRectangleProperty;
//		bool usingWesteros; 
//		bool usingRialto;
//	} Config_Params;

static GstPlayerTests::Config_Params tbl[] = {
	{GST_FORMAT_INVALID, 	  false, false, false, false, true, false },
	{GST_FORMAT_INVALID, 	  false, false, false, false, true, true  },
	{GST_FORMAT_INVALID, 	  false, false, false, true,  true, false },
	{GST_FORMAT_AUDIO_ES_AC3, true,  true,  true,  false, true, false } };

// Parameter test class, for running same tests with different settings

class GstPlayerTestsP : public GstPlayerTests,
							public testing::WithParamInterface<int>
{
};

TEST_P(GstPlayerTestsP, Configure)
{
	int idx = GetParam();
	ASSERT_TRUE(idx < (sizeof(tbl) / sizeof(tbl[0])));
	GstPlayerTests::Config_Params *setup = &tbl[idx];
	
	ConstructAMPGstPlayer();

	SetupPipeline(setup);

	DestroyAMPGstPlayer();
}


TEST_P(GstPlayerTestsP, SetAudioVolume)
{
	int idx = GetParam();
	ASSERT_TRUE(idx < (sizeof(tbl) / sizeof(tbl[0])));
	GstPlayerTests::Config_Params *setup = &tbl[idx];

	// Setup
	ConstructAMPGstPlayer();
	SetupPipeline(setup);

	// Code under test
	
	// Muted, and volume not set (note this is not the case for non-rialto AMLOGIC builds)
	int volume = 0;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("mute"), Matcher<int>(true))).Times(1);
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(_))).Times(0);
	mInterfaceGstPlayer->SetAudioVolume(volume);
	mInterfaceGstPlayer->SetVolumeOrMuteUnMute();

	// Unmuted and volume set to 100.0
	volume = 100;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("mute"), Matcher<int>(false))).Times(1);
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(volume / 100.0))).Times(1);
	mInterfaceGstPlayer->SetAudioVolume(volume);
	mInterfaceGstPlayer->SetVolumeOrMuteUnMute();

	// No change to mute, and volume set to 50.0
	volume = 50;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("mute"), Matcher<int>(_))).Times(0);
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(volume / 100.0))).Times(1);
	mInterfaceGstPlayer->SetAudioVolume(volume);
	mInterfaceGstPlayer->SetVolumeOrMuteUnMute();

	//Tidy Up
	DestroyAMPGstPlayer();
}

TEST_P(GstPlayerTestsP, SetVideoMute)
{
	// Setup
	ConstructAMPGstPlayer();
	SetupPipeline(&tbl[0]);

	bool mute = true;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("show-video-window"), Matcher<int>(!mute))).Times(1);
	mInterfaceGstPlayer->SetVideoMute(mute);

	mute = false;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("show-video-window"), Matcher<int>(!mute))).Times(1);
	mInterfaceGstPlayer->SetVideoMute(mute);

	//Tidy Up
	DestroyAMPGstPlayer();
}

INSTANTIATE_TEST_SUITE_P(GstPlayer,GstPlayerTestsP, testing::Values(0,1,2,3));

TEST_F(GstPlayerTests, TimerAdd)
{
	// Setup
	gpointer user_data = nullptr;
	int repeatTimeout = 100;
	guint taskId = 0;
	GstElement dummyelement; 
	mInterfaceGstPlayer = new InterfacePlayerRDK();
	ConstructAMPGstPlayer();

	// Expectations

	// Code under test - Callback Pointer = Null, user_data = Null
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .Times(0);
	mInterfaceGstPlayer->TimerAdd(nullptr, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(0,taskId);

	// Code under test - user_data = Null
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .Times(0);
	mInterfaceGstPlayer->TimerAdd(ProgressCallbackOnTimeout, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(0,taskId);

	user_data = &dummyelement;
	taskId = 1;

	// Code under test - taskId = 1 timer already added
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .Times(0);
	mInterfaceGstPlayer->TimerAdd(ProgressCallbackOnTimeout, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(1,taskId);

	taskId = 0;

	// Code under test - Success Path
	EXPECT_CALL(*g_mockGLib, g_timeout_add(_, _, _)) .WillOnce(Return(1));
	mInterfaceGstPlayer->TimerAdd(ProgressCallbackOnTimeout, repeatTimeout, taskId, user_data, "TimerAdd");
	EXPECT_EQ(1,taskId);

	//Tidy Up
	DestroyAMPGstPlayer();
}


TEST_F(GstPlayerTests, TimerRemove)
{
	// Setup
	guint taskId = 0;

	ConstructAMPGstPlayer();

	// Expectations

	mInterfaceGstPlayer = new InterfacePlayerRDK(); 
	EXPECT_CALL(*g_mockGLib, g_source_remove(_)) .Times(0);

	// Code under test - taskId = 0 timer not added to be removed
	mInterfaceGstPlayer->TimerRemove(taskId, "TimerRemove");
	EXPECT_EQ(0,taskId);

	taskId = 1;

	// Code under test - Success Path
	EXPECT_CALL(*g_mockGLib, g_source_remove(_)) .WillOnce(Return(TRUE));
	mInterfaceGstPlayer->TimerRemove(taskId, "TimerRemove");
	EXPECT_EQ(0,taskId);
}


TEST_F(GstPlayerTests, SetAudioVolume_NoSink)
{
	// Setup
	ConstructAMPGstPlayer();

	// Code under test
	
	// No sink, so no call to set volume or mute expected
	int volume = 0;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("mute"), Matcher<int>(_))).Times(0);
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("volume"), Matcher<double>(_))).Times(0);
	mInterfaceGstPlayer->SetAudioVolume(volume);

	volume = 100;
	mInterfaceGstPlayer->SetAudioVolume(volume);

	volume = 50;
	mInterfaceGstPlayer->SetAudioVolume(volume);

	//Tidy Up
	DestroyAMPGstPlayer();
}

TEST_F(GstPlayerTests, SetVideoMute_NoSink)
{
	// Setup
	ConstructAMPGstPlayer();

	bool mute = true;
	EXPECT_CALL(*g_mockGLib, g_object_set(NotNull(), StrEq("show-video-window"), Matcher<int>(_))).Times(0);
	mInterfaceGstPlayer->SetVideoMute(mute);

	//Tidy Up
	DestroyAMPGstPlayer();
}

extern void MonitorAV( InterfacePlayerRDK *_this );

TEST_F(GstPlayerTests, MonitorAV )
{
	const int avpos[][2] =
	{
		{  280,  243},
		{  540,  503},
		{  780,  743},// ok
		{ 1040, 1003},// ok
		{ 1280, 1243},// ok
		{ 1540, 1503},// ok
		{ 1780, 1743},// ok
		{ 2040, 2003},// ok
		{ 2280, 2263},// ok
		{ 2540, 2503},// ok
		{ 2800, 2763},// ok
		{ 3040, 3003},// ok
		{ 3300, 3263},// ok
		{ 3540, 3503},// ok
		{ 3940, 5653},// video-only
		{ 5140, 5893},// video-only
		{ 6180, 6153},// ok
		{ 6420, 6393},// ok
		{ 6680, 6653},// ok
		{ 6920, 6893},// ok
		{ 7180, 7153},// ok
		{ 7420, 7413},// ok
		{ 7680, 7653},// ok
		{ 7940, 7913},// ok
		{ 8180, 8153},// ok
		{ 8440, 8413},// ok
		{ 8680, 8653},// ok
		{ 8940, 8913},// ok
		{ 9180, 9153},// ok
		{ 9440, 9413},// ok
		// av gap
		{11560,11520},// ok
		{11820,11780},// ok
		{12060,12020},// ok
		{12320,12280},// ok
		{12560,12520},// ok
		{12820,12780},// ok
		{13080,13040},// ok
		{13320,13280},// ok
		{13580,13540},// ok
		{13820,13780},// ok
		{14080,14040},// ok
		{14320,14280},// ok
		{14580,14540},// ok
		{14820,14780},// ok
		{15080,15040},// ok
		{15320,15280},// ok
		{15380,15540},// audio only
		{15380,15780},// audio only
		{15380,16040},// audio only
		{15380,16280},// audio only
		{15380,16540},// audio only
		{15380,16780},// audio only
		{15380,17040},// audio only
		{17320,17300},// ok
		{17580,17540},// ok
		{17840,17800},// ok
		{18080,18040},// ok
		{18340,18300},// ok
		{18580,18540},// ok
		{18840,18800},// ok
		{19080,19040},// ok
		{19220,19321},// bad avsync
		{19220,19321},// stalled av
		{19220,19321},// stalled av
		{19220,19321},// stalled av
		{19220,19321},// stalled av
		{19220,19321},// stalled av
	};
	ConstructAMPGstPlayer();
	SetupPipeline(&tbl[0]);
	
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillRepeatedly(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PLAYING),
			Return(GST_STATE_CHANGE_SUCCESS)));
	for( int idx=0; idx<sizeof(avpos)/sizeof(avpos[0]); idx++ )
	{
		EXPECT_CALL( *g_mockPlayerUtils, GetCurrentTimeMS()).WillOnce(DoAll(Return(1000+idx*250)));
		
		EXPECT_CALL(*g_mockGStreamer, gst_element_query_position( _, _, _))
			.WillOnce(DoAll(
							SetArgPointee<2>(G_GINT64_CONSTANT(1000000)*avpos[idx][eGST_MEDIATYPE_VIDEO]),
							Return(TRUE)))
			.WillOnce(DoAll(
							SetArgPointee<2>(G_GINT64_CONSTANT(1000000)*avpos[idx][eGST_MEDIATYPE_AUDIO]),
							Return(TRUE)));
		MonitorAV(mInterfaceGstPlayer);
	}
	DestroyAMPGstPlayer();
}


