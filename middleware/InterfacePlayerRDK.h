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

#ifndef INTERFACE_PLAYER_H
#define INTERFACE_PLAYER_H

#include <map>
#include <cstdint>
#include <string.h>
#include <vector>
#include <mutex>
#include "PlayerLogManager.h"
#include "PlayerScheduler.h"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <atomic>
#include <set>
#include <mutex>
#include "GstHandlerControl.h"
#include "gstplayertaskpool.h"
#include <functional>
#include <condition_variable>
#include <chrono>
#include "GstUtils.h"
#include <any>
#include "SocUtils.h"

/**
 * @enum eGstPlayFlags
 * @brief Enum of configuration flags used by playbin
 */

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5
#define GST_NORMAL_PLAY_RATE 1
#define GST_TRACK_COUNT 4 /**< internal use - audio+video+sub+aux track */
#define VIDEO_COORDINATES_SIZE 32
#define GST_TASK_ID_INVALID 0
#define GST_NORMAL_PLAY_RATE 1
#define GST_ERROR_DESCRIPTION_LENGTH 256
#define NOW_STEADY_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() /**< Getting current steady clock in milliseconds */

/*
 *@enum msgType
 */
enum MsgType
{
	MESSAGE_ERROR,
	MESSAGE_WARNING,
	MESSAGE_EOS,
	MESSAGE_APPLICATION,
	MESSAGE_STATE_CHANGE,
	MESSAGE_CLOCK_LOST
};

struct BusEventData
{
	MsgType msgType;
	std::string msg;
	std::string dbg_info;
	bool setPlaybackRate;
	bool firstBufferProcessed;
	bool receivedFirstFrame;
};

/**
 * @brief Function pointer for the idle task
 * @param[in] arg - Arguments
 * @return Idle task status
 */
typedef int (*BackgroundTask)(void *arg);

struct Configs
{
	std::string networkProxyValue;
	bool gstreamerSubsEnabled;
	bool tcpServerSink;
	int tcpPort;
	bool appSrcForProgressivePlayback;
	bool seamlessAudioSwitch;
	bool enablePTSReStamp;
	std::string manifestUrl;
	int media;
	int videoBufBytes;
	bool enableDisconnectSignals;
	int eosInjectionMode;
	bool enablePtsSync;
	int framesToQueue;
	int vodTrickModeFPS;
	int enableGstPosQuery;
	int audioBufBytes;
	std::string networkProxy;
	float progressTimer;
	bool audioOnlyMode;
	bool enableRectPropertyCfg;
	bool audioDecoderStreamSync;
	bool gstreamerBufferingBeforePlay;
	bool seiTimeCode;
	bool useWesterosSink;
	bool useRialtoSink;
	bool gstLogging;
	bool progressLogging;
	bool monitorAV;
	bool disableUnderflow;
	int monitorAvsyncThresholdMs;
	int monitorJumpThresholdMs;
};

typedef enum
{
	eGST_MEDIAFORMAT_HLS,				   /**< HLS Media */
	eGST_MEDIAFORMAT_DASH,				   /**< Dash Media */
	eGST_MEDIAFORMAT_PROGRESSIVE,		   /**< Progressive Media */
	eGST_MEDIAFORMAT_HLS_MP4,			   /**< HLS mp4 Format */
	eGST_MEDIAFORMAT_OTA,				   /**< OTA Media */
	eGST_MEDIAFORMAT_HDMI,				   /**< HDMI Format */
	eGST_MEDIAFORMAT_COMPOSITE,			   /**< Composite Media */
	eGST_MEDIAFORMAT_SMOOTHSTREAMINGMEDIA, /**< Smooth streaming Media */
	eGST_MEDIAFORMAT_RMF,				   /**< RMF media */
	eGST_MEDIAFORMAT_UNKNOWN			   /**< Unknown media format */
} GstMediaFormat;

struct GstTaskControlData
{
	guint taskID;
	bool taskIsPending;
	std::string taskName;
	GstTaskControlData(const char *taskIdent) : taskID(0), taskIsPending(false), taskName(taskIdent ? taskIdent : "undefined") {};
};

enum GstVideoZoomMode
{
	GST_VIDEO_ZOOM_NONE = 0,		   /**< Video Zoom None */
	GST_VIDEO_ZOOM_DIRECT = 1,		   /**< Video Zoom Direct */
	GST_VIDEO_ZOOM_NORMAL = 2,		   /**< Video Zoom Normal */
	GST_VIDEO_ZOOM_16X9_STRETCH = 3,   /**< Video Zoom 16x9 stretch */
	GST_VIDEO_ZOOM_4x3_PILLAR_BOX = 4, /**< Video Zoom 4x3 pillar box */
	GST_VIDEO_ZOOM_FULL = 5,		   /**< Video Zoom Full */
	GST_VIDEO_ZOOM_GLOBAL = 6		   /**< Video Zoom Global */
};

typedef struct GstPlaybackQualityData
{
	long long rendered;
	long long dropped;
} GstPlaybackQualityStruct;

enum GstEOSInjectionModeCode
{
	/* EOS events are only injected into the gstreamer pipeline by
	 * InterfacePlayerRDK::EndOfStreamReached() & InterfacePlayerRDK::Discontinuity().
	 */
	GstEOS_INJECTION_MODE_NO_EXTRA,

