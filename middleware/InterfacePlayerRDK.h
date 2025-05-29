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
#include "PlayerScheduler.h"
#include <atomic>
#include <set>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <chrono>
#include <any>
#include "SocUtils.h"

class InterfacePlayerPriv;

/**
 * @brief Function pointer for the idle task
 * @param[in] arg - Arguments
 * @return Idle task status
 */
typedef int (*BackgroundTask)(void *arg);

struct GstTaskControlData
{
        guint taskID;
        bool taskIsPending;
        std::string taskName;
        GstTaskControlData(const char *taskIdent) : taskID(0), taskIsPending(false), taskName(taskIdent ? taskIdent : "undefined") {};
};

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

typedef struct GstPlaybackQualityData
{
	long long rendered;
	long long dropped;
} GstPlaybackQualityStruct;

enum class InterfaceCB
{
	firstVideoFrameDisplayed,
	idleCb,
	progressCb,
	firstVideoFrameReceived,
	notifyEOS,
	startNewSubtitleStream // Add more events here if needed
};

// Class to encapsulate GStreamer-related functionality
class InterfacePlayerRDK
{
	private:
		bool trickTeardown;
		std::mutex mMutex;
		std::map<std::string, int> configMap;

	public:
		Configs *m_gstConfigParam;
		char *mDrmSystem;
		void *mEncrypt;
		void *mDRMSessionManager;
		std::map<InterfaceCB, std::function<void()>> callbackMap;
		std::map<InterfaceCB, std::function<void(int)>> setupStreamCallbackMap;
		PlayerScheduler mScheduler;

		InterfacePlayerRDK();
		~InterfacePlayerRDK();

		InterfacePlayerPriv* GetPrivatePlayer();

		/**
		 * @brief Clears the flags for an idle task.
		 * @param[in] taskDetails The details of the task to clear flags for.
		 */
		void IdleTaskClearFlags(GstTaskControlData &taskDetails);

		/**
		 * @brief Removes an idle task from the GStreamer pipeline.
		 * @param[in] taskDetails The details of the task to be removed.
		 * @return true if the task is successfully removed, false otherwise.
		 */
		bool IdleTaskRemove(GstTaskControlData &taskDetails);

		/**
		 * @brief Adds an idle task.
		 * @param[in] taskDetails The details of the task to be added.
		 * @param[in] funcPtr The function pointer for the background task.
		 * @return True if the task was added successfully, false otherwise.
		 */
		bool IdleTaskAdd(GstTaskControlData &taskDetails, BackgroundTask funcPtr);


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

		/**
		 * @brief Checks if the pipeline is currently in paused state
		 * @return true if pipeline is paused, false otherwise
		 */
		bool IsPipelinePaused();

		/**
		 * @brief Sets a flag indicating that pipeline transition to PLAYING state is pending
		 */
		void EnablePendingPlayState();
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
		std::function<void(int, bool, bool, bool &, bool &)> notifyFirstFrameCallback;
		std::function<void(const unsigned char *, int, int, int)> gstCbExportYUVFrame;

		pthread_mutex_t mProtectionLock;
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
		bool WaitForSourceSetup(int mediaType);

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
		void TearDownStream(int mediaType);

		/**
		 * @brief Initializes the source for the player.
		 * @param[in] PlayerInstance The player instance.
		 * @param[in] source The source to initialize.
		 * @param[in] eMEDIATYPE_VIDEO The media type for video.
		 */
		void InitializeSourceForPlayer(void *PlayerInstance, void *source, int mediaType);

		/**
		 * @brief Sets up the stream.
		 * @param[in] streamId The ID of the stream to set up.
		 * @param[in] _this The instance of the player.
		 * @param[in] url The URL of the stream.
		 * @return An integer indicating the success or failure of the setup.
		 */
		int SetupStream(int streamId, void *_this, std::string url);

		/**
		 * @brief Gets the video PTS value.
		 * @return The video PTS value.
		 */
		long long GetVideoPTS(void);

		/**
		 * @brief Sets the callback for the first frame.
		 * @param[in] callback The callback function to be called on the first frame.
		 */
		void FirstFrameCallback(std::function<void(int, bool, bool, bool &, bool &)> callback);

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
		 * @brief Pauses GStreamer.
		 * @param[in] pause Indicates whether to pause or resume.
		 * @param[in] forceStopGstreamerPreBuffering Indicates whether to force stop GStreamer pre-buffering.
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
		int InterfacePlayer_SetupStream(int streamId, std::string url);

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
		 * @fn to check LiveLatencyCorrectionSupported or not
		 *
		 */
		static bool IsLiveLatencyCorrectionSupported();

		/**
		 * @brief Disables the decoder handle notification.
		 */
		void DisableDecoderHandleNotified();

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
		void setEncryption(void *mEncrypt, void *mDRMSessionManager);

		/**
		 * @brief Removes all active GStreamer probes.
		 */
		void RemoveProbes();

		/**
		 * @fn RemoveProbe
		 * @brief Remove probe for a particular media type
		 * @param[in] mediaType The media type for which the probe should be removed
		 */
		void RemoveProbe(int mediaType);

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

	private:
		InterfacePlayerPriv *interfacePlayerPriv;

};

struct data
{
	bool StreamReady;
	double ElapsedSeconds;
	bool GstWaitingForData;
};

#endif // INTERFACE_PLAYER_H
