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

#include <iostream>
#include "InterfacePlayerRDK.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "PlayerLogManager.h"
#include <sys/time.h>
#include "GstUtils.h"
#include "PlayerExternalsInterface.h"						//ToDo: Replace once outputprotection moved to middleware
#include <inttypes.h>
#include "TextStyleAttributes.h"
#include <memory>

#ifdef USE_EXTERNAL_STATS
// narrowly define MediaType for backwards compatibility
#define MediaType GstMediaType
#include "aamp-xternal-stats.h"
#undef MediaType
#endif
#include "PlayerUtils.h"

#define DEFAULT_BUFFERING_TO_MS 10                       /**< TimeOut interval to check buffer fullness */
#define DEFAULT_BUFFERING_MAX_MS (1000)                  /**< max buffering time */
#define DEFAULT_BUFFERING_MAX_CNT (DEFAULT_BUFFERING_MAX_MS/DEFAULT_BUFFERING_TO_MS)   /**< max buffering timeout count */
#define NORMAL_PLAY_RATE 1
#define DEFAULT_TIMEOUT_FOR_SOURCE_SETUP (1000)          /**< Default timeout value in milliseconds */
#define DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS 12         /**< Currently MAX FRAG DURATION + 2*/
#define INVALID_RATE -9999


#if GLIB_CHECK_VERSION(2, 68, 0)
// avoid deprecated g_memdup when g_memdup2 available
#define PLAYER_G_MEMDUP(src, size) g_memdup2((src), (gsize)(size))
#else
#define PLAYER_G_MEMDUP(src, size) g_memdup((src), (guint)(size))
#endif
#define GST_DELAY_BETWEEN_PTS_CHECK_FOR_EOS_ON_UNDERFLOW 500    /**< A timeout interval in milliseconds to check pts in case of underflow */
#define GST_MIN_DECODE_ERROR_INTERVAL 10000                     /**< Minimum time interval in milliseconds between two decoder error CB to send anomaly error */
#define BUFFERING_TIMEOUT_PRIORITY -70                           /**< 0 is DEFAULT priority whereas -100 is the HIGH_PRIORITY */


// for now name is being kept as aamp should be changed when gst-plugins are migrated
static const char* GstPluginNamePR = "aampplayreadydecryptor";
static const char* GstPluginNameWV = "aampwidevinedecryptor";
static const char* GstPluginNameCK = "aampclearkeydecryptor";
static const char* GstPluginNameVMX = "aampverimatrixdecryptor";
#define GST_MIN_PTS_UPDATE_INTERVAL 4000                        /**< Time duration in milliseconds if exceeded and pts has not changed; it is concluded pts is not changing */

#include <assert.h>
#define NO_PLAYBIN 1
#define GST_NORMAL_PLAY_RATE		1

/*InterfacePlayerRDK constructor*/
InterfacePlayerRDK::InterfacePlayerRDK() : mPlayerName(),
mProtectionLock(), mPauseInjector(false), mSourceSetupMutex(), stopCallback(NULL), tearDownCb(NULL), notifyFirstFrameCallback(NULL),
mSourceSetupCV(), mScheduler(), callbackMap(), setupStreamCallbackMap(), mDrmSystem(NULL), mEncrypt(NULL)
{
	gstPrivateContext = new GstPlayerPriv();
	m_gstConfigParam = new Configs();
	m_gstConfigParam->framesToQueue = SocUtils::RequiredQueuedFrames();
	pthread_mutex_init(&mProtectionLock, NULL);
	for (int i = 0; i < GST_TRACK_COUNT; i++)
		pthread_mutex_init(&gstPrivateContext->stream[i].sourceLock, NULL);
	// start Scheduler Worker for task handling
	mScheduler.StartScheduler();
	socInterface = SocInterface::CreateSocInterface();
}

/* InterfacePlayerRDK destructor*/
InterfacePlayerRDK::~InterfacePlayerRDK()
{
	DestroyPipeline();
	if (mDrmSystem)
	{
		delete[] mDrmSystem;
	}
	mScheduler.StopScheduler();
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		pthread_mutex_destroy(&gstPrivateContext->stream[i].sourceLock);
	}
	delete gstPrivateContext;
	gstPrivateContext = nullptr;
	socInterface = nullptr;
	pthread_mutex_destroy(&mProtectionLock);
}

/*GstPlayerPriv constructor*/
GstPlayerPriv::GstPlayerPriv() : monitorAVstate(), pipeline(NULL), bus(NULL),
total_bytes(0), n_audio(0), current_audio(0),
periodicProgressCallbackIdleTaskId(GST_TASK_ID_INVALID),
bufferingTimeoutTimerId(GST_TASK_ID_INVALID), video_dec(NULL), audio_dec(NULL), TaskControlMutex(), firstProgressCallbackIdleTask("FirstProgressCallback"),
video_sink(NULL), audio_sink(NULL), subtitle_sink(NULL), task_pool(NULL),
rate(GST_NORMAL_PLAY_RATE), zoom(GST_VIDEO_ZOOM_NONE), videoMuted(false), audioMuted(false), volumeMuteMutex(), subtitleMuted(false),
audioVolume(1.0), eosCallbackIdleTaskId(GST_TASK_ID_INVALID), eosCallbackIdleTaskPending(false),
firstFrameReceived(false), pendingPlayState(false), decoderHandleNotified(false),
firstFrameCallbackIdleTaskId(GST_TASK_ID_INVALID), firstFrameCallbackIdleTaskPending(false),
using_westerossink(false), usingRialtoSink(false), pauseOnStartPlayback(false), eosSignalled(false),
buffering_enabled(FALSE), buffering_in_progress(FALSE), buffering_timeout_cnt(0),
buffering_target_state(GST_STATE_NULL),
lastKnownPTS(0), ptsUpdatedTimeMS(0), ptsCheckForEosOnUnderflowIdleTaskId(GST_TASK_ID_INVALID),
numberOfVideoBuffersSent(0), segmentStart(0), positionQuery(NULL), durationQuery(NULL),
paused(false), pipelineState(GST_STATE_NULL),
firstVideoFrameDisplayedCallbackTask("FirstVideoFrameDisplayedCallback"),
firstTuneWithWesterosSinkOff(false),
decodeErrorMsgTimeMS(0), decodeErrorCBCount(0),
progressiveBufferingEnabled(false), progressiveBufferingStatus(false), forwardAudioBuffers(false),
enableSEITimeCode(true), firstVideoFrameReceived(false), firstAudioFrameReceived(false), NumberOfTracks(0), playbackQuality{},
filterAudioDemuxBuffers(false),
aSyncControl(), syncControl(), callbackControl(), seekPosition(0)
{
	memset(videoRectangle, '\0', VIDEO_COORDINATES_SIZE);
	/* default video scaling should take into account actual graphics
	 * resolution instead of assuming 1280x720.
	 * By default we where setting the resolution has 0,0,1280,720.
	 * For Full HD this default resolution will not scale to full size.
	 * So, we no need to set any default rectangle size here,
	 * since the video will display full screen, if a gstreamer pipeline is started
	 * using the westerossink connected using westeros compositor.
	 */
	strcpy(videoRectangle, "");
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		protectionEvent[i] = NULL;
	}
}
/*GstPlayerPriv destructor*/
GstPlayerPriv::~GstPlayerPriv()
{
	g_clear_object(&pipeline);
	g_clear_object(&bus);
	g_clear_object(&video_dec);
	g_clear_object(&audio_dec);
	g_clear_object(&video_sink);
	g_clear_object(&audio_sink);
	g_clear_object(&subtitle_sink);
	g_clear_object(&task_pool);
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		g_clear_object(&protectionEvent[i]);
	}
	g_clear_object(&positionQuery);
	g_clear_object(&durationQuery);
}


/**
 * @fn gstGetMediaTypeName
 * @brief Get the name of the media type
 *
 * @param[in] mediaType The media type
 * @return The name of the media type
 */
const char *gstGetMediaTypeName(GstMediaType mediaType)
{
	static const char *name[] =
	{
		"video",//eMEDIATYPE_VIDEO
		"audio",//eMEDIATYPE_AUDIO
		"text",//eMEDIATYPE_SUBTITLE
		"aux_audio",//eMEDIATYPE_AUX_AUDIO
		"manifest",//eMEDIATYPE_MANIFEST
		"licence",//eMEDIATYPE_LICENCE
		"iframe",//eMEDIATYPE_IFRAME
		"init_video",//eMEDIATYPE_INIT_VIDEO
		"init_audio",//eMEDIATYPE_INIT_AUDIO
		"init_text",//eMEDIATYPE_INIT_SUBTITLE
		"init_aux_audio",//eMEDIATYPE_INIT_AUX_AUDIO
		"playlist_video",//eMEDIATYPE_PLAYLIST_VIDEO
		"playlist_audio",//eMEDIATYPE_PLAYLIST_AUDIO
		"playlist_text",//eMEDIATYPE_PLAYLIST_SUBTITLE
		"playlist_aux_audio",//eMEDIATYPE_PLAYLIST_AUX_AUDIO
		"playlist_iframe",//eMEDIATYPE_PLAYLIST_IFRAME
		"init_iframe",//eMEDIATYPE_INIT_IFRAME
		"dsm_cc",//eMEDIATYPE_DSM_CC
		"image",//eMEDIATYPE_IMAGE
	};
	if( mediaType < eGST_MEDIATYPE_DEFAULT )
	{
		return name[mediaType];
	}
	else
	{
		return "UNKNOWN";
	}
}


static GstStateChangeReturn SetStateWithWarnings(GstElement *element, GstState targetState);
/**
 * @brief Configures the GStreamer pipeline.
 * @param format Video format.
 * @param audioFormat Audio format.
 * @param auxFormat Auxiliary format.
 * @param subFormat Whether subtitle format is enabled.
 * @param bESChangeStatus Whether ES change status is enabled.
 * @param forwardAudioToAux Whether audio should be forwarded to the auxiliary output.
 * @param setReadyAfterPipelineCreation Whether to set the player as ready after pipeline creation.
 * @param isSubEnable Whether subtitles are enabled.
 * @param trackId Track ID.
 * @param rate Bitrate.
 * @param pipelineName Pipeline name.
 * @param PipelinePriority Pipeline priority.
 */
void InterfacePlayerRDK::ConfigurePipeline(int format, int audioFormat, int auxFormat,
										   int subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation,
										   bool isSubEnable, int32_t trackId, gint rate, const char *pipelineName, int PipelinePriority, bool FirstFrameFlag, std::string manifestUrl)
{
	mFirstFrameRequired = FirstFrameFlag;
	GstStreamOutputFormat gstFormat 	= static_cast<GstStreamOutputFormat>(format);
	GstStreamOutputFormat gstAudioFormat 	= static_cast<GstStreamOutputFormat>(audioFormat);
	GstStreamOutputFormat gstAuxFormat 	= static_cast<GstStreamOutputFormat>(auxFormat);
	GstStreamOutputFormat gstSubFormat 	= static_cast<GstStreamOutputFormat>(subFormat);

	GstStreamOutputFormat newFormat[GST_TRACK_COUNT];
	newFormat[eGST_MEDIATYPE_VIDEO] = gstFormat;
	newFormat[eGST_MEDIATYPE_AUDIO] = gstAudioFormat;

	if(isSubEnable)
	{
		MW_LOG_MIL("Gstreamer subs enabled");
		newFormat[eGST_MEDIATYPE_SUBTITLE] = gstSubFormat;
	}
	else
	{
		MW_LOG_MIL("Gstreamer subs disabled");
		newFormat[eGST_MEDIATYPE_SUBTITLE]=GST_FORMAT_INVALID;
	}

	/*Enable sending of audio data to the auxiliary output*/
	if(forwardAudioToAux)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Override auxFormat %d -> %d", auxFormat, audioFormat);
		gstPrivateContext->forwardAudioBuffers = true;
		newFormat[eGST_MEDIATYPE_AUX_AUDIO] = gstAudioFormat;
	}
	else
	{
		gstPrivateContext->forwardAudioBuffers = false;
		newFormat[eGST_MEDIATYPE_AUX_AUDIO] = gstAuxFormat;
	}

	if(!(m_gstConfigParam->useWesterosSink))
	{
		gstPrivateContext->using_westerossink = false;
		gstPrivateContext->firstTuneWithWesterosSinkOff = socInterface->IsFirstTuneWithWesteros();
	}

	else
	{
		gstPrivateContext->using_westerossink = true;
		socInterface->SetWesterosSinkState(true);
	}

	if(!(m_gstConfigParam->useRialtoSink))
	{
		gstPrivateContext->usingRialtoSink = false;
		MW_LOG_MIL("Rialto disabled");
	}
	else
	{
		gstPrivateContext->usingRialtoSink = true;
		if (gstPrivateContext->using_westerossink)
		{
			MW_LOG_WARN("Rialto and Westeros Sink enabled");
		}
		else
		{
			MW_LOG_MIL("Rialto enabled");
		}
	}

	if(rate != INVALID_RATE)
	{
		gstPrivateContext->rate = rate;
	}

	if (gstPrivateContext->pipeline == NULL || gstPrivateContext->bus == NULL)
	{
		MW_LOG_MIL("Create pipeline %s (pipeline %p bus %p)", pipelineName, gstPrivateContext->pipeline, gstPrivateContext->bus);
		CreatePipeline(pipelineName, PipelinePriority); 		/*Create a new pipeline if pipeline or the message bus does not exist*/
	}

	if(setReadyAfterPipelineCreation)
	{
		if(SetStateWithWarnings(gstPrivateContext->pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
		{
			MW_LOG_ERR("InterfacePlayerRDK_Configure GST_STATE_READY failed on forceful set");
		}
		else
		{
			MW_LOG_INFO("Forcefully set pipeline to ready state due to track_id change");
			PipelineSetToReady = true;
		}
	}

	bool configureStream[GST_TRACK_COUNT];
	bool configurationChanged = false;
	memset(configureStream, 0, sizeof(configureStream));

	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		gst_media_stream *stream = &gstPrivateContext->stream[i];
		if(stream->format != newFormat[i])
		{
			if (newFormat[i] != GST_FORMAT_INVALID)
			{
				MW_LOG_MIL("Closing stream %d old format = %d, new format = %d",i, stream->format, newFormat[i]);
				configureStream[i] = true;
				gstPrivateContext->NumberOfTracks++;
			}
			configurationChanged = true;
		}
		if(socInterface->ShouldTearDownForTrickplay())
		{
		if(gstPrivateContext->rate > 1 || gstPrivateContext->rate < 0)
		{
			if (eGST_MEDIATYPE_VIDEO == i)
				configureStream[i] = true;
			else
			{
				TearDownStream((GstMediaType)i);
				configureStream[i] = false;
			}
		}
		}
		/* Force configure the bin for mid stream audio type change */
		if (!configureStream[i] && bESChangeStatus && (eGST_MEDIATYPE_AUDIO == i))
		{
			MW_LOG_MIL("AudioType Changed. Force configure pipeline");
			configureStream[i] = true;
		}

		stream->resetPosition = true;
		stream->eosReached = false;
		stream->firstBufferProcessed = false;
	}

	/* For Rialto, teardown and rebuild the gstreamer streams if the
	 * configuration changes. This allows the "single-path-stream" property to
	 * be set correctly.
	 */
	if((gstPrivateContext->usingRialtoSink) && (configurationChanged))
	{
		MW_LOG_INFO("Teardown and rebuild the pipeline for Rialto");

		for (int i = 0; i < GST_TRACK_COUNT; i++)
		{
			gst_media_stream *stream = &gstPrivateContext->stream[i];
			if (stream->format != GST_FORMAT_INVALID)
			{
				TearDownStream((GstMediaType)i);
			}

			if(newFormat[i] != GST_FORMAT_INVALID)
			{
				configureStream[i] = true;
			}
		}
	}

	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		gst_media_stream *stream = &gstPrivateContext->stream[i];

		if ((configureStream[i] && (newFormat[i] != GST_FORMAT_INVALID)) ||
			/* Allow to create audio pipeline along with video pipeline if trickplay initiated before the pipeline going to play/paused state to fix unthrottled trickplay */
			(trickTeardown && (eGST_MEDIATYPE_AUDIO == i))) // remove the trickTeardown api not required
		{
			trickTeardown = false;
			TearDownStream((GstMediaType)i);
			stream->format = newFormat[i];
			stream->trackId = trackId;

			/* Sets up the stream for the given MediaType */
			if(0 != InterfacePlayer_SetupStream((GstMediaType)i, manifestUrl))
			{
				MW_LOG_ERR("InterfacePlayerRDK: track %d failed", i);
				//Don't kill the tune for subtitles
				if (eGST_MEDIATYPE_SUBTITLE != (GstMediaType)i)
				{
					return;
				}
			}


		}
	}
	if ((gstPrivateContext->usingRialtoSink) && (m_gstConfigParam->media != eGST_MEDIAFORMAT_PROGRESSIVE))
	{
		/* Reconfigure the Rialto video sink to update the single path stream
		 * property. This enables rialtomsevideosink to call
		 * allSourcesAttached() at the right time to enable streaming on the
		 * server side.
		 * For progressive media, we don't know what tracks are used.
		 */
		GstElement* vidsink = NULL;
		g_object_get(gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO].sinkbin, "video-sink", &vidsink, NULL);
		if(vidsink)
		{
			gboolean videoOnly = (audioFormat == GST_FORMAT_INVALID);
			MW_LOG_INFO("Setting single-path-stream to %d", videoOnly);
			g_object_set(vidsink, "single-path-stream", videoOnly, NULL);
		}
		else
		{
			MW_LOG_WARN("Couldn't get video-sink");
		}
	}
	if (gstPrivateContext->pauseOnStartPlayback && GST_NORMAL_PLAY_RATE == gstPrivateContext->rate)
	{
		MW_LOG_INFO("Setting state to GST_STATE_PAUSED - pause on playback enabled");
		gstPrivateContext->paused = true;
		gstPrivateContext->pendingPlayState = false;
		if (SetStateWithWarnings(gstPrivateContext->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
		{
			MW_LOG_ERR("InterfacePlayerRDK: GST_STATE_PAUSED failed");
		}
	}
	/* If buffering is enabled, set the pipeline in Paused state, once sufficient content has been buffered the pipeline will be set to GST_STATE_PLAYING */
	else if (gstPrivateContext->buffering_enabled && format != GST_FORMAT_INVALID && GST_NORMAL_PLAY_RATE == gstPrivateContext->rate)
	{
		MW_LOG_INFO("Setting state to GST_STATE_PAUSED, target state to GST_STATE_PLAYING");
		gstPrivateContext->buffering_target_state = GST_STATE_PLAYING;
		gstPrivateContext->buffering_in_progress = true;
		gstPrivateContext->buffering_timeout_cnt = DEFAULT_BUFFERING_MAX_CNT;

		if (SetStateWithWarnings(gstPrivateContext->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
		{
			MW_LOG_ERR("InterfacePlayerRDK_Configure GST_STATE_PAUSED failed");
		}
		gstPrivateContext->pendingPlayState = false;
		gstPrivateContext->paused = false;
	}
	else
	{
		MW_LOG_INFO("Setting state to GST_STATE_PLAYING");
		if (SetStateWithWarnings(gstPrivateContext->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			MW_LOG_ERR("InterfacePlayerRDK: GST_STATE_PLAYING failed");
		}
		gstPrivateContext->pendingPlayState = false;
		gstPrivateContext->paused = false;
	}
	gstPrivateContext->eosSignalled = false;
	gstPrivateContext->numberOfVideoBuffersSent = 0;
	gstPrivateContext->decodeErrorMsgTimeMS = 0;
	gstPrivateContext->decodeErrorCBCount = 0;
	if (gstPrivateContext->usingRialtoSink)
	{
		MW_LOG_INFO("RialtoSink subtitle_sink = %p ",gstPrivateContext->subtitle_sink);
		GstContext *context = gst_context_new("streams-info", false);
		GstStructure *contextStructure = gst_context_writable_structure(context);
		if( !gstPrivateContext->subtitle_sink ) MW_LOG_WARN( "subtitle_sink==NULL" );
		gst_structure_set(
						  contextStructure,
						  "video-streams", G_TYPE_UINT, (gstPrivateContext->video_sink)?0x1u:0x0u,
						  "audio-streams", G_TYPE_UINT, (gstPrivateContext->audio_sink)?0x1u:0x0u,
						  "text-streams", G_TYPE_UINT, (gstPrivateContext->subtitle_sink)?0x1u:0x0u,
						  nullptr );
		gst_element_set_context(GST_ELEMENT(gstPrivateContext->pipeline), context);
		gst_context_unref(context);
	}
}

/**
 * @brief Invoked synchronously when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval GST_BUS_PASS to pass the message to the async queue
 */
static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * msg, InterfacePlayerRDK * pInterfacePlayerRDK);

void InterfacePlayerRDK::SetPauseOnStartPlayback(bool enable)
{
	gstPrivateContext->pauseOnStartPlayback = enable;
}

/**
 * @brief Idle callback to notify first frame rendered event
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
gboolean InterfacePlayerRDK::IdleCallbackOnFirstFrame(gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *)user_data;
	if (pInterfacePlayerRDK)
	{
		pInterfacePlayerRDK->TriggerEvent(InterfaceCB::firstVideoFrameReceived);
		pInterfacePlayerRDK->gstPrivateContext->firstFrameCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;
		pInterfacePlayerRDK->gstPrivateContext->firstFrameCallbackIdleTaskPending = false;
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Callback invoked after first video frame decoded
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 */
static void GstPlayer_OnFirstVideoFrameCallback(GstElement* object, guint arg0, gpointer arg1,
												InterfacePlayerRDK * pInterfacePlayerRDK)

{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameReceived = true;
	pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_VIDEO);

}

/**
 * @brief Callback invoked after first audio buffer decoded
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 */
static void GstPlayer_OnAudioFirstFrameAudDecoder(GstElement* object, guint arg0, gpointer arg1,
												  InterfacePlayerRDK * pInterfacePlayerRDK)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	pInterfacePlayerRDK->gstPrivateContext->firstAudioFrameReceived = true;
	pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_AUDIO);
}

/**
 * @brief Idle callback to notify end-of-stream event
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
gboolean InterfacePlayerRDK::IdleCallbackOnEOS(gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *)user_data;
	if (pInterfacePlayerRDK)
	{
		MW_LOG_MIL("eosCallbackIdleTaskId %d", pInterfacePlayerRDK->gstPrivateContext->eosCallbackIdleTaskId);
		pInterfacePlayerRDK->TriggerEvent(InterfaceCB::notifyEOS);
		pInterfacePlayerRDK->gstPrivateContext->eosCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;
		pInterfacePlayerRDK->gstPrivateContext->eosCallbackIdleTaskPending = false;
	}
	return G_SOURCE_REMOVE;
}

void MonitorAV( InterfacePlayerRDK *pInterfacePlayerRDK )
{
	const int AVSYNC_THRESHOLD_MS = pInterfacePlayerRDK->m_gstConfigParam->monitorAvsyncThresholdMs;
	const int JUMP_THRESHOLD_MS = pInterfacePlayerRDK->m_gstConfigParam->monitorJumpThresholdMs;


	GstState state = GST_STATE_VOID_PENDING;
	GstState pending = GST_STATE_VOID_PENDING;
	GstClockTime timeout = 0;
	gint64 av_position[2] = {0,0};
	gint rc = gst_element_get_state(pInterfacePlayerRDK->gstPrivateContext->pipeline, &state, &pending, timeout );
	if( rc == GST_STATE_CHANGE_SUCCESS )
	{
		if( state == GST_STATE_PLAYING )
		{
			struct MonitorAVState *monitorAVState = &pInterfacePlayerRDK->gstPrivateContext->monitorAVstate;
			const char *description = NULL;
			int numTracks = 0;
			bool bigJump = false;
			long long tNow = GetCurrentTimeMS();
			if( !monitorAVState->tLastReported )
			{
				monitorAVState->tLastReported = tNow;
			}
			// skip reading audio position when trickplay is active
			int maxTracks = (pInterfacePlayerRDK->gstPrivateContext->rate == GST_NORMAL_PLAY_RATE) ? 2 : 1;
			for( int i=0; i<maxTracks; i++ )
			{ // eMEDIATYPE_VIDEO=0, eMEDIATYPE_AUDIO=1
				auto sinkbin = pInterfacePlayerRDK->gstPrivateContext->stream[i].sinkbin;
				if (sinkbin && (pInterfacePlayerRDK->gstPrivateContext->stream[i].format != GST_FORMAT_INVALID))
				{
					gint64 position = GST_CLOCK_TIME_NONE;
					if( gst_element_query_position(sinkbin, GST_FORMAT_TIME, &position) )
					{
						long long ms = GST_TIME_AS_MSECONDS(position);
						if( ms == monitorAVState->av_position[i] )
						{
							if( description )
							{ // both tracks stalled
								description = "stall";
							}
							else
							{
								description = (i==eGST_MEDIATYPE_VIDEO)?"video freeze":"audio drop";
							}
						}
						else if( i == eGST_MEDIATYPE_VIDEO && monitorAVState->happy )
						{
							auto actualDelta = ms - monitorAVState->av_position[i];
							auto expectedDelta = tNow - monitorAVState->tLastSampled;
							if( actualDelta  > expectedDelta+JUMP_THRESHOLD_MS )
							{
								bigJump = true;
							}
						}
						av_position[i] = ms;
						numTracks++;
					}
				}
			}
			monitorAVState->tLastSampled = tNow;
			switch( numTracks )
			{
				case 0:
					description = "eos";
					break;
				case 1:
					description = "trickplay";
					break;
				case 2:
					if( abs(av_position[0] - av_position[1]) > AVSYNC_THRESHOLD_MS )
					{
						if( !description )
						{ // both moving, but diverged
							description = "avsync";
						}
					}
					else if( bigJump )
					{ // workaround to detect decoders that jump over AV gaps without delay
						description = "jump";
					}
					break;
				default:
					break;
			}
			if( !description )
			{ // fill in OK if nothing flagged
				description = "ok";
			}
			if( monitorAVState->description!=description )
			{ // log only when interpretation of AV state has changed
				if( monitorAVState->description )
				{ // avoid logging for initial NULL description
					MW_LOG_MIL( "MonitorAV_%s: %" G_GINT64_FORMAT ",%" G_GINT64_FORMAT ",%d,%lld",
							   monitorAVState->description,
							   monitorAVState->av_position[eGST_MEDIATYPE_VIDEO],
							   monitorAVState->av_position[eGST_MEDIATYPE_AUDIO],
							   (int)(monitorAVState->av_position[eGST_MEDIATYPE_VIDEO] - monitorAVState->av_position[eGST_MEDIATYPE_AUDIO]),
							   monitorAVState->tLastSampled - monitorAVState->tLastReported );
				}
				MW_LOG_MIL( "MonitorAV_%s: %" G_GINT64_FORMAT ",%" G_GINT64_FORMAT ",%d,0",
							   description,
								av_position[eGST_MEDIATYPE_VIDEO],
								av_position[eGST_MEDIATYPE_AUDIO],
								(int)(av_position[eGST_MEDIATYPE_VIDEO] - av_position[eGST_MEDIATYPE_AUDIO]) );
				monitorAVState->tLastReported = monitorAVState->tLastSampled;
				monitorAVState->description = description;
			}
			// remember most recently sniffed pair of video and audio positions
			monitorAVState->av_position[eGST_MEDIATYPE_VIDEO] = av_position[eGST_MEDIATYPE_VIDEO];
			monitorAVState->av_position[eGST_MEDIATYPE_AUDIO] = av_position[eGST_MEDIATYPE_AUDIO];
		}
	}
	else
	{
		MW_LOG_WARN( "gst_element_get_state %d", state );
	}
}

/**
 * @brief Timer's callback to notify playback progress event
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 * @retval G_SOURCE_CONTINUE, this function to be called periodically
 */
