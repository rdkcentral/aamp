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
#ifndef INTERFACE_PLAYER_PRIV_H
#define INTERFACE_PLAYER_PRIV_H

#include <map>
#include <cstdint>
#include <string.h>
#include <vector>
#include <mutex>
#include <PlayerLogManager.h>
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
#include <any>
#include "SocInterface.h"
#include "InterfacePlayerRDK.h"
#include "GstUtils.h"

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5
#define GST_TRACK_COUNT 4 /**< internal use - audio+video+sub+aux track */
#define VIDEO_COORDINATES_SIZE 32
#define GST_TASK_ID_INVALID 0
#define GST_NORMAL_PLAY_RATE 1
#define GST_ERROR_DESCRIPTION_LENGTH 256
#define NOW_STEADY_TS_MS std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() /**< Getting current steady clock in milliseconds */

typedef enum
{
	eGST_MEDIAFORMAT_HLS,                              /**< HLS Media */
	eGST_MEDIAFORMAT_DASH,                             /**< Dash Media */
	eGST_MEDIAFORMAT_PROGRESSIVE,              /**< Progressive Media */
	eGST_MEDIAFORMAT_HLS_MP4,                          /**< HLS mp4 Format */
	eGST_MEDIAFORMAT_OTA,                              /**< OTA Media */
	eGST_MEDIAFORMAT_HDMI,                             /**< HDMI Format */
	eGST_MEDIAFORMAT_COMPOSITE,                        /**< Composite Media */
	eGST_MEDIAFORMAT_SMOOTHSTREAMINGMEDIA, /**< Smooth streaming Media */
	eGST_MEDIAFORMAT_RMF,                              /**< RMF media */
	eGST_MEDIAFORMAT_UNKNOWN                           /**< Unknown media format */
} GstMediaFormat;

typedef enum
{
	eGST_PLAY_FLAG_VIDEO = (1 << 0),                         /**< value is 0x001 */
	eGST_PLAY_FLAG_AUDIO = (1 << 1),                         /**< value is 0x002 */
	eGST_PLAY_FLAG_TEXT = (1 << 2),                          /**< value is 0x004 */
	eGST_PLAY_FLAG_VIS = (1 << 3),                           /**< value is 0x008 */
	eGST_PLAY_FLAG_SOFT_VOLUME = (1 << 4),           /**< value is 0x010 */
	eGST_PLAY_FLAG_NATIVE_AUDIO = (1 << 5),          /**< value is 0x020 */
	eGST_PLAY_FLAG_NATIVE_VIDEO = (1 << 6),          /**< value is 0x040 */
	eGST_PLAY_FLAG_DOWNLOAD = (1 << 7),                      /**< value is 0x080 */
	eGST_PLAY_FLAG_BUFFERING = (1 << 8),             /**< value is 0x100 */
	eGST_PLAY_FLAG_DEINTERLACE = (1 << 9),           /**< value is 0x200 */
	eGST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10) /**< value is 0x400 */
} eGstPlayFlags;

