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

#ifndef STREAM_SINK_H
#define STREAM_SINK_H

#include "StreamOutputFormat.h"
#include "AampMediaType.h"

/**
 * @struct PlaybackQualityData
 * @brief Playback quality data information
 */
typedef struct PlaybackQualityData
{
    long long rendered;
    long long dropped;
} PlaybackQualityStruct;

/**
 * @class StreamSink
 * @brief GStreamer Abstraction class for the implementation of AAMPGstPlayer and gstaamp plugin
 */
class StreamSink
{
public:

    /**
     *   @brief  Configure output formats
     *
     *   @param[in]  format - Video output format.
     *   @param[in]  audioFormat - Audio output format.
     *   @param[in]  auxFormat - Aux audio output format.
     *   @param[in]  bESChangeStatus - Flag to keep force configure the pipeline value
     *   @param[in]  forwardAudioToAux - Flag denotes if audio buffers have to be forwarded to aux pipeline
     *   @param[in]  setReadyAfterPipelineCreation - Flag denotes if pipeline has to be reset to ready or not
     *   @return void
     */
    virtual void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation=false){}

    /**
     *   @brief  API to send audio/video buffer into the sink.
     *
     *   @param[in]  mediaType - Type of the media.
     *   @param[in]  ptr - Pointer to the buffer; caller responsible of freeing memory
     *   @param[in]  len - Buffer length.
     *   @param[in]  fpts - Presentation Time Stamp.
     *   @param[in]  fdts - Decode Time Stamp
     *   @param[in]  fDuration - Buffer duration.
     *   @return void
     */
    virtual bool SendCopy( AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)= 0;

    /**
     *   @brief  API to send audio/video buffer into the sink.
     *
     *   @param[in]  mediaType - Type of the media.
     *   @param[in]  buffer - Pointer to the AampGrowableBuffer; ownership is taken by the sink
     *   @param[in]  fpts - Presentation Time Stamp.
     *   @param[in]  fdts - Decode Time Stamp
     *   @param[in]  fDuration - Buffer duration.
     *   @param[in]  fragmentPTSoffset - Offset PTS
     *   @param[in]  initFragment - flag for buffer type (init, data)
     *   @return void
     */
    virtual bool SendTransfer( AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment = false, bool discontinuity = false)= 0;

    /**
     *   @brief  Checks pipeline is configured for media type
     *
     *   @param[in]  mediaType - Media Type
     *   @return void
     */
    virtual bool PipelineConfiguredForMedia(AampMediaType type){return true;}

    /**
     *   @brief  Notifies EOS to sink
     *
     *   @param[in]  mediaType - Media Type
     *   @return void
     */
    virtual void EndOfStreamReached(AampMediaType mediaType){}

    /**
     *   @brief Start the stream
     *
     *   @return void
     */
    virtual void Stream(void){}

    /**
     *   @fn Stop
     *
     *   @param[in]  keepLastFrame - Keep the last frame on screen (true/false)
     *   @return void
     */
    virtual void Stop(bool keepLastFrame){}

    /**
     *   @brief Flush the pipeline
     *
     *   @param[in]  position - playback position
     *   @param[in]  rate - Speed
     *   @param[in]  shouldTearDown - if pipeline is not in a valid state, tear down pipeline
     *   @return void
     */
    virtual void Flush(double position = 0, int rate = AAMP_NORMAL_PLAY_RATE, bool shouldTearDown = true){}

    /**
     *   @brief Flush the audio playbin
     *   @param[in]  position - playback position
     *   @return void
     */
    virtual void FlushTrack(AampMediaType mediaType,double position = 0){}

    /**
     *   @brief Set player rate to audio/video sink
     *
     *   @param[in]  rate - Speed
     *   @return true if player rate is set successfully
     */
    virtual bool SetPlayBackRate ( double rate ){return true;}
    /**
     *   @brief Adjust the pipeline
     *
     *   @param[in]  position - playback position
     *   @param[in]  rate - Speed
     *   @return void
     */
    virtual bool AdjustPlayBackRate(double position, double rate){ return true; }

    /**
     *   @brief Enabled or disable playback pause
     *
     *   @param[in] pause  Enable/Disable
     *   @param[in] forceStopGstreamerPreBuffering - true for disabling buffer-in-progress
     *   @return true if content successfully paused
     */
    virtual bool Pause(bool pause, bool forceStopGstreamerPreBuffering){ return true; }

    /**
     *   @brief Get playback duration in milliseconds
     *
     *   @return duration in ms.
     */
    virtual long GetDurationMilliseconds(void){ return 0; };

    /**
     *   @brief Get playback position in milliseconds
     *
     *   @return Position in ms.
     */
    virtual long long GetPositionMilliseconds(void){ return 0; };

    /**
     *   @brief Get Video 90 KHz Video PTS
     *
     *   @return video PTS
     */
    virtual long long GetVideoPTS(void){ return 2; };

    /**
     *   @brief Get closed caption handle
     *
     *   @return Closed caption handle
     */
    virtual unsigned long getCCDecoderHandle(void) { return 0; };

    /**
     *   @brief Set video display rectangle co-ordinates
     *
     *   @param[in]  x - x position
     *   @param[in]  y - y position
     *   @param[in]  w - Width
     *   @param[in]  h - Height
     *   @return void
     */
    virtual void SetVideoRectangle(int x, int y, int w, int h){};

    /**
     *   @brief Set video zoom state
     *
     *   @param[in]  zoom - Zoom mode
     *   @return void
     */
    virtual void SetVideoZoom(VideoZoomMode zoom){};

    /**
     *   @brief Set video mute state
     *
     *   @param[in] muted - true: video muted, false: video unmuted
     *   @return void
     */
    virtual void SetVideoMute(bool muted){};

    /**
     *   @brief Set subtitle mute state
     *
     *   @param[in] muted - true: subtitle muted, false: subtitle unmuted
     *   @return void
     */
    virtual void SetSubtitleMute(bool muted){};

    /**
     *   @brief Set subtitle pts offset in sink
     *
     *   @param[in] pts_offset - pts offset for subs display
     *   @return void
     */
    virtual void SetSubtitlePtsOffset(std::uint64_t pts_offset){};

    /**
     *   @brief Set volume level
     *
     *   @param[in]  volume - Minimum 0, maximum 100.
     *   @return void
     */
    virtual void SetAudioVolume(int volume){};

    /**
     *   @brief StreamSink Dtor
     */
    virtual ~StreamSink(){};

    /**
     *   @brief Process PTS discontinuity for a stream type
     *
     *   @param[in]  mediaType - Media Type
     *   @return TRUE if discontinuity processed
     */
    virtual bool Discontinuity( AampMediaType mediaType) = 0;


    /**
     * @brief Check if PTS is changing
     *
     * @param[in] timeout - max time period within which PTS hasn't changed
     * @retval true if PTS is changing, false if PTS hasn't changed for timeout msecs
     */
    virtual bool CheckForPTSChangeWithTimeout(long timeout) { return true; }

    /**
     *   @brief Check whether cache is empty
     *
     *   @param[in]  mediaType - Media Type
     *   @return true: empty, false: not empty
     */
    virtual bool IsCacheEmpty(AampMediaType mediaType){ return true; };

    /**
     * @brief Reset EOS SignalledFlag
     */
    virtual void ResetEOSSignalledFlag(){};

    /**
     *   @brief API to notify that fragment caching done
     *
     *   @return void
     */
    virtual void NotifyFragmentCachingComplete(){};

    /**
     *   @brief API to notify that fragment caching is ongoing
     *
     *   @return void
     */
    virtual void NotifyFragmentCachingOngoing(){};

    /**
     *   @brief Get the video dimensions
     *
     *   @param[out]  w - Width
     *   @param[out]  h - Height
     *   @return void
     */
    virtual void GetVideoSize(int &w, int &h){};

    /**
     *   @brief Queue-up the protection event.
     *
     *   @param[in]  protSystemId - DRM system ID.
     *   @param[in]  ptr - Pointer to the protection data.
     *   @param[in]  len - Length of the protection data.
     *   @return void
     */
    virtual void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, AampMediaType type) {};

    /**
     *   @brief Clear the protection event.
     *
     *   @return void
     */
    virtual void ClearProtectionEvent() {};

    /**
     *   @brief Signal discontinuity on trickmode if restamping is done by stream sink.
     *
     *   @return void
     */
    virtual void SignalTrickModeDiscontinuity() {};

    /**
     *   @brief Seek stream sink to desired position and playback rate with a flushing seek
     *
     *   @param[in]  position - desired playback position.
     *   @param[in]  rate - desired playback rate.
     *   @return void
     */
    virtual void SeekStreamSink(double position, double rate) {};

    /**
     *   @brief Get the video window co-ordinates
     *
     *   @return current video co-ordinates in x,y,w,h format
     */
    virtual std::string GetVideoRectangle() { return std::string(); };

    /**
     *   @brief Stop buffering in sink
     *
     *   @param[in] forceStop - true if buffering to be stopped without any checks
     *   @return void
     */
    virtual void StopBuffering(bool forceStop) { };

    /**
     *   @brief Set the text style of the subtitle to the options passed
     *   @param[in] options - reference to the Json string that contains the information
     *   @return - true indicating successful operation in passing options to the parser
     */
    virtual bool SetTextStyle(const std::string &options) { return false; }

    /**
     *   @brief Get the set text style of the subtitle
     *   @param[in] textStyle - Json string that contains text style
     *   @return - true indicating indicating that subtitles are enabled
     */
    virtual bool GetTextStyle(std::string &textStyle) { return false; }

    /**
     * @brief API to set track Id to audio sync property in case of AC4 audio
     *
     * @param[in] trackId - AC4 track Id parsed by aamp based of preference
     * @return bool status of API
     */

    /**
     *   @brief Get the video playback quality
     *
     *   @return current video playback quality
     */
    virtual PlaybackQualityStruct* GetVideoPlaybackQuality() { return NULL; };

    /**
     * @brief Signal the new clock to subtitle module
     * @return - true indicating successful operation in sending the clock update
     */
    virtual bool SignalSubtitleClock( void ) { return false; };

    /**
     * @fn SetPauseOnPlayback
     * @brief Set to pause on next playback start
     * @param[in] enable - Flag to set whether enabled
     */
    virtual void SetPauseOnStartPlayback(bool enable) {};

    /**
     * @brief Notifies the injector to resume buffer pushing.
     */
    virtual void NotifyInjectorToResume() {};

    /**
     * @brief Notifies the injector to pause buffer pushing.
     */
    virtual void NotifyInjectorToPause() {};

};

#endif // STREAM_SINK_H