gboolean InterfacePlayerRDK::ProgressCallbackOnTimeout(gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *)user_data;
	if (pInterfacePlayerRDK)
	{
		if (pInterfacePlayerRDK->m_gstConfigParam->monitorAV)
		{
			MonitorAV(pInterfacePlayerRDK);
		}
		pInterfacePlayerRDK->TriggerEvent(InterfaceCB::progressCb);
		MW_LOG_TRACE("current %d, stored %d ", g_source_get_id(g_main_current_source()), pInterfacePlayerRDK->gstPrivateContext->periodicProgressCallbackIdleTaskId);
	}
	return G_SOURCE_CONTINUE;
}

/**
 * @brief Idle callback to start progress notifier timer
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
gboolean InterfacePlayerRDK::IdleCallback(gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *)user_data;
	if (pInterfacePlayerRDK)
	{
		pInterfacePlayerRDK->TriggerEvent(InterfaceCB::idleCb);
		pInterfacePlayerRDK->IdleTaskClearFlags(pInterfacePlayerRDK->gstPrivateContext->firstProgressCallbackIdleTask);

		if ( !(pInterfacePlayerRDK->TimerIsRunning( pInterfacePlayerRDK->gstPrivateContext->periodicProgressCallbackIdleTaskId)) )
		{
			double  reportProgressInterval = pInterfacePlayerRDK->m_gstConfigParam->progressTimer;
			reportProgressInterval *= 1000; //convert s to ms

			GSourceFunc timerFunc = ProgressCallbackOnTimeout;
			pInterfacePlayerRDK->TimerAdd(timerFunc, (int)reportProgressInterval, pInterfacePlayerRDK->gstPrivateContext->periodicProgressCallbackIdleTaskId, user_data, "periodicProgressCallbackIdleTask");
		}
		else
		{
			MW_LOG_INFO("Progress callback already available: periodicProgressCallbackIdleTaskId %d", pInterfacePlayerRDK->gstPrivateContext->periodicProgressCallbackIdleTaskId);
		}
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Idle callback to notify first video frame was displayed
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
gboolean InterfacePlayerRDK::IdleCallbackFirstVideoFrameDisplayed(gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *)user_data;
	if (pInterfacePlayerRDK)
	{
		pInterfacePlayerRDK->TriggerEvent(InterfaceCB::firstVideoFrameDisplayed);
		pInterfacePlayerRDK->IdleTaskRemove(pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameDisplayedCallbackTask);
	}
	return G_SOURCE_REMOVE;
}

bool gst_StartsWith( const char *inputStr, const char *prefix );
/**
 *@brief set the encrypted content, should be used by playready plugin
 */
void InterfacePlayerRDK::setEncryption(void *Encrypt)
{
	mEncrypt = Encrypt;
}

/**
 *@brief sets the preferred drm by app
 *@param[in] drmID preferred drm
 */
void InterfacePlayerRDK::SetPreferredDRM(const char *drmID)
{
	if (drmID != NULL)
	{
		if (mDrmSystem != NULL)
		{
			delete[] mDrmSystem;
		}
		mDrmSystem = new char[strlen(drmID) + 1];
		if (mDrmSystem != NULL)
		{
			strcpy(mDrmSystem, drmID);
		}
		else
		{
			MW_LOG_ERR("Memory allocation failed for mDrmSystem\n");
		}
	}
}

/**
 * @brief Called from the mainloop when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval FALSE if the event source should be removed.
 */
static gboolean bus_message(GstBus * bus, GstMessage * msg, InterfacePlayerRDK * pInterfacePlayerRDK);

/**
 * @brief check if element is instance
 */
static void type_check_instance( const char * str, GstElement * elem);

/**
 * @fn InterfacePlayerRDK_SignalEOS
 * @brief Signal EOS to the appsrc associated with the supplied media stream
 * @param[in] media_stream the media stream to inject EOS into
 */
static void GstPlayer_SignalEOS(gst_media_stream& stream)
{
	if (stream.source)
	{
		auto ret = gst_app_src_end_of_stream(GST_APP_SRC_CAST(stream.source));
		//GST_FLOW_OK is expected in PAUSED or PLAYING states; GST_FLOW_FLUSHING is expected in other states.
		if (ret != GST_FLOW_OK)
		{
			MW_LOG_WARN("gst_app_src_push_buffer  error: %d", ret);
		}
	}
}

static void GstPlayer_SignalEOS(gst_media_stream* stream)
{
	if(stream)
	{
		GstPlayer_SignalEOS(*stream);
	}
}

/**
 *  @brief inject EOS for all media types to ensure the pipeline can be set to NULL quickly*/
static void GstPlayer_SignalEOS(GstPlayerPriv* gstPrivateContext)
{
	MW_LOG_MIL(" InterfacePlayer: Inject EOS into all streams.");

	if(gstPrivateContext && gstPrivateContext->pipeline)
	{
		for(int mediaType=eGST_MEDIATYPE_VIDEO; mediaType<=eGST_MEDIATYPE_AUX_AUDIO; mediaType++)
		{
			GstPlayer_SignalEOS(gstPrivateContext->stream[mediaType]);
		}
	}
	else
	{
		MW_LOG_WARN(" InterfacePlayer: null pointer check failed");
	}
}

/**
 * @fn SetSeekPosition
 * @param[in] positionSecs - the start position to seek the pipeline to in seconds
 */
void InterfacePlayerRDK::SetSeekPosition(double positionSecs)
{
	gstPrivateContext->seekPosition = positionSecs;
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		gstPrivateContext->stream[i].pendingSeek = true;
	}
}


static constexpr int RECURSION_LIMIT = 10;

/**
 *  @brief GetElementPointers adds the supplied element/bin and any child elements up to RECURSION_LIMIT depth to elements
 */
static void GetElementPointers(gpointer pElementOrBin, std::set<gpointer>& elements, int& recursionCount)
{
	recursionCount++;
	if(RECURSION_LIMIT < recursionCount)
	{
		MW_LOG_ERR(" Interface recursion limit exceeded");
	}
	else if(GST_IS_ELEMENT(pElementOrBin))
	{
		elements.insert(pElementOrBin);
		if(GST_IS_BIN(pElementOrBin))
		{
			for (auto currentListItem = GST_BIN_CHILDREN(reinterpret_cast<_GstElement*>(pElementOrBin));
				 currentListItem;
				 currentListItem = currentListItem->next)
			{
				auto currentChildElement = currentListItem->data;
				if (nullptr != currentChildElement)
				{
					//Recursive function call to support nesting of gst elements up RECURSION_LIMIT
					GetElementPointers(currentChildElement, elements, recursionCount);
				}
			}
		}
	}

	recursionCount--;
}

/**
 *  @brief GetElementPointers returns a set of pointers to the supplied element/bin and any child elements up to RECURSION_LIMIT depth
 */
static std::set<gpointer>  GetElementPointers(gpointer pElementOrBin)
{
	int recursionCount = 0;
	std::set<gpointer> elements;
	GetElementPointers(pElementOrBin, elements, recursionCount);
	return elements;
}

void InterfacePlayerRDK::DisconnectSignals()
{
	const std::lock_guard<std::mutex> lock(gstPrivateContext->mSignalVectorAccessMutex);
	if(m_gstConfigParam->enableDisconnectSignals)
	{
		std::set<gpointer> elements = GetElementPointers(gstPrivateContext->pipeline);

		for(auto data: gstPrivateContext->mCallBackIdentifiers)
		{
			if (data.instance == nullptr)
			{
				MW_LOG_ERR(" InterfacePlayerRDK: %s signal handler, connected instance pointer is null", data.name.c_str());
			}
			else if(data.id == 0)
			{
				MW_LOG_ERR(" InterfacePlayerRDK: %s signal handler id is 0", data.name.c_str());
			}
			else if(!elements.count(data.instance))
			{
				// This is expected following some tune failures
				MW_LOG_WARN(" InterfacePlayerRDK: %s signal handler, connected instance is not in the pipeline", data.name.c_str());
			}
			else if(!g_signal_handler_is_connected(data.instance, data.id))
			{
				MW_LOG_ERR("InterfacePlayerRDK: %s signal handler not connected", data.name.c_str());
			}
			else
			{
				MW_LOG_WARN("InterfacePlayerRDK: disconnecting %s signal handler", data.name.c_str());
				g_signal_handler_disconnect(data.instance, data.id);
			}
		}
	}
	else
	{
		MW_LOG_WARN("Interface enableDisconnectSignals==false. Signals have not been disconnected.");
	}
	gstPrivateContext->mCallBackIdentifiers.clear();
}
/**
 *  @brief TimerRemove - remove a glib timer in thread safe manner, if it exists
 */
void InterfacePlayerRDK::TimerRemove(guint& taskId, const char* timerName)
{
	std::lock_guard<std::mutex> lock(gstPrivateContext->TaskControlMutex);
	if ( 0 != taskId )
	{
		MW_LOG_INFO("InterfacePlayerRDK: Remove timer '%.50s', %d", (nullptr!=timerName) ? timerName : "unknown", taskId);
		g_source_remove(taskId);                                        /* Removes the source as per the taskId */
		taskId = 0;
	}
	else
	{
		MW_LOG_TRACE("Interface Timer '%.50s' with taskId = %d already removed.", (nullptr!=timerName) ? timerName : "unknown", taskId);
	}
}
/**
 * @fn RemoveProbes
 * @brief Remove probes from the pipeline
 */
void InterfacePlayerRDK::RemoveProbes()
{
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		RemoveProbe((GstMediaType)i);
	}
}

/**
 * @fn RemoveProbe
 * @brief Remove probe for a particular media type
 * @param[in] mediaType The media type for which the probe should be removed
 */
void InterfacePlayerRDK::RemoveProbe(GstMediaType mediaType)
{
	gst_media_stream *stream = &gstPrivateContext->stream[mediaType];
	if (stream->demuxProbeId && stream->demuxPad)
	{
		MW_LOG_WARN("InterfacePlayerRDK: Removing probe for media type %d, probe id %lu", mediaType, stream->demuxProbeId);
		gst_pad_remove_probe(stream->demuxPad, stream->demuxProbeId);
		stream->demuxProbeId = 0;
		stream->demuxPad = NULL;
	}
}

/**
 *  @brief Cleanup an existing Gstreamer pipeline and associated resources
 */
void InterfacePlayerRDK::DestroyPipeline()
{
	if (gstPrivateContext->pipeline)
	{
		/*"Destroying gstreamer pipeline" should only be logged when there is a pipeline to destroy
		 and each "Destroying gstreamer pipeline" log entry should have one, prior "Creating gstreamer pipeline" log entry*/
		MW_LOG_MIL("Interface Destroying gstreamer pipeline");
		gst_object_unref(gstPrivateContext->pipeline);         /* Decreases the reference count on gstPrivateContext->pipeline, in this case it will become zero,
																the reference to gstPrivateContext->pipeline will be freed in gstreamer */
		gstPrivateContext->pipeline = NULL;
	}
	if (gstPrivateContext->bus)
	{
		gst_bus_remove_watch(gstPrivateContext->bus);
		gst_object_unref(gstPrivateContext->bus);              /* Decreases the reference count on gstPrivateContext->bus, in this case it will become zero,
																the reference to gstPrivateContext->bus will be freed in gstreamer */
		gstPrivateContext->bus = NULL;
	}
	if(gstPrivateContext->task_pool)
	{
		gst_object_unref(gstPrivateContext->task_pool);
		gstPrivateContext->task_pool = NULL;
	}

	if (gstPrivateContext->positionQuery)
	{
		/* Decrease the refcount of the query. If the refcount reaches 0, the query will be freed */
		gst_query_unref(gstPrivateContext->positionQuery);
		gstPrivateContext->positionQuery = NULL;
	}

	//video decoder handle will change with new pipeline
	gstPrivateContext->decoderHandleNotified = false;
	gstPrivateContext->NumberOfTracks = 0;
}

/**
 * @brief wraps gst_element_get_name handling unnamed elements and resource freeing
 * @param[in] element a GstElement
 * @retval The name of element or "unnamed element" as a std::string
 */
static std::string SafeName(GstElement *element)
{
	std::string name;
	auto elementName = gst_element_get_name(element);
	if(elementName)
	{
		name = elementName;
		g_free((void *)elementName);
	}
	else
	{
		name = "unnamed element";
	}
	return name;
}

/**
 * @brief Generates a state description for gst target, next and pending state i.e. **not current state**.
 * @param[in] state  - the state of the current element
 * @param[in] start - a  char to place before the state text e.g. on open bracket
 * @param[in] end  - a char to place after the state text e.g. a close bracket
 * @param[in] currentState - the current state from the same element as 'state'
 * @param[in] parentState - the state of the parent, if there is one
 * @return  - "" unless state is 'interesting' otherwise *start* *state description* *end* e.g. {GST_STATE_READY}
 */
static std::string StateText(GstState state, char start, char end, GstState currentState, GstState parentState = GST_STATE_VOID_PENDING)
{
	if((state == GST_STATE_VOID_PENDING) || ((state == currentState) && ((state == parentState) || (parentState == GST_STATE_VOID_PENDING))))
	{
		return "";
	}
	else
	{
		std::string returnStringBuilder(1, start);
		returnStringBuilder += gst_element_state_get_name(state);
		returnStringBuilder += end;
		return returnStringBuilder;
	}
}

/**
 * @brief - returns a string describing pElementOrBin and its children (if any).
 * The top level elements name:state are shown along with any child elements in () separated by ,
 * State information is displayed as GST_STATE[GST_STATE_TARGET]{GST_STATE_NEXT}<GST_STATE_PENDING>
 * Target state, next state and pending state are not always shown.
 * Where GST_STATE_CHANGE for the element is not GST_STATE_CHANGE_SUCCESS an additional character is appended to the element name:
 GST_STATE_CHANGE_FAILURE: "!", GST_STATE_CHANGE_ASYNC:"~", GST_STATE_CHANGE_NO_PREROLL:"*"
 * @param[in] pElementOrBin - pointer to a gst element or bin
 * @param[in] pParent - parent (optional)
 * @param recursionCount - variable shared with self calls to limit recursion depth
 * @return - description string
 */
static std::string GetStatus(gpointer pElementOrBin, int& recursionCount, gpointer pParent = nullptr)
{
	recursionCount++;
	constexpr int RECURSION_LIMIT = 10;
	if(RECURSION_LIMIT < recursionCount)
	{
		return "recursion limit exceeded";
	}

	std::string returnStringBuilder("");
	if(nullptr !=pElementOrBin)
	{
		if(GST_IS_ELEMENT(pElementOrBin))
		{
			auto pElement = reinterpret_cast<_GstElement*>(pElementOrBin);

			bool validParent = (pParent != nullptr) && GST_IS_ELEMENT(pParent);

			returnStringBuilder += SafeName(pElement);
			GstState state;
			GstState statePending;
			auto changeStatus = gst_element_get_state(pElement, &state, &statePending, 0);
			switch(changeStatus)
			{
				case  GST_STATE_CHANGE_FAILURE:
					returnStringBuilder +="!";
					break;

				case  GST_STATE_CHANGE_SUCCESS:
					//no annotation
					break;

				case  GST_STATE_CHANGE_ASYNC:
					returnStringBuilder +="~";
					break;

				case  GST_STATE_CHANGE_NO_PREROLL:
					returnStringBuilder +="*";
					break;

				default:
					returnStringBuilder +="?";
					break;
			}

			returnStringBuilder += ":";

			returnStringBuilder += gst_element_state_get_name(state);

			returnStringBuilder += StateText(statePending, '<', '>', state,
											 validParent?GST_STATE_PENDING(pParent):GST_STATE_VOID_PENDING);
			returnStringBuilder += StateText(GST_STATE_TARGET(pElement), '[', ']', state,
											 validParent?GST_STATE_TARGET(pParent):GST_STATE_VOID_PENDING);
			returnStringBuilder += StateText(GST_STATE_NEXT(pElement), '{', '}', state,
											 validParent?GST_STATE_NEXT(pParent):GST_STATE_VOID_PENDING);
		}

		//note bin inherits from element so name bin name is also printed above, with state info where applicable
		if(GST_IS_BIN(pElementOrBin))
		{
			returnStringBuilder += " (";

			auto pBin = reinterpret_cast<_GstElement*>(pElementOrBin);
			bool first = true;
			for (auto currentListItem = GST_BIN_CHILDREN(pBin);
				 currentListItem;
				 currentListItem = currentListItem->next)
			{
				if(first)
				{
					first = false;
				}
				else
				{
					returnStringBuilder += ", ";
				}

				auto currentChildElement = currentListItem->data;
				if (nullptr != currentChildElement)
				{
					//Recursive function call to support nesting of gst elements up RECURSION_LIMIT
					returnStringBuilder += GetStatus(currentChildElement, recursionCount, pBin);
				}
			}
			returnStringBuilder += ")";
		}
	}
	recursionCount--;
	return returnStringBuilder;
}

static void LogStatus(GstElement* pElementOrBin)
{
	int recursionCount = 0;
	MW_LOG_MIL("InterfacePlayerRDK: %s Status: %s",SafeName(pElementOrBin).c_str(), GetStatus(pElementOrBin, recursionCount).c_str());
}


/**
 * @brief wraps gst_element_set_state and adds log messages where applicable
 * @param[in] element the GstElement whose state is to be changed
 * @param[in] targetState the GstState to apply to element
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval Result of the state change (from inner gst_element_set_state())
 */
