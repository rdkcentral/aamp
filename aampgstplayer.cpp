/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file aampgstplayer.cpp
 * @brief Gstreamer based player impl for AAMP
 */

#include "AampMemoryUtils.h"
#include "AampHandlerControl.h"
#include "gstaamptaskpool.h"
#include "aampgstplayer.h"
#include "isobmffbuffer.h"
#include "AampUtils.h"
#include "AampGstUtils.h"
#include "TextStyleAttributes.h"
#include "AampStreamSinkManager.h"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "priv_aamp.h"
#include <pthread.h>
#include <atomic>
#include <algorithm>

#include "ID3Metadata.hpp"
#include "AampSegmentInfo.hpp"
#include "AampBufferControl.h"

#ifdef AAMP_MPD_DRM
#include "aampoutputprotection.h"
#endif

#if GLIB_CHECK_VERSION(2, 68, 0)
// avoid deprecated g_memdup when g_memdup2 available
#define AAMP_G_MEMDUP(src, size) g_memdup2((src), (gsize)(size))
#else
#define AAMP_G_MEMDUP(src, size) g_memdup((src), (guint)(size))
#endif

#ifdef USE_EXTERNAL_STATS
// narrowly define MediaType for backwards compatibility
#define MediaType AampMediaType
#include "aamp-xternal-stats.h"
#undef MediaType
#endif

/**
 * @enum GstPlayFlags
 * @brief Enum of configuration flags used by playbin
 */
typedef enum {
	GST_PLAY_FLAG_VIDEO = (1 << 0),             /**< value is 0x001 */
	GST_PLAY_FLAG_AUDIO = (1 << 1),             /**< value is 0x002 */
	GST_PLAY_FLAG_TEXT = (1 << 2),              /**< value is 0x004 */
	GST_PLAY_FLAG_VIS = (1 << 3),               /**< value is 0x008 */
	GST_PLAY_FLAG_SOFT_VOLUME = (1 << 4),       /**< value is 0x010 */
	GST_PLAY_FLAG_NATIVE_AUDIO = (1 << 5),      /**< value is 0x020 */
	GST_PLAY_FLAG_NATIVE_VIDEO = (1 << 6),      /**< value is 0x040 */
	GST_PLAY_FLAG_DOWNLOAD = (1 << 7),          /**< value is 0x080 */
	GST_PLAY_FLAG_BUFFERING = (1 << 8),         /**< value is 0x100 */
	GST_PLAY_FLAG_DEINTERLACE = (1 << 9),       /**< value is 0x200 */
	GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10) /**< value is 0x400 */
} GstPlayFlags;

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5

#define DEFAULT_BUFFERING_TO_MS 10                       /**< TimeOut interval to check buffer fullness */
#define DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS 12         /**< Currently MAX FRAG DURATION + 2  */

#define DEFAULT_BUFFERING_MAX_MS (1000)                  /**< max buffering time */
#define DEFAULT_BUFFERING_MAX_CNT (DEFAULT_BUFFERING_MAX_MS/DEFAULT_BUFFERING_TO_MS)   /**< max buffering timeout count */

#define AAMP_MIN_PTS_UPDATE_INTERVAL 4000                        /**< Time duration in milliseconds if exceeded and pts has not changed; it is concluded pts is not changing */
#define AAMP_DELAY_BETWEEN_PTS_CHECK_FOR_EOS_ON_UNDERFLOW 500    /**< A timeout interval in milliseconds to check pts in case of underflow */
#define BUFFERING_TIMEOUT_PRIORITY -70                           /**< 0 is DEFAULT priority whereas -100 is the HIGH_PRIORITY */
#define AAMP_MIN_DECODE_ERROR_INTERVAL 10000                     /**< Minimum time interval in milliseconds between two decoder error CB to send anomaly error */
#define VIDEO_COORDINATES_SIZE 32

/**
 * @name gmapDecoderLoookUptable
 *
 * @brief Decoder map list lookup table
 * convert from codec to string map list of gstreamer
 * component.
 */
static std::map <std::string, std::vector<std::string>> gmapDecoderLoookUptable =
{
	{"ac-3", {"omxac3dec", "avdec_ac3", "avdec_ac3_fixed"}},
	{"ac-4", {"omxac4dec"}}
};

/**
 * @struct media_stream
 * @brief Holds stream(Audio, Video, Subtitle and Aux-Audio) specific variables.
 */
struct media_stream
{
	GstElement *sinkbin;						/**< Sink element to consume data */
	GstElement *source;							/**< to provide data to the pipleline */
	StreamOutputFormat format;					/**< Stream output format for this stream */
	bool pendingSeek;							/**< Flag denotes if a seek event has to be sent to the source */
	bool resetPosition;							/**< To indicate that the position of the stream is reset */
	bool bufferUnderrun;
	bool eosReached;							/**< To indicate the status of End of Stream reached */
	bool sourceConfigured;						/**< To indicate that the current source is initialised and configured */
	pthread_mutex_t sourceLock;
	uint32_t timeScale;
	int32_t trackId;							/**< Current Audio Track Id,so far it is implimented for AC4 track selection only */
	bool firstBufferProcessed;					/**< Indicates if the first buffer is processed in this stream */
	GstPad *demuxPad;							/**< Demux src pad >*/
	gulong demuxProbeId;						/**< Demux pad probe ID >*/
	AampBufferControl::BufferControlMaster mBufferControl;

	media_stream() : sinkbin(NULL), source(NULL), format(FORMAT_INVALID),
			 pendingSeek(false), resetPosition(false),
			 bufferUnderrun(false), eosReached(false), sourceConfigured(false), sourceLock(PTHREAD_MUTEX_INITIALIZER)
			, timeScale(1), trackId(-1)
			, firstBufferProcessed(false)
			,mBufferControl(), demuxPad(NULL), demuxProbeId(0)
	{

	}

	~media_stream()
	{
		g_clear_object(&sinkbin);
		g_clear_object(&source);
	}

	media_stream(const media_stream&)=delete;

	media_stream& operator=(const media_stream&)=delete;


};

struct MonitorAVState
{
	long long tLastReported;
	long long tLastSampled;
	gint64 av_position[2];
	const char *description;
	bool happy;
};
/**
 * @struct AAMPGstPlayerPriv
 * @brief Holds private variables of AAMPGstPlayer
 */
struct AAMPGstPlayerPriv
{
	AAMPGstPlayerPriv(const AAMPGstPlayerPriv&) = delete;
	AAMPGstPlayerPriv& operator=(const AAMPGstPlayerPriv&) = delete;

	MonitorAVState monitorAVstate;

	media_stream stream[AAMP_TRACK_COUNT];
	GstElement *pipeline; 				/**< GstPipeline used for playback. */
	GstBus *bus;					/**< Bus for receiving GstEvents from pipeline. */
	guint64 total_bytes;
	gint n_audio; 					/**< Number of audio tracks. */
	gint current_audio; 				/**< Offset of current audio track. */
	std::mutex TaskControlMutex; 			/**< For scheduling/de-scheduling or resetting async tasks/variables and timers */
	TaskControlData firstProgressCallbackIdleTask;
	guint periodicProgressCallbackIdleTaskId; 	/**< ID of timed handler created for notifying progress events. */
	guint bufferingTimeoutTimerId; 			/**< ID of timer handler created for buffering timeout. */
	GstElement *video_dec; 				/**< Video decoder used by pipeline. */
	GstElement *audio_dec; 				/**< Audio decoder used by pipeline. */
	GstElement *video_sink; 			/**< Video sink used by pipeline. */
	GstElement *audio_sink; 			/**< Audio sink used by pipeline. */
	GstElement *subtitle_sink; 			/**< Subtitle sink used by pipeline. */
	GstTaskPool *task_pool;				/**< Task pool in case RT priority is needed. */

	int rate; 					/**< Current playback rate. */
	VideoZoomMode zoom; 				/**< Video-zoom setting. */
	bool videoMuted; 				/**< Video mute status. */
	bool audioMuted; 				/**< Audio mute status. */
	std::mutex volumeMuteMutex;			/**< Mutex to ensure setVolumeOrMuteUnMute is thread-safe. */
	bool subtitleMuted; 				/**< Subtitle mute status. */
	double audioVolume; 				/**< Audio volume. */
	guint eosCallbackIdleTaskId; 			/**< ID of idle handler created for notifying EOS event. */
	std::atomic<bool> eosCallbackIdleTaskPending; 	/**< Set if any eos callback is pending. */
	bool firstFrameReceived; 			/**< Flag that denotes if first frame was notified. */
	char videoRectangle[VIDEO_COORDINATES_SIZE]; 	/**< Video-rectangle co-ordinates in format x,y,w,h. */
	bool pendingPlayState; 				/**< Flag that denotes if set pipeline to PLAYING state is pending. */
	bool decoderHandleNotified; 			/**< Flag that denotes if decoder handle was notified. */
	guint firstFrameCallbackIdleTaskId; 		/**< ID of idle handler created for notifying first frame event. */
	GstEvent *protectionEvent[AAMP_TRACK_COUNT]; 	/**< GstEvent holding the pssi data to be sent downstream. */
	std::atomic<bool> firstFrameCallbackIdleTaskPending; /**< Set if any first frame callback is pending. */
	bool using_westerossink; 			/**< true if westros sink is used as video sink */
	bool usingRialtoSink;                           /**< true if rialto sink is used for video and audio sinks */
	bool pauseOnStartPlayback;			/**< true if should start playback paused */
	std::atomic<bool> eosSignalled; 		/**< Indicates if EOS has signaled */
	gboolean buffering_enabled; 			/**< enable buffering based on multiqueue */
	gboolean buffering_in_progress; 		/**< buffering is in progress */
	guint buffering_timeout_cnt;    		/**< make sure buffering_timeout doesn't get stuck */
	GstState buffering_target_state; 		/**< the target state after buffering */
	gint64 lastKnownPTS; 				/**< To store the PTS of last displayed video */
	long long ptsUpdatedTimeMS; 			/**< Timestamp when PTS was last updated */
	guint ptsCheckForEosOnUnderflowIdleTaskId; 	/**< ID of task to ensure video PTS is not moving before notifying EOS on underflow. */
	int numberOfVideoBuffersSent; 			/**< Number of video buffers sent to pipeline */
	gint64 segmentStart;				/**< segment start value; required when qtdemux is enabled or restamping is disabled; -1 to send a segment.start query to gstreamer */
	GstQuery *positionQuery; 			/**< pointer that holds a position query object */
	GstQuery *durationQuery; 			/**< pointer that holds a duration query object */
	bool paused; 					/**< if pipeline is deliberately put in PAUSED state due to user interaction */
	GstState pipelineState; 			/**< current state of pipeline */
	TaskControlData firstVideoFrameDisplayedCallbackTask; /**< Task control data of the handler created for notifying state changed to Playing */
	bool firstTuneWithWesterosSinkOff; 		/**<  track if first tune was done for certain build */
	long long decodeErrorMsgTimeMS; 		/**< Timestamp when decode error message last posted */
	int decodeErrorCBCount; 			/**< Total decode error cb received within thresold time */
	bool progressiveBufferingEnabled;
	bool progressiveBufferingStatus;
	bool forwardAudioBuffers; 			/**< flag denotes if audio buffers to be forwarded to aux pipeline */
	bool enableSEITimeCode;				/**< Enables SEI Time Code handling */
	bool firstVideoFrameReceived; 			/**< flag that denotes if first video frame was notified. */
	bool firstAudioFrameReceived; 			/**< flag that denotes if first audio frame was notified */
	int  NumberOfTracks;	      			/**< Indicates the number of tracks */
 	PlaybackQualityStruct playbackQuality;		/**< video playback quality info */

	struct CallbackData
	{
		gpointer instance;
		gulong id;
		std::string name;
		CallbackData(gpointer _instance, gulong _id, std::string _name):instance(_instance), id(_id), name(_name){};
		CallbackData(const CallbackData& original):instance(original.instance), id(original.id), name(original.name){};
    	CallbackData(CallbackData&& original):instance(original.instance), id(original.id), name(original.name){};
    	CallbackData& operator=(const CallbackData&) = delete;
    	CallbackData& operator=(CallbackData&& original)
		{
			instance = std::move(original.instance);
			id = std::move(original.id);
			name = std::move(original.name);
			return *this;
		}
    	~CallbackData(){};
	};
	std::mutex mSignalVectorAccessMutex;
	std::vector<CallbackData> mCallBackIdentifiers;
	AampHandlerControl aSyncControl;
	AampHandlerControl syncControl;
	AampHandlerControl callbackControl;

	bool filterAudioDemuxBuffers;			/**< flag to filter audio demux buffers */
	double seekPosition;					/**< the position to seek the pipeline to in seconds */

	AAMPGstPlayerPriv() : monitorAVstate(), pipeline(NULL), bus(NULL),
			total_bytes(0), n_audio(0), current_audio(0),
			periodicProgressCallbackIdleTaskId(AAMP_TASK_ID_INVALID),
			bufferingTimeoutTimerId(AAMP_TASK_ID_INVALID), video_dec(NULL), audio_dec(NULL),TaskControlMutex(),firstProgressCallbackIdleTask("FirstProgressCallback"),
			video_sink(NULL), audio_sink(NULL), subtitle_sink(NULL),task_pool(NULL),
			rate(AAMP_NORMAL_PLAY_RATE), zoom(VIDEO_ZOOM_NONE), videoMuted(false), audioMuted(false), volumeMuteMutex(), subtitleMuted(false),
			audioVolume(1.0), eosCallbackIdleTaskId(AAMP_TASK_ID_INVALID), eosCallbackIdleTaskPending(false),
			firstFrameReceived(false), pendingPlayState(false), decoderHandleNotified(false),
			firstFrameCallbackIdleTaskId(AAMP_TASK_ID_INVALID), firstFrameCallbackIdleTaskPending(false),
			using_westerossink(false), usingRialtoSink(false), pauseOnStartPlayback(false), eosSignalled(false),
			buffering_enabled(FALSE), buffering_in_progress(FALSE), buffering_timeout_cnt(0),
			buffering_target_state(GST_STATE_NULL),
			lastKnownPTS(0), ptsUpdatedTimeMS(0), ptsCheckForEosOnUnderflowIdleTaskId(AAMP_TASK_ID_INVALID),
			numberOfVideoBuffersSent(0), segmentStart(0), positionQuery(NULL), durationQuery(NULL),
			paused(false), pipelineState(GST_STATE_NULL),
			firstVideoFrameDisplayedCallbackTask("FirstVideoFrameDisplayedCallback"),
			firstTuneWithWesterosSinkOff(false),
			decodeErrorMsgTimeMS(0), decodeErrorCBCount(0),
			progressiveBufferingEnabled(false), progressiveBufferingStatus(false)
			, forwardAudioBuffers (false), enableSEITimeCode(true),firstVideoFrameReceived(false),firstAudioFrameReceived(false),NumberOfTracks(0),playbackQuality{},
			filterAudioDemuxBuffers(false)
			,aSyncControl(), syncControl(),callbackControl()
			,seekPosition(0)
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
		for(int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			protectionEvent[i] = NULL;
		}
	}

	~AAMPGstPlayerPriv()
	{
		g_clear_object(&pipeline);
		g_clear_object(&bus);
		g_clear_object(&video_dec);
		g_clear_object(&audio_dec);
		g_clear_object(&video_sink);
		g_clear_object(&audio_sink);
		g_clear_object(&subtitle_sink);
		g_clear_object(&task_pool);
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			g_clear_object(&protectionEvent[i]);
		}
		g_clear_object(&positionQuery);
		g_clear_object(&durationQuery);
	}
};

static const char* GstPluginNamePR = "aampplayreadydecryptor";
static const char* GstPluginNameWV = "aampwidevinedecryptor";
static const char* GstPluginNameCK = "aampclearkeydecryptor";
static const char* GstPluginNameVMX = "aampverimatrixdecryptor";


/**
 * @brief Called from the mainloop when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval FALSE if the event source should be removed.
 */
static gboolean bus_message(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this);

/**
 * @fn bus_sync_handler
 * @brief Invoked synchronously when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval GST_BUS_PASS to pass the message to the async queue
 */
static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this);

/**
 * @brief g_timeout callback to wait for buffering to change
 *        pipeline from paused->playing
 */
static gboolean buffering_timeout (gpointer data);

/**
 * @brief check if elemement is instance
 */
static void type_check_instance( const char * str, GstElement * elem);

/**
 * @fn SetStateWithWarnings
 * @brief wraps gst_element_set_state and adds log messages where applicable
 * @param[in] element the GstElement whose state is to be changed
 * @param[in] targetState the GstState to apply to element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval Result of the state change (from inner gst_element_set_state())
 */
static GstStateChangeReturn SetStateWithWarnings(GstElement *element, GstState targetState);

/**
 * @brief AAMPGstPlayer Constructor
 */
AAMPGstPlayer::AAMPGstPlayer(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function<void(const unsigned char *, int, int, int) > exportFrames) : aamp(NULL), mEncryptedAamp(NULL), privateContext(NULL), mBufferingLock(), mProtectionLock(), PipelineSetToReady(false), trickTeardown(false), m_ID3MetadataHandler{id3HandlerCallback}, cbExportYUVFrame(NULL)
{
	privateContext = new AAMPGstPlayerPriv();
	if(privateContext)
	{
		this->aamp = aamp;

		// Initially set to this instance, can be changed by SetEncryptedAamp
		this->mEncryptedAamp = aamp;

		pthread_mutex_init(&mBufferingLock, NULL);
		pthread_mutex_init(&mProtectionLock, NULL);
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
			pthread_mutex_init(&privateContext->stream[i].sourceLock, NULL);

		this->cbExportYUVFrame = exportFrames;

		std::string debugLevel = GETCONFIGVALUE(eAAMPConfig_GstDebugLevel);
		if (!debugLevel.empty())
		{
			gst_debug_set_threshold_from_string(debugLevel.c_str(), 1);
		}
	}
	else
	{
		AAMPLOG_WARN("privateContext  is null");  //CID:85372 - Null Returns
	}
}

/**
 * @brief AAMPGstPlayer Destructor
 */
AAMPGstPlayer::~AAMPGstPlayer()
{
	DestroyPipeline();
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		pthread_mutex_destroy(&privateContext->stream[i].sourceLock);
	SAFE_DELETE(privateContext);
	pthread_mutex_destroy(&mBufferingLock);
	pthread_mutex_destroy(&mProtectionLock);
}

void AAMPGstPlayer::SignalConnect(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data)
{
	{
		const std::lock_guard<std::mutex> lock(privateContext->mSignalVectorAccessMutex);
		auto id = g_signal_connect(instance, detailed_signal, c_handler, data);
		if(0<id)
		{
			AAMPLOG_MIL("AAMPGstPlayer: Connected %s", detailed_signal);
			AAMPGstPlayerPriv::CallbackData Identifier{instance, id, detailed_signal};
			privateContext->mCallBackIdentifiers.push_back(Identifier);
		}
		else
		{
			AAMPLOG_WARN("AAMPGstPlayer: Could not connect %s", detailed_signal);
		}
	}
	privateContext->callbackControl.enable();
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
		AAMPLOG_ERR("recursion limit exceeded");
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

void AAMPGstPlayer::RemoveSignalsFromDisconnectList(gpointer pElementOrBin)
{
	const std::lock_guard<std::mutex> lock(privateContext->mSignalVectorAccessMutex);
	if(pElementOrBin)
	{
		const auto originalSize = privateContext->mCallBackIdentifiers.size();
		privateContext->mCallBackIdentifiers.erase(std::remove_if(
			privateContext->mCallBackIdentifiers.begin(),
			privateContext->mCallBackIdentifiers.end(),
			[pElementOrBin](AAMPGstPlayerPriv::CallbackData const & element) {return element.instance == pElementOrBin;}),
			privateContext->mCallBackIdentifiers.end());
		const auto newSize = privateContext->mCallBackIdentifiers.size();
		unsigned int entriesRemoved = static_cast<unsigned int>(originalSize-newSize);
		if(entriesRemoved)
		{
			AAMPLOG_INFO("%u entries removed.", entriesRemoved);
		}
	}
}

void AAMPGstPlayer::DisconnectSignals()
{
	const std::lock_guard<std::mutex> lock(privateContext->mSignalVectorAccessMutex);
	if(ISCONFIGSET(eAAMPConfig_enableDisconnectSignals))
	{
		std::set<gpointer> elements = GetElementPointers(privateContext->pipeline);

		for(auto data: privateContext->mCallBackIdentifiers)
		{
			if (data.instance == nullptr)
			{
				AAMPLOG_ERR("AAMPGstPlayer: %s signal handler, connected instance pointer is null", data.name.c_str());
			}
			else if(data.id == 0)
			{
				AAMPLOG_ERR("AAMPGstPlayer: %s signal handler id is 0", data.name.c_str());
			}
			else if(!elements.count(data.instance))
			{
				// This is expected following some tune failures
				AAMPLOG_WARN("AAMPGstPlayer: %s signal handler, connected instance is not in the pipeline", data.name.c_str());
			}
			else if(!g_signal_handler_is_connected(data.instance, data.id))
			{
				AAMPLOG_ERR("AAMPGstPlayer: %s signal handler not connected", data.name.c_str());
			}
			else
			{
				AAMPLOG_WARN("AAMPGstPlayer: disconnecting %s signal handler", data.name.c_str());
				g_signal_handler_disconnect(data.instance, data.id);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("eAAMPConfig_enableDisconnectSignals==false. Signals have not been disconnected.");
	}
	privateContext->mCallBackIdentifiers.clear();
}

/**
 *  @brief IdleTaskAdd - add an async/idle task in a thread safe manner, assuming it is not queued
 */
bool AAMPGstPlayer::IdleTaskAdd(TaskControlData& taskDetails, BackgroundTask funcPtr)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);

	if (0 == taskDetails.taskID)
	{
		taskDetails.taskIsPending = false;
		taskDetails.taskID = aamp->ScheduleAsyncTask(funcPtr, (void *)this);
		// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
		if(0 != taskDetails.taskID)
		{
			taskDetails.taskIsPending = true;
			ret = true;
			AAMPLOG_INFO("Task '%.50s' was added with ID = %d.", taskDetails.taskName.c_str(), taskDetails.taskID);
		}
		else
		{
			AAMPLOG_INFO("Task '%.50s' was not added or already ran.", taskDetails.taskName.c_str());
		}
	}
	else
	{
		AAMPLOG_WARN("Task '%.50s' was already pending.", taskDetails.taskName.c_str());
	}
	return ret;
}

/**
 *  @brief IdleTaskRemove - remove an async task in a thread safe manner, if it is queued
 */
bool AAMPGstPlayer::IdleTaskRemove(TaskControlData& taskDetails)
{
	bool ret = false;
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);

	if (0 != taskDetails.taskID)
	{
		AAMPLOG_INFO("AAMPGstPlayer: Remove task <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
		aamp->RemoveAsyncTask(taskDetails.taskID);
		taskDetails.taskID = 0;
		ret = true;
	}
	else
	{
		AAMPLOG_TRACE("AAMPGstPlayer: Task already removed <%.50s>, with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	taskDetails.taskIsPending = false;
	return ret;
}

/**
 * @brief IdleTaskClearFlags - clear async task id and pending flag in a thread safe manner
 *                             e.g. called when the task executes
 */
void AAMPGstPlayer::IdleTaskClearFlags(TaskControlData& taskDetails)
{
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);
	if ( 0 != taskDetails.taskID )
	{
		AAMPLOG_INFO("AAMPGstPlayer: Clear task control flags <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	else
	{
		AAMPLOG_TRACE("AAMPGstPlayer: Task control flags were already cleared <%.50s> with ID %d", taskDetails.taskName.c_str(), taskDetails.taskID);
	}
	taskDetails.taskIsPending = false;
	taskDetails.taskID = 0;
}

/**
 *  @brief TimerAdd - add a new glib timer in thread safe manner
 */
void AAMPGstPlayer::TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName)
{
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);
	if (funcPtr && user_data)
	{
		if (0 == taskId)
		{
			/* Sets the function pointed by functPtr to be called at regular intervals of repeatTimeout, supplying user_data to the function */
			taskId = g_timeout_add(repeatTimeout, funcPtr, user_data);
			AAMPLOG_INFO("AAMPGstPlayer: Added timer '%.50s', %d", (nullptr!=timerName) ? timerName : "unknown" , taskId);
		}
		else
		{
			AAMPLOG_INFO("AAMPGstPlayer: Timer '%.50s' already added, taskId=%d", (nullptr!=timerName) ? timerName : "unknown", taskId);
		}
	}
	else
	{
		AAMPLOG_ERR("Bad pointer. funcPtr = %p, user_data=%p",funcPtr,user_data);
	}
}

/**
 *  @brief TimerRemove - remove a glib timer in thread safe manner, if it exists
 */
void AAMPGstPlayer::TimerRemove(guint& taskId, const char* timerName)
{
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);
	if ( 0 != taskId )
	{
		AAMPLOG_INFO("AAMPGstPlayer: Remove timer '%.50s', %d", (nullptr!=timerName) ? timerName : "unknown", taskId);
		g_source_remove(taskId);					/* Removes the source as per the taskId */
		taskId = 0;
	}
	else
	{
		AAMPLOG_TRACE("Timer '%.50s' with taskId = %d already removed.", (nullptr!=timerName) ? timerName : "unknown", taskId);
	}
}

/**
 *  @brief TimerIsRunning - Check whether timer is currently running
 */
bool AAMPGstPlayer::TimerIsRunning(guint& taskId)
{
	std::lock_guard<std::mutex> lock(privateContext->TaskControlMutex);

	return (AAMP_TASK_ID_INVALID != taskId);
}

static AampMediaType GetMediaTypeForSource(const GstElement *source, const AAMPGstPlayer *_this)
{
	if (source && _this)
	{
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			/* eMEDIATYPE_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_SUBTITLE, eMEDIATYPE_AUX_AUDIO */
			if (source == _this->privateContext->stream[i].source)
			{
				return static_cast<AampMediaType>(i);
			}
		}

		AAMPLOG_WARN("unmapped source!");
	}
	else
	{
		AAMPLOG_ERR("Null check failed.");
	}

	return eMEDIATYPE_DEFAULT;
}