	/* In addition to the EOS_INJECTION_MODE_NO_EXTRA cases
	 * EOS is injected in InterfacePlayerRDK::Stop() prior to setting the state to null.
	 */
	GstEOS_INJECTION_MODE_STOP_ONLY,
};

typedef enum
{
	eGST_STATE_IDLE,		 /**< 0  - Player is idle */
	eGST_STATE_INITIALIZING, /**< 1  - Player is initializing a particular content */
	eGST_STATE_INITIALIZED,	 /**< 2  - Player has initialized for a content successfully */
	eGST_STATE_PREPARING,	 /**< 3  - Player is loading all associated resources */
	eGST_STATE_PREPARED,	 /**< 4  - Player has loaded all associated resources successfully */
	eGST_STATE_BUFFERING,	 /**< 5  - Player is in buffering state */
	eGST_STATE_PAUSED,		 /**< 6  - Playback is paused */
	eGST_STATE_SEEKING,		 /**< 7  - Seek is in progress */
	eGST_STATE_PLAYING,		 /**< 8  - Playback is in progress */
	eGST_STATE_STOPPING,	 /**< 9  - Player is stopping the playback */
	eGST_STATE_STOPPED,		 /**< 10 - Player has stopped playback successfully */
	eGST_STATE_COMPLETE,	 /**< 11 - Playback completed */
	eGST_STATE_ERROR,		 /**< 12 - Error encountered and playback stopped */
	eGST_STATE_RELEASED,	 /**< 13 - Player has released all resources for playback */
	eGST_STATE_BLOCKED		 /**< 14 - Player has blocked and cant play content*/
} GstPrivPlayerState;

enum class InterfaceCB
{
	firstVideoFrameDisplayed,
	idleCb,
	progressCb,
	firstVideoFrameReceived,
	notifyEOS,
	startNewSubtitleStream // Add more events here if needed
};

/**
 * @name gstMapDecoderLookUptable
 *
 * @brief Decoder map list lookup table
 * convert from codec to string map list of gstreamer
 * component.
 */
static std::map<std::string, std::vector<std::string>> gstMapDecoderLookUptable =
	{
		{"ac-3", {"omxac3dec", "avdec_ac3", "avdec_ac3_fixed"}},
		{"ac-4", {"omxac4dec"}}};

struct gst_media_stream
{
	GstElement *sinkbin;		  /**< Sink element to consume data */
	GstElement *source;			  /**< to provide data to the pipleline */
	GstStreamOutputFormat format; /**< Stream output format for this stream */
	bool pendingSeek;			  /**< Flag denotes if a seek event has to be sent to the source */
	bool resetPosition;			  /**< To indicate that the position of the stream is reset */
	bool bufferUnderrun;
	bool eosReached;	   /**< To indicate the status of End of Stream reached */
	bool sourceConfigured; /**< To indicate that the current source is initialized and configured */
	pthread_mutex_t sourceLock;
	uint32_t timeScale;
	int32_t trackId;		   /**< Current Audio Track Id,so far it is implemented for AC4 track selection only */
	bool firstBufferProcessed; /**< Indicates if the first buffer is processed in this stream */
	GstPad *demuxPad;		   /**< Demux src pad >*/
	gulong demuxProbeId;	   /**< Demux pad probe ID >*/

	gst_media_stream() : sinkbin(NULL), source(NULL), format(GST_FORMAT_INVALID),
						 pendingSeek(false), resetPosition(false),
						 bufferUnderrun(false), eosReached(false), sourceConfigured(false), sourceLock(PTHREAD_MUTEX_INITIALIZER), timeScale(1), trackId(-1), firstBufferProcessed(false), demuxPad(NULL), demuxProbeId(0)
	{
	}

	~gst_media_stream()
	{
		g_clear_object(&sinkbin);
		g_clear_object(&source);
	}

	gst_media_stream(const gst_media_stream &) = delete;

	gst_media_stream &operator=(const gst_media_stream &) = delete;
};

struct MonitorAVState
{
	long long tLastReported;
	long long tLastSampled;
	const char *description;
	gint64 av_position[2];
	bool happy;
};
/**
 * @struct GstPlayerPriv
 * @brief Holds private variables of InterfacePlayerRDK
 */
struct GstPlayerPriv
{
	GstPlayerPriv(const GstPlayerPriv &) = delete;
	GstPlayerPriv &operator=(const GstPlayerPriv &) = delete;

