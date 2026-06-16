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
#include "MockGstUtils.h"
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

TEST_F(InterfacePlayerTests, GstFlush_ISOBMFFMediaPositionReset)
{
	double position = 10.0;
	int rate = 2; // Trickplay rate
	bool shouldTearDown = false;
	bool isAppSeek = false;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"testpipeline"}};
	GstElement gst_element_audio_sink = {.object = {.name = (gchar *)"testaudiosink"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->audio_sink = &gst_element_audio_sink;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].format = GST_FORMAT_ISO_BMFF;
	mPlayerContext->rate = rate;
	mPlayerConfigParams->media = eGST_MEDIAFORMAT_DASH;
	mPlayerContext->usingRialtoSink = true;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(
			SetArgPointee<1>(GST_STATE_PLAYING),
			SetArgPointee<2>(GST_STATE_PAUSED),
			Return(GST_STATE_CHANGE_SUCCESS)));

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek(&gst_element_pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, 0 * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
		.WillOnce(Return(TRUE)); //position reset to zero

	EXPECT_TRUE(mInterfaceGstPlayer->Flush(position, rate, shouldTearDown, isAppSeek));
}

TEST_F(InterfacePlayerTests, SignalConnect_Success)
{
	g_mockGstHandlerControl= new StrictMock<MockGstHandlerControl>();
	g_mockGLib = new StrictMock<MockGLib>();

	gpointer instance = reinterpret_cast<gpointer>(0x1234);
	const gchar *detailed_signal = "test-signal";
	GCallback c_handler = reinterpret_cast<GCallback>(0x5678);
	gpointer data = reinterpret_cast<gpointer>(0x9ABC);

	EXPECT_CALL(*g_mockGLib, g_signal_connect(instance, StrEq(detailed_signal), c_handler, data))
		.WillOnce(Return(1));

	mInterfacePrivatePlayer->SignalConnect(instance, detailed_signal, c_handler, data);

	EXPECT_EQ(mPlayerContext->mCallBackIdentifiers.size(), 1);
	EXPECT_EQ(mPlayerContext->mCallBackIdentifiers[0].instance, instance);
}

TEST_F(InterfacePlayerTests, SignalConnect_Failure)
{
	g_mockGLib = new StrictMock<MockGLib>();
	gpointer instance = reinterpret_cast<gpointer>(0x1234);
	const gchar *detailed_signal = "test-signal";
	GCallback c_handler = reinterpret_cast<GCallback>(0x5678);
	gpointer data = reinterpret_cast<gpointer>(0x9ABC);

	EXPECT_CALL(*g_mockGLib, g_signal_connect(instance, StrEq(detailed_signal), c_handler, data))
		.WillOnce(Return(0));

	mInterfacePrivatePlayer->SignalConnect(instance, detailed_signal, c_handler, data);

	EXPECT_EQ(mPlayerContext->mCallBackIdentifiers.size(), 0);
}

TEST_F(InterfacePlayerTests, InitializeSourceForPlayer_Video)
{
	g_mockGstUtils = new StrictMock<MockGstUtils>();

	void* playerInstance = mInterfaceGstPlayer;
	void* source = reinterpret_cast<void*>(0x1234);
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	bool isFogEnabled = true;

	GstCaps caps = {};
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	mPlayerConfigParams->videoBufBytes = 500;
	mPlayerConfigParams->useMp4Demux = false;

	EXPECT_CALL(*g_mockGLib, g_signal_connect(source, StrEq("need-data"), _, playerInstance)).WillOnce(Return(1));
	EXPECT_CALL(*g_mockGLib, g_signal_connect(source, StrEq("enough-data"), _, playerInstance)).WillOnce(Return(1));
	EXPECT_CALL(*g_mockGLib, g_signal_connect(source, StrEq("seek-data"), _, playerInstance)).WillOnce(Return(1));

	EXPECT_CALL(*g_mockGStreamer, gst_app_src_set_caps(_, _));
	EXPECT_CALL(*g_mockGStreamer, gst_mini_object_unref(_));

	EXPECT_CALL(*g_mockGstUtils, GetCaps(_)).WillOnce(Return(&caps));

	mInterfaceGstPlayer->InitializeSourceForPlayer(playerInstance, source, mediaType);

	EXPECT_TRUE(stream->sourceConfigured);

	delete g_mockGstUtils;
}

TEST_F(InterfacePlayerTests, InitializeSourceForPlayer_Audio_CapsNull)
{

	g_mockGstUtils = new StrictMock<MockGstUtils>();

	void* playerInstance = mInterfaceGstPlayer;
	void* source = reinterpret_cast<void*>(0x1234);
	GstMediaType mediaType = eGST_MEDIATYPE_AUDIO;
	bool isFogEnabled = true;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	mPlayerConfigParams->audioBufBytes = 500;

	EXPECT_CALL(*g_mockGLib, g_signal_connect(source, StrEq("need-data"), _, playerInstance)).WillOnce(Return(1));
	EXPECT_CALL(*g_mockGLib, g_signal_connect(source, StrEq("enough-data"), _, playerInstance)).WillOnce(Return(1));
	EXPECT_CALL(*g_mockGLib, g_signal_connect(source, StrEq("seek-data"), _, playerInstance)).WillOnce(Return(1));
	EXPECT_CALL(*g_mockGStreamer, gst_app_src_set_stream_type(GST_APP_SRC(source), GST_APP_STREAM_TYPE_SEEKABLE));

	EXPECT_CALL(*g_mockGstUtils, GetCaps(_)).WillOnce(Return(nullptr));

	mInterfaceGstPlayer->InitializeSourceForPlayer(playerInstance, source, mediaType);

	EXPECT_TRUE(stream->sourceConfigured);

	delete g_mockGstUtils;
}

TEST_F(InterfacePlayerTests, SendGstEvents_PendingSeek)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime pts = 1000;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->pendingSeek = true;
	stream->source = &gst_element_pipeline;
	mPlayerConfigParams->enableGstPosQuery = TRUE;
	mPlayerConfigParams->enablePTSReStamp = TRUE;
	mPlayerConfigParams->vodTrickModeFPS = 24;

	mPlayerContext->seekPosition = 10;

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek_simple(GST_ELEMENT(stream->source), GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, mPlayerContext->seekPosition * GST_SECOND))
		.WillOnce(Return(TRUE));

	mInterfacePrivatePlayer->SendGstEvents(mediaType, pts,mPlayerConfigParams->enableGstPosQuery , mPlayerConfigParams->enablePTSReStamp, mPlayerConfigParams->vodTrickModeFPS);

	EXPECT_FALSE(stream->pendingSeek);
}

TEST_F(InterfacePlayerTests, SendGstEvents_NoPendingSeek)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime pts = 1000;
	mPlayerConfigParams->enablePTSReStamp = TRUE;
	mPlayerConfigParams->vodTrickModeFPS = 24;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->pendingSeek = false;
	stream->source = &gst_element_pipeline;

	mInterfacePrivatePlayer->SendGstEvents(mediaType, pts,mPlayerConfigParams->enableGstPosQuery , mPlayerConfigParams->enablePTSReStamp, mPlayerConfigParams->vodTrickModeFPS);

	EXPECT_FALSE(stream->pendingSeek);
}

TEST_F(InterfacePlayerTests, SendGstEvents_ProtectionEventOtherTrack)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime pts = 1000;
	mPlayerConfigParams->enablePTSReStamp = TRUE;
	mPlayerConfigParams->vodTrickModeFPS = 24;


	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->source = &gst_element_pipeline;
	mPlayerContext->protectionEvent[mediaType] = nullptr;
	mPlayerContext->protectionEvent[eGST_MEDIATYPE_AUDIO] = reinterpret_cast<GstEvent*>(0x1234);

	GstPad pad = {};
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(GST_ELEMENT(stream->source), StrEq("src")))
		.WillRepeatedly(Return(&pad));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(_,_))
		.WillRepeatedly(Return(TRUE));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(_)).Times(2);

	mInterfacePrivatePlayer->SendGstEvents(mediaType, pts,mPlayerConfigParams->enableGstPosQuery , mPlayerConfigParams->enablePTSReStamp, mPlayerConfigParams->vodTrickModeFPS);

	EXPECT_FALSE(stream->resetPosition);
}

TEST_F(InterfacePlayerTests, SendGstEvents_ProtectionEventSameTrack)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime pts = 1000;
	mPlayerConfigParams->enablePTSReStamp = TRUE;
	mPlayerConfigParams->vodTrickModeFPS = 24;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->source = &gst_element_pipeline;
	mPlayerContext->protectionEvent[mediaType] = reinterpret_cast<GstEvent*>(0x1234);

	GstPad pad = {};
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(GST_ELEMENT(stream->source), StrEq("src")))
		.WillRepeatedly(Return(&pad));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(_,_))
		.WillRepeatedly(Return(FALSE));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(_)).Times(2);

	mInterfacePrivatePlayer->SendGstEvents(mediaType, pts,mPlayerConfigParams->enableGstPosQuery , mPlayerConfigParams->enablePTSReStamp, mPlayerConfigParams->vodTrickModeFPS);

	EXPECT_FALSE(stream->resetPosition);
}