/**
 * @brief Callback for appsrc "need-data" signal
 * @param[in] source pointer to appsrc instance triggering "need-data" signal
 * @param[in] size size of data required
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static void need_data(GstElement *source, guint size, AAMPGstPlayer *_this)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	AampMediaType mediaType = GetMediaTypeForSource(source, _this);
	if (mediaType != eMEDIATYPE_DEFAULT)
	{
		UsingPlayerId playerId( _this->aamp->mPlayerId );
		struct media_stream *stream = &_this->privateContext->stream[mediaType];
		if(stream)
		{
			stream->mBufferControl.needData(_this, mediaType);
		}
		else
		{
			AAMPLOG_ERR( "Null check failed." );
		}
	}
}


/**
 * @brief Callback for appsrc "enough-data" signal
 * @param[in] source pointer to appsrc instance triggering "enough-data" signal
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static void enough_data(GstElement *source, AAMPGstPlayer *_this)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	if(_this && _this->aamp)
	{
		if (_this->aamp->DownloadsAreEnabled()) // avoid processing enough data if the downloads are already disabled.
		{
			UsingPlayerId playerId( _this->aamp->mPlayerId );
			AampMediaType mediaType = GetMediaTypeForSource(source, _this);
			if (mediaType != eMEDIATYPE_DEFAULT)
			{
				struct media_stream *stream = &_this->privateContext->stream[mediaType];
				if(stream)
				{
					stream->mBufferControl.enoughData(_this, mediaType);
				}
				else
				{
					AAMPLOG_ERR( "%s Null check failed.", GetMediaTypeName(mediaType));
				}
			}
		}
	}
	else
	{
		AAMPLOG_ERR( "Null check failed." );
	}
}


/**
 * @brief Callback for appsrc "seek-data" signal
 * @param[in] src pointer to appsrc instance triggering "seek-data" signal
 * @param[in] offset seek position offset
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static gboolean appsrc_seek(GstAppSrc *src, guint64 offset, AAMPGstPlayer * _this)
{
	HANDLER_CONTROL_HELPER(_this->privateContext->callbackControl, TRUE);
#ifdef TRACE
	AAMPLOG_MIL("appsrc %p seek-signal - offset %" G_GUINT64_FORMAT, src, offset);
#endif
	return TRUE;
}

#if GST_CHECK_VERSION(1,18,0)
// avoid compilation failure if building with Ubuntu 20.04 having gst<1.18
/**
 * @brief AAMPGstPlayer_HandleInstantRateChangeSeekProbe
 * @param[in] pad pad element
 * @param[in] info Pad information
 * @param[in] data pointer to data
 */
static GstPadProbeReturn AAMPGstPlayer_HandleInstantRateChangeSeekProbe(GstPad* pad, GstPadProbeInfo* info, gpointer data)
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
    	    AAMPLOG_INFO("In default case of  GST_EVENT_TYPE in padprobeReturn");
            return GST_PAD_PROBE_OK;
    };

    gdouble rate = 1.0;
    GstSeekFlags flags = GST_SEEK_FLAG_NONE;
    gst_event_parse_seek (event, &rate, nullptr, &flags, nullptr, nullptr, nullptr, nullptr);
    AAMPLOG_TRACE("rate %f segment->rate %f segment->format %d %d", rate, segment->rate, segment->format, GST_FORMAT_TIME);

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
 * @brief Initialize properties/callback of appsrc
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 * @param[in] source pointer to appsrc instance to be initialized
 * @param[in] mediaType stream type
 */
static void InitializeSource(AAMPGstPlayer *_this, GObject *source, AampMediaType mediaType = eMEDIATYPE_VIDEO)
{
	media_stream *stream = &_this->privateContext->stream[mediaType];
	GstCaps * caps = NULL;
	bool isFogEnabled = _this->aamp->mTSBEnabled;

	_this->SignalConnect(source, "need-data", G_CALLBACK(need_data), _this);		/* Sets up the call back function for need data event */
	_this->SignalConnect(source, "enough-data", G_CALLBACK(enough_data), _this);	/* Sets up the call back function for enough data event */
	_this->SignalConnect(source, "seek-data", G_CALLBACK(appsrc_seek), _this);		/* Sets up the call back function for seek data event */
	gst_app_src_set_stream_type(GST_APP_SRC(source), GST_APP_STREAM_TYPE_SEEKABLE);
	if (eMEDIATYPE_VIDEO == mediaType )
	{
		int MaxGstVideoBufBytes = isFogEnabled ? _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstVideoBufBytesForFogLive) : _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstVideoBufBytes);
		AAMPLOG_INFO("Setting gst Video buffer max bytes to %d FogLive :%d ", MaxGstVideoBufBytes,isFogEnabled);
		g_object_set(source, "max-bytes", (guint64)MaxGstVideoBufBytes, NULL);			/* Sets the maximum video buffer bytes as per configuration*/
	}
	else if (eMEDIATYPE_AUDIO == mediaType || eMEDIATYPE_AUX_AUDIO == mediaType)
	{
		int MaxGstAudioBufBytes = isFogEnabled ? _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstAudioBufBytesForFogLive) : _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstAudioBufBytes);
		AAMPLOG_INFO("Setting gst Audio buffer max bytes to %d FogLive :%d ", MaxGstAudioBufBytes,isFogEnabled);
		g_object_set(source, "max-bytes", (guint64)MaxGstAudioBufBytes, NULL);			/* Sets the maximum audio buffer bytes as per configuration*/
	}
	g_object_set(source, "min-percent", 50, NULL);								/* Trigger the need data event when the queued bytes fall below 50% */
	/* "format" can be used to perform seek or query/conversion operation*/
	/* gstreamer.freedesktop.org recommends to use GST_FORMAT_TIME 'if you don't have a good reason to query for samples/frames' */
	g_object_set(source, "format", GST_FORMAT_TIME, NULL);
	caps = GetGstCaps(stream->format, (PlatformType)_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType));

	if (caps != NULL)
	{
		gst_app_src_set_caps(GST_APP_SRC(source), caps);
		gst_caps_unref(caps);
	}
	else
	{
		/* If capabilites can not be established, set typefind TRUE. typefind determines the media-type of a stream and once type has been
		 * detected it sets its src pad caps to the found media type
		 */
		g_object_set(source, "typefind", TRUE, NULL);
	}
	stream->sourceConfigured = true;
}


/**
 * @brief Callback when source is added by playbin
 * @param[in] object a GstObject
 * @param[in] orig the object that originated the signal
 * @param[in] pspec the property that changed
 * @param[in] _this pointer to AAMPGstPlayer instance associated with the playback
 */
static void found_source(GObject * object, GObject * orig, GParamSpec * pspec, AAMPGstPlayer * _this )
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	AampMediaType mediaType = eMEDIATYPE_DEFAULT;
	if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_VIDEO].sinkbin))
	{
		AAMPLOG_MIL("Found source for video");
		mediaType = eMEDIATYPE_VIDEO;
	}
	else if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_AUDIO].sinkbin))
	{
		AAMPLOG_MIL("Found source for audio");
		mediaType = eMEDIATYPE_AUDIO;
	}
	else if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_AUX_AUDIO].sinkbin))
	{
		AAMPLOG_MIL("Found source for auxiliary audio");
		mediaType = eMEDIATYPE_AUX_AUDIO;
	}
	else if (object == G_OBJECT(_this->privateContext->stream[eMEDIATYPE_SUBTITLE].sinkbin))
	{
		AAMPLOG_MIL("Found source for subtitle");
		mediaType = eMEDIATYPE_SUBTITLE;
	}
	else
	{
		AAMPLOG_WARN("found_source didn't find a valid source");
	}
	if( mediaType != eMEDIATYPE_DEFAULT)
	{
		media_stream *stream;
		stream = &_this->privateContext->stream[mediaType];
		g_object_get(orig, pspec->name, &stream->source, NULL);
		InitializeSource(_this, G_OBJECT(stream->source), mediaType);
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
	AAMPGstPlayer * _this = (AAMPGstPlayer *)data;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	
	if (!strcmp(GST_ELEMENT_NAME(source), "source"))
	{
		std::string networkProxyValue = _this->aamp->GetNetworkProxy();		/* Get the proxy network setting from configuration*/
		if(!networkProxyValue.empty())
		{
			g_object_set(source, "proxy", networkProxyValue.c_str(), NULL);
			AAMPLOG_MIL("httpsoup -> Set network proxy '%s'", networkProxyValue.c_str());
		}
	}
	if (_this->aamp->mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)		//setting souphttpsrc priority back to GST_RANK_PRIMARY
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature (gst_registry_get (), "souphttpsrc");
		if (pluginFeature == NULL)
		{
			AAMPLOG_ERR("AAMPGstPlayer: souphttpsrc plugin feature not available;");
		}
		else
		{
			AAMPLOG_INFO("AAMPGstPlayer: souphttpsrc plugin priority set to GST_RANK_PRIMARY");
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY );
			gst_object_unref(pluginFeature);
		}
	}
}

static GstPadProbeReturn AAMPGstPlayer_DemuxPadProbeCallback(GstPad * pad, GstPadProbeInfo * info, AAMPGstPlayer * _this)
{
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
	if (_this)
	{
		// Filter audio buffers until video PTS is reached
		if (_this->privateContext->filterAudioDemuxBuffers &&
			pad == _this->privateContext->stream[eMEDIATYPE_AUDIO].demuxPad)
		{
			// PTS in nanoseconds
			gint64 currentPTS = (((double)_this->GetVideoPTS() / (double)90000) * GST_SECOND);
			if (GST_BUFFER_PTS(buffer) < currentPTS)
			{
				AAMPLOG_INFO("Dropping buffer: currentPTS=%" G_GINT64_FORMAT " buffer pts=%" G_GINT64_FORMAT, currentPTS, GST_BUFFER_PTS(buffer));
				return GST_PAD_PROBE_DROP;
			}
			else
			{
				AAMPLOG_WARN("Resetting filterAudioDemuxBuffers buffer pts=%" G_GINT64_FORMAT, GST_BUFFER_PTS(buffer));
				_this->privateContext->filterAudioDemuxBuffers = false;
			}
		}
	}
	return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn AAMPGstPlayer_DemuxPadProbeCallbackEvent(GstPad *pad, GstPadProbeInfo *info, AAMPGstPlayer *_this)
{
	if (_this)
	{
		if ((pad == _this->privateContext->stream[eMEDIATYPE_VIDEO].demuxPad) && (_this->privateContext->rate == AAMP_NORMAL_PLAY_RATE))
		{
			GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
			if (GST_EVENT_TYPE(event) == GST_EVENT_SEGMENT)
			{
				GstSegment segment;
				gst_event_copy_segment(event, &segment);
				AAMPLOG_TRACE("duration  %" G_GUINT64_FORMAT " start %" G_GUINT64_FORMAT " stop %" G_GUINT64_FORMAT,
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

static GstPadProbeReturn AAMPGstPlayer_DemuxPadProbeCallbackAny(GstPad *pad, GstPadProbeInfo *info, AAMPGstPlayer *_this)
{
	GstPadProbeReturn rtn = GST_PAD_PROBE_OK;
	AAMPLOG_TRACE("type %u",info->type);
	if (info->type & GST_PAD_PROBE_TYPE_BUFFER)
	{
		rtn = AAMPGstPlayer_DemuxPadProbeCallback(pad, info, _this);
	}
	else if (info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
	{
		rtn = AAMPGstPlayer_DemuxPadProbeCallbackEvent(pad, info, _this);
	}
	return rtn;
}

static void AAMPGstPlayer_OnDemuxPadAddedCb(GstElement* demux, GstPad* newPad, AAMPGstPlayer * _this)
{
	if (_this)
	{
		GstPadProbeType mask = GST_PAD_PROBE_TYPE_INVALID;

		if ( _this->aamp->mConfig->IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		{
			mask = GST_PAD_PROBE_TYPE_BUFFER;
		}
		if ( _this->aamp->mConfig->IsConfigSet(eAAMPConfig_EnablePTSReStamp))
		{
			//cast to Keep compiler happy
			mask = static_cast<GstPadProbeType>(static_cast<int>(mask) | static_cast<int>(GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM));
		}
		AAMPLOG_TRACE("mask %u",mask);
		// We need to identify which stream the demux belongs to.
		// We can't use a CAPS based check, for use-cases such as aux-audio
		GstElement *parent = GST_ELEMENT_PARENT(demux);
		bool found = false;
		while (parent)
		{
			if (aamp_StartsWith(GST_ELEMENT_NAME(parent), "playbin"))
			{
				for (int i = 0; i < AAMP_TRACK_COUNT; i++)
				{
					media_stream *stream = &_this->privateContext->stream[i];
					if (parent == stream->sinkbin)
					{
						if (stream->demuxPad == NULL)
						{
							stream->demuxPad = newPad;
							stream->demuxProbeId = gst_pad_add_probe(newPad,
									mask,
									(GstPadProbeCallback)AAMPGstPlayer_DemuxPadProbeCallbackAny,
									_this,
									NULL);
							AAMPLOG_WARN("Added probe to qtdemux type[%d] src pad: %s", i, GST_PAD_NAME(newPad));
						}
						else
						{
							AAMPLOG_WARN("Ignoring additional pad");
						}
						found = true;
					}
				}
				break;
			}
			AAMPLOG_TRACE("Got Parent: %s", GST_ELEMENT_NAME(parent));
			parent = GST_ELEMENT_PARENT(parent);
		}
		if (!found)
		{
			GstCaps* caps = gst_pad_get_current_caps(newPad);
			gchar *capsStr = gst_caps_to_string(caps);
			AAMPLOG_WARN("No matching stream found for demux: %s and caps: %s", GST_ELEMENT_NAME(demux), capsStr);
			g_free(capsStr);
			if (caps)
			{
				gst_caps_unref(caps);
			}
		}
	}
}

static void element_setup_cb(GstElement * playbin, GstElement * element, AAMPGstPlayer *_this)
{
	gchar* elemName = gst_element_get_name(element);
	if (elemName && aamp_StartsWith(elemName, "qtdemux"))
	{
		AAMPLOG_WARN( "Add pad-added callback to demux:%s\n", elemName);
		g_signal_connect(element, "pad-added", G_CALLBACK(AAMPGstPlayer_OnDemuxPadAddedCb), _this);
	}
	g_free(elemName);
}

/**
 * @brief Idle callback to notify first frame rendered event
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallbackOnFirstFrame(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		_this->aamp->NotifyFirstFrameReceived(_this->getCCDecoderHandle());
		_this->privateContext->firstFrameCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
		_this->privateContext->firstFrameCallbackIdleTaskPending = false;
	}
	return G_SOURCE_REMOVE;
}


/**
 * @brief Idle callback to notify end-of-stream event
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallbackOnEOS(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		AAMPLOG_MIL("eosCallbackIdleTaskId %d", _this->privateContext->eosCallbackIdleTaskId);
		_this->aamp->NotifyEOSReached();
		_this->privateContext->eosCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
		_this->privateContext->eosCallbackIdleTaskPending = false;
	}
	return G_SOURCE_REMOVE;
}

void MonitorAV( AAMPGstPlayer *_this )
{
	const int AVSYNC_THRESHOLD_MS = 100;
	const int JUMP_THRESHOLD_MS = 100;
	GstState state = GST_STATE_VOID_PENDING;
	GstState pending = GST_STATE_VOID_PENDING;
	GstClockTime timeout = 0;
	gint64 av_position[2] = {0,0};
	gint rc = gst_element_get_state(_this->privateContext->pipeline, &state, &pending, timeout );
	if( rc == GST_STATE_CHANGE_SUCCESS )
	{
		if( state == GST_STATE_PLAYING )
		{
			struct MonitorAVState *monitorAVState = &_this->privateContext->monitorAVstate;
			const char *description = NULL;
			int numTracks = 0;
			bool bigJump = false;
			long long tNow = aamp_GetCurrentTimeMS();
			if( !monitorAVState->tLastReported )
			{
				monitorAVState->tLastReported = tNow;
			}
			// skip reading audio position when trickplay is active
			int maxTracks = (_this->privateContext->rate == AAMP_NORMAL_PLAY_RATE) ? 2 : 1;
			for( int i=0; i<maxTracks; i++ )
			{ // eMEDIATYPE_VIDEO=0, eMEDIATYPE_AUDIO=1
				auto sinkbin = _this->privateContext->stream[i].sinkbin;
				if (sinkbin && (_this->privateContext->stream[i].format != FORMAT_INVALID))
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
								description = (i==eMEDIATYPE_VIDEO)?"video freeze":"audio drop";
							}
						}
						else if( i == eMEDIATYPE_VIDEO && monitorAVState->happy )
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
					AAMPLOG_MIL( "MonitorAV_%s: %" G_GINT64_FORMAT ",%" G_GINT64_FORMAT ",%d,%lld",
							   monitorAVState->description,
							   monitorAVState->av_position[eMEDIATYPE_VIDEO],
							   monitorAVState->av_position[eMEDIATYPE_AUDIO],
							   (int)(monitorAVState->av_position[eMEDIATYPE_VIDEO] - monitorAVState->av_position[eMEDIATYPE_AUDIO]),
							   monitorAVState->tLastSampled - monitorAVState->tLastReported );
				}
				AAMPLOG_MIL( "MonitorAV_%s: %" G_GINT64_FORMAT ",%" G_GINT64_FORMAT ",%d,0",
							   description,
								av_position[eMEDIATYPE_VIDEO],
								av_position[eMEDIATYPE_AUDIO],
								(int)(av_position[eMEDIATYPE_VIDEO] - av_position[eMEDIATYPE_AUDIO]) );
				monitorAVState->tLastReported = monitorAVState->tLastSampled;
				monitorAVState->description = description;
			}
			// remember most recently sniffed pair of video and audio positions
			monitorAVState->av_position[eMEDIATYPE_VIDEO] = av_position[eMEDIATYPE_VIDEO];
			monitorAVState->av_position[eMEDIATYPE_AUDIO] = av_position[eMEDIATYPE_AUDIO];
		}
	}
	else
	{
		AAMPLOG_WARN( "gst_element_get_state %d", state );
	}
}

/**
 * @brief Timer's callback to notify playback progress event
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_CONTINUE, this function to be called periodically
 */
static gboolean ProgressCallbackOnTimeout(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		UsingPlayerId playerId( _this->aamp->mPlayerId );
		
		if( _this->aamp->mConfig->IsConfigSet(eAAMPConfig_MonitorAV) )
		{
			MonitorAV(_this);
		}
		
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			_this->privateContext->stream[i].mBufferControl.update(_this, static_cast<AampMediaType>(i));
		}
		_this->aamp->ReportProgress();
		AAMPLOG_TRACE("current %d, stored %d ", g_source_get_id(g_main_current_source()), _this->privateContext->periodicProgressCallbackIdleTaskId);
	}
	return G_SOURCE_CONTINUE;
}


/**
 * @brief Idle callback to start progress notifier timer
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallback(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		UsingPlayerId playerId( _this->aamp->mPlayerId );
		// mAsyncTuneEnabled passed, because this could be called from Scheduler or main loop
		_this->aamp->ReportProgress();
		_this->IdleTaskClearFlags(_this->privateContext->firstProgressCallbackIdleTask);

		if ( !(_this->TimerIsRunning(_this->privateContext->periodicProgressCallbackIdleTaskId)) )
		{
			double  reportProgressInterval = _this->aamp->mConfig->GetConfigValue(eAAMPConfig_ReportProgressInterval);
			reportProgressInterval *= 1000; //convert s to ms

			GSourceFunc timerFunc = ProgressCallbackOnTimeout;
			_this->TimerAdd(timerFunc, (int)reportProgressInterval, _this->privateContext->periodicProgressCallbackIdleTaskId, user_data, "periodicProgressCallbackIdleTask");
		}
		else
		{
			AAMPLOG_INFO("Progress callback already available: periodicProgressCallbackIdleTaskId %d", _this->privateContext->periodicProgressCallbackIdleTaskId);
		}
	}
	return G_SOURCE_REMOVE;
}

/**
 * @brief Idle callback to notify first video frame was displayed
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean IdleCallbackFirstVideoFrameDisplayed(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
	if (_this)
	{
		_this->aamp->NotifyFirstVideoFrameDisplayed();
		_this->IdleTaskRemove(_this->privateContext->firstVideoFrameDisplayedCallbackTask);
	}
	return G_SOURCE_REMOVE;
}

/**
 *  @brief Notify first Audio and Video frame through an idle function
 */
void AAMPGstPlayer::NotifyFirstFrame(AampMediaType type)
{
	bool firstBufferNotified=false;

	// LogTuneComplete will be noticed after getting video first frame.
	// incase of audio or video only playback NumberofTracks =1, so in that case also LogTuneCompleted needs to captured when either audio/video frame received.
	if ((!privateContext->firstFrameReceived && (privateContext->firstVideoFrameReceived
			|| (1 == privateContext->NumberOfTracks && (privateContext->firstAudioFrameReceived || privateContext->firstVideoFrameReceived)))) 
			&& !aamp->mIsFlushOpeartionInProgress )
	{
		privateContext->firstFrameReceived = true;
		aamp->LogFirstFrame();
		aamp->LogTuneComplete();
		aamp->NotifyFirstBufferProcessed(GetVideoRectangle());
		firstBufferNotified=true;
	}

	if (eMEDIATYPE_VIDEO == type)
	{
		if((aamp->mTelemetryInterval > 0) && aamp->mDiscontinuityFound)
		{
			aamp->SetDiscontinuityParam();
		}

		AAMPLOG_MIL("AAMPGstPlayer_OnFirstVideoFrameCallback. got First Video Frame");

		// No additional checks added here, since the NotifyFirstFrame will be invoked only once
		// in westerossink disabled case until fixes it. Also aware of NotifyFirstBufferProcessed called
		// twice in this function, since it updates timestamp for calculating time elapsed, its trivial
		if (!firstBufferNotified && !aamp->mIsFlushOpeartionInProgress)
		{
			aamp->NotifyFirstBufferProcessed(GetVideoRectangle());
		}

		if (!privateContext->decoderHandleNotified)
		{
			privateContext->decoderHandleNotified = true;
			privateContext->firstFrameCallbackIdleTaskPending = false;
			privateContext->firstFrameCallbackIdleTaskId = aamp->ScheduleAsyncTask(IdleCallbackOnFirstFrame, (void *)this, "FirstFrameCallback");
			// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
			if(privateContext->firstFrameCallbackIdleTaskId != AAMP_TASK_ID_INVALID)
			{
				privateContext->firstFrameCallbackIdleTaskPending = true;
			}
		}
		else if (PipelineSetToReady)
		{
			//If pipeline is set to ready forcefully due to change in track_id, then re-initialize CC
			aamp->InitializeCC(getCCDecoderHandle());
		}

		IdleTaskAdd(privateContext->firstProgressCallbackIdleTask, IdleCallback);

		if (aamp->IsFirstVideoFrameDisplayedRequired())
		{
			if ( !IdleTaskAdd(privateContext->firstVideoFrameDisplayedCallbackTask, IdleCallbackFirstVideoFrameDisplayed))
			{
				AAMPLOG_WARN("IdleCallbackFirstVideoFrameDisplayed was not added.");
			}
		}
		PipelineSetToReady = false;
	}
	else if (eMEDIATYPE_AUDIO == type)
	{
		AAMPLOG_MIL("AAMPGstPlayer_OnAudioFirstFrameAudDecoder. got First Audio Frame");
		if (aamp->mAudioOnlyPb)
		{
			if (!privateContext->decoderHandleNotified)
			{
				privateContext->decoderHandleNotified = true;
				privateContext->firstFrameCallbackIdleTaskPending = false;
				privateContext->firstFrameCallbackIdleTaskId = aamp->ScheduleAsyncTask(IdleCallbackOnFirstFrame, (void *)this, "FirstFrameCallback");
				// Wait for scheduler response , if failed to create task for wrong state , not to make pending flag as true
				if(privateContext->firstFrameCallbackIdleTaskId != AAMP_TASK_ID_INVALID)
				{
					privateContext->firstFrameCallbackIdleTaskPending = true;
				}
			}
			IdleTaskAdd(privateContext->firstProgressCallbackIdleTask, IdleCallback);
		}
	}

}

/**
 * @brief Callback invoked after first video frame decoded
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnFirstVideoFrameCallback(GstElement* object, guint arg0, gpointer arg1,
	AAMPGstPlayer * _this)

{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	_this->privateContext->firstVideoFrameReceived = true;
	_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);

}

/**
 * @brief Callback invoked after receiving the SEI Time Code information
 * @param[in] object pointer to element raising the callback
 * @param[in] hours Hour value of the SEI Timecode
 * @param[in] minutes Minute value of the SEI Timecode
 * @param[in] seconds Second value of the SEI Timecode
 * @param[in] user_data pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_redButtonCallback(GstElement* object, guint hours, guint minutes, guint seconds, gpointer user_data)
{
       AAMPGstPlayer *_this = (AAMPGstPlayer *)user_data;
       if (_this)
       {
		HANDLER_CONTROL_HELPER_CALLBACK_VOID();
               char buffer[16];
               snprintf(buffer,16,"%d:%d:%d",hours,minutes,seconds);
               _this->aamp->seiTimecode.assign(buffer);
       }
}

/**
 * @brief Callback invoked after first audio buffer decoded
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnAudioFirstFrameAudDecoder(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	_this->privateContext->firstAudioFrameReceived = true;
	_this->NotifyFirstFrame(eMEDIATYPE_AUDIO);
}

/**
 * @brief Check if gstreamer element is video decoder
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of the decoder
 */
bool AAMPGstPlayer_isVideoDecoder(const char* name, AAMPGstPlayer * _this)
{
	if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{
		return (aamp_StartsWith(name, "omxwmvdec") || aamp_StartsWith(name, "omxh26")
				|| aamp_StartsWith(name, "omxav1dec") || aamp_StartsWith(name, "omxvp") || aamp_StartsWith(name, "omxmpeg"));
	}
	return (_this->privateContext->using_westerossink ? aamp_StartsWith(name, "westerossink"):
				_this->privateContext->usingRialtoSink ? aamp_StartsWith(name, "rialtomsevideosink"): aamp_StartsWith(name, "brcmvideodecoder"));
}

/**
 * @brief Check if gstreamer element is video sink
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of video sink
 */
bool AAMPGstPlayer_isVideoSink(const char* name, AAMPGstPlayer * _this)
{
	if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{       
		return (aamp_StartsWith(name, "westerossink") || aamp_StartsWith(name, "rtkv1sink") || (_this->privateContext->usingRialtoSink && aamp_StartsWith(name, "rialtomsevideosink") == true));
	}
	return	(!_this->privateContext->using_westerossink && aamp_StartsWith(name, "brcmvideosink") == true) || // brcmvideosink0, brcmvideosink1, ...
		( _this->privateContext->using_westerossink && aamp_StartsWith(name, "westerossink") == true) ||
		(_this->privateContext->usingRialtoSink && aamp_StartsWith(name, "rialtomsevideosink") == true);
}

/**
 * @brief Check if gstreamer element is audio sink or audio decoder
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of audio sink or audio decoder
 */
bool AAMPGstPlayer_isAudioSinkOrAudioDecoder(const char* name, AAMPGstPlayer * _this)
{

	if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{       
		return (aamp_StartsWith(name, "rtkaudiosink")
				|| aamp_StartsWith(name, "alsasink")
				|| aamp_StartsWith(name, "fakesink")
				|| (_this->privateContext->usingRialtoSink && aamp_StartsWith(name, "rialtomseaudiosink") == true));
	}
	return (aamp_StartsWith(name, "brcmaudiodecoder") || aamp_StartsWith(name, "amlhalasink"));
}

/**
 * @brief Check if gstreamer element is audio decoder
 * @param[in] name Name of the element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval TRUE if element name is that of audio or video decoder
 */
bool AAMPGstPlayer_isVideoOrAudioDecoder(const char* name, AAMPGstPlayer * _this)
{
	// The idea is to identify video or audio decoder plugin created at runtime by playbin and register to its first-frame/pts-error callbacks
	// This support is available in specific plugins in RDK builds and hence checking only for such plugin instances here
	// For platforms that doesnt support callback, we use GST_STATE_PLAYING state change of playbin to notify first frame to app
	bool isAudioOrVideoDecoder = false;
	const auto platformType = _this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType);
	if (!_this->privateContext->using_westerossink && aamp_StartsWith(name, "brcmvideodecoder"))
	{
		isAudioOrVideoDecoder = true;
	}

	else if(platformType == ePLATFORM_REALTEK && aamp_StartsWith(name, "omx"))
	{
		isAudioOrVideoDecoder = true;
	}

	else if ((platformType != ePLATFORM_REALTEK) && _this->privateContext->using_westerossink && aamp_StartsWith(name, "westerossink"))
	{
		isAudioOrVideoDecoder = true;
	}
	else if (_this->privateContext->usingRialtoSink && aamp_StartsWith(name, "rialtomse"))
	{
		isAudioOrVideoDecoder = true;
	}
	else if (aamp_StartsWith(name, "brcmaudiodecoder"))
	{
		isAudioOrVideoDecoder = true;
	}
	return isAudioOrVideoDecoder;
}

/**
 * @brief Notifies EOS if video decoder pts is stalled
 * @param[in] user_data pointer to AAMPGstPlayer instance
 * @retval G_SOURCE_REMOVE, if the source should be removed
 */
static gboolean VideoDecoderPtsCheckerForEOS(gpointer user_data)
{
	AAMPGstPlayer *_this = (AAMPGstPlayer *) user_data;
	AAMPGstPlayerPriv *privateContext = _this->privateContext;
	gint64 currentPTS = _this->GetVideoPTS();			/* Gets the currentPTS from the 'video-pts' property of the element */

	if (currentPTS == privateContext->lastKnownPTS)
	{
		AAMPLOG_MIL("PTS not changed");
		_this->NotifyEOS();								/* Notify EOS if the PTS has not changed */
	}
	else
	{
		AAMPLOG_MIL("Video PTS still moving lastKnownPTS %" G_GUINT64_FORMAT " currentPTS %" G_GUINT64_FORMAT " ##", privateContext->lastKnownPTS, currentPTS);
	}
	privateContext->ptsCheckForEosOnUnderflowIdleTaskId = AAMP_TASK_ID_INVALID;
	return G_SOURCE_REMOVE;
}

/**
 *  @brief Callback function to get video frames
 */
GstFlowReturn AAMPGstPlayer::AAMPGstPlayer_OnVideoSample(GstElement* object, AAMPGstPlayer * _this)
{
#if defined(__APPLE__)
	HANDLER_CONTROL_HELPER(_this->privateContext->callbackControl, GST_FLOW_OK);
	if(_this && _this->cbExportYUVFrame)
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
					_this->cbExportYUVFrame(map.data, (int)map.size, width, height);
					gst_buffer_unmap(buffer, &map);
				}
				else
				{
					AAMPLOG_ERR("buffer map failed\n");
				}
			}
			else
			{
				AAMPLOG_ERR("buffer NULL\n");
			}
			gst_sample_unref(sample);
		}
		else
		{
			AAMPLOG_WARN("sample NULL\n");
		}
	}
#endif
	return GST_FLOW_OK;
}

/**
 * @brief Callback invoked when facing an underflow
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnGstBufferUnderflowCb(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_DisableUnderflow))
	{ // optionally ignore underflow
		AAMPLOG_WARN("##  [WARN] Ignored underflow from %s, disableUnderflow config enabled ##", GST_ELEMENT_NAME(object));
	}
	else
	{
		//TODO - Handle underflow
		AampMediaType type = eMEDIATYPE_DEFAULT;  //CID:89173 - Resolve Uninit
		AAMPGstPlayerPriv *privateContext = _this->privateContext;
		const auto platformType = _this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType);

		bool isVideo = false;

		if (platformType == ePLATFORM_REALTEK)
		{
			isVideo = AAMPGstPlayer_isVideoSink(GST_ELEMENT_NAME(object), _this);
		}
		else
		{
			isVideo = AAMPGstPlayer_isVideoDecoder(GST_ELEMENT_NAME(object), _this);
		}

		if (isVideo)
		{
			type = eMEDIATYPE_VIDEO;
		}
		else if (AAMPGstPlayer_isAudioSinkOrAudioDecoder(GST_ELEMENT_NAME(object), _this))
		{
			type = eMEDIATYPE_AUDIO;
		}
		else
		{
			AAMPLOG_WARN("## WARNING!! Underflow message from %s not handled, unmapped underflow!", GST_ELEMENT_NAME(object));
			return;
		}

		AAMPLOG_WARN("## APP[%s] Got Underflow message from %s type %d ##", (_this->aamp->GetAppName()).c_str(), GST_ELEMENT_NAME(object), type);
		bool isBufferFull = _this->privateContext->stream[type].mBufferControl.isBufferFull(type);
		_this->privateContext->stream[type].mBufferControl.underflow(_this, type);
		_this->privateContext->stream[type].bufferUnderrun = true;

		if ((_this->privateContext->stream[type].eosReached) && (_this->privateContext->rate > 0))
		{
			if (!privateContext->ptsCheckForEosOnUnderflowIdleTaskId)
			{
				privateContext->lastKnownPTS =_this->GetVideoPTS();			/* Gets the currentPTS from the 'video-pts' property of the element */
				privateContext->ptsUpdatedTimeMS = NOW_STEADY_TS_MS;
				privateContext->ptsCheckForEosOnUnderflowIdleTaskId = g_timeout_add(AAMP_DELAY_BETWEEN_PTS_CHECK_FOR_EOS_ON_UNDERFLOW, VideoDecoderPtsCheckerForEOS, _this);
																	/*g_timeout_add - Sets the function VideoDecoderPtsCheckerForEOS to be called at regular intervals*/
			}
			else
			{
				AAMPLOG_WARN("ptsCheckForEosOnUnderflowIdleTask ID %d already running, ignore underflow", (int)privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
			}
		}
		else
		{
			AAMPLOG_WARN("Mediatype %d underrun, when eosReached is %d", type, _this->privateContext->stream[type].eosReached);

#ifdef USE_EXTERNAL_STATS
			INC_RETUNE_COUNT(type); // Increment the retune count for low level AV metric
#endif

			_this->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, type, isBufferFull);		/* Schedule a retune */
		}
	}
}