static GstStateChangeReturn SetStateWithWarnings(GstElement *element, GstState targetState)
{
	GstStateChangeReturn rc = GST_STATE_CHANGE_FAILURE;
	if(element)
	{
		//in a synchronous only transition gst_element_set_state can lockup if there are pipeline errors
		bool syncOnlyTransition = (targetState==GST_STATE_NULL)||(targetState==GST_STATE_READY);

		GstState current;																	/* To hold the current state of the element */
		GstState pending;																	/* Pending state, used in printing the pending state of the element */

		auto stateChangeReturn = gst_element_get_state(element, &current, &pending, 0);		/* Get the current playing state of the element with no blocking timeout,  this function is MT-safe */
		switch(stateChangeReturn)
		{
			case GST_STATE_CHANGE_FAILURE:
				MW_LOG_ERR("InterfacePlayerRDK: %s is in FAILURE state : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
				LogStatus(element);
				break;
			case GST_STATE_CHANGE_SUCCESS:
				MW_LOG_DEBUG("InterfacePlayerRDK: %s is in success state : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
				break;
			case GST_STATE_CHANGE_ASYNC:
				if(syncOnlyTransition)
				{
					MW_LOG_MIL("InterfacePlayerRDK: %s state is changing asynchronously : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
					LogStatus(element);
				}
				break;
			default:
				MW_LOG_ERR("InterfacePlayerRDK: %s is in an unknown state", SafeName(element).c_str());
				break;
		}

		if(syncOnlyTransition)
		{
			MW_LOG_MIL("InterfacePlayerRDK: Attempting to set %s state to %s", SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		else
		{
			MW_LOG_DEBUG("InterfacePlayerRDK: Attempting to set %s state to %s", SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		rc = gst_element_set_state(element, targetState);					/* Set the state of the element to the targetState, this function is MT-safe*/
		if(syncOnlyTransition)
		{
			MW_LOG_MIL("InterfacePlayerRDK: %s state set to %s",  SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		else
		{
			MW_LOG_DEBUG(" InterfacePlayerRDK: %s state set to %s, rc:%d",  SafeName(element).c_str(), gst_element_state_get_name(targetState), rc);
		}
	}
	else
	{
		MW_LOG_ERR("InterfacePlayerRDK: Attempted to set the state of a null pointer");
	}
	return rc;
}


void InterfacePlayerRDK::TearDownStream(GstMediaType mediaType)
{
	tearDownCb(true, mediaType);
	RemoveProbe(mediaType);
	gst_media_stream* stream = &gstPrivateContext->stream[mediaType];
	stream->bufferUnderrun = false;
	stream->eosReached = false;
	if (stream->format != GST_FORMAT_INVALID)
	{
		pthread_mutex_lock(&stream->sourceLock);
		if (gstPrivateContext->pipeline)
		{
			gstPrivateContext->buffering_in_progress = false;   /* stopping pipeline, don't want to change state if GST_MESSAGE_ASYNC_DONE message comes in */
			/* set the playbin state to NULL before detach it */
			if (stream->sinkbin)
			{
				if (GST_STATE_CHANGE_FAILURE == SetStateWithWarnings(GST_ELEMENT(stream->sinkbin), GST_STATE_NULL))
				{
					MW_LOG_ERR("InterfacePlayerRDK::TearDownStream: Failed to set NULL state for sinkbin");
				}
				if (!gst_bin_remove(GST_BIN(gstPrivateContext->pipeline), GST_ELEMENT(stream->sinkbin)))			/* Removes the sinkbin element from the pipeline */
				{
					MW_LOG_ERR("InterfacePlayerRDK::TearDownStream:  Unable to remove sinkbin from pipeline");
				}
			}
			else
			{
				MW_LOG_WARN("InterfacePlayerRDK::TearDownStream:  sinkbin = NULL, skip remove sinkbin from pipeline");
			}
		}
		//After sinkbin is removed from pipeline, a new decoder handle may be generated
		if (mediaType == eGST_MEDIATYPE_VIDEO)
		{
			gstPrivateContext->decoderHandleNotified = false;
		}
		stream->format = GST_FORMAT_INVALID;
		g_clear_object(&stream->sinkbin);
		g_clear_object(&stream->source);
		stream->sourceConfigured = false;
		pthread_mutex_unlock(&stream->sourceLock);
	}
	if (mediaType == eGST_MEDIATYPE_VIDEO)
	{
		g_clear_object(&gstPrivateContext->video_dec);
		g_clear_object(&gstPrivateContext->video_sink);
	}
	else if (mediaType == eGST_MEDIATYPE_AUDIO)
	{
		g_clear_object(&gstPrivateContext->audio_dec);
		g_clear_object(&gstPrivateContext->audio_sink);
	}
	else if (mediaType == eGST_MEDIATYPE_SUBTITLE)
	{
		g_clear_object(&gstPrivateContext->subtitle_sink);
	}
	tearDownCb(false, mediaType);
	MW_LOG_MIL("InterfacePlayerRDK::TearDownStream: exit mediaType = %d", mediaType);
}

void InterfacePlayerRDK::Stop(bool keepLastFrame)
{
	std::lock_guard<std::mutex> lock(mMutex);
	/*  make the execution of this function more deterministic and
	 *  reduce scope for potential pipeline lockups*/

	gstPrivateContext->syncControl.disable();
	gstPrivateContext->aSyncControl.disable();
	std::unique_lock<std::mutex> sourceSetupLock(mSourceSetupMutex);
	mSourceSetupCV.notify_all();
	if(gstPrivateContext->bus)
	{
		gst_bus_remove_watch(gstPrivateContext->bus);           /* Remove the watch from bus so that gstreamer no longer sends messages to it */
	}

	if(!keepLastFrame)
	{
		gstPrivateContext->firstFrameReceived = false;
		gstPrivateContext->firstVideoFrameReceived = false;
		gstPrivateContext->firstAudioFrameReceived = false ;
	}
	this->IdleTaskRemove(gstPrivateContext->firstProgressCallbackIdleTask);

	this->TimerRemove(gstPrivateContext->periodicProgressCallbackIdleTaskId, "periodicProgressCallbackIdleTaskId");
	if (gstPrivateContext->bufferingTimeoutTimerId)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove bufferingTimeoutTimerId %d", gstPrivateContext->bufferingTimeoutTimerId);
		g_source_remove(gstPrivateContext->bufferingTimeoutTimerId);
		gstPrivateContext->bufferingTimeoutTimerId = PLAYER_TASK_ID_INVALID;
	}
	if (gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove ptsCheckForEosCallbackIdleTaskId %d",gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		g_source_remove(gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId = PLAYER_TASK_ID_INVALID;
	}
	if (gstPrivateContext->eosCallbackIdleTaskPending)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove eosCallbackIdleTaskId %d",gstPrivateContext->eosCallbackIdleTaskId);
		mScheduler.RemoveTask(gstPrivateContext->eosCallbackIdleTaskId);
		gstPrivateContext->eosCallbackIdleTaskPending = false;
		gstPrivateContext->eosCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;
	}
	if (gstPrivateContext->firstFrameCallbackIdleTaskPending)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove firstFrameCallbackIdleTaskId %d",gstPrivateContext->firstFrameCallbackIdleTaskId);
		mScheduler.RemoveTask(gstPrivateContext->firstFrameCallbackIdleTaskId);
		gstPrivateContext->firstFrameCallbackIdleTaskPending = false;
		gstPrivateContext->firstFrameCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;
	}
	this->IdleTaskRemove(gstPrivateContext->firstVideoFrameDisplayedCallbackTask);
	/* Prevent potential side effects of injecting EOS and
	 * make the stop process more deterministic by:
	 1) Confirming that bus handlers (disabled above) have completed
	 2) disabling and disconnecting signals
	 3) confirming that all signal handlers have completed.
	 * This should complete very quickly and
	 * should not have a significant performance impact.*/
	gstPrivateContext->syncControl.waitForDone(50, "bus_sync_handler");
	gstPrivateContext->aSyncControl.waitForDone(50, "bus_message");
	gstPrivateContext->callbackControl.disable();
	DisconnectSignals();
	gstPrivateContext->aSyncControl.waitForDone(100, "callback handler");

	// Remove probes before setting the pipeline to NULL
	RemoveProbes();

	if (gstPrivateContext->pipeline)
	{
		const auto EOSMode = m_gstConfigParam->eosInjectionMode;
		if(GstEOS_INJECTION_MODE_STOP_ONLY == EOSMode)
		{
			//Ensure prompt transition to GST_STATE_NULL
			GstPlayer_SignalEOS(gstPrivateContext);
		}

		gstPrivateContext->buffering_in_progress = false;   /* stopping pipeline, don't want to change state if GST_MESSAGE_ASYNC_DONE message comes in */
		SetStateWithWarnings(gstPrivateContext->pipeline, GST_STATE_NULL);
		MW_LOG_MIL(" InterfacePlayerRDK: Pipeline state set to null");
	}
	if(PlayerExternalsInterface::IsPlayerExternalsInterfaceInstanceActive())
	{
		std::shared_ptr<PlayerExternalsInterface> pInstance = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();
		pInstance->setGstElement((GstElement *)(NULL));
	}
	for(int i = 0; i<GST_TRACK_COUNT;i++)
	{
		TearDownStream((GstMediaType(i)));
	}
	DestroyPipeline();
	gstPrivateContext->rate = GST_NORMAL_PLAY_RATE;
	gstPrivateContext->lastKnownPTS = 0;
	gstPrivateContext->segmentStart = 0;
	gstPrivateContext->paused = false;
	gstPrivateContext->pipelineState = GST_STATE_NULL;
	// Reset mute and volume params
	gstPrivateContext->audioMuted = false;
	gstPrivateContext->videoMuted = false;
	gstPrivateContext->subtitleMuted = false;
	gstPrivateContext->audioVolume = 1.0;
}

void InterfacePlayerRDK::ResetGstEvents()
{
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		gstPrivateContext->stream[i].resetPosition = true;
		// Pipeline is already flushed, no need to send seek event again
		gstPrivateContext->stream[i].pendingSeek = false;
		gstPrivateContext->stream[i].eosReached = false;
		gstPrivateContext->stream[i].firstBufferProcessed = false;
	}

}

void InterfacePlayerRDK::SetPendingSeek(bool state)
{
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		gstPrivateContext->stream[i].pendingSeek = state;
	}
}

bool InterfacePlayerRDK::GetTrickTeardown()
{
	return trickTeardown;
}

void InterfacePlayerRDK::SetTrickTearDown(bool state)
{
	trickTeardown = state;
}

/**
 *  @brief IdleTaskRemove - remove an async task in a thread safe manner, if it is queued
 */
bool InterfacePlayerRDK::IdleTaskRemove(GstTaskControlData& taskDetails)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(gstPrivateContext->TaskControlMutex);

	if (0 != taskDetails.taskID)
	{
		MW_LOG_INFO("InterfacePlayerRDK: Remove task <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
		mScheduler.RemoveTask(taskDetails.taskID);
		taskDetails.taskID = 0;
		ret = true;
	}
	else
	{
		MW_LOG_TRACE("InterfacePlayerRDK: Task already removed <%.50s>, with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	taskDetails.taskIsPending = false;
	return ret;
}

bool InterfacePlayerRDK::IsUsingRialtoSink()
{
	if (gstPrivateContext)
	{
		return gstPrivateContext->usingRialtoSink;
	}
	return false;
}

/**
 *  @brief Flush cached GstBuffers and set seek position & rate
 */
bool InterfacePlayerRDK::Flush(double position, int rate, bool shouldTearDown, bool isAppSeek)
{
	GstState aud_current;
	GstState aud_pending;
	GstState current;
	GstState pending;

	gst_media_stream *stream = &gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO];
	gstPrivateContext->rate = rate;
	gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun = false;
	gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].bufferUnderrun = false;
	if (gstPrivateContext->eosCallbackIdleTaskPending)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove eosCallbackIdleTaskId %d", gstPrivateContext->eosCallbackIdleTaskId);
		mScheduler.RemoveTask(gstPrivateContext->eosCallbackIdleTaskId);
		gstPrivateContext->eosCallbackIdleTaskId = PLAYER_TASK_ID_INVALID;
		gstPrivateContext->eosCallbackIdleTaskPending = false;

	}
	if (gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove ptsCheckForEosCallbackIdleTaskId %d", gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		g_source_remove(gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId = PLAYER_TASK_ID_INVALID;

	}
	if (gstPrivateContext->bufferingTimeoutTimerId)
	{
		MW_LOG_MIL("InterfacePlayerRDK: Remove bufferingTimeoutTimerId %d", gstPrivateContext->bufferingTimeoutTimerId);
		g_source_remove(gstPrivateContext->bufferingTimeoutTimerId);
		gstPrivateContext->bufferingTimeoutTimerId = PLAYER_TASK_ID_INVALID;

	}

	// If the pipeline is not setup, we will cache the value for later
	SetSeekPosition(position);

	if (gstPrivateContext->pipeline == NULL)
	{
		MW_LOG_WARN("InterfacePlayerRDK: Pipeline is NULL");
		return false;
	}
	bool bAsyncModify = false;
	if (!gstPrivateContext->usingRialtoSink)
	{
		bAsyncModify = socInterface->DisableAsyncAudio(gstPrivateContext->audio_sink, rate, isAppSeek);
		// Send EOS to audio sink to prevent flush getting blocked waiting for preroll
		if (gstPrivateContext->audio_sink)
		{
			GstPlayer_SignalEOS(gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO]);
		}
	}
	GstStateChangeReturn ret;
	ret = gst_element_get_state(gstPrivateContext->pipeline, &current, &pending, 100 * GST_MSECOND);
	if ((current != GST_STATE_PLAYING && current != GST_STATE_PAUSED) || ret == GST_STATE_CHANGE_FAILURE)
	{
		MW_LOG_WARN("InterfacePlayerRDK: Pipeline state %s, ret %u", gst_element_state_get_name(current), ret);
		if (shouldTearDown)
		{
			MW_LOG_WARN("InterfacePlayerRDK: Pipeline is not in playing/paused state, hence resetting it");
			if (rate > GST_NORMAL_PLAY_RATE)
			{
				SetTrickTearDown(true);
			}

			stopCallback(true);
			// Set the rate back to the original value if it was an recovery Stop() call
			gstPrivateContext->rate = rate;
		}
		return false;
	}

	else
	{
		/* pipeline may enter paused state even when audio decoder is not ready, check again */
		if (gstPrivateContext->audio_dec)
		{
			ret = gst_element_get_state(gstPrivateContext->audio_dec, &aud_current, &aud_pending, 0);
			if ((aud_current != GST_STATE_PLAYING && aud_current != GST_STATE_PAUSED) || ret == GST_STATE_CHANGE_FAILURE)
			{
				if (shouldTearDown)
				{
					MW_LOG_WARN("InterfacePlayerRDK: Pipeline is in playing/paused state, but audio_dec is in %s state, resetting it ret %u",
								gst_element_state_get_name(aud_current), ret);
					stopCallback(true);
					// Set the rate back to the original value if it was an recovery Stop() call
					gstPrivateContext->rate = rate;
					return false;
				}
			}
		}
		MW_LOG_MIL("InterfacePlayerRDK: Pipeline is in %s state position %f ret %d", gst_element_state_get_name(current), position, ret);
	}
	/* Disabling the flush flag to avoid */
	/* flush call again (which may cause freeze sometimes)      */
	/* from SendGstEvents() API.
	 */
	ResetGstEvents();
	MW_LOG_INFO("InterfacePlayerRDK: Pipeline flush seek - start = %f rate = %d", position, rate);
	double playRate = 1.0;
	if (eGST_MEDIAFORMAT_PROGRESSIVE == static_cast<GstMediaFormat>(m_gstConfigParam->media))
	{
		playRate = rate;
	}

	if ((stream->format == GST_FORMAT_ISO_BMFF) && (eGST_MEDIAFORMAT_PROGRESSIVE != static_cast<GstMediaFormat>(m_gstConfigParam->media)))
	{
		if ((socInterface->IsSimulatorSink() || gstPrivateContext->usingRialtoSink) && rate != GST_NORMAL_PLAY_RATE)
		{
			MW_LOG_INFO("Resetting seek position to zero");
			position = 0;
		}
	}
	if (!gst_element_seek(gstPrivateContext->pipeline, playRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
						  position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
	{
		MW_LOG_ERR("Seek failed");
		SetPendingSeek(true);
	}
	if(bAsyncModify)
	{
		socInterface->SetSinkAsync(gstPrivateContext->audio_sink, (gboolean)TRUE);
	}
	gstPrivateContext->eosSignalled = false;
	gstPrivateContext->numberOfVideoBuffersSent = 0;
	return true;
}
void InterfacePlayerRDK::SignalConnect(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data)
{
	{
		const std::lock_guard<std::mutex> lock(gstPrivateContext->mSignalVectorAccessMutex);
		auto id = g_signal_connect(instance, detailed_signal, c_handler, data);
		if(0<id)
		{
			MW_LOG_MIL("InterfacePlayerRDK: Connected %s", detailed_signal);
			GstPlayerPriv::CallbackData Identifier{instance, id, detailed_signal};
			gstPrivateContext->mCallBackIdentifiers.push_back(Identifier);
		}
		else
		{
			MW_LOG_WARN("InterfacePlayerRDK: Could not connect %s", detailed_signal);
		}
	}
	gstPrivateContext->callbackControl.enable();
}

static gboolean gstappsrc_seek(void *src, guint64 offset, void* _this)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK*)_this;
	HANDLER_CONTROL_HELPER(pInterfacePlayerRDK->gstPrivateContext->callbackControl, TRUE);
#ifdef TRACE
	MW_LOG_MIL("appsrc %p seek-signal - offset %" G_GUINT64_FORMAT, src, offset);
#endif
	return TRUE;
}
static GstMediaType gstGetMediaTypeForSource(const void  *source, const void *_this)
{
	if (source && _this)
	{
		InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK*)_this;
		for (int i = 0; i < GST_TRACK_COUNT; i++)
		{
			/* eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE, eMEDIATYPE_AUX_AUDIO */
			if (source == pInterfacePlayerRDK->gstPrivateContext->stream[i].source)
			{
				return static_cast<GstMediaType>(i);
			}
		}
		MW_LOG_WARN("unmapped source!");
	}
	else
	{
		MW_LOG_ERR("Null check failed.");
	}

	return eGST_MEDIATYPE_DEFAULT;
}
/**
 * @brief Callback for appsrc "need-data" signal
 * @param[in] source pointer to appsrc instance triggering "need-data" signal
 * @param[in] size size of data required
 * @param[in] _this pointer to InterfacePlayerRDK instance associated with the playback
 */
static void gst_need_data(void *source, guint size, void *_this)
{
	InterfacePlayerRDK* pInterfacePlayerRDK = (InterfacePlayerRDK*)_this;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	GstMediaType mediaType = gstGetMediaTypeForSource(source, pInterfacePlayerRDK);
	if (mediaType != eGST_MEDIATYPE_DEFAULT)
	{
		struct gst_media_stream *stream = &pInterfacePlayerRDK->gstPrivateContext->stream[mediaType];
		if(stream)
		{
			int media = static_cast<int>(mediaType);
			pInterfacePlayerRDK->NeedDataCb(media);
		}
		else
		{
			MW_LOG_ERR( "Null check failed." );
		}
	}
}
static void gst_enough_data(GstElement *source, void *_this)
{
	InterfacePlayerRDK* pInterfacePlayerRDK = (InterfacePlayerRDK*)_this;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	if(pInterfacePlayerRDK)
	{
		if (!pInterfacePlayerRDK->mPauseInjector) // avoid processing enough data if the downloads are already disabled.
		{
			GstMediaType mediaType = gstGetMediaTypeForSource(source, pInterfacePlayerRDK);
			if (mediaType != eGST_MEDIATYPE_DEFAULT)
			{
				struct gst_media_stream *stream = &pInterfacePlayerRDK->gstPrivateContext->stream[mediaType];
				if(stream)
				{
					int media = static_cast<int>(mediaType);
					pInterfacePlayerRDK->EnoughDataCb(media);
				}
				else
				{
					MW_LOG_ERR( "%s Null check failed.", gstGetMediaTypeName(mediaType));
				}
			}
		}
	}
	else
	{
		MW_LOG_ERR( "Null check failed." );
	}
}
void InterfacePlayerRDK::InitializeSourceForPlayer(void *PlayerInstance, void * source, GstMediaType mediaType)
{
	InterfacePlayerRDK* _this = (InterfacePlayerRDK*)PlayerInstance;
	GstCaps * caps = NULL;
	gst_media_stream *stream = &_this->gstPrivateContext->stream[mediaType];
	_this->SignalConnect(source, "need-data", G_CALLBACK(gst_need_data), _this);
	_this->SignalConnect(source, "enough-data", G_CALLBACK(gst_enough_data), _this);	/* Sets up the call back function for enough data event */
	_this->SignalConnect(source, "seek-data", G_CALLBACK(gstappsrc_seek), _this);		/* Sets up the call back function for seek data event */
	gst_app_src_set_stream_type(GST_APP_SRC(source), GST_APP_STREAM_TYPE_SEEKABLE);
	if (eGST_MEDIATYPE_VIDEO == mediaType )
	{
		int MaxGstVideoBufBytes = m_gstConfigParam->videoBufBytes;
		MW_LOG_INFO("Setting gst Video buffer max bytes to %d", MaxGstVideoBufBytes);
		g_object_set(source, "max-bytes", (guint64)MaxGstVideoBufBytes, NULL);			/* Sets the maximum video buffer bytes as per configuration*/
	}
	else if (eGST_MEDIATYPE_AUDIO == mediaType || eGST_MEDIATYPE_AUX_AUDIO == mediaType)
	{

		int MaxGstAudioBufBytes = m_gstConfigParam->audioBufBytes;
		MW_LOG_INFO("Setting gst Audio buffer max bytes to %d", MaxGstAudioBufBytes);
		g_object_set(source, "max-bytes", (guint64)MaxGstAudioBufBytes, NULL);			/* Sets the maximum audio buffer bytes as per configuration*/
	}
	g_object_set(source, "min-percent", 50, NULL);								/* Trigger the need data event when the queued bytes fall below 50% */
	/* "format" can be used to perform seek or query/conversion operation*/
	/* gstreamer.freedesktop.org recommends to use GST_FORMAT_TIME 'if you don't have a good reason to query for samples/frames' */
	g_object_set(source, "format", GST_FORMAT_TIME, NULL);
	caps = GetCaps(static_cast<GstStreamOutputFormat>(stream->format));

	if (caps != NULL)
	{
		gst_app_src_set_caps(GST_APP_SRC(source), caps);
		gst_caps_unref(caps);
	}
	else
	{
		/* If capabilities can not be established, set typefind TRUE. typefind determines the media-type of a stream and once type has been
		 * detected it sets its src pad caps to the found media type
		 */
		g_object_set(source, "typefind", TRUE, NULL);
	}
	stream->sourceConfigured = true;
}

static GstPadProbeReturn InterfacePlayerRDK_DemuxPadProbeCallbackEvent(GstPad *pad, GstPadProbeInfo *info, void* _this)
{
	if (_this)
	{
		if ((pad == ((InterfacePlayerRDK*)(_this))->gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO].demuxPad) && (((InterfacePlayerRDK*)_this)->gstPrivateContext->rate == GST_NORMAL_PLAY_RATE))
		{
			GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
			if (GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT)
			{
				GstSegment segment;
				gst_event_copy_segment(event, &segment);
				MW_LOG_TRACE("duration  %" G_GUINT64_FORMAT " start %" G_GUINT64_FORMAT " stop %" G_GUINT64_FORMAT,
							 segment.duration, segment.start, segment.stop);

				// Reset the stop value
				segment.stop = GST_CLOCK_TIME_NONE;

				// Replace the event with a new one
				GstEvent *new_event = gst_event_new_segment(&segment);
				gst_event_replace(&event, new_event);
				gst_event_unref(new_event);

				// Update the probe info with the new event
				info->data = event;
			}
		}
	}
	return GST_PAD_PROBE_OK;
}


static GstPadProbeReturn InterfacePlayerRDK_DemuxPadProbeCallback(GstPad * pad, GstPadProbeInfo * info, void* _this)
{
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
	if (_this)
	{
		// Filter audio buffers until video PTS is reached
		if (((InterfacePlayerRDK*)_this)->gstPrivateContext->filterAudioDemuxBuffers &&
			pad == ((InterfacePlayerRDK*)_this)->gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].demuxPad)
		{
			// PTS in nanoseconds
			gint64 currentPTS = (((double)((InterfacePlayerRDK*)_this)->GetVideoPTS() / (double)90000) * GST_SECOND);
			if (GST_BUFFER_PTS(buffer) < currentPTS)
			{
				MW_LOG_INFO("Dropping buffer: currentPTS=%" G_GINT64_FORMAT " buffer pts=%" G_GINT64_FORMAT, currentPTS, GST_BUFFER_PTS(buffer));
				return GST_PAD_PROBE_DROP;
			}
			else
			{
				MW_LOG_WARN("Resetting filterAudioDemuxBuffers buffer pts=%" G_GINT64_FORMAT, GST_BUFFER_PTS(buffer));
				((InterfacePlayerRDK*)_this)->gstPrivateContext->filterAudioDemuxBuffers = false;
			}
		}
	}
	return GST_PAD_PROBE_OK;
}
static GstPadProbeReturn InterfacePlayerRDK_DemuxPadProbeCallbackAny(GstPad *pad, GstPadProbeInfo *info, void *_this)
{
	GstPadProbeReturn rtn = GST_PAD_PROBE_OK;
	MW_LOG_TRACE("type %u",info->type);
	if (info->type & GST_PAD_PROBE_TYPE_BUFFER)
	{
		rtn = InterfacePlayerRDK_DemuxPadProbeCallback(pad, info, _this);
	}
	else if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
	{
		rtn = InterfacePlayerRDK_DemuxPadProbeCallbackEvent(pad, info, _this);
	}
	return rtn;
}
static void GstPlayer_OnDemuxPadAddedCb(GstElement* demux, GstPad* newPad, void* _this)
{
	InterfacePlayerRDK* pInterfacePlayerRDK = (InterfacePlayerRDK *)_this;
	if (pInterfacePlayerRDK)
	{
		GstPadProbeType mask = GST_PAD_PROBE_TYPE_INVALID;

		if (pInterfacePlayerRDK->m_gstConfigParam->seamlessAudioSwitch)
		{
			mask = GST_PAD_PROBE_TYPE_BUFFER;
		}
		if (pInterfacePlayerRDK->m_gstConfigParam->enablePTSReStamp)
		{
			//cast to Keep compiler happy
			mask = static_cast<GstPadProbeType>(static_cast<int>(mask) | static_cast<int>(GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM));
		}
		MW_LOG_TRACE("mask %u",mask);
		// We need to identify which stream the demux belongs to.
		// We can't use a CAPS based check, for use-cases such as aux-audio
		GstElement *parent = GST_ELEMENT_PARENT(demux);
		bool found = false;
		while (parent)
		{
			if (gst_StartsWith(GST_ELEMENT_NAME(parent), "playbin"))
			{
				for (int i = 0; i < GST_TRACK_COUNT; i++)
				{
					gst_media_stream *stream = &pInterfacePlayerRDK->gstPrivateContext->stream[i];
					if (parent == stream->sinkbin)
					{
						if (stream->demuxPad == NULL)
						{
							stream->demuxPad = newPad;
							stream->demuxProbeId = gst_pad_add_probe(newPad,
																	 mask,
																	 (GstPadProbeCallback)InterfacePlayerRDK_DemuxPadProbeCallbackAny,
																	 pInterfacePlayerRDK,
																	 NULL);
							MW_LOG_WARN("Added probe to qtdemux type[%d] src pad: %s", i, GST_PAD_NAME(newPad));
						}
						else
						{
							MW_LOG_WARN("Ignoring additional pad");
						}
						found = true;
					}
				}
				break;
			}
			MW_LOG_TRACE("Got Parent: %s", GST_ELEMENT_NAME(parent));
			parent = GST_ELEMENT_PARENT(parent);
		}
		if (!found)
		{
			GstCaps* caps = gst_pad_get_current_caps(newPad);
			gchar *capsStr = gst_caps_to_string(caps);
			MW_LOG_WARN("No matching stream found for demux: %s and caps: %s", GST_ELEMENT_NAME(demux), capsStr);
			g_free(capsStr);
			if (caps)
			{
				gst_caps_unref(caps);
			}
		}
	}
}
static void element_setup_cb(void *playbin, void *element, void *instance)
{
	gchar* elemName = gst_element_get_name((GstElement*)element);
	if (elemName && gst_StartsWith(elemName, "qtdemux"))
	{
		MW_LOG_WARN( "Add pad-added callback to demux:%s", elemName);
		g_signal_connect(element, "pad-added", G_CALLBACK(GstPlayer_OnDemuxPadAddedCb), instance);
	}
	g_free(elemName);
}

/**
 * @brief Initialize properties/callback of appsrc
 * @param[in] _this pointer to InterfacePlayerRDK instance associated with the playback
 * @param[in] source pointer to appsrc instance to be initialized
 * @param[in] mediaType stream type
 */
static void gstInitializeSource(void *_this, GObject *source, int iMediaType = eGST_MEDIATYPE_VIDEO)
{

	InterfacePlayerRDK* pInterfacePlayerRDK = (InterfacePlayerRDK*)_this;
	GstMediaType mediaType = (GstMediaType)iMediaType;

	gst_media_stream *stream = &pInterfacePlayerRDK->gstPrivateContext->stream[mediaType];

	pInterfacePlayerRDK->InitializeSourceForPlayer(pInterfacePlayerRDK,source, mediaType);
	stream->sourceConfigured = true;
}
/**
 * @brief Callback when source is added by playbin
 * @param[in] object a GstObject
 * @param[in] orig the object that originated the signal
 * @param[in] pspec the property that changed
 * @param[in] _this pointer to InterfacePlayerRDK instance associated with the playback
 */
static void gst_found_source(GObject * object, GObject * orig, GParamSpec * pspec, void* data )
{
	InterfacePlayerRDK* pInterfacePlayerRDK = (InterfacePlayerRDK*)data;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	GstMediaType mediaType = eGST_MEDIATYPE_DEFAULT;
	if (object == G_OBJECT(pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO].sinkbin))
	{
		MW_LOG_MIL("Found source for video");
		mediaType = eGST_MEDIATYPE_VIDEO;
	}
	else if (object == G_OBJECT(pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].sinkbin))
	{
		MW_LOG_MIL("Found source for audio");
		mediaType = eGST_MEDIATYPE_AUDIO;
	}
	else if (object == G_OBJECT(pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_AUX_AUDIO].sinkbin))
	{
		MW_LOG_MIL("Found source for auxiliary audio");
		mediaType = eGST_MEDIATYPE_AUX_AUDIO;
	}
	else if(object == G_OBJECT(pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].sinkbin))
	{
		MW_LOG_MIL("Found source for subtitle");
		mediaType = eGST_MEDIATYPE_SUBTITLE;
	}
	else
	{
		MW_LOG_WARN("found_source didn't find a valid source");
	}
	if( mediaType != eGST_MEDIATYPE_DEFAULT)
	{
		gst_media_stream *stream;
		stream = &pInterfacePlayerRDK->gstPrivateContext->stream[mediaType];
		g_object_get(orig, pspec->name, &stream->source, NULL);
		gstInitializeSource(pInterfacePlayerRDK, G_OBJECT(stream->source), mediaType);
	}
}
static void callback_element_added (GstElement * element, GstElement * source, gpointer data)
{
	InterfacePlayerRDK * pInterfacePlayerRDK = (InterfacePlayerRDK*)data;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	MW_LOG_INFO("callback_element_added: %s",GST_ELEMENT_NAME(source));
	if (element == pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_AUX_AUDIO].sinkbin)
	{
		pInterfacePlayerRDK->socInterface->SetAudioRoutingProperties(source);
	}
}
/**
 * @brief callback when the source has been created
 * @param[in] element is the pipeline
 * @param[in] source the creation of source triggered this callback
 * @param[in] data pointer to data associated with the playback
 */
static void httpsoup_source_setup (GstElement * element, GstElement * source, gpointer data)
{
	InterfacePlayerRDK * pInterfacePlayerRDK = (InterfacePlayerRDK*)data;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	if (!strcmp(GST_ELEMENT_NAME(source), "source"))
	{
		std::string networkProxyValue = pInterfacePlayerRDK->m_gstConfigParam->networkProxy;		/* Get the proxy network setting from configuration*/
		if(!networkProxyValue.empty())
		{
			g_object_set(source, "proxy", networkProxyValue.c_str(), NULL);
			MW_LOG_MIL("httpsoup -> Set network proxy '%s'", networkProxyValue.c_str());
		}
	}
	if (pInterfacePlayerRDK->m_gstConfigParam->media == eGST_MEDIAFORMAT_PROGRESSIVE)		//Setting souphttpsrc priority back to GST_RANK_PRIMARY
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature (gst_registry_get (), "souphttpsrc");
		if (pluginFeature == NULL)
		{
			MW_LOG_ERR("InterfacePlayerRDK: souphttpsrc plugin feature not available;");
		}
		else
		{
			MW_LOG_INFO("InterfacePlayerRDK: souphttpsrc plugin priority set to GST_RANK_PRIMARY");
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY );
			gst_object_unref(pluginFeature);
		}
	}
}
static GstElement* InterfacePlayerRDK_GetAppSrc(void *_this, GstMediaType mediaType)
{
	GstElement *source;
	source = gst_element_factory_make("appsrc", NULL);
	if (NULL == source)
	{
		MW_LOG_WARN("InterfacePlayerRDK_GetAppSrc Cannot create source");
		return NULL;
	}
	gstInitializeSource(_this, G_OBJECT(source), mediaType);
	return source;
}