TEST_F(InterfacePlayerTests, SendQtDemuxOverrideEvent_EnablePTSReStampFalse)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime pts = 1000;
	const void *ptr = nullptr;
	size_t len = 0;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->source = &gst_element_pipeline;
	mPlayerConfigParams->enablePTSReStamp = false;
	mPlayerContext->rate = 2.0;
	mPlayerConfigParams->vodTrickModeFPS = 30;
	mInterfacePrivatePlayer->mPlayerName = "testPlayer";

	GstPad pad = {};
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(GST_ELEMENT(stream->source), StrEq("src")))
		.WillRepeatedly(Return(&pad));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(_, _))
		.WillRepeatedly(Return(TRUE));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(_)).Times(1);

	gboolean result = mInterfacePrivatePlayer->SendQtDemuxOverrideEvent(mediaType, pts, mPlayerConfigParams->enablePTSReStamp , mPlayerConfigParams->vodTrickModeFPS , ptr, len);

	EXPECT_TRUE(result);
}

TEST_F(InterfacePlayerTests, SendQtDemuxOverrideEvent_EnablePTSReStampTrue)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime pts = 1000;
	const void *ptr = nullptr;
	size_t len = 0;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->source = &gst_element_pipeline;
	mPlayerConfigParams->enablePTSReStamp = true;
	mPlayerContext->rate = 1.0;
	mPlayerConfigParams->vodTrickModeFPS = 30;
	mInterfacePrivatePlayer->mPlayerName = "testPlayer";

	GstPad pad = {};
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(GST_ELEMENT(stream->source), StrEq("src")))
		.WillRepeatedly(Return(&pad));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(_, _))
		.WillRepeatedly(Return(FALSE));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(_)).Times(1);

	gboolean result = mInterfacePrivatePlayer->SendQtDemuxOverrideEvent(mediaType, pts, mPlayerConfigParams->enablePTSReStamp , mPlayerConfigParams->vodTrickModeFPS , ptr, len);

	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, ForwardAudioBuffersToAux_True)
{
	mPlayerContext->forwardAudioBuffers = true;
	mPlayerContext->stream[eGST_MEDIATYPE_AUX_AUDIO].format = GST_FORMAT_ISO_BMFF;

	EXPECT_TRUE(mInterfaceGstPlayer->ForwardAudioBuffersToAux());

	mPlayerContext->forwardAudioBuffers = false;
	mPlayerContext->stream[eGST_MEDIATYPE_AUX_AUDIO].format = GST_FORMAT_ISO_BMFF;

	EXPECT_FALSE(mInterfaceGstPlayer->ForwardAudioBuffersToAux());

	mPlayerContext->forwardAudioBuffers = true;
	mPlayerContext->stream[eGST_MEDIATYPE_AUX_AUDIO].format = GST_FORMAT_INVALID;

	EXPECT_FALSE(mInterfaceGstPlayer->ForwardAudioBuffersToAux());
}

TEST_F(InterfacePlayerTests, GetVideoRectangle)
{
	std::string expectedRectangle = "0,0,1920,1080";
	strncpy(mPlayerContext->videoRectangle, expectedRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	EXPECT_EQ(mInterfaceGstPlayer->GetVideoRectangle(), expectedRectangle);
}

TEST_F(InterfacePlayerTests, GstSetSubtitlePtsOffset_WithSubtitleSink)
{
	GstElement subtitle_sink = {};
	mPlayerContext->subtitle_sink = &subtitle_sink;
	std::uint64_t pts_offset = 1000;
	mInterfaceGstPlayer->SetSubtitlePtsOffset(pts_offset);
}

TEST_F(InterfacePlayerTests, GstSetSubtitlePtsOffset_WithoutSubtitleSink)
{
	mPlayerContext->subtitle_sink = nullptr;
	mInterfaceGstPlayer->SetSubtitlePtsOffset(1000);
}

TEST_F(InterfacePlayerTests, GstResetFirstFrame)
{
	mPlayerContext->firstFrameReceived = true;
	mInterfaceGstPlayer->ResetFirstFrame();
	EXPECT_FALSE(mPlayerContext->firstFrameReceived);
}

TEST_F(InterfacePlayerTests, GstGetVideoPlaybackQuality_StatsNull)
{
	GstElement video_sink = {};
	GstStructure stats = {1};
	mPlayerContext->video_sink = &video_sink;
	GstPlaybackQualityStruct* result = mInterfaceGstPlayer->GetVideoPlaybackQuality();
	EXPECT_EQ(result,nullptr);
}

TEST_F(InterfacePlayerTests, GstGetPositionMilliseconds)
{
	mPlayerContext->pipeline = nullptr;

	long long result = mInterfaceGstPlayer->GetPositionMilliseconds();

	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 0);

	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->positionQuery = nullptr;

	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 0);

	mPlayerContext->positionQuery = &query;
	mPlayerContext->pipelineState = GST_STATE_READY;
	mPlayerContext->paused = false;

	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 0);

	mPlayerContext->pipelineState = GST_STATE_PLAYING;
	mPlayerContext->paused = false;
	mPlayerContext->segmentStart = -1;
	EXPECT_CALL(*g_mockGStreamer, gst_element_query(_,_)).WillRepeatedly(Return(FALSE));
	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 0);

	EXPECT_CALL(*g_mockGStreamer, gst_element_query(_,_)).Times(2).WillOnce(Return(TRUE)).WillOnce(Return(FALSE));
	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 0);

	mPlayerContext->segmentStart = 1;
	mPlayerConfigParams->media = eGST_MEDIAFORMAT_PROGRESSIVE;
	gint64 pos = 5000000;
	EXPECT_CALL(*g_mockGStreamer, gst_query_parse_position(_,_,_))
		.WillOnce(DoAll(SetArgPointee<2>(pos), Return()));
	EXPECT_CALL(*g_mockGStreamer, gst_element_query(_,_)).WillRepeatedly(Return(TRUE));
	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 4); // {(5000000/1000000) -1 } * 4

	mPlayerContext->segmentStart = -1;
	EXPECT_CALL(*g_mockGStreamer, gst_query_parse_position(_,_,_))
		.WillOnce(DoAll(SetArgPointee<2>(pos), Return()));
	EXPECT_CALL(*g_mockGStreamer, gst_element_query(_,_)).Times(2).WillOnce(Return(FALSE)).WillOnce(Return(TRUE));
	EXPECT_EQ(mInterfaceGstPlayer->GetPositionMilliseconds(), 5); // (5000000/1000000)  * 4
}

TEST_F(InterfacePlayerTests, GstGetDurationMilliseconds_PipelineNull)
{
	//PipelineNull
	mPlayerContext->pipeline = nullptr;
	long result = mInterfaceGstPlayer->GetDurationMilliseconds();
	EXPECT_EQ(result, 0);
}

TEST_F(InterfacePlayerTests, GstGetDurationMilliseconds_PipelineNotPlayingOrPaused)
{
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->pipelineState = GST_STATE_READY;
	mPlayerContext->paused = false;

	long result = mInterfaceGstPlayer->GetDurationMilliseconds();

	EXPECT_EQ(result, 0);
}

TEST_F(InterfacePlayerTests, GstGetVideoSize_ValidRectangle)
{
	int width = 0;
	int height = 0;
	std::string videoRectangle = "10,20,1920,1080";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mInterfaceGstPlayer->GetVideoSize(width, height);

	EXPECT_EQ(width, 1920);
	EXPECT_EQ(height, 1080);
}

TEST_F(InterfacePlayerTests, GstGetVideoSize_InvalidRectangle)
{
	int width = 0;
	int height = 0;
	std::string videoRectangle = "10,20,0,0";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mInterfaceGstPlayer->GetVideoSize(width, height);

	EXPECT_EQ(width, 0);
	EXPECT_EQ(height, 0);
}

TEST_F(InterfacePlayerTests, GstGetVideoSize_InvalidRectangle2)
{
	int width = 0;
	int height = 0;
	std::string videoRectangle = "10,20,-12,-240000000000"; //negative values , overflow values
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mInterfaceGstPlayer->GetVideoSize(width, height);

	EXPECT_EQ(width, 0);
	EXPECT_EQ(height, 0);
}

TEST_F(InterfacePlayerTests, GstGetVideoSize_PartialRectangle)
{
	int width = 0;
	int height = 0;
	std::string videoRectangle = "10,20,1920";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mInterfaceGstPlayer->GetVideoSize(width, height);

	EXPECT_EQ(width, 0);
	EXPECT_EQ(height, 0);
}

TEST_F(InterfacePlayerTests, GstGetVideoSize_EmptyRectangle)
{
	int width = 0;
	int height = 0;
	std::string videoRectangle = "";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mInterfaceGstPlayer->GetVideoSize(width, height);

	EXPECT_EQ(width, 0);
	EXPECT_EQ(height, 0);
}