/**
 * @brief Callback invoked a PTS error is encountered
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnGstPtsErrorCb(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	AAMPLOG_ERR("## APP[%s] Got PTS error message from %s ##", (_this->aamp->GetAppName()).c_str(), GST_ELEMENT_NAME(object));
	const auto platformType = _this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType);

	bool isVideo = false;

	if (platformType == ePLATFORM_REALTEK)
	{
		isVideo = AAMPGstPlayer_isVideoSink(GST_ELEMENT_NAME(object), _this);
	}
	else
	{
		isVideo = AAMPGstPlayer_isVideoDecoder(GST_ELEMENT_NAME(object), _this);
	}

	if (isVideo)
	{
		_this->aamp->ScheduleRetune(eGST_ERROR_PTS, eMEDIATYPE_VIDEO);
	}
	else if (AAMPGstPlayer_isAudioSinkOrAudioDecoder(GST_ELEMENT_NAME(object), _this))
	{
		_this->aamp->ScheduleRetune(eGST_ERROR_PTS, eMEDIATYPE_AUDIO);
	}
}

/**
 * @brief Callback invoked a Decode error is encountered
 * @param[in] object pointer to element raising the callback
 * @param[in] arg0 number of arguments
 * @param[in] arg1 array of arguments
 * @param[in] _this pointer to AAMPGstPlayer instance
 */
static void AAMPGstPlayer_OnGstDecodeErrorCb(GstElement* object, guint arg0, gpointer arg1,
        AAMPGstPlayer * _this)
{
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
	long long deltaMS = NOW_STEADY_TS_MS - _this->privateContext->decodeErrorMsgTimeMS;
	_this->privateContext->decodeErrorCBCount += 1;
	if (deltaMS >= AAMP_MIN_DECODE_ERROR_INTERVAL)
	{
#ifdef USE_EXTERNAL_STATS
		INC_DECODE_ERROR(); // Increment the decoder error for low level AV metric
#endif

		_this->aamp->SendAnomalyEvent(ANOMALY_WARNING, "Decode Error Message Callback=%d time=%d",_this->privateContext->decodeErrorCBCount, AAMP_MIN_DECODE_ERROR_INTERVAL);
		_this->privateContext->decodeErrorMsgTimeMS = NOW_STEADY_TS_MS;
		AAMPLOG_ERR("## APP[%s] Got Decode Error message from %s ## total_cb=%d timeMs=%d", (_this->aamp->GetAppName()).c_str(), GST_ELEMENT_NAME(object),  _this->privateContext->decodeErrorCBCount, AAMP_MIN_DECODE_ERROR_INTERVAL);
		_this->privateContext->decodeErrorCBCount = 0;
	}
}

static gboolean buffering_timeout (gpointer data)
{
	AAMPGstPlayer * _this = (AAMPGstPlayer *) data;
	auto aamp = _this->aamp;
	if (_this && _this->privateContext)
	{
		UsingPlayerId playerId( aamp->mPlayerId );
		AAMPGstPlayerPriv * privateContext = _this->privateContext;
		if (_this->privateContext->buffering_in_progress)
		{
			int frames = -1;
			if (_this->privateContext->video_dec)
			{
				g_object_get(_this->privateContext->video_dec,"queued_frames",(uint*)&frames,NULL);
				AAMPLOG_DEBUG("queued_frames: %d", frames);
			}
			MediaFormat mediaFormatRet;
			mediaFormatRet = aamp->GetMediaFormatTypeEnum();
			/* Disable re-tune on buffering timeout for DASH as unlike HLS,
			DRM key acquisition can end after injection, and buffering is not expected
			to be completed by the 1 second timeout
			*/
			if (G_UNLIKELY(((mediaFormatRet != eMEDIAFORMAT_DASH) && (mediaFormatRet != eMEDIAFORMAT_PROGRESSIVE) && (mediaFormatRet != eMEDIAFORMAT_HLS_MP4)) && (privateContext->buffering_timeout_cnt == 0) && aamp->mConfig->IsConfigSet(eAAMPConfig_ReTuneOnBufferingTimeout) && (privateContext->numberOfVideoBuffersSent > 0)))
			{
				AAMPLOG_WARN("Schedule retune. numberOfVideoBuffersSent %d frames %i", privateContext->numberOfVideoBuffersSent, frames);
				privateContext->buffering_in_progress = false;
				_this->DumpDiagnostics();
				aamp->ScheduleRetune(eGST_ERROR_VIDEO_BUFFERING, eMEDIATYPE_VIDEO);
			}
			else if (frames == -1 || frames >= GETCONFIGVALUE(eAAMPConfig_RequiredQueuedFrames) || privateContext->buffering_timeout_cnt-- == 0)
			{
				AAMPLOG_MIL("Set pipeline state to %s - buffering_timeout_cnt %u  frames %i", gst_element_state_get_name(_this->privateContext->buffering_target_state), (_this->privateContext->buffering_timeout_cnt+1), frames);
				SetStateWithWarnings (_this->privateContext->pipeline, _this->privateContext->buffering_target_state);

				if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_BROADCOM)
				{
					// Setting first fractional rate as DEFAULT_INITIAL_RATE_CORRECTION_SPEED right away on PLAYING to avoid audio drop
					if (aamp->mConfig->IsConfigSet(eAAMPConfig_EnableLiveLatencyCorrection) && aamp->IsLive())
					{
						AAMPLOG_WARN("Setting first fractional rate %.6f right after moving to PLAYING", DEFAULT_INITIAL_RATE_CORRECTION_SPEED);
						_this->SetPlayBackRate(DEFAULT_INITIAL_RATE_CORRECTION_SPEED);
					}
				}
				_this->privateContext->buffering_in_progress = false;
				if(!aamp->IsGstreamerSubsEnabled())
				{
					aamp->UpdateSubtitleTimestamp();
				}
			}
		}
		if (!_this->privateContext->buffering_in_progress)
		{
			//reset timer id after buffering operation is completed
			_this->privateContext->bufferingTimeoutTimerId = AAMP_TASK_ID_INVALID;
		}
		return _this->privateContext->buffering_in_progress;
	}
	else
	{
		AAMPLOG_WARN("in buffering_timeout got invalid or NULL handle ! _this =  %p   _this->privateContext = %p ",
		_this, (_this? _this->privateContext: NULL) );
		return false;
	}
}

/**
 * @brief Called from the mainloop when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval FALSE if the event source should be removed.
 */
static gboolean bus_message(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this)
{
	UsingPlayerId playerId( _this->aamp->mPlayerId );
	HANDLER_CONTROL_HELPER( _this->privateContext->aSyncControl, FALSE);
	GError *error;
	gchar *dbg_info;
	bool isPlaybinStateChangeEvent;
	const auto platformType = _this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType);

	switch (GST_MESSAGE_TYPE(msg))
	{ // see https://developer.gnome.org/gstreamer/stable/gstreamer-GstMessage.html#GstMessage
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &error, &dbg_info);				/* Extracts the GError and debug string from the GstMessage i.e msg */
		g_printerr("GST_MESSAGE_ERROR %s: %s\n", GST_OBJECT_NAME(msg->src), error->message);
		char errorDesc[MAX_ERROR_DESCRIPTION_LENGTH];
		memset(errorDesc, '\0', MAX_ERROR_DESCRIPTION_LENGTH);
		strncpy(errorDesc, "GstPipeline Error:", 18);
		strncat(errorDesc, error->message, MAX_ERROR_DESCRIPTION_LENGTH - 18 - 1);	/* Constructs erroDesc string, describing error for further action */
		if (strstr(error->message, "video decode error") != NULL)
		{
			_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, errorDesc, false);	/* Forward the information to handle error */
		}
		else if(strstr(error->message, "HDCP Compliance Check Failure") != NULL)
		{
			// Trying to play a 4K content on a non-4K TV .Report error to XRE with no retune
			_this->aamp->SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR, errorDesc, false);	/* Forward the information to handle error */
		}
		else if (strstr(error->message, "Internal data stream error") && _this->aamp->mConfig->IsConfigSet(eAAMPConfig_RetuneForGSTError))
		{
			// This can be executed only for Peacock when it hits Internal data stream error.
			AAMPLOG_ERR("Schedule retune for GstPipeline Error");
			_this->aamp->ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO);
		}
		else if (strstr(error->message, "Error parsing H.264 stream"))
		{
			//forwarding this error causes a freeze
			AAMPLOG_WARN("%s", errorDesc);
		}
		else
		{
			_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, errorDesc);			/* Forward the information to handle error */
		}
		g_printerr("Debug Info: %s\n", (dbg_info) ? dbg_info : "none");
		g_clear_error(&error);					/* Frees the resources allocated to error and sets error to NULL */
		g_free(dbg_info);						/* Frees memory resources used by dbg_info */
		break;

	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &error, &dbg_info);			/* Extracts the GError and debug string from the GstMessage i.e msg */
		g_printerr("GST_MESSAGE_WARNING %s: %s\n", GST_OBJECT_NAME(msg->src), error->message);
		if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_DecoderUnavailableStrict)  && strstr(error->message, "No decoder available") != NULL)
		{
			char warnDesc[MAX_ERROR_DESCRIPTION_LENGTH];
			snprintf( warnDesc, MAX_ERROR_DESCRIPTION_LENGTH, "GstPipeline Error:%s", error->message );
			// decoding failures due to unsupported codecs are received as warnings, i.e.
			// "No decoder available for type 'video/x-gst-fourcc-av01"
			_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, warnDesc, false);			/* Forward the information to handle error */
		}
		g_printerr("Debug Info: %s\n", (dbg_info) ? dbg_info : "none");
		g_clear_error(&error);						/* Frees the resources allocated to error and sets error to NULL */
		g_free(dbg_info);							/* Frees memory resources used by dbg_info */
		break;

	case GST_MESSAGE_EOS:
		/**
		 * pipeline event: end-of-stream reached
		 * application may perform flushing seek to resume playback
		 */
		AAMPLOG_MIL("GST_MESSAGE_EOS");
		_this->NotifyEOS();
		break;

	case GST_MESSAGE_STATE_CHANGED:
	{
		GstState old_state, new_state, pending_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);		/* Extracts the old and new states from the GstMessage.*/

		isPlaybinStateChangeEvent = (GST_MESSAGE_SRC(msg) == GST_OBJECT(_this->privateContext->pipeline));

		if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_GSTLogging) || isPlaybinStateChangeEvent)
		{
			AAMPLOG_MIL("%s %s -> %s (pending %s)",
				GST_OBJECT_NAME(msg->src),
				gst_element_state_get_name(old_state),
				gst_element_state_get_name(new_state),
				gst_element_state_get_name(pending_state));
			if (isPlaybinStateChangeEvent && _this->privateContext->pauseOnStartPlayback && (new_state == GST_STATE_PAUSED))
			{
				GstElement *video_sink = _this->privateContext->video_sink;
				const char *frame_step_on_preroll_prop = "frame-step-on-preroll";

				_this->privateContext->pauseOnStartPlayback = false;

				if (video_sink && (g_object_class_find_property(G_OBJECT_GET_CLASS(video_sink), frame_step_on_preroll_prop) != NULL))
				{
					AAMPLOG_INFO("Setting %s property and sending step", frame_step_on_preroll_prop);
					g_object_set(G_OBJECT(video_sink), frame_step_on_preroll_prop,1, NULL);
			    	// From testing, step only required for a specific platform but harmless for others
					if (!gst_element_send_event(video_sink, gst_event_new_step(GST_FORMAT_BUFFERS, 1, 1.0, FALSE, FALSE)))
					{
						AAMPLOG_ERR("error sending step event");
					}
					g_object_set(G_OBJECT(video_sink), frame_step_on_preroll_prop,0, NULL);

					if (_this->privateContext->usingRialtoSink)
					{
						_this->privateContext->firstVideoFrameReceived = true;
						_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);
					}
				}
				else
				{
					// Property not available on platform so simulating first video frame received
					AAMPLOG_WARN("%s property not present on video_sink", frame_step_on_preroll_prop);
					_this->privateContext->firstVideoFrameReceived = true;
					_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);
				}
			}
			if (isPlaybinStateChangeEvent && new_state == GST_STATE_PLAYING)
			{
				_this->privateContext->pauseOnStartPlayback = false;

				if(platformType == ePLATFORM_AMLOGIC)
				{
					//To support first frame notification on audioOnlyPlayback for hls streams on this platform
					if(_this->aamp->mAudioOnlyPb && !_this->privateContext->firstAudioFrameReceived && _this->privateContext->NumberOfTracks==1)
					{
						media_stream *stream = &_this->privateContext->stream[eMEDIATYPE_AUDIO];
						g_object_get(stream->sinkbin, "n-audio", &_this->privateContext->n_audio, NULL);
						if(_this->privateContext->n_audio > 0)
						{
							AAMPLOG_MIL("Audio only playback detected, hence notify first frame");
							_this->privateContext->firstAudioFrameReceived = true;
							_this->NotifyFirstFrame(eMEDIATYPE_AUDIO);
						}
					}
				}

				// progressive ff case, notify to update trickStartUTCMS
				if (_this->aamp->mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
				{
					_this->aamp->NotifyFirstBufferProcessed(_this->GetVideoRectangle());
					_this->IdleTaskAdd(_this->privateContext->firstProgressCallbackIdleTask, IdleCallback);
				}
				if (_this->privateContext->usingRialtoSink)
				{
					_this->privateContext->firstVideoFrameReceived = true;
					_this->privateContext->firstAudioFrameReceived = true;
					_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);
				}
				if(platformType == ePLATFORM_REALTEK)
				{// For  westeros-sink disabled
				 // prevent calling NotifyFirstFrame after first tune, ie when upausing
				 // pipeline during flush
					if(_this->privateContext->firstTuneWithWesterosSinkOff)
					{
						_this->privateContext->firstTuneWithWesterosSinkOff = false;
						_this->privateContext->firstVideoFrameReceived = true;
						_this->privateContext->firstAudioFrameReceived = true;
						_this->NotifyFirstFrame(eMEDIATYPE_VIDEO);
					}
				}
				if( platformType == ePLATFORM_DEFAULT )
				{
					if(!_this->privateContext->firstFrameReceived)
					{
						_this->privateContext->firstFrameReceived = true;
						_this->aamp->LogFirstFrame();
						_this->aamp->LogTuneComplete();
					}
					_this->aamp->NotifyFirstFrameReceived(_this->getCCDecoderHandle());
					//Note: Progress event should be sent after the decoderAvailable event only.
					//BRCM platform sends progress event after AAMPGstPlayer_OnFirstVideoFrameCallback.
					_this->IdleTaskAdd(_this->privateContext->firstProgressCallbackIdleTask, IdleCallback);
				}
				if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_GSTLogging))
				{
					GST_DEBUG_BIN_TO_DOT_FILE((GstBin *)_this->privateContext->pipeline, GST_DEBUG_GRAPH_SHOW_ALL, "myplayer");
					// output graph to .dot format which can be visualized with Graphviz tool if:
					// gstreamer is configured with --gst-enable-gst-debug
					// and "gst" is enabled in aamp.cfg
					// and environment variable GST_DEBUG_DUMP_DOT_DIR is set to a basepath(e.g. /opt).
				}

				// First Video Frame Displayed callback for westeros-sink is initialized
				// via OnFirstVideoFrameCallback()->NotifyFirstFrame() which is more accurate
				if( (!_this->privateContext->using_westerossink)
					&& (_this->aamp->IsFirstVideoFrameDisplayedRequired()) )
				{
					_this->IdleTaskAdd(_this->privateContext->firstVideoFrameDisplayedCallbackTask, IdleCallbackFirstVideoFrameDisplayed);
				}

				if(_this->aamp->mSetPlayerRateAfterFirstframe 
						|| ((platformType == ePLATFORM_REALTEK || platformType == ePLATFORM_BROADCOM) && ((AAMP_SLOWMOTION_RATE == _this->aamp->playerrate) && (_this->aamp->rate == AAMP_NORMAL_PLAY_RATE))))
				{
					StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(_this->aamp);
					if (sink)
					{
						if(_this->aamp->mSetPlayerRateAfterFirstframe)
						{
							_this->aamp->mSetPlayerRateAfterFirstframe=false;

							if(false != sink->SetPlayBackRate(_this->aamp->playerrate))
							{
								_this->aamp->rate=_this->aamp->playerrate;
								_this->aamp->SetAudioVolume(0);
							}
						}
						else if (platformType == ePLATFORM_REALTEK || platformType == ePLATFORM_BROADCOM)
						{
							if(false != sink->SetPlayBackRate(_this->aamp->rate))
							{
								_this->aamp->playerrate=_this->aamp->rate;
							}
						}
					}
				}
			}
		}
		if ((NULL != msg->src) && AAMPGstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), _this))
		{
#ifdef AAMP_MPD_DRM
			// This is the video decoder, send this to the output protection module
			// so it can get the source width/height
			if (AAMPGstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), _this))
			{
				if(AampOutputProtection::IsAampOutputProcectionInstanceActive())
				{
					AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();
					pInstance->setGstElement((GstElement *)(msg->src));
					pInstance->Release();
				}
			}
#endif
		}