/**
 *  @brief Callback function to get video frames
 */
GstFlowReturn InterfacePlayerRDK::InterfacePlayerRDK_OnVideoSample(GstElement* object, void *_this)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK*)_this;
#if defined(__APPLE__)
	HANDLER_CONTROL_HELPER(pInterfacePlayerRDK->gstPrivateContext->callbackControl, GST_FLOW_OK);
	if(pInterfacePlayerRDK && pInterfacePlayerRDK->gstCbExportYUVFrame)
	{
		GstSample *sample = gst_app_sink_pull_sample (GST_APP_SINK (object));
		if (sample)
		{
			int width, height;
			GstCaps *caps = gst_sample_get_caps(sample);
			GstStructure *capsStruct = gst_caps_get_structure(caps,0);
			gst_structure_get_int(capsStruct,"width",&width);
			gst_structure_get_int(capsStruct,"height",&height);
			GstBuffer *buffer = gst_sample_get_buffer(sample);
			if (buffer)
			{
				GstMapInfo map;
				if (gst_buffer_map(buffer, &map, GST_MAP_READ))
				{
					pInterfacePlayerRDK->gstCbExportYUVFrame(map.data, (int)map.size, width, height);
					gst_buffer_unmap(buffer, &map);
				}
				else
				{
					MW_LOG_ERR("buffer map failed");
				}
			}
			else
			{
				MW_LOG_ERR("buffer NULL");
			}
			gst_sample_unref(sample);
		}
		else
		{
			MW_LOG_WARN("sample NULL");
		}
	}
#endif
	return GST_FLOW_OK;
}

int InterfacePlayerRDK::SetupStream(int streamId,  void *playerInstance, std::string manifest)
{
	InterfacePlayerRDK* pInterfacePlayerRDK = (InterfacePlayerRDK*)playerInstance;
	gst_media_stream* stream = &pInterfacePlayerRDK->gstPrivateContext->stream[streamId];
	if (eGST_MEDIATYPE_SUBTITLE == streamId)
	{
		if(m_gstConfigParam->gstreamerSubsEnabled)
		{
			if (pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink)
			{
				stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(gst_element_factory_make("playbin", NULL)));
				MW_LOG_INFO("subs using rialto subtitle sink");
				GstElement* textsink = gst_element_factory_make("rialtomsesubtitlesink", NULL);
				if (textsink)
				{
					MW_LOG_INFO("Created rialtomsesubtitlesink: %s", GST_ELEMENT_NAME(textsink));
				}
				else
				{
					MW_LOG_WARN("Failed to create rialtomsesubtitlesink");
				}
				auto subtitlebin = gst_bin_new("subtitlebin");
				auto vipertransform = gst_element_factory_make("vipertransform", NULL);
				gst_bin_add_many(GST_BIN(subtitlebin),vipertransform,textsink,NULL);
				gst_element_link(vipertransform, textsink);
				gst_element_add_pad(subtitlebin, gst_ghost_pad_new("sink", gst_element_get_static_pad(vipertransform, "sink")));

				g_object_set(stream->sinkbin, "text-sink", subtitlebin, NULL);
				pInterfacePlayerRDK->gstPrivateContext->subtitle_sink = textsink;
				MW_LOG_MIL("using rialtomsesubtitlesink muted=%d sink=%p", pInterfacePlayerRDK->gstPrivateContext->subtitleMuted, pInterfacePlayerRDK->gstPrivateContext->subtitle_sink);
				g_object_set(textsink, "mute", pInterfacePlayerRDK->gstPrivateContext->subtitleMuted ? TRUE : FALSE, NULL);
			}
			else
			{
#ifdef NO_PLAYBIN
				MW_LOG_INFO("subs using subtecbin");
				stream->sinkbin = gst_element_factory_make("subtecbin", NULL);			/* Creates a new element of "subtecbin" type and returns a new GstElement */
				if (!stream->sinkbin)													/* When a new element can not be created a NULL is returned */
				{
					MW_LOG_WARN("Cannot set up subtitle subtecbin");
					return -1;
				}
				stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(stream->sinkbin));	/* Retain a counted reference to sinkbin. */
				g_object_set(G_OBJECT(stream->sinkbin), "sync", FALSE, NULL);

				stream->source = GST_ELEMENT(gst_object_ref_sink(InterfacePlayerRDK_GetAppSrc(pInterfacePlayerRDK, eGST_MEDIATYPE_SUBTITLE)));
				gst_bin_add_many(GST_BIN(pInterfacePlayerRDK->gstPrivateContext->pipeline), stream->source, stream->sinkbin, NULL);		/* Add source and sink to the current pipeline */

				if (!gst_element_link_many(stream->source, stream->sinkbin, NULL))			/* forms a GstElement link chain; linking stream->source to stream->sinkbin */
				{
					MW_LOG_ERR("Failed to link subtitle elements");
					return -1;
				}

				gst_element_sync_state_with_parent(stream->source);
				gst_element_sync_state_with_parent(stream->sinkbin);
				pInterfacePlayerRDK->gstPrivateContext->subtitle_sink = GST_ELEMENT(gst_object_ref(stream->sinkbin));
				g_object_set(stream->sinkbin, "mute", pInterfacePlayerRDK->gstPrivateContext->subtitleMuted ? TRUE : FALSE, NULL);
				return 0;
#else
				MW_LOG_INFO("subs using playbin");
				stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(gst_element_factory_make("playbin", NULL)));
				auto vipertransform = gst_element_factory_make("vipertransform", NULL);
				auto textsink = gst_element_factory_make("subtecsink", NULL);
				auto subtitlebin = gst_bin_new("subtitlebin");
				gst_bin_add_many(GST_BIN(subtitlebin), vipertransform, textsink, NULL);
				gst_element_link(vipertransform, textsink);
				gst_element_add_pad(subtitlebin, gst_ghost_pad_new("sink", gst_element_get_static_pad(vipertransform, "sink")));

				g_object_set(stream->sinkbin, "text-sink", subtitlebin, NULL);
#endif
			}
		}
	}
	else
	{
		MW_LOG_INFO("using playbin");						/* Media is not subtitle, use the generic playbin */
		stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(gst_element_factory_make("playbin", NULL)));	/* Creates a new element of "playbin" type and returns a new GstElement */

		if (m_gstConfigParam->tcpServerSink)
		{
			MW_LOG_INFO("using tcpserversink");
			GstElement* sink = gst_element_factory_make("tcpserversink", NULL);
			int tcp_port = m_gstConfigParam->tcpPort;
			// TCPServerSinkPort of 0 is treated specially and should not be incremented for audio
			if (eGST_MEDIATYPE_VIDEO == streamId)
			{
				g_object_set (G_OBJECT (sink), "port", tcp_port,"host","127.0.0.1",NULL);
				g_object_set(stream->sinkbin, "video-sink", sink, NULL);
			}
			else if (eGST_MEDIATYPE_AUDIO == streamId)
			{
				g_object_set (G_OBJECT (sink), "port", (tcp_port>0)?tcp_port+1:tcp_port,"host","127.0.0.1",NULL);
				g_object_set(stream->sinkbin, "audio-sink", sink, NULL);
			}
		}
		else if (pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink && eGST_MEDIATYPE_VIDEO == streamId)
		{
			MW_LOG_INFO("using rialtomsevideosink");
			GstElement* vidsink = gst_element_factory_make("rialtomsevideosink", NULL);
			if (vidsink)
			{
				MW_LOG_INFO("Created rialtomsevideosink: %s", GST_ELEMENT_NAME(vidsink));
				g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);				/* In the stream->sinkbin, set the video-sink property to vidsink */
				GstMediaFormat mediaFormat = (GstMediaFormat)m_gstConfigParam->media;
				if(eGST_MEDIAFORMAT_HLS == mediaFormat)
				{
					MW_LOG_INFO("setting has-drm=false for clear HLS/TS playback");
					g_object_set(vidsink, "has-drm", FALSE, NULL);
				}
				pInterfacePlayerRDK->gstPrivateContext->video_sink = vidsink;
			}
			else
			{
				MW_LOG_WARN("Failed to create rialtomsevideosink");
			}
		}
		else if (pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink && eGST_MEDIATYPE_AUDIO == streamId)
		{
			MW_LOG_INFO("using rialtomseaudiosink");
			GstElement* audSink = gst_element_factory_make("rialtomseaudiosink",NULL);
			if(audSink)
			{
				MW_LOG_INFO("Created rialtomseaudiosink : %s",GST_ELEMENT_NAME(audSink));
				g_object_set(stream->sinkbin, "audio-sink", audSink, NULL);
				pInterfacePlayerRDK->gstPrivateContext->audio_sink = audSink;
			}
			else
			{
				MW_LOG_WARN("Failed to create rialtomseaudiosink");
			}
		}
		else if (pInterfacePlayerRDK->gstPrivateContext->using_westerossink && eGST_MEDIATYPE_VIDEO == streamId)
		{
			GstElement* vidsink = pInterfacePlayerRDK->socInterface->GetVideoSink(stream->sinkbin);
			(void)vidsink;
		}

#if defined(__APPLE__)
		if( pInterfacePlayerRDK->gstCbExportYUVFrame )
		{
			if (eGST_MEDIATYPE_VIDEO == streamId)
			{
				MW_LOG_MIL("using appsink");
				GstElement* appsink = gst_element_factory_make("appsink", NULL);
				assert(appsink);
				GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", NULL);
				gst_app_sink_set_caps (GST_APP_SINK(appsink), caps);
				g_object_set (G_OBJECT(appsink), "emit-signals", TRUE, "sync", TRUE, NULL);
				pInterfacePlayerRDK->SignalConnect(appsink, "new-sample", G_CALLBACK (InterfacePlayerRDK::InterfacePlayerRDK_OnVideoSample), this);
				g_object_set(stream->sinkbin, "video-sink", appsink, NULL);
				GstObject **oldobj = (GstObject **)&pInterfacePlayerRDK->gstPrivateContext->video_sink;
				GstObject *newobj = (GstObject *)appsink;
				gst_object_replace( oldobj, newobj );
			}
		}
#endif

		if (eGST_MEDIATYPE_AUX_AUDIO == streamId)
		{
			// We need to route audio through audsrvsink
			GstElement *audiosink = gst_element_factory_make("audsrvsink", NULL); /* Creates a new element of "audsrvsink" type and returns a new GstElement */
			g_object_set(audiosink, "session-type", 2, NULL);
			g_object_set(audiosink, "session-name", "btSAP", NULL);
			g_object_set(audiosink, "session-private", TRUE, NULL);

			g_object_set(stream->sinkbin, "audio-sink", audiosink, NULL); /* In the stream->sinkbin, set the audio-sink property to audiosink */
			if (pInterfacePlayerRDK->socInterface->RequiredElementSetup())
			{
				pInterfacePlayerRDK->SignalConnect(stream->sinkbin, "element-setup", G_CALLBACK(callback_element_added), this);
			}

			MW_LOG_MIL("using audsrvsink");
		}
	}
	gst_bin_add(GST_BIN(pInterfacePlayerRDK->gstPrivateContext->pipeline), stream->sinkbin);					/* Add the stream sink to the pipeline */
	gint flags;
	g_object_get(stream->sinkbin, "flags", &flags, NULL);									/* Read the state of the current flags */
	MW_LOG_MIL("playbin flags1: 0x%x", flags); // 0x617 on settop

	bool isSub = (eGST_MEDIATYPE_SUBTITLE == streamId);
	socInterface->SetPlaybackFlags(flags, isSub);

	g_object_set(stream->sinkbin, "flags", flags, NULL); // needed?
	GstMediaFormat mediaFormat = (GstMediaFormat)m_gstConfigParam->media;

	if((mediaFormat != eGST_MEDIAFORMAT_PROGRESSIVE) || ( m_gstConfigParam->appSrcForProgressivePlayback))
	{
		g_object_set(stream->sinkbin, "uri", "appsrc://", NULL);			/* Assign uri property to appsrc, this will enable data insertion into pipeline */
		pInterfacePlayerRDK->SignalConnect(stream->sinkbin, "deep-notify::source", G_CALLBACK(gst_found_source), this);
	}
	else
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature (gst_registry_get (), "souphttpsrc");		//increasing souphttpsrc priority
		if (pluginFeature == NULL)
		{
			MW_LOG_ERR("InterfacePlayerRDK: souphttpsrc plugin feature not available;");
		}
		else
		{
			MW_LOG_INFO("InterfacePlayerRDK: souphttpsrc plugin priority set to GST_RANK_PRIMARY + 111");
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
			gst_object_unref(pluginFeature);
		}
		g_object_set(stream->sinkbin, "uri", manifest.c_str(), NULL);
		pInterfacePlayerRDK->SignalConnect(stream->sinkbin, "source-setup", G_CALLBACK(httpsoup_source_setup), this);
	}

	if (( ((mediaFormat == eGST_MEDIAFORMAT_DASH || mediaFormat == eGST_MEDIAFORMAT_HLS_MP4) &&
		   m_gstConfigParam->seamlessAudioSwitch))
		||
		(mediaFormat == eGST_MEDIAFORMAT_DASH && eGST_MEDIATYPE_VIDEO == streamId &&
		 m_gstConfigParam->enablePTSReStamp))
	{
		// Send the media_stream object so that qtdemux can be instantly mapped to media type without caps/parent check
		g_signal_connect(stream->sinkbin, "element_setup", G_CALLBACK(element_setup_cb), pInterfacePlayerRDK);
	}
	if (eGST_MEDIATYPE_VIDEO == streamId && (mediaFormat==eGST_MEDIAFORMAT_DASH || mediaFormat==eGST_MEDIAFORMAT_HLS_MP4))
	{
		// enable multiqueue
		int MaxGstVideoBufBytes = m_gstConfigParam->videoBufBytes;
		MW_LOG_INFO("Setting gst Video buffer size bytes to %d", MaxGstVideoBufBytes);
		socInterface->SetVideoBufferSize(stream->sinkbin, MaxGstVideoBufBytes);
	}
	if (eGST_MEDIATYPE_AUDIO == streamId)
	{
		socInterface->ConfigurePluginPriority();
	}
	gst_element_sync_state_with_parent(stream->sinkbin);
	return 0;
}

/**
 * @fn SendGstEvents
 * @param[in] mediaType stream type
 */
void InterfacePlayerRDK::SendGstEvents(GstMediaType mediaType, GstClockTime pts)
{
	gst_media_stream* stream = &gstPrivateContext->stream[mediaType];
	gboolean enableOverride = FALSE;
	GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");       /* Retrieves the src pad */

	if(stream->pendingSeek)
	{
		if(gstPrivateContext->seekPosition > 0)
		{
			MW_LOG_MIL("gst_element_seek_simple! mediaType:%d pts:%" GST_TIME_FORMAT " seekPosition:%" GST_TIME_FORMAT,
				mediaType, GST_TIME_ARGS(pts), GST_TIME_ARGS(gstPrivateContext->seekPosition * GST_SECOND));
			if(!gst_element_seek_simple(GST_ELEMENT(stream->source), GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, (gstPrivateContext->seekPosition * GST_SECOND)))
			{
				MW_LOG_ERR("Seek failed");
			}

		}
		stream->pendingSeek = false;
	}

	enableOverride = SendQtDemuxOverrideEvent(mediaType, pts);

	if (mediaType == eGST_MEDIATYPE_VIDEO)
	{
		// Westerossink gives position as an absolute value from segment.start. In Player's GStreamer pipeline
		// appsrc's base class - basesrc sends an additional segment event since we performed a flushing seek.
		// To figure out the new segment.start, we need to send a segment query which will be replied
		// by basesrc to get the updated segment event values.
		// When override is enabled qtdemux internally restamps and sends segment.start = 0 which is part of
		// Player's change in qtdemux so we don't need to query segment.start
		// Enabling position query based progress reporting for non-westerossink configurations.
		// Player will send a segment.start query if segmentStart is -1.
		if (m_gstConfigParam->enableGstPosQuery && (enableOverride == FALSE))
		{
			gstPrivateContext->segmentStart = -1;
		}
		else
		{
			gstPrivateContext->segmentStart = 0;
		}
	}

	if (stream->format == GST_FORMAT_ISO_BMFF)
	{
		// There is a possibility that only single protection event is queued for multiple type
		// since they are encrypted using same id. Hence check if protection event is queued for
		// other types
		GstEvent* event = gstPrivateContext->protectionEvent[mediaType];
		if (event == NULL)
		{
			// Check protection event for other types
			for (int i = 0; i < GST_TRACK_COUNT; i++)
			{
				if (i != mediaType && gstPrivateContext->protectionEvent[i] != NULL)
				{
					event = gstPrivateContext->protectionEvent[i];
					break;
				}
			}
		}
		if(event)
		{
			MW_LOG_MIL("pushing protection event! mediatype: %d", mediaType);
			if (!gst_pad_push_event(sourceEleSrcPad, gst_event_ref(event)))
			{
				MW_LOG_ERR("push protection event failed!");
			}
		}
	}
	gst_object_unref(sourceEleSrcPad);
	stream->resetPosition = false;
}

/**
 * @fn SendQtDemuxOverrideEvent
 * @param[in] mediaType stream type
 * @param[in] pts position value of buffer
 * @param[in] ptr buffer pointer
 * @param[in] len length of buffer
 * @ret TRUE if override is enabled, FALSE otherwise
 */
gboolean InterfacePlayerRDK::SendQtDemuxOverrideEvent(GstMediaType mediaType, GstClockTime pts, const void *ptr, size_t len)
{
	gst_media_stream* stream = &gstPrivateContext->stream[mediaType];
	gboolean enableOverride = false;

	if (!m_gstConfigParam->enablePTSReStamp)
	{
		enableOverride = (gstPrivateContext->rate != GST_NORMAL_PLAY_RATE);
	}
	GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");       /* Retrieves the src pad */
	if (stream->format == GST_FORMAT_ISO_BMFF && mediaType != eGST_MEDIATYPE_SUBTITLE)
	{
		/*      The below statement creates a new eventStruct with the player's name suffixed with '_override' and sets its three variables as follows:-
		 1) the variable 'enable' has datatype of G_TYPE_BOOLEAN and has value enableOverride.
		 2) the variable 'rate' has datatype of G_TYPE_FLOAT and is set to (float)playerInstance->gstPrivateContext->rate.
		 3) the variable playerName suffixed with 'player' has datatype of G_TYPE_BOOLEAN and a value of TRUE.
		 */
		std::string overrideName = mPlayerName + "_override";
		std::string player = mPlayerName + "player";

		GstStructure * eventStruct = gst_structure_new(overrideName.c_str(), "enable", G_TYPE_BOOLEAN, enableOverride, "rate", G_TYPE_FLOAT, (float)gstPrivateContext->rate, player.c_str(), G_TYPE_BOOLEAN, TRUE, "fps", G_TYPE_UINT, (guint)m_gstConfigParam->vodTrickModeFPS, NULL);
		if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
		{
			MW_LOG_ERR("Error on sending qtdemux override event");
		}
	}
	gst_object_unref(sourceEleSrcPad);
	return enableOverride;
}

/**
 *  @brief Check if audio buffers to be forwarded or not
 */
bool InterfacePlayerRDK::ForwardAudioBuffersToAux()
{
	return (gstPrivateContext->forwardAudioBuffers && gstPrivateContext->stream[eGST_MEDIATYPE_AUX_AUDIO].format != GST_FORMAT_INVALID);
}

/**
 *  @brief Get the video rectangle co-ordinates
 */
std::string InterfacePlayerRDK::GetVideoRectangle()
{
	return std::string(gstPrivateContext->videoRectangle);
}

void InterfacePlayerRDK::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	if (gstPrivateContext->usingRialtoSink)
	{
		if(gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].source)
		{
			MW_LOG_INFO("usingRialtoSink pts_offset gst_seek_simple %" PRIu64 "", pts_offset);
			GstClockTime pts = ((double)pts_offset) * GST_SECOND;
			GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, pts, nullptr)};
			if (!gst_element_send_event(gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].source, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure)))
			{
				MW_LOG_WARN("usingRialtoSink Failed to seek text-sink element");
			}
		}
	}
	else if (gstPrivateContext->subtitle_sink)
	{
		MW_LOG_INFO("pts_offset %" PRIu64 ", subtitle_sink =%p", pts_offset, gstPrivateContext->subtitle_sink);
		//We use seek_pos_seconds as an offset during seek, so we subtract that here to get an offset from zero position
		g_object_set(gstPrivateContext->subtitle_sink, "pts-offset", static_cast<std::uint64_t>(pts_offset*1000), NULL);
	}
	else
		MW_LOG_INFO("subtitle_sink is NULL");
}

/**
 * @brief Reset first frame
 */
void InterfacePlayerRDK::ResetFirstFrame(void)
{
	MW_LOG_WARN("Reset first frame");
	gstPrivateContext->firstFrameReceived = false;
}

GstPlaybackQualityStruct* InterfacePlayerRDK::GetVideoPlaybackQuality(void)
{
	GstStructure *stats= 0;
	GstElement *element;
	if((socInterface->IsPlaybackQualityFromSink()))
	{
		element = gstPrivateContext->video_sink;
	}
	else
	{
		element = gstPrivateContext->video_dec;
	}
	if( element )
	{
		g_object_get( G_OBJECT(element), "stats", &stats, NULL );
		if ( stats )
		{
			const GValue *value;
			value= gst_structure_get_value( stats, "rendered" );
			if ( value )
			{
				gstPrivateContext->playbackQuality.rendered= g_value_get_uint64( value );
			}
			value= gst_structure_get_value( stats, "dropped" );
			if ( value )
			{
				gstPrivateContext->playbackQuality.dropped= g_value_get_uint64( value );
			}
			MW_LOG_MIL("rendered %lld dropped %lld", gstPrivateContext->playbackQuality.rendered, gstPrivateContext->playbackQuality.dropped);
			gst_structure_free( stats );
			return &gstPrivateContext->playbackQuality;
		}
		else
		{
			MW_LOG_ERR("Failed to get sink stats");
		}
	}
	return NULL;
}

/**
 *  @brief Get playback position in MS
 */
long long InterfacePlayerRDK::GetPositionMilliseconds(void)
{
	long long rc = 0;
	if (gstPrivateContext->pipeline == NULL)
	{
		MW_LOG_ERR("Pipeline is NULL");
		return rc;
	}
	if (gstPrivateContext->positionQuery == NULL)
	{
		MW_LOG_ERR("Position query is NULL");
		return rc;
	}
	// Perform gstreamer query and related operation only when pipeline is playing or if deliberately put in paused
	if (gstPrivateContext->pipelineState != GST_STATE_PLAYING &&
		!(gstPrivateContext->pipelineState == GST_STATE_PAUSED && gstPrivateContext->paused) &&
		// The player should be (and probably soon will be) in the playing state so don't exit early.
		GST_STATE_TARGET(gstPrivateContext->pipeline) != GST_STATE_PLAYING)
	{
		MW_LOG_INFO("Pipeline is in %s state %s target state, paused=%d returning position as %lld", gst_element_state_get_name(gstPrivateContext->pipelineState), gst_element_state_get_name(GST_STATE_TARGET(gstPrivateContext->pipeline)), gstPrivateContext->paused, rc);
		return rc;
	}
	gst_media_stream* video = &gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO];
	// segment.start needs to be queried
	if (gstPrivateContext->segmentStart == -1)
	{
		GstQuery *segmentQuery = gst_query_new_segment(GST_FORMAT_TIME);
		// Send query to video playbin in pipeline.
		// Special case include trickplay, where only video playbin is active
		// This is to get the actual start position from video decoder/sink. If these element doesn't support the query appsrc should respond
		if (gst_element_query(video->source, segmentQuery) == TRUE)
		{
			gint64 start;
			gst_query_parse_segment(segmentQuery, NULL, NULL, &start, NULL);
			gstPrivateContext->segmentStart = GST_TIME_AS_MSECONDS(start);
			MW_LOG_MIL("InterfacePlayerRDK: Segment start: %" G_GINT64_FORMAT, gstPrivateContext->segmentStart);
		}
		else
		{
			MW_LOG_ERR("InterfacePlayerRDK: segment query failed");
		}
		gst_query_unref(segmentQuery);
	}
	if (gst_element_query(video->sinkbin,gstPrivateContext->positionQuery) == TRUE)
	{
		gint64 pos = 0;
		int rate = gstPrivateContext->rate;
		gst_query_parse_position(gstPrivateContext->positionQuery, NULL, &pos);
		if ( eGST_MEDIAFORMAT_PROGRESSIVE == static_cast<GstMediaFormat>(m_gstConfigParam->media))
		{
			rate = 1; // MP4 position query always return absolute value
		}
		if (gstPrivateContext->segmentStart > 0)
		{
			// Deduct segment.start to find the actual time of media that's played.
			rc = (GST_TIME_AS_MSECONDS(pos) - gstPrivateContext->segmentStart) * rate;
			MW_LOG_DEBUG("positionQuery pos - %" G_GINT64_FORMAT " rc - %lld SegStart -%" G_GINT64_FORMAT, GST_TIME_AS_MSECONDS(pos), rc,gstPrivateContext->segmentStart);
		}
		else
		{
			rc = GST_TIME_AS_MSECONDS(pos) * rate;
			MW_LOG_DEBUG("positionQuery pos - %" G_GINT64_FORMAT " rc - %lld" , GST_TIME_AS_MSECONDS(pos), rc);
		}
		//MW_LOG_MIL("InterfacePlayerRDK: with positionQuery pos - %" G_GINT64_FORMAT " rc - %lld", GST_TIME_AS_MSECONDS(pos), rc);
		//positionQuery is not unref-ed here, because it could be reused for future position queries
	}
	return rc;
}