TEST_F(InterfacePlayerTests, GstSetSubtitleMute_WithSubtitleSink)
{
	GstElement subtitle_sink = {};
	mPlayerContext->subtitle_sink = &subtitle_sink;

	EXPECT_CALL(*g_mockGLib, g_object_set(&subtitle_sink, StrEq("mute"), Matcher<int>(true)));
	mInterfaceGstPlayer->SetSubtitleMute(true);
	EXPECT_TRUE(mPlayerContext->subtitleMuted);

	EXPECT_CALL(*g_mockGLib, g_object_set(&subtitle_sink, StrEq("mute"), Matcher<int>(false)));
	mInterfaceGstPlayer->SetSubtitleMute(false);
	EXPECT_FALSE(mPlayerContext->subtitleMuted);
}

TEST_F(InterfacePlayerTests, GstSetSubtitleMute_WithoutSubtitleSink)
{
	mPlayerContext->subtitle_sink = nullptr;

	EXPECT_CALL(*g_mockGLib, g_object_set(_, StrEq("mute"),Matcher<int>(_))).Times(0);
	mInterfaceGstPlayer->SetSubtitleMute(true);
	EXPECT_TRUE(mPlayerContext->subtitleMuted);

	mInterfaceGstPlayer->SetSubtitleMute(false);
	EXPECT_FALSE(mPlayerContext->subtitleMuted);
}

TEST_F(InterfacePlayerTests, GstSetVideoRectangle_SameCoordinates)
{
	int x = 10, y = 20, w = 1920, h = 1080;
	std::string videoRectangle = "10,20,1920,1080";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mInterfaceGstPlayer->SetVideoRectangle(x, y, w, h);

	EXPECT_STREQ(mPlayerContext->videoRectangle, videoRectangle.c_str());
}

TEST_F(InterfacePlayerTests, GstSetVideoRectangle_DifferentCoordinates)
{
	int x = 10, y = 20, w = 1920, h = 1080;
	std::string videoRectangle = "0,0,1280,720";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mPlayerConfigParams->enableRectPropertyCfg = true;
	GstElement video_sink = {};
	mPlayerContext->video_sink = &video_sink;

	EXPECT_CALL(*g_mockGLib, g_object_set(&video_sink, StrEq("rectangle"),  Matcher<char *>(_)));

	mInterfaceGstPlayer->SetVideoRectangle(x, y, w, h);

	EXPECT_STREQ(mPlayerContext->videoRectangle, "10,20,1920,1080");
}

TEST_F(InterfacePlayerTests, GstSetVideoRectangle_EnableRectPropertyCfgFalse)
{
	int x = 10, y = 20, w = 1920, h = 1080;
	std::string videoRectangle = "0,0,1280,720";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mPlayerConfigParams->enableRectPropertyCfg = false;

	mInterfaceGstPlayer->SetVideoRectangle(x, y, w, h);

	EXPECT_STREQ(mPlayerContext->videoRectangle, "10,20,1920,1080");
}

TEST_F(InterfacePlayerTests, GstSetVideoRectangle_VideoSinkNull)
{
	int x = 10, y = 20, w = 1920, h = 1080;
	std::string videoRectangle = "0,0,1280,720";
	strncpy(mPlayerContext->videoRectangle, videoRectangle.c_str(), sizeof(mPlayerContext->videoRectangle) - 1);

	mPlayerConfigParams->enableRectPropertyCfg = true;
	mPlayerContext->video_sink = nullptr;

	mInterfaceGstPlayer->SetVideoRectangle(x, y, w, h);

	EXPECT_STREQ(mPlayerContext->videoRectangle, "10,20,1920,1080");
}


TEST_F(InterfacePlayerTests, StopBuffering_ForceStopTrue)
{
	bool isPlaying = false;
	bool forceStop = true;

	GstElement video_dec = {};
	mPlayerContext->video_dec = &video_dec;
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(_, _, _, _))
			 .WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PLAYING), SetArgPointee<2>(GST_STATE_PLAYING), Return(GST_STATE_CHANGE_SUCCESS)));


	bool result = mInterfaceGstPlayer->StopBuffering(forceStop, isPlaying);

	EXPECT_TRUE(result);
	EXPECT_TRUE(isPlaying);
}


TEST_F(InterfacePlayerTests, StopBuffering_EnoughData)
{
	bool isPlaying = false;
	bool forceStop = false;

	GstElement video_dec = {};
	mPlayerContext->video_dec = &video_dec;
	mPlayerConfigParams->framesToQueue = 5;

	EXPECT_CALL(*g_mockGLib, g_object_get(&video_dec, StrEq("queued_frames"),Matcher<uint*>(_)))
		.WillOnce(DoAll(SetArgPointee<2>(6), Return()));

	GstElement pipeline = {};
	mPlayerContext->pipeline = &pipeline;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(_, _, _, _))
		.WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PLAYING), SetArgPointee<2>(GST_STATE_NULL), Return(GST_STATE_CHANGE_SUCCESS)));

	bool result = mInterfaceGstPlayer->StopBuffering(forceStop, isPlaying);

	EXPECT_TRUE(result);
	EXPECT_TRUE(isPlaying);
}

TEST_F(InterfacePlayerTests, StopBuffering_NotEnoughData)
{
	bool isPlaying = false;
	bool forceStop = false;

	GstElement video_dec = {};
	mPlayerContext->video_dec = &video_dec;
	mPlayerConfigParams->framesToQueue = 5;

	EXPECT_CALL(*g_mockGLib, g_object_get(&video_dec, StrEq("queued_frames"),Matcher<uint*>(_)))
		.WillOnce(DoAll(SetArgPointee<2>(4), Return()));

	bool result = mInterfaceGstPlayer->StopBuffering(forceStop, isPlaying);

	EXPECT_FALSE(result);
	EXPECT_FALSE(isPlaying);
}

TEST_F(InterfacePlayerTests, StopBuffering_GstElementGetStateFailure)
{
	bool isPlaying = false;
	bool forceStop = false;

	GstElement video_dec = {};
	mPlayerContext->video_dec = &video_dec;
	mPlayerConfigParams->framesToQueue = 5;

	EXPECT_CALL(*g_mockGLib, g_object_get(&video_dec, StrEq("queued_frames"),Matcher<uint*>(_)))
		.WillOnce(DoAll(SetArgPointee<2>(6), Return()));

	GstElement pipeline = {};
	mPlayerContext->pipeline = &pipeline;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&pipeline, _, _, _))
		.WillOnce(Return(GST_STATE_CHANGE_FAILURE));

	bool result = mInterfaceGstPlayer->StopBuffering(forceStop, isPlaying);

	EXPECT_FALSE(result);
	EXPECT_FALSE(isPlaying);
}

TEST_F(InterfacePlayerTests, WaitForSourceSetup_Success)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->sourceConfigured = false;

	std::thread setupThread([&]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		{
			std::lock_guard<std::mutex> lock(mInterfaceGstPlayer->mSourceSetupMutex);
			stream->sourceConfigured = true;
		}
		mInterfaceGstPlayer->mSourceSetupCV.notify_all();
	});

	bool result = mInterfaceGstPlayer->WaitForSourceSetup(mediaType);

	EXPECT_TRUE(result);
	EXPECT_TRUE(stream->sourceConfigured);

	setupThread.join();
}

TEST_F(InterfacePlayerTests, WaitForSourceSetup_Timeout)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->sourceConfigured = false;

	bool result = mInterfaceGstPlayer->WaitForSourceSetup(mediaType);

	EXPECT_FALSE(result);
	EXPECT_FALSE(stream->sourceConfigured);
}

TEST_F(InterfacePlayerTests, WaitForSourceSetup_PauseInjector)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->sourceConfigured = false;
	mInterfaceGstPlayer->mPauseInjector = true;

	bool result = mInterfaceGstPlayer->WaitForSourceSetup(mediaType);

	EXPECT_FALSE(result);
	EXPECT_FALSE(stream->sourceConfigured);
}

TEST_F(InterfacePlayerTests, ForwardBuffersToAuxPipeline_WaitForSourceSetupFailed)
{
	GstBuffer buffer = {};
    gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_AUX_AUDIO];
    stream->format = GST_FORMAT_ISO_BMFF;
    stream->sourceConfigured = false;

	EXPECT_CALL(*g_mockGStreamer, gst_buffer_copy_into(_, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockGStreamer, gst_app_src_push_buffer(_, _)).Times(0);

    // Run the method
    mInterfacePrivatePlayer->ForwardBuffersToAuxPipeline(&buffer, true, mInterfaceGstPlayer);
}

TEST_F(InterfacePlayerTests, ForwardBuffersToAuxPipeline_PushBufferFailed)
{
	GstBuffer buffer = {};
	gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_AUX_AUDIO];
	stream->sourceConfigured = true;
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->source = &gst_element_pipeline;

	GstBuffer fwdBuffer = {};
	//assert(false) in source code causes premature exit which causes expect_call to fail
	ON_CALL(*g_mockGStreamer, gst_buffer_new())
		.WillByDefault(Return(&fwdBuffer));
	ON_CALL(*g_mockGStreamer, gst_buffer_copy_into(_,_,_,_,_))
		.WillByDefault(Return(TRUE));
	ON_CALL(*g_mockGStreamer, gst_app_src_push_buffer(_,_))
		.WillByDefault(Return(GST_FLOW_ERROR));

	//catches the assert(false) in the function
	EXPECT_DEATH(mInterfacePrivatePlayer->ForwardBuffersToAuxPipeline(&buffer,true,mInterfaceGstPlayer), "Assertion");
}