enum GstVideoZoomMode
{
	GST_VIDEO_ZOOM_NONE = 0,                   /**< Video Zoom None */
	GST_VIDEO_ZOOM_DIRECT = 1,                 /**< Video Zoom Direct */
	GST_VIDEO_ZOOM_NORMAL = 2,                 /**< Video Zoom Normal */
	GST_VIDEO_ZOOM_16X9_STRETCH = 3,   /**< Video Zoom 16x9 stretch */
	GST_VIDEO_ZOOM_4x3_PILLAR_BOX = 4, /**< Video Zoom 4x3 pillar box */
	GST_VIDEO_ZOOM_FULL = 5,                   /**< Video Zoom Full */
	GST_VIDEO_ZOOM_GLOBAL = 6                  /**< Video Zoom Global */
};

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
	eGST_STATE_IDLE,                 /**< 0  - Player is idle */
	eGST_STATE_INITIALIZING, /**< 1  - Player is initializing a particular content */
	eGST_STATE_INITIALIZED,  /**< 2  - Player has initialized for a content successfully */
	eGST_STATE_PREPARING,    /**< 3  - Player is loading all associated resources */
	eGST_STATE_PREPARED,     /**< 4  - Player has loaded all associated resources successfully */
	eGST_STATE_BUFFERING,    /**< 5  - Player is in buffering state */
	eGST_STATE_PAUSED,               /**< 6  - Playback is paused */
	eGST_STATE_SEEKING,              /**< 7  - Seek is in progress */
	eGST_STATE_PLAYING,              /**< 8  - Playback is in progress */
	eGST_STATE_STOPPING,     /**< 9  - Player is stopping the playback */
	eGST_STATE_STOPPED,              /**< 10 - Player has stopped playback successfully */
	eGST_STATE_COMPLETE,     /**< 11 - Playback completed */
	eGST_STATE_ERROR,                /**< 12 - Error encountered and playback stopped */
	eGST_STATE_RELEASED,     /**< 13 - Player has released all resources for playback */
	eGST_STATE_BLOCKED               /**< 14 - Player has blocked and cant play content*/
} GstPrivPlayerState;

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
	GstElement *sinkbin;              /**< Sink element to consume data */
	GstElement *source;                       /**< to provide data to the pipleline */
	GstStreamOutputFormat format; /**< Stream output format for this stream */
	bool pendingSeek;                         /**< Flag denotes if a seek event has to be sent to the source */
	bool resetPosition;                       /**< To indicate that the position of the stream is reset */
	bool bufferUnderrun;
	bool eosReached;           /**< To indicate the status of End of Stream reached */
	bool sourceConfigured; /**< To indicate that the current source is initialized and configured */
	pthread_mutex_t sourceLock;
	uint32_t timeScale;
	int32_t trackId;                   /**< Current Audio Track Id,so far it is implemented for AC4 track selection only */
	bool firstBufferProcessed; /**< Indicates if the first buffer is processed in this stream */
	GstPad *demuxPad;                  /**< Demux src pad >*/
	gulong demuxProbeId;       /**< Demux pad probe ID >*/

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
#if 0
struct MonitorAVState
{
	long long tLastReported;
	long long tLastSampled;
	const char *description;
	gint64 av_position[2];
	bool happy;

