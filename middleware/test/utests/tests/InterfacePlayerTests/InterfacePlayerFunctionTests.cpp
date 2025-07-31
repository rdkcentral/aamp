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

TEST_F(InterfacePlayerTests, SetPauseOnStartPlayback)
{
	EXPECT_EQ(mPlayerContext->pauseOnStartPlayback, false);
	mInterfaceGstPlayer->SetPauseOnStartPlayback(true);
	EXPECT_EQ(mPlayerContext->pauseOnStartPlayback, true);
	mInterfaceGstPlayer->SetPauseOnStartPlayback(false);
	EXPECT_EQ(mPlayerContext->pauseOnStartPlayback, false);
}

TEST_F(InterfacePlayerTests, SetEncryption)
{
	void* testEncryptPointer = reinterpret_cast<void*>(0x1234); // A dummy pointer for testing
	void* testDrmSessionMgr = reinterpret_cast<void*>(0x5678); // Another dummy pointer for testing

	mInterfaceGstPlayer->setEncryption(testEncryptPointer, testDrmSessionMgr);
	EXPECT_EQ(mInterfaceGstPlayer->mEncrypt, testEncryptPointer);
	EXPECT_EQ(mInterfaceGstPlayer->mDRMSessionManager, testDrmSessionMgr);
}

TEST_F(InterfacePlayerTests, SetPreferredDRM)
{
	const char* testDrmID1 = "Widevine";
	mInterfaceGstPlayer->SetPreferredDRM(testDrmID1);
	EXPECT_STREQ(mInterfaceGstPlayer->mDrmSystem, testDrmID1);
	//null check
	const char* testDrmID2 = NULL;
	mInterfaceGstPlayer->SetPreferredDRM(testDrmID2);
	EXPECT_STREQ(mInterfaceGstPlayer->mDrmSystem, testDrmID1);
}

TEST_F(InterfacePlayerTests, GstSetSeekPosition)
{
	double testPosition1 = 30.5;
	mInterfaceGstPlayer->SetSeekPosition(testPosition1);
	EXPECT_DOUBLE_EQ(mPlayerContext->seekPosition, testPosition1);
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		EXPECT_TRUE(mPlayerContext->stream[i].pendingSeek);
	}

	double testPosition2 = 100.0;
	mInterfaceGstPlayer->SetSeekPosition(testPosition2);
	EXPECT_DOUBLE_EQ(mPlayerContext->seekPosition, testPosition2);
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		EXPECT_TRUE(mPlayerContext->stream[i].pendingSeek);
	}

	// Test with a negative position
	double testPosition3 = -5.0;
	mInterfaceGstPlayer->SetSeekPosition(testPosition3);
	EXPECT_DOUBLE_EQ(mPlayerContext->seekPosition, testPosition3);
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		EXPECT_TRUE(mPlayerContext->stream[i].pendingSeek);
	}
}

TEST_F(InterfacePlayerTests, GstTimerRemove)
{
	g_mockGLib = new StrictMock<MockGLib>();


	guint testTaskID = 1234;
	const char* testTimerName = "testTimer";

	EXPECT_CALL(*g_mockGLib, g_source_remove(testTaskID))
		.WillOnce(Return(TRUE));
	mInterfaceGstPlayer->TimerRemove(testTaskID, testTimerName);
	EXPECT_EQ(testTaskID, 0);

	mInterfaceGstPlayer->TimerRemove(testTaskID, testTimerName); // Task ID is 0

}