	gst_media_stream stream[GST_TRACK_COUNT];
	MonitorAVState monitorAVstate;
	GstElement *pipeline; /**< GstPipeline used for playback. */
	GstBus *bus;		  /**< Bus for receiving GstEvents from pipeline. */
	guint64 total_bytes;
	gint n_audio;				 /**< Number of audio tracks. */
	gint current_audio;			 /**< Offset of current audio track. */
	std::mutex TaskControlMutex; /**< For scheduling/de-scheduling or resetting async tasks/variables and timers */
	GstTaskControlData firstProgressCallbackIdleTask;
	guint periodicProgressCallbackIdleTaskId; /**< ID of timed handler created for notifying progress events. */
	guint bufferingTimeoutTimerId;			  /**< ID of timer handler created for buffering timeout. */
	GstElement *video_dec;					  /**< Video decoder used by pipeline. */
	GstElement *audio_dec;					  /**< Audio decoder used by pipeline. */
	GstElement *video_sink;					  /**< Video sink used by pipeline. */
	GstElement *audio_sink;					  /**< Audio sink used by pipeline. */
	GstElement *subtitle_sink;				  /**< Subtitle sink used by pipeline. */
	GstTaskPool *task_pool;					  /**< Task pool in case RT priority is needed. */

	int rate;											 /**< Current playback rate. */
	GstVideoZoomMode zoom;								 /**< Video-zoom setting. */
	bool videoMuted;									 /**< Video mute status. */
	bool audioMuted;									 /**< Audio mute status. */
	std::mutex volumeMuteMutex;							 /**< Mutex to ensure setVolumeOrMuteUnMute is thread-safe. */
	bool subtitleMuted;									 /**< Subtitle mute status. */
	double audioVolume;									 /**< Audio volume. */
	guint eosCallbackIdleTaskId;						 /**< ID of idle handler created for notifying EOS event. */
	std::atomic<bool> eosCallbackIdleTaskPending;		 /**< Set if any eos callback is pending. */
	bool firstFrameReceived;							 /**< Flag that denotes if first frame was notified. */
	bool pendingPlayState;								 /**< Flag that denotes if set pipeline to PLAYING state is pending. */
	bool decoderHandleNotified;							 /**< Flag that denotes if decoder handle was notified. */
	guint firstFrameCallbackIdleTaskId;					 /**< ID of idle handler created for notifying first frame event. */
	GstEvent *protectionEvent[GST_TRACK_COUNT];			 /**< GstEvent holding the pssi data to be sent downstream. */
	std::atomic<bool> firstFrameCallbackIdleTaskPending; /**< Set if any first frame callback is pending. */
	bool using_westerossink;							 /**< true if westeros sink is used as video sink */
	bool usingRialtoSink;								 /**< true if rialto sink is used for video and audio sinks */
	char videoRectangle[VIDEO_COORDINATES_SIZE];
	bool pauseOnStartPlayback;								 /**< true if should start playback paused */
	std::atomic<bool> eosSignalled;							 /**< Indicates if EOS has signaled */
	gboolean buffering_enabled;								 /**< enable buffering based on multiqueue */
	gboolean buffering_in_progress;							 /**< buffering is in progress */
	guint buffering_timeout_cnt;							 /**< make sure buffering_timeout doesn't get stuck */
	GstState buffering_target_state;						 /**< the target state after buffering */
	gint64 lastKnownPTS;									 /**< To store the PTS of last displayed video */
	long long ptsUpdatedTimeMS;								 /**< Timestamp when PTS was last updated */
	guint ptsCheckForEosOnUnderflowIdleTaskId;				 /**< ID of task to ensure video PTS is not moving before notifying EOS on underflow. */
	int numberOfVideoBuffersSent;							 /**< Number of video buffers sent to pipeline */
	gint64 segmentStart;									 /**< segment start value; required when qtdemux is enabled or restamping is disabled; -1 to send a segment.start query to gstreamer */
	GstQuery *positionQuery;								 /**< pointer that holds a position query object */
	GstQuery *durationQuery;								 /**< pointer that holds a duration query object */
	bool paused;											 /**< if pipeline is deliberately put in PAUSED state due to user interaction */
	GstState pipelineState;									 /**< current state of pipeline */
	GstTaskControlData firstVideoFrameDisplayedCallbackTask; /**< Task control data of the handler created for notifying state changed to Playing */
	bool firstTuneWithWesterosSinkOff;						 /**< track if first tune was done for Realtekce build */
	long long decodeErrorMsgTimeMS;							 /**< Timestamp when decode error message last posted */
	int decodeErrorCBCount;									 /**< Total decode error cb received within threshold time */
	bool progressiveBufferingEnabled;
	bool progressiveBufferingStatus;
	bool forwardAudioBuffers;				  /**< flag denotes if audio buffers to be forwarded to aux pipeline */
	bool enableSEITimeCode;					  /**< Enables SEI Time Code handling */
	bool firstVideoFrameReceived;			  /**< flag that denotes if first video frame was notified. */
	bool firstAudioFrameReceived;			  /**< flag that denotes if first audio frame was notified */
	int NumberOfTracks;						  /**< Indicates the number of tracks */
	GstPlaybackQualityStruct playbackQuality; /**< video playback quality info */
	struct CallbackData
	{
		gpointer instance;
		gulong id;
		std::string name;
		CallbackData(gpointer _instance, gulong _id, std::string _name) : instance(_instance), id(_id), name(_name) {};
		CallbackData(const CallbackData &original) : instance(original.instance), id(original.id), name(original.name) {};
		CallbackData(CallbackData &&original) : instance(original.instance), id(original.id), name(original.name) {};
		CallbackData &operator=(const CallbackData &) = delete;
		CallbackData &operator=(CallbackData &&original)
		{
			instance = std::move(original.instance);
			id = std::move(original.id);
			name = std::move(original.name);
			return *this;
		}
		~CallbackData() {};
	};
	std::mutex mSignalVectorAccessMutex;
	std::vector<CallbackData> mCallBackIdentifiers;
	GstHandlerControl aSyncControl;
	GstHandlerControl syncControl;
	GstHandlerControl callbackControl;