#if GST_CHECK_VERSION(1,18,0)
// avoid compilation failure if building with Ubuntu 20.04 having gst<1.18
		else if ((platformType == ePLATFORM_AMLOGIC) && NULL != msg->src)
		{
			if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
			{
				if(aamp_StartsWith(GST_OBJECT_NAME(msg->src), "source"))
				{
					GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(msg->src), "src");
					if (sourceEleSrcPad)
					{
						gst_pad_add_probe (
								sourceEleSrcPad,
								GST_PAD_PROBE_TYPE_EVENT_BOTH,
								AAMPGstPlayer_HandleInstantRateChangeSeekProbe,
								gst_segment_new(),
								reinterpret_cast<GDestroyNotify>(gst_segment_free));
						gst_object_unref(sourceEleSrcPad);
					}
				}
			}
		}
#endif
		if ((NULL != msg->src) && ((platformType == ePLATFORM_REALTEK && AAMPGstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), _this)) || (platformType != ePLATFORM_REALTEK && AAMPGstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), _this))) && (!_this->privateContext->usingRialtoSink))
		{
			if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
			{
				_this->SignalConnect(msg->src, "buffer-underflow-callback",
						G_CALLBACK(AAMPGstPlayer_OnGstBufferUnderflowCb), _this);			/* Sets up the call back function on 'buffer-underflow-callback' event */
				_this->SignalConnect(msg->src, "pts-error-callback",
						G_CALLBACK(AAMPGstPlayer_OnGstPtsErrorCb), _this);
				if (platformType != ePLATFORM_REALTEK && AAMPGstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), _this))
				{
					// To register decode-error-callback for video decoder source alone
					_this->SignalConnect(msg->src, "decode-error-callback",
							G_CALLBACK(AAMPGstPlayer_OnGstDecodeErrorCb), _this);
				}
			}
		}

		if ((NULL != msg->src) &&
			((aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rialtomsevideosink") == true) ||
			(aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rialtomseaudiosink") == true)))
		{
            if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
			{
				g_signal_connect(msg->src, "buffer-underflow-callback",
					G_CALLBACK(AAMPGstPlayer_OnGstBufferUnderflowCb), _this);		/* Sets up the call back function on 'buffer-underflow-callback' event */
			}
		}
	}
		break;

	case GST_MESSAGE_TAG:
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
	{
		 /* In this case, the current clock as selected by the pipeline has become unusable. The pipeline will select a new clock on the next PLAYING state change.
			As per the gstreamer.desktop org, the application should set the pipeline to PAUSED and back to PLAYING when GST_MESSAGE_CLOCK_LOST is received.
			During DASH playback (e.g. when the pipeline is torn down on transition to trickplay), this is done elsewhere. */
		MediaFormat mediaFormat = _this->aamp->GetMediaFormatTypeEnum();				/* Get the Media format type of current media */
		AAMPLOG_WARN("GST_MESSAGE_CLOCK_LOST");
		if (mediaFormat != eMEDIAFORMAT_DASH)
		{
			// get new clock - needed?
			SetStateWithWarnings(_this->privateContext->pipeline, GST_STATE_PAUSED);
			SetStateWithWarnings(_this->privateContext->pipeline, GST_STATE_PLAYING);
		}
		break;
	}

	case GST_MESSAGE_RESET_TIME:		/* Message from pipeline to request resetting its running time */
#ifdef TRACE
		GstClockTime running_time;
		gst_message_parse_reset_time (msg, &running_time);
		printf("GST_MESSAGE_RESET_TIME %llu\n", (unsigned long long)running_time);
#endif
		break;

	case GST_MESSAGE_STREAM_STATUS:
	case GST_MESSAGE_ELEMENT: // can be used to collect pts, dts, pid
	case GST_MESSAGE_DURATION:
	case GST_MESSAGE_LATENCY:
		break;
	case GST_MESSAGE_NEW_CLOCK:
		AAMPLOG_DEBUG("GST_MESSAGE_NEW_CLOCK element:%s", GST_OBJECT_NAME(msg->src));
		break;
	case GST_MESSAGE_APPLICATION:
		const GstStructure *msgS;
		msgS = gst_message_get_structure (msg);
		if (gst_structure_has_name (msgS, "HDCPProtectionFailure")) {
			AAMPLOG_ERR("Received HDCPProtectionFailure event.Schedule Retune ");
			_this->Flush(0, AAMP_NORMAL_PLAY_RATE, true);
			_this->aamp->ScheduleRetune(eGST_ERROR_OUTPUT_PROTECTION_ERROR,eMEDIATYPE_VIDEO);
		}
		break;

	default:
		AAMPLOG_WARN("msg type %s not supported", gst_message_type_get_name(msg->type));
		break;
	}
	return TRUE;
}


/**
 * @brief Invoked synchronously when a message is available on the bus
 * @param[in] bus the GstBus that sent the message
 * @param[in] msg the GstMessage
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval GST_BUS_PASS to pass the message to the async queue
 */
static GstBusSyncReply bus_sync_handler(GstBus * bus, GstMessage * msg, AAMPGstPlayer * _this)
{
	UsingPlayerId playerId( _this->aamp->mPlayerId );
	HANDLER_CONTROL_HELPER( _this->privateContext->syncControl, GST_BUS_PASS);
	switch(GST_MESSAGE_TYPE(msg))
	{
	case GST_MESSAGE_STATE_CHANGED:
		GstState old_state, new_state;
		gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);

		if (GST_MESSAGE_SRC(msg) == GST_OBJECT(_this->privateContext->pipeline))
		{
			_this->privateContext->pipelineState = new_state;
		}

		/* Moved the below code block from bus_message() async handler to bus_sync_handler()
		 * to avoid a timing case crash when accessing wrong video_sink element after it got deleted during pipeline reconfigure on codec change in mid of playback.
		 */
		if (new_state == GST_STATE_PAUSED && old_state == GST_STATE_READY)
		{
			if (AAMPGstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), _this))
			{ // video scaling patch
				/*
				 brcmvideosink doesn't sets the rectangle property correct by default
				 gst-inspect-1.0 brcmvideosink
				 g_object_get(_this->privateContext->pipeline, "video-sink", &videoSink, NULL); - reports NULL
				 note: alternate "window-set" works as well
				 */
				gst_object_replace((GstObject **)&_this->privateContext->video_sink, msg->src);

				if (_this->privateContext->usingRialtoSink)
				{
					if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_EnableRectPropertyCfg))
					{
						AAMPLOG_MIL("AAMPGstPlayer - using %s, setting cached rectangle and video mute", GST_OBJECT_NAME(msg->src));
						g_object_set(msg->src, "rectangle", _this->privateContext->videoRectangle, NULL);
					}
					else
					{
						AAMPLOG_MIL("AAMPGstPlayer - using %s, setting video mute", GST_OBJECT_NAME(msg->src));
					}
					g_object_set(msg->src, "show-video-window", !_this->privateContext->videoMuted, NULL);
				}
				else if (_this->privateContext->using_westerossink && !_this->aamp->mConfig->IsConfigSet(eAAMPConfig_EnableRectPropertyCfg))
				{
					AAMPLOG_MIL("AAMPGstPlayer - using westerossink, setting cached video mute and zoom");
					g_object_set(msg->src, "zoom-mode", _this->privateContext->zoom, NULL );
					g_object_set(msg->src, "show-video-window", !_this->privateContext->videoMuted, NULL);
				}
				else
				{
					AAMPLOG_MIL("AAMPGstPlayer setting cached rectangle, video mute and zoom");
					g_object_set(msg->src, "rectangle", _this->privateContext->videoRectangle, NULL);
					g_object_set(msg->src, "zoom-mode", _this->privateContext->zoom, NULL );
					g_object_set(msg->src, "show-video-window", !_this->privateContext->videoMuted, NULL);
				}
			}
			else if ((aamp_StartsWith(GST_OBJECT_NAME(msg->src), "brcmaudiosink") == true)
					 || (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rialtomseaudiosink") == true))
			{
				gst_object_replace((GstObject **)&_this->privateContext->audio_sink, msg->src);
				_this->setVolumeOrMuteUnMute();
			}
			else if (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "amlhalasink") == true)
			{
				gst_object_replace((GstObject **)&_this->privateContext->audio_sink, msg->src);

				g_object_set(_this->privateContext->audio_sink, "disable-xrun", TRUE, NULL);
				// Apply audio settings that may have been set before pipeline was ready
				_this->setVolumeOrMuteUnMute();
			}
			else if (strstr(GST_OBJECT_NAME(msg->src), "brcmaudiodecoder"))
			{
				// this reduces amount of data in the fifo, which is flushed/lost when transition from expert to normal modes
				g_object_set(msg->src, "limit_buffering_ms", 1500, NULL);   /* default 500ms was a bit low.. try 1500ms */
				g_object_set(msg->src, "limit_buffering", 1, NULL);
				AAMPLOG_MIL("Found audiodecoder, limiting audio decoder buffering");

				/* if aamp->mAudioDecoderStreamSync==false, tell decoder not to look for 2nd/next frame sync, decode if it finds a single frame sync */
				g_object_set(msg->src, "stream_sync_mode", (_this->aamp->mAudioDecoderStreamSync)? 1 : 0, NULL);
				AAMPLOG_MIL("For audiodecoder set 'stream_sync_mode': %d", _this->aamp->mAudioDecoderStreamSync);
			}
			else if ((_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK) && (aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rtkaudiosink")
					 || aamp_StartsWith(GST_OBJECT_NAME(msg->src), "alsasink")
					 || aamp_StartsWith(GST_OBJECT_NAME(msg->src), "fakesink")))
			{
				gst_object_replace((GstObject **)&_this->privateContext->audio_sink, msg->src);
				// Apply audio settings that may have been set before pipeline was ready
				_this->setVolumeOrMuteUnMute();
			}
		}
		if (old_state == GST_STATE_NULL && new_state == GST_STATE_READY)
		{
			if ((NULL != msg->src) && AAMPGstPlayer_isVideoOrAudioDecoder(GST_OBJECT_NAME(msg->src), _this))
			{
				if (AAMPGstPlayer_isVideoDecoder(GST_OBJECT_NAME(msg->src), _this))
				{
					gst_object_replace((GstObject **)&_this->privateContext->video_dec, msg->src);
					type_check_instance("bus_sync_handle: video_dec ", _this->privateContext->video_dec);
					_this->SignalConnect(_this->privateContext->video_dec, "first-video-frame-callback",
									G_CALLBACK(AAMPGstPlayer_OnFirstVideoFrameCallback), _this);
					if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) != ePLATFORM_REALTEK)
					{
                                        	g_object_set(msg->src, "report_decode_errors", TRUE, NULL);
					}

				}
				else
				{
					gst_object_replace((GstObject **)&_this->privateContext->audio_dec, msg->src);
					type_check_instance("bus_sync_handle: audio_dec ", _this->privateContext->audio_dec);
					
					if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) != ePLATFORM_REALTEK)
					{
						_this->SignalConnect(msg->src, "first-audio-frame-callback",
									G_CALLBACK(AAMPGstPlayer_OnAudioFirstFrameAudDecoder), _this);
					}
					int trackId = _this->privateContext->stream[eMEDIATYPE_AUDIO].trackId;
					if (trackId >= 0) /** AC4 track selected **/
					{
						if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) != ePLATFORM_BROADCOM)
						{
							/** AC4 support added for non Broadcom platforms */
							AAMPLOG_INFO("Selecting AC4 Track Id : %d", trackId);
							g_object_set(msg->src, "ac4-presentation-group-index", trackId, NULL);
						}
						else
						{
							AAMPLOG_WARN("AC4 support has not done for this platform - track Id: %d", trackId);
						}
					}
				}
			}
			if ((NULL != msg->src) &&
					AAMPGstPlayer_isVideoSink(GST_OBJECT_NAME(msg->src), _this) &&
					(!_this->privateContext->usingRialtoSink))
			{
				if(_this->privateContext->enableSEITimeCode)
				{
					g_object_set(msg->src, "enable-timecode", 1, NULL);
					_this->SignalConnect(msg->src, "timecode-callback",
							G_CALLBACK(AAMPGstPlayer_redButtonCallback), _this);
				}

				if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
				{
					g_object_set(msg->src, "freerun-threshold", DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS, NULL);
				}

			}
			if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
			{
				if ((NULL != msg->src) && aamp_StartsWith(GST_OBJECT_NAME(msg->src), "rtkaudiosink"))
					_this->SignalConnect(msg->src, "first-audio-frame",
							G_CALLBACK(AAMPGstPlayer_OnAudioFirstFrameAudDecoder), _this);
			}
			/*This block is added to share the PrivateInstanceAAMP object
			  with PlayReadyDecryptor Plugin, for tune time profiling

			  AAMP is added as a property of playready plugin
			*/
			if ((NULL != msg->src) &&
			  (aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNamePR) == true ||
			   aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameWV) == true ||
			   aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameCK) == true ||
			   aamp_StartsWith(GST_OBJECT_NAME(msg->src), GstPluginNameVMX) == true))
			{
				AAMPLOG_MIL("AAMPGstPlayer setting encrypted aamp (%p) instance for %s decryptor", _this->mEncryptedAamp, GST_OBJECT_NAME(msg->src));
				GValue val = { 0, };
				g_value_init(&val, G_TYPE_POINTER);
				g_value_set_pointer(&val, (gpointer) _this->mEncryptedAamp);
				g_object_set_property(G_OBJECT(msg->src), "aamp", &val);
			}
		}
		break;
	case GST_MESSAGE_NEED_CONTEXT:

		/*
		 * Code to avoid logs flooding with NEED-CONTEXT message for DRM systems
		 */
		const gchar* contextType;
		gst_message_parse_context_type(msg, &contextType);
		if (!g_strcmp0(contextType, "drm-preferred-decryption-system-id"))
		{
			AAMPLOG_MIL("Setting %s as preferred drm",GetDrmSystemName(_this->aamp->GetPreferredDRM()));
			GstContext* context = gst_context_new("drm-preferred-decryption-system-id", FALSE);
			GstStructure* contextStructure = gst_context_writable_structure(context);	/* Gets a writeable structure of context, context still own the structure*/
			gst_structure_set(contextStructure, "decryption-system-id", G_TYPE_STRING, GetDrmSystemID(_this->aamp->GetPreferredDRM()),  NULL);
			gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(msg)), context);
/* TODO: Fix this once preferred DRM is correct
			_this->aamp->setCurrentDrm(_this->aamp->GetPreferredDRM());
 */
		}

		break;

	case GST_MESSAGE_ASYNC_DONE:
		AAMPLOG_INFO("Received GST_MESSAGE_ASYNC_DONE message");
		if (_this->privateContext->buffering_in_progress)
		{
			_this->privateContext->bufferingTimeoutTimerId = g_timeout_add_full(BUFFERING_TIMEOUT_PRIORITY, DEFAULT_BUFFERING_TO_MS, buffering_timeout, _this, NULL);
		}

		break;

	case GST_MESSAGE_STREAM_STATUS:
		if(_this->privateContext->task_pool)
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
					if (task && _this->privateContext->task_pool)
					{
						gst_task_set_pool(task, _this->privateContext->task_pool);
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

	return GST_BUS_PASS;		/* pass the message to the async queue */
}

/**
 *  @brief Create a new Gstreamer pipeline
 */
bool AAMPGstPlayer::CreatePipeline()
{
	bool ret = false;
	/* Destroy any existing pipeline before creating a new one */
	if (privateContext->pipeline || privateContext->bus)
	{
		DestroyPipeline();
	}

	/*Each "Creating gstreamer pipeline" should be paired with one, subsequent "Destroying gstreamer pipeline" log entry.
	"Creating gstreamer pipeline" is intentionally placed after the DestroyPipeline() call above to maintain this sequence*/
	AAMPLOG_MIL("Creating gstreamer pipeline");
	privateContext->pipeline = gst_pipeline_new("AAMPGstPlayerPipeline");
	if (privateContext->pipeline)
	{
		privateContext->bus = gst_pipeline_get_bus(GST_PIPELINE(privateContext->pipeline));		/*Gets the GstBus of pipeline. The bus allows applications to receive GstMessage packets.*/
		const char *envVal = getenv("AAMP_AV_PIPELINE_PRIORITY");
		if(envVal)
		{
			privateContext->task_pool =  (GstTaskPool*)g_object_new (GST_TYPE_AAMP_TASKPOOL, NULL);
		}
		if (privateContext->bus)
		{
			privateContext->aSyncControl.enable();
			guint busWatchId = gst_bus_add_watch(privateContext->bus, (GstBusFunc) bus_message, this); /* Creates a watch for privateContext->bus, invoking 'bus_message' when a asynchronous message on the bus is available */
			(void)busWatchId;
			privateContext->syncControl.enable();
			gst_bus_set_sync_handler(privateContext->bus, (GstBusSyncHandler) bus_sync_handler, this, NULL);	/* Assigns a synchronous bus_sync_handler for synchronous messages */
			privateContext->buffering_enabled = ISCONFIGSET(eAAMPConfig_GStreamerBufferingBeforePlay);
			privateContext->buffering_in_progress = false;
			privateContext->buffering_timeout_cnt = DEFAULT_BUFFERING_MAX_CNT;
			privateContext->buffering_target_state = GST_STATE_NULL;
			AAMPLOG_MIL("%s buffering_enabled %u", GST_ELEMENT_NAME(privateContext->pipeline), privateContext->buffering_enabled);
			if (privateContext->positionQuery == NULL)
			{
				/* Construct a new position query that will used to query the 'current playback position' when needed.
					The time base specified is in nanoseconds */
				privateContext->positionQuery = gst_query_new_position(GST_FORMAT_TIME);
			}

			/* Use to enable the timing synchronisation with gstreamer */
			privateContext->enableSEITimeCode = ISCONFIGSET(eAAMPConfig_SEITimeCode);
			ret = true;
		}
		else
		{
			AAMPLOG_ERR("AAMPGstPlayer - gst_pipeline_get_bus failed");
		}
	}
	else
	{
		AAMPLOG_ERR("AAMPGstPlayer - gst_pipeline_new failed");
	}

	return ret;
}

/**
 *  @brief Cleanup an existing Gstreamer pipeline and associated resources
 */
void AAMPGstPlayer::DestroyPipeline()
{
	if (privateContext->pipeline)
	{
		/*"Destroying gstreamer pipeline" should only be logged when there is a pipeline to destroy
		  and each "Destroying gstreamer pipeline" log entry should have one, prior "Creating gstreamer pipeline" log entry*/
		AAMPLOG_MIL("Destroying gstreamer pipeline");
		gst_object_unref(privateContext->pipeline);		/* Decreases the reference count on privateContext->pipeline, in this case it will become zero,
															the reference to privateContext->pipeline will be freed in gstreamer */
		privateContext->pipeline = NULL;
	}
	if (privateContext->bus)
	{
		gst_bus_remove_watch(privateContext->bus);
		gst_object_unref(privateContext->bus);		/* Decreases the reference count on privateContext->bus, in this case it will become zero,
															the reference to privateContext->bus will be freed in gstreamer */
		privateContext->bus = NULL;
	}
	if(privateContext->task_pool)
	{
		gst_object_unref(privateContext->task_pool);
		privateContext->task_pool = NULL;
	}

	if (privateContext->positionQuery)
	{
		/* Decrease the refcount of the query. If the refcount reaches 0, the query will be freed */
		gst_query_unref(privateContext->positionQuery);
		privateContext->positionQuery = NULL;
	}

	//video decoder handle will change with new pipeline
	privateContext->decoderHandleNotified = false;
	privateContext->NumberOfTracks = 0;
}

/**
 *  @brief Retrieve the video decoder handle from pipeline
 */
unsigned long AAMPGstPlayer::getCCDecoderHandle()
{
	gpointer dec_handle = NULL;
	if(this->privateContext->video_dec != NULL)
	{
		AAMPLOG_MIL("Querying video decoder for handle");
		if(this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
		{
			dec_handle = this->privateContext->video_dec;
		}
		else
		{
			g_object_get(this->privateContext->video_dec, "videodecoder", &dec_handle, NULL);
		}
	}
	AAMPLOG_MIL("video decoder handle received %p for video_dec %p", dec_handle, privateContext->video_dec);
	return (unsigned long)dec_handle;
}

/**
 *  @brief Generate a protection event
 */
void AAMPGstPlayer::QueueProtectionEvent(const char *protSystemId, const void *initData, size_t initDataSize, AampMediaType type)
{
#ifdef AAMP_MPD_DRM
	/* There is a possibility that only single protection event is queued for multiple type since they are encrypted using same id.
	 * Don't worry if you see only one protection event queued here.
	 */
	pthread_mutex_lock(&mProtectionLock);
	if (privateContext->protectionEvent[type] != NULL)
	{
		AAMPLOG_MIL("Previously cached protection event is present for type(%d), clearing!", type);
		gst_event_unref(privateContext->protectionEvent[type]);
		privateContext->protectionEvent[type] = NULL;
	}
	pthread_mutex_unlock(&mProtectionLock);

	AAMPLOG_MIL("Queueing protection event for type(%d) keysystem(%s) initData(%p) initDataSize(%zu)", type, protSystemId, initData, initDataSize);

	/* Giving invalid initData into ProtectionEvent causing "GStreamer-CRITICAL" assertion error. So if the initData is valid then its good to call the ProtectionEvent further. */
	if (initData && initDataSize)
	{
		GstBuffer *pssi;

		pssi = gst_buffer_new_wrapped(AAMP_G_MEMDUP (initData, initDataSize), (gsize)initDataSize);
		pthread_mutex_lock(&mProtectionLock);
		if (this->aamp->IsDashAsset())
		{
			privateContext->protectionEvent[type] = gst_event_new_protection (protSystemId, pssi, "dash/mpd");
		}
		else
		{
			privateContext->protectionEvent[type] = gst_event_new_protection (protSystemId, pssi, "hls/m3u8");
		}
		pthread_mutex_unlock(&mProtectionLock);

		gst_buffer_unref (pssi);
	}
#endif
}

/**
 *  @brief Cleanup generated protection event
 */
void AAMPGstPlayer::ClearProtectionEvent()
{
	pthread_mutex_lock(&mProtectionLock);
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		if(privateContext->protectionEvent[i])
		{
			AAMPLOG_MIL("removing protection event! ");
			gst_event_unref (privateContext->protectionEvent[i]);
			privateContext->protectionEvent[i] = NULL;
		}
	}
	pthread_mutex_unlock(&mProtectionLock);
}

/**
 * @brief Create an appsrc element for a particular format
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @param[in] mediaType media type
 * @retval pointer to appsrc instance
 */
static GstElement* AAMPGstPlayer_GetAppSrc(AAMPGstPlayer *_this, AampMediaType mediaType)
{
	GstElement *source;
	source = gst_element_factory_make("appsrc", NULL);
	if (NULL == source)
	{
		AAMPLOG_WARN("AAMPGstPlayer_GetAppSrc Cannot create source");
		return NULL;
	}
	InitializeSource(_this, G_OBJECT(source), mediaType);

	return source;
}

static void AAMPGstPlayer_SignalEOS(media_stream* stream);

/**
 * @fn RemoveProbes
 * @brief Remove probes from the pipeline
 */
void AAMPGstPlayer::RemoveProbes()
{
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		media_stream *stream = &privateContext->stream[(AampMediaType)i];
		if (stream->demuxProbeId && stream->demuxPad)
		{
			gst_pad_remove_probe(stream->demuxPad, stream->demuxProbeId);
			stream->demuxProbeId = 0;
			stream->demuxPad = NULL;
		}
	}
}

/**
 *  @brief Cleanup resources and flags for a particular stream type
 */
void AAMPGstPlayer::TearDownStream(AampMediaType mediaType)
{
	media_stream* stream = &privateContext->stream[mediaType];
	stream->mBufferControl.teardownStart();
	stream->bufferUnderrun = false;
	stream->eosReached = false;
	if (stream->format != FORMAT_INVALID)
	{
		pthread_mutex_lock(&stream->sourceLock);
		if (privateContext->pipeline)
		{
			privateContext->buffering_in_progress = false;   /* stopping pipeline, don't want to change state if GST_MESSAGE_ASYNC_DONE message comes in */
			/* set the playbin state to NULL before detach it */
			if (stream->sinkbin)
			{
				if (GST_STATE_CHANGE_FAILURE == SetStateWithWarnings(GST_ELEMENT(stream->sinkbin), GST_STATE_NULL))
				{
					AAMPLOG_ERR("AAMPGstPlayer::TearDownStream: Failed to set NULL state for sinkbin");
				}
				if (!gst_bin_remove(GST_BIN(privateContext->pipeline), GST_ELEMENT(stream->sinkbin)))			/* Removes the sinkbin element from the pipeline */
				{
					AAMPLOG_ERR("AAMPGstPlayer::TearDownStream:  Unable to remove sinkbin from pipeline");
				}
			}
			else
			{
				AAMPLOG_WARN("AAMPGstPlayer::TearDownStream:  sinkbin = NULL, skip remove sinkbin from pipeline");
			}
		}
		//After sinkbin is removed from pipeline, a new decoder handle may be generated
		if (mediaType == eMEDIATYPE_VIDEO)
		{
			privateContext->decoderHandleNotified = false;
		}
		stream->format = FORMAT_INVALID;
		g_clear_object(&stream->sinkbin);
		g_clear_object(&stream->source);
		stream->sourceConfigured = false;
		pthread_mutex_unlock(&stream->sourceLock);
	}
	if (mediaType == eMEDIATYPE_VIDEO)
	{
		g_clear_object(&privateContext->video_dec);
		g_clear_object(&privateContext->video_sink);
	}
	else if (mediaType == eMEDIATYPE_AUDIO)
	{
		g_clear_object(&privateContext->audio_dec);
		g_clear_object(&privateContext->audio_sink);
	}
	else if (mediaType == eMEDIATYPE_SUBTITLE)
	{
		g_clear_object(&privateContext->subtitle_sink);
	}

	stream->mBufferControl.teardownEnd();

	AAMPLOG_MIL("AAMPGstPlayer::TearDownStream: exit mediaType = %d", mediaType);
}