TEST_F(InterfacePlayerTests, GstDisconnectSignalsTest)
{
	mPlayerConfigParams->enableDisconnectSignals =false;
	mInterfaceGstPlayer->DisconnectSignals();
	mPlayerConfigParams->enableDisconnectSignals =true;
	mInterfaceGstPlayer->DisconnectSignals();

	 // Create a dummy pipeline and elements
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_element_src = {.object = {.name = (gchar *)"fakesrc"}};
	GstElement gst_element_sink = {.object = {.name = (gchar *)"fakesink"}};

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq("testpipeline")))
			.WillOnce(Return(&gst_element_pipeline));
	EXPECT_CALL(*g_mockGStreamer, gst_element_factory_make(StrEq("fakesrc"), StrEq("source")))
		.WillOnce(Return(&gst_element_src));

	GstElement *pipeline = gst_pipeline_new("testpipeline");
	GstElement *source = gst_element_factory_make("fakesrc", "source");

	mPlayerContext->pipeline = pipeline;

	EXPECT_CALL(*g_mockGLib, g_type_check_instance_is_a(_,0)).Times(2)
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockGLib, g_signal_handler_is_connected(_,_)).Times(2)
		.WillOnce(Return(false)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockGLib, g_signal_handler_disconnect(_,_))
		.WillOnce(Return(true));

	mPlayerContext->mCallBackIdentifiers.push_back(GstPlayerPriv::CallbackData(nullptr, 0 , "test1"));	//data.instance == nullptr
	mPlayerContext->mCallBackIdentifiers.push_back( GstPlayerPriv::CallbackData(source, 0 , "test2"));	//data.id == 0
	mPlayerContext->mCallBackIdentifiers.push_back( GstPlayerPriv::CallbackData(source, 1 , "test3"));	//!elements.count(data.instance)
	mPlayerContext->mCallBackIdentifiers.push_back( GstPlayerPriv::CallbackData(pipeline, 5 , "test4")); //!g_signal_handler_is_connected(data.instance, data.id)
	mPlayerContext->mCallBackIdentifiers.push_back( GstPlayerPriv::CallbackData(pipeline, 5 , "test5"));
	mInterfaceGstPlayer->DisconnectSignals();

	EXPECT_EQ(mPlayerContext->mCallBackIdentifiers.size(), 0);
}

TEST_F(InterfacePlayerTests, GstRemoveProbes)
{
	GstPad pad1 = {.object = {.name = (gchar *)"pad1"}};
	GstPad pad2 = {.object = {.name = (gchar *)"pad2"}};
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].demuxPad = &pad1;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].demuxProbeId = 1234;
	mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].demuxPad = &pad2;
	mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].demuxProbeId = 5678;

	EXPECT_CALL(*g_mockGStreamer, gst_pad_remove_probe(&pad1, 1234));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_remove_probe(&pad2, 5678));
	mInterfaceGstPlayer->RemoveProbes();
}

TEST_F(InterfacePlayerTests, GstDestroyPipeline)
{
	delete g_mockGStreamer;
	g_mockGStreamer = new StrictMock<MockGStreamer>();

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(mPlayerContext->pipeline));

	//deleting bus
	GstBus gst_element_bus = {.object = {.name = (gchar *)"testbus"}};
	mPlayerContext->bus = &gst_element_bus;
	EXPECT_CALL(*g_mockGStreamer, gst_bus_remove_watch(mPlayerContext->bus));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(mPlayerContext->bus));

	//deleting task_pool
	GstTaskPool gst_element_task_pool = {.object = {.name = (gchar *)"testtaskpool"}};
	mPlayerContext->task_pool = &gst_element_task_pool;
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(mPlayerContext->task_pool));


	//deleting positionQuery
	GstQuery* gst_element_query = new GstQuery();
	mPlayerContext->positionQuery = gst_element_query;
	EXPECT_CALL(*g_mockGStreamer, gst_mini_object_unref(NotNull())); // unable to mock gst_query_unref

	mInterfaceGstPlayer->DestroyPipeline();
	EXPECT_EQ(mPlayerContext->pipeline, nullptr);
	EXPECT_EQ(mPlayerContext->bus, nullptr);
	EXPECT_EQ(mPlayerContext->task_pool, nullptr);
	EXPECT_EQ(mPlayerContext->positionQuery, nullptr);

}

TEST_F(InterfacePlayerTests, TearDownStreamTest_successvideo)
{

	GstElement gst_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_sinkbin = {.object = {.name = (gchar *)"testbin"}};

	gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_VIDEO];
	stream->format = GST_FORMAT_VIDEO_ES_H264;
	stream->eosReached = true;
	stream->bufferUnderrun = true;

	mPlayerContext->pipeline = &gst_pipeline;
	mPlayerContext->buffering_in_progress = true;
	stream->sinkbin = &gst_sinkbin;


	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_sinkbin, _,_,0))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));
	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_sinkbin, GST_STATE_NULL))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));


	mInterfaceGstPlayer->TearDownStream(eGST_MEDIATYPE_VIDEO);
	EXPECT_EQ(stream->format, GST_FORMAT_INVALID);
	EXPECT_EQ(stream->bufferUnderrun, false);
	EXPECT_EQ(stream->eosReached, false);
	EXPECT_EQ(mPlayerContext->buffering_in_progress, false);
	EXPECT_EQ(stream->sinkbin, nullptr);


}