TEST_F(InterfacePlayerTests, HandleVideoBufferSent_SubsequentBuffer)
{
	mPlayerContext->numberOfVideoBuffersSent = 5;
	bool result = mInterfaceGstPlayer->HandleVideoBufferSent();

	EXPECT_FALSE(result);
	EXPECT_EQ(mPlayerContext->numberOfVideoBuffersSent, 6);
}

TEST_F(InterfacePlayerTests, SetPlayerName)
{
	std::string testName = "TestPlayer";
	mInterfaceGstPlayer->SetPlayerName(testName);
	EXPECT_EQ(mInterfacePrivatePlayer->mPlayerName, testName);
}

TEST_F(InterfacePlayerTests, PauseInjector_SetsPauseInjectorToTrue)
{
	mInterfaceGstPlayer->mPauseInjector = false;
	mInterfaceGstPlayer->PauseInjector();
	EXPECT_TRUE(mInterfaceGstPlayer->mPauseInjector);
}

TEST_F(InterfacePlayerTests, ResumeInjector_SetsPauseInjectorToFalseAndNotifiesAll)
{
	mInterfaceGstPlayer->mPauseInjector = true;
	mInterfaceGstPlayer->ResumeInjector();
	EXPECT_FALSE(mInterfaceGstPlayer->mPauseInjector);
}

TEST_F(InterfacePlayerTests, SendNewSegmentEvent_VideoMediaType)
{
	GstMediaType mediaType = eGST_MEDIATYPE_VIDEO;
	GstClockTime startPts = 1000;
	GstClockTime stopPts = 2000;
	mPlayerContext->stream[mediaType].format = GST_FORMAT_ISO_BMFF;

	GstPad gst_pad = {};
	GstEvent gst_event = {};

	// Mock the necessary GStreamer functions and objects
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(_, StrEq("src")))
		.WillRepeatedly(Return(&gst_pad));
	EXPECT_CALL(*g_mockGStreamer, gst_segment_init(_, GST_FORMAT_TIME))
		.WillRepeatedly(DoAll(SetArgPointee<0>(GstSegment{}), Return()));
	EXPECT_CALL(*g_mockGStreamer, gst_event_new_segment(_))
		.WillRepeatedly(Return(&gst_event));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(_, _))
		.WillOnce(Return(TRUE)).WillOnce(Return(FALSE));	// success and failure case
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(_))
		.Times(2);


	mInterfacePrivatePlayer->SendNewSegmentEvent(mediaType, startPts, stopPts);	//success
	mInterfacePrivatePlayer->SendNewSegmentEvent(mediaType, startPts, stopPts);	//failure
}

TEST_F(InterfacePlayerTests, Queue_and_ClearProtectionEvent)
{
	std::string formatType = "cenc";
	const char *protSystemId = "test-system-id";
	const char initData[] = "test-init-data";
	size_t initDataSize = sizeof(initData);
	int mediaType = eGST_MEDIATYPE_VIDEO;
	GstBuffer gst_buffer = {};
	GstEvent gst_event = {};

	// Mock the necessary GStreamer functions and objects
	EXPECT_CALL(*g_mockGStreamer, gst_buffer_new_wrapped(NotNull(), initDataSize))
		.WillRepeatedly(Return(&gst_buffer));
	EXPECT_CALL(*g_mockGStreamer, gst_event_new_protection(StrEq(protSystemId), &gst_buffer, StrEq(formatType.c_str())))
		.WillRepeatedly(Return(&gst_event));

	mInterfaceGstPlayer->QueueProtectionEvent(formatType, protSystemId, initData, initDataSize, mediaType);

	//call again to test GstPrivateContext->protectionEvent[type] != NULL
	mInterfaceGstPlayer->QueueProtectionEvent(formatType, protSystemId, initData, initDataSize, mediaType);

	// Verify the protection event is queued
	EXPECT_EQ(mPlayerContext->protectionEvent[mediaType], &gst_event);
	EXPECT_NE(mPlayerContext->protectionEvent[mediaType],nullptr) ;

	//test InterfacePlayerRDK::ClearProtectionEvent()
	mInterfaceGstPlayer->ClearProtectionEvent();
	EXPECT_EQ(mPlayerContext->protectionEvent[mediaType], nullptr);
}

TEST_F(InterfacePlayerTests, Pause_Success)
{
	bool pause = true;
	bool forceStopGstreamerPreBuffering = true;

	// Set pipeline to a non-null value
	mPlayerContext->pipeline = &gst_element_pipeline;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(_, NotNull(), NotNull(), _))
		.WillRepeatedly(Return(GST_STATE_CHANGE_SUCCESS));

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(_, GST_STATE_PAUSED))
		.WillOnce(Return(GST_STATE_CHANGE_ASYNC));

	bool result = mInterfaceGstPlayer->Pause(pause, forceStopGstreamerPreBuffering);

	EXPECT_TRUE(result);
	EXPECT_FALSE(mPlayerContext->buffering_in_progress);
	EXPECT_EQ(mPlayerContext->buffering_target_state, GST_STATE_PAUSED);
	EXPECT_TRUE(mPlayerContext->paused);
	EXPECT_FALSE(mPlayerContext->pendingPlayState);
}


TEST_F(InterfacePlayerTests, Pause_PipelineNull)
{
	bool pause = true;
	bool forceStopGstreamerPreBuffering = false;

	// Set pipeline to NULL
	mPlayerContext->pipeline = NULL;
	bool result = mInterfaceGstPlayer->Pause(pause, forceStopGstreamerPreBuffering);
	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, Pause_Failure)
{
	bool pause = true;
	bool forceStopGstreamerPreBuffering = false;

	// Set pipeline to a non-null value
	mPlayerContext->pipeline = &gst_element_pipeline;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(_, NotNull(), NotNull(), _))
		.WillRepeatedly(Return(GST_STATE_CHANGE_SUCCESS));

	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(_, GST_STATE_PAUSED))
		.WillOnce(Return(GST_STATE_CHANGE_FAILURE));

	bool result = mInterfaceGstPlayer->Pause(pause, forceStopGstreamerPreBuffering);

	EXPECT_TRUE(result);
}

TEST_F(InterfacePlayerTests, IsCacheEmpty_SourceNull)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->source = nullptr;

	bool result = mInterfaceGstPlayer->IsCacheEmpty(mediaType);

	EXPECT_TRUE(result);
}

TEST_F(InterfacePlayerTests, IsCacheEmpty_CacheLevelNotEmpty)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->source = &gst_element_pipeline;

	EXPECT_CALL(*g_mockGStreamer, gst_app_src_get_current_level_bytes(GST_APP_SRC(stream->source)))
		.WillOnce(Return(1000));

	bool result = mInterfaceGstPlayer->IsCacheEmpty(mediaType);

	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, IsCacheEmpty_CacheLevelEmpty_BufferUnderrun)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->source = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun = true;

	EXPECT_CALL(*g_mockGStreamer, gst_app_src_get_current_level_bytes(GST_APP_SRC(stream->source)))
		.WillOnce(Return(0));

	bool result = mInterfaceGstPlayer->IsCacheEmpty(mediaType);

	EXPECT_TRUE(result);
}

TEST_F(InterfacePlayerTests, IsCacheEmpty_CacheLevelEmpty_PTSChanged)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->source = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun = false;
	mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].bufferUnderrun = false;

	EXPECT_CALL(*g_mockGStreamer, gst_app_src_get_current_level_bytes(GST_APP_SRC(stream->source)))
		.WillOnce(Return(0));

	bool result = mInterfaceGstPlayer->IsCacheEmpty(mediaType);

	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, ResetEOSSignalledFlag)
{
	mPlayerContext->eosSignalled = true;
	mInterfaceGstPlayer->ResetEOSSignalledFlag();
	EXPECT_FALSE(mPlayerContext->eosSignalled);
}

TEST_F(InterfacePlayerTests, PipelineConfiguredForMedia_True)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->sourceConfigured = true;

	bool result = mInterfaceGstPlayer->PipelineConfiguredForMedia(mediaType);

	EXPECT_TRUE(result);
}

TEST_F(InterfacePlayerTests, PipelineConfiguredForMedia_False)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->sourceConfigured = false;

	bool result = mInterfaceGstPlayer->PipelineConfiguredForMedia(mediaType);

	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, GetBufferControlData_PausedState)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	GstElement sinkbin = {};
	stream->sinkbin = &sinkbin;
	mPlayerContext->paused = false;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&sinkbin, NotNull(), NotNull(), _))
		.WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PAUSED), SetArgPointee<2>(GST_STATE_NULL), Return(GST_STATE_CHANGE_SUCCESS)));

	bool result = mInterfaceGstPlayer->GetBufferControlData(mediaType);

	EXPECT_TRUE(result);

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&sinkbin, NotNull(), NotNull(), _))
		.WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PLAYING), SetArgPointee<2>(GST_STATE_NULL), Return(GST_STATE_CHANGE_SUCCESS)));

	result = mInterfaceGstPlayer->GetBufferControlData(mediaType);

	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, IsStreamReadyTest)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	GstElement sinkbin = {};
	stream->sinkbin = &sinkbin;
	stream->sourceConfigured = true;

	EXPECT_TRUE(mInterfaceGstPlayer->IsStreamReady(mediaType));

	stream->sinkbin = nullptr;
	stream->sourceConfigured = true;
	EXPECT_FALSE(mInterfaceGstPlayer->IsStreamReady(mediaType));

	sinkbin = {};
	stream->sinkbin = &sinkbin;
	stream->sourceConfigured = false;
	EXPECT_FALSE(mInterfaceGstPlayer->IsStreamReady(mediaType));


	stream->sinkbin = nullptr;
	stream->sourceConfigured = false;
	EXPECT_FALSE(mInterfaceGstPlayer->IsStreamReady(mediaType));
}