	bool filterAudioDemuxBuffers; /**< flag to filter audio demux buffers */
	double seekPosition;		  /**< the position to seek the pipeline to in seconds */
	GstPlayerPriv();
	~GstPlayerPriv();
};

// Class to encapsulate GStreamer-related functionality
class InterfacePlayerRDK
{
private:
	bool trickTeardown;
	std::mutex mMutex;
	std::map<std::string, int> configMap;
	PlayerScheduler mScheduler;

public:
	std::shared_ptr<SocInterface> socInterface;
	GstPlayerPriv *gstPrivateContext;
	Configs *m_gstConfigParam;
	char *mDrmSystem;
	void *mEncrypt;
	std::map<InterfaceCB, std::function<void()>> callbackMap;
	std::map<InterfaceCB, std::function<void(int)>> setupStreamCallbackMap;

	InterfacePlayerRDK();
	~InterfacePlayerRDK();

	/**
	 * @brief Idle callback for the first frame.
	 *
	 * @param[in] user_data User data passed to the callback.
	 * @return TRUE if the callback should be called again, FALSE otherwise.
	 */
	static gboolean IdleCallbackOnFirstFrame(gpointer user_data);

	/**
	 * @brief Idle callback for end of stream (EOS).
	 *
	 * @param[in] user_data User data passed to the callback.
	 * @return TRUE if the callback should be called again, FALSE otherwise.
	 */
	static gboolean IdleCallbackOnEOS(gpointer user_data);

	/**
	 * @brief Progress callback on timeout.
	 *
	 * @param[in] user_data User data passed to the callback.
	 * @return TRUE if the callback should be called again, FALSE otherwise.
	 */
	static gboolean ProgressCallbackOnTimeout(gpointer user_data);

	/**
	 * @brief General idle callback.
	 *
	 * @param[in] user_data User data passed to the callback.
	 * @return TRUE if the callback should be called again, FALSE otherwise.
	 */
	static gboolean IdleCallback(gpointer user_data);

	/**
	 * @brief Idle callback for the first video frame displayed.
	 *
	 * @param[in] user_data User data passed to the callback.
	 * @return TRUE if the callback should be called again, FALSE otherwise.
	 */
	static gboolean IdleCallbackFirstVideoFrameDisplayed(gpointer user_data);

	/*Callback function types for various GStreamer events*/
	using BusMessageCallback = std::function<void(const BusEventData)>;
	using HandleOnGstBufferUnderflowCb = std::function<void(int mediaType)>;
	using HandleOnGstDecodeErrorCb = std::function<void(int CbCount)>;
	using HandleOnGstPtsErrorCb = std::function<void(bool isVideo, bool isAudioSink)>;
	using HandleBuffering_timeoutCb = std::function<void(bool isBufferingTimeoutConditionMet, bool isRateCorrectionDefaultOnPlaying, bool isPlayerReady)>;
	using HandleRedButtonCallback = std::function<void(const char *data)>;
	using HandleNeedDataCb = std::function<void(int mediaType)>;
	using HandleEnoughDataCb = std::function<void(int mediaType)>;

	/*
	 *@brief Registers need data callback from application
	 */
	void RegisterNeedDataCb(const HandleNeedDataCb &callback)
	{
	        NeedDataCb = callback;
	}

	/*
 	*@brief Registers enough data callback from application
	 */
	void RegisterEnoughDataCb(const HandleEnoughDataCb &callback)
        {
                EnoughDataCb = callback;
        }

	/*
	 *@brief register registerHandleBuffering_timeoutCb
	 */
	void RegisterBufferingTimeoutCb(const HandleBuffering_timeoutCb &callback)
	{
		OnBuffering_timeoutCb = callback;
	}

	/*
	 *@brief register HandleOnGstPtsErrorCb
	 */
	void RegisterGstPtsErrorCb(const HandleOnGstPtsErrorCb &callback)
	{
		OnGstPtsErrorCb = callback;
	}

	/*
	 *@brief register handleOnGstDecodeErrorCb
	 */
	void RegisterGstDecodeErrorCb(const HandleOnGstDecodeErrorCb &callback)
	{
		OnGstDecodeErrorCb = callback;
	}

	/*
	 *@brief register OnGstBufferUnderflowCb
	 */
	void RegisterBufferUnderflowCb(const HandleOnGstBufferUnderflowCb &callback)
	{
		OnGstBufferUnderflowCb = callback;
	}

	/*
	 *@brief register registerHandleRedButtonCallback
	 */
	void RegisterHandleRedButtonCallback(const HandleRedButtonCallback &callback)
	{
		OnHandleRedButtonCallback = callback;
	}