TEST_F(InterfacePlayerTests, TearDownStreamTest_failvideo)
{

	GstElement gst_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_sinkbin = {.object = {.name = (gchar *)"testbin"}};

	gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_VIDEO];
	stream->format = GST_FORMAT_VIDEO_ES_H264;
	stream->eosReached = true;
	stream->bufferUnderrun = true;

	mPlayerContext->pipeline = &gst_pipeline;
	mPlayerContext->buffering_in_progress = true;

	mInterfaceGstPlayer->TearDownStream(eGST_MEDIATYPE_VIDEO);
	EXPECT_EQ(stream->format, GST_FORMAT_INVALID);
	EXPECT_EQ(stream->bufferUnderrun, false);
	EXPECT_EQ(stream->eosReached, false);
	EXPECT_EQ(mPlayerContext->buffering_in_progress, false);
	EXPECT_EQ(stream->sinkbin, nullptr);
}

TEST_F(InterfacePlayerTests, TearDownStreamTest_misc)
{
	gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_AUDIO];
	stream->format = GST_FORMAT_VIDEO_ES_H264; //dummy value
	mInterfaceGstPlayer->TearDownStream(eGST_MEDIATYPE_AUDIO);
	stream = &mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE];
	stream->format = GST_FORMAT_VIDEO_ES_H264; //dummy value
	mInterfaceGstPlayer->TearDownStream(eGST_MEDIATYPE_SUBTITLE);
}

TEST_F(InterfacePlayerTests, GstStopTestTrue)
{
	mPlayerContext->syncControl.enable();
	mPlayerContext->aSyncControl.enable();
	GstBus gst_element_bus = {.object = {.name = (gchar *)"testbus"}};
	mPlayerContext->bus = &gst_element_bus;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;

	mPlayerContext->firstProgressCallbackIdleTask.taskID = 100;
	mPlayerContext->firstProgressCallbackIdleTask.taskIsPending = true;

	mPlayerContext->bufferingTimeoutTimerId = 200;
	mPlayerContext->ptsCheckForEosOnUnderflowIdleTaskId = 300;
	mPlayerContext->eosCallbackIdleTaskPending = true;
	mPlayerContext->firstFrameCallbackIdleTaskPending = true;

	mPlayerConfigParams->eosInjectionMode = GstEOS_INJECTION_MODE_STOP_ONLY;

	//Expect_Calls
	EXPECT_CALL(*g_mockGStreamer, gst_bus_remove_watch(mPlayerContext->bus)).Times(2);
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(mPlayerContext->bus));
	EXPECT_CALL(*g_mockGLib, g_source_remove(200));
	EXPECT_CALL(*g_mockGLib, g_source_remove(300));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(mPlayerContext->pipeline));
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _,_,0))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));
	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_NULL))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));


	mInterfaceGstPlayer->Stop(true);
	EXPECT_EQ(mPlayerContext->syncControl.isEnabled(),false);
	EXPECT_EQ(mPlayerContext->aSyncControl.isEnabled(),false);
	EXPECT_EQ(mPlayerContext->firstProgressCallbackIdleTask.taskID,0);
	EXPECT_EQ(mPlayerContext->firstProgressCallbackIdleTask.taskIsPending,false);
	EXPECT_EQ(mPlayerContext->bufferingTimeoutTimerId,PLAYER_TASK_ID_INVALID);
	EXPECT_EQ(mPlayerContext->ptsCheckForEosOnUnderflowIdleTaskId,PLAYER_TASK_ID_INVALID);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskId,PLAYER_TASK_ID_INVALID);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskPending,false);
	EXPECT_EQ(mPlayerContext->firstFrameCallbackIdleTaskId,PLAYER_TASK_ID_INVALID);
	EXPECT_EQ(mPlayerContext->firstFrameCallbackIdleTaskPending,false);
}