TEST_F(InterfacePlayerTests, SignalTrickModeDiscontinuity_TrickmodeSuccess)
{
	gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_VIDEO];
	stream->source = &gst_element_pipeline;
	mPlayerContext->rate = 2; // != NORMAL_PLAY_RATE
	mPlayerConfigParams->vodTrickModeFPS = 30;

	GstPad gst_pad = {};
	GstEvent gst_event = {};
	GstStructure gst_structure = {};

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(_, StrEq("src")))
		.WillOnce(Return(&gst_pad));
	EXPECT_CALL(*g_mockGStreamer, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM,_))
		.WillOnce(Return(&gst_event));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(&gst_pad,_))
		.WillOnce(Return(TRUE)); // Success case
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&gst_pad))
		.Times(1);

	mInterfaceGstPlayer->SignalTrickModeDiscontinuity();
}

TEST_F(InterfacePlayerTests, SignalTrickModeDiscontinuity_TrickmodeFail)
{
	gst_media_stream* stream = &mPlayerContext->stream[eGST_MEDIATYPE_VIDEO];
	stream->source = &gst_element_pipeline;
	mPlayerContext->rate = 2; // != NORMAL_PLAY_RATE
	mPlayerConfigParams->vodTrickModeFPS = 30;

	GstPad gst_pad = {};
	GstEvent gst_event = {};
	GstStructure gst_structure = {};

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(_, StrEq("src")))
		.WillOnce(Return(&gst_pad));
	EXPECT_CALL(*g_mockGStreamer, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM,_))
		.WillOnce(Return(&gst_event));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(&gst_pad,_))
		.WillOnce(Return(false)); // Failure case
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&gst_pad))
		.Times(1);

	mInterfaceGstPlayer->SignalTrickModeDiscontinuity();
}

TEST_F(InterfacePlayerTests, EnableGstDebugLogging)
{
	std::string debugLevel = "";
	EXPECT_CALL(*g_mockGStreamer, gst_debug_set_threshold_from_string(_, _)).Times(0);
	mInterfaceGstPlayer->EnableGstDebugLogging(debugLevel);

	debugLevel = "GST_LEVEL_DEBUG";
	EXPECT_CALL(*g_mockGStreamer, gst_debug_set_threshold_from_string(StrEq(debugLevel.c_str()), 1)).Times(1);
	mInterfaceGstPlayer->EnableGstDebugLogging(debugLevel);
}

TEST_F(InterfacePlayerTests, CheckDiscontinuity_FirstBufferNotProcessed)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	int streamFormat = GST_FORMAT_ISO_BMFF;
	bool codecChange = false;
	bool unblockDiscProcess = false;
	bool shouldHaltBuffering = false;

	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->firstBufferProcessed = false;

	bool result = mInterfaceGstPlayer->CheckDiscontinuity(mediaType, streamFormat, codecChange, unblockDiscProcess, shouldHaltBuffering);

	EXPECT_FALSE(result);
	EXPECT_FALSE(unblockDiscProcess);
	EXPECT_FALSE(shouldHaltBuffering);
}

TEST_F(InterfacePlayerTests, CheckDiscontinuity_AudioDiscontinuity)
{
	int mediaType = eGST_MEDIATYPE_AUDIO;
	int streamFormat = GST_FORMAT_ISO_BMFF;
	bool codecChange = true;
	bool unblockDiscProcess = false;
	bool shouldHaltBuffering = false;

	mPlayerConfigParams->enablePTSReStamp = true;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->firstBufferProcessed = true;

	gst_media_stream* subtitleStream = &mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE];
	subtitleStream->source = &gst_element_pipeline;


	bool result = mInterfaceGstPlayer->CheckDiscontinuity(mediaType, streamFormat, codecChange, unblockDiscProcess, shouldHaltBuffering);

	EXPECT_TRUE(result);
	EXPECT_FALSE(unblockDiscProcess);
	EXPECT_TRUE(shouldHaltBuffering);
}

TEST_F(InterfacePlayerTests, CheckDiscontinuity_EnablePTSReStamp_NoCodecChange)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	int streamFormat = GST_FORMAT_ISO_BMFF;
	bool codecChange = false;
	bool unblockDiscProcess = false;
	bool shouldHaltBuffering = false;

	mPlayerConfigParams->enablePTSReStamp = true;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_ISO_BMFF;
	stream->firstBufferProcessed = true;

	bool result = mInterfaceGstPlayer->CheckDiscontinuity(mediaType, streamFormat, codecChange, unblockDiscProcess, shouldHaltBuffering);

	EXPECT_TRUE(result);
	EXPECT_TRUE(unblockDiscProcess);
	EXPECT_FALSE(shouldHaltBuffering);
}

TEST_F(InterfacePlayerTests, TimerAdd_ValidFunctionAndUserData)
{
	GSourceFunc funcPtr = [](gpointer data) -> gboolean { return G_SOURCE_CONTINUE; };
	int repeatTimeout = 1000;
	guint taskId = 0;
	gpointer user_data = reinterpret_cast<gpointer>(0x1234);
	const char* timerName = "TestTimer";

	EXPECT_CALL(*g_mockGLib, g_timeout_add(repeatTimeout, funcPtr, user_data))
		.WillOnce(Return(1));

	mInterfaceGstPlayer->TimerAdd(funcPtr, repeatTimeout, taskId, user_data, timerName);

	EXPECT_EQ(taskId, 1);
}

TEST_F(InterfacePlayerTests, TimerAdd_TaskAlreadyAdded)
{
	GSourceFunc funcPtr = [](gpointer data) -> gboolean { return G_SOURCE_CONTINUE; };
	int repeatTimeout = 1000;
	guint taskId = 1; // Task already added
	gpointer user_data = reinterpret_cast<gpointer>(0x1234);
	const char* timerName = "TestTimer";

	mInterfaceGstPlayer->TimerAdd(funcPtr, repeatTimeout, taskId, user_data, timerName);

	EXPECT_EQ(taskId, 1);
}

TEST_F(InterfacePlayerTests, TimerAdd_InvalidFunctionPointer)
{
	GSourceFunc funcPtr = nullptr; // Invalid function pointer
	int repeatTimeout = 1000;
	guint taskId = 0;
	gpointer user_data = reinterpret_cast<gpointer>(0x1234);
	const char* timerName = "TestTimer";

	mInterfaceGstPlayer->TimerAdd(funcPtr, repeatTimeout, taskId, user_data, timerName);

	EXPECT_EQ(taskId, 0);
}

TEST_F(InterfacePlayerTests, TimerAdd_InvalidUserData)
{
	GSourceFunc funcPtr = [](gpointer data) -> gboolean { return G_SOURCE_CONTINUE; };
	int repeatTimeout = 1000;
	guint taskId = 0;
	gpointer user_data = nullptr; // Invalid user data
	const char* timerName = "TestTimer";

	mInterfaceGstPlayer->TimerAdd(funcPtr, repeatTimeout, taskId, user_data, timerName);

	EXPECT_EQ(taskId, 0);
}

TEST_F(InterfacePlayerTests, TimerIsRunning_TaskRunning)
{
	guint taskId = 1; // Task is running
	bool result = mInterfaceGstPlayer->TimerIsRunning(taskId);
	EXPECT_TRUE(result);

	taskId = PLAYER_TASK_ID_INVALID; // Task is not running
	result = mInterfaceGstPlayer->TimerIsRunning(taskId);
	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, IdleTaskClearFlags_Success)
{
	GstTaskControlData taskDetails("TestTask");
	taskDetails.taskID = 1;
	taskDetails.taskIsPending = true;

	mInterfaceGstPlayer->IdleTaskClearFlags(taskDetails);

	EXPECT_FALSE(taskDetails.taskIsPending);
	EXPECT_EQ(taskDetails.taskID, 0);
}

TEST_F(InterfacePlayerTests, IdleTaskClearFlags_Fail)
{
	GstTaskControlData taskDetails("TestTask2");
	taskDetails.taskID = 0;
	taskDetails.taskIsPending = true;

	mInterfaceGstPlayer->IdleTaskClearFlags(taskDetails);

	EXPECT_FALSE(taskDetails.taskIsPending);
	EXPECT_EQ(taskDetails.taskID, 0);
}

TEST_F(InterfacePlayerTests, IdleTaskAdd_TaskNotPending)
{

	GstTaskControlData taskDetails("TestTask");
	BackgroundTask funcPtr = [](void* data) -> int { return 0;};

	EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(_))
				.WillOnce(Return(1));


	bool result = mInterfaceGstPlayer->IdleTaskAdd(taskDetails, funcPtr);

	EXPECT_TRUE(result);
	EXPECT_TRUE(taskDetails.taskIsPending);
	EXPECT_NE(taskDetails.taskID, 0);
}