	/*callback declarations*/
	BusMessageCallback busMessageCallback;
	HandleOnGstBufferUnderflowCb OnGstBufferUnderflowCb;
	HandleOnGstDecodeErrorCb OnGstDecodeErrorCb;
	HandleOnGstPtsErrorCb OnGstPtsErrorCb;
	HandleBuffering_timeoutCb OnBuffering_timeoutCb;
	HandleRedButtonCallback OnHandleRedButtonCallback;
	HandleNeedDataCb NeedDataCb;
	HandleEnoughDataCb EnoughDataCb;

	std::function<void(bool)> stopCallback;
	std::function<void(bool, int)> tearDownCb;
	std::function<void(GstMediaType, bool, bool, bool &, bool &)> notifyFirstFrameCallback;
	std::function<void(const unsigned char *, int, int, int)> gstCbExportYUVFrame;

	pthread_mutex_t mProtectionLock;
	std::string mPlayerName;
	bool mFirstFrameRequired;
	bool mPauseInjector;
	bool mResumeInjector;
	bool PipelineSetToReady;				/**< To indicate the pipeline is set to ready forcefully */
	std::mutex mSourceSetupMutex;			/**< Protects the source setup state>*/
	std::condition_variable mSourceSetupCV; /**< Conditional Variable to notify changes in the source setup status>*/
	bool mSchedulerStarted;

	/**
	 * @brief Creates a GStreamer pipeline.

	* @param pipelineName Name of the pipeline.
	* @param PipelinePriority Priority of the pipeline.

	* @return True if the pipeline is created successfully, false otherwise.
	 */
	bool CreatePipeline(const char *pipelineName, int PipelinePriority);
	/**
	 * @brief Connects a signal to a handler.
	 *
	 * @param[in] instance The instance to connect the signal to.
	 * @param[in] detailed_signal The detailed signal to connect.
	 * @param[in] c_handler The callback handler for the signal.
	 * @param[in] data User data to pass to the callback handler.
	 */
	void SignalConnect(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data);

	/**
	 * @brief Sets the player name.
	 *
	 * @param[in] name The name of the player.
	 */
	void SetPlayerName(std::string name);

	/**
	 *@brief sets the preferred drm by app
	 *@param[in] drmID preferred drm
	 */
	void SetPreferredDRM(const char *drmID);
	/**
	 * @fn SendQtDemuxOverrideEvent
	 * @param[in] mediaType stream type
	 * @param[in] pts position value of buffer
	 * @param[in] ptr buffer pointer
	 * @param[in] len length of buffer
	 * @ret TRUE if override is enabled, FALSE otherwise
	 */
	gboolean SendQtDemuxOverrideEvent(GstMediaType mediaType, GstClockTime pts, const void *ptr = nullptr, size_t len = 0);

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
	void ConfigurePipeline(int, int, int, int, bool, bool, bool, bool, int32_t, gint, const char *, int, bool, std::string url);

	/**
	 * @brief Enables or disables pausing on playback start.
	 * @param enable True to enable pausing, false to disable.
	 */
	void SetPauseOnStartPlayback(bool enable);

	/**
	 * @brief SendGstEvents
	 * @param[in] mediaType stream type
	 * @param[in] pts position value of first buffer
	 */
	void SendGstEvents(GstMediaType mediaType, GstClockTime pts);

	/**
	 * @fn ForwardAudioBuffersToAux
	 *
	 * @return bool - true if audio to be forwarded
	 */
	bool ForwardAudioBuffersToAux();

	/**
	 * @brief Flush the track playbin
	 * @param[in] pos - position to seek to after flush
	 * @param[in] pos - audio delta
	 * @param[in] pos - subtitle delta
	 */
	double FlushTrack(int mediaType, double pos, double audioDelta, double subDelta);

	/**
	 *   @fn set video zoom
	 *
	 */
	void SetVideoZoom(int zoom_mode);
	/**
	 *   @fn set video mute
	 *
	 */
	void SetVideoMute(bool muted);
	/**
	 *    @fn set text style
	 *
	 */
	bool SetTextStyle(const std::string &options);
	/**
	 *   @fn GetVideoRectangle
	 *
	 */
	std::string GetVideoRectangle();

	/**
	 * @fn SetSubtitlePtsOffset
	 * @param[in] pts_offset pts offset for subs
	 */
	void SetSubtitlePtsOffset(std::uint64_t pts_offset);

	/**
	 * @fn SetSubtitleMute
	 * @param[in] muted true to mute subtitle otherwise false
	 */
	void SetSubtitleMute(bool mute);

	/**
	 * @fn GetPositionMilliseconds
	 * @retval playback position in MS
	 */
	long long GetPositionMilliseconds(void);

	/**
	 * @fn GetVideoPlaybackQuality
	 * returns video playback quality data
	 */
	GstPlaybackQualityStruct *GetVideoPlaybackQuality(void);
	/**
	 * @fn GetDurationMilliseconds
	 * @retval playback duration in MS
	 */
	long GetDurationMilliseconds(void);