/**
 *  @brief Get playback duration in MS
 */
long InterfacePlayerRDK::GetDurationMilliseconds(void)
{
	long rc = 0;
	if( gstPrivateContext->pipeline )
	{
		if( gstPrivateContext->pipelineState == GST_STATE_PLAYING || // playing
		   (gstPrivateContext->pipelineState == GST_STATE_PAUSED && gstPrivateContext->paused) ) // paused by user
		{
			gstPrivateContext->durationQuery = gst_query_new_duration(GST_FORMAT_TIME);	/*Constructs a new stream duration query object to query in the given format */
			if( gstPrivateContext->durationQuery )
			{
				gboolean res = gst_element_query(gstPrivateContext->pipeline,gstPrivateContext->durationQuery);
				if( res )
				{
					gint64 duration;
					gst_query_parse_duration(gstPrivateContext->durationQuery, NULL, &duration); /* parses the value into duration */
					rc = GST_TIME_AS_MSECONDS(duration);
				}
				else
				{
					MW_LOG_ERR("Duration query failed");
				}
				gst_query_unref(gstPrivateContext->durationQuery);		/* Decreases the refcount of the durationQuery. In this case the count will be zero, so it will be freed*/
				gstPrivateContext->durationQuery = NULL;
			}
			else
			{
				MW_LOG_WARN("Duration query is NULL");
			}
		}
		else
		{
			MW_LOG_WARN("Pipeline is in %s state", gst_element_state_get_name(gstPrivateContext->pipelineState) );
		}
	}
	else
	{
		MW_LOG_WARN("Pipeline is null");
	}
	return rc;
}
/**
 *  @brief Get video display's width and height
 */
void InterfacePlayerRDK::GetVideoSize(int &width, int &height)
{
	int x;
	int y;
	int w = 0;
	int h = 0;
	if ((4 == sscanf(gstPrivateContext->videoRectangle, "%d,%d,%d,%d", &x, &y, &w, &h)) && (w > 0) && (h > 0))
	{
		width = w;
		height = h;
	}
}

void InterfacePlayerRDK::SetSubtitleMute(bool muted)
{
	gstPrivateContext->subtitleMuted = muted;
	if (gstPrivateContext->subtitle_sink)
	{
		MW_LOG_INFO("muted %d, subtitle_sink =%p", muted, gstPrivateContext->subtitle_sink);
		g_object_set(gstPrivateContext->subtitle_sink, "mute", gstPrivateContext->subtitleMuted ? TRUE : FALSE, NULL);		/* Update the 'mute' property of the sink */
	}
	else
		MW_LOG_INFO("subtitle_sink is NULL");
}

/**
 *  @brief Set video display rectangle co-ordinates
 */
void InterfacePlayerRDK::SetVideoRectangle(int x, int y, int w, int h)
{
	int currentX = 0, currentY = 0, currentW = 0, currentH = 0;
	if (strcmp(gstPrivateContext->videoRectangle, "") != 0)
	{
		sscanf(gstPrivateContext->videoRectangle,"%d,%d,%d,%d",&currentX,&currentY,&currentW,&currentH);
	}
	//check the existing VideoRectangle co-ordinates
	if ((currentX == x) && (currentY == y) && (currentW == w) && (currentH == h))
	{
		MW_LOG_TRACE("Ignoring new co-ordinates, same as current Rect (x:%d, y:%d, w:%d, h:%d)", currentX, currentY, currentW, currentH);
		//ignore setting same rectangle co-ordinates and return
		return;
	}
	snprintf(gstPrivateContext->videoRectangle, sizeof(gstPrivateContext->videoRectangle), "%d,%d,%d,%d", x,y,w,h);
	MW_LOG_MIL("Rect %s, video_sink =%p",
			   gstPrivateContext->videoRectangle, gstPrivateContext->video_sink);
	if (m_gstConfigParam->enableRectPropertyCfg)
	{
		if (gstPrivateContext->video_sink)
		{
			g_object_set(gstPrivateContext->video_sink, "rectangle", gstPrivateContext->videoRectangle, NULL);
		}
		else
		{
			MW_LOG_WARN("Scaling not possible at this time");
		}
	}
	else
	{
		MW_LOG_WARN("New co-ordinates ignored since westerossink is used");
	}
}

/**
 *  @brief Un-pause pipeline and notify buffer end event to player
 */
bool InterfacePlayerRDK::StopBuffering(bool forceStop, bool &isPlaying)
{
	bool sendEndEvent = false;
	if (gstPrivateContext->video_dec)
	{
		int frames = -1;
		g_object_get(gstPrivateContext->video_dec,"queued_frames",(uint*)&frames,NULL);
		bool stopBuffering = forceStop;
		if( !stopBuffering )
		{
			if (frames == -1 || frames >= m_gstConfigParam->framesToQueue)
			{
				stopBuffering = true;
			}
		}

		if (!stopBuffering)
		{
			static int bufferLogCount = 0;
			if (0 == (bufferLogCount++ % 10) )
			{
				MW_LOG_WARN("Not enough data available to stop buffering, frames %d !", frames);
			}
		}

		else
		{
			MW_LOG_MIL("Enough data available to stop buffering, frames %d !", frames);
			GstState current, pending;

			if(GST_STATE_CHANGE_FAILURE == gst_element_get_state(gstPrivateContext->pipeline, &current, &pending, 0 * GST_MSECOND))
			{
				sendEndEvent = false;
			}
			else
			{
				isPlaying = true;
				if (current == GST_STATE_PLAYING)
				{
					sendEndEvent = true;
				}
			}
		}

	}
	return sendEndEvent;
}

/**
 *  @brief Retrieve the video decoder handle from pipeline
 */
unsigned long InterfacePlayerRDK::GetCCDecoderHandle()
{
	gpointer dec_handle = NULL;
	if(this->gstPrivateContext->video_dec != NULL)
	{
		MW_LOG_MIL("Querying video decoder for handle");
		socInterface->GetCCDecoderHandle(&dec_handle, this->gstPrivateContext->video_dec);
	}
	MW_LOG_MIL("video decoder handle received %p for video_dec %p", dec_handle, gstPrivateContext->video_dec);
	return (unsigned long)dec_handle;
}

/**
 * @brief Wait for source element to be configured.
 */
bool InterfacePlayerRDK::WaitForSourceSetup(GstMediaType mediaType)
{
	bool ret = false;
	gst_media_stream* stream = &gstPrivateContext->stream[mediaType];
	int timeRemaining = DEFAULT_TIMEOUT_FOR_SOURCE_SETUP;
	int waitInterval = 100;  // Polling interval (ms)

	std::unique_lock<std::mutex> lock(mSourceSetupMutex);
	MW_LOG_WARN("Source element[%p] for track[%d] not configured, waiting for setup!", stream->source, mediaType);

	while (timeRemaining >= 0)
	{
		// Wait for either a notification or timeout
		auto timeout = std::chrono::milliseconds(waitInterval);
		mSourceSetupCV.wait_for(lock, timeout);
		if (mPauseInjector)
		{
			//Playback stopped by application
			break;
		}
		else
		{
			if (stream->sourceConfigured)
			{
				MW_LOG_MIL("Source element[%p] for track[%d] setup completed!", stream->source, mediaType);
				ret = true;
				break;
			}
		}

		// Reduce remaining time
		timeRemaining -= waitInterval;
	}

	// If not successful, log timeout
	if (!ret)
	{
		MW_LOG_WARN("Wait for source element setup for track[%d] exited/timed out!", mediaType);
	}

	return ret;
}

/**
 *  @brief Forward buffer to aux pipeline
 */
void InterfacePlayerRDK::ForwardBuffersToAuxPipeline(GstBuffer *buffer)
{
	gst_media_stream *stream = &gstPrivateContext->stream[eGST_MEDIATYPE_AUX_AUDIO];
	if (!stream->sourceConfigured && stream->format != GST_FORMAT_INVALID)
	{
		bool status = WaitForSourceSetup(eGST_MEDIATYPE_AUX_AUDIO);
		if (mPauseInjector && !status)
		{
			// Buffer is not owned by us, no need to free
			return;
		}
	}

	GstBuffer *fwdBuffer = gst_buffer_new();
	if (fwdBuffer != NULL)
	{
		if (FALSE == gst_buffer_copy_into(fwdBuffer, buffer, GST_BUFFER_COPY_ALL, 0, -1))
		{
			MW_LOG_ERR("Error while copying audio buffer to auxiliary buffer!!");
			gst_buffer_unref(fwdBuffer);
			return;
		}
		//MW_LOG_TRACE("Forward audio buffer to auxiliary pipeline!!");
		GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(stream->source), fwdBuffer);
		if (ret != GST_FLOW_OK)
		{
			MW_LOG_ERR("gst_app_src_push_buffer error: %d[%s] mediaType %d", ret, gst_flow_get_name (ret), (int)eGST_MEDIATYPE_AUX_AUDIO);
			assert(false);
		}
	}
}

bool InterfacePlayerRDK::HandleVideoBufferSent()
{
	bool isFirstBuffer = (gstPrivateContext->numberOfVideoBuffersSent == 0);
	gstPrivateContext->numberOfVideoBuffersSent++;
	return isFirstBuffer;
}

void InterfacePlayerRDK::SetPlayerName(std::string name)
{
	mPlayerName = name;
}

/**
 *  @brief Inject stream buffer to gstreamer pipeline
 */
bool InterfacePlayerRDK::SendHelper(int type, const void *ptr, size_t len, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool copy, bool initFragment, bool &discontinuity, bool &notifyFirstBufferProcessed, bool &sendNewSegmentEvent, bool &resetTrickUTC, bool &firstBufferPushed)
{
	GstMediaType mediaType = static_cast<GstMediaType>(type);
	GstClockTime pts = (GstClockTime)(fpts * GST_SECOND);
	GstClockTime dts = (GstClockTime)(fdts * GST_SECOND);
	GstClockTime duration = (GstClockTime)(fDuration * 1000000000LL);
	gst_media_stream *stream = &gstPrivateContext->stream[mediaType];
	if (eGST_MEDIATYPE_SUBTITLE == mediaType && discontinuity)
	{
		MW_LOG_WARN( "[%d] Discontinuity detected - setting subtitle clock to %" GST_TIME_FORMAT " dAR %d rP %d init %d sC %d",
					mediaType,
					GST_TIME_ARGS(pts),
					!mPauseInjector,
					stream->resetPosition,
					initFragment,
					stream->sourceConfigured );
		//gst_element_seek_simple(GST_ELEMENT(stream->source), GST_FORMAT_TIME, GST_SEEK_FLAG_NONE, pts);
	}

	bool segmentEventSent = false;
	bool isFirstBuffer = stream->resetPosition;
	// Make sure source element is present before data is injected
	// If format is FORMAT_INVALID, we don't know what we are doing here
	pthread_mutex_lock(&stream->sourceLock);

	if (!stream->sourceConfigured && stream->format != GST_FORMAT_INVALID)
	{
		bool status = WaitForSourceSetup(mediaType);

		if (mPauseInjector || !status)
		{
			pthread_mutex_unlock(&stream->sourceLock);
			return false;
		}
	}
	if (isFirstBuffer)
	{
		//Send Gst Event when first buffer received after new tune, seek or period change
		SendGstEvents((GstMediaType)mediaType, pts);

		if (mediaType == eGST_MEDIATYPE_AUDIO && ForwardAudioBuffersToAux())
		{
			SendGstEvents((GstMediaType)eGST_MEDIATYPE_AUX_AUDIO, pts);
		}

		// included to fix av sync / trickmode speed issues
		// Also add check for trick-play on 1st frame.
		if (socInterface->IsPlatformSegmentReady() && sendNewSegmentEvent == true)
		{
			SendNewSegmentEvent(mediaType, pts, 0);
			segmentEventSent = true;
		}
		MW_LOG_DEBUG("mediaType[%d] SendGstEvents - first buffer received !!! initFragment: %d, pts: %" G_GUINT64_FORMAT, mediaType, initFragment, pts);

	}

	sendNewSegmentEvent = segmentEventSent;
	bool bPushBuffer = !mPauseInjector;
	if(bPushBuffer)
	{
		GstBuffer *buffer;
		gint64 pts_offset{(gint64)(fragmentPTSoffset * (-1000))};

		if(copy)
		{
			buffer = gst_buffer_new_and_alloc((guint)len);

			if (buffer)
			{
				GstMapInfo map;
				gst_buffer_map(buffer, &map, GST_MAP_WRITE);
				memcpy(map.data, ptr, len);
				gst_buffer_unmap(buffer, &map);
				GST_BUFFER_PTS(buffer) = pts;
				GST_BUFFER_DTS(buffer) = dts;
				GST_BUFFER_DURATION(buffer) = duration;
				if (mediaType == eGST_MEDIATYPE_SUBTITLE)
					GST_BUFFER_OFFSET(buffer) = pts_offset;

				MW_LOG_DEBUG("Sending segment for mediaType[%d]. pts %" G_GUINT64_FORMAT " dts %" G_GUINT64_FORMAT, mediaType, pts, dts);
				MW_LOG_DEBUG(" fragmentPTSoffset %" G_GINT64_FORMAT, pts_offset);
			}
			else
			{
				bPushBuffer = false;
			}
		}
		else
		{ // transfer
			buffer = gst_buffer_new_wrapped((gpointer)ptr,(gsize)len);

			if (buffer)
			{
				GST_BUFFER_PTS(buffer) = pts;
				GST_BUFFER_DTS(buffer) = dts;
				GST_BUFFER_DURATION(buffer) = duration;
				if (mediaType == eGST_MEDIATYPE_SUBTITLE)
					GST_BUFFER_OFFSET(buffer) = pts_offset;

				MW_LOG_INFO("Sending segment for mediaType[%d]. pts %" G_GUINT64_FORMAT " dts %" G_GUINT64_FORMAT" len:%zu init:%d discontinuity:%d dur:%" G_GUINT64_FORMAT,
							mediaType, pts, dts, len, initFragment, discontinuity,duration);

			}
			else
			{
				bPushBuffer = false;
			}
		}

		if (bPushBuffer)
		{
			if (mediaType == eGST_MEDIATYPE_AUDIO && ForwardAudioBuffersToAux())
			{
				ForwardBuffersToAuxPipeline(buffer);
			}

			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(stream->source), buffer);

			if (ret != GST_FLOW_OK)
			{
				MW_LOG_ERR("gst_app_src_push_buffer error: %d[%s] mediaType %d", ret, gst_flow_get_name (ret), (int)mediaType);
				if (ret != GST_FLOW_EOS && ret !=  GST_FLOW_FLUSHING)
				{ // an unexpected error has occurred
					if (mediaType == eGST_MEDIATYPE_SUBTITLE)
					{ // occurs sometimes when injecting subtitle fragments
						if (!stream->source)
						{
							MW_LOG_ERR("subtitle appsrc is NULL");
						}
						else if (!GST_IS_APP_SRC(stream->source))
						{
							MW_LOG_ERR("subtitle appsrc is invalid");
						}
					}
					else
					{ // if we get here, something has gone terribly wrong
						assert(0);
					}
				}
			}
			else if (stream->bufferUnderrun)
			{
				stream->bufferUnderrun = false;
			}

			// PROFILE_BUCKET_FIRST_BUFFER after successful push of first gst buffer
			if (isFirstBuffer == true && ret == GST_FLOW_OK)
				firstBufferPushed = true;
			if (!stream->firstBufferProcessed && !initFragment)
			{
				stream->firstBufferProcessed = true;
			}
		}
	}
	discontinuity = isFirstBuffer || discontinuity;
	pthread_mutex_unlock(&stream->sourceLock);
	if (isFirstBuffer)
	{
		if(!gstPrivateContext->using_westerossink)
		{
			notifyFirstBufferProcessed = true;
		}
		resetTrickUTC = socInterface->ResetTrickUTC();
	}
	if (eGST_MEDIATYPE_VIDEO == mediaType)
	{
		gstPrivateContext->numberOfVideoBuffersSent++;
	}
	return bPushBuffer;
}

void InterfacePlayerRDK::PauseInjector()
{
	std::unique_lock<std::mutex> lock(mSourceSetupMutex);
	mPauseInjector = true;
}

void InterfacePlayerRDK::ResumeInjector()
{
	std::unique_lock<std::mutex> lock(mSourceSetupMutex);
	mPauseInjector = false;
	mSourceSetupCV.notify_all();
}

/**
 *  @brief Send new segment event to pipeline
 */
void InterfacePlayerRDK::SendNewSegmentEvent(GstMediaType mediaType, GstClockTime startPts ,GstClockTime stopPts)
{
	gst_media_stream* stream = &gstPrivateContext->stream[mediaType];
	GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
	if (stream->format == GST_FORMAT_ISO_BMFF)
	{
		GstSegment segment;
		gst_segment_init(&segment, GST_FORMAT_TIME);

		segment.start = startPts;
		segment.position = 0;
		segment.rate = GST_NORMAL_PLAY_RATE;
		segment.applied_rate = GST_NORMAL_PLAY_RATE;
		if(stopPts) segment.stop = stopPts;
		if(!socInterface->IsVideoMaster())
		{
			//  notify westerossink of rate to run in Vmaster mode
			if ((GstMediaType)mediaType == eGST_MEDIATYPE_VIDEO)
				segment.applied_rate = gstPrivateContext->rate;

		}
		MW_LOG_INFO("Sending segment event for mediaType[%d]. start %" G_GUINT64_FORMAT " stop %" G_GUINT64_FORMAT" rate %f applied_rate %f", mediaType, segment.start, segment.stop, segment.rate, segment.applied_rate);
		GstEvent* event = gst_event_new_segment (&segment);
		if (!gst_pad_push_event(sourceEleSrcPad, event))
		{
			MW_LOG_ERR("gst_pad_push_event segment error");
		}
	}
	gst_object_unref(sourceEleSrcPad);
}

/**
 *  @brief Generate a protection event
 */
void InterfacePlayerRDK::QueueProtectionEvent(const std::string& formatType, const char *protSystemId, const void *initData, size_t initDataSize, int mediaType)
{
	/* There is a possibility that only single protection event is queued for multiple type since they are encrypted using same id.
	 * Don't worry if you see only one protection event queued here.
	 */
	GstMediaType type = static_cast<GstMediaType>(mediaType);
	pthread_mutex_lock(&mProtectionLock);
	if (gstPrivateContext->protectionEvent[type] != NULL)
	{
		MW_LOG_MIL("Previously cached protection event is present for type(%d), clearing!", type);
		gst_event_unref(gstPrivateContext->protectionEvent[type]);
		gstPrivateContext->protectionEvent[type] = NULL;
	}
	pthread_mutex_unlock(&mProtectionLock);

	MW_LOG_MIL("Queueing protection event for type(%d) keysystem(%s) initData(%p) initDataSize(%zu)", type, protSystemId, initData, initDataSize);

	/* Giving invalid initData into ProtectionEvent causing "GStreamer-CRITICAL" assertion error. So if the initData is valid then its good to call the ProtectionEvent further. */
	if (initData && initDataSize)
	{
		GstBuffer *pssi;

		pssi = gst_buffer_new_wrapped(PLAYER_G_MEMDUP (initData, initDataSize), (gsize)initDataSize);
		pthread_mutex_lock(&mProtectionLock);
		gstPrivateContext->protectionEvent[type] = gst_event_new_protection (protSystemId, pssi, formatType.c_str());
		pthread_mutex_unlock(&mProtectionLock);

		gst_buffer_unref (pssi);
	}
}

/**
 *  @brief Cleanup generated protection event
 */
void InterfacePlayerRDK::ClearProtectionEvent()
{
	pthread_mutex_lock(&mProtectionLock);
	for (int i = 0; i < GST_TRACK_COUNT; i++)
	{
		if(gstPrivateContext->protectionEvent[i])
		{
			MW_LOG_MIL("removing protection event! ");
			gst_event_unref (gstPrivateContext->protectionEvent[i]);
			gstPrivateContext->protectionEvent[i] = NULL;
		}
	}
	pthread_mutex_unlock(&mProtectionLock);
}
/**
 * @brief Validate pipeline state transition within a max timeout
 * @param[in] _this pointer to InterfacePlayerRDK instance
 * @param[in] stateToValidate state to be validated
 * @param[in] msTimeOut max timeout in MS
 * @retval Current pipeline state
 */
static GstState validateStateWithMsTimeout( InterfacePlayerRDK *pInterfacePlayerRDK, GstState stateToValidate, guint msTimeOut)
{
	GstState gst_current;
	GstState gst_pending;
	float timeout = 100.0;
	gint gstGetStateCnt = GST_ELEMENT_GET_STATE_RETRY_CNT_MAX;

	do
	{
		if ((GST_STATE_CHANGE_SUCCESS
			 == gst_element_get_state(pInterfacePlayerRDK->gstPrivateContext->pipeline, &gst_current, &gst_pending, timeout * GST_MSECOND))
			&& (gst_current == stateToValidate))
		{
			GST_WARNING(
						"validateStateWithMsTimeout - PIPELINE gst_element_get_state - SUCCESS : State = %d, Pending = %d",
						gst_current, gst_pending);
			return gst_current;
		}
		g_usleep (msTimeOut * 1000); // Let pipeline safely transition to required state
	}
	while ((gst_current != stateToValidate) && (gstGetStateCnt-- != 0));

	MW_LOG_ERR("validateStateWithMsTimeout - PIPELINE gst_element_get_state - FAILURE : State = %d, Pending = %d",
			   gst_current, gst_pending);
	return gst_current;
}

/**
 *  @brief To pause/play pipeline
 */
bool InterfacePlayerRDK::Pause(bool pause , bool forceStopGstreamerPreBuffering)
{
	bool retValue = true;
	if (gstPrivateContext->pipeline != NULL)
	{
		GstState nextState = pause ? GST_STATE_PAUSED : GST_STATE_PLAYING;

		if (GST_STATE_PAUSED == nextState && forceStopGstreamerPreBuffering)
		{
			/* maybe in a timing case during the playback start,
			 * gstreamer pre buffering and underflow buffering runs simultaneously and
			 * it will end up pausing the pipeline due to buffering_target_state has the value as GST_STATE_PAUSED.
			 * To avoid this case, stopping the gstreamer pre buffering logic by setting the buffering_in_progress to false
			 * and the resume play will be handled from StopBuffering once after getting enough buffer/frames.
			 */
			gstPrivateContext->buffering_in_progress = false;
		}

		GstStateChangeReturn rc = SetStateWithWarnings(gstPrivateContext->pipeline, nextState);
		if (GST_STATE_CHANGE_ASYNC == rc)
		{
			/* CID:330433 Waiting while holding lock. Sleep introduced in validateStateWithMsTimeout to prevent continuous polling when synchronizing pipeline state.
			 * Too risky to remove mutex lock. It may be replaced if approach is redesigned in future */
			/* wait a bit longer for the state change to conclude */
			if (nextState != validateStateWithMsTimeout(this,nextState, 100))
			{
				MW_LOG_ERR("InterfacePlayerRDK_Pause - validateStateWithMsTimeout - FAILED GstState %d", nextState);
			}
		}
		else if (GST_STATE_CHANGE_SUCCESS != rc)
		{
			MW_LOG_ERR("InterfacePlayerRDK_Pause - gst_element_set_state - FAILED rc %d", rc);
		}
		gstPrivateContext->buffering_target_state = nextState;
		gstPrivateContext->paused = pause;
		gstPrivateContext->pendingPlayState = false;
	}
	else
	{
		MW_LOG_WARN("Pipeline is NULL");
		retValue = false;
	}
	return retValue;
}

/**
 *  @brief Check if PTS is changing
 *  @retval true if PTS changed from lastKnown PTS or timeout hasn't expired, will optimistically return true if video-pts attribute is not available from decoder
 */
bool InterfacePlayerRDK::CheckForPTSChangeWithTimeout(long timeout)
{
	bool ret = true;
	gint64 currentPTS = GetVideoPTS();			/* Gets the currentPTS from the 'video-pts' property of the element */
	if (currentPTS != 0)
	{
		if (currentPTS != gstPrivateContext->lastKnownPTS)
		{
			MW_LOG_MIL("InterfacePlayerRDK: There is an update in PTS prevPTS:%" G_GINT64_FORMAT " newPTS: %" G_GINT64_FORMAT ,
					   gstPrivateContext->lastKnownPTS, currentPTS);
			gstPrivateContext->ptsUpdatedTimeMS = NOW_STEADY_TS_MS;			/* save a copy of the current steady clock in milliseconds */
			gstPrivateContext->lastKnownPTS = currentPTS;
		}
		else
		{
			long diff = NOW_STEADY_TS_MS - gstPrivateContext->ptsUpdatedTimeMS;
			if (diff > timeout)
			{
				MW_LOG_WARN("InterfacePlayerRDK: Video PTS hasn't been updated for %ld ms and timeout - %ld ms", diff, timeout);
				ret = false;
			}
		}
	}
	else
	{
		MW_LOG_MIL("InterfacePlayerRDK: video-pts parsed is: %" G_GINT64_FORMAT ,
				   currentPTS);
	}
	return ret;
}
/**
 *  @brief Check if cache empty for a media type
 */