TEST_F(InterfacePlayerTests, IdleTaskAdd_TaskAlreadyPending)
{
	GstTaskControlData taskDetails("TestTask");
	taskDetails.taskID = 1;
	taskDetails.taskIsPending = true;
	BackgroundTask funcPtr = [](void* data) -> int { return 0;};

	bool result = mInterfaceGstPlayer->IdleTaskAdd(taskDetails, funcPtr);

	EXPECT_FALSE(result);
	EXPECT_TRUE(taskDetails.taskIsPending);
	EXPECT_EQ(taskDetails.taskID, 1);
}

TEST_F(InterfacePlayerTests, IdleTaskAdd_TaskFailedToAdd)
{
	GstTaskControlData taskDetails("TestTask");
	BackgroundTask funcPtr = [](void* data) -> int { return 0;};

	// Simulate failure to add task
	EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(_))
				.WillOnce(Return(0));

	bool result = mInterfaceGstPlayer->IdleTaskAdd(taskDetails, funcPtr);

	EXPECT_FALSE(result);
	EXPECT_FALSE(taskDetails.taskIsPending);
	EXPECT_EQ(taskDetails.taskID, 0);
}

TEST_F(InterfacePlayerTests, FirstFrameCallback_SetAndInvoke)
{
    bool callbackInvoked = false;
    GstMediaType expectedMediaType = eGST_MEDIATYPE_VIDEO;
    bool notifyFirstBuffer = false;
    bool decoderHandleNotified = false;
    bool requireFirstVideoFrameDisplay = false;
    bool audioOnly = false;

    // Set the callback using lambda
	mInterfaceGstPlayer->FirstFrameCallback(
		[&](int mediaType, bool notifyBuffer, bool decoderNotified, bool& requireFirstVideoFrame, bool& audio) {
			callbackInvoked = true;
			EXPECT_EQ(mediaType, static_cast<int>(expectedMediaType));
			EXPECT_EQ(notifyBuffer, notifyFirstBuffer);
			EXPECT_EQ(decoderNotified, decoderHandleNotified);

			requireFirstVideoFrame = true;
			audio = true;
		});
		
    // Invoke the callback
    mInterfaceGstPlayer->notifyFirstFrameCallback(expectedMediaType, notifyFirstBuffer, decoderHandleNotified,
                                                  requireFirstVideoFrameDisplay, audioOnly);

    // Validate callback was invoked and reference outputs are updated
    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(requireFirstVideoFrameDisplay);
    EXPECT_TRUE(audioOnly);
}

TEST_F(InterfacePlayerTests, StopCallback_SetAndInvoke)
{
	bool callbackInvoked = false;
	bool status = true;

	mInterfaceGstPlayer->StopCallback([&](bool stat) {
		callbackInvoked = true;
		EXPECT_EQ(stat, status);
	});

	mInterfaceGstPlayer->stopCallback(status);

	EXPECT_TRUE(callbackInvoked);
}

TEST_F(InterfacePlayerTests, TearDownCallback_SetAndInvoke)
{
	bool callbackInvoked = false;
	bool status = true;
	int mediaType = eGST_MEDIATYPE_VIDEO;

	mInterfaceGstPlayer->TearDownCallback([&](bool stat, int type) {
		callbackInvoked = true;
		EXPECT_EQ(stat, status);
		EXPECT_EQ(type, mediaType);
	});

	mInterfaceGstPlayer->tearDownCb(status, mediaType);

	EXPECT_TRUE(callbackInvoked);
}

TEST_F(InterfacePlayerTests, NotifyFirstFrame_VideoType_FirstFrameNotReceived)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	mPlayerContext->firstFrameReceived = false;
	mPlayerContext->firstVideoFrameReceived = true;
	mPlayerContext->NumberOfTracks = 1;
	mPlayerContext->firstAudioFrameReceived = false;
	mPlayerContext->decoderHandleNotified = false;
	mInterfaceGstPlayer->PipelineSetToReady = true;

	bool callbackInvoked = false;
	mInterfaceGstPlayer->FirstFrameCallback([&](int type, bool notifyBuffer, bool decoderNotified, bool& requireFirstVideoFrame, bool& audio) {
		callbackInvoked = true;
		EXPECT_EQ(type, eGST_MEDIATYPE_VIDEO);
		EXPECT_TRUE(notifyBuffer);
		EXPECT_TRUE(decoderNotified);
		requireFirstVideoFrame = true;
		audio = false;
	});

	EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(_)).Times(3)
		.WillOnce(Return(1)).WillOnce(Return(0)).WillOnce(Return(0));

	mInterfaceGstPlayer->NotifyFirstFrame(mediaType);

	EXPECT_TRUE(callbackInvoked);
	EXPECT_TRUE(mPlayerContext->firstFrameReceived);
	EXPECT_TRUE(mPlayerContext->decoderHandleNotified);
	EXPECT_TRUE(mPlayerContext->firstFrameCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->firstFrameCallbackIdleTaskId, 1);
	EXPECT_FALSE(mInterfaceGstPlayer->PipelineSetToReady);
}

TEST_F(InterfacePlayerTests, NotifyFirstFrame_AudioType_FirstFrameNotReceived)
{
	int mediaType = eGST_MEDIATYPE_AUDIO;
	mPlayerContext->firstFrameReceived = false;
	mPlayerContext->firstAudioFrameReceived = true;
	mPlayerContext->NumberOfTracks = 1;
	mPlayerContext->firstVideoFrameReceived = false;
	mPlayerContext->decoderHandleNotified = false;
	mInterfaceGstPlayer->PipelineSetToReady = true;

	bool callbackInvoked = false;
	mInterfaceGstPlayer->FirstFrameCallback([&](int type, bool notifyBuffer, bool decoderNotified, bool& requireFirstVideoFrame, bool& audio) {
		callbackInvoked = true;
		EXPECT_EQ(type, eGST_MEDIATYPE_AUDIO);
		EXPECT_TRUE(notifyBuffer);
		EXPECT_TRUE(decoderNotified);
		requireFirstVideoFrame = false;
		audio = true;
	});

	EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(_))
		.WillRepeatedly(Return(1));

	mInterfaceGstPlayer->NotifyFirstFrame(mediaType);

	EXPECT_TRUE(callbackInvoked);
	EXPECT_TRUE(mPlayerContext->firstFrameReceived);
	EXPECT_TRUE(mPlayerContext->decoderHandleNotified);
	EXPECT_TRUE(mPlayerContext->firstFrameCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->firstFrameCallbackIdleTaskId, 1);
}

TEST_F(InterfacePlayerTests, TriggerEvent_CallbackExists)
{
	InterfaceCB event = InterfaceCB::idleCb;
	bool callbackInvoked = false;

	mInterfaceGstPlayer->callbackMap[event] = [&]() {
		callbackInvoked = true;
	};

	mInterfaceGstPlayer->TriggerEvent(event);

	EXPECT_TRUE(callbackInvoked);
}

TEST_F(InterfacePlayerTests, TriggerEvent_CallbackDoesNotExist)
{
	InterfaceCB event = InterfaceCB::idleCb;
	mInterfaceGstPlayer->TriggerEvent(event);
}

TEST_F(InterfacePlayerTests, TriggerEventWithData_CallbackExists)
{
	InterfaceCB event = InterfaceCB::idleCb;
	int expectedData = 42;
	bool callbackInvoked = false;

	mInterfaceGstPlayer->setupStreamCallbackMap[event] = [&](int data) {
		callbackInvoked = true;
		EXPECT_EQ(data, expectedData);
	};
	mInterfaceGstPlayer->TriggerEvent(event, expectedData);

	EXPECT_TRUE(callbackInvoked);
}

TEST_F(InterfacePlayerTests, TriggerEventWithData_CallbackDoesNotExist)
{
	InterfaceCB event = InterfaceCB::idleCb;
	int data = 42;
	mInterfaceGstPlayer->TriggerEvent(event, data);
}

extern bool gst_StartsWith( const char *inputStr, const char *prefix );
TEST_F(InterfacePlayerTests, StartsWithTest)
{
	EXPECT_TRUE(gst_StartsWith("TestString", "Test"));
}