	MonitorAVState() : tLastReported(0), tLastSampled(0), description(nullptr), happy(false)
	{
		av_position[0] = 0; // Video position
		av_position[1] = 0; // Audio position
	}
};
#endif
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
	GstBus *bus;              /**< Bus for receiving GstEvents from pipeline. */
	guint64 total_bytes;
	gint n_audio;                            /**< Number of audio tracks. */
	gint current_audio;                      /**< Offset of current audio track. */
	std::mutex TaskControlMutex; /**< For scheduling/de-scheduling or resetting async tasks/variables and timers */
	GstTaskControlData firstProgressCallbackIdleTask;
	guint periodicProgressCallbackIdleTaskId; /**< ID of timed handler created for notifying progress events. */
	guint bufferingTimeoutTimerId;                    /**< ID of timer handler created for buffering timeout. */
	GstElement *video_dec;                                    /**< Video decoder used by pipeline. */
	GstElement *audio_dec;                                    /**< Audio decoder used by pipeline. */
	GstElement *video_sink;                                   /**< Video sink used by pipeline. */
	GstElement *audio_sink;                                   /**< Audio sink used by pipeline. */
	GstElement *subtitle_sink;                                /**< Subtitle sink used by pipeline. */
	GstTaskPool *task_pool;                                   /**< Task pool in case RT priority is needed. */

	int rate;                                                                                        /**< Current playback rate. */
	GstVideoZoomMode zoom;                                                           /**< Video-zoom setting. */
	bool videoMuted;                                                                         /**< Video mute status. */
	bool audioMuted;                                                                         /**< Audio mute status. */
	std::mutex volumeMuteMutex;                                                      /**< Mutex to ensure setVolumeOrMuteUnMute is thread-safe. */
	bool subtitleMuted;                                                                      /**< Subtitle mute status. */
	double audioVolume;                                                                      /**< Audio volume. */
	guint eosCallbackIdleTaskId;                                             /**< ID of idle handler created for notifying EOS event. */
	std::atomic<bool> eosCallbackIdleTaskPending;            /**< Set if any eos callback is pending. */
	bool firstFrameReceived;                                                         /**< Flag that denotes if first frame was notified. */
	bool pendingPlayState;                                                           /**< Flag that denotes if set pipeline to PLAYING state is pending. */
	bool decoderHandleNotified;                                                      /**< Flag that denotes if decoder handle was notified. */
	guint firstFrameCallbackIdleTaskId;                                      /**< ID of idle handler created for notifying first frame event. */
	GstEvent *protectionEvent[GST_TRACK_COUNT];                      /**< GstEvent holding the pssi data to be sent downstream. */
	std::atomic<bool> firstFrameCallbackIdleTaskPending; /**< Set if any first frame callback is pending. */
	bool using_westerossink;                                                         /**< true if westeros sink is used as video sink */
	bool usingRialtoSink;                                                            /**< true if rialto sink is used for video and audio sinks */
	char videoRectangle[VIDEO_COORDINATES_SIZE];
	bool pauseOnStartPlayback;                                                               /**< true if should start playback paused */
	std::atomic<bool> eosSignalled;                                                  /**< Indicates if EOS has signaled */
	gboolean buffering_enabled;                                                              /**< enable buffering based on multiqueue */
	gboolean buffering_in_progress;                                                  /**< buffering is in progress */
	guint buffering_timeout_cnt;                                                     /**< make sure buffering_timeout doesn't get stuck */
	GstState buffering_target_state;                                                 /**< the target state after buffering */
	gint64 lastKnownPTS;                                                                     /**< To store the PTS of last displayed video */
	long long ptsUpdatedTimeMS;                                                              /**< Timestamp when PTS was last updated */
	guint ptsCheckForEosOnUnderflowIdleTaskId;                               /**< ID of task to ensure video PTS is not moving before notifying EOS on underflow. */
	int numberOfVideoBuffersSent;                                                    /**< Number of video buffers sent to pipeline */
	gint64 segmentStart;                                                                     /**< segment start value; required when qtdemux is enabled or restamping is disabled; -1 to send a segment.start query to gstreamer */
	GstQuery *positionQuery;                                                                 /**< pointer that holds a position query object */
	GstQuery *durationQuery;                                                                 /**< pointer that holds a duration query object */
	bool paused;                                                                                     /**< if pipeline is deliberately put in PAUSED state due to user interaction */
	GstState pipelineState;                                                                  /**< current state of pipeline */
	GstTaskControlData firstVideoFrameDisplayedCallbackTask; /**< Task control data of the handler created for notifying state changed to Playing */
	bool firstTuneWithWesterosSinkOff;                                               /**< track if first tune was done for Realtekce build */
	long long decodeErrorMsgTimeMS;                                                  /**< Timestamp when decode error message last posted */
	int decodeErrorCBCount;                                                                  /**< Total decode error cb received within threshold time */
	bool progressiveBufferingEnabled;
	bool progressiveBufferingStatus;
	bool forwardAudioBuffers;                                 /**< flag denotes if audio buffers to be forwarded to aux pipeline */
	bool enableSEITimeCode;                                   /**< Enables SEI Time Code handling */
	bool firstVideoFrameReceived;                     /**< flag that denotes if first video frame was notified. */
	bool firstAudioFrameReceived;                     /**< flag that denotes if first audio frame was notified */
	int NumberOfTracks;                                               /**< Indicates the number of tracks */
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
	double seekPosition;              /**< the position to seek the pipeline to in seconds */
	GstPlayerPriv();
	~GstPlayerPriv();
};

class InterfacePlayerPriv
{
	private:
	protected:
	public:
		InterfacePlayerPriv();
		~InterfacePlayerPriv();
		GstPlayerPriv *gstPrivateContext;
		std::shared_ptr<SocInterface> socInterface;
		std::string mPlayerName;

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
		 * @brief SendGstEvents
		 * @param[in] mediaType stream type
		 * @param[in] pts position value of first buffer
		 */
		void SendGstEvents(int mediaType, GstClockTime pts, int enableGstPosQuery, bool enablePTSReStamp, int vodTrickModeFPS);

		/**
		 * @brief Sends a new segment event.
		 * @param[in] mediaType The type of media stream.
		 * @param[in] startPts The start PTS value.
		 * @param[in] stopPts The stop PTS value.
		 */
		void SendNewSegmentEvent(int mediaType, GstClockTime startPts, GstClockTime stopPts);

		/**
		 * @fn SendQtDemuxOverrideEvent
		 * @param[in] mediaType stream type
		 * @param[in] pts position value of buffer
		 * @param[in] ptr buffer pointer
		 * @param[in] len length of buffer
		 * @ret TRUE if override is enabled, FALSE otherwise
		 */
		gboolean SendQtDemuxOverrideEvent(int mediaType, GstClockTime pts, bool enablePTSReStamp, int vodTrickModeFPS, const void *ptr = nullptr, size_t len = 0);

		/**
		 * @fn ForwardBuffersToAuxPipeline
		 *
		 * @param[in] buffer - input buffer to be forwarded
		 */
		void ForwardBuffersToAuxPipeline(GstBuffer *buffer, bool pauseInjector, void *user_data);

};
#endif