bool InterfacePlayerRDK::IsCacheEmpty(int Type)
{
	GstMediaType mediaType = (GstMediaType)Type;
	bool ret = true;
	gst_media_stream *stream = &gstPrivateContext->stream[mediaType];
	if (stream->source)
	{
		guint64 cacheLevel = gst_app_src_get_current_level_bytes (GST_APP_SRC(stream->source));			/*Get the number of currently queued bytes inside stream->source)*/
		if(0 != cacheLevel)
		{
			MW_LOG_TRACE("InterfacePlayerRDK::Cache level  %" G_GUINT64_FORMAT "", cacheLevel);
			ret = false;
		}
		else
		{
			// Changed to MW_LOG_TRACE, to avoid log flooding
			// We're seeing this logged frequently during live linear playback, despite no user-facing problem.
			MW_LOG_TRACE("InterfacePlayerRDK::Cache level empty");
			if (gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun == true ||
				gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].bufferUnderrun == true)				/* Interpret bufferUnderun as cachelevel being empty */
			{
				MW_LOG_WARN("InterfacePlayerRDK::Received buffer underrun signal for video(%d) or audio(%d) previously",gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO].bufferUnderrun,
							gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].bufferUnderrun);
			}
			else
			{
				bool ptsChanged = CheckForPTSChangeWithTimeout(GST_MIN_PTS_UPDATE_INTERVAL);
				if(!ptsChanged)
				{
					//PTS hasn't changed for the timeout value
					MW_LOG_WARN("InterfacePlayerRDK: Appsrc cache is empty and PTS hasn't been updated for more than %dms and ret(%d)",
								GST_MIN_PTS_UPDATE_INTERVAL, ret);
				}
				else
				{
					ret = false;		/* Pts changing, conclude that cache is not empty */
				}
			}
		}
	}
	return ret;
}

/**
 *  @brief Reset EOS SignalledFlag
 */
void InterfacePlayerRDK::ResetEOSSignalledFlag()
{
	gstPrivateContext->eosSignalled = false;
}

/**
 *  @brief Checks to see if the pipeline is configured for specified media type
 */
bool InterfacePlayerRDK::PipelineConfiguredForMedia(int type)
{
	bool pipelineConfigured = true;

	gst_media_stream *stream = &gstPrivateContext->stream[type];
	if (stream)
	{
		pipelineConfigured = stream->sourceConfigured;
	}
	return pipelineConfigured;
}

bool InterfacePlayerRDK::GetBufferControlData(int iMediaType)
{
	bool GstWaitingForData = false;
	GstState current;
	GstState pending;
	GstMediaType mediaType = (GstMediaType)iMediaType;
	const gst_media_stream *stream = &gstPrivateContext->stream[mediaType];
	gst_element_get_state(stream->sinkbin, &current, &pending, 0);

	/* Transitions to Paused can block due to lack of data
	 ** state should match player's target play/pause state*/
	bool pipelineShouldBePlaying = !gstPrivateContext->paused;
	GstWaitingForData = ((pending == GST_STATE_PAUSED) ||
						 (pipelineShouldBePlaying && (current != GST_STATE_PLAYING)));
	if (GstWaitingForData)
	{
		MW_LOG_WARN("BufferControlExternalData %s GStreamer (current %s, %s, should be %s))",
					gstGetMediaTypeName(mediaType), gst_element_state_get_name(current),
					gst_element_state_get_name(pending),
					pipelineShouldBePlaying ? "GST_STATE_PLAYING" : "GST_STATE_PAUSED");
	}
	return GstWaitingForData;
}
bool InterfacePlayerRDK::IsStreamReady(int mediaType)
{
	bool StreamReady = false;

	const gst_media_stream *stream = &gstPrivateContext->stream[mediaType];

	StreamReady = stream->sinkbin && stream->sourceConfigured;
	return StreamReady;
}
/**
 *  @brief Signal trick mode discontinuity to gstreamer pipeline
 */
void InterfacePlayerRDK::SignalTrickModeDiscontinuity()
{

	gst_media_stream* stream = &gstPrivateContext->stream[eGST_MEDIATYPE_VIDEO];
	if (stream && (gstPrivateContext->rate != GST_NORMAL_PLAY_RATE) )
	{
		GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
		int vodTrickplayFPS = m_gstConfigParam->vodTrickModeFPS;
		GstStructure * eventStruct = gst_structure_new("aamp-tm-disc", "fps", G_TYPE_UINT, (guint)vodTrickplayFPS, NULL);
		if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
		{
			MW_LOG_WARN("Error on sending player tm-disc");
		}
		else
		{
			MW_LOG_MIL("Sent player tm-disc event");
		}
		gst_object_unref(sourceEleSrcPad);
	}
}

void InterfacePlayerRDK::EnableGstDebugLogging(std::string debugLevel)
{
	if (!debugLevel.empty())
	{
		gst_debug_set_threshold_from_string(debugLevel.c_str(), 1);
	}
}

/**
 *  @brief Process discontinuity for a stream type
 */
bool InterfacePlayerRDK::CheckDiscontinuity(int mediaType, int streamFormat , bool codecChange, bool &unblockDiscProcess, bool &shouldHaltBuffering)
{
	bool ret = false;
	GstMediaType type = (GstMediaType)mediaType;
	gst_media_stream *stream = &gstPrivateContext->stream[type];

	GstStreamOutputFormat Format = (GstStreamOutputFormat)streamFormat;
	MW_LOG_MIL("Entering InterfacePlayerRDK: type(%d) format(%d) firstBufferProcessed(%d)", (int)type, stream->format, stream->firstBufferProcessed);

	/*Handle discontinuity only if at least one buffer is pushed*/
	if (stream->format != GST_FORMAT_INVALID && stream->firstBufferProcessed == false)
	{
		MW_LOG_WARN("Discontinuity received before first buffer - ignoring");
	}
	else
	{
		MW_LOG_DEBUG("stream->format %d, stream->firstBufferProcessed %d", stream->format , stream->firstBufferProcessed);
		if(m_gstConfigParam->enablePTSReStamp && (Format == GST_FORMAT_ISO_BMFF) && ( !codecChange ))
		{
			unblockDiscProcess = true;
			ret = true;
		}
		else
		{
			if (m_gstConfigParam->enablePTSReStamp && codecChange)
			{
				MW_LOG_WARN("PTS-RESTAMP ENABLED, but we have codec change, so Signal EOS (%s).",gstGetMediaTypeName(type));
			}
			GstPlayer_SignalEOS(stream);
			// We are in buffering, but we received discontinuity, un-pause pipeline
			shouldHaltBuffering = true;
			ret = true;

			//If we have an audio discontinuity, signal subtec as well
			if ((type == eGST_MEDIATYPE_AUDIO) && (gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].source))
			{
				GstPlayer_SignalEOS(gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE]);
			}
		}
	}
	return ret;
}

/**
 *  @brief TimerAdd - add a new glib timer in thread safe manner
 */
void InterfacePlayerRDK::TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName)
{
	std::lock_guard<std::mutex> lock(gstPrivateContext->TaskControlMutex);
	if (funcPtr && user_data)
	{
		if (0 == taskId)
		{
			/* Sets the function pointed by functPtr to be called at regular intervals of repeatTimeout, supplying user_data to the function */
			taskId = g_timeout_add(repeatTimeout, funcPtr, user_data);
			MW_LOG_INFO("InterfacePlayerRDK: Added timer '%.50s', %d", (nullptr!=timerName) ? timerName : "unknown" , taskId);
		}
		else
		{
			MW_LOG_INFO("InterfacePlayerRDK: Timer '%.50s' already added, taskId=%d", (nullptr!=timerName) ? timerName : "unknown", taskId);
		}
	}
	else
	{
		MW_LOG_ERR("Bad pointer. funcPtr = %p, user_data=%p",funcPtr,user_data);
	}
}

/*
 *  @brief TimerIsRunning - Check whether timer is currently running
 */
bool InterfacePlayerRDK::TimerIsRunning(guint& taskId)
{
	std::lock_guard<std::mutex> lock(gstPrivateContext->TaskControlMutex);

	return (PLAYER_TASK_ID_INVALID != taskId);
}

/**
 * @brief IdleTaskClearFlags - clear async task id and pending flag in a thread safe manner
 *                             e.g. called when the task executes
 */
void InterfacePlayerRDK::IdleTaskClearFlags(GstTaskControlData& taskDetails)
{
	std::lock_guard<std::mutex> lock(gstPrivateContext->TaskControlMutex);
	if ( 0 != taskDetails.taskID )
	{
		MW_LOG_INFO("InterfacePlayerRDK: Clear task control flags <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	else
	{
		MW_LOG_TRACE("InterfacePlayerRDK: Task control flags were already cleared <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	taskDetails.taskIsPending = false;
	taskDetails.taskID = 0;
}

/**
 *  @brief IdleTaskAdd - add an async/idle task in a thread safe manner, assuming it is not queued
 */
bool InterfacePlayerRDK::IdleTaskAdd(GstTaskControlData& taskDetails, BackgroundTask funcPtr)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(gstPrivateContext->TaskControlMutex);

	if (0 == taskDetails.taskID)
	{
		taskDetails.taskIsPending = false;
		taskDetails.taskID = mScheduler.ScheduleTask(PlayerAsyncTaskObj(funcPtr, (void *)this));
		// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
		if(0 != taskDetails.taskID)
		{
			taskDetails.taskIsPending = true;
			ret = true;
			MW_LOG_INFO("Task '%.50s' was added with ID = %d.", taskDetails.taskName.c_str(), taskDetails.taskID);
		}
		else
		{
			MW_LOG_INFO("Task '%.50s' was not added or already ran.", taskDetails.taskName.c_str());
		}
	}
	else
	{
		MW_LOG_WARN("Task '%.50s' was already pending.", taskDetails.taskName.c_str());
	}
	return ret;
}


void InterfacePlayerRDK::FirstFrameCallback(std::function<void(GstMediaType, bool, bool, bool&, bool&)> callback)
{
	notifyFirstFrameCallback = callback;
}

void InterfacePlayerRDK::StopCallback(std::function<void(bool)> callback)
{
	stopCallback = callback;
}
void InterfacePlayerRDK::TearDownCallback(std::function<void(bool, int)> callback)
{
	tearDownCb = callback;
}

/**
 *  @brief Notify first Audio and Video frame through an idle function
 */
void InterfacePlayerRDK::NotifyFirstFrame(int mediaType)
{
	GstMediaType type = static_cast<GstMediaType>(mediaType);
	bool notifyFirstBuffer = false;
	bool audioOnly = false;
	bool requireFirstVideoFrameDisplay = false;
	if (!gstPrivateContext->firstFrameReceived && (gstPrivateContext->firstVideoFrameReceived
												   || (1 == gstPrivateContext->NumberOfTracks && (gstPrivateContext->firstAudioFrameReceived || gstPrivateContext->firstVideoFrameReceived))))
	{
		gstPrivateContext->firstFrameReceived = true;
		notifyFirstBuffer = true;
		PlayerLogManager::setLogLevel(mLOGLEVEL_WARN);				//Align with player LogTuneComplete once the first frame starts, required for prod builds
	}
	if(notifyFirstFrameCallback)
	{
		notifyFirstFrameCallback(type, notifyFirstBuffer, (!gstPrivateContext->decoderHandleNotified && PipelineSetToReady), requireFirstVideoFrameDisplay, audioOnly);
	}

	if (eGST_MEDIATYPE_VIDEO == type)
	{
		MW_LOG_MIL("InterfacePlayerRDK_OnFirstVideoFrameCallback. got First Video Frame");

		if (!gstPrivateContext->decoderHandleNotified)
		{
			gstPrivateContext->decoderHandleNotified = true;
			gstPrivateContext->firstFrameCallbackIdleTaskPending = false;
			gstPrivateContext->firstFrameCallbackIdleTaskId = mScheduler.ScheduleTask(PlayerAsyncTaskObj(IdleCallbackOnFirstFrame, (void *)this, "FirstFrameCallback"));
			// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
			if(gstPrivateContext->firstFrameCallbackIdleTaskId != PLAYER_TASK_ID_INVALID)
			{
				gstPrivateContext->firstFrameCallbackIdleTaskPending = true;
			}
		}

		IdleTaskAdd(gstPrivateContext->firstProgressCallbackIdleTask, IdleCallback);

		if (requireFirstVideoFrameDisplay)
		{
			if ( !IdleTaskAdd(gstPrivateContext->firstVideoFrameDisplayedCallbackTask, IdleCallbackFirstVideoFrameDisplayed))
			{
				MW_LOG_WARN("IdleCallbackFirstVideoFrameDisplayed was not added.");
			}
		}
		PipelineSetToReady = false;
	}
	else if (eGST_MEDIATYPE_AUDIO == type)
	{
		MW_LOG_MIL("InterfacePlayerRDK_OnAudioFirstFrameAudDecoder. got First Audio Frame");
		if (audioOnly)
		{
			if (!gstPrivateContext->decoderHandleNotified)
			{
				gstPrivateContext->decoderHandleNotified = true;
				gstPrivateContext->firstFrameCallbackIdleTaskPending = false;
				gstPrivateContext->firstFrameCallbackIdleTaskId = mScheduler.ScheduleTask(PlayerAsyncTaskObj(IdleCallbackOnFirstFrame, (void *)this, "FirstFrameCallback"));
				// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
				if(gstPrivateContext->firstFrameCallbackIdleTaskId != PLAYER_TASK_ID_INVALID)
				{
					gstPrivateContext->firstFrameCallbackIdleTaskPending = true;
				}
			}
			IdleTaskAdd(gstPrivateContext->firstProgressCallbackIdleTask, IdleCallback);
		}
	}

}

void InterfacePlayerRDK::TriggerEvent(InterfaceCB event)
{
	auto it = callbackMap.find(event);
	if (it != callbackMap.end())
	{
		it->second();
	}
	else
	{
		MW_LOG_ERR("Unknown event - No callback registered!");
	}
}
void InterfacePlayerRDK::TriggerEvent(InterfaceCB event, int data)
{
	auto it = setupStreamCallbackMap.find(event);
	if (it != setupStreamCallbackMap.end())
	{
		it->second(data);
	}
	else
	{
		MW_LOG_ERR("Unknown event - No callback registered!");
	}
}
/* @brief Check if string start with a prefix
 *
 * @retval TRUE if substring is found in bigstring
 */
bool gst_StartsWith( const char *inputStr, const char *prefix )
{
	bool rc = true;
	while( *prefix )
	{
		if( *inputStr++ != *prefix++ )
		{
			rc = false;
			break;
		}
	}
	return rc;
}

/**
 * @brief Check if gstreamer element is audio decoder
 * @param[in] name Name of the element
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval TRUE if element name is that of audio or video decoder
 */
bool GstPlayer_isVideoOrAudioDecoder(const char *name, InterfacePlayerRDK *pInterfacePlayerRDK)
{
	// The idea is to identify video or audio decoder plugin created at runtime by playbin and register to its first-frame/pts-error callbacks
	// This support is available in BCOM plugins in RDK builds and hence checking only for such plugin instances here
	// For platforms that doesnt support callback, we use GST_STATE_PLAYING state change of playbin to notify first frame to app
	bool isAudioOrVideoDecoder = false;
	bool isRialto = pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink;

	if (pInterfacePlayerRDK->socInterface->IsAudioOrVideoDecoder(name, isRialto))
	{
		isAudioOrVideoDecoder = true;
	}
	return isAudioOrVideoDecoder;
}

/**
 * @brief Check if gstreamer element is video decoder
 * @param[in] name Name of the element
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval TRUE if element name is that of the decoder
 */
bool GstPlayer_isVideoDecoder(const char* name, InterfacePlayerRDK * pInterfacePlayerRDK)
{
	bool isRialto = pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink;
	return pInterfacePlayerRDK->socInterface->IsVideoDecoder(name, isRialto);
}

#if GST_CHECK_VERSION(1,18,0)
/**
 * @brief GstPlayer_HandleInstantRateChangeSeekProbe
 * @param[in] pad pad element
 * @param[in] info Pad information
 * @param[in] data pointer to data
 */
static GstPadProbeReturn GstPlayer_HandleInstantRateChangeSeekProbe(GstPad* pad, GstPadProbeInfo* info, gpointer data)
{
	GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
	GstSegment *segment = reinterpret_cast<GstSegment*>(data);

	switch ( GST_EVENT_TYPE(event) )
	{
		case GST_EVENT_SEEK:
			break;
		case  GST_EVENT_SEGMENT:
			gst_event_copy_segment(event, segment); //intentional fall through as the variable segment is used to persist data
		default:
			MW_LOG_INFO("In default case of  GST_EVENT_TYPE in padprobeReturn");
			return GST_PAD_PROBE_OK;
	};

	gdouble rate = 1.0;
	GstSeekFlags flags = GST_SEEK_FLAG_NONE;
	gst_event_parse_seek (event, &rate, nullptr, &flags, nullptr, nullptr, nullptr, nullptr);
	MW_LOG_TRACE("rate %f segment->rate %f segment->format %d %d", rate, segment->rate, segment->format, GST_FORMAT_TIME);

	if (!!(flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE))
	{
		gdouble rateMultiplier = rate / segment->rate;
		GstEvent *rateChangeEvent =
		gst_event_new_instant_rate_change(rateMultiplier, static_cast<GstSegmentFlags>(flags));

		gst_event_set_seqnum (rateChangeEvent, gst_event_get_seqnum (event));
		GstPad *peerPad = gst_pad_get_peer(pad);

		if ( gst_pad_send_event (peerPad, rateChangeEvent) != TRUE )
			GST_PAD_PROBE_INFO_FLOW_RETURN(info) = GST_FLOW_NOT_SUPPORTED;

		gst_object_unref(peerPad);
		gst_event_unref(event);
		return GST_PAD_PROBE_HANDLED;
	}
	return GST_PAD_PROBE_OK;
}
#endif

/**
 * @brief Check if gstreamer element is video sink
 * @param[in] name Name of the element
 * @param[in] pInterfacePlayerRDK pointer to INterfacePlayerRDK instance
 * @retval TRUE if element name is that of video sink
 */
bool GstPlayer_isVideoSink(const char* name, InterfacePlayerRDK* pInterfacePlayerRDK)
{
	bool isRialto = pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink;
	return pInterfacePlayerRDK->socInterface->IsVideoSink(name, isRialto);
}

/**
 * @brief Creates a GStreamer pipeline.

 * @param pipelineName Name of the pipeline.
 * @param PipelinePriority Priority of the pipeline.

 * @return True if the pipeline is created successfully, false otherwise.
 */
bool InterfacePlayerRDK::CreatePipeline(const char *pipelineName, int PipelinePriority)
{
	bool ret = false;
	/* Destroy any existing pipeline before creating a new one */
	if (gstPrivateContext->pipeline || gstPrivateContext->bus)
	{
		DestroyPipeline();
	}
	MW_LOG_MIL("Creating gstreamer pipeline %s priority %d", pipelineName, PipelinePriority);
	gstPrivateContext->pipeline = gst_pipeline_new(pipelineName); //get it from app

	if (gstPrivateContext->pipeline)
	{
		gstPrivateContext->bus = gst_pipeline_get_bus(GST_PIPELINE(gstPrivateContext->pipeline));
		if(PipelinePriority >= 0)
		{
			gstPrivateContext->task_pool =  (GstTaskPool*)g_object_new (GST_TYPE_PLAYER_TASKPOOL, NULL);
		}
		if(gstPrivateContext->bus)
		{
			gstPrivateContext->aSyncControl.enable();
			guint busWatchId = gst_bus_add_watch(gstPrivateContext->bus, (GstBusFunc) bus_message, this);
			(void)busWatchId;
			gstPrivateContext->syncControl.enable();
			gst_bus_set_sync_handler(gstPrivateContext->bus, (GstBusSyncHandler) bus_sync_handler, this, NULL);
			gstPrivateContext->buffering_enabled = m_gstConfigParam->gstreamerBufferingBeforePlay;
			gstPrivateContext->buffering_in_progress = false;
			gstPrivateContext->buffering_timeout_cnt = DEFAULT_BUFFERING_MAX_CNT;
			gstPrivateContext->buffering_target_state = GST_STATE_NULL;
			MW_LOG_MIL("%s buffering_enabled %u", GST_ELEMENT_NAME(gstPrivateContext->pipeline), gstPrivateContext->buffering_enabled);
			if (gstPrivateContext->positionQuery == NULL)
			{

				gstPrivateContext->positionQuery = gst_query_new_position(GST_FORMAT_TIME);
				/* Construct a new position query that will used to query the 'current playback position' when needed.
				 The time base specified is in nanoseconds */
			}
			/* Use to enable the timing synchronization with gstreamer */
			gstPrivateContext->enableSEITimeCode = m_gstConfigParam->seiTimeCode;
			ret = true;
		}
		else
		{
			MW_LOG_ERR("InterfacePlayerRDK - gst_pipeline_get_bus failed");
		}
	}
	else
	{
		MW_LOG_ERR("InterfacePlayerRDK - gst_pipeline_new failed");
	}
	return ret;
}

/**
 *  @brief Gets Video PTS
 */
long long InterfacePlayerRDK::GetVideoPTS(void)
{
	gint64 currentPTS = 0;
	currentPTS = socInterface->GetVideoPts(gstPrivateContext->video_sink, gstPrivateContext->video_dec, gstPrivateContext->using_westerossink);
	return (long long)currentPTS;
}

/**
 * @brief Notifies EOS if video decoder pts is stalled
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean VideoDecoderPtsCheckerForEOS(gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *) user_data;
	gint64 currentPTS = pInterfacePlayerRDK->GetVideoPTS();                       /* Gets the currentPTS from the 'video-pts' property of the element */

	if (currentPTS == pInterfacePlayerRDK->gstPrivateContext->lastKnownPTS)
	{
		MW_LOG_MIL("PTS not changed");
		pInterfacePlayerRDK->NotifyEOS();                                                             /* Notify EOS if the PTS has not changed */
	}
	else
	{
		MW_LOG_MIL("Video PTS still moving lastKnownPTS %" G_GUINT64_FORMAT " currentPTS %" G_GUINT64_FORMAT " ##", pInterfacePlayerRDK->gstPrivateContext->lastKnownPTS, currentPTS);
	}
	pInterfacePlayerRDK->gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId = GST_TASK_ID_INVALID;
	return G_SOURCE_REMOVE;
}

/**
 * @brief Check if gstreamer element is audio sink or audio decoder
 * @param[in] name Name of the element
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval TRUE if element name is that of audio sink or audio decoder
 */
bool GstPlayer_isAudioSinkOrAudioDecoder(const char* name, InterfacePlayerRDK * pInterfacePlayerRDK)
{
	bool isRialto = pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink;
	return pInterfacePlayerRDK->socInterface->IsAudioSinkOrAudioDecoder(name, isRialto);
}


/**
 * @brief Callback invoked when facing an underflow
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 */
static void GstPlayer_OnGstBufferUnderflowCb(GstElement* object, guint arg0, gpointer arg1,
											 InterfacePlayerRDK * pInterfacePlayerRDK)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	if (pInterfacePlayerRDK->m_gstConfigParam->disableUnderflow)
	{ // optionally ignore underflow
		MW_LOG_WARN("##  [WARN] Ignored underflow, disableUnderflow config enabled ##");
	}
	else
	{
		//TODO - Handle underflow
		GstMediaType type = eGST_MEDIATYPE_DEFAULT;  //CID:89173 - Resolve Uninit
		bool isVideo = false;

		if (pInterfacePlayerRDK->socInterface->IsVideoSinkHandleErrors())
		{
			isVideo = GstPlayer_isVideoSink(GST_ELEMENT_NAME(object), pInterfacePlayerRDK);
		}
		else
		{
			isVideo = GstPlayer_isVideoDecoder(GST_ELEMENT_NAME(object), pInterfacePlayerRDK);
		}

		if (isVideo)
		{
			type = eGST_MEDIATYPE_VIDEO;
		}
		else if (GstPlayer_isAudioSinkOrAudioDecoder(GST_ELEMENT_NAME(object), pInterfacePlayerRDK))
		{
			type = eGST_MEDIATYPE_AUDIO;
		}
		else
		{
			MW_LOG_WARN("## WARNING!! Underflow message from %s not handled, unmapped underflow!", GST_ELEMENT_NAME(object));
			return;
		}

		MW_LOG_WARN("## Got Underflow message from %s type %d ##", GST_ELEMENT_NAME(object), type);
		pInterfacePlayerRDK->gstPrivateContext->stream[type].bufferUnderrun = true;

		if ((pInterfacePlayerRDK->gstPrivateContext->stream[type].eosReached) && (pInterfacePlayerRDK->gstPrivateContext->rate > 0))
		{
			if (!pInterfacePlayerRDK->gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId)
			{
				pInterfacePlayerRDK->gstPrivateContext->lastKnownPTS = pInterfacePlayerRDK->GetVideoPTS();			/* Gets the currentPTS from the 'video-pts' property of the element */
				pInterfacePlayerRDK->gstPrivateContext->ptsUpdatedTimeMS = NOW_STEADY_TS_MS;
				pInterfacePlayerRDK->gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId = g_timeout_add(GST_DELAY_BETWEEN_PTS_CHECK_FOR_EOS_ON_UNDERFLOW, VideoDecoderPtsCheckerForEOS, pInterfacePlayerRDK);
				/*g_timeout_add - Sets the function VideoDecoderPtsCheckerForEOS to be called at regular intervals*/
			}
			else
			{
				MW_LOG_WARN("ptsCheckForEosOnUnderflowIdleTask ID %d already running, ignore underflow", (int)pInterfacePlayerRDK->gstPrivateContext->ptsCheckForEosOnUnderflowIdleTaskId);
			}
		}
		else
		{
			MW_LOG_WARN("Mediatype %d underrun, when eosReached is %d", type, pInterfacePlayerRDK->gstPrivateContext->stream[type].eosReached);
		}
		if(pInterfacePlayerRDK->OnGstBufferUnderflowCb)
		{
			pInterfacePlayerRDK->OnGstBufferUnderflowCb(static_cast<int>(type));
		}
		else
		{
			MW_LOG_ERR("underflow callback not registered");
		}
#ifdef USE_EXTERNAL_STATS
		INC_RETUNE_COUNT(type); // Increment the retune count for low level AV metric
#endif
	}
}

/**
 * @brief Callback invoked a PTS error is encountered
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 */
static void GstPlayer_OnGstPtsErrorCb(GstElement *object, guint arg0, gpointer arg1,
									  InterfacePlayerRDK *pInterfacePlayerRDK)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	bool isVideo = false;
	bool isAudioSink = false;
	if (pInterfacePlayerRDK->socInterface->IsVideoSinkHandleErrors())
	{
		isVideo = GstPlayer_isVideoSink(GST_ELEMENT_NAME(object), pInterfacePlayerRDK);
	}
	else
	{
		isVideo = GstPlayer_isVideoDecoder(GST_ELEMENT_NAME(object), pInterfacePlayerRDK);
	}
	if (GstPlayer_isAudioSinkOrAudioDecoder(GST_ELEMENT_NAME(object), pInterfacePlayerRDK))
	{
		isAudioSink = true;
	}
	pInterfacePlayerRDK->OnGstPtsErrorCb(isVideo, isAudioSink);
}