#define DEFAULT_BUFFERING_MAX_CNT (100) 
TEST_F(InterfacePlayerTests, CreatePipeline_Success)
{
	const char* pipelineName = "testPipeline";
	int pipelinePriority = 1;

	//force destroy old pipeline
	GstElement gst_element_pipeline2 = {.object = {.name = (gchar *)pipelineName}};
	mPlayerContext->pipeline = &gst_element_pipeline2;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)pipelineName}};
	GstBus gst_element_bus = {.object = {.name = (gchar *)"testbus"}};
	GstTaskPool gst_element_task_pool = {.object = {.name = (gchar *)"testtaskpool"}};
	GstQuery* gst_element_query = new GstQuery();

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq(pipelineName)))
		.WillOnce(Return(&gst_element_pipeline));
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(GST_PIPELINE(&gst_element_pipeline)))
		.WillOnce(Return(&gst_element_bus));
	mPlayerContext->task_pool = &gst_element_task_pool;
	EXPECT_CALL(*g_mockGStreamer, gst_bus_add_watch(&gst_element_bus, _, _))
		.WillOnce(Return(1));
	EXPECT_CALL(*g_mockGStreamer, gst_bus_set_sync_handler(&gst_element_bus, _, _, nullptr));
	EXPECT_CALL(*g_mockGStreamer, gst_query_new_position(GST_FORMAT_TIME))
		.WillOnce(Return(gst_element_query));

	bool result = mInterfaceGstPlayer->CreatePipeline(pipelineName, pipelinePriority);

	EXPECT_TRUE(result);
	EXPECT_EQ(mPlayerContext->pipeline, &gst_element_pipeline);
	EXPECT_EQ(mPlayerContext->bus, &gst_element_bus);;
	EXPECT_EQ(mPlayerContext->positionQuery, gst_element_query);
	EXPECT_EQ(mPlayerContext->buffering_enabled, mPlayerConfigParams->gstreamerBufferingBeforePlay);
	EXPECT_FALSE(mPlayerContext->buffering_in_progress);
	EXPECT_EQ(mPlayerContext->buffering_timeout_cnt, DEFAULT_BUFFERING_MAX_CNT);
	EXPECT_EQ(mPlayerContext->buffering_target_state, GST_STATE_NULL);
	EXPECT_EQ(mPlayerContext->enableSEITimeCode, mPlayerConfigParams->seiTimeCode);
}

TEST_F(InterfacePlayerTests, CreatePipeline_Failure_GetBusFailed)
{
	const char* pipelineName = "testPipeline";
	int pipelinePriority = 1;

	GstElement gst_element_pipeline = {.object = {.name = (gchar *)pipelineName}};

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq(pipelineName)))
		.WillOnce(Return(&gst_element_pipeline));
	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_get_bus(GST_PIPELINE(&gst_element_pipeline)))
		.WillOnce(Return(nullptr));

	bool result = mInterfaceGstPlayer->CreatePipeline(pipelineName, pipelinePriority);

	EXPECT_FALSE(result);
	EXPECT_EQ(mPlayerContext->pipeline, &gst_element_pipeline);
	EXPECT_EQ(mPlayerContext->bus, nullptr);
}

TEST_F(InterfacePlayerTests, CreatePipeline_Failure_PipelineNewFailed)
{
	const char* pipelineName = "testPipeline";
	int pipelinePriority = 1;

	EXPECT_CALL(*g_mockGStreamer, gst_pipeline_new(StrEq(pipelineName)))
		.WillOnce(Return(nullptr));

	bool result = mInterfaceGstPlayer->CreatePipeline(pipelineName, pipelinePriority);

	EXPECT_FALSE(result);
	EXPECT_EQ(mPlayerContext->pipeline, nullptr);
	EXPECT_EQ(mPlayerContext->bus, nullptr);
}

TEST_F(InterfacePlayerTests, SetAudioVolume)
{
	// Test with volume 0
	mInterfaceGstPlayer->SetAudioVolume(0);
	EXPECT_DOUBLE_EQ(mPlayerContext->audioVolume, 0.0);
	mInterfaceGstPlayer->SetAudioVolume(150);
	EXPECT_DOUBLE_EQ(mPlayerContext->audioVolume, 1.5);
	mInterfaceGstPlayer->SetAudioVolume(-50);
	EXPECT_DOUBLE_EQ(mPlayerContext->audioVolume, -0.5); //expected?
}

TEST_F(InterfacePlayerTests, SetVolumeOrMuteUnMute_Null)
{
	mInterfaceGstPlayer->SetVolumeOrMuteUnMute();
}

TEST_F(InterfacePlayerTests, SetVideoZoomTest)
{
	GstElement gst_element_video_sink = {.object = {.name = (gchar *)"video_sink"}};
	mPlayerContext->video_sink = &gst_element_video_sink;
	mPlayerContext->usingRialtoSink = false;

	EXPECT_CALL(*g_mockGLib, g_object_set(&gst_element_video_sink, StrEq("zoom-mode"), Matcher<int>(2))).Times(AnyNumber());

	mInterfaceGstPlayer->SetVideoZoom(2);
	EXPECT_EQ(mPlayerContext->zoom, static_cast<GstVideoZoomMode>(2));

	mPlayerContext->usingRialtoSink = true;
	mInterfaceGstPlayer->SetVideoZoom(2);
}

TEST_F(InterfacePlayerTests, SetVideoMute_ValidVideoSink)
{
	GstElement gst_element_video_sink = {.object = {.name = (gchar *)"video_sink"}};
	mPlayerContext->video_sink = &gst_element_video_sink;
	EXPECT_CALL(*g_mockGLib, g_object_set(&gst_element_video_sink, StrEq("show-video-window"), Matcher<int>(0)));
	mInterfaceGstPlayer->SetVideoMute(true);
	EXPECT_TRUE(mPlayerContext->videoMuted);
}

TEST_F(InterfacePlayerTests, SetVideoMute_InvalidVideoSink)
{
	mPlayerContext->video_sink = nullptr;
	EXPECT_CALL(*g_mockGLib, g_object_set(_, _,Matcher<int>(_))).Times(0);
	mInterfaceGstPlayer->SetVideoMute(true);
	EXPECT_TRUE(mPlayerContext->videoMuted);
}

TEST_F(InterfacePlayerTests, SetTextStyle_Null)
{
	mPlayerContext->subtitle_sink = nullptr;

	std::string options = "ffffff";
	EXPECT_FALSE(mInterfaceGstPlayer->SetTextStyle(options));
}

TEST_F(InterfacePlayerTests, NotifyEOS_TaskSchedulingFailed)
{
	// Initial setup
	mPlayerContext->eosSignalled = false;
	mPlayerContext->eosCallbackIdleTaskPending = false;
	mPlayerContext->eosCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;

	// Mock the scheduler to return an invalid task ID
	EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(_))
		.WillOnce(Return(PLAYER_TASK_ID_INVALID));

	// Call the method
	mInterfaceGstPlayer->NotifyEOS();

	// Verify the expected outcomes
	EXPECT_TRUE(mPlayerContext->eosSignalled);
	EXPECT_FALSE(mPlayerContext->eosCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskId, PLAYER_TASK_ID_INVALID);
}

TEST_F(InterfacePlayerTests, NotifyEOS_FirstCall)
{
	mPlayerContext->eosSignalled = false;
	mPlayerContext->eosCallbackIdleTaskPending = false;
	mPlayerContext->eosCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;

	// Mock the scheduler to return a valid task ID
	EXPECT_CALL(*g_mockPlayerScheduler, ScheduleTask(_))
		.WillOnce(Return(1234));

	mInterfaceGstPlayer->NotifyEOS();

	EXPECT_TRUE(mPlayerContext->eosSignalled);
	EXPECT_TRUE(mPlayerContext->eosCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskId, 1234);
}

TEST_F(InterfacePlayerTests, NotifyEOS_TaskAlreadyPending)
{
	mPlayerContext->eosSignalled = false;
	mPlayerContext->eosCallbackIdleTaskPending = true;
	mPlayerContext->eosCallbackIdleTaskId = 1234;

	mInterfaceGstPlayer->NotifyEOS();

	EXPECT_FALSE(mPlayerContext->eosSignalled);
	EXPECT_TRUE(mPlayerContext->eosCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskId, 1234);
}

TEST_F(InterfacePlayerTests, NotifyEOS_AlreadySignalled)
{
	mPlayerContext->eosSignalled = true;
	mPlayerContext->eosCallbackIdleTaskPending = false;
	mPlayerContext->eosCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;

	mInterfaceGstPlayer->NotifyEOS();

	EXPECT_TRUE(mPlayerContext->eosSignalled);
	EXPECT_FALSE(mPlayerContext->eosCallbackIdleTaskPending);
	EXPECT_EQ(mPlayerContext->eosCallbackIdleTaskId, PLAYER_TASK_ID_INVALID);
}

TEST_F(InterfacePlayerTests, NotifyFragmentCachingComplete_PendingPlayStateTrue)
{
	mPlayerContext->pendingPlayState = true;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"pipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;

	// Mock the SetStateWithWarnings function to return GST_STATE_CHANGE_SUCCESS
	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PLAYING))
		.WillOnce(Return(GST_STATE_CHANGE_SUCCESS));

	mInterfaceGstPlayer->NotifyFragmentCachingComplete();
	EXPECT_FALSE(mPlayerContext->pendingPlayState);
}

TEST_F(InterfacePlayerTests, NotifyFragmentCachingComplete_SetStateFailure)
{
	mPlayerContext->pendingPlayState = true;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"pipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;

	// Mock the SetStateWithWarnings function to return GST_STATE_CHANGE_FAILURE
	EXPECT_CALL(*g_mockGStreamer, gst_element_set_state(&gst_element_pipeline, GST_STATE_PLAYING))
		.WillOnce(Return(GST_STATE_CHANGE_FAILURE));

	mInterfaceGstPlayer->NotifyFragmentCachingComplete();
	EXPECT_FALSE(mPlayerContext->pendingPlayState);
}

