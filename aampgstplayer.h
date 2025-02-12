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
 * @file aampgstplayer.h
 * @brief Gstreamer based player for AAMP
 */

#ifndef AAMPGSTPLAYER_H
#define AAMPGSTPLAYER_H

#include "priv_aamp.h"
#include "ID3Metadata.hpp"

#include <stddef.h>
#include <functional>
#include <gst/gst.h>
#include <pthread.h>

/**
 * @struct AAMPGstPlayerPriv
 * @brief forward declaration of AAMPGstPlayerPriv
 */
struct AAMPGstPlayerPriv;

/**
 * 
*/
class SegmentInfo_t;

/**
 * @struct TaskControlData
 * @brief data for scheduling and handling asynchronous tasks
 */
struct TaskControlData
{
	guint       taskID;
	bool        taskIsPending;
	std::string taskName;
	TaskControlData(const char* taskIdent) : taskID(0), taskIsPending(false), taskName(taskIdent ? taskIdent : "undefined") {};
};

/**
 * @brief Function pointer for the idle task
 * @param[in] arg - Arguments
 * @return Idle task status
 */
typedef int (*BackgroundTask)(void *arg);

/**
 * @struct BufferControlData
 * @brief data needed for buffer control purposes
 */
struct BufferControlData
{
	bool StreamReady = false;
	double ElapsedSeconds = 0;
	bool GstWaitingForData = false;
};

/**
 * @class AAMPGstPlayer
 * @brief Class declaration of Gstreamer based player
 */
class AAMPGstPlayer : public StreamSink
{
private:
	/**
         * @fn SendHelper
         * @param[in] mediaType stream type
         * @param[in] ptr buffer pointer
         * @param[in] len length of buffer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] duration duration of buffer (in sec)
         * @param[in] copy to map or transfer the buffer
         * @param[in] initFragment flag for buffer type (init, data)
         */
	bool SendHelper(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration, bool copy, bool initFragment = 0, bool discontinuity = false);

	/**
	 * @fn SendGstEvents
	 * @param[in] mediaType stream type
	 * @param[in] pts position value of first buffer
	 */
	void SendGstEvents(AampMediaType mediaType, GstClockTime pts);

	/**
         * @fn SendNewSegmentEvent
         * @param[in] mediaType stream type
         * @param[in] startPts Start Position of first buffer
         * @param[in] stopPts Stop position of last buffer
         */
	void SendNewSegmentEvent(AampMediaType mediaType, GstClockTime startPts ,GstClockTime stopPts = 0);

	/**
	 * @fn SendQtDemuxOverrideEvent
	 * @param[in] mediaType stream type
	 * @param[in] pts position value of buffer
	 * @param[in] ptr buffer pointer
	 * @param[in] len length of buffer
	 * @ret TRUE if override is enabled, FALSE otherwise
	 */
	gboolean SendQtDemuxOverrideEvent(AampMediaType mediaType, GstClockTime pts, const void *ptr = nullptr, size_t len = 0);

public:

	class PrivateInstanceAAMP *aamp;
	class PrivateInstanceAAMP *mEncryptedAamp;
	/**
         * @fn Configure
         * @param[in] format video format
         * @param[in] audioFormat audio format
         * @param[in] auxFormat aux audio format
         * @param[in] subFormat subtitle format
         * @param[in] bESChangeStatus flag to indicate if the audio type changed in mid stream
         * @param[in] forwardAudioToAux if audio buffers to be forwarded to aux pipeline
         * @param[in] setReadyAfterPipelineCreation True/False for pipeline is created
         */
	void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation=false) override;
	/**
         * @fn SendCopy
         * @param[in] mediaType stream type
         * @param[in] ptr buffer pointer
         * @param[in] len length of buffer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] fDuration duration of buffer (in sec)
         */
	bool SendCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration) override;
	/**
         * @fn SendTransfer
         * @param[in] mediaType stream type
         * @param[in] buffer buffer as AampGrowableBuffer pointer
         * @param[in] fpts PTS of buffer (in sec)
         * @param[in] fdts DTS of buffer (in sec)
         * @param[in] fDuration duration of buffer (in sec)
         * @param[in] initFragment flag for buffer type (init, data)
         */
	bool SendTransfer(AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double fDuration, bool initFragment = false, bool discontinuity = false) override;
	/**
         * @fn PipelineConfiguredForMedia
         * @param[in] type stream type
         */
	bool PipelineConfiguredForMedia(AampMediaType type) override;
	/**
         * @fn EndOfStreamReached
         * @param[in] type stream type
         */
	void EndOfStreamReached(AampMediaType type) override;
	/**
         * @fn Stream
         */
	void Stream(void) override;

	/**
         * @fn Stop
         * @param[in] keepLastFrame denotes if last video frame should be kept
         */
	void Stop(bool keepLastFrame) override;
	/**
         * @fn Flush
         * @param[in] position playback seek position
         * @param[in] rate playback rate
         * @param[in] shouldTearDown flag indicates if pipeline should be destroyed if in invalid state
         */
	void Flush(double position, int rate, bool shouldTearDown) override;
	/**
         * @fn Pause
         * @param[in] pause flag to pause/play the pipeline
         * @param[in] forceStopGstreamerPreBuffering - true for disabling buffer-in-progress
         * @retval true if content successfully paused
         */
	bool Pause(bool pause, bool forceStopGstreamerPreBuffering) override;
	/**
         * @fn GetPositionMilliseconds
         * @retval playback position in MS
         */
	long long GetPositionMilliseconds(void) override;
	/**
         * @fn GetDurationMilliseconds
         * @retval playback duration in MS
         */
	long GetDurationMilliseconds(void) override;
	/**
         * @fn getCCDecoderHandle
         * @retval the decoder handle
         */
	unsigned long getCCDecoderHandle(void) override;
	/**
         * @fn GetVideoPTS
         * @retval Video PTS value
         */
	virtual long long GetVideoPTS(void) override;
	/**
         * @fn SetVideoRectangle
         * @param[in] x x co-ordinate of display rectangle
         * @param[in] y y co-ordinate of display rectangle
         * @param[in] w width of display rectangle
         * @param[in] h height of display rectangle
         */
	void SetVideoRectangle(int x, int y, int w, int h) override;
	/**
         * @fn Discontinuity
         * @param mediaType Media stream type
         * @retval true if discontinuity processed
         */
	bool Discontinuity( AampMediaType mediaType) override;
	/**
         * @fn SetVideoZoom
         * @param[in] zoom zoom setting to be set
         */
	void SetVideoZoom(VideoZoomMode zoom) override;
	/**
         * @fn ResetFirstFrame
         */
	void ResetFirstFrame(void);
	/**
         * @fn SetVideoMute
         * @param[in] muted true to mute video otherwise false
         */
	void SetVideoMute(bool muted) override;
	/**
         * @fn SetAudioVolume
         * @param[in] volume audio volume value (0-100)
         */
	void SetAudioVolume(int volume) override;
	/**
         * @fn SetSubtitleMute
         * @param[in] muted true to mute subtitle otherwise false
         */
	void SetSubtitleMute(bool mute) override;
	/**
         * @fn SetSubtitlePtsOffset
         * @param[in] pts_offset pts offset for subs
         */
	void SetSubtitlePtsOffset(std::uint64_t pts_offset) override;
	/**
         * @fn setVolumeOrMuteUnMute
         * @note set privateContext->audioVolume before calling this function
         */
	void setVolumeOrMuteUnMute(void);
	/**
         * @fn IsCacheEmpty
         * @param[in] mediaType stream type
         * @retval true if cache empty
         */
	bool IsCacheEmpty(AampMediaType mediaType) override;
	/**
         * @fn ResetEOSSignalledFlag
         */
	void ResetEOSSignalledFlag() override;
	/**
         * @fn CheckForPTSChangeWithTimeout
         *
         * @param[in] timeout - to check if PTS hasn't changed within a time duration
         */
	bool CheckForPTSChangeWithTimeout(long timeout) override;
	/**
         * @fn NotifyFragmentCachingComplete
         */
	void NotifyFragmentCachingComplete() override;
	/**
         * @fn NotifyFragmentCachingOngoing
         */
	void NotifyFragmentCachingOngoing() override;
	/**
         * @fn GetVideoSize
         * @param[out] w width video width
         * @param[out] h height video height
         */
	void GetVideoSize(int &w, int &h) override;
	/**
         * @fn QueueProtectionEvent
         * @param[in] protSystemId keysystem to be used
         * @param[in] ptr initData DRM initialization data
         * @param[in] len initDataSize DRM initialization data size
         * @param[in] type Media type
         */
	void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, AampMediaType type) override;
	/**
         * @fn ClearProtectionEvent
         */
	void ClearProtectionEvent() override;
	/**
         * @fn IdleTaskAdd
         * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
         * @param[in] funcPtr function pointer to add to the asynchronous queue task
         * @return true - if task was added
         */
	bool IdleTaskAdd(TaskControlData& taskDetails, BackgroundTask funcPtr);
	/**
         * @fn IdleTaskRemove
         * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
         * @return true - if task was removed
         */
	bool IdleTaskRemove(TaskControlData& taskDetails);
	/**
         * @fn IdleTaskClearFlags
         * @param[in] taskDetails task control data (e.g. id, pending flag and task name)
         */
	void IdleTaskClearFlags(TaskControlData& taskDetails);
	/**
         * @fn TimerAdd
         * @param[in] funcPtr function to execute on timer expiry
         * @param[in] repeatTimeout timeout between calls in ms
         * @param[in] user_data data to pass to the timer function
         * @param[in] timerName name of the timer being added
         * @param[out] taskId id of the timer to be returned
         */
	void TimerAdd(GSourceFunc funcPtr, int repeatTimeout, guint& taskId, gpointer user_data, const char* timerName = nullptr);
	/**
         * @fn TimerRemove
         * @param[in] taskId id of the timer to be removed
         * @param[in] timerName name of the timer being removed (for debug) (opt)
         */
	void TimerRemove(guint& taskId, const char* timerName = nullptr);
	/**
         * @fn TimerIsRunning
         * @param[in] taskId id of the timer to be removed
         * @return true - timer is currently running
         */
	bool TimerIsRunning(guint& taskId);
	/**
         * @fn StopBuffering
         *
         * @param[in] forceStop - true to force end buffering
         */
	void StopBuffering(bool forceStop) override;

	/**
	 * @fn SetPlayBackRate
	 * @param[in] rate playback rate
	 * @return true if playrate adjusted
	 */
	bool SetPlayBackRate ( double rate ) override;

	bool PipelineSetToReady; /**< To indicate the pipeline is set to ready forcefully */
	bool trickTeardown;		/**< To indicate that the tear down is initiated in trick play */
	struct AAMPGstPlayerPriv *privateContext;
	
	/**
	 * Constructor
	 * 
	 * @param[in] aamp Pointer to parent aamp instance
	 * @param[in] id3HandlerCallback Function to call to generate the JS event for in ID3 packet 
	 */
	AAMPGstPlayer(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames = nullptr);
	AAMPGstPlayer(const AAMPGstPlayer&) = delete;
	AAMPGstPlayer& operator=(const AAMPGstPlayer&) = delete;
	/**
	* @fn ~AAMPGstPlayer
	*/
	~AAMPGstPlayer();
	/**
	 * @fn InitializeAAMPGstreamerPlugins
	 */
	static void InitializeAAMPGstreamerPlugins();
	/**
	 * @fn InferPlatformFromPluginScan
	 */
	static PlatformType InferPlatformFromPluginScan();
	/**
	 * @fn NotifyEOS
	 */
	void NotifyEOS();
	/**
	 * @fn NotifyFirstFrame
	 * @param[in] type media type of the frame which is decoded, either audio or video.
	 */
	void NotifyFirstFrame(AampMediaType type);
	/**
     	 * @fn DumpDiagnostics
    	 *
     	 */
	void DumpDiagnostics();
	/**
     	 *   @fn SignalTrickModeDiscontinuity
     	 *   @return void
     	 */
	void SignalTrickModeDiscontinuity() override;
	
	std::function< void(const unsigned char *, int, int, int) > cbExportYUVFrame;
	/**
     	 * @fn AAMPGstPlayer_OnVideoSample
     	 * @param[in] object - pointer to appsink instance triggering "new-sample" signal
     	 * @param[in] _this  - pointer to AAMPGstPlayer instance
     	 * @retval GST_FLOW_OK
     	 */
	static GstFlowReturn AAMPGstPlayer_OnVideoSample(GstElement* object, AAMPGstPlayer * _this);

    	/**
     	 * @fn SeekStreamSink
     	 * @param position playback seek position
	 * @param rate play rate  
     	 */
	void SeekStreamSink(double position, double rate) override;
	
	/**
     	 * @fn static IsCodecSupported
    	 * @param[in] codecName - name of the codec value
     	 */
	static bool IsCodecSupported(const std::string &codecName);
	
	/**
     	 *   @fn GetVideoRectangle
    	 *
     	 */
	std::string GetVideoRectangle() override;
	
	/**
         * @fn to check MS2V12Supported or not
         *
         */
    static bool IsMS2V12Supported();
	/**
        *   @fn SignalConnect
	    *   @note wraps g_signal_connect, adds functionality required by DisconnectSignals()
	 */
	void SignalConnect(gpointer instance, const gchar *detailed_signal, GCallback c_handler, gpointer data);

	/**
		* @brief Set the text style of the subtitle to the options passed
		* @fn SetTextStyle()
		* @param[in] options - reference to the Json string that contains the information
		* @return - true indicating successful operation in passing options to the parser
	 */
	bool SetTextStyle(const std::string &options) override;

	/**
	 * @fn GetVideoPlaybackQuality
	 * returns video playback quality data
	 */
	PlaybackQualityStruct* GetVideoPlaybackQuality(void) override;

	/**
	 * @fn FlushTrack
	 */
	void FlushTrack(AampMediaType mediaType,double pos) override;

	/**
	 * @fn ChangeAamp
	 * @brief Change the instance of PrivateInstanceAAMP that is using the gstreamer pipeline,
	 * when it is being used as a single pipeline shared among multiple instances of PrivateInstanceAAMP
   	 * @param[in] newAamp - pointer to new instance of PrivateInstanceAAMP
	 * @param[in] id3HandlerCallback - the id3 callback handle associated with this instance of PrivateInstanceAAMP
	 */
	void ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback);

	/**
	 * @fn IsAssociatedAamp
	 * @brief Check if the specified player is associated with the pipeline
   	 * @param[in] aampInstance - pointer to new instance of PrivateInstanceAAMP
	 */
	bool IsAssociatedAamp(PrivateInstanceAAMP *aampInstance);

	/**
	 * @fn SetEncryptedAamp
   	 * @param[in] aamp - Pointer to the instance of PrivateInstanceAAMP that has the encrypted content
	 */
	void SetEncryptedAamp(PrivateInstanceAAMP *aamp);

	/**
	 * @fn SignalSubtitleClock
	 * @brief Signal the new clock to subtitle module
	 * @return - true indicating successful operation in sending the clock update
	 */
	bool SignalSubtitleClock() override;