	/**
	 * @fn ResetFirstFrame
	 */
	void ResetFirstFrame(void);
	/**
	 * @fn GetVideoSize
	 * @param[out] w width video width
	 * @param[out] h height video height
	 */
	void GetVideoSize(int &width, int &height);
	/**
	 * @fn SetVideoRectangle
	 * @param[in] x x co-ordinate of display rectangle
	 * @param[in] y y co-ordinate of display rectangle
	 * @param[in] w width of display rectangle
	 * @param[in] h height of display rectangle
	 */
	void SetVideoRectangle(int x, int y, int w, int h);
	/**
	 * @brief Un-pauses the pipeline and notifies the player of the buffer end event.
	 * @param[in] forceStop True to force stop buffering.
	 * @param[out] isPlaying Indicates whether playback is ongoing.
	 * @return True if buffering was stopped successfully, false otherwise.
	 */
	bool StopBuffering(bool forceStop, bool &isPlaying);

	/**
	 * @brief Gets the CC decoder handle.
	 * @return The CC decoder handle.
	 */
	unsigned long GetCCDecoderHandle(void);

	/**
	 * @brief Waits for the source setup.
	 * @param[in] mediaType The type of media stream.
	 * @return True if the source setup was successful, false otherwise.
	 */
	bool WaitForSourceSetup(GstMediaType mediaType);

	/**
	 * @brief Send PTS/DTS to downstream elements.
	 * @param[in] type The type of event.
	 * @param[in] ptr Pointer to the event data.
	 * @param[in] len Length of the event data.
	 * @param[in] fpts First PTS value.
	 * @param[in] fdts First DTS value.
	 * @param[in] fDuration Duration of the event.
	 * @param[in] fragmentPTSoffset Offset PTS value.
	 * @param[in] copy True to copy the event data.
	 * @param[in] initFragment True if this is an initialization fragment.
	 * @param[out] discontinuity Indicates whether there is a discontinuity.
	 * @param[out] notifyFirstBufferProcessed Indicates whether the first buffer was processed.
	 * @param[out] sendNewSegmentEvent Indicates whether to send a new segment event.
	 * @param[out] resetTrickUTC Indicates whether to reset the trick UTC.
	 * @param[out] firstBufferPushed Indicates whether the first buffer was pushed.
	 * @return True if the event was sent successfully, false otherwise.
	 */
	bool SendHelper(int type, const void *ptr, size_t len, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool copy, bool initFragment, bool &discontinuity, bool &notifyFirstBufferProcessed, bool &sendNewSegmentEvent, bool &resetTrickUTC, bool &firstBufferPushed);

	/**
	 * @brief Pauses the injector.
	 */
	void PauseInjector();

	/**
	 * @brief Resumes the injector.
	 */
	void ResumeInjector();

	/**
	 * @brief Sends a new segment event.
	 * @param[in] mediaType The type of media stream.
	 * @param[in] startPts The start PTS value.
	 * @param[in] stopPts The stop PTS value.
	 */
	void SendNewSegmentEvent(GstMediaType mediaType, GstClockTime startPts, GstClockTime stopPts);

	/**
	 * @brief Handles the video buffer sent event.
	 * @return True if the video buffer was handled successfully, false otherwise.
	 */
	bool HandleVideoBufferSent();

	/**
	 * @fn QueueProtectionEvent
	 * @param[in] formatType hls/dash format
	 * @param[in] protSystemId keysystem to be used
	 * @param[in] ptr initData DRM initialization data
	 * @param[in] len initDataSize DRM initialization data size
	 * @param[in] type Media type
	 */
	void QueueProtectionEvent(const std::string &formatType, const char *protSystemId, const void *initData, size_t initDataSize, int mediaType);
	/**
	 * @brief Sets the playback rate in audio and video sink.
	 * @param[in] rate The playback rate to set.
	 * @return True if the playback rate was set successfully, false otherwise.
	 */
	bool SetPlayBackRate(double rate);

	/**
	 * @brief Sets the audio volume or mute/unmute.
	 */
	void SetVolumeOrMuteUnMute(void);

	/**
	 * @brief Sets the audio volume.
	 * @param[in] volume The volume level to set.
	 */
	void SetAudioVolume(int volume);

	/**
	 * @brief Tears down the stream.
	 * @param[in] mediaType The type of media stream.
	 */
	void TearDownStream(GstMediaType mediaType);

	/**
	 * @brief Initializes the source for the player.
	 * @param[in] PlayerInstance The player instance.
	 * @param[in] source The source to initialize.
	 * @param[in] eMEDIATYPE_VIDEO The media type for video.
	 */
	void InitializeSourceForPlayer(void *PlayerInstance, void *source, GstMediaType eMEDIATYPE_VIDEO);

	/**
	 * @brief Gets the app source element for a given media type.
	 * @param[in] mediaType The type of media stream.
	 * @return A pointer to the app source element.
	 */
	GstElement* GetAppSrc(int mediaType);

	/**
	 * @brief Sets up the stream.
	 * @param[in] streamId The ID of the stream to set up.
	 * @param[in] _this The instance of the player.
	 * @param[in] url The URL of the stream.
	 * @return An integer indicating the success or failure of the setup.
	 */
	int SetupStream(int streamId, void *_this, std::string url);

	/**
	 * @brief Callback for handling video samples in Player's GStreamer player.
	 * @param[in] object The GStreamer element.
	 * @param[in] _this The instance of the player.
	 * @return The flow return status.
	 */
	static GstFlowReturn InterfacePlayerRDK_OnVideoSample(GstElement *object, void *_this);

