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

#ifndef REALTEK_SOC_INTERFACE_H
#define REALTEK_SOC_INTERFACE_H

#include "SocInterface.h"

#define REQUIRED_QUEUED_FRAMES_REALTEK (3+1)
/**
 * @brief Realtek SoC interface class.
 *
 * This class provides an implementation of the SocInterface for Realtek SoC.
 */
class RealtekSocInterface : public SocInterface
{

	public:
		RealtekSocInterface();


		/**
		 * @brief Check if audio fragments should be synchronized.
		 *
		 * Determines whether audio fragments should be synchronized.
		 *
		 * @return True if audio fragments should be synchronized, false otherwise.
		 */
		bool IsAudioFragmentSyncSupported()override{return true;}

		/*@brief returns true if video stats required from sink otherwise false*/
		bool IsPlaybackQualityFromSink() override {return true;}

		/**
		 * @brief Get video sink from sinkbin.
		 * @param sinkbin The GStreamer sinkbin.
		 */

		GstElement* GetVideoSink(GstElement* sinkbin)override;


		/**
		 * @brief Check if live latency correction should be enabled.
		 *
		 * Determines whether live latency correction should be enabled.
		 *
		 * @return True if live latency correction should be enabled, false otherwise.
		 */
		bool EnableLiveLatencyCorrection()override{return true;}

		/**
		 * @brief Get the required number of queued frames.
		 *
		 * Returns the total number of frames that should be queued.
		 *
		 * @return The required number of queued frames.
		 */
		int RequiredQueuedFrames()override{return REQUIRED_QUEUED_FRAMES_REALTEK;}

		/**
		 * Sets buffer size and duration for the given GstElement.
		 *
		 * @param sink The GstElement to configure.
		 * @param size The desired buffer size.
		 */
		void SetVideoBufferSize(GstElement *sink, int size)override;

		/**
		 * Sets asynchronous mode for the given Sink.
		 *
		 * @param sink element.
		 * @param status Enable (TRUE) or disable (FALSE) asynchronous mode.
		 */
		void SetSinkAsync(GstElement *sink, gboolean status)override;

		/**
		 * Checks if this is the first tune with Westeros disabled.
		 *
		 * @return True if this is the first tune with Westeros disabled for the current platform, false otherwise.
		 */
		bool IsFirstTuneWithWesteros()override{return true;}