static void callback_element_added (GstElement * element, GstElement * source, gpointer data)
{
    AAMPGstPlayer * _this = (AAMPGstPlayer *)data;
	HANDLER_CONTROL_HELPER_CALLBACK_VOID();
    AAMPLOG_INFO("callback_element_added: %s",GST_ELEMENT_NAME(source));
    if (element == _this->privateContext->stream[eMEDIATYPE_AUX_AUDIO].sinkbin)
    {
        if ((strstr(GST_ELEMENT_NAME(source), "omxaacdec") != NULL) ||
            (strstr(GST_ELEMENT_NAME(source), "omxac3dec") != NULL) ||
            (strstr(GST_ELEMENT_NAME(source), "omxeac3dec") != NULL) ||
            (strstr(GST_ELEMENT_NAME(source), "omxmp3dec") != NULL) ||
            (strstr(GST_ELEMENT_NAME(source), "omxvorbisdec") != NULL) ||
            (strstr(GST_ELEMENT_NAME(source), "omxac4dec") != NULL))
        {
            g_object_set(source, "audio-tunnel-mode", FALSE, NULL );
            AAMPLOG_INFO("callback_element_added audio-tunnel-mode FALSE");
            g_object_set(source, "aux-audio", TRUE, NULL );
            AAMPLOG_INFO("callback_element_added aux-audio TRUE");
        }
    }
}

#define NO_PLAYBIN 1
/**
 * @brief Setup pipeline for a particular stream type
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @param[in] streamId stream type
 * @retval 0, if setup successfully. -1, for failure
 */
static int AAMPGstPlayer_SetupStream(AAMPGstPlayer *_this, AampMediaType streamId)
{
	media_stream* stream = &_this->privateContext->stream[streamId];
	if (eMEDIATYPE_SUBTITLE == streamId)
	{
		if(_this->aamp->IsGstreamerSubsEnabled())
		{
			_this->aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);					/* Stop any ongoing downloads before setting up a new subtitle stream */
			if (_this->privateContext->usingRialtoSink )
			{
				stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(gst_element_factory_make("playbin", NULL)));
				AAMPLOG_INFO("subs using rialto subtitle sink");
				GstElement* textsink = gst_element_factory_make("rialtomsesubtitlesink", NULL);
				if (textsink)
				{
					AAMPLOG_INFO("Created rialtomsesubtitlesink: %s", GST_ELEMENT_NAME(textsink));
				}
				else
				{
					AAMPLOG_WARN("Failed to create rialtomsesubtitlesink");
				}
				auto subtitlebin = gst_bin_new("subtitlebin");
				auto vipertransform = gst_element_factory_make("vipertransform", NULL);
				gst_bin_add_many(GST_BIN(subtitlebin),vipertransform,textsink,NULL);
				gst_element_link(vipertransform, textsink);
				gst_element_add_pad(subtitlebin, gst_ghost_pad_new("sink", gst_element_get_static_pad(vipertransform, "sink")));

				g_object_set(stream->sinkbin, "text-sink", subtitlebin, NULL);
                                _this->privateContext->subtitle_sink = textsink;
				AAMPLOG_MIL("using rialtomsesubtitlesink muted=%d sink=%p", _this->privateContext->subtitleMuted, _this->privateContext->subtitle_sink);
				g_object_set(textsink, "mute", _this->privateContext->subtitleMuted ? TRUE : FALSE, NULL);
			}
			else
			{
#ifdef NO_PLAYBIN
			AAMPLOG_INFO("subs using subtecbin");
			stream->sinkbin = gst_element_factory_make("subtecbin", NULL);			/* Creates a new element of "subtecbin" type and returns a new GstElement */
			if (!stream->sinkbin)													/* When a new element can not be created a NULL is returned */
			{
				AAMPLOG_WARN("Cannot set up subtitle subtecbin");
				return -1;
			}
			stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(stream->sinkbin));	/* Retain a counted reference to sinkbin. */
			g_object_set(G_OBJECT(stream->sinkbin), "sync", FALSE, NULL);

			stream->source = GST_ELEMENT(gst_object_ref_sink(AAMPGstPlayer_GetAppSrc(_this, eMEDIATYPE_SUBTITLE)));
			gst_bin_add_many(GST_BIN(_this->privateContext->pipeline), stream->source, stream->sinkbin, NULL);		/* Add source and sink to the current pipeline */

			if (!gst_element_link_many(stream->source, stream->sinkbin, NULL))			/* forms a GstElement link chain; linking stream->source to stream->sinkbin */
			{
				AAMPLOG_ERR("Failed to link subtitle elements");
				return -1;
			}

			gst_element_sync_state_with_parent(stream->source);
			gst_element_sync_state_with_parent(stream->sinkbin);
			_this->privateContext->subtitle_sink = GST_ELEMENT(gst_object_ref(stream->sinkbin));
			g_object_set(stream->sinkbin, "mute", _this->privateContext->subtitleMuted ? TRUE : FALSE, NULL);

			return 0;
#else
			AAMPLOG_INFO("subs using playbin");
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
		AAMPLOG_INFO("using playbin");						/* Media is not subtitle, use the generic playbin */
		stream->sinkbin = GST_ELEMENT(gst_object_ref_sink(gst_element_factory_make("playbin", NULL)));	/* Creates a new element of "playbin" type and returns a new GstElement */

		if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_useTCPServerSink) )
		{
			AAMPLOG_INFO("using tcpserversink");
			GstElement* sink = gst_element_factory_make("tcpserversink", NULL);
			int tcp_port = _this->aamp->mConfig->GetConfigValue(eAAMPConfig_TCPServerSinkPort);
			// TCPServerSinkPort of 0 is treated specially and should not be incremented for audio
			if (eMEDIATYPE_VIDEO == streamId)
			{
				g_object_set (G_OBJECT (sink), "port", tcp_port,"host","127.0.0.1",NULL);
				g_object_set(stream->sinkbin, "video-sink", sink, NULL);
			}
			else if (eMEDIATYPE_AUDIO == streamId)
			{
				g_object_set (G_OBJECT (sink), "port", (tcp_port>0)?tcp_port+1:tcp_port,"host","127.0.0.1",NULL);
				g_object_set(stream->sinkbin, "audio-sink", sink, NULL);
			}
		}
		else if (_this->privateContext->usingRialtoSink && eMEDIATYPE_VIDEO == streamId)
		{
			AAMPLOG_INFO("using rialtomsevideosink");
			GstElement* vidsink = gst_element_factory_make("rialtomsevideosink", NULL);
			if (vidsink)
			{
				/* In the stream->sinkbin, set the video-sink property to vidsink */
				g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);
				MediaFormat mediaFormat = _this->aamp->GetMediaFormatTypeEnum();
				if(eMEDIAFORMAT_HLS == mediaFormat)
				{
					AAMPLOG_INFO("HLS/TS , set the has-drm property to false");
					g_object_set(vidsink, "has-drm", FALSE, NULL);
				}
			}
			else
			{
				AAMPLOG_WARN("Failed to create rialtomsevideosink");
			}
		}
		else if (_this->privateContext->using_westerossink && eMEDIATYPE_VIDEO == streamId)
		{
			AAMPLOG_INFO("using westerossink");
			GstElement* vidsink = gst_element_factory_make("westerossink", NULL);
			if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_BROADCOM)
			{
				g_object_set(vidsink, "secure-video", TRUE, NULL);
			}
			g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);					/* In the stream->sinkbin, set the video-sink property to vidsink */
		}
		else if ((_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_BROADCOM) && !_this->privateContext->using_westerossink && eMEDIATYPE_VIDEO == streamId)
		{
			GstElement* vidsink = gst_element_factory_make("brcmvideosink", NULL);
			g_object_set(vidsink, "secure-video", TRUE, NULL);
			g_object_set(stream->sinkbin, "video-sink", vidsink, NULL);
		}

#if defined(__APPLE__)
		if( _this->cbExportYUVFrame )
		{
			if (eMEDIATYPE_VIDEO == streamId)
			{
				AAMPLOG_MIL("using appsink\n");
				GstElement* appsink = gst_element_factory_make("appsink", NULL);
				assert(appsink);
				GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420", NULL);
				gst_app_sink_set_caps (GST_APP_SINK(appsink), caps);
				g_object_set (G_OBJECT(appsink), "emit-signals", TRUE, "sync", TRUE, NULL);
				_this->SignalConnect(appsink, "new-sample", G_CALLBACK (AAMPGstPlayer::AAMPGstPlayer_OnVideoSample), _this);
				g_object_set(stream->sinkbin, "video-sink", appsink, NULL);
				GstObject **oldobj = (GstObject **)&_this->privateContext->video_sink;
				GstObject *newobj = (GstObject *)appsink;
				gst_object_replace( oldobj, newobj );
			}
		}
#endif

		if (eMEDIATYPE_AUX_AUDIO == streamId)
		{
			// We need to route audio through audsrvsink
			GstElement *audiosink = gst_element_factory_make("audsrvsink", NULL);		/* Creates a new element of "audsrvsink" type and returns a new GstElement */
			g_object_set(audiosink, "session-type", 2, NULL );
			g_object_set(audiosink, "session-name", "btSAP", NULL );
			g_object_set(audiosink, "session-private", TRUE, NULL );

			g_object_set(stream->sinkbin, "audio-sink", audiosink, NULL);				/* In the stream->sinkbin, set the audio-sink property to audiosink */
			if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
			{
				_this->SignalConnect(stream->sinkbin, "element-setup",G_CALLBACK (callback_element_added), _this);
			}

			AAMPLOG_MIL("using audsrvsink");
		}
	}
	gst_bin_add(GST_BIN(_this->privateContext->pipeline), stream->sinkbin);					/* Add the stream sink to the pipeline */
	gint flags;
	g_object_get(stream->sinkbin, "flags", &flags, NULL);									/* Read the state of the current flags */
	AAMPLOG_MIL("playbin flags1: 0x%x", flags); // 0x617 on settop
#if (defined(__APPLE__))
	flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_SOFT_VOLUME;;
#else
	flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_NATIVE_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO;
#endif
	if(_this->aamp->mConfig->IsConfigSet(eAAMPConfig_NoNativeAV))
	{
		flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO | GST_PLAY_FLAG_SOFT_VOLUME;
	}
	else if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{
		flags = GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO |  GST_PLAY_FLAG_NATIVE_AUDIO | GST_PLAY_FLAG_NATIVE_VIDEO | GST_PLAY_FLAG_SOFT_VOLUME;
	}


	if (eMEDIATYPE_SUBTITLE == streamId) flags = GST_PLAY_FLAG_TEXT;
	g_object_set(stream->sinkbin, "flags", flags, NULL); // needed?
	MediaFormat mediaFormat = _this->aamp->GetMediaFormatTypeEnum();				/* Get the Media format type of current media */
	if((mediaFormat != eMEDIAFORMAT_PROGRESSIVE) ||  _this->aamp->mConfig->IsConfigSet(eAAMPConfig_UseAppSrcForProgressivePlayback))
	{
		g_object_set(stream->sinkbin, "uri", "appsrc://", NULL);			/* Assign uri property to appsrc, this will enable data insertion into pipeline */
		_this->SignalConnect(stream->sinkbin, "deep-notify::source", G_CALLBACK(found_source), _this);
	}
	else
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature (gst_registry_get (), "souphttpsrc");		//increasing souphttpsrc priority
		if (pluginFeature == NULL)
		{
			AAMPLOG_ERR("AAMPGstPlayer: souphttpsrc plugin feature not available;");
		}
		else
		{
			AAMPLOG_INFO("AAMPGstPlayer: souphttpsrc plugin priority set to GST_RANK_PRIMARY + 111");
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
			gst_object_unref(pluginFeature);
		}
		g_object_set(stream->sinkbin, "uri", _this->aamp->GetManifestUrl().c_str(), NULL);
		_this->SignalConnect(stream->sinkbin, "source-setup", G_CALLBACK (httpsoup_source_setup), _this);
	}

	if ( ((mediaFormat == eMEDIAFORMAT_DASH || mediaFormat == eMEDIAFORMAT_HLS_MP4) &&
		_this->aamp->mConfig->IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		||
		   (mediaFormat == eMEDIAFORMAT_DASH && eMEDIATYPE_VIDEO == streamId && 
		    _this->aamp->mConfig->IsConfigSet(eAAMPConfig_EnablePTSReStamp)) )
	{
		// Send the media_stream object so that qtdemux can be instantly mapped to media type without caps/parent check
		g_signal_connect(stream->sinkbin, "element_setup", G_CALLBACK(element_setup_cb), _this);
	}

	if(_this->aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{
		if (eMEDIATYPE_VIDEO == streamId && (mediaFormat==eMEDIAFORMAT_DASH || mediaFormat==eMEDIAFORMAT_HLS_MP4) )
		{ // enable multiqueue
			bool isFogEnabled = _this->aamp->mTSBEnabled;
			int MaxGstVideoBufBytes = isFogEnabled ? _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstVideoBufBytesForFogLive) : _this->aamp->mConfig->GetConfigValue(eAAMPConfig_GstVideoBufBytes);
			AAMPLOG_INFO("Setting gst Video buffer size bytes to %d FogLive : %d", MaxGstVideoBufBytes,isFogEnabled);
			g_object_set(stream->sinkbin, "buffer-size", (guint64)MaxGstVideoBufBytes, NULL);
			g_object_set(stream->sinkbin, "buffer-duration", 3000000000, NULL); //3000000000(ns), 3s
		}
	}
#ifdef UBUNTU
	if (eMEDIATYPE_AUDIO == streamId)
	{
		// Deprecate using PulseAudio (if installed) on Ubuntu
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature(gst_registry_get(), "pulsesink");
		if (pluginFeature != NULL)
		{
			AAMPLOG_INFO("AAMPGstPlayer: pulsesink plugin priority set to GST_RANK_SECONDARY");
			gst_plugin_feature_set_rank(pluginFeature, GST_RANK_SECONDARY);
			gst_object_unref(pluginFeature);
		}
	}
#endif
	gst_element_sync_state_with_parent(stream->sinkbin);
	return 0;
}

/**
 * @fn SendGstEvents
 * @param[in] mediaType stream type
 */
void AAMPGstPlayer::SendGstEvents(AampMediaType mediaType, GstClockTime pts)
{
	media_stream* stream = &privateContext->stream[mediaType];
	gboolean enableOverride = FALSE;
	GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");	/* Retrieves the src pad */

	if(stream->pendingSeek)
	{
		if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) != ePLATFORM_AMLOGIC)
		{
			if (privateContext->seekPosition > 0)
			{
				AAMPLOG_MIL("gst_element_seek_simple! mediaType:%d pts:%" GST_TIME_FORMAT " seekPosition:%" GST_TIME_FORMAT,
						mediaType, GST_TIME_ARGS(pts), GST_TIME_ARGS(privateContext->seekPosition * GST_SECOND));
				if (!gst_element_seek_simple(GST_ELEMENT(stream->source), GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, (privateContext->seekPosition * GST_SECOND)))
				{
					AAMPLOG_ERR("Seek failed");
				}
			}
		}
		stream->pendingSeek = false;
	}

	enableOverride = SendQtDemuxOverrideEvent(mediaType, pts);

	if (mediaType == eMEDIATYPE_VIDEO)
	{
		//Westerossink gives position as an absolute value from segment.start. In AAMP's GStreamer pipeline
		// appsrc's base class - basesrc sends an additional segment event since we performed a flushing seek.
		// To figure out the new segment.start, we need to send a segment query which will be replied
		// by basesrc to get the updated segment event values.
		// When override is enabled qtdemux internally restamps and sends segment.start = 0 which is part of
		// AAMP's change in qtdemux so we don't need to query segment.start
		// Enabling position query based progress reporting for non-westerossink configurations.
		// AAMP will send a segment.start query if segmentStart is -1.
		if (ISCONFIGSET(eAAMPConfig_EnableGstPositionQuery) && (enableOverride == FALSE))
		{
			privateContext->segmentStart = -1;
		}
		else
		{
			privateContext->segmentStart = 0;
		}
	}

	if (stream->format == FORMAT_ISO_BMFF)
	{
		// There is a possibility that only single protection event is queued for multiple type
		// since they are encrypted using same id. Hence check if proection event is queued for
		// other types
		GstEvent* event = privateContext->protectionEvent[mediaType];
		if (event == NULL)
		{
			// Check protection event for other types
			for (int i = 0; i < AAMP_TRACK_COUNT; i++)
			{
				if (i != mediaType && privateContext->protectionEvent[i] != NULL)
				{
					event = privateContext->protectionEvent[i];
					break;
				}
			}
		}
		if(event)
		{
			AAMPLOG_MIL("pushing protection event! mediatype: %d", mediaType);
			if (!gst_pad_push_event(sourceEleSrcPad, gst_event_ref(event)))
			{
				AAMPLOG_ERR("push protection event failed!");
			}
		}
	}
	gst_object_unref(sourceEleSrcPad);
	stream->resetPosition = false;
}

/**
 *  @brief Send new segment event to pipeline
 */
void AAMPGstPlayer::SendNewSegmentEvent(AampMediaType mediaType, GstClockTime startPts ,GstClockTime stopPts)
{
	media_stream* stream = &privateContext->stream[mediaType];
	if (stream->format == FORMAT_ISO_BMFF)
	{
		GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
		GstSegment segment;
		gst_segment_init(&segment, GST_FORMAT_TIME);

		segment.start = startPts;
		segment.position = 0;
		segment.rate = AAMP_NORMAL_PLAY_RATE;
		// Set applied rate and notify westerossink to get vmaster mode to work - needed on some devices
		segment.applied_rate = (mediaType == eMEDIATYPE_VIDEO) ? privateContext->rate : AAMP_NORMAL_PLAY_RATE;
		if(stopPts)
		{
			segment.stop = stopPts;
		}

		AAMPLOG_INFO("Sending segment event for mediaType[%d]. start %" G_GUINT64_FORMAT " stop %" G_GUINT64_FORMAT" rate %f applied_rate %f", mediaType, segment.start, segment.stop, segment.rate, segment.applied_rate);
		GstEvent* event = gst_event_new_segment (&segment);
		if (!gst_pad_push_event(sourceEleSrcPad, event))
		{
			AAMPLOG_ERR("gst_pad_push_event segment error");
		}

		gst_object_unref(sourceEleSrcPad);
	}
}

/**
 *  @brief Inject stream buffer to gstreamer pipeline
 */
bool AAMPGstPlayer::SendHelper(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration, bool copy, bool initFragment, bool discontinuity)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		if (eMEDIATYPE_VIDEO == mediaType)
		{
			if( privateContext->numberOfVideoBuffersSent == 0 )
			{ // required in order for subtitle harvesting/processing to work
				aamp->UpdateSubtitleTimestamp();
			  // required in order to fetch more than eAAMPConfig_PrePlayBufferCount video segments see WaitForFreeFragmentAvailable()
				aamp->NotifyFirstFrameReceived(getCCDecoderHandle());
			}
			privateContext->numberOfVideoBuffersSent++;
		}
		return false;
	}
	GstClockTime pts = (GstClockTime)(fpts * GST_SECOND);
	GstClockTime dts = (GstClockTime)(fdts * GST_SECOND);
	GstClockTime duration = (GstClockTime)(fDuration * 1000000000LL);
	media_stream *stream = &privateContext->stream[mediaType];

	if (eMEDIATYPE_SUBTITLE == mediaType && discontinuity)
	{
		AAMPLOG_WARN( "[%d] Discontinuity detected - setting subtitle clock to %" GST_TIME_FORMAT " dAR %d rP %d init %d sC %d",
					 mediaType,
					 GST_TIME_ARGS(pts),
					 aamp->DownloadsAreEnabled(),
					 stream->resetPosition,
					 initFragment,
					 stream->sourceConfigured );
		//gst_element_seek_simple(GST_ELEMENT(stream->source), GST_FORMAT_TIME, GST_SEEK_FLAG_NONE, pts);
	}

	// This block checks if the data contain a valid ID3 header and if it is the case
	// calls the callback function.
	{
		namespace aih = aamp::id3_metadata::helpers;

		if (aih::IsValidMediaType(mediaType) &&
			aih::IsValidHeader(static_cast<const uint8_t*>(ptr), len))
		{
			m_ID3MetadataHandler(mediaType, static_cast<const uint8_t*>(ptr), len,
				{fpts, fdts, fDuration}, nullptr);
		}
	}

	// Ignore eMEDIATYPE_DSM_CC packets
	if(mediaType == eMEDIATYPE_DSM_CC)
	{
		return false;
	}

	bool isFirstBuffer = stream->resetPosition;
	// Make sure source element is present before data is injected
	// If format is FORMAT_INVALID, we don't know what we are doing here
	pthread_mutex_lock(&stream->sourceLock);

	if (!stream->sourceConfigured && stream->format != FORMAT_INVALID)
	{
		bool status = WaitForSourceSetup(mediaType);

		if (!aamp->DownloadsAreEnabled() || !status)
		{
			pthread_mutex_unlock(&stream->sourceLock);
			return false;
		}
	}
	if (isFirstBuffer)
	{
		//Send Gst Event when first buffer received after new tune, seek or period change
		SendGstEvents(mediaType, pts);

		if (mediaType == eMEDIATYPE_AUDIO && ForwardAudioBuffersToAux())
		{
			SendGstEvents(eMEDIATYPE_AUX_AUDIO, pts);
		}

		if (!aamp->mbNewSegmentEvtSent[mediaType] || (mediaType == eMEDIATYPE_VIDEO && aamp->rate != AAMP_NORMAL_PLAY_RATE))
		{
			SendNewSegmentEvent(mediaType, pts, 0);
			aamp->mbNewSegmentEvtSent[mediaType] = true;
		}
		AAMPLOG_DEBUG("mediaType[%d] SendGstEvents - first buffer received !!! initFragment: %d, pts: %" G_GUINT64_FORMAT, mediaType, initFragment, pts);

	}

	bool bPushBuffer = aamp->DownloadsAreEnabled();

	if(bPushBuffer)
	{
		GstBuffer *buffer;

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
				AAMPLOG_DEBUG("Sending segment for mediaType[%d]. pts %" G_GUINT64_FORMAT " dts %" G_GUINT64_FORMAT, mediaType, pts, dts);
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
				AAMPLOG_INFO("Sending segment for mediaType[%d]. pts %" G_GUINT64_FORMAT " dts %" G_GUINT64_FORMAT" len:%zu init:%d discontinuity:%d dur:%" G_GUINT64_FORMAT, 
				mediaType, pts, dts, len, initFragment, discontinuity,duration);
			}
			else
			{
				bPushBuffer = false;
			}
		}

		if (bPushBuffer)
		{
			if (mediaType == eMEDIATYPE_AUDIO && ForwardAudioBuffersToAux())
			{
				ForwardBuffersToAuxPipeline(buffer);
			}

			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(stream->source), buffer);

			if (ret != GST_FLOW_OK)
			{
				AAMPLOG_ERR("gst_app_src_push_buffer error: %d[%s] mediaType %d", ret, gst_flow_get_name (ret), (int)mediaType);
				if (ret != GST_FLOW_EOS && ret !=  GST_FLOW_FLUSHING)
				{ // an unexpected error has occured
					if (mediaType == eMEDIATYPE_SUBTITLE)
					{ // occurs sometimes when injecting subtitle fragments
						if (!stream->source)
						{
							AAMPLOG_ERR("subtitle appsrc is NULL");
						}
						else if (!GST_IS_APP_SRC(stream->source))
						{
							AAMPLOG_ERR("subtitle appsrc is invalid");
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

			// PROFILE_BUCKET_FIRST_BUFFER after successfull push of first gst buffer
			if (isFirstBuffer == true && ret == GST_FLOW_OK)
				this->aamp->profiler.ProfilePerformed(PROFILE_BUCKET_FIRST_BUFFER);

			if (!stream->firstBufferProcessed && !initFragment)
			{
				stream->firstBufferProcessed = true;
			}
		}
	}

	pthread_mutex_unlock(&stream->sourceLock);

	if(bPushBuffer)
	{
		stream->mBufferControl.notifyFragmentInject(this, mediaType, fpts, fdts, fDuration, (isFirstBuffer||discontinuity));
	}

	if (eMEDIATYPE_VIDEO == mediaType)
	{
		// For westerossink, it will send first-video-frame-callback signal after each flush
		// So we can move NotifyFirstBufferProcessed to the more accurate signal callback
		if (isFirstBuffer)
		{
			if (!privateContext->using_westerossink)
			{
				aamp->NotifyFirstBufferProcessed(GetVideoRectangle());
				if((aamp->mTelemetryInterval > 0) && aamp->mDiscontinuityFound)
				{
					aamp->SetDiscontinuityParam();
				}
			}

			// HACK: Have this hack until reakteck Westeros fixes missing first frame call back missing during trick play.
			if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
			{
				aamp->ResetTrickStartUTCTime();

			}
		}

		privateContext->numberOfVideoBuffersSent++;

		StopBuffering(false);
	}

	return bPushBuffer;
}

/**
 *  @brief inject HLS/ts elementary stream buffer to gstreamer pipeline
 */
bool AAMPGstPlayer::SendCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
	return SendHelper( mediaType, ptr, len, fpts, fdts, fDuration, true /*copy*/ );
}

/**
 *  @brief inject mp4 segment to gstreamer pipeline
 */
bool AAMPGstPlayer::SendTransfer(AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double fDuration, bool initFragment, bool discontinuity)
{
	return SendHelper( mediaType, ptr, len, fpts, fdts, fDuration, false /*transfer*/, initFragment, discontinuity );
}

/**
 * @brief To start playback
 */
void AAMPGstPlayer::Stream()
{
}


/**
 * @brief Configure pipeline based on A/V formats
 */
