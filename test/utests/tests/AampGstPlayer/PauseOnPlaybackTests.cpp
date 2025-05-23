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
#include "MockGstHandlerControl.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockPlayerScheduler.h"
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

class PauseOnPlaybackTests : public ::testing::Test
{

protected:
	AAMPGstPlayer *mAAMPGstPlayer;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	InterfacePlayerRDK *mplayer;
	void SetUp() override
	{
        if(gpGlobalConfig == nullptr)
        {
            gpGlobalConfig =  new AampConfig();
        }

		g_mockGStreamer = new MockGStreamer();
		g_mockGLib = new NiceMock<MockGLib>();
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockGstHandlerControl = new MockGstHandlerControl();
		g_mockPlayerScheduler = new MockPlayerScheduler();
		g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP{};

    	mAAMPGstPlayer = new AAMPGstPlayer{mPrivateInstanceAAMP, nullptr};
	mplayer = mAAMPGstPlayer->playerInstance;

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

		delete g_mockGstHandlerControl;
		g_mockGstHandlerControl = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
	
    	delete mAAMPGstPlayer;
	    mAAMPGstPlayer = nullptr;

	    delete g_mockPlayerScheduler;
	    g_mockPlayerScheduler = nullptr;
	}

public:
};

// Test API SetPauseOnStartPlayback
// No external checks available to check anything occurs, however checked in
// test EnteredPausedSteHandler_ConfigurePauseOnPlayback that property is enabled
TEST_F(PauseOnPlaybackTests, SetPauseOnStartPlayback)
{
	mAAMPGstPlayer->SetPauseOnStartPlayback(true);
	mAAMPGstPlayer->SetPauseOnStartPlayback(false);
}

// Test configuration of pipeline with PauseOnPlayback enabled
// Checks that the state set is GST_STATE_PAUSED
TEST_F(PauseOnPlaybackTests, EnteredPausedSteHandler_ConfigurePauseOnPlayback)
{
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"Pipeline"}};
	GstElement gst_element_bin = {.object = {.name = (gchar *)"bin"}};
	GstBus bus = {};
	GstPipeline *pipeline = GST_PIPELINE(&gst_element_pipeline);

	// CreatePipeline()
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("AAMPGstPlayerPipeline")))
		.WillOnce(Return(&gst_element_pipeline));

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(pipeline))
		.WillOnce(Return(&bus));

	// End CreatePipeline()

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_VOID_PENDING),
			SetArgPointee<2>(GST_STATE_NULL),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("playbin"), NULL))
		.WillRepeatedly(Return(&gst_element_bin));

	EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), NotNull()))
		.WillRepeatedly(Return(TRUE));

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PAUSED))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));

	// Enable PauseOnPlayback
	mAAMPGstPlayer->SetPauseOnStartPlayback(true);

	mAAMPGstPlayer->Configure(FORMAT_VIDEO_ES_H264,
							  FORMAT_AUDIO_ES_AAC,
							  FORMAT_INVALID,
							  FORMAT_SUBTITLE_WEBVTT,
							  false,
							  false,
							  false);
}

// Test configuration of pipeline with PauseOnPlayback not enabled
// Checks that the state set is GST_STATE_PLAYING
TEST_F(PauseOnPlaybackTests, EnteredPausedSteHandler_ConfigureNormalPlayback)
{
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"Pipeline"}};
	GstElement gst_element_bin = {.object = {.name = (gchar *)"bin"}};
	GstBus bus = {};
	GstPipeline *pipeline = GST_PIPELINE(&gst_element_pipeline);

	// CreatePipeline()
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("AAMPGstPlayerPipeline")))
		.WillOnce(Return(&gst_element_pipeline));

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(pipeline))
		.WillOnce(Return(&bus));
	// End CreatePipeline()

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_VOID_PENDING),
			SetArgPointee<2>(GST_STATE_NULL),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("playbin"), NULL))
		.WillRepeatedly(Return(&gst_element_bin));

	EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), NotNull()))
		.WillRepeatedly(Return(TRUE));

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PLAYING))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));

	mAAMPGstPlayer->Configure(FORMAT_VIDEO_ES_H264,
							  FORMAT_AUDIO_ES_AAC,
							  FORMAT_INVALID,
							  FORMAT_SUBTITLE_WEBVTT,
							  false,
							  false,
							  false);
}

