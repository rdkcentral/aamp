/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
 * @class AampStreamSinkInactive
 * @brief Class declaration holding information for an inactive sink
 */


#ifndef AAMPSTREAMSINKINACTIVE_H
#define AAMPSTREAMSINKINACTIVE_H

class AampStreamSinkInactive : public StreamSink
{

public:
	AampStreamSinkInactive(id3_callback_t id3HandlerCallback) : mId3HandlerCallback(id3HandlerCallback)
	{
	}

	AampStreamSinkInactive(const AampStreamSinkInactive&) = delete;
	AampStreamSinkInactive& operator=(const AampStreamSinkInactive&) = delete;

	/**
   	 * @fn ~AampStreamSinkInactive
   	 */
	~AampStreamSinkInactive()
	{
	}
	/**
     *   @fn Configure
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation=false)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SendCopy
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool SendCopy( AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn SendTransfer
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool SendTransfer( AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double duration, double fragmentPTSoffset, bool initFragment = false, bool discontinuity = false)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn EndOfStreamReached
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void EndOfStreamReached(AampMediaType mediaType)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn Stream
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void Stream(void)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn Stop
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void Stop(bool keepLastFrame)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn Flush
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void Flush(double position = 0, int rate = AAMP_NORMAL_PLAY_RATE, bool shouldTearDown = true)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetPlayBackRate
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool SetPlayBackRate ( double rate )
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn AdjustPlayBackRate
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool AdjustPlayBackRate(double position, double rate)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn Pause
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool Pause(bool pause, bool forceStopGstreamerPreBuffering)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn GetDurationMilliseconds
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual long GetDurationMilliseconds(void)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return 0;
	}
	/**
     *   @fn GetPositionMilliseconds
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual long long GetPositionMilliseconds(void)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return 0;
	}
	/**
     *   @fn GetVideoPTS
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual long long GetVideoPTS(void)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return 0;
	}
	/**
     *   @fn getCCDecoderHandle
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual unsigned long getCCDecoderHandle(void)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return 0;
	}
	/**
     *   @fn SetVideoRectangle
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SetVideoRectangle(int x, int y, int w, int h)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetVideoZoom
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SetVideoZoom(VideoZoomMode zoom)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetVideoMute
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SetVideoMute(bool muted)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetSubtitleMute
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SetSubtitleMute(bool muted)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetSubtitlePtsOffset
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SetSubtitlePtsOffset(std::uint64_t pts_offset)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetAudioVolume
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SetAudioVolume(int volume)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn Discontinuity
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool Discontinuity( AampMediaType mediaType)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn CheckForPTSChangeWithTimeout
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool CheckForPTSChangeWithTimeout(long timeout)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn IsCacheEmpty
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool IsCacheEmpty(AampMediaType mediaType)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn ResetEOSSignalledFlag
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void ResetEOSSignalledFlag()
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn NotifyFragmentCachingComplete
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void NotifyFragmentCachingComplete()
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn NotifyFragmentCachingOngoing
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void NotifyFragmentCachingOngoing()
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn GetVideoSize
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void GetVideoSize(int &w, int &h)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn QueueProtectionEvent
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, AampMediaType type)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn ClearProtectionEvent
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void ClearProtectionEvent()
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SignalTrickModeDiscontinuity
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SignalTrickModeDiscontinuity()
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SeekStreamSink
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void SeekStreamSink(double position, double rate)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn GetVideoRectangle
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual std::string GetVideoRectangle()
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return "";
	}
	/**
     *   @fn StopBuffering
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual void StopBuffering(bool forceStop)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
	}
	/**
     *   @fn SetTextStyle
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool SetTextStyle(const std::string &options)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn GetTextStyle
	 *   @brief stub implementation for Inactive aamp instance
	 */
	virtual bool GetTextStyle(std::string &textStyle)
	{
		AAMPLOG_WARN("Called AAMPGstPlayer()::%s stub", __FUNCTION__);
		return false;
	}
	/**
     *   @fn GetID3MetadataHandler
	 *   @brief Returns the id3 callback handle associated with this instance
	 */
	id3_callback_t GetID3MetadataHandler()
	{
		return mId3HandlerCallback;
	}

private:
	id3_callback_t mId3HandlerCallback;		/**< Returns the id3 callback handle associated with this instance */
};
#endif /* AAMPSTREAMSINKINACTIVE_H */