	/**
	 * @brief Gets the video PTS value.
	 * @return The video PTS value.
	 */
	long long GetVideoPTS(void);

	/**
	 * @brief Clears the flags for an idle task.
	 * @param[in] taskDetails The details of the task to clear flags for.
	 */
	void IdleTaskClearFlags(GstTaskControlData &taskDetails);

	/**
	 * @brief Sets the callback for the first frame.
	 * @param[in] callback The callback function to be called on the first frame.
	 */
	void FirstFrameCallback(std::function<void(GstMediaType, bool, bool, bool &, bool &)> callback);

	/**
	 * @brief Sets the callback for stopping.
	 * @param[in] callback The callback function to be called when stopping.
	 */
	void StopCallback(std::function<void(bool)> callback);

	/**
	 * @brief Sets the callback for tearing down.
	 * @param[in] callback The callback function to be called when tearing down.
	 */
	void TearDownCallback(std::function<void(bool, int)> callback);

	/**
	 * @brief Notifies the first frame.
	 * @param[in] mediaType The type of media.
	 */
	void NotifyFirstFrame(int mediaType);

	/**
	 * @brief Adds an idle task.
	 * @param[in] taskDetails The details of the task to be added.
	 * @param[in] funcPtr The function pointer for the background task.
	 * @return True if the task was added successfully, false otherwise.
	 */
	bool IdleTaskAdd(GstTaskControlData &taskDetails, BackgroundTask funcPtr);

	/**
	 * @brief Pauses GStreamer.
	 * @param[in] pause Indicates whether to pause or resume.
	 * @param[in] forceStopGstreamerPreBuffering Indicates whether to force stop GStreamer prebuffering.
	 * @return True if the operation was successful, false otherwise.
	 */
	bool Pause(bool pause, bool forceStopGstreamerPreBuffering);

	/**
	 * @brief Checks for PTS change with a timeout.
	 * @param[in] timeout The timeout value in milliseconds.
	 * @return True if a PTS change was detected within the timeout, false otherwise.
	 */
	bool CheckForPTSChangeWithTimeout(long timeout);

	/**
	 * @brief Checks if the cache is empty for a given media type.
	 * @param[in] mediaType The type of media stream.
	 * @return True if the cache is empty, false otherwise.
	 */
	bool IsCacheEmpty(int mediaType);

	/**
	 * @brief Resets the EOS signalled flag.
	 */
	void ResetEOSSignalledFlag(void);

	/**
	 * @brief Checks if the pipeline is configured for a given media type.
	 * @param[in] type The type of media stream.
	 * @return True if the pipeline is configured, false otherwise.
	 */
	bool PipelineConfiguredForMedia(int type);

	/**
	 * @brief Gets buffer control data for a given media type.
	 * @param[in] mediaType The type of media stream.
	 * @return True if the buffer control data was retrieved successfully, false otherwise.
	 */
	bool GetBufferControlData(int mediaType);

	/**
	 * @brief Checks if the stream is ready for a given media type.
	 * @param[in] mediaType The type of media stream.
	 * @return True if the stream is ready, false otherwise.
	 */
	bool IsStreamReady(int mediaType);

	/**
	 * @brief Signals a trick mode discontinuity.
	 */
	void SignalTrickModeDiscontinuity();

	/**
	 * @brief Processes discontinuity for a stream type.
	 *
	 * @param[in] mediaType The type of media stream.
	 * @param[in] streamFormat The format of the stream.
	 * @param[in] codecChange Indicates whether there is a codec change.
	 * @param[out] unblockDiscProcess Indicates whether to unblock the discontinuity process.
	 * @param[out] shouldHaltBuffering Indicates whether buffering should be halted.
	 * @return True if the discontinuity was processed successfully, false otherwise.
	 */
	bool CheckDiscontinuity(int mediaType, int streamFormat, bool codecChange, bool &unblockDiscProcess, bool &shouldHaltBuffering);

	/**
	 * @brief Sets up the stream for the interface player.
	 * @param[in] streamId The ID of the stream to set up.
	 * @param[in] url The URL of the stream.
	 * @return An integer indicating the success or failure of the setup.
	 */
	int InterfacePlayer_SetupStream(GstMediaType streamId, std::string url);

	/**
	 * @brief Triggers an event.
	 *
	 * This function triggers the specified event with the given data.
	 *
	 * @param[in] event The event to trigger.
	 * @param[in] data The data associated with the event.
	 */
	void TriggerEvent(InterfaceCB event, int data);
	/**
	 * @brief Triggers an event.
	 * @param[in] event The event to trigger.
	 */
	void TriggerEvent(InterfaceCB event);

	/**
	 * @brief Checks if a codec is supported.
	 * @param[in] codecName The name of the codec to check.
	 * @return True if the codec is supported, false otherwise.
	 */
	static bool IsCodecSupported(const std::string &codecName);

	/**
	 * @brief Disables the decoder handle notification.
	 */
	void DisableDecoderHandleNotified();