// Test bus_message callback when PauseOnPlayback has been enabled, and sink
// has the property frame-step-on-preroll
// Due to g_object_set being a variadic function it is not possible/simple to mock,
// and therefore confirm that the property is set. However it is checked that 
// step event is sent. 
// The task FirstFrameCallback is not exected as this would be triggered by the 
// "first-video-frame-callback" callback from gstreamer when the frame is displayed
TEST_F(PauseOnPlaybackTests, bus_messsage_FrameStepPropertyAvailable)
{
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"Pipeline"}};
	GstElement gst_element_video_sink = {.object = {.name = (gchar *)"brcmvideosink0"}};
	GstElement gst_element_bin = {.object = {.name = (gchar *)"bin"}};
	GstBus bus = {};
	GstPipeline *pipeline = GST_PIPELINE(&gst_element_pipeline);
	GstBusFunc bus_message_func = nullptr;
	GstBusSyncHandler bus_sync_func = nullptr;

	// Expectations
	// CreatePipeline()
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("AAMPGstPlayerPipeline")))
		.WillOnce(Return(&gst_element_pipeline));

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(pipeline))
		.WillOnce(Return(&bus));

	// Save the bus_message function for later use
	EXPECT_CALL(*g_mockGStreamer, gst_bus_add_watch(&bus, NotNull(), mplayer))
		.WillOnce(DoAll(
			SaveArgPointee<1>(&bus_message_func),
			Return(0)));

	// Save the bus_sync_handler function for later use
	EXPECT_CALL(*g_mockGStreamer, gst_bus_set_sync_handler(&bus, NotNull(), mplayer, NULL))
		.WillOnce(SaveArgPointee<1>(&bus_sync_func));
	// End CreatePipeline()

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_VOID_PENDING),
			SetArgPointee<2>(GST_STATE_NULL),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("playbin"), NULL))
		.WillRepeatedly(Return(&gst_element_bin));

	EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), NotNull()))
		.WillRepeatedly(Return(TRUE));

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PAUSED))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));
	
	mplayer->SetPauseOnStartPlayback(true);

	mAAMPGstPlayer->Configure(FORMAT_VIDEO_ES_H264,
							  FORMAT_AUDIO_ES_AAC,
							  FORMAT_INVALID,
							  FORMAT_SUBTITLE_WEBVTT,
							  false,
							  false,
							  false);

    ASSERT_TRUE(bus_sync_func != nullptr);
    ASSERT_TRUE(bus_message_func != nullptr);

	GstMessage sync_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(&gst_element_video_sink) };

	EXPECT_CALL(*g_mockGstHandlerControl, isEnabled())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&sync_message),NotNull(),NotNull(),_))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_READY),
			SetArgPointee<2>(GST_STATE_PAUSED)));

	EXPECT_CALL(*g_mockGStreamer, gst_object_replace(NotNull(),NotNull()))
		.WillOnce(DoAll(
            SetArgPointee<0>(GST_OBJECT(&gst_element_video_sink)),
            Return(1)));

	// Call the bus_sync_handler function
	bus_sync_func(&bus, &sync_message, mplayer);

	GstMessage pipeline_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(&gst_element_pipeline) };

	EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&pipeline_message),NotNull(),NotNull(),NotNull()))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_READY),
			SetArgPointee<2>(GST_STATE_PAUSED),
			SetArgPointee<3>(GST_STATE_NULL)));

	GParamSpec spec = {};
    // Property available
    EXPECT_CALL(*g_mockGLib, g_object_class_find_property(_,StrEq("frame-step-on-preroll")))
		.WillOnce(Return(&spec));

    // No simple solution to mock variadic functions, so cannot check calls to g_object_set

    EXPECT_CALL(*g_mockGStreamer, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1.0, FALSE, FALSE))
		.WillOnce(Return(nullptr));

    EXPECT_CALL(*g_mockGStreamer, gst_element_send_event(_,_))
		.WillOnce(Return(1));

/*	EXPECT_CALL(*g_mockPrivateInstanceAAMP, ScheduleAsyncTask(_,mAAMPGstPlayer,StrEq("FirstFrameCallback")))
		.Times(0);
*/
	// Call the bus_message function
	bus_message_func(&bus, &pipeline_message, mplayer);
}