void AAMPGstPlayer::Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation)
{
	AAMPLOG_MIL("videoFormat %d audioFormat %d auxFormat %d subFormat %d",format, audioFormat, auxFormat, subFormat);
	StreamOutputFormat newFormat[AAMP_TRACK_COUNT];
	newFormat[eMEDIATYPE_VIDEO] = format;
	newFormat[eMEDIATYPE_AUDIO] = audioFormat;

	if(aamp->IsGstreamerSubsEnabled())			/* Ignore the sub titles if Subtec is not enabled */
	{
		newFormat[eMEDIATYPE_SUBTITLE] = subFormat;
		AAMPLOG_MIL("Gstreamer subs enabled");
	}
	else
	{
		newFormat[eMEDIATYPE_SUBTITLE]=FORMAT_INVALID;
		AAMPLOG_MIL("Gstreamer subs disabled");
	}

	/* Enable sending of audio data to the auxiliary output */
	if (forwardAudioToAux)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Override auxFormat %d -> %d", auxFormat, audioFormat);
		privateContext->forwardAudioBuffers = true;
		newFormat[eMEDIATYPE_AUX_AUDIO] = audioFormat;
	}
	else
	{
		privateContext->forwardAudioBuffers = false;
		newFormat[eMEDIATYPE_AUX_AUDIO] = auxFormat;
	}

	if (!ISCONFIGSET(eAAMPConfig_UseWesterosSink))
	{
		privateContext->using_westerossink = false;
		if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
		{
			privateContext->firstTuneWithWesterosSinkOff = true;
		}
	}
	else
	{
		privateContext->using_westerossink = true;
	}

	if (!ISCONFIGSET(eAAMPConfig_useRialtoSink))
	{
		privateContext->usingRialtoSink = false;
		AAMPLOG_MIL("Rialto disabled");
	}
	else
	{
		privateContext->usingRialtoSink = true;
		if (privateContext->using_westerossink)
		{
			AAMPLOG_WARN("Rialto and Westeros Sink enabled");
		}
		else
		{
			AAMPLOG_MIL("Rialto enabled");
		}
	}

#ifdef AAMP_STOP_SINK_ON_SEEK
	privateContext->rate = aamp->rate;
#endif

	if (privateContext->pipeline == NULL || privateContext->bus == NULL)
	{
		CreatePipeline();						/* Create a new pipeline if pipeline or the message bus does not exist */
	}

	if (setReadyAfterPipelineCreation)
	{
		if(SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_READY) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_ERR("AAMPGstPlayer_Configure GST_STATE_READY failed on forceful set");
		}
		else
		{
			AAMPLOG_INFO("Forcefully set pipeline to ready state due to track_id change");
			PipelineSetToReady = true;
		}
	}

	bool configureStream[AAMP_TRACK_COUNT];
	bool configurationChanged = false;
	memset(configureStream, 0, sizeof(configureStream));

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		media_stream *stream = &privateContext->stream[i];
		if (stream->format != newFormat[i])
		{
			if (newFormat[i] != FORMAT_INVALID)
			{
				AAMPLOG_MIL("Closing stream %d old format = %d, new format = %d",
								i, stream->format, newFormat[i]);
				configureStream[i] = true;
				privateContext->NumberOfTracks++;
			}

			configurationChanged = true;
		}

#if defined(__APPLE__) || defined(UBUNTU)
        if (this->privateContext->rate > 1 || this->privateContext->rate < 0)
        {
            if (eMEDIATYPE_VIDEO == i)
                configureStream[i] = true;
            else
            {
                TearDownStream((AampMediaType) i);
                configureStream[i] = false;
            }
        }
#endif

		/* Force configure the bin for mid stream audio type change */
		if (!configureStream[i] && bESChangeStatus && (eMEDIATYPE_AUDIO == i))
		{
			AAMPLOG_MIL("AudioType Changed. Force configure pipeline");
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
	if ((privateContext->usingRialtoSink) && (configurationChanged))
	{
		AAMPLOG_INFO("Teardown and rebuild the pipeline for Rialto");
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			media_stream *stream = &privateContext->stream[i];
			if (stream->format != FORMAT_INVALID)
			{
				TearDownStream((AampMediaType) i);
			}

			if (newFormat[i] != FORMAT_INVALID)
			{
				configureStream[i] = true;
			}
		}
	}

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		media_stream *stream = &privateContext->stream[i];

		if ((configureStream[i] && (newFormat[i] != FORMAT_INVALID)) ||
			/* Allow to create audio pipeline along with video pipeline if trickplay initiated before the pipeline going to play/paused state to fix unthrottled trickplay */
			(trickTeardown && (eMEDIATYPE_AUDIO == i)))
		{
			trickTeardown = false;
			TearDownStream((AampMediaType) i);
			stream->format = newFormat[i];
			stream->trackId = aamp->GetCurrentAudioTrackId();
			if (0 != AAMPGstPlayer_SetupStream(this, (AampMediaType)i))			/* Sets up the stream for the given AampMediaType */
			{
				AAMPLOG_ERR("AAMPGstPlayer: track %d failed", i);
				//Don't kill the tune for subtitles
				if (eMEDIATYPE_SUBTITLE != (AampMediaType)i)
				{
					return;
				}
			}
		}
	}

	if ((privateContext->usingRialtoSink) && (aamp->mMediaFormat != eMEDIAFORMAT_PROGRESSIVE))
	{
		/* Reconfigure the Rialto video sink to update the single path stream
		 * property. This enables rialtomsevideosink to call
		 * allSourcesAttached() at the right time to enable streaming on the
		 * server side.
		 * For progressive media, we don't know what tracks are used.
		 */
		GstElement* vidsink = NULL;
		g_object_get(privateContext->stream[eMEDIATYPE_VIDEO].sinkbin, "video-sink", &vidsink, NULL);
		if (vidsink)
		{
			gboolean videoOnly = (audioFormat == FORMAT_INVALID);
			AAMPLOG_INFO("Setting single-path-stream to %d", videoOnly);
			g_object_set(vidsink, "single-path-stream", videoOnly, NULL);
		}
		else
		{
			AAMPLOG_WARN("Couldn't get video-sink");
		}
	}

	if (privateContext->pauseOnStartPlayback && AAMP_NORMAL_PLAY_RATE == privateContext->rate)
	{
		AAMPLOG_INFO("Setting state to GST_STATE_PAUSED - pause on playback enabled");
		privateContext->paused = true;
		privateContext->pendingPlayState = false;
		if (SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_ERR("AAMPGstPlayer: GST_STATE_PAUSED failed");
		}
	}
	/* If buffering is enabled, set the pipeline in Paused state, once sufficient content has been buffered the pipeline will be set to GST_STATE_PLAYING */
	else if (this->privateContext->buffering_enabled && format != FORMAT_INVALID && AAMP_NORMAL_PLAY_RATE == privateContext->rate)
	{
		AAMPLOG_INFO("Setting state to GST_STATE_PAUSED, target state to GST_STATE_PLAYING");
		this->privateContext->buffering_target_state = GST_STATE_PLAYING;
		this->privateContext->buffering_in_progress = true;
		this->privateContext->buffering_timeout_cnt = DEFAULT_BUFFERING_MAX_CNT;
		if (SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_ERR("AAMPGstPlayer_Configure GST_STATE_PAUSED failed");
		}
		privateContext->pendingPlayState = false;
		privateContext->paused = false;
	}
	else
	{
		AAMPLOG_INFO("Setting state to GST_STATE_PLAYING");
		if (SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_ERR("AAMPGstPlayer: GST_STATE_PLAYING failed");
		}
		privateContext->pendingPlayState = false;
		privateContext->paused = false;
	}
	privateContext->eosSignalled = false;
	privateContext->numberOfVideoBuffersSent = 0;
	privateContext->decodeErrorMsgTimeMS = 0;
	privateContext->decodeErrorCBCount = 0;
	if (privateContext->usingRialtoSink)
	{
		AAMPLOG_INFO("RialtoSink subtitle_sink = %p ",privateContext->subtitle_sink);
		GstContext *context = gst_context_new("streams-info", false);
		GstStructure *contextStructure = gst_context_writable_structure(context);
		if( !privateContext->subtitle_sink ) AAMPLOG_WARN( "subtitle_sink==NULL" );
		gst_structure_set(
						  contextStructure,
						  "video-streams", G_TYPE_UINT, 0x1u,
						  "audio-streams", G_TYPE_UINT, 0x1u,
						  "text-streams", G_TYPE_UINT, (privateContext->subtitle_sink)?0x1u:0x0u,
						  nullptr );
		gst_element_set_context(GST_ELEMENT(privateContext->pipeline), context);
		gst_context_unref(context);
	}
#ifdef TRACE
	AAMPLOG_MIL("exiting AAMPGstPlayer");
#endif
}


/**
 * @fn AAMPGstPlayer_SignalEOS
 * @brief Signal EOS to the appsrc associated with the supplied media stream
 * @param[in] media_stream the media stream to inject EOS into
 */
static void AAMPGstPlayer_SignalEOS(media_stream& stream)
{
	if (stream.source)
	{
		auto ret = gst_app_src_end_of_stream(GST_APP_SRC_CAST(stream.source));
		//GST_FLOW_OK is expected in PAUSED or PLAYING states; GST_FLOW_FLUSHING is expected in other states.
		if (ret != GST_FLOW_OK)
		{
			AAMPLOG_WARN("gst_app_src_push_buffer  error: %d", ret);
		}
	}
}

static void AAMPGstPlayer_SignalEOS(media_stream* stream)
{
	if(stream)
	{
		AAMPGstPlayer_SignalEOS(*stream);
	}
}

/**
 *  @brief inject EOS for all media types to ensure the pipeline can be set to NULL quickly*/
static void AAMPGstPlayer_SignalEOS(AAMPGstPlayerPriv* privateContext)
{
	AAMPLOG_MIL("AAMPGstPlayer: Inject EOS into all streams.");

	if(privateContext && privateContext->pipeline)
	{
		for(int mediaType=eMEDIATYPE_VIDEO; mediaType<=eMEDIATYPE_AUX_AUDIO; mediaType++)
		{
			AAMPGstPlayer_SignalEOS(privateContext->stream[mediaType]);
		}
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer: null pointer check failed");
	}
}

/**
 *  @brief Checks to see if the pipeline is configured for specified media type
 */
bool AAMPGstPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	bool pipelineConfigured = true;

	if( type != eMEDIATYPE_SUBTITLE || aamp->IsGstreamerSubsEnabled() )
	{
		media_stream *stream = &privateContext->stream[type];
		if (stream)
		{
			pipelineConfigured = stream->sourceConfigured;
		}
	}
	return pipelineConfigured;
}

/**
 *  @brief Starts processing EOS for a particular stream type
 */
void AAMPGstPlayer::EndOfStreamReached(AampMediaType type)
{
	AAMPLOG_MIL("entering AAMPGstPlayer_EndOfStreamReached type %d", (int)type);

	media_stream *stream = &privateContext->stream[type];
	stream->eosReached = true;
	if ((stream->format != FORMAT_INVALID) && stream->firstBufferProcessed == false)
	{
		AAMPLOG_MIL("EOS received as first buffer ");
		NotifyEOS();
	}
	else
	{
		NotifyFragmentCachingComplete();		/*Set pipeline to PLAYING state once fragment caching is complete*/
		AAMPGstPlayer_SignalEOS(stream);

		/*For trickmodes, give EOS to audio source*/
		if (AAMP_NORMAL_PLAY_RATE != privateContext->rate)
		{
			AAMPGstPlayer_SignalEOS(privateContext->stream[eMEDIATYPE_AUDIO]);
			if (privateContext->stream[eMEDIATYPE_SUBTITLE].source)
			{
				AAMPGstPlayer_SignalEOS(privateContext->stream[eMEDIATYPE_SUBTITLE]);
			}
		}
		else
		{
			if ((privateContext->stream[eMEDIATYPE_AUDIO].eosReached) &&
				(!privateContext->stream[eMEDIATYPE_SUBTITLE].eosReached) &&
				(privateContext->stream[eMEDIATYPE_SUBTITLE].source))
			{
				privateContext->stream[eMEDIATYPE_SUBTITLE].eosReached = true;
				AAMPGstPlayer_SignalEOS(privateContext->stream[eMEDIATYPE_SUBTITLE]);
			}
		}

		// We are in buffering, but we received end of stream, un-pause pipeline
		StopBuffering(true);
	}
}

/**
 *  @brief Stop playback and any idle handlers active at the time
 */
void AAMPGstPlayer::Stop(bool keepLastFrame)
{
	AAMPLOG_MIL("entering AAMPGstPlayer_Stop keepLastFrame %d", keepLastFrame);

	/*  make the execution of this function more deterministic and
	 *  reduce scope for potential pipeline lockups*/
	privateContext->syncControl.disable();
	privateContext->aSyncControl.disable();
	if(privateContext->bus)
	{
		gst_bus_remove_watch(privateContext->bus);		/* Remove the watch from bus so that gstreamer no longer sends messages to it */
	}

	if(!keepLastFrame)
	{
		privateContext->firstFrameReceived = false;
		privateContext->firstVideoFrameReceived = false;
		privateContext->firstAudioFrameReceived = false ;
	}

	this->IdleTaskRemove(privateContext->firstProgressCallbackIdleTask);			/* removes firstProgressCallbackIdleTask in a thread safe manner */

	this->TimerRemove(this->privateContext->periodicProgressCallbackIdleTaskId, "periodicProgressCallbackIdleTaskId");		/* Removes the timer with the id of periodicProgressCallbackIdleTaskId */

	if (this->privateContext->bufferingTimeoutTimerId)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove bufferingTimeoutTimerId %d", privateContext->bufferingTimeoutTimerId);
		g_source_remove(privateContext->bufferingTimeoutTimerId);
		privateContext->bufferingTimeoutTimerId = AAMP_TASK_ID_INVALID;
	}
	if (privateContext->ptsCheckForEosOnUnderflowIdleTaskId)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove ptsCheckForEosCallbackIdleTaskId %d", privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		g_source_remove(privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		privateContext->ptsCheckForEosOnUnderflowIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	if (this->privateContext->eosCallbackIdleTaskPending)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove eosCallbackIdleTaskId %d", privateContext->eosCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->eosCallbackIdleTaskId);
		privateContext->eosCallbackIdleTaskPending = false;
		privateContext->eosCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	if (this->privateContext->firstFrameCallbackIdleTaskPending)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove firstFrameCallbackIdleTaskId %d", privateContext->firstFrameCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->firstFrameCallbackIdleTaskId);
		privateContext->firstFrameCallbackIdleTaskPending = false;
		privateContext->firstFrameCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
	}
	this->IdleTaskRemove(privateContext->firstVideoFrameDisplayedCallbackTask);

	/* Prevent potential side effects of injecting EOS and
	 * make the stop process more deterministic by:
			1) Confirming that bus handlers (disabled above) have completed
			2) disabling and disconnecting signals
			3) confirming that all signal handlers have completed.
	 * This should complete very quickly and
	 * should not have a significant performance impact.*/
	privateContext->syncControl.waitForDone(50, "bus_sync_handler");
	privateContext->aSyncControl.waitForDone(50, "bus_message");
	privateContext->callbackControl.disable();
	DisconnectSignals();
	privateContext->aSyncControl.waitForDone(100, "callback handler");

	// Remove probes before setting the pipeline to NULL
	RemoveProbes();

	if (this->privateContext->pipeline)
	{
		const auto EOSMode = GETCONFIGVALUE(eAAMPConfig_EOSInjectionMode);
		if(EOS_INJECTION_MODE_STOP_ONLY == EOSMode)
		{
			//Ensure prompt transition to GST_STATE_NULL
			AAMPGstPlayer_SignalEOS(this->privateContext);
		}

		privateContext->buffering_in_progress = false;   /* stopping pipeline, don't want to change state if GST_MESSAGE_ASYNC_DONE message comes in */
		SetStateWithWarnings(this->privateContext->pipeline, GST_STATE_NULL);
		AAMPLOG_MIL("AAMPGstPlayer: Pipeline state set to null");
	}
#ifdef AAMP_MPD_DRM
	if(AampOutputProtection::IsAampOutputProcectionInstanceActive())
	{
		AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();
		pInstance->setGstElement((GstElement *)(NULL));
		pInstance->Release();
	}
#endif
	aamp->seiTimecode.assign("");
	TearDownStream(eMEDIATYPE_VIDEO);
	TearDownStream(eMEDIATYPE_AUDIO);
	TearDownStream(eMEDIATYPE_SUBTITLE);
	TearDownStream(eMEDIATYPE_AUX_AUDIO);
	DestroyPipeline();
	privateContext->rate = AAMP_NORMAL_PLAY_RATE;
	privateContext->lastKnownPTS = 0;
	privateContext->segmentStart = 0;
	privateContext->paused = false;
	privateContext->pipelineState = GST_STATE_NULL;
	AAMPLOG_MIL("exiting AAMPGstPlayer_Stop");
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
					//no annnotation
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
	AAMPLOG_MIL("AAMPGstPlayer: %s Status: %s",SafeName(pElementOrBin).c_str(), GetStatus(pElementOrBin, recursionCount).c_str());
}

/**
 * @brief Validate pipeline state transition within a max timeout
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @param[in] stateToValidate state to be validated
 * @param[in] msTimeOut max timeout in MS
 * @retval Current pipeline state
 */