/**
 * @brief Callback invoked a Decode error is encountered
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] pInterfacePlayerRDK pointer to GstPlayer instance
 */
static void GstPlayer_OnGstDecodeErrorCb(GstElement* object, guint arg0, gpointer arg1,
										 InterfacePlayerRDK * pInterfacePlayerRDK)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	long long deltaMS = NOW_STEADY_TS_MS - pInterfacePlayerRDK->gstPrivateContext->decodeErrorMsgTimeMS;
	pInterfacePlayerRDK->gstPrivateContext->decodeErrorCBCount += 1;
	if (deltaMS >= GST_MIN_DECODE_ERROR_INTERVAL)
	{
		pInterfacePlayerRDK->OnGstDecodeErrorCb(pInterfacePlayerRDK->gstPrivateContext->decodeErrorCBCount);
		pInterfacePlayerRDK->gstPrivateContext->decodeErrorMsgTimeMS = NOW_STEADY_TS_MS;
		MW_LOG_ERR("Got Decode Error message from %s total_cb=%d timeMs=%d", GST_ELEMENT_NAME(object),  pInterfacePlayerRDK->gstPrivateContext->decodeErrorCBCount, GST_MIN_DECODE_ERROR_INTERVAL);
		pInterfacePlayerRDK->gstPrivateContext->decodeErrorCBCount = 0;
#ifdef USE_EXTERNAL_STATS
		INC_DECODE_ERROR(); // Increment the decoder error for low level AV metric
#endif

	}
}


/**
 * @brief Called from the mainloop when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval FALSE if the event source should be removed.
 */
static gboolean bus_message(GstBus * bus, GstMessage * msg, InterfacePlayerRDK * pInterfacePlayerRDK)
{
	HANDLER_CONTROL_HELPER( pInterfacePlayerRDK->gstPrivateContext->aSyncControl, FALSE);
	GError *error;
	gchar *dbg_info;
	bool isPlaybinStateChangeEvent;
	BusEventData busEvent;
	busEvent.firstBufferProcessed = false;
	busEvent.setPlaybackRate = false;
	busEvent.receivedFirstFrame = false;
	switch (GST_MESSAGE_TYPE(msg))
	{
		case GST_MESSAGE_ERROR:
			gst_message_parse_error(msg, &error, &dbg_info);
			MW_LOG_ERR("GST_MESSAGE_ERROR %s: %s\n", GST_OBJECT_NAME(msg->src), error->message);
			busEvent.msgType = MESSAGE_ERROR;
			busEvent.msg = error->message;
			if(dbg_info)
			{
				busEvent.dbg_info = dbg_info;
			}
			else
			{
				busEvent.dbg_info[0] = '\0';
			}
			pInterfacePlayerRDK->busMessageCallback(busEvent);
			MW_LOG_ERR("Debug Info: %s\n", (dbg_info) ? dbg_info : "none");
			g_clear_error(&error);
			g_free(dbg_info);
			break;

		case GST_MESSAGE_WARNING:
			gst_message_parse_warning(msg, &error, &dbg_info);
			MW_LOG_ERR("GST_MESSAGE_WARNING %s: %s\n", GST_OBJECT_NAME(msg->src), error->message);
			busEvent.msgType = MESSAGE_WARNING;
			busEvent.msg = error->message;
			if(dbg_info)
			{
				busEvent.dbg_info = dbg_info;
			}
			else
			{
				busEvent.dbg_info[0] = '\0';
			}
			pInterfacePlayerRDK->busMessageCallback(busEvent);
			MW_LOG_ERR("Debug Info: %s\n", (dbg_info) ? dbg_info : "none");
			g_clear_error(&error);
			g_free(dbg_info);
			break;

		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state, pending_state;
			gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
			isPlaybinStateChangeEvent = (GST_MESSAGE_SRC(msg) == GST_OBJECT(pInterfacePlayerRDK->gstPrivateContext->pipeline));
			const gchar *srcName = GST_OBJECT_NAME(msg->src);

			busEvent.msg = srcName ? srcName : "Unknown source";
			busEvent.dbg_info = "N/A";
			busEvent.msgType = MESSAGE_STATE_CHANGE;

			if(isPlaybinStateChangeEvent || pInterfacePlayerRDK->m_gstConfigParam->gstLogging)
			{
				MW_LOG_MIL("%s %s -> %s (pending %s)",
						   GST_OBJECT_NAME(msg->src),
						   gst_element_state_get_name(old_state),
						   gst_element_state_get_name(new_state),
						   gst_element_state_get_name(pending_state));

				if(isPlaybinStateChangeEvent && pInterfacePlayerRDK->gstPrivateContext->pauseOnStartPlayback && (new_state == GST_STATE_PAUSED))
				{
					GstElement *video_sink = pInterfacePlayerRDK->gstPrivateContext->video_sink;
					const char *frame_step_on_preroll_prop = "frame-step-on-preroll";
					pInterfacePlayerRDK->gstPrivateContext->pauseOnStartPlayback = false;

					if(video_sink && (g_object_class_find_property(G_OBJECT_GET_CLASS(video_sink), frame_step_on_preroll_prop) != NULL))
					{
						MW_LOG_INFO("Setting %s property and sending step", frame_step_on_preroll_prop);
						g_object_set(G_OBJECT(video_sink), frame_step_on_preroll_prop,1, NULL);

						if(!gst_element_send_event(video_sink, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1.0, FALSE, FALSE)))
						{
							MW_LOG_ERR("error sending step event");
						}
						g_object_set(G_OBJECT(video_sink), frame_step_on_preroll_prop,0, NULL);

						if(pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink)
						{
							pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameReceived = true;
							pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_VIDEO);
						}
					}
					else
					{
						MW_LOG_WARN("%s property not present on video_sink", frame_step_on_preroll_prop);
						pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameReceived = true;
						pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_VIDEO);
					}
				}
				if(isPlaybinStateChangeEvent && new_state == GST_STATE_PLAYING)
				{
					pInterfacePlayerRDK->gstPrivateContext->pauseOnStartPlayback = false;

					busEvent.setPlaybackRate = pInterfacePlayerRDK->socInterface->SetPlatformPlaybackRate();
					if(pInterfacePlayerRDK->m_gstConfigParam->audioOnlyMode && !pInterfacePlayerRDK->gstPrivateContext->firstAudioFrameReceived && pInterfacePlayerRDK->gstPrivateContext->NumberOfTracks==1)
					{
						gst_media_stream *stream = &pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO];
						bool ret = pInterfacePlayerRDK->socInterface->AudioOnlyMode(stream->sinkbin);
						if(ret)
						{
							MW_LOG_MIL("Audio only playback detected, hence notify first frame");
							pInterfacePlayerRDK->gstPrivateContext->firstAudioFrameReceived = ret;
							pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_AUDIO);
						}
					}
					if(pInterfacePlayerRDK->m_gstConfigParam->media == eGST_MEDIAFORMAT_PROGRESSIVE)
					{
						pInterfacePlayerRDK->IdleTaskAdd(pInterfacePlayerRDK->gstPrivateContext->firstProgressCallbackIdleTask, pInterfacePlayerRDK->IdleCallback);
						// application needs to NotifyFirstBufferProcessed
						busEvent.firstBufferProcessed = true;

					}
					if(pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink)
					{
						pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameReceived = true;
						pInterfacePlayerRDK->gstPrivateContext->firstAudioFrameReceived = true;
						pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_VIDEO);
					}
					if(pInterfacePlayerRDK->gstPrivateContext->firstTuneWithWesterosSinkOff && pInterfacePlayerRDK->socInterface->NotifyVideoFirstFrame())
					{
						pInterfacePlayerRDK->gstPrivateContext->firstTuneWithWesterosSinkOff = false;
						pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameReceived = true;
						pInterfacePlayerRDK->gstPrivateContext->firstAudioFrameReceived = true;
						pInterfacePlayerRDK->NotifyFirstFrame(eGST_MEDIATYPE_VIDEO);
					}
					if(pInterfacePlayerRDK->socInterface->IsSimulatorFirstFrame())
					{
						if(!pInterfacePlayerRDK->gstPrivateContext->firstFrameReceived)
						{
							pInterfacePlayerRDK->gstPrivateContext->firstFrameReceived = true;
							busEvent.receivedFirstFrame = true;
						}
						pInterfacePlayerRDK->TriggerEvent(InterfaceCB::firstVideoFrameReceived);
						//Note: Progress event should be sent after the decoderAvailable event only.
						//BRCM platform sends progress event after InterfacePlayerRDK_OnFirstVideoFrameCallback.
						pInterfacePlayerRDK->IdleTaskAdd(pInterfacePlayerRDK->gstPrivateContext->firstProgressCallbackIdleTask, pInterfacePlayerRDK->IdleCallback);
					}
					if (pInterfacePlayerRDK->m_gstConfigParam->gstLogging)
					{
						GST_DEBUG_BIN_TO_DOT_FILE((GstBin *)pInterfacePlayerRDK->gstPrivateContext->pipeline, GST_DEBUG_GRAPH_SHOW_ALL, "myplayer");
						// output graph to .dot format which can be visualized with Graphviz tool if:
						// gstreamer is configured with --gst-enable-gst-debug
						// and "gst" is enabled in player cfg
						// and environment variable GST_DEBUG_DUMP_DOT_DIR is set to a basepath(e.g. /opt).
					}
					// First Video Frame Displayed callback for westeros-sink is initialized
					// via OnFirstVideoFrameCallback()->NotifyFirstFrame() which is more accurate
					if((!pInterfacePlayerRDK->gstPrivateContext->using_westerossink) && pInterfacePlayerRDK->mFirstFrameRequired)
					{
						pInterfacePlayerRDK->IdleTaskAdd(pInterfacePlayerRDK->gstPrivateContext->firstVideoFrameDisplayedCallbackTask, pInterfacePlayerRDK->IdleCallbackFirstVideoFrameDisplayed);
					}

				}
			}
			//this code should be handled as part of IARM modification
			if ((NULL != msg->src) && GstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK))
			{
				// This is the video decoder, send this to the output protection module
				// so it can get the source width/height
				if (GstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK))
				{
					if(PlayerExternalsInterface::IsPlayerExternalsInterfaceInstanceActive())
					{
						std::shared_ptr<PlayerExternalsInterface> pInstance = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();
						pInstance->setGstElement((GstElement *)(msg->src));
					}
				}
			}
			else if(NULL != msg->src)
			{
				if((old_state == GST_STATE_NULL && new_state == GST_STATE_READY))
				{
					if(gst_StartsWith(GST_OBJECT_NAME(msg->src), "source"))
					{
						GstPad* sourceEleSrcPad = pInterfacePlayerRDK->socInterface->GetSourcePad(GST_ELEMENT(msg->src));
						if(sourceEleSrcPad)
						{
							gst_pad_add_probe (
											   sourceEleSrcPad,
											   GST_PAD_PROBE_TYPE_EVENT_BOTH,
											   GstPlayer_HandleInstantRateChangeSeekProbe,
											   gst_segment_new(),
											   reinterpret_cast<GDestroyNotify>(gst_segment_free));
							gst_object_unref(sourceEleSrcPad);
						}
					}
				}

			}
			if((NULL != msg->src) && ((pInterfacePlayerRDK->socInterface->IsVideoSinkHandleErrors() && GstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK)) || (!pInterfacePlayerRDK->socInterface->IsVideoSinkHandleErrors() && GstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src),pInterfacePlayerRDK))) && (!pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink))
			{
				if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
				{
					pInterfacePlayerRDK->SignalConnect(msg->src, "buffer-underflow-callback",
						G_CALLBACK(GstPlayer_OnGstBufferUnderflowCb), pInterfacePlayerRDK);
					pInterfacePlayerRDK->SignalConnect(msg->src, "pts-error-callback",
													   G_CALLBACK(GstPlayer_OnGstPtsErrorCb), pInterfacePlayerRDK);
					if (!pInterfacePlayerRDK->socInterface->IsVideoSinkHandleErrors() && GstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK))
					{
						pInterfacePlayerRDK->SignalConnect(msg->src, "decode-error-callback",
														   G_CALLBACK(GstPlayer_OnGstDecodeErrorCb), pInterfacePlayerRDK);
					}
				}
			}
			if((NULL != msg->src) &&
			   ((gst_StartsWith(GST_OBJECT_NAME(msg->src), "rialtomsevideosink") == true) ||
				(gst_StartsWith(GST_OBJECT_NAME(msg->src), "rialtomseaudiosink") == true)))
			{
				if(old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
				{
					pInterfacePlayerRDK->SignalConnect(msg->src, "buffer-underflow-callback",
													   G_CALLBACK(GstPlayer_OnGstBufferUnderflowCb), pInterfacePlayerRDK);
				}
			}
			pInterfacePlayerRDK->busMessageCallback(busEvent);
		}

			break;

		case GST_MESSAGE_EOS:
			busEvent.msgType = MESSAGE_EOS;
			//strncpy(busEvent.dbg_info, "N/A", GST_ERROR_DESCRIPTION_LENGTH - 1);
			//busEvent.dbg_info[GST_ERROR_DESCRIPTION_LENGTH - 1] = '\0';
			//strncpy(busEvent.msg, "N/A", GST_ERROR_DESCRIPTION_LENGTH - 1);
			//busEvent.msg[GST_ERROR_DESCRIPTION_LENGTH - 1] = '\0';
			busEvent.msg = "N/A";
			busEvent.dbg_info = "N/A";
			pInterfacePlayerRDK->busMessageCallback(busEvent);
			MW_LOG_MIL("GST_MESSAGE_EOS");
			pInterfacePlayerRDK->NotifyEOS();
			break;

		case GST_MESSAGE_QOS:
		{
			gboolean live;
			guint64 running_time;
			guint64 stream_time;
			guint64 timestamp;
			guint64 duration;
			gst_message_parse_qos(msg, &live, &running_time, &stream_time, &timestamp, &duration);
			break;

		}
		case GST_MESSAGE_CLOCK_LOST:
			/* In this case, the current clock as selected by the pipeline has become unusable. The pipeline will select a new clock on the next PLAYING state change.
			 As per the gstreamer.desktop org, the application should set the pipeline to PAUSED and back to PLAYING when GST_MESSAGE_CLOCK_LOST is received.
			 During DASH playback (e.g. when the pipeline is torn down on transition to trickplay), this is done elsewhere. */
			MW_LOG_WARN("GST_MESSAGE_CLOCK_LOST");
			if(eGST_MEDIAFORMAT_DASH != static_cast<GstMediaFormat>(pInterfacePlayerRDK->m_gstConfigParam->media))
			{
				SetStateWithWarnings(pInterfacePlayerRDK->gstPrivateContext->pipeline, GST_STATE_PAUSED);
				SetStateWithWarnings(pInterfacePlayerRDK->gstPrivateContext->pipeline, GST_STATE_PLAYING);
			}
			break;

		case GST_MESSAGE_TAG:
			break;

		case GST_MESSAGE_RESET_TIME:
			GstClockTime running_time;
			gst_message_parse_reset_time (msg, &running_time);
			MW_LOG_TRACE("GST_MESSAGE_RESET_TIME %llu\n", (unsigned long long)running_time);
			break;

		case GST_MESSAGE_STREAM_STATUS:
		case GST_MESSAGE_ELEMENT:	// can be used to collect pts, dts, pid
		case GST_MESSAGE_DURATION:
		case GST_MESSAGE_LATENCY:
			break;
		case GST_MESSAGE_NEW_CLOCK:
			MW_LOG_DEBUG("GST_MESSAGE_NEW_CLOCK element:%s", GST_OBJECT_NAME(msg->src));
			break;

		case GST_MESSAGE_APPLICATION:
		{
			const GstStructure *msgS;
			msgS = gst_message_get_structure (msg);
			gchar *structureStr = gst_structure_to_string(msgS);
			if(structureStr)
			{
				busEvent.msg = structureStr;
				g_free(structureStr);
			}
			else
			{
				busEvent.msg[0] = '\0';
			}
			busEvent.msgType = MESSAGE_APPLICATION;
			//strncpy(busEvent.dbg_info, "N/A", sizeof(busEvent.dbg_info) - 1);
			//busEvent.dbg_info[sizeof(busEvent.dbg_info) - 1] = '\0';
			busEvent.dbg_info = "N/A";
			pInterfacePlayerRDK->busMessageCallback(busEvent);
		}
			break;
		default:
			MW_LOG_WARN("msg type %s not supported", gst_message_type_get_name(msg->type));
			break;
	}
	return TRUE;
}

/**
 *@brief Set playback rate to audio/video sinks
 */

bool InterfacePlayerRDK::SetPlayBackRate(double rate)
{
	bool ret = false;
	std::vector<GstElement*> sources;
	MW_LOG_TRACE("InterfacePlayerRDK: gst_event_new_instant_rate_change: %f ...V6", rate);
	for (int iTrack = 0; iTrack < GST_TRACK_COUNT; iTrack++)
	{
		if (iTrack != static_cast<int>(eGST_MEDIATYPE_SUBTITLE) && gstPrivateContext->stream[iTrack].source != nullptr)
		{
			sources.push_back(gstPrivateContext->stream[iTrack].source);
		}
	}
	ret = socInterface->SetPlaybackRate(sources, gstPrivateContext->pipeline, rate, gstPrivateContext->video_dec,gstPrivateContext->audio_dec);
	return ret;
}

/**
 *  @brief Set audio volume
 */
void InterfacePlayerRDK::SetAudioVolume(int volume)
{
	gstPrivateContext->audioVolume = static_cast<double>(volume) / 100.0;
}

/**
 *  @brief Set audio volume or mute
 */
void InterfacePlayerRDK::SetVolumeOrMuteUnMute(void)
{
	const std::lock_guard<std::mutex> lock(gstPrivateContext->volumeMuteMutex);
	GstElement *gSource = nullptr;
	const char *mutePropertyName = nullptr;
	const char *volumePropertyName = nullptr;
	bool isSinkBinVolume = false;
	if (gstPrivateContext->usingRialtoSink)
	{
		gSource = gstPrivateContext->audio_sink;
		mutePropertyName = "mute";
		volumePropertyName = "volume";
	}

	else
	{
		socInterface->SetAudioProperty(&volumePropertyName, &mutePropertyName, isSinkBinVolume);
		if(isSinkBinVolume)
		{
			//some platforms sets volume/mute property on sinkbin rather then audio sink
			gSource = gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].sinkbin;
		}
		if (nullptr == gSource)
		{
			gSource = gstPrivateContext->audio_sink;
		}
	}
	MW_LOG_MIL("volume == %lf muted == %s", gstPrivateContext->audioVolume,gstPrivateContext->audioMuted ? "true" : "false");
	if (nullptr != gSource)
	{
		if (nullptr != mutePropertyName)
		{
			/* Muting the audio decoder in general to avoid audio passthrough in expert mode for locked channel */
			if (0 == gstPrivateContext->audioVolume)
			{
				MW_LOG_MIL("Audio Muted");
				g_object_set(gSource, mutePropertyName, true, NULL);
				gstPrivateContext->audioMuted = true;
			}
			else if (gstPrivateContext->audioMuted)
			{
				MW_LOG_MIL("Audio Unmuted after a Mute");
				g_object_set(gSource, mutePropertyName, false, NULL);
				gstPrivateContext->audioMuted = false;
			}
			else
			{
				// Deliberately left empty
			}
		}
		if ((nullptr != volumePropertyName) && (false == gstPrivateContext->audioMuted))
		{
			MW_LOG_MIL("Setting Volume %f using \"%s\" property",gstPrivateContext->audioVolume, volumePropertyName);
			g_object_set(gSource, volumePropertyName, gstPrivateContext->audioVolume, NULL);
		}
	}
	else
	{
		MW_LOG_WARN("No element to set volume/mute");
	}
}

void type_check_instance(const char * str, GstElement * elem)
{
	MW_LOG_MIL("%s %p type_check %d", str, elem, G_TYPE_CHECK_INSTANCE (elem));
}

static gboolean buffering_timeout (gpointer data)
{
	InterfacePlayerRDK * pInterfacePlayerRDK = (InterfacePlayerRDK *) data;
	bool isBufferingTimeoutConditionMet = false;
	bool isRateCorrectionDefaultOnPlaying = false;
	bool isPlayerReady = false;

	if(pInterfacePlayerRDK && pInterfacePlayerRDK->gstPrivateContext)
	{
		if (pInterfacePlayerRDK->gstPrivateContext->buffering_in_progress)
		{
			int frames = -1;
			if (pInterfacePlayerRDK->gstPrivateContext->video_dec)
			{
				g_object_get(pInterfacePlayerRDK->gstPrivateContext->video_dec,"queued_frames",(uint*)&frames,NULL);
				MW_LOG_DEBUG("queued_frames: %d", frames);
			}
			GstMediaFormat mediaFormatRet;
			mediaFormatRet = (GstMediaFormat)pInterfacePlayerRDK->m_gstConfigParam->media;
			/* Disable re-tune on buffering timeout for DASH as unlike HLS,
			 * DRM key acquisition can end after injection, and buffering is not expected
			 * to be completed by the 1 second timeout
			 */
			if (G_UNLIKELY(((mediaFormatRet != eGST_MEDIAFORMAT_DASH) && (mediaFormatRet != eGST_MEDIAFORMAT_PROGRESSIVE) && (mediaFormatRet != eGST_MEDIAFORMAT_HLS_MP4))
						   && (pInterfacePlayerRDK->gstPrivateContext->buffering_timeout_cnt == 0) && (pInterfacePlayerRDK->gstPrivateContext->numberOfVideoBuffersSent > 0)))
			{
				MW_LOG_WARN("numberOfVideoBuffersSent %d frames %i", pInterfacePlayerRDK->gstPrivateContext->numberOfVideoBuffersSent, frames);
				isBufferingTimeoutConditionMet = true;
				pInterfacePlayerRDK->gstPrivateContext->buffering_in_progress = false;
				pInterfacePlayerRDK->DumpDiagnostics();
				// application can schedule a retune based on isBufferingTimeoutConditionMet
			}
			else if (frames == -1 || frames >= pInterfacePlayerRDK->m_gstConfigParam->framesToQueue || pInterfacePlayerRDK->gstPrivateContext->buffering_timeout_cnt-- == 0)
			{
				MW_LOG_MIL("Set pipeline state to %s - buffering_timeout_cnt %u  frames %i",
						gst_element_state_get_name(pInterfacePlayerRDK->gstPrivateContext->buffering_target_state), (pInterfacePlayerRDK->gstPrivateContext->buffering_timeout_cnt+1), frames);
				SetStateWithWarnings (pInterfacePlayerRDK->gstPrivateContext->pipeline, pInterfacePlayerRDK->gstPrivateContext->buffering_target_state);
				isRateCorrectionDefaultOnPlaying = pInterfacePlayerRDK->socInterface->SetRateCorrection();
				pInterfacePlayerRDK->gstPrivateContext->buffering_in_progress = false;
				isPlayerReady = true;
			}
		}
		if (!pInterfacePlayerRDK->gstPrivateContext->buffering_in_progress)
		{
			//reset timer id after buffering operation is completed
			pInterfacePlayerRDK->gstPrivateContext->bufferingTimeoutTimerId = GST_TASK_ID_INVALID;
		}

		if(pInterfacePlayerRDK->OnBuffering_timeoutCb)
		{
			pInterfacePlayerRDK->OnBuffering_timeoutCb(isBufferingTimeoutConditionMet, isRateCorrectionDefaultOnPlaying, isPlayerReady);
		}
		return pInterfacePlayerRDK->gstPrivateContext->buffering_in_progress;
	}
	else
	{
		return false;
	}
}

/**
 *  @brief Set video zoom
 */
void InterfacePlayerRDK::SetVideoZoom(int zoom_mode)
{
	MW_LOG_MIL(" SetVideoZoom :: ZoomMode %d, video_sink =%p",zoom_mode, gstPrivateContext->video_sink);

	gstPrivateContext->zoom = static_cast<GstVideoZoomMode>(zoom_mode);
	if ((gstPrivateContext->video_sink) && (!gstPrivateContext->usingRialtoSink))
	{
		g_object_set(gstPrivateContext->video_sink, "zoom-mode", zoom_mode, NULL);
	}
	else
	{
		MW_LOG_WARN("InterfacePlayerRDK not setting video zoom");
	}
}

/**
 * @brief Set video mute
 */
void InterfacePlayerRDK::SetVideoMute(bool muted)
{
	MW_LOG_INFO("muted=%d video_sink =%p", muted, gstPrivateContext->video_sink);
	gstPrivateContext->videoMuted = muted;
	if (gstPrivateContext->video_sink)
	{
		g_object_set(gstPrivateContext->video_sink, "show-video-window", !gstPrivateContext->videoMuted, NULL);	/* videoMuted to true implies setting the 'show-video-window' to false */
	}
	else
	{
		MW_LOG_INFO("InterfacePlayerRDK not setting video mute");
	}
}

/**
 * @brief Set the text style of the subtitle to the options passed
 */