TEST_F(InterfacePlayerTests, NotifyFragmentCachingComplete_PendingPlayStateFalse)
{
	mPlayerContext->pendingPlayState = false;
	mInterfaceGstPlayer->NotifyFragmentCachingComplete();
	EXPECT_FALSE(mPlayerContext->pendingPlayState);
}

TEST_F(InterfacePlayerTests, EndOfStreamReached_FirstBufferProcessedFalse)
{
	bool shouldHaltBuffering = false;
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_VIDEO_ES_H264;
	stream->firstBufferProcessed = false;

	mInterfaceGstPlayer->EndOfStreamReached(mediaType, shouldHaltBuffering);

	EXPECT_TRUE(stream->eosReached);
	EXPECT_FALSE(shouldHaltBuffering);
}

TEST_F(InterfacePlayerTests, EndOfStreamReached_FirstBufferProcessedTrue_NormalPlayRate)
{
	bool shouldHaltBuffering = false;
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_VIDEO_ES_H264;
	stream->firstBufferProcessed = true;
	mPlayerContext->rate = GST_NORMAL_PLAY_RATE;
	mPlayerContext->stream[eGST_MEDIATYPE_AUDIO].eosReached = true;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = reinterpret_cast<GstElement*>(0x1234);

	mInterfaceGstPlayer->EndOfStreamReached(mediaType, shouldHaltBuffering);

	EXPECT_TRUE(stream->eosReached);
	EXPECT_TRUE(shouldHaltBuffering);
}

TEST_F(InterfacePlayerTests, EndOfStreamReached_FirstBufferProcessedTrue_TrickMode)
{
	bool shouldHaltBuffering = false;
	int mediaType = eGST_MEDIATYPE_VIDEO;
	gst_media_stream* stream = &mPlayerContext->stream[mediaType];
	stream->format = GST_FORMAT_VIDEO_ES_H264;
	stream->firstBufferProcessed = true;
	mPlayerContext->rate = 2.0;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = reinterpret_cast<GstElement*>(0x1234);

	mInterfaceGstPlayer->EndOfStreamReached(mediaType, shouldHaltBuffering);

	EXPECT_TRUE(stream->eosReached);
	EXPECT_TRUE(shouldHaltBuffering);
}

TEST_F(InterfacePlayerTests, InterfacePlayer_SetupStream_Success) //failure case todo
{
	GstMediaType streamId = eGST_MEDIATYPE_VIDEO;
	std::string manifestUrl = "http://example.com/manifest.mpd";
	int retvalue = mInterfaceGstPlayer->InterfacePlayer_SetupStream(streamId, manifestUrl);

	EXPECT_EQ(retvalue, 0);
}

TEST_F(InterfacePlayerTests, DisableDecoderHandleNotified)
{
	mPlayerContext->decoderHandleNotified = true;
	mInterfaceGstPlayer->DisableDecoderHandleNotified();
	EXPECT_FALSE(mPlayerContext->decoderHandleNotified);
}

TEST_F(InterfacePlayerTests, SignalSubtitleClock_PadPushFail)
{
	gint64 vPTS = 90000; // 1 second in 90KHz clock
	bool state = false;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"pipeline"}};
	GstElement gst_element_source = {.object = {.name = (gchar *)"source"}};
	GstPad gst_pad_src = {.object = {.name = (gchar *)"src"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].format = GST_FORMAT_SUBTITLE_MP4;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = &gst_element_source;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PLAYING), Return(GST_STATE_CHANGE_SUCCESS)));
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(&gst_element_source, StrEq("src")))
		.WillOnce(Return(&gst_pad_src));
	EXPECT_CALL(*g_mockGLib, g_type_check_instance_is_a(_,_))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockGStreamer, gst_structure_new())
		.WillOnce(Return(reinterpret_cast<GstStructure*>(0x1234)));
	EXPECT_CALL(*g_mockGStreamer, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, reinterpret_cast<GstStructure*>(0x1234)))
		.WillOnce(Return(reinterpret_cast<GstEvent*>(0x5678)));
	EXPECT_CALL(*g_mockGStreamer, gst_pad_push_event(&gst_pad_src, reinterpret_cast<GstEvent*>(0x5678)))
		.WillOnce(Return(FALSE));
	EXPECT_CALL(*g_mockGStreamer, gst_object_unref(&gst_pad_src));

	bool result = mInterfaceGstPlayer->SignalSubtitleClock(vPTS, state);
	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, SignalSubtitleClock_NonPlayingState)
{
	gint64 vPTS = 90000; // 1 second in 90KHz clock
	bool state = false;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"pipeline"}};
	GstElement gst_element_source = {.object = {.name = (gchar *)"source"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].format = GST_FORMAT_SUBTITLE_MP4;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = &gst_element_source;

	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PAUSED), Return(GST_STATE_CHANGE_SUCCESS)));
	EXPECT_CALL(*g_mockGLib, g_type_check_instance_is_a(_,_))
		.WillRepeatedly(Return(true));

	bool result = mInterfaceGstPlayer->SignalSubtitleClock(vPTS, state);
	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, SignalSubtitleClock_Null)
{
	gint64 vPTS = 90000; // 1 second in 90KHz clock
	bool state = false;
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"pipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].format = GST_FORMAT_SUBTITLE_MP4;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = nullptr;

	//subtitle appsrc is NULL
	bool result = mInterfaceGstPlayer->SignalSubtitleClock(vPTS, state);
	EXPECT_FALSE(result);
	GstElement gst_element_source = {.object = {.name = (gchar *)"source"}};
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = &gst_element_source;
	result = mInterfaceGstPlayer->SignalSubtitleClock(vPTS, state);
	EXPECT_FALSE(result);

	//subtitle appsrc is invalid
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].format = GST_FORMAT_SUBTITLE_MP4;
	mPlayerContext->stream[eGST_MEDIATYPE_SUBTITLE].source = &gst_element_source;

	//sourceEleSrcPad is NULL
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_state(&gst_element_pipeline, _, _, _))
		.WillOnce(DoAll(SetArgPointee<1>(GST_STATE_PLAYING), Return(GST_STATE_CHANGE_SUCCESS)));
	EXPECT_CALL(*g_mockGLib, g_type_check_instance_is_a(_,_))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockGStreamer, gst_element_get_static_pad(&gst_element_source, StrEq("src")))
		.WillOnce(Return(nullptr));
	result = mInterfaceGstPlayer->SignalSubtitleClock(vPTS, state);
	EXPECT_FALSE(result);

}

TEST_F(InterfacePlayerTests, FlushTrack_VideoType)
{
	int mediaType = eGST_MEDIATYPE_VIDEO;
	double pos = 10.0;
	double audioDelta = 0.0;
	double subDelta = 3.0;
	GstElement gst_element_source = {.object = {.name = (gchar *)"source"}};
	GstElement gst_element_pipeline = {.object = {.name = (gchar *)"pipeline"}};
	mPlayerContext->pipeline = &gst_element_pipeline;
	mPlayerContext->stream[eGST_MEDIATYPE_VIDEO].source = &gst_element_source;

	EXPECT_CALL(*g_mockGStreamer, gst_element_seek_simple(&gst_element_source, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, (pos + subDelta) * GST_SECOND))
		.WillOnce(Return(TRUE));

	double rate = mInterfaceGstPlayer->FlushTrack(mediaType, pos, audioDelta, subDelta);

	EXPECT_EQ(rate, GST_NORMAL_PLAY_RATE);
	EXPECT_FALSE(mPlayerContext->filterAudioDemuxBuffers);
}

extern bool GstPlayer_isVideoOrAudioDecoder(const char* name, InterfacePlayerRDK * _this);
TEST_F(InterfacePlayerTests, GstPlayer_isVideoOrAudioDecoder_RialtoSink)
{
	mPlayerContext->usingRialtoSink = true;	
	EXPECT_TRUE( GstPlayer_isVideoOrAudioDecoder("rialtomsevideosink", mInterfaceGstPlayer) );
	EXPECT_TRUE( GstPlayer_isVideoOrAudioDecoder("rialtomseaudiosink", mInterfaceGstPlayer) );
	EXPECT_FALSE( GstPlayer_isVideoOrAudioDecoder("rialtomsesubtitlesink", mInterfaceGstPlayer) );
}

TEST_F(InterfacePlayerTests, GstPlayer_isVideoOrAudioDecoder_NotDecoder)
{
	const char* name = "notadecoder";
	bool result = GstPlayer_isVideoOrAudioDecoder(name, mInterfaceGstPlayer);
	EXPECT_FALSE(result);
}

TEST_F(InterfacePlayerTests, SetVolumeOrMuteUnMute_UsingRialtoSink)
{
	mPlayerContext->usingRialtoSink = true;
	GstElement gst_element_audio_sink = {.object = {.name = (gchar *)"audio_sink"}};
	mPlayerContext->audio_sink = &gst_element_audio_sink;
	mPlayerContext->audioVolume = 0.5;
	mPlayerContext->audioMuted = false;

	EXPECT_CALL(*g_mockGLib, g_object_set(&gst_element_audio_sink, StrEq("volume"), Matcher<double>(0.5)));

	mInterfaceGstPlayer->SetVolumeOrMuteUnMute();
}