static GstState validateStateWithMsTimeout( AAMPGstPlayer *_this, GstState stateToValidate, guint msTimeOut)
{
	GstState gst_current;
	GstState gst_pending;
	float timeout = 100.0;
	gint gstGetStateCnt = GST_ELEMENT_GET_STATE_RETRY_CNT_MAX;

	do
	{
		if ((GST_STATE_CHANGE_SUCCESS
				== gst_element_get_state(_this->privateContext->pipeline, &gst_current, &gst_pending, timeout * GST_MSECOND))
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

	AAMPLOG_ERR("validateStateWithMsTimeout - PIPELINE gst_element_get_state - FAILURE : State = %d, Pending = %d",
			gst_current, gst_pending);
	return gst_current;
}

/**
 * @brief wraps gst_element_set_state and adds log messages where applicable
 * @param[in] element the GstElement whose state is to be changed
 * @param[in] targetState the GstState to apply to element
 * @param[in] _this pointer to AAMPGstPlayer instance
 * @retval Result of the state change (from inner gst_element_set_state())
 */
static GstStateChangeReturn SetStateWithWarnings(GstElement *element, GstState targetState)
{
    GstStateChangeReturn rc = GST_STATE_CHANGE_FAILURE;
	if(element)
	{
		//In a synchronous only transition gst_element_set_state can lockup if there are pipeline errors
		bool syncOnlyTransition = (targetState==GST_STATE_NULL)||(targetState==GST_STATE_READY);

		GstState current;																	/* To hold the current state of the element */
		GstState pending;																	/* Pending state, used in printing the pending state of the element */

		auto stateChangeReturn = gst_element_get_state(element, &current, &pending, 0);		/* Get the current playing state of the element with no blocking timeout,  this function is MT-safe */
		switch(stateChangeReturn)
		{
			case GST_STATE_CHANGE_FAILURE:
				AAMPLOG_ERR("AAMPGstPlayer: %s is in FAILURE state : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
				LogStatus(element);
				break;
			case GST_STATE_CHANGE_SUCCESS:
				AAMPLOG_DEBUG("AAMPGstPlayer: %s is in success state : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
				break;
			case GST_STATE_CHANGE_ASYNC:
				if(syncOnlyTransition)
				{
					AAMPLOG_MIL("AAMPGstPlayer: %s state is changing asynchronously : current %s  pending %s", SafeName(element).c_str(),gst_element_state_get_name(current), gst_element_state_get_name(pending));
					LogStatus(element);
				}
				break;
			default:
				AAMPLOG_ERR("AAMPGstPlayer: %s is in an unknown state", SafeName(element).c_str());
				break;
		}

		if(syncOnlyTransition)
		{
			AAMPLOG_MIL("AAMPGstPlayer: Attempting to set %s state to %s", SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		else
		{
			AAMPLOG_DEBUG("AAMPGstPlayer: Attempting to set %s state to %s", SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		rc = gst_element_set_state(element, targetState);					/* Set the state of the element to the targetState, this function is MT-safe*/
		if(syncOnlyTransition)
		{
			AAMPLOG_MIL("AAMPGstPlayer: %s state set to %s",  SafeName(element).c_str(), gst_element_state_get_name(targetState));
		}
		else
		{
			AAMPLOG_DEBUG("AAMPGstPlayer: %s state set to %s, rc:%d",  SafeName(element).c_str(), gst_element_state_get_name(targetState), rc);
		}
	}
	else
	{
		AAMPLOG_ERR("AAMPGstPlayer: Attempted to set the state of a null pointer");
	}
    return rc;
}

/**
  * @brief Set the instance of PrivateInstanceAAMP that has encrypted content, used in the context of
  * single pipeline.
  * @param[in] aamp - Pointer to the instance of PrivateInstanceAAMP that has the encrypted content
  */
void AAMPGstPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	mEncryptedAamp = aamp;
}

bool AAMPGstPlayer::IsAssociatedAamp(PrivateInstanceAAMP *aampInstance)
{
	return aamp == aampInstance;
}

/**
  * @brief Change the instance of PrivateInstanceAAMP that is using the gstreamer
  * pipeline, when it is being used as a single pipeline shared among multiple
  * instances of PrivateInstanceAAMP
  * @param[in] newAamp - pointer to new instance of PrivateInstanceAAMP
  * @param[in] id3HandlerCallback - the id3 callback handle associated with this instance of PrivateInstanceAAMP
  */
void AAMPGstPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	aamp = newAamp;
	privateContext->decoderHandleNotified = false;
	m_ID3MetadataHandler = id3HandlerCallback;
}
/**
 * @brief Flush the track playbin
 * @param[in] pos - position to seek to after flush
 */
void AAMPGstPlayer::FlushTrack(AampMediaType type,double pos)
{
	double startPosition = 0;
	AAMPLOG_MIL("Entering AAMPGstPlayer::FlushTrack() type[%d] pipeline state %s pos %lf",(int)type,
			gst_element_state_get_name(GST_STATE(privateContext->pipeline)), pos);

	media_stream *stream = &this->privateContext->stream[type];
	double rate = (double)AAMP_NORMAL_PLAY_RATE;
	if(eMEDIATYPE_AUDIO == type)
	{
		if (aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_AMLOGIC)
		{
			g_object_set(G_OBJECT(this->privateContext->audio_sink), "seamless-switch", TRUE, NULL);
		}

		privateContext->filterAudioDemuxBuffers = true;
		pos = pos + aamp->mAudioDelta;
	}
	else
	{
		pos = pos + aamp->mSubtitleDelta;
	}
	gst_element_seek_simple (GST_ELEMENT(stream->source),
							GST_FORMAT_TIME,
							GST_SEEK_FLAG_FLUSH,
							pos * GST_SECOND);
	if(aamp->mCorrectionRate != rate)
	{
		AAMPLOG_MIL("Reset Rate Correction to 1");
		aamp->mCorrectionRate = rate;
	}

	startPosition = pos;
	AAMPLOG_MIL("Exiting AAMPGstPlayer::FlushTrack() type[%d] pipeline state: %s startPosition: %lf Delta %lf",(int)type, gst_element_state_get_name(GST_STATE(privateContext->pipeline)), startPosition, (int)type==eMEDIATYPE_AUDIO?aamp->mAudioDelta:aamp->mSubtitleDelta);
}

/**
 *  @brief Get playback duration in MS
 */
long AAMPGstPlayer::GetDurationMilliseconds(void)
{
	long rc = 0;
	if( privateContext->pipeline )
	{
		if( privateContext->pipelineState == GST_STATE_PLAYING || // playing
		    (privateContext->pipelineState == GST_STATE_PAUSED && privateContext->paused) ) // paused by user
		{
			privateContext->durationQuery = gst_query_new_duration(GST_FORMAT_TIME);	/*Constructs a new stream duration query object to query in the given format */
			if( privateContext->durationQuery )
			{
				gboolean res = gst_element_query(privateContext->pipeline,privateContext->durationQuery);
				if( res )
				{
					gint64 duration;
					gst_query_parse_duration(privateContext->durationQuery, NULL, &duration); /* parses the value into duration */
					rc = GST_TIME_AS_MSECONDS(duration);
				}
				else
				{
					AAMPLOG_ERR("Duration query failed");
				}
				gst_query_unref(privateContext->durationQuery);		/* Decreases the refcount of the durationQuery. In this case the count will be zero, so it will be freed*/
				privateContext->durationQuery = NULL;
			}
			else
			{
				AAMPLOG_WARN("Duration query is NULL");
			}
		}
		else
		{
			AAMPLOG_WARN("Pipeline is in %s state", gst_element_state_get_name(privateContext->pipelineState) );
		}
	}
	else
	{
		AAMPLOG_WARN("Pipeline is null");
	}
	return rc;
}

/**
 *  @brief Get playback position in MS
 */
long long AAMPGstPlayer::GetPositionMilliseconds(void)
{
	long long rc = 0;
	if (privateContext->pipeline == NULL)
	{
		AAMPLOG_ERR("Pipeline is NULL");
		return rc;
	}

	if (privateContext->positionQuery == NULL)
	{
		AAMPLOG_ERR("Position query is NULL");
		return rc;
	}

	// Perform gstreamer query and related operation only when pipeline is playing or if deliberately put in paused
	if (privateContext->pipelineState != GST_STATE_PLAYING &&
		!(privateContext->pipelineState == GST_STATE_PAUSED && privateContext->paused) &&
		// The player should be (and probably soon will be) in the playing state so don't exit early.
		GST_STATE_TARGET(privateContext->pipeline) != GST_STATE_PLAYING)
	{
		AAMPLOG_INFO("Pipeline is in %s state %s target state, paused=%d returning position as %lld", gst_element_state_get_name(privateContext->pipelineState), gst_element_state_get_name(GST_STATE_TARGET(privateContext->pipeline)), privateContext->paused, rc);
		return rc;
	}

	media_stream* video = &privateContext->stream[eMEDIATYPE_VIDEO];

	// segment.start needs to be queried
	if (privateContext->segmentStart == -1)
	{
		GstQuery *segmentQuery = gst_query_new_segment(GST_FORMAT_TIME);
		// Send query to video playbin in pipeline.
		// Special case include trickplay, where only video playbin is active
		// This is to get the actual start position from video decoder/sink. If these element doesn't support the query appsrc should respond
		if (gst_element_query(video->source, segmentQuery) == TRUE)
		{
			gint64 start;
			gst_query_parse_segment(segmentQuery, NULL, NULL, &start, NULL);
			privateContext->segmentStart = GST_TIME_AS_MSECONDS(start);
			AAMPLOG_MIL("AAMPGstPlayer: Segment start: %" G_GINT64_FORMAT, privateContext->segmentStart);
		}
		else
		{
			AAMPLOG_ERR("AAMPGstPlayer: segment query failed");
		}
		gst_query_unref(segmentQuery);
	}

	if (gst_element_query(video->sinkbin, privateContext->positionQuery) == TRUE)
	{
		gint64 pos = 0;
		int rate = privateContext->rate;
		gst_query_parse_position(privateContext->positionQuery, NULL, &pos);
		if (aamp->mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
		{
			rate = 1; // MP4 position query always return absolute value
		}

		if (privateContext->segmentStart > 0)
		{
			// Deduct segment.start to find the actual time of media that's played.
			rc = (GST_TIME_AS_MSECONDS(pos) - privateContext->segmentStart) * rate;
			AAMPLOG_DEBUG("positionQuery pos - %" G_GINT64_FORMAT " rc - %lld SegStart -%" G_GINT64_FORMAT, GST_TIME_AS_MSECONDS(pos), rc,privateContext->segmentStart);
		}
		else
		{
			rc = GST_TIME_AS_MSECONDS(pos) * rate;
			AAMPLOG_DEBUG("positionQuery pos - %" G_GINT64_FORMAT " rc - %lld" , GST_TIME_AS_MSECONDS(pos), rc);
		}
		//AAMPLOG_MIL("AAMPGstPlayer: with positionQuery pos - %" G_GINT64_FORMAT " rc - %lld", GST_TIME_AS_MSECONDS(pos), rc);

		//positionQuery is not unref-ed here, because it could be reused for future position queries
	}

	return rc;
}

/**
 *  @brief To pause/play pipeline
 */
bool AAMPGstPlayer::Pause( bool pause, bool forceStopGstreamerPreBuffering )
{
	bool retValue = true;

	aamp->SyncBegin();					/* Obtains a mutex lock */

	AAMPLOG_MIL("entering AAMPGstPlayer_Pause - pause(%d) stop-pre-buffering(%d)", pause, forceStopGstreamerPreBuffering);

	if (privateContext->pipeline != NULL)
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
			privateContext->buffering_in_progress = false;
		}

		GstStateChangeReturn rc = SetStateWithWarnings(this->privateContext->pipeline, nextState);
		if (GST_STATE_CHANGE_ASYNC == rc)
		{
			/* CID:330433 Waiting while holding lock. Sleep introduced in validateStateWithMsTimeout to prevent continuous polling when synchronising pipeline state.
			 * Too risky to remove mutex lock. It may be replaced if approach is redesigned in future */
			/* wait a bit longer for the state change to conclude */
			if (nextState != validateStateWithMsTimeout(this,nextState, 100))
			{
				AAMPLOG_ERR("AAMPGstPlayer_Pause - validateStateWithMsTimeout - FAILED GstState %d", nextState);
			}
		}
		else if (GST_STATE_CHANGE_SUCCESS != rc)
		{
			AAMPLOG_ERR("AAMPGstPlayer_Pause - gst_element_set_state - FAILED rc %d", rc);
		}
		privateContext->buffering_target_state = nextState;
		privateContext->paused = pause;
		privateContext->pendingPlayState = false;
		if(!aamp->IsGstreamerSubsEnabled())
			aamp->PauseSubtitleParser(pause);
	}
	else
	{
		AAMPLOG_WARN("Pipeline is NULL");
		retValue = false;
	}

#if 0
	GstStateChangeReturn rc;
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		media_stream *stream = &privateContext->stream[iTrack];
		if (stream->source)
		{
			rc = SetStateWithWarnings(privateContext->stream->sinkbin, GST_STATE_PAUSED);
		}
	}
#endif

	aamp->SyncEnd();					/* Releases the mutex */

	return retValue;
}

/**
 *  @brief Set video display rectangle co-ordinates
 */
void AAMPGstPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
	int currentX = 0, currentY = 0, currentW = 0, currentH = 0;

	if (strcmp(privateContext->videoRectangle, "") != 0)
	{
		sscanf(privateContext->videoRectangle,"%d,%d,%d,%d",&currentX,&currentY,&currentW,&currentH);
	}
	//check the existing VideoRectangle co-ordinates
	if ((currentX == x) && (currentY == y) && (currentW == w) && (currentH == h))
	{
		AAMPLOG_TRACE("Ignoring new co-ordinates, same as current Rect (x:%d, y:%d, w:%d, h:%d)", currentX, currentY, currentW, currentH);
		//ignore setting same rectangle co-ordinates and return
		return;
	}

	snprintf(privateContext->videoRectangle, sizeof(privateContext->videoRectangle), "%d,%d,%d,%d", x,y,w,h);
	AAMPLOG_MIL("Rect %s, video_sink =%p",
			privateContext->videoRectangle, privateContext->video_sink);
	if (ISCONFIGSET(eAAMPConfig_EnableRectPropertyCfg))
	{
		if (privateContext->video_sink)
		{
			g_object_set(privateContext->video_sink, "rectangle", privateContext->videoRectangle, NULL);
		}
		else
		{
			AAMPLOG_WARN("Scaling not possible at this time");
		}
	}
	else
	{
		AAMPLOG_WARN("New co-ordinates ignored since westerossink is used");
	}
}

/**
 *  @brief Set video zoom
 */
void AAMPGstPlayer::SetVideoZoom(VideoZoomMode zoom)
{
	AAMPLOG_MIL("SetVideoZoom :: ZoomMode %d, video_sink =%p",
			zoom, privateContext->video_sink);

	privateContext->zoom = zoom;
	if ((privateContext->video_sink) && (!privateContext->usingRialtoSink))
	{
		g_object_set(privateContext->video_sink, "zoom-mode", zoom, NULL);
	}
	else
	{
		AAMPLOG_WARN("AAMPGstPlayer not setting video zoom");
	}
}

void AAMPGstPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	if (privateContext->usingRialtoSink)
	{
		if(privateContext->stream[eMEDIATYPE_SUBTITLE].source)
		{
			AAMPLOG_INFO("usingRialtoSink pts_offset gst_seek_simple %" PRIu64 ", seek_pos_seconds %2f", pts_offset, aamp->seek_pos_seconds);
			GstClockTime pts = ((double)pts_offset) * GST_SECOND;
			GstStructure *structure{gst_structure_new("set-pts-offset", "pts-offset", G_TYPE_UINT64, pts, nullptr)};
			if (!gst_element_send_event(privateContext->stream[eMEDIATYPE_SUBTITLE].source, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure)))
			{
				AAMPLOG_WARN("usingRialtoSink Failed to seek text-sink element");
			}
		}
	}
	else if (privateContext->subtitle_sink)
	{
		AAMPLOG_INFO("pts_offset %" PRIu64 ", seek_pos_seconds %2f, subtitle_sink =%p", pts_offset, aamp->seek_pos_seconds, privateContext->subtitle_sink);
		//We use seek_pos_seconds as an offset during seek, so we subtract that here to get an offset from zero position
		g_object_set(privateContext->subtitle_sink, "pts-offset", static_cast<std::uint64_t>(pts_offset*1000), NULL);
	}
	else
		AAMPLOG_INFO("subtitle_sink is NULL");
}

void AAMPGstPlayer::SetSubtitleMute(bool muted)
{
	privateContext->subtitleMuted = muted;

	if (privateContext->subtitle_sink)
	{
		AAMPLOG_INFO("muted %d, subtitle_sink =%p", muted, privateContext->subtitle_sink);

		g_object_set(privateContext->subtitle_sink, "mute", privateContext->subtitleMuted ? TRUE : FALSE, NULL);		/* Update the 'mute' property of the sink */
	}
	else
		AAMPLOG_INFO("subtitle_sink is NULL");
}

/**
 * @brief Reset first frame
 */
void AAMPGstPlayer::ResetFirstFrame(void)
{
	AAMPLOG_WARN("Reset first frame");
	privateContext->firstFrameReceived = false;
}

/**
 * @brief Set video mute
 */
void AAMPGstPlayer::SetVideoMute(bool muted)
{
	AAMPLOG_INFO("muted=%d video_sink =%p", muted, privateContext->video_sink);

	privateContext->videoMuted = muted;
	if (privateContext->video_sink)
	{
		g_object_set(privateContext->video_sink, "show-video-window", !privateContext->videoMuted, NULL);	/* videoMuted to true implies setting the 'show-video-window' to false */
	}
	else
	{
		AAMPLOG_INFO("AAMPGstPlayer not setting video mute");
	}
}

/**
 *  @brief Set audio volume
 */
void AAMPGstPlayer::SetAudioVolume(int volume)
{
	privateContext->audioVolume = volume / 100.0;
	setVolumeOrMuteUnMute();
}

/**
 *  @brief Set audio volume or mute
 */
void AAMPGstPlayer::setVolumeOrMuteUnMute(void)
{
	const std::lock_guard<std::mutex> lock(privateContext->volumeMuteMutex);
	GstElement *gSource = nullptr;
	const char *mutePropertyName = nullptr;
	const char *volumePropertyName = nullptr;

	if (privateContext->usingRialtoSink)
	{
		gSource = privateContext->audio_sink;
		mutePropertyName = "mute";
		volumePropertyName = "volume";
	}

	else
	{
#if defined(__APPLE__)
		// Why do these platforms set volume/mute on the sinkbin rather than the audio_sink?
		// Or why do the other platforms not also do this?
		gSource = privateContext->stream[eMEDIATYPE_AUDIO].sinkbin;
#endif
		if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
		{
			gSource = privateContext->stream[eMEDIATYPE_AUDIO].sinkbin;
		}
		if (nullptr == gSource)
		{
			gSource = privateContext->audio_sink;
		}
		if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_AMLOGIC)
		{
			/* Avoid mute property setting for AMLOGIC as use of "mute" property on pipeline is impacting all other players */
			/* Using "stream-volume" property of audio-sink for setting volume and mute for AMLOGIC platform */
			volumePropertyName = "stream-volume";
		}
		else
		{
			mutePropertyName = "mute";
			volumePropertyName = "volume";
		}
	}
	AAMPLOG_MIL("volume == %lf muted == %s", privateContext->audioVolume, privateContext->audioMuted ? "true" : "false");
	if (nullptr != gSource)
	{
		if (nullptr != mutePropertyName)
		{
			/* Muting the audio decoder in general to avoid audio passthrough in expert mode for locked channel */
			if (0 == privateContext->audioVolume)
			{
				AAMPLOG_MIL("Audio Muted");
				g_object_set(gSource, mutePropertyName, true, NULL);
				privateContext->audioMuted = true;
			}
			else if (privateContext->audioMuted)
			{
				AAMPLOG_MIL("Audio Unmuted after a Mute");
				g_object_set(gSource, mutePropertyName, false, NULL);
				privateContext->audioMuted = false;
			}
			else
			{
				// Deliberately left empty
			}
		}
		if ((nullptr != volumePropertyName) && (false == privateContext->audioMuted))
		{
			AAMPLOG_MIL("Setting Volume %f using \"%s\" property", privateContext->audioVolume, volumePropertyName);
			g_object_set(gSource, volumePropertyName, privateContext->audioVolume, NULL);
		}
	}
	else
	{
		AAMPLOG_WARN("No element to set volume/mute");
	}
}

/**
 *  @brief Flush cached GstBuffers and set seek position & rate
 */
void AAMPGstPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		return;
	}
	media_stream *stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	privateContext->rate = rate;
	//TODO: Need to decide if required for AUX_AUDIO
	privateContext->stream[eMEDIATYPE_VIDEO].bufferUnderrun = false;
	privateContext->stream[eMEDIATYPE_AUDIO].bufferUnderrun = false;

	if (privateContext->eosCallbackIdleTaskPending)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove eosCallbackIdleTaskId %d", privateContext->eosCallbackIdleTaskId);
		aamp->RemoveAsyncTask(privateContext->eosCallbackIdleTaskId);
		privateContext->eosCallbackIdleTaskId = AAMP_TASK_ID_INVALID;
		privateContext->eosCallbackIdleTaskPending = false;
	}

	if (privateContext->ptsCheckForEosOnUnderflowIdleTaskId)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove ptsCheckForEosCallbackIdleTaskId %d", privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		g_source_remove(privateContext->ptsCheckForEosOnUnderflowIdleTaskId);
		privateContext->ptsCheckForEosOnUnderflowIdleTaskId = AAMP_TASK_ID_INVALID;
	}

	if (privateContext->bufferingTimeoutTimerId)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Remove bufferingTimeoutTimerId %d", privateContext->bufferingTimeoutTimerId);
		g_source_remove(privateContext->bufferingTimeoutTimerId);
		privateContext->bufferingTimeoutTimerId = AAMP_TASK_ID_INVALID;
	}

	// If the pipeline is not setup, we will cache the value for later
	SetSeekPosition(position);

	if (privateContext->pipeline == NULL)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Pipeline is NULL");
		return;
	}

	bool bAsyncModify = FALSE;
	if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{
		if (privateContext->audio_sink)
		{
			PrivAAMPState state = eSTATE_IDLE;
			aamp->GetState(state);
			if (privateContext->audio_sink)
			{
				if (privateContext->rate > 1 || privateContext->rate < 0 || state == eSTATE_SEEKING)
				{
					//aamp won't feed audio bitstreame to gstreamer at trickplay.
					//It needs to disable async of audio base sink to prevent audio sink never sends ASYNC_DONE to pipeline.
					if(aamp_StartsWith(GST_OBJECT_NAME(privateContext->audio_sink), "rialtomseaudiosink") == false)
					{
						AAMPLOG_MIL("Disable async for audio stream at trickplay");
						if(gst_base_sink_is_async_enabled(GST_BASE_SINK(privateContext->audio_sink)) == TRUE)
						{
							gst_base_sink_set_async_enabled(GST_BASE_SINK(privateContext->audio_sink), FALSE);
							bAsyncModify = TRUE;
						}
					}
				}
			}
		}
	}
	//Check if pipeline is in playing/paused state. If not flush doesn't work
	GstState current, pending;
	GstStateChangeReturn ret;
	ret = gst_element_get_state(privateContext->pipeline, &current, &pending, 100 * GST_MSECOND);
	if ((current != GST_STATE_PLAYING && current != GST_STATE_PAUSED) || ret == GST_STATE_CHANGE_FAILURE)
	{
		AAMPLOG_WARN("AAMPGstPlayer: Pipeline state %s, ret %u", gst_element_state_get_name(current), ret);
		if (shouldTearDown)
		{
			AAMPLOG_WARN("AAMPGstPlayer: Pipeline is not in playing/paused state, hence resetting it");
			if(rate > AAMP_NORMAL_PLAY_RATE)
			{
				trickTeardown = true;
			}
			Stop(true);
			// Set the rate back to the original value if it was an recovery Stop() call
			privateContext->rate = rate;
		}
		return;
	}
	else
	{
		/* pipeline may enter paused state even when audio decoder is not ready, check again */
		if (privateContext->audio_dec)
		{
			GstState aud_current, aud_pending;
			ret = gst_element_get_state(privateContext->audio_dec, &aud_current, &aud_pending, 0);
			if ((aud_current != GST_STATE_PLAYING && aud_current != GST_STATE_PAUSED) || ret == GST_STATE_CHANGE_FAILURE)
			{
				if (shouldTearDown)
				{
					AAMPLOG_WARN("AAMPGstPlayer: Pipeline is in playing/paused state, but audio_dec is in %s state, resetting it ret %u",
								 gst_element_state_get_name(aud_current), ret);
					Stop(true);
					// Set the rate back to the original value if it was an recovery Stop() call
					privateContext->rate = rate;
					return;
				}
			}
		}
		AAMPLOG_MIL("AAMPGstPlayer: Pipeline is in %s state position %f ret %d", gst_element_state_get_name(current), position, ret);
	}
	/* Disabling the flush flag */
	/* flush call again (which may cause freeze sometimes)      */
	/* from SendGstEvents() API.              */
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		privateContext->stream[i].resetPosition = true;
		// Pipeline is already flushed, no need to send seek event again
		privateContext->stream[i].pendingSeek = false;
		privateContext->stream[i].eosReached = false;
		privateContext->stream[i].firstBufferProcessed = false;
		//reset buffer control states prior to gstreamer flush so that the first needs_data event is caught
		privateContext->stream[i].mBufferControl.flush();
	}

	AAMPLOG_INFO("AAMPGstPlayer: Pipeline flush seek - start = %f rate = %d", position, rate);
	double playRate = 1.0;
	if (eMEDIAFORMAT_PROGRESSIVE == aamp->mMediaFormat)
	{
		playRate = rate;
	}



	if ((stream->format == FORMAT_ISO_BMFF) && (eMEDIAFORMAT_PROGRESSIVE != aamp->mMediaFormat))
	{
#if !defined(UBUNTU)
		if (privateContext->usingRialtoSink)
#endif
		{
			gboolean enableOverride = (rate != AAMP_NORMAL_PLAY_RATE);
			/* If PTS restamping is enabled, set the seek position to zero. */
			if (enableOverride)
			{
				AAMPLOG_INFO("Resetting seek position to zero");
				position = 0;
			}
		}
	}
	if (!gst_element_seek(privateContext->pipeline, playRate, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET,
						  position * GST_SECOND, GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
	{
		AAMPLOG_ERR("Seek failed");
		// In Ubuntu, when we handle EOS based codec switching (ie, when audio playbin is added again to pipeline),
		// we have seen that above Flush() fails and audio playbin clock is not adjusted properly causing AV sync issues.
		// We can't identify here if its a codec switch or normal flush, so we are setting pendingSeek to true to all tracks
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			privateContext->stream[i].pendingSeek = true;
		}
	}
	if((aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK) && bAsyncModify == TRUE)
	{
		gst_base_sink_set_async_enabled(GST_BASE_SINK(privateContext->audio_sink), TRUE);
	}
	privateContext->eosSignalled = false;
	privateContext->numberOfVideoBuffersSent = 0;
	aamp->mCorrectionRate = (double)AAMP_NORMAL_PLAY_RATE;
}

/**
 *  @brief Process discontinuity for a stream type
 */
bool AAMPGstPlayer::Discontinuity(AampMediaType type)
{
	bool ret = false;
	media_stream *stream = &privateContext->stream[type];
	AAMPLOG_MIL("Entering AAMPGstPlayer: type(%d) format(%d) firstBufferProcessed(%d)", (int)type, stream->format, stream->firstBufferProcessed);

	/*Handle discontinuity only if atleast one buffer is pushed*/
	if (stream->format != FORMAT_INVALID && stream->firstBufferProcessed == false)
	{
		AAMPLOG_WARN("Discontinuity received before first buffer - ignoring");
	}
	else
	{
		AAMPLOG_DEBUG("stream->format %d, stream->firstBufferProcessed %d", stream->format , stream->firstBufferProcessed);
		if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF) && ( !aamp->ReconfigureForCodecChange() ))
		{
			AAMPLOG_WARN("NO EOS: PTS-RESTAMP ENABLED and codec has not changed");
			aamp->CompleteDiscontinutyDataDeliverForPTSRestamp(type);
			ret = true;
		}
		else
		{
			if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && aamp->ReconfigureForCodecChange())
			{
				AAMPLOG_WARN("PTS-RESTAMP ENABLED, but we have codec change, so Signal EOS (%s).",GetMediaTypeName(type));
			}
			AAMPGstPlayer_SignalEOS(stream);
			// We are in buffering, but we received discontinuity, un-pause pipeline
			StopBuffering(true);
			ret = true;

			//If we have an audio discontinuity, signal subtec as well
			if ((type == eMEDIATYPE_AUDIO) && (privateContext->stream[eMEDIATYPE_SUBTITLE].source))
			{
				AAMPGstPlayer_SignalEOS(privateContext->stream[eMEDIATYPE_SUBTITLE]);
			}
		}
	}
	return ret;
}

/**
 *  @brief Check if PTS is changing
 *  @retval true if PTS changed from lastKnown PTS or timeout hasn't expired, will optimistically return true^M^M
 *                         if video-pts attribute is not available from decoder
 */
bool AAMPGstPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	bool ret = true;
	gint64 currentPTS = GetVideoPTS();			/* Gets the currentPTS from the 'video-pts' property of the element */
	if (currentPTS != 0)
	{
		if (currentPTS != privateContext->lastKnownPTS)
		{
			AAMPLOG_MIL("AAMPGstPlayer: There is an update in PTS prevPTS:%" G_GINT64_FORMAT " newPTS: %" G_GINT64_FORMAT "\n",
							privateContext->lastKnownPTS, currentPTS);
			privateContext->ptsUpdatedTimeMS = NOW_STEADY_TS_MS;			/* save a copy of the current steady clock in milliseconds */
			privateContext->lastKnownPTS = currentPTS;
		}
		else
		{
			long diff = NOW_STEADY_TS_MS - privateContext->ptsUpdatedTimeMS;
			if (diff > timeout)
			{
				AAMPLOG_WARN("AAMPGstPlayer: Video PTS hasn't been updated for %ld ms and timeout - %ld ms", diff, timeout);
				ret = false;
			}
		}
	}
	else
	{
		AAMPLOG_MIL("AAMPGstPlayer: video-pts parsed is: %" G_GINT64_FORMAT "\n",
			currentPTS);
	}
	return ret;
}

/**
 *  @brief Gets Video PTS
 */
long long AAMPGstPlayer::GetVideoPTS(void)
{
	gint64 currentPTS = 0;
	GstElement *element;
	const auto platformType = aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType);
	if(platformType == ePLATFORM_REALTEK)
	{
		element = privateContext->video_sink;
	}
	else
	{
		element = privateContext->video_dec;
	}

	if( element )
	{
		g_object_get(element, "video-pts", &currentPTS, NULL);			/* Gets the 'video-pts' from the element into the currentPTS */
		//Westeros sink sync returns PTS in 90Khz format where as some other platforms returns in 45 KHz,
		// hence converting to 90Khz
		if(platformType != ePLATFORM_REALTEK && !privateContext->using_westerossink)
		{
			currentPTS = currentPTS * 2; // convert from 45 KHz to 90 Khz PTS
		}
	}
	return (long long) currentPTS;
}


PlaybackQualityStruct* AAMPGstPlayer::GetVideoPlaybackQuality(void)
{
	GstStructure *stats= 0;
	GstElement *element;

	if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_REALTEK)
	{
		element = privateContext->video_sink;
	}
	else
	{
		element = privateContext->video_dec;
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
				privateContext->playbackQuality.rendered= g_value_get_uint64( value );
			}
			value= gst_structure_get_value( stats, "dropped" );
			if ( value )
			{
				privateContext->playbackQuality.dropped= g_value_get_uint64( value );
			}
			AAMPLOG_MIL("rendered %lld dropped %lld\n", privateContext->playbackQuality.rendered, privateContext->playbackQuality.dropped);
			gst_structure_free( stats );

			return &privateContext->playbackQuality;
		}
		else
		{
			AAMPLOG_ERR("Failed to get sink stats");
		}
	}
	return NULL;
}
/**
 *  @brief Reset EOS SignalledFlag
 */
void AAMPGstPlayer::ResetEOSSignalledFlag()
{
	privateContext->eosSignalled = false;
}

/**
 *  @brief Check if cache empty for a media type
 */