		/**
		 * @brief Get SoC volume property name.
		 * @return Volume property name.
		 */
		void SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume)override;

		/**
		 * @brief Sets the playback rate for the given GStreamer elements.
		 *
		 * @param sources A vector of GStreamer source elements.
		 * @param pipeline The main GStreamer pipeline.
		 * @param rate The desired playback rate.
		 * @param video_dec The video decoder element.
		 * @param audio_dec The audio decoder element.
		 * @return True if the playback rate was set successfully, false otherwise.
		 */
		bool SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec) override;

		/**
		 * @brief Set AC4 tracks.
		 * @param src Source element.
		 * @param trackId Track ID.
		 */
		void SetAC4Tracks(GstElement *src, int trackId) override;

		/**
		 * @brief Set platform playback rate.
		 * @return True on success, false otherwise.
		 */
		bool SetPlatformPlaybackRate() override{return true;}

		/**
		 * @brief Set rate correction.
		 * @return True on success, false otherwise.
		 */
		bool SetRateCorrection() override {return false;}

		/**
		 * @brief Check if the given name is a video sink.
		 * @param name Element name.
		 * @param isRialto Rialto flag.
		 * @return True if it's a video sink, false otherwise.
		 */
		bool IsVideoSink(const char* name, bool isRialto)override;

		/**
		 * @brief Check if the given name is an audio sink or audio decoder.
		 * @param name Element name.
		 * @param isRialto Rialto flag.
		 * @return True if it's an audio sink or audio decoder, false otherwise.
		 */
		bool IsAudioSinkOrAudioDecoder(const char* name, bool isRialto)override;

		/**
		 * @brief Check if the given name is a video decoder.
		 * @param name Element name.
		 * @param isRialto Rialto flag.
		 * @return True if it's a video decoder, false otherwise.
		 */
		bool IsVideoDecoder(const char* name, bool isRialto)override;

		/**
		 * @brief Configure the audio sink.
		 * @param audio_sink Pointer to the audio sink element.
		 * @param src Source object.
		 * @param decStreamSync Decoder stream synchronization flag.
		 * @return True on success, false otherwise.
		 */
		bool ConfigureAudioSink(GstElement **audio_sink, GstObject *src, bool decStreamSync)override;

		/**
		 * @brief Check if the given name is an audio or video decoder.
		 * @param name Element name.
		 * @param IsWesteros Westeros flag.
		 * @return True if it's an audio or video decoder, false otherwise.
		 */
		bool IsAudioOrVideoDecoder(const char* name, bool isRialto)override;

		/**
		 * @brief Disable asynchronous audio.
		 * @param audio_sink Audio sink element.
		 * @param rate Playback rate.
		 * @param isSeeking True if seeking is in progress, false otherwise.
		 * @return True if async changed from enabled to disabled, false otherwise.
		 */
		bool DisableAsyncAudio(GstElement *audio_sink, int rate, bool isSeeking)override;

		/**
		 * Gets the decoder handle from the video decoder element.
		 *
		 * @param dec_handle Pointer to store the decoder handle.
		 * @param video_dec The video decoder element.
		 */
		void GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)override;

		/**
		 * @brief Resets the trick play UTC.
		 *
		 * @return True if the reset is required, false otherwise.
		 */
		bool ResetTrickUTC()override{return true;}

		/**
		 * @brief Get video PTS.
		 *
		 * Retrieves the current video presentation timestamp (PTS).
		 *
		 * @param video_sink The video sink element.
		 * @param video_dec The video decoder element.
		 * @param isWesteros A flag for Westeros logic.
		 *
		 * @return Video PTS in nanoseconds, or -1 on error.
		 */
		long long GetVideoPts(GstElement *video_sink, GstElement *video_dec, bool isWesteros)override;

		/**
		 * @brief Notify first video frame.
		 */
		bool NotifyVideoFirstFrame()override{return true;}

		/**
		 * @brief Set decode error on source.
		 *
		 * Sets a decode error flag on the given source object.
		 *
		 * @param src The source object.
		 */
		void SetDecodeError(GstObject* src)override{}

		/**
		 * @brief Set freerun threshold on source.
		 *
		 * Sets the freerun threshold on the given source object.
		 *
		 * @param src The source object.
		 */
		void SetFreerunThreshold(GstObject* src)override;

		/**
		 * @brief Check if element setup is required.
		 *
		 * Determines if the element requires setup before it can be used.
		 *
		 * @return True if setup is required, false otherwise.
		 */
		bool RequiredElementSetup()override{return true;}

		/**
		 * @brief Set audio routing properties on source.
		 *
		 * Sets audio routing properties on the given source element.
		 *
		 * @param source The source element.
		 */
		void SetAudioRoutingProperties(GstElement *source)override;

		/**
		 * @brief Check if first audio frame callback is set.
		 *
		 * Determines if a callback function has been set for the first audio frame.
		 *
		 * @return True if a callback is set, false otherwise.
		 */
		bool HasFirstAudioFrameCallback()override{return false;}

		/**
		 * @brief Check if video sink errors are handled.
		 *
		 * Determines if the platform handles errors from the video sink.
		 *
		 * @return True if video sink errors are handled, false otherwise.
		 */
		bool IsVideoSinkHandleErrors()override{return true;}

		/**
		 * @brief Set playback flags.
		 *
		 * Sets the playback flags based on the given parameters.
		 * @param flags Reference to the flags integer.
		 * @param isSub Flag indicating whether the content is a subtitle.
		 */
		void SetPlaybackFlags(gint &flags, bool isSub)override;

		/**
		 *@brief Sets the platform specific H264 caps
		 */
		void SetH264Caps(GstCaps *caps)override;

		/**
		 *@brief Sets the HEVC caps
		 */
		void SetHevcCaps(GstCaps *caps)override;


};
#endif