TEST_F(InterfacePlayerTests, TestResetGstEvents)
{
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		mPlayerContext->stream[i].resetPosition = false;
		mPlayerContext->stream[i].pendingSeek = true;
		mPlayerContext->stream[i].eosReached = true;
		mPlayerContext->stream[i].firstBufferProcessed = true;
	}

	mInterfaceGstPlayer->ResetGstEvents();

	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		EXPECT_TRUE(mPlayerContext->stream[i].resetPosition);
		EXPECT_FALSE(mPlayerContext->stream[i].pendingSeek);
		EXPECT_FALSE(mPlayerContext->stream[i].eosReached);
		EXPECT_FALSE(mPlayerContext->stream[i].firstBufferProcessed);
	}
}

TEST_F(InterfacePlayerTests, SetPendingSeekTrue)
{
	mInterfaceGstPlayer->SetPendingSeek(true);
	for (int i = 0; i < GST_TRACK_COUNT; ++i)
	{
		EXPECT_TRUE(mPlayerContext->stream[i].pendingSeek);
	}
}

TEST_F(InterfacePlayerTests, SetPendingSeekFalse)
{
	mInterfaceGstPlayer->SetPendingSeek(false);
	for (int i = 0; i < GST_TRACK_COUNT; ++i)
	{
		EXPECT_FALSE(mPlayerContext->stream[i].pendingSeek);
	}
}

TEST_F(InterfacePlayerTests, GetSetTrickTearDownTrue) {
	mInterfaceGstPlayer->SetTrickTearDown(true);
	EXPECT_TRUE(mInterfaceGstPlayer->GetTrickTeardown());
}

TEST_F(InterfacePlayerTests, GetSetTrickTearDownFalse) {
	mInterfaceGstPlayer->SetTrickTearDown(false);
	EXPECT_FALSE(mInterfaceGstPlayer->GetTrickTeardown());
}

TEST_F(InterfacePlayerTests, IdleTaskRemove_TaskExists) {

	GstTaskControlData taskDetails("TestTask");
	taskDetails.taskID = 1;
	taskDetails.taskIsPending = true;

	bool result = mInterfaceGstPlayer->IdleTaskRemove(taskDetails);

	EXPECT_TRUE(result);
	EXPECT_EQ(taskDetails.taskID, 0);
	EXPECT_FALSE(taskDetails.taskIsPending);
}

TEST_F(InterfacePlayerTests, IdleTaskRemove_TaskDoesNotExist) 
{
	GstTaskControlData taskDetails("TestTask");
	taskDetails.taskID = 0;
	taskDetails.taskIsPending = true;

	bool result = mInterfaceGstPlayer->IdleTaskRemove(taskDetails);

	EXPECT_FALSE(result);
	EXPECT_EQ(taskDetails.taskID, 0);
	EXPECT_FALSE(taskDetails.taskIsPending);
}

TEST_F(InterfacePlayerTests, IsUsingRialtoSink_true) 
{
	mPlayerContext->usingRialtoSink = true;
	EXPECT_TRUE(mInterfaceGstPlayer->IsUsingRialtoSink());
}

TEST_F(InterfacePlayerTests, IsUsingRialtoSink_false) 
{
	mPlayerContext->usingRialtoSink = false;
	EXPECT_FALSE(mInterfaceGstPlayer->IsUsingRialtoSink());
}

TEST_F(InterfacePlayerTests, IsUsingRialtoSink_null) 
{
	mPlayerContext = nullptr;
	EXPECT_FALSE(mInterfaceGstPlayer->IsUsingRialtoSink());
	mPlayerContext = new GstPlayerPriv(); //to avoid segfault as null context not expected as such. causes crash at InterfacePlayerRDK::GstDestroyPipeline
}

TEST_F(InterfacePlayerTests, GstFlush_PipelineNull)
{
	double position = 10.0;
	int rate = 1;
	bool shouldTearDown = false;
	bool isAppSeek = false;

	EXPECT_FALSE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
}

TEST_F(InterfacePlayerTests, GstFlush_PipelineNotPlayingOrPaused)
{
	double position = 10.0;
	int rate = 1;
	bool shouldTearDown = true;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_READY),
			SetArgPointee<2>(GST_STATE_NULL),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(_, _, _, _, _, _, _, _)).Times(0);

	EXPECT_FALSE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
}