bool InterfacePlayerRDK::SetTextStyle(const std::string &options)
{
	bool ret = false;
	if (gstPrivateContext->subtitle_sink)
	{
		TextStyleAttributes textStyleAttributes;
		uint32_t attributesMask = 0;
		attributesType attributesValues = {0};
		if (textStyleAttributes.getAttributes(options, attributesValues, attributesMask) == 0)
		{
			if (attributesMask)
			{
				GstStructure *attributes = gst_structure_new ("Attributes",
															  "font_color", G_TYPE_UINT, attributesValues[TextStyleAttributes::FONT_COLOR_ARR_POSITION],
															  "background_color", G_TYPE_UINT, attributesValues[TextStyleAttributes::BACKGROUND_COLOR_ARR_POSITION],
															  "font_opacity", G_TYPE_UINT, attributesValues[TextStyleAttributes::FONT_OPACITY_ARR_POSITION],
															  "background_opacity", G_TYPE_UINT, attributesValues[TextStyleAttributes::BACKGROUND_OPACITY_ARR_POSITION],
															  "font_style", G_TYPE_UINT, attributesValues[TextStyleAttributes::FONT_STYLE_ARR_POSITION],
															  "font_size", G_TYPE_UINT, attributesValues[TextStyleAttributes::FONT_SIZE_ARR_POSITION],
															  "window_color", G_TYPE_UINT, attributesValues[TextStyleAttributes::WIN_COLOR_ARR_POSITION],
															  "window_opacity", G_TYPE_UINT, attributesValues[TextStyleAttributes::WIN_OPACITY_ARR_POSITION],
															  "edge_type", G_TYPE_UINT, attributesValues[TextStyleAttributes::EDGE_TYPE_ARR_POSITION],
															  "edge_color", G_TYPE_UINT, attributesValues[TextStyleAttributes::EDGE_COLOR_ARR_POSITION],
															  "attribute_mask", G_TYPE_UINT, attributesMask,
															  NULL);
				g_object_set(gstPrivateContext->subtitle_sink, "attribute-values", attributes, NULL);
				gst_structure_free (attributes);
			}
		}
		ret = true;
	}
	else
	{
		MW_LOG_INFO("InterfacePlayerRDK: subtitle sink not set");
	}
	return ret;
}

/**
 * @brief Callback invoked after receiving the SEI Time Code information
 * @param[in] object pointer to element raising the callback
 * @param[in] hours Hour value of the SEI Timecode
 * @param[in] minutes Minute value of the SEI Timecode
 * @param[in] seconds Second value of the SEI Timecode
 * @param[in] user_data pointer to InterfacePlayerRDK instance
 */
static void GstPlayer_redButtonCallback(GstElement* object, guint hours, guint minutes, guint seconds, gpointer user_data)
{
	InterfacePlayerRDK *pInterfacePlayerRDK = (InterfacePlayerRDK *)user_data;
	if (pInterfacePlayerRDK)
	{
		HANDLER_CONTROL_HELPER_CALLBACK_VOID();
		char buffer[16];
		snprintf(buffer,16,"%d:%d:%d",hours,minutes,seconds);
		if(pInterfacePlayerRDK->OnHandleRedButtonCallback)
		{
			pInterfacePlayerRDK->OnHandleRedButtonCallback(buffer);
		}
	}
}

/**
 * @brief Invoked synchronously when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @retval GST_BUS_PASS to pass the message to the async queue
 */
static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * msg, InterfacePlayerRDK * pInterfacePlayerRDK)
{
	HANDLER_CONTROL_HELPER( pInterfacePlayerRDK->gstPrivateContext->syncControl, GST_BUS_PASS);
	switch(GST_MESSAGE_TYPE(msg))
	{
		case GST_MESSAGE_STATE_CHANGED:
			GstState old_state, new_state;
			gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);

			if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pInterfacePlayerRDK->gstPrivateContext->pipeline))
			{
				pInterfacePlayerRDK->gstPrivateContext->pipelineState = new_state;
			}
			/* Moved the below code block from bus_message() async handler to bus_sync_handler()
			 * to avoid a timing case crash when accessing wrong video_sink element after it got deleted during pipeline reconfigure on codec change in mid of playback.
			 */
			if (new_state == GST_STATE_PAUSED && old_state == GST_STATE_READY)
			{
				if (GstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK))
				{ // video scaling patch
					/*
					 brcmvideosink doesn't sets the rectangle property correct by default
					 gst-inspect-1.0 brcmvideosink
					 g_object_get(pInterfacePlayerRDK->privateContext->pipeline, "video-sink", &videoSink, NULL); - reports NULL
					 note: alternate "window-set" works as well
					 */
					gst_object_replace((GstObject **)&pInterfacePlayerRDK->gstPrivateContext->video_sink, msg->src);

					if (pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink)
					{
						if (pInterfacePlayerRDK->m_gstConfigParam->enableRectPropertyCfg)
						{
							MW_LOG_MIL("InterfacePlayerRDK - using %s, setting cached rectangle(%s)",
								GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK->gstPrivateContext->videoRectangle);
							g_object_set(msg->src, "rectangle", pInterfacePlayerRDK->gstPrivateContext->videoRectangle, NULL);
						}
						MW_LOG_MIL("InterfacePlayerRDK - using %s, setting cached video mute(%d)",
							GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK->gstPrivateContext->videoMuted);
						g_object_set(msg->src, "show-video-window", !pInterfacePlayerRDK->gstPrivateContext->videoMuted, NULL);
					}
					else if ((pInterfacePlayerRDK->gstPrivateContext->using_westerossink) && !(pInterfacePlayerRDK->m_gstConfigParam->enableRectPropertyCfg))
					{
						MW_LOG_MIL("InterfacePlayerRDK - using westerossink, setting cached video mute(%d) and zoom(%d)",
							pInterfacePlayerRDK->gstPrivateContext->videoMuted, pInterfacePlayerRDK->gstPrivateContext->zoom);
						g_object_set(msg->src, "zoom-mode", pInterfacePlayerRDK->gstPrivateContext->zoom, NULL );
						g_object_set(msg->src, "show-video-window", !pInterfacePlayerRDK->gstPrivateContext->videoMuted, NULL);
					}
					else
					{
						MW_LOG_MIL("InterfacePlayerRDK setting cached rectangle(%s), video mute(%d) and zoom(%d)",
							pInterfacePlayerRDK->gstPrivateContext->videoRectangle,
							pInterfacePlayerRDK->gstPrivateContext->videoMuted,
							pInterfacePlayerRDK->gstPrivateContext->zoom);
						g_object_set(msg->src, "rectangle", pInterfacePlayerRDK->gstPrivateContext->videoRectangle, NULL);
						g_object_set(msg->src, "zoom-mode", pInterfacePlayerRDK->gstPrivateContext->zoom, NULL );
						g_object_set(msg->src, "show-video-window", !pInterfacePlayerRDK->gstPrivateContext->videoMuted, NULL);
					}
				}
				else
				{
					if((gst_StartsWith(GST_OBJECT_NAME(msg->src), "rialtomseaudiosink")))
					{
						gst_object_replace((GstObject **)&pInterfacePlayerRDK->gstPrivateContext->audio_sink, msg->src);
						pInterfacePlayerRDK->SetVolumeOrMuteUnMute();
					}
					else
					{
						bool status = pInterfacePlayerRDK->socInterface->ConfigureAudioSink(&pInterfacePlayerRDK->gstPrivateContext->audio_sink, msg->src, pInterfacePlayerRDK->m_gstConfigParam->audioDecoderStreamSync);
						if(status)
						{
							pInterfacePlayerRDK->SetVolumeOrMuteUnMute();
						}
					}
				}

			}
			if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
			{
				if ((NULL != msg->src) && GstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK))
				{
					if (GstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK))
					{
						gst_object_replace((GstObject **)&pInterfacePlayerRDK->gstPrivateContext->video_dec, msg->src);
						type_check_instance("bus_sync_handle: video_dec ", pInterfacePlayerRDK->gstPrivateContext->video_dec);
						pInterfacePlayerRDK->SignalConnect(pInterfacePlayerRDK->gstPrivateContext->video_dec, "first-video-frame-callback",
									G_CALLBACK(GstPlayer_OnFirstVideoFrameCallback), pInterfacePlayerRDK);
						pInterfacePlayerRDK->socInterface->SetDecodeError(msg->src);
					}
					else
					{
						gst_object_replace((GstObject **)&pInterfacePlayerRDK->gstPrivateContext->audio_dec, msg->src);
						type_check_instance("bus_sync_handle: audio_dec ", pInterfacePlayerRDK->gstPrivateContext->audio_dec);

						if(pInterfacePlayerRDK->socInterface->HasFirstAudioFrameCallback())
						{
							pInterfacePlayerRDK->SignalConnect(msg->src, "first-audio-frame-callback",
															   G_CALLBACK(GstPlayer_OnAudioFirstFrameAudDecoder), pInterfacePlayerRDK);
						}
						int trackId = pInterfacePlayerRDK->gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].trackId;
						if (trackId >= 0) /** AC4 track selected **/
						{
							pInterfacePlayerRDK->socInterface->SetAC4Tracks(GST_ELEMENT(msg->src), trackId);
						}

					}
				}
				if ((NULL != msg->src) && GstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), pInterfacePlayerRDK) && (!pInterfacePlayerRDK->gstPrivateContext->usingRialtoSink))
				{
					if(pInterfacePlayerRDK->gstPrivateContext->enableSEITimeCode)
					{
						g_object_set(msg->src, "enable-timecode", 1, NULL);
						pInterfacePlayerRDK->SignalConnect(msg->src, "timecode-callback",
														   G_CALLBACK(GstPlayer_redButtonCallback), pInterfacePlayerRDK);
					}
					pInterfacePlayerRDK->socInterface->SetFreerunThreshold(msg->src);
				}
				if(!pInterfacePlayerRDK->socInterface->HasFirstAudioFrameCallback())
				{
					if ((NULL != msg->src) && gst_StartsWith(GST_OBJECT_NAME(msg->src), "rtkaudiosink"))
					{
						pInterfacePlayerRDK->SignalConnect(msg->src, "first-audio-frame",
														   G_CALLBACK(GstPlayer_OnAudioFirstFrameAudDecoder), pInterfacePlayerRDK);
					}
				}
				if ((NULL != msg->src) &&
					(gst_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNamePR) == true ||
					 gst_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameWV) == true ||
					 gst_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameCK) == true ||
					 gst_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameVMX) == true))
				{
					MW_LOG_MIL("InterfacePlayerRDK setting encrypted player (%p) instance for %s decryptor", pInterfacePlayerRDK->mEncrypt, GST_OBJECT_NAME(msg->src));
					GValue val = { 0, };
					g_value_init(&val, G_TYPE_POINTER);
					g_value_set_pointer(&val, (gpointer) pInterfacePlayerRDK->mEncrypt); // encryption is being passed by player
					g_object_set_property(G_OBJECT(msg->src), pInterfacePlayerRDK->mPlayerName.c_str(), &val);
				}
			}
			break;

		case GST_MESSAGE_NEED_CONTEXT:

			const gchar* contextType;
			gst_message_parse_context_type(msg, &contextType);
			if (!g_strcmp0(contextType, "drm-preferred-decryption-system-id"))
			{
				MW_LOG_MIL("Setting %s as preferred drm",pInterfacePlayerRDK->mDrmSystem);
				GstContext* context = gst_context_new("drm-preferred-decryption-system-id", FALSE);
				GstStructure* contextStructure = gst_context_writable_structure(context);
				gst_structure_set(contextStructure, "decryption-system-id", G_TYPE_STRING, pInterfacePlayerRDK->mDrmSystem,  NULL);
				gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
			}

			break;
		case GST_MESSAGE_ASYNC_DONE:

			MW_LOG_INFO("Received GST_MESSAGE_ASYNC_DONE message");
			if (pInterfacePlayerRDK->gstPrivateContext->buffering_in_progress)
			{
				pInterfacePlayerRDK->gstPrivateContext->bufferingTimeoutTimerId = g_timeout_add_full(BUFFERING_TIMEOUT_PRIORITY, DEFAULT_BUFFERING_TO_MS, buffering_timeout, pInterfacePlayerRDK, NULL);
			}

			break;

		case GST_MESSAGE_STREAM_STATUS:

			if(pInterfacePlayerRDK->gstPrivateContext->task_pool)
			{
				GstStreamStatusType type;
				GstElement *owner;
				const GValue *val;
				GstTask *task = NULL;
				gst_message_parse_stream_status (msg, &type, &owner);
				val = gst_message_get_stream_status_object (msg);
				if (G_VALUE_TYPE (val) == GST_TYPE_TASK)
				{
					task =(GstTask*) g_value_get_object (val);
				}
				switch (type)
				{
					case GST_STREAM_STATUS_TYPE_CREATE:
						if (task && pInterfacePlayerRDK->gstPrivateContext->task_pool)
						{
							gst_task_set_pool(task, pInterfacePlayerRDK->gstPrivateContext->task_pool);
						}
						break;

					default:
						break;
				}
			}

			break;

		default:
			break;

	}
	return GST_BUS_PASS;							/* pass the message to the async queue */
}


/**
 * @brief Notify EOS to core player asynchronously if required.
 * @note Used internally by InterfacePlayer
 */
void InterfacePlayerRDK::NotifyEOS()
{
	if (!gstPrivateContext->eosSignalled)
	{
		if (!gstPrivateContext->eosCallbackIdleTaskPending)
		{
			/*Scheduling and executed async task immediately without returing the task id.
			 Which is leading to set the task pending always true when SLE is reached END_OF_LIST.
			 Due to this 30 tick is reported. changing the logic to set task pending to true before adding the task in notifyEOS function
			 and making it pending task to false if task id is invalid and eoscallback is pending.*/
			gstPrivateContext->eosCallbackIdleTaskPending = true;
			// eosSignalled is reset once the async task is completed either in Configure/Flush/ResetEOSSignalled, so set the flag before scheduling the task
			gstPrivateContext->eosSignalled = true;
			gstPrivateContext->eosCallbackIdleTaskId = mScheduler.ScheduleTask(PlayerAsyncTaskObj(IdleCallbackOnEOS, (void *)this, "IdleCallbackOnEOS"));
			if (gstPrivateContext->eosCallbackIdleTaskId == PLAYER_TASK_ID_INVALID && true == gstPrivateContext->eosCallbackIdleTaskPending)
			{
				gstPrivateContext->eosCallbackIdleTaskPending = false;
				MW_LOG_MIL("eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
						   (gstPrivateContext->eosCallbackIdleTaskPending ? 1 : 0),gstPrivateContext->eosCallbackIdleTaskId);
			}
			else
			{
				MW_LOG_MIL("eosCallbackIdleTask scheduled eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
						   (gstPrivateContext->eosCallbackIdleTaskPending ? 1 : 0),gstPrivateContext->eosCallbackIdleTaskId);
			}
		}
		else
		{
			MW_LOG_WARN("IdleCallbackOnEOS already registered previously, hence skip! eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
						(gstPrivateContext->eosCallbackIdleTaskPending ? 1 : 0),gstPrivateContext->eosCallbackIdleTaskId);
		}
	}
	else
	{
		MW_LOG_WARN("EOS already signaled, hence skip! eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
					(gstPrivateContext->eosCallbackIdleTaskPending ? 1 : 0),gstPrivateContext->eosCallbackIdleTaskId);
	}
}

/**
 *  @brief Set pipeline to PLAYING state once fragment caching is complete
 */
void InterfacePlayerRDK::NotifyFragmentCachingComplete()
{
	if(gstPrivateContext->pendingPlayState)
	{
		MW_LOG_MIL("InterfacePlayer: Setting pipeline to PLAYING state ");
		if (SetStateWithWarnings(gstPrivateContext->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			MW_LOG_ERR("InterfacePlayer_Configure GST_STATE_PLAYING failed");
		}
		gstPrivateContext->pendingPlayState = false;
	}
	else
	{
		MW_LOG_MIL("InterfacePlayer: No pending PLAYING state");
	}
}

/**
 *  @brief Starts processing EOS for a particular stream type
 */
void InterfacePlayerRDK::EndOfStreamReached(int mediaType, bool &shouldHaltBuffering)
{
	MW_LOG_MIL("entering InterfacePlayer_EndOfStreamReached type %d", mediaType);
	GstMediaType type = static_cast<GstMediaType>(mediaType);

	gst_media_stream *stream = &gstPrivateContext->stream[type];
	stream->eosReached = true;
	if ((stream->format != GST_FORMAT_INVALID) && stream->firstBufferProcessed == false)
	{
		MW_LOG_MIL("EOS received as first buffer ");
		NotifyEOS();
	}
	else
	{
		NotifyFragmentCachingComplete();                /*Set pipeline to PLAYING state once fragment caching is complete*/
		GstPlayer_SignalEOS(stream);

		/*For trickmodes, give EOS to audio source*/
		if (GST_NORMAL_PLAY_RATE != gstPrivateContext->rate)
		{
			GstPlayer_SignalEOS(gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO]);
			if (gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].source)
			{
				GstPlayer_SignalEOS(gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE]);
			}
		}
		else
		{
			if ((gstPrivateContext->stream[eGST_MEDIATYPE_AUDIO].eosReached) &&
				(!gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].eosReached) &&
				(gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].source))
			{
				gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE].eosReached = true;
				GstPlayer_SignalEOS(gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE]);
			}
		}

		// We are in buffering, but we received end of stream, un-pause pipeline
		shouldHaltBuffering = true;
	}
}

#define NO_PLAYBIN 1
/**
 * @brief Setup pipeline for a particular stream type
 * @param[in] pInterfacePlayerRDK pointer to InterfacePlayerRDK instance
 * @param[in] streamId stream type
 * @retval 0, if setup successfully. -1, for failure
 */
int InterfacePlayerRDK::InterfacePlayer_SetupStream(GstMediaType streamId, std::string manifestUrl)
{
	int retvalue = 0;
	this->TriggerEvent(InterfaceCB::startNewSubtitleStream, streamId);
	retvalue = this->SetupStream(streamId, (void*)this, manifestUrl);

	return retvalue;
}
/*
 * @brief Check whether Gstreamer platform has support of the given codec or not.
 *        codec to component mapping done in gstreamer side.
 * @param codecName - Name of codec to be checked
 * @return True if platform has the support else false
 */

bool InterfacePlayerRDK::IsCodecSupported(const std::string &codecName)
{
	bool retValue = false;
	GstRegistry* registry = gst_registry_get();
	for (std::string &componentName: gstMapDecoderLookUptable[codecName])
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, componentName.c_str());	/* searches for codec in the registry */
		if (pluginFeature != NULL)
		{
			retValue = true;
			break;
		}
	}
	return retValue;
}

void InterfacePlayerRDK::DisableDecoderHandleNotified()
{
	gstPrivateContext->decoderHandleNotified = false;
}

/**
 * @fn SignalSubtitleClock
 * @brief Signal the new clock to subtitle module
 * @return - true indicating successful operation in sending the clock update
 */
bool InterfacePlayerRDK::SignalSubtitleClock(gint64 vPTS, bool state)
{
	bool signalSent=false;
	bool underflowState = state;
	gint64 videoPTS = vPTS;
	gst_media_stream* stream = &gstPrivateContext->stream[eGST_MEDIATYPE_SUBTITLE];
	if ( stream && (stream->format != GST_FORMAT_INVALID) )
	{
		if (!stream->source)
		{
			MW_LOG_ERR("subtitle appsrc is NULL");
		}
		else if (!GST_IS_APP_SRC(stream->source))
		{
			MW_LOG_ERR("subtitle appsrc is invalid");
		}
		else
		{
			//Check if pipeline is in playing/paused state.
			GstState current, pending;
			GstStateChangeReturn ret;
			ret = gst_element_get_state(gstPrivateContext->pipeline, &current, &pending, 0);
			if ( (current == GST_STATE_PLAYING) && (ret != GST_STATE_CHANGE_FAILURE) && (underflowState != true) )
			{
				GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");	/* Retrieves the src pad */
				if (sourceEleSrcPad != NULL)
				{
					if (videoPTS > 0)
					{
						//GetVideoPTS returns PTS in 90KHz clock, convert it to nanoseconds for max precision
						GstClockTime pts = ((double)videoPTS / 90000.0) * GST_SECOND;
						GstStructure * eventStruct = gst_structure_new("sub_clock_sync", "current-pts", G_TYPE_UINT64, pts, NULL);
						if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
						{
							MW_LOG_ERR("Error on sending sub_clock_sync event");
							MW_LOG_ERR("Got VideoPTS: %" G_GINT64_FORMAT " and converted pts: %" G_GUINT64_FORMAT " , state = %d, pending = %d", videoPTS, pts, current, pending);
						}
						else
						{
							MW_LOG_DEBUG("Sent sub_clock_sync event, pts = %" G_GUINT64_FORMAT ", pts from sink was %" G_GUINT64_FORMAT "", pts, videoPTS);
							signalSent=true;
						}
					}
					else
					{
						MW_LOG_INFO("Got invalid video PTS: %" G_GINT64_FORMAT ". Clock not sent.", videoPTS);
					}
					gst_object_unref(sourceEleSrcPad);
				}
				else
				{
					MW_LOG_ERR("sourceEleSrcPad is NULL. Failed to send subtec clock event");
				}
			}
			else
			{
				MW_LOG_TRACE("Not sending clock event in non-play state to avoid gstreamer lockup, state = %d, pending = %d, underflow = %d.",
							 current, pending, underflowState);
			}
		}
	}
	else
	{
		if (stream)
		{
			MW_LOG_WARN("Invalid stream->format = %d)", stream->format);
		}
		else
		{
			MW_LOG_ERR("stream invalid)");
		}
	}
	//MW_LOG_TRACE("Exit SignalSubtitleClock");
	return signalSent;
}
/**
 *  @brief Increase the rank of Player decryptor plugins
 */
void InterfacePlayerRDK::InitializePlayerGstreamerPlugins()
{
	// Ensure GST is initialized
	if (!gst_init_check(nullptr, nullptr, nullptr)) {
		MW_LOG_ERR("gst_init_check() failed");
	}

#define PLUGINS_TO_LOWER_RANK_MAX    2
	static const char *plugins_to_lower_rank[PLUGINS_TO_LOWER_RANK_MAX] = {
		"aacparse",
		"ac3parse",
	};
	GstRegistry* registry = gst_registry_get();

	GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, GstPluginNamePR);

	if (pluginFeature == NULL)
	{
		MW_LOG_ERR("InterfacePlayerRDK: %s plugin feature not available; reloading player's plugin", GstPluginNamePR);
		GstPlugin * plugin = gst_plugin_load_by_name ("aamp");
		if(plugin)
		{
			gst_object_unref(plugin);
		}
		pluginFeature = gst_registry_lookup_feature(registry, GstPluginNamePR);
		if(pluginFeature == NULL)
			MW_LOG_ERR("InterfacePlayerRDK: %s plugin feature not available", GstPluginNamePR);
	}
	if(pluginFeature)
	{
		// CID:313773 gst_registry_remove_feature() will unref pluginFeature internally and
		// gst_registry_add_feature() will ref it again. So to maintain the refcount we do a ref and unref here
		// gst_registry_lookup_feature() will return pluginFeature after incrementing refcount which is unref at the end
		gst_object_ref(pluginFeature);
		gst_registry_remove_feature (registry, pluginFeature);
		gst_registry_add_feature (registry, pluginFeature);
		gst_object_unref(pluginFeature);

		MW_LOG_MIL("InterfacePlayerRDK: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNamePR);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameWV);

	if (pluginFeature == NULL)
	{
		MW_LOG_ERR("InterfacePlayerRDK: %s plugin feature not available", GstPluginNameWV);
	}
	else
	{
		MW_LOG_MIL("InterfacePlayerRDK: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameWV);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameCK);

	if (pluginFeature == NULL)
	{
		MW_LOG_ERR("InterfacePlayerRDK: %s plugin feature not available", GstPluginNameCK);
	}
	else
	{
		MW_LOG_MIL("InterfacePlayerRDK: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameCK);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameVMX);

	if (pluginFeature == NULL)
	{
		MW_LOG_ERR("InterfacePlayerRDK %s plugin feature not available", GstPluginNameVMX);
	}
	else
	{
		MW_LOG_MIL("InterfacePlayerRDK %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameVMX);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}
	for (int i=0; i<PLUGINS_TO_LOWER_RANK_MAX; i++)
	{
		pluginFeature = gst_registry_lookup_feature(registry, plugins_to_lower_rank[i]);
		if(pluginFeature)
		{
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY - 1);
			gst_object_unref(pluginFeature);
			MW_LOG_MIL("InterfacePlayerRDK: %s plugin priority set to GST_RANK_PRIMARY  - 1", plugins_to_lower_rank[i]);
		}
	}
}

/**
 * @brief Flush the track playbin
 * @param[in] pos - position to seek to after flush
 * @param[in] audioDelta - audio delta to be added to pos
 * @param[in] subDelta - subtitle delta to be added to pos
 */
double InterfacePlayerRDK::FlushTrack(int mediaType, double pos, double audioDelta, double subDelta)
{
	double startPosition = 0;
	GstMediaType type = static_cast<GstMediaType>(mediaType);

	MW_LOG_MIL("Entering InterfacePlayerRDK::FlushTrack() type[%d] pipeline state %s pos %lf",(int)type,
			   gst_element_state_get_name(GST_STATE(gstPrivateContext->pipeline)), pos);
	gst_media_stream *stream = &this->gstPrivateContext->stream[type];
	double rate = (double)GST_NORMAL_PLAY_RATE;

	if(eGST_MEDIATYPE_AUDIO == type)
	{
		socInterface->SetSeamlessSwitch(this->gstPrivateContext->audio_sink, TRUE);
		gstPrivateContext->filterAudioDemuxBuffers = true;
		pos = pos + audioDelta;

	}
	else
	{
		pos = pos + subDelta;
	}
	gst_element_seek_simple (GST_ELEMENT(stream->source),
							 GST_FORMAT_TIME,
							 GST_SEEK_FLAG_FLUSH,
							 pos * GST_SECOND);

	startPosition = pos;
	MW_LOG_MIL("Exiting InterfacePlayerRDK::FlushTrack() type[%d] pipeline state: %s startPosition: %lf Delta %lf",(int)type, gst_element_state_get_name(GST_STATE(gstPrivateContext->pipeline)), startPosition, (int)type==eGST_MEDIATYPE_AUDIO?audioDelta:subDelta);

	return rate;
}

/**
 *  @brief Dump diagnostic information
 *
 */
void InterfacePlayerRDK::DumpDiagnostics()
{
	MW_LOG_MIL("video_dec %p audio_dec %p video_sink %p audio_sink %p numberOfVideoBuffersSent %d",
			   gstPrivateContext->video_dec, gstPrivateContext->audio_dec, gstPrivateContext->video_sink,
			   gstPrivateContext->audio_sink, gstPrivateContext->numberOfVideoBuffersSent);
	socInterface->DumpDiagnosis();
}