	/**
	 * @fn ForwardBuffersToAuxPipeline
	 *
	 * @param[in] buffer - input buffer to be forwarded
	 */
	void ForwardBuffersToAuxPipeline(GstBuffer *buffer);
	/*
	 *@brief register busMessageCallback
	 */
	void RegisterBusEvent(const BusMessageCallback &callback)
	{
		busMessageCallback = callback;
	}
	/**
	 * @fn ClearProtectionEvent
	 */
	void ClearProtectionEvent();

	/**
	 * @brief Sets the seek position for the GStreamer pipeline.
	 * @param[in] positionSecs The position to seek to, in seconds.
	 */
	void SetSeekPosition(double positionSecs);

	/**
	 * @brief Disconnects all signals associated with the GStreamer pipeline.
	 */
	void DisconnectSignals();

	/**
	 * @brief Removes a GStreamer timer task.
	 * @param[in,out] taskId    Reference to the ID of the task to be removed.
	 * @param[in]     timerName The name of the timer to be removed
	 */
	void TimerRemove(guint &taskId, const char *timerName);
	/**
	 * @brief get the encryption from application to share it with PlayReadyDecryptor Plugin
	 */
	void setEncryption(void *mEncrypt);

	/**
	 * @brief Removes all active GStreamer probes.
	 */
	void RemoveProbes();

	/**
	 * @fn RemoveProbe
	 * @brief Remove probe for a particular media type
	 * @param[in] mediaType The media type for which the probe should be removed
	 */
	void RemoveProbe(GstMediaType mediaType);

	/**
	 * @brief Destroys the GStreamer pipeline.
	 */
	void DestroyPipeline();

	/**
	 * @brief Stops the GStreamer pipeline.
	 *
	 * @param[in] keepLastFrame Whether to retain the last displayed frame.
	 */
	void Stop(bool keepLastFrame);

	/**
	 * @brief Sets the pending seek state.
	 * @param[in] state True to enable pending seek, false to disable.
	 */
	void SetPendingSeek(bool state);

	/**
	 *@brief returns true if using rialto sink
	 */
	bool IsUsingRialtoSink();

	/**
	 * @brief Resets GStreamer event states for all tracks.
	 * This function updates the internal states for each track,
	 * marking the position as reset, disabling pending seeks,
	 * and clearing EOS and buffer flags.
	 */
	void ResetGstEvents();

	/**
	 * @brief Retrieves the trick play teardown status
	 */
	bool GetTrickTeardown();

	/**
	 * @brief Sets the trick play teardown state.
	 * @param[in] state True to enable trick play teardown, false to disable.
	 */
	void SetTrickTearDown(bool state);

	/**
	 * @brief Flushes the GStreamer pipeline.
	 * @param[in] position The position to flush to, in seconds.
	 * @param[in] rate The playback rate.
	 * @param[in] shouldTearDown Whether to tear down the pipeline.
	 * @param[in] GstState The desired GStreamer pipeline state.
	 * @param[in] gstMediaFormat The media format for the pipeline.
	 */
	bool Flush(double position, int rate, bool shouldTearDown, bool isAppSeek);

	/**
	 * @brief Removes an idle task from the GStreamer pipeline.
	 * @param[in] taskDetails The details of the task to be removed.
	 * @return true if the task is successfully removed, false otherwise.
	 */
	bool IdleTaskRemove(GstTaskControlData &taskDetails);

	/**
	 * @fn TimerAdd
	 * @param[in] funcPtr function to execute on timer expiry
	 * @param[in] repeatTimeout timeout between calls in ms
	 * @param[in] user_data data to pass to the timer function
	 * @param[in] timerName name of the timer being added
	 * @param[out] taskId id of the timer to be returned
	 */
	void TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint &taskId, gpointer user_data, const char *timerName = nullptr);
	/**
	 * @fn TimerIsRunning
	 * @param[in] taskId id of the timer to be removed
	 * @return true - timer is currently running
	 */
	bool TimerIsRunning(guint &taskId);
	/**
	 * @fn NotifyEOS
	 */
	void NotifyEOS();
	/**
	 * @fn NotifyFragmentCachingComplete
	 */
	void NotifyFragmentCachingComplete();
	/**
	 * @fn EndOfStreamReached
	 * @param[in] type stream type
	 */
	void EndOfStreamReached(int mediaType, bool &shouldHaltBuffering);

	/**
	 * @brief Signals the subtitle clock.
	 *
	 * @param[in] vPTS The presentation timestamp.
	 * @param[in] state The state to set for the subtitle clock.
	 * @return True if the operation was successful, false otherwise.
	 */
	bool SignalSubtitleClock(gint64 vPTS, bool state);

	/**
	 * @brief Initializes Player GStreamer plugins.
	 *
	 * This function initializes the necessary plugins for Player GStreamer.
	 */
	static void InitializePlayerGstreamerPlugins();

	/**
	 * @brief Dumps diagnostic information.
	 */
	void DumpDiagnostics();

	/**
	 * @brief Enables GStreamer debug logging.
	 * @param[in] debugLevel The level of debug logging to enable.
	 */
	void EnableGstDebugLogging(std::string debugLevel);
};
struct data
{
	bool StreamReady;
	double ElapsedSeconds;
	bool GstWaitingForData;
};

#endif // INTERFACE_PLAYER_H
