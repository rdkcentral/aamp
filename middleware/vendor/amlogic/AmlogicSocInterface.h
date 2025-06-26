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

#ifndef AMLOGIC_SOC_INTERFACE_H
#define AMLOGIC_SOC_INTERFACE_H

#include "SocInterface.h"

/**
 * @brief Amlogic SoC interface class.
 *
 * This class provides an implementation of the SocInterface for Amlogic SoCs.
 */
class AmlogicSocInterface : public SocInterface 
{
	public:
		AmlogicSocInterface();

		/**
		 * @brief Get video sink from sinkbin.
		 * @param sinkbin The GStreamer sinkbin.
		 */

		GstElement* GetVideoSink(GstElement* sinkbin)override;

		/**
		 * @brief Get SoC volume property name.
		 * @return Volume property name.
		 */
		void SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume)override;

		/**
		 * @brief enables the seamless switch property
		 * @param object The GStreamer element to configure.
		 * @param value True to enable seamless switching, false to disable.
		 */
		void SetSeamlessSwitch(GstElement* object, gboolean value) override;

		/**
		 * @brief Sets the sinkbin to audio-only mode.
		 *
		 * This is a pure virtual function that must be implemented by derived classes.
		 *
		 * @param sinkbin The GStreamer sinkbin to configure.
		 * @param property The name of the property to set for audio-only mode.
		 */
		bool AudioOnlyMode(GstElement *sinkbin) override;

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
		 * @brief Retrieves the source pad of the given GStreamer element.
		 *
		 * This is a pure virtual function that must be implemented by derived classes.
		 *
		 * @param element The GStreamer element to retrieve the source pad from.
		 * @return A pointer to the source pad of the element, or NULL if not found.
		 */
		GstPad* GetSourcePad(GstElement* element) override;

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
		 * @brief Retrieves the CC decoder handle.
		 * @param[out] dec_handle Pointer to store the retrieved CC decoder handle.
		 * @param[in] video_dec The GStreamer video decoder element from which the CC decoder handle is extracted.
		 */

		void GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)override;

		/**
		 * @brief Disable asynchronous audio.
		 * @param audio_sink Audio sink element.
		 * @param rate Playback rate.
		 * @param isSeeking True if seeking is in progress, false otherwise.
		 * @return True on success, false otherwise.
		 */
		void SetPlaybackFlags(gint &flags,  bool isSub)override;

		/**
		 * @brief Resets segment event flags during trickplay transitions.
		 *
		 * Manages segment event tracking for trickplay scenarios without disrupting seekplay or advertisements.
		 */
		virtual bool ResetNewSegmentEvent()override{return true;}

		/**
		 *@brief check if platform segment is ready
		 *
		 *it is used in scenarios where AV synchronization and trick mode speed adjustments are necessary.
		 */
		bool IsPlatformSegmentReady()override{return true;}

		/**
		 * @brief Check if the video is the master stream.
		 *
		 * This function always returns false, indicating that the video is not the master stream.
		 *
		 * @return false indicating the video is not the master stream.
		 */
		bool IsVideoMaster()override{return false;}

};

#endif