TEST_F(InterfacePlayerTests, GstFlush_DisableAsyncForTrickplay)
{
	double position = 10.0;
	int rate = 30; //trickplay
	bool shouldTearDown = true;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_element_audio_sink = {.object = {.name = (gchar *)"testaudiosink"}};
	mPlayerContext->pipeline = &gst_element_pipeline; mPlayerContext->audio_sink = &gst_element_audio_sink;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].format = GST_FORMAT_ISO_BMFF;
	mPlayerContext->rate = rate;
	//mPlayerConfigParams->platformType = eGST_PLATFORM_REALTEK;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(&gst_element_pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
		.WillOnce(Return(TRUE));

	EXPECT_TRUE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
}


TEST_F(InterfacePlayerTests, GstFlush_AudioDecoderNotReady)
{
	double position = 10.0;
	int rate = 1;
	bool shouldTearDown = true;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_element_audio_dec = {.object = {.name = (gchar *)"testaudiodec"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->audio_dec = &gst_element_audio_dec;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_audio_dec, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_READY),
			SetArgPointee<2>(GST_STATE_NULL),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(_, _, _, _, _, _, _, _)).Times(0);

	EXPECT_FALSE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
}

TEST_F(InterfacePlayerTests, GstFlush_Success)
{
	double position = 10.0;
	int rate = 1;
	bool shouldTearDown = false;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_element_audio_dec = {.object = {.name = (gchar *)"testaudiodec"}};
	GstElement gst_element_audio_sink = {.object = {.name = (gchar *)"testaudiosink"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->audio_dec = &gst_element_audio_dec;
	mPlayerContext->audio_sink = &gst_element_audio_sink;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].format = GST_FORMAT_ISO_BMFF;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun = true;
	mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].bufferUnderrun = true;
	mPlayerContext->eosCallbackIdleTaskPending = true;
	mPlayerContext->ptsCheckForEosOnUnderflowIdleTaskId = 300;
	mPlayerContext->bufferingTimeoutTimerId = 200;
	mPlayerContext->rate = rate;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_audio_dec, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(&gst_element_pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
		.WillOnce(Return(TRUE));

	EXPECT_CALL(*g_mockGLib, g_source_remove(200));
	EXPECT_CALL(*g_mockGLib, g_source_remove(300));

	EXPECT_TRUE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
	EXPECT_FALSE(mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun);
	EXPECT_FALSE(mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].bufferUnderrun);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskId, PLAYER_TASK_ID_INVALID);
	EXPECT_FALSE(mPlayerContext->eosCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->ptsCheckForEosOnUnderflowIdleTaskId, PLAYER_TASK_ID_INVALID);
	EXPECT_EQ(mPlayerContext->bufferingTimeoutTimerId, PLAYER_TASK_ID_INVALID);
	EXPECT_FALSE(mPlayerContext->eosSignalled);
	EXPECT_EQ(mPlayerContext->numberOfVideoBuffersSent, 0);
}

TEST_F(InterfacePlayerTests, GstFlush_SeekFailed)
{
	double position = 10.0;
	int rate = 1;
	bool shouldTearDown = false;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_element_audio_dec = {.object = {.name = (gchar *)"testaudiodec"}};
	GstElement gst_element_audio_sink = {.object = {.name = (gchar *)"testaudiosink"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->audio_dec = &gst_element_audio_dec;
	mPlayerContext->audio_sink = &gst_element_audio_sink;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].format = GST_FORMAT_ISO_BMFF;
	mPlayerContext->rate = rate;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_audio_dec, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(&gst_element_pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
		.WillOnce(Return(FALSE));

	EXPECT_TRUE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek)); //FLUSH is true even if seek is failed , needs to be confirmed TODO.

}

TEST_F(InterfacePlayerTests, GstFlush_ProgressiveMediaFormat)
{
	double position = 10.0;
	int rate = 2; // Trickplay rate
	bool shouldTearDown = false;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerConfigParams->media = eGST_MEDIAFORMAT_PROGRESSIVE;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(&gst_element_pipeline, rate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
		.WillOnce(Return(TRUE));

	EXPECT_TRUE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
}