bool AAMPGstPlayer::IsCacheEmpty(AampMediaType mediaType)
{
	bool ret = true;
	media_stream *stream = &privateContext->stream[mediaType];
	if (stream->source)
	{
		guint64 cacheLevel = gst_app_src_get_current_level_bytes (GST_APP_SRC(stream->source));			/*Get the number of currently queued bytes inside stream->source)*/
		if(0 != cacheLevel)
		{
			AAMPLOG_TRACE("AAMPGstPlayer::Cache level  %" G_GUINT64_FORMAT "", cacheLevel);
			ret = false;
		}
		else
		{
			// Changed to AAMPLOG_TRACE, to avoid log flooding
			// We're seeing this logged frequently during live linear playback, despite no user-facing problem.
			AAMPLOG_TRACE("AAMPGstPlayer::Cache level empty");
			if (privateContext->stream[eMEDIATYPE_VIDEO].bufferUnderrun == true ||
				privateContext->stream[eMEDIATYPE_AUDIO].bufferUnderrun == true)				/* Interpret bufferUnderun as cachelevel being empty */
			{
				AAMPLOG_WARN("AAMPGstPlayer::Received buffer underrun signal for video(%d) or audio(%d) previously",privateContext->stream[eMEDIATYPE_VIDEO].bufferUnderrun,
							 privateContext->stream[eMEDIATYPE_AUDIO].bufferUnderrun);
			}
			else
			{
				bool ptsChanged = CheckForPTSChangeWithTimeout(AAMP_MIN_PTS_UPDATE_INTERVAL);
				if(!ptsChanged)
				{
					//PTS hasn't changed for the timeout value
					AAMPLOG_WARN("AAMPGstPlayer: Appsrc cache is empty and PTS hasn't been updated for more than %dms and ret(%d)",
								 AAMP_MIN_PTS_UPDATE_INTERVAL, ret);
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
 *  @brief Set pipeline to PLAYING state once fragment caching is complete
 */
void AAMPGstPlayer::NotifyFragmentCachingComplete()
{
	if(privateContext->pendingPlayState)
	{
		AAMPLOG_MIL("AAMPGstPlayer: Setting pipeline to PLAYING state ");
		if (SetStateWithWarnings(privateContext->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
		{
			AAMPLOG_ERR("AAMPGstPlayer_Configure GST_STATE_PLAYING failed");
		}
		privateContext->pendingPlayState = false;
	}
	else
	{
		AAMPLOG_MIL("AAMPGstPlayer: No pending PLAYING state");
	}
}

/**
 *  @brief Set pipeline to PAUSED state to wait on NotifyFragmentCachingComplete()
 */
void AAMPGstPlayer::NotifyFragmentCachingOngoing()
{
	if(!privateContext->paused)
	{
		Pause(true, true);
	}
	privateContext->pendingPlayState = true;
}

/**
 *  @brief Get video display's width and height
 */
void AAMPGstPlayer::GetVideoSize(int &width, int &height)
{
	int x;
	int y;
	int w = 0;
	int h = 0;

	if ((4 == sscanf(privateContext->videoRectangle, "%d,%d,%d,%d", &x, &y, &w, &h)) && (w > 0) && (h > 0))
	{
		width = w;
		height = h;
	}
}

/***
 * @fn  IsCodecSupported
 *
 * @brief Check whether Gstreamer platform has support of the given codec or not.
 *        codec to component mapping done in gstreamer side.
 * @param codecName - Name of codec to be checked
 * @return True if platform has the support else false
 */

bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	bool retValue = false;
	GstRegistry* registry = gst_registry_get();
	for (std::string &componentName: gmapDecoderLoookUptable[codecName])
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

/**
 * @brief function to check whether the device is having MS12V2 audio support or not
 */
bool AAMPGstPlayer::IsMS2V12Supported()
{
	bool IsMS12V2 = false;
#ifdef AAMP_MPD_DRM
		AampOutputProtection *pInstance = AampOutputProtection::GetAampOutputProcectionInstance();
		IsMS12V2  = pInstance->IsMS2V12Supported();
		pInstance->Release();
#endif
	return IsMS12V2;
}

/**
 *  @brief Increase the rank of AAMP decryptor plugins
 */
void AAMPGstPlayer::InitializeAAMPGstreamerPlugins()
{
	// Ensure GST is initialized
	if (!gst_init_check(nullptr, nullptr, nullptr)) {
		AAMPLOG_ERR("gst_init_check() failed");
	}

#ifdef AAMP_MPD_DRM
#define PLUGINS_TO_LOWER_RANK_MAX    2
	static const char *plugins_to_lower_rank[PLUGINS_TO_LOWER_RANK_MAX] = {
		"aacparse",
		"ac3parse",
	};
	GstRegistry* registry = gst_registry_get();

	GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, GstPluginNamePR);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available; reloading aamp plugin", GstPluginNamePR);
		GstPlugin * plugin = gst_plugin_load_by_name ("aamp");
		if(plugin)
		{
			gst_object_unref(plugin);
		}
		pluginFeature = gst_registry_lookup_feature(registry, GstPluginNamePR);
		if(pluginFeature == NULL)
			AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available", GstPluginNamePR);
	}
	if(pluginFeature)
	{
		// CID:313773 gst_registry_remove_feature() will unref pluginFeature internally and
		// gst_registry_add_feature() will ref it again. So to maintain the refcount we do a ref and unref here
		// gst_registry_lookup_feature() will return pluginFeature after incrementing refcount which is unreffed at the end
		gst_object_ref(pluginFeature);
		gst_registry_remove_feature (registry, pluginFeature);
		gst_registry_add_feature (registry, pluginFeature);
		gst_object_unref(pluginFeature);

		AAMPLOG_MIL("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNamePR);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameWV);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available", GstPluginNameWV);
	}
	else
	{
		AAMPLOG_MIL("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameWV);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameCK);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer: %s plugin feature not available", GstPluginNameCK);
	}
	else
	{
		AAMPLOG_MIL("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameCK);
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_PRIMARY + 111);
		gst_object_unref(pluginFeature);
	}

	pluginFeature = gst_registry_lookup_feature(registry, GstPluginNameVMX);

	if (pluginFeature == NULL)
	{
		AAMPLOG_ERR("AAMPGstPlayer %s plugin feature not available", GstPluginNameVMX);
	}
	else
	{
		AAMPLOG_MIL("AAMPGstPlayer %s plugin priority set to GST_RANK_PRIMARY + 111", GstPluginNameVMX);
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
			AAMPLOG_MIL("AAMPGstPlayer: %s plugin priority set to GST_RANK_PRIMARY  - 1\n", plugins_to_lower_rank[i]);
		}
	}
#endif
}

/**
 *  @brief To enable certain aamp configs based upon platform check
 */
PlatformType AAMPGstPlayer::InferPlatformFromPluginScan()
{
	// Ensure GST is initialized
	if (!gst_init_check(nullptr, nullptr, nullptr)) {
		AAMPLOG_ERR("gst_init_check() failed");
	}
	static const std::pair<const char*, PlatformType> plugins[] = {
		{"amlhalasink", ePLATFORM_AMLOGIC},
		{"omxeac3dec", ePLATFORM_REALTEK},
		{"brcmaudiodecoder", ePLATFORM_BROADCOM},
	};

	GstRegistry* registry = gst_registry_get();

	for (const auto& plugin : plugins)
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, plugin.first);
		if (pluginFeature)
		{
			gst_object_unref(pluginFeature);
			AAMPLOG_MIL("AAMPGstPlayer: %s plugin found in registry", plugin.first);
			return plugin.second;
		}
	}

	AAMPLOG_WARN("AAMPGstPlayer: None of the plugins found in registry");
	return ePLATFORM_DEFAULT;
}


/**
 * @brief Notify EOS to core aamp asynchronously if required.
 * @note Used internally by AAMPGstPlayer
 */
void AAMPGstPlayer::NotifyEOS()
{
	if (!privateContext->eosSignalled)
	{
		if (!privateContext->eosCallbackIdleTaskPending)
		{
			/*Scheduling and executed async task immediately without returing the task id.
			Which is leading to set the task pending always true when SLE is reached END_OF_LIST.
			Due to this 30 tick is reported. changing the logic to set task pending to true before adding the task in notifyEOS function
			and making it pending task to false if task id is invalid and eoscallback is pending.*/
			privateContext->eosCallbackIdleTaskPending = true;
			// eosSignalled is reset once the async task is completed either in Configure/Flush/ResetEOSSignalled, so set the flag before scheduling the task
			privateContext->eosSignalled = true;
			privateContext->eosCallbackIdleTaskId = aamp->ScheduleAsyncTask(IdleCallbackOnEOS, (void *)this, "IdleCallbackOnEOS");
			if (privateContext->eosCallbackIdleTaskId == AAMP_TASK_ID_INVALID && true == privateContext->eosCallbackIdleTaskPending)
			{
				privateContext->eosCallbackIdleTaskPending = false;
				AAMPLOG_MIL("eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
							(privateContext->eosCallbackIdleTaskPending ? 1 : 0),privateContext->eosCallbackIdleTaskId);
			}
			else
			{
				AAMPLOG_MIL("eosCallbackIdleTask scheduled eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
								(privateContext->eosCallbackIdleTaskPending ? 1 : 0),privateContext->eosCallbackIdleTaskId);
			}
		}
		else
		{
			AAMPLOG_WARN("IdleCallbackOnEOS already registered previously, hence skip! eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
														(privateContext->eosCallbackIdleTaskPending ? 1 : 0),privateContext->eosCallbackIdleTaskId);
		}
	}
	else
	{
		AAMPLOG_WARN("EOS already signaled, hence skip! eosCallbackIdleTaskPending(%d),eosCallbackIdleTaskId(%d)",
														(privateContext->eosCallbackIdleTaskPending ? 1 : 0),privateContext->eosCallbackIdleTaskId);
	}
}

/**
 *  @brief Dump a file to log
 */
static void DumpFile(const char* fileName)
{
	int c;
	FILE *fp = fopen(fileName, "r");
	if (fp)
	{
		printf("\n************************Dump %s **************************\n", fileName);
		c = getc(fp);
		while (c != EOF)
		{
			printf("%c", c);
			c = getc(fp);
		}
		fclose(fp);
		printf("\n**********************Dump %s end *************************\n", fileName);
	}
	else
	{
		AAMPLOG_WARN("Could not open %s", fileName);
	}
}

/**
 *  @brief Dump diagnostic information
 *
 */
void AAMPGstPlayer::DumpDiagnostics()
{
	AAMPLOG_MIL("video_dec %p audio_dec %p video_sink %p audio_sink %p numberOfVideoBuffersSent %d",
			privateContext->video_dec, privateContext->audio_dec, privateContext->video_sink,
			privateContext->audio_sink, privateContext->numberOfVideoBuffersSent);
	if(aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType) == ePLATFORM_BROADCOM)
	{
		DumpFile("/proc/brcm/transport");
		DumpFile("/proc/brcm/video_decoder");
		DumpFile("/proc/brcm/audio");
	}
}

/**
 *  @brief Signal trick mode discontinuity to gstreamer pipeline
 */
void AAMPGstPlayer::SignalTrickModeDiscontinuity()
{
	media_stream* stream = &privateContext->stream[eMEDIATYPE_VIDEO];
	if (stream && (privateContext->rate != AAMP_NORMAL_PLAY_RATE) )
	{
		GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");
		int vodTrickplayFPS = GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS);
		GstStructure * eventStruct = gst_structure_new("aamp-tm-disc", "fps", G_TYPE_UINT, (guint)vodTrickplayFPS, NULL);
		if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
		{
			AAMPLOG_WARN("Error on sending aamp-tm-disc");
		}
		else
		{
			AAMPLOG_MIL("Sent aamp-tm-disc event");
		}
		gst_object_unref(sourceEleSrcPad);
	}
}

/**
 *  @brief Flush the data in case of a new tune pipeline
 */
void AAMPGstPlayer::SeekStreamSink(double position, double rate)
{
	// shouldTearDown is set to false, because in case of a new tune pipeline
	// might not be in a playing/paused state which causes Flush() to destroy
	// pipeline. This has to be avoided.
	Flush(position, rate, false);

}

/**
 *  @brief Get the video rectangle co-ordinates
 */
std::string AAMPGstPlayer::GetVideoRectangle()
{
	return std::string(privateContext->videoRectangle);
}

/**
 *  @brief Un-pause pipeline and notify buffer end event to player
 */
void AAMPGstPlayer::StopBuffering(bool forceStop)
{
	pthread_mutex_lock(&mBufferingLock);
	//Check if we are in buffering
	if (ISCONFIGSET(eAAMPConfig_ReportBufferEvent) && privateContext->video_dec && aamp->GetBufUnderFlowStatus())
	{
		int frames = -1;
		g_object_get(privateContext->video_dec,"queued_frames",(uint*)&frames,NULL);
		bool stopBuffering = forceStop;
		if( !stopBuffering )
		{
			if (frames == -1 || frames >= GETCONFIGVALUE(eAAMPConfig_RequiredQueuedFrames) )
			{
				stopBuffering = true;
			}
		}
		if (stopBuffering)
		{
			AAMPLOG_MIL("Enough data available to stop buffering, frames %d !", frames);
			GstState current, pending;
			bool sendEndEvent = false;

			if(GST_STATE_CHANGE_FAILURE != gst_element_get_state(privateContext->pipeline, &current, &pending, 0 * GST_MSECOND))
			{
				if (current == GST_STATE_PLAYING)
				{
					sendEndEvent = true;
				}
				else
				{
					sendEndEvent = aamp->PausePipeline(false, false);
					aamp->UpdateSubtitleTimestamp();
				}
			}
			else
			{
				sendEndEvent = false;
			}

			if( !sendEndEvent )
			{
				AAMPLOG_ERR("Failed to un-pause pipeline for stop buffering!");
			}
			else
			{
				aamp->SendBufferChangeEvent();		/* To indicate buffer availability */
			}
	        }
		else
		{
			static int bufferLogCount = 0;
			if (0 == (bufferLogCount++ % 10) )
			{
				AAMPLOG_WARN("Not enough data available to stop buffering, frames %d !", frames);
			}
		}
	}
	pthread_mutex_unlock(&mBufferingLock);
}


void type_check_instance(const char * str, GstElement * elem)
{
	AAMPLOG_MIL("%s %p type_check %d", str, elem, G_TYPE_CHECK_INSTANCE (elem));
}

/**
 * @brief Wait for source element to be configured.
 */
bool AAMPGstPlayer::WaitForSourceSetup(AampMediaType mediaType)
{
	bool ret = false;
	int timeRemaining = GETCONFIGVALUE(eAAMPConfig_SourceSetupTimeout);
	media_stream *stream = &privateContext->stream[mediaType];

	int waitInterval = 100; //ms

	AAMPLOG_WARN("Source element[%p] for track[%d] not configured, wait for setup to complete!", stream->source, mediaType);
	while(timeRemaining >= 0)
	{
		aamp->InterruptableMsSleep(waitInterval);	/*Sleep until timeout is reached or interrupted*/
		if (aamp->DownloadsAreEnabled())
		{
			if (stream->sourceConfigured)
			{
				AAMPLOG_MIL("Source element[%p] for track[%d] setup completed!", stream->source, mediaType);
				ret = true;
				break;
			}
		}
		else
		{
			//Playback stopped by application
			break;
		}
		timeRemaining -= waitInterval;
	}

	if (!ret)
	{
		AAMPLOG_WARN("Wait for source element setup for track[%d] exited/timedout!", mediaType);
	}
	return ret;
}

/**
 *  @brief Forward buffer to aux pipeline
 */
void AAMPGstPlayer::ForwardBuffersToAuxPipeline(GstBuffer *buffer)
{
	media_stream *stream = &privateContext->stream[eMEDIATYPE_AUX_AUDIO];
	if (!stream->sourceConfigured && stream->format != FORMAT_INVALID)
	{
		bool status = WaitForSourceSetup(eMEDIATYPE_AUX_AUDIO);
		if (!aamp->DownloadsAreEnabled() || !status)
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
			AAMPLOG_ERR("Error while copying audio buffer to auxiliary buffer!!");
			gst_buffer_unref(fwdBuffer);
			return;
		}
		//AAMPLOG_TRACE("Forward audio buffer to auxiliary pipeline!!");
		GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(stream->source), fwdBuffer);
		if (ret != GST_FLOW_OK)
		{
			AAMPLOG_ERR("gst_app_src_push_buffer error: %d[%s] mediaType %d", ret, gst_flow_get_name (ret), (int)eMEDIATYPE_AUX_AUDIO);
			assert(false);
		}
	}
}

/**
 *  @brief Check if audio buffers to be forwarded or not
 */
bool AAMPGstPlayer::ForwardAudioBuffersToAux()
{
	return (privateContext->forwardAudioBuffers && privateContext->stream[eMEDIATYPE_AUX_AUDIO].format != FORMAT_INVALID);
}

/**
 * @}
 */

/**
 * @brief  Set playback rate to audio/video sinks
 */
bool AAMPGstPlayer::SetPlayBackRate ( double rate )
{
	/** For gst version 1.18*/
	const auto platform = aamp->mConfig->GetConfigValue(eAAMPConfig_PlatformType);
#if GST_CHECK_VERSION(1,18,0)
// avoid compilation failure if building with Ubuntu 20.04 having gst<1.18
	if (platform == ePLATFORM_AMLOGIC)
	{
		AAMPLOG_TRACE("AAMPGstPlayer: gst_event_new_instant_rate_change: %f ...V6", rate);
		for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
		{
			if( (iTrack != (int)eMEDIATYPE_SUBTITLE) && privateContext->stream[iTrack].source != NULL)
			{
				GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(privateContext->stream[iTrack].source), "src");
				gst_pad_send_event(sourceEleSrcPad, gst_event_new_seek (rate, GST_FORMAT_TIME,
							static_cast<GstSeekFlags>(GST_SEEK_FLAG_INSTANT_RATE_CHANGE), GST_SEEK_TYPE_NONE,
							0, GST_SEEK_TYPE_NONE, 0));
				AAMPLOG_INFO("Seeking in %s ( %d )", GetMediaTypeName(static_cast<AampMediaType>(iTrack)), iTrack);
				gst_object_unref(sourceEleSrcPad);
			}
		}
		AAMPLOG_MIL ("Current rate: %g", rate);
	}
	else
#endif
	{
		if (platform == ePLATFORM_REALTEK || platform == ePLATFORM_AMLOGIC)
		{
			AAMPLOG_MIL("AAMPGstPlayer: =send custom-instant-rate-change : %f ...", rate);
			GstStructure *structure = gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, rate, NULL);
			if (!structure)
			{
				AAMPLOG_ERR("AAMPGstPlayer: Failed to create custom-instant-rate-change structure");
				return false;
			}

			/* The above statement creates a new GstStructure with the name
			'custom-instant-rate-change' that has a member variable
			'rate' of G_TYPE_DOUBLE and a value of rate i.e. second last parameter */
			GstEvent * rate_event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure);
			if (!rate_event)
			{
				AAMPLOG_ERR("AAMPGstPlayer: Failed to create rate_event");
				/* cleanup */
				gst_structure_free (structure);
				return false;
			}
			int ret = gst_element_send_event( privateContext->pipeline, rate_event );
			if(!ret)
			{
				AAMPLOG_ERR("AAMPGstPlayer: Rate change failed : %g [gst_element_send_event]", rate);
				return false;
			}
			AAMPLOG_MIL ("Current rate: %g", rate);
		}
		else if(platform == ePLATFORM_BROADCOM)
		{
			AAMPLOG_MIL("send custom-instant-rate-change : %f ...", rate);

			GstStructure *structure = gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, rate, NULL);
			if (!structure)
			{
				AAMPLOG_ERR("failed to create custom-instant-rate-change structure");
				return false;
			}

			GstEvent * rate_event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure);
			if (!rate_event)
			{
				AAMPLOG_ERR("failed to create rate_event");
				/* cleanup */
				gst_structure_free (structure);
				return false;
			}

			AAMPLOG_MIL("rate_event %p video_decoder %p audio_decoder %p", (void*)rate_event, (void*)privateContext->video_dec, (void *)privateContext->audio_dec);
			if (privateContext->video_dec)
			{
				if (!gst_element_send_event(privateContext->video_dec,  gst_event_ref(rate_event)))
				{
					AAMPLOG_ERR("failed to push rate_event %p to video sink %p", (void*)rate_event, (void*)privateContext->video_dec);
				}
			}

			if (privateContext->audio_dec)
			{
				if (!gst_element_send_event(privateContext->audio_dec,  gst_event_ref(rate_event)))
				{
					AAMPLOG_ERR("failed to push rate_event %p to audio decoder %p", (void*)rate_event, (void*)privateContext->audio_dec);
				}
			}
			// Unref since we have explicitly increased ref count
			gst_event_unref(rate_event);
			AAMPLOG_MIL ("Current rate: %g", rate);
		}
		else
		{
			return false;
		}
	}
	return true;
}

/**
  * @brief Set the text style of the subtitle to the options passed
  */
bool AAMPGstPlayer::SetTextStyle(const std::string &options)
{
	bool ret = false;

	if (privateContext->subtitle_sink)
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
				g_object_set(privateContext->subtitle_sink, "attribute-values", attributes, NULL);
				gst_structure_free (attributes);
			}
		}
		ret = true;
	}
	else
	{
		AAMPLOG_INFO("AAMPGstPlayer: subtitle sink not set");
	}

	return ret;
}

/**
 * @fn SendQtDemuxOverrideEvent
 * @param[in] mediaType stream type
 * @param[in] pts position value of buffer
 * @param[in] ptr buffer pointer
 * @param[in] len length of buffer
 * @ret TRUE if override is enabled, FALSE otherwise
 */
gboolean AAMPGstPlayer::SendQtDemuxOverrideEvent(AampMediaType mediaType, GstClockTime pts, const void *ptr, size_t len)
{
	media_stream* stream = &privateContext->stream[mediaType];
	gboolean enableOverride = false;
	if (!ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
	{
		enableOverride = (privateContext->rate != AAMP_NORMAL_PLAY_RATE);
	}
	GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");	/* Retrieves the src pad */
	if (stream->format == FORMAT_ISO_BMFF && mediaType != eMEDIATYPE_SUBTITLE)
	{
		int vodTrickplayFPS = GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS);
		/* 	The below statement creates a new eventStruct with the name 'aamp_override' and sets its three variables as follows:-
			1) the variable 'enable' has datatype of G_TYPE_BOOLEAN and has value enableOverride.
			2) the variable 'rate' has datatype of G_TYPE_FLOAT and is set to (float)privateContext->rate.
			3) the variable 'aampplayer' has datatype of G_TYPE_BOOLEAN and a value of TRUE.
		*/
		GstStructure * eventStruct = gst_structure_new("aamp_override", "enable", G_TYPE_BOOLEAN, enableOverride, "rate", G_TYPE_FLOAT, (float)privateContext->rate, "aampplayer", G_TYPE_BOOLEAN, TRUE, "fps", G_TYPE_UINT, (guint)vodTrickplayFPS, NULL);
		if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
		{
			AAMPLOG_ERR("Error on sending qtdemux override event");
		}
	}
	gst_object_unref(sourceEleSrcPad);
	return enableOverride;
}

/**
 * @fn SignalSubtitleClock
 * @brief Signal the new clock to subtitle module
 * @param[in] verboseDebug - enable more debug
 * @return - true indicating successful operation in sending the clock update
 */
bool AAMPGstPlayer::SignalSubtitleClock(bool verboseDebug)
{
	//AAMPLOG_TRACE("Enter SignalSubtitleClock");
	bool signalSent=false;
	media_stream* stream = &privateContext->stream[eMEDIATYPE_SUBTITLE];
	if ( stream && (stream->format != FORMAT_INVALID) )
	{
		if (!stream->source)
		{
			AAMPLOG_ERR("subtitle appsrc is NULL");
		}
		else if (!GST_IS_APP_SRC(stream->source))
		{
			AAMPLOG_ERR("subtitle appsrc is invalid");
		}
		else
		{
			//Check if pipeline is in playing/paused state.
			GstState current, pending;
			GstStateChangeReturn ret;
			ret = gst_element_get_state(privateContext->pipeline, &current, &pending, 0);
			bool underflowState=aamp->GetBufUnderFlowStatus();		
			if ( (current == GST_STATE_PLAYING) && (ret != GST_STATE_CHANGE_FAILURE) && (underflowState != true) )
			{
				GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(stream->source), "src");	/* Retrieves the src pad */
				if (sourceEleSrcPad != NULL)
				{
					gint64 videoPTS = aamp->GetVideoPTS();
					if (videoPTS > 0)
					{
						//GetVideoPTS returns PTS in 90KHz clock, convert it to nanoseconds for max precision
						GstClockTime pts = ((double)videoPTS / 90000.0) * GST_SECOND;						
						GstStructure * eventStruct = gst_structure_new("sub_clock_sync", "current-pts", G_TYPE_UINT64, pts, NULL);
						if (!gst_pad_push_event(sourceEleSrcPad, gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, eventStruct)))
						{
							AAMPLOG_ERR("Error on sending sub_clock_sync event");
							AAMPLOG_ERR("Got VideoPTS: %" G_GINT64_FORMAT " and converted pts: %" G_GUINT64_FORMAT " , state = %d, pending = %d", videoPTS, pts, current, pending);
						}
						else
						{
							if (verboseDebug)
							{
								AAMPLOG_WARN("Sent sub_clock_sync event, pts = %" G_GUINT64_FORMAT ", pts from sink was %" G_GUINT64_FORMAT "", pts, videoPTS);
						       	}
							else
							{
								AAMPLOG_DEBUG("Sent sub_clock_sync event, pts = %" G_GUINT64_FORMAT ", pts from sink was %" G_GUINT64_FORMAT "", pts, videoPTS);
							}
							signalSent=true;
						}
					}
					else
					{
						AAMPLOG_INFO("Got invalid video PTS: %" G_GINT64_FORMAT ". Clock not sent.", videoPTS);
					}
					gst_object_unref(sourceEleSrcPad);
				}
				else
				{
					AAMPLOG_ERR("sourceEleSrcPad is NULL. Failed to send subtec clock event");
				}
			}
			else
			{
				AAMPLOG_TRACE("Not sending clock event in non-play state to avoid gstreamer lockup, state = %d, pending = %d, underflow = %d.",
					current, pending, underflowState);
			}
		}
	}
	else
	{
		if (stream)
		{		
			AAMPLOG_WARN("Invalid stream->format = %d)", stream->format);
		}
		else
		{
			AAMPLOG_ERR("stream invalid)");
		}
	}
	//AAMPLOG_TRACE("Exit SignalSubtitleClock");
	return signalSent;
}

void AAMPGstPlayer::GetBufferControlData(AampMediaType mediaType, BufferControlData &data) const
{
	const media_stream *stream = &privateContext->stream[mediaType];

	data.StreamReady = stream->sinkbin && stream->sourceConfigured;
	if (data.StreamReady)
	{
		data.ElapsedSeconds = std::abs(aamp->GetPositionRelativeToSeekSeconds());

		GstState current;
		GstState pending;
		gst_element_get_state(stream->sinkbin, &current, &pending, 0);

		/* Transitions to Paused can block due to lack of data
		** state should match aamp target play/pause state*/
		bool pipelineShouldBePlaying = !privateContext->paused;
		data.GstWaitingForData = ((pending == GST_STATE_PAUSED) ||
								  (pipelineShouldBePlaying && (current != GST_STATE_PLAYING)));
		if (data.GstWaitingForData)
		{
			AAMPLOG_WARN("BufferControlExternalData %s GStreamer (current %s, %s, should be %s))",
			GetMediaTypeName(mediaType), gst_element_state_get_name(current),
			gst_element_state_get_name(pending),
			pipelineShouldBePlaying ? "GST_STATE_PLAYING" : "GST_STATE_PAUSED");
		}
	}
	else
	{
		data.ElapsedSeconds = 0;
		data.GstWaitingForData = false;
	}
}

/**
 * @fn SetSeekPosition
 * @param[in] positionSecs - the start position to seek the pipeline to in seconds
 */
void AAMPGstPlayer::SetSeekPosition(double positionSecs)
{
	privateContext->seekPosition = positionSecs;
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		privateContext->stream[i].pendingSeek = true;
	}
}

void AAMPGstPlayer::SetPauseOnStartPlayback(bool enable)
{
	privateContext->pauseOnStartPlayback = enable;
}
