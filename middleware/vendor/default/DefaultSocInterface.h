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

#ifndef DEFAULT_SOC_INTERFACE_H
#define DEFAULT_SOC_INTERFACE_H

#include "SocInterface.h"

/**
 * @brief Default SoC interface class.
 *
 * This class provides an implementation of the APPLE/UBUNTU/RPI gstreamer code.
 */
class DefaultSocInterface : public SocInterface
{

	public:
		DefaultSocInterface();

		/**
		 * @brief Check if AppSrc should be used.
		 *
		 * Determines whether the AppSrc element should be used in the current context.
		 *
		 * @return True if AppSrc should be used, false otherwise.
		 */
		bool UseAppSrc()override;

		/**
		 * @brief Check if Westeros sink should be used.
		 *
		 * Determines whether the Westeros sink should be used in the current context.
		 *
		 * @return True if Westeros sink should be used, false otherwise.
		 */
		bool UseWesterosSink()override{return false;}

		/**
		 * @brief Get volume property name.
		 * @return Volume property name.
		 */
		void SetAudioProperty(const char **volume, const char **mute, bool& isSinkBinVolume)override;

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
		bool SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec) override{return false;}

		/**
		 * @brief Set AC4 tracks.
		 * @param src Source element.
		 * @param trackId Track ID.
		 */
		void SetAC4Tracks(GstElement *src, int trackId) override;

		/**
		 * @brief Set rate correction.
		 * @return True on success, false otherwise.
		 */
		bool SetRateCorrection() override {return false;}

		void GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)override{};

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
		bool IsAudioSinkOrAudioDecoder(const char* name, bool isRialto) override {return false;}

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

		/**
		 * @brief checks if the firstFrame is received from the simulator
		 */
		bool IsSimulatorFirstFrame()override;

		/**
		 * @brief checks if the sink is from the simulator
		 */
		bool IsSimulatorSink()override;

		/**
		 * @brief Configure the plugin priority for PulseAudio.
		 */
		void ConfigurePluginPriority()override;

		/**
		 * @brief checks if the teardown is required for simulator
		 */
		bool ShouldTearDownForTrickplay()override;

		/**
		 * @brief checks if the video sample is from the simulator
		 */
		bool IsSimulatorVideoSample()override;

		/**
		 * @brief checks if the video sample is from the simulator
		 */
		void SetH264Caps(GstCaps *caps)override;

		/**
		 *@brief Sets the HEVC caps for simulator
		 */
		void SetHevcCaps(GstCaps *caps)override;

};

#endif