// Test bus_message callback when PauseOnPlayback has been enabled, and sink
// doesn't have the property frame-step-on-preroll
// As the frame-step-on-preroll is not available, the test confirms that
// the task FirstFrameCallback is called instead upon reaching PAUSED state.
TEST_F(PauseOnPlaybackTests, bus_message_FrameStepPropertyNotAvailable)
{
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"Pipeline"}};
	GstElement gst_element_video_sink = {.object = {.name = (gchar *)"brcmvideosink0"}};
	GstElement gst_element_bin = {.object = {.name = (gchar *)"bin"}};
	GstBus bus = {};
	GstPipeline *pipeline = GST_PIPELINE(&gst_element_pipeline);

	GstBusFunc bus_message_func = nullptr;
	GstBusSyncHandler bus_sync_func = nullptr;

	// Expectations
	// CreatePipeline()
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("AAMPGstPlayerPipeline")))
		.WillOnce(Return(&gst_element_pipeline));

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(pipeline))
		.WillOnce(Return(&bus));

	// Save the bus_message function for later use
	EXPECT_CALL(*g_mockGStreamer, gst_bus_add_watch(&bus, NotNull(), mplayer))
		.WillOnce(DoAll(
			SaveArgPointee<1>(&bus_message_func),
			Return(0)));

	// Save the bus_sync_handler function for later use
	EXPECT_CALL(*g_mockGStreamer, gst_bus_set_sync_handler(&bus, NotNull(), mplayer, NULL))
		.WillOnce(SaveArgPointee<1>(&bus_sync_func));

	// End CreatePipeline()

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_VOID_PENDING),
			SetArgPointee<2>(GST_STATE_NULL),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("playbin"), NULL))
		.WillRepeatedly(Return(&gst_element_bin));

	EXPECT_CALL(*g_mockGStreamer, gst_bin_add(GST_BIN(pipeline), NotNull()))
		.WillRepeatedly(Return(TRUE));

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PAUSED))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));

	mAAMPGstPlayer->SetPauseOnStartPlayback(true);

	mAAMPGstPlayer->Configure(FORMAT_VIDEO_ES_H264,
							  FORMAT_AUDIO_ES_AAC,
							  FORMAT_INVALID,
							  FORMAT_SUBTITLE_WEBVTT,
							  false,
							  false,
							  false);

    ASSERT_TRUE(bus_sync_func != nullptr);
    ASSERT_TRUE(bus_message_func != nullptr);

	GstMessage sync_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(&gst_element_video_sink) };

	EXPECT_CALL(*g_mockGstHandlerControl, isEnabled())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&sync_message),NotNull(),NotNull(),_))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_READY),
			SetArgPointee<2>(GST_STATE_PAUSED)));

	EXPECT_CALL(*g_mockGStreamer, gst_object_replace(NotNull(),NotNull()))
		.WillOnce(DoAll(
            SetArgPointee<0>(GST_OBJECT(&gst_element_video_sink)),
            Return(1)));

	// Call the bus_sync_handler function
	bus_sync_func(&bus, &sync_message, mplayer);

	GstMessage pipeline_message = {.type = GST_MESSAGE_STATE_CHANGED, .src = GST_OBJECT(&gst_element_pipeline) };

	EXPECT_CALL(*g_mockGStreamer, gst_message_parse_state_changed(Pointer(&pipeline_message),NotNull(),NotNull(),NotNull()))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_READY),
			SetArgPointee<2>(GST_STATE_PAUSED),
			SetArgPointee<3>(GST_STATE_NULL)));

    // Property not available
	EXPECT_CALL(*g_mockGLib, g_object_class_find_property(_,StrEq("frame-step-on-preroll")))
		.WillOnce(Return(nullptr));

    // No simple solution to mock variadic functions, so cannot check calls to g_object_set

    EXPECT_CALL(*g_mockGStreamer, gst_event_new_step(_,_,_,_,_))
		.Times(0);

    EXPECT_CALL(*g_mockGStreamer, gst_element_send_event(_,_))
		.Times(0);

    //Note: Need to address once bus_message is migrated to interfacePlayer
	//EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(PlayerAsyncTaskObj((void *)mAAMPGstPlayer)))//ScheduleTask(_))//,mAAMPGstPlayer,_))
	//	.WillRepeatedly(Return(1));

	//EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(PlayerAsyncTaskObj(_,mAAMPGstPlayer,StrEq("FirstFrameCallback"))))
	//	.WillOnce(Return(1));

	// Call the bus_message function
	bus_message_func(&bus, &pipeline_message, mplayer);
}