/**
	 * @fn GetBufferControlData
	 * @brief Gets the data needed for buffer control purposes
	 * @param[in] mediaType - Media stream type
	 * @param[out] data - Data needed for buffer control
	 */
	void GetBufferControlData(AampMediaType mediaType, BufferControlData &data) const;

	/**
	 * @fn SetPauseOnPlayback
	 * @brief Set to pause on next playback start
	 * @param[in] enable - Flag to set whether enabled
	 */
	void SetPauseOnStartPlayback(bool enable) override;

private:
	/**
     	 * @fn TearDownStream
     	 * @param[in] mediaType stream type
     	 */
	void TearDownStream(AampMediaType mediaType);
	/**
     	 * @fn CreatePipeline
     	 */
	bool CreatePipeline();
	/**
     	 * @fn DestroyPipeline
     	 */
	void DestroyPipeline();
	static bool initialized;
	/**
     	 * @fn WaitForSourceSetup
     	 *
     	 * @param[in] mediaType - source element for media type
     	 * @return bool - true if source setup completed within timeout
     	 */
	bool WaitForSourceSetup(AampMediaType mediaType);
	/**
     	 * @fn ForwardBuffersToAuxPipeline
    	 *
     	 * @param[in] buffer - input buffer to be forwarded
     	 */
	void ForwardBuffersToAuxPipeline(GstBuffer *buffer);
	/**
     	 * @fn ForwardAudioBuffersToAux
     	 *
     	 * @return bool - true if audio to be forwarded
     	 */
	bool ForwardAudioBuffersToAux();
	/**
         *   @fn DisconnectSignals
	     *   @brief runs g_signal_handler_disconnect() for signals connected using SignalConnect() since
	     *  this function was last called unless
	     *  these were subsequently removed using RemoveSignalsFromDisconnectList()
	     */
	void DisconnectSignals();

	/**
	     *   @fn RemoveSignalsFromDisconnectList
	     *   @brief Prevents signals associated with the supplied element from being disconnected by DisconnectSignals
	     */
	void RemoveSignalsFromDisconnectList(const gpointer pElementOrBin);
	
	/**
	 * @fn RemoveProbes
	 * @brief Remove probes from the pipeline
	 */
	void RemoveProbes();

	/**
	 * @fn SetSeekPosition
	 * @param[in] positionSecs - the start position to seek the pipeline to in seconds
	 */
	void SetSeekPosition(double positionSecs);

	pthread_mutex_t mBufferingLock;
	pthread_mutex_t mProtectionLock;

	id3_callback_t m_ID3MetadataHandler; /**< Function to call to generate the JS event for in ID3 packet */
};

#endif // AAMPGSTPLAYER_H
