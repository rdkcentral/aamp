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

#ifndef BRCM_SOC_INTERFACE_H
#define BRCM_SOC_INTERFACE_H

#include "SocInterface.h"

/**
 * @brief BRCM SoC interface class.
 *
 * This class provides an implementation of the SocInterface for BRCM SoC.
 */
class BrcmSocInterface : public SocInterface
{
	public:
		BrcmSocInterface();

		/**
		 * @brief Check if AC4 should be disabled.
		 *
		 * Determines whether AC4 support should be disabled.
		 *
		 * @return True if AC4 should be disabled, false otherwise.
		 */
		bool IsSupportedAC4()override{return true;}

		/**
		 * @brief Check if PTS restamping is supported by the platform.
		 *
		 * Determines whether the platform supports PTS restamping.
		 *
		 * @return True if PTS restamping is supported, false otherwise.
		 */
		bool EnablePTSRestamp()override{return true;}

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
		 * @brief Get video sink from sinkbin.
		 * @param sinkbin The GStreamer sinkbin.
		 */
		GstElement* GetVideoSink(GstElement* sinkbin) override;

		/**
		 * @brief Set AC4 tracks.
		 * @param src Source element.
		 * @param trackId Track ID.
		 */
		void SetAC4Tracks(GstElement *src, int trackId) override{MW_LOG_WARN("AC4 support has not done for this platform - track Id: %d", trackId);}

		/**
		 * @brief Set platform playback rate.
		 * @return True on success, false otherwise.
		 */
		bool SetPlatformPlaybackRate() override{return true;}

		/**
		 * @brief Set rate correction.
		 * @return True on success, false otherwise.
		 */
		bool SetRateCorrection() override {return true;}

		void GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)override;
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
		 * @brief Set playback flags.
		 *
		 * Sets the playback flags based on the given parameters.
		 * @param flags Reference to the flags integer.
		 * @param isSub Flag indicating whether the content is a subtitle.
		 */
		void SetPlaybackFlags(gint &flags, bool isSub)override;
};


#endif
