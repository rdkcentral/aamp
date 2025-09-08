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

#include "AmlogicSocInterface.h"

/**
 * @brief AmlogicSocInterface constructor.
 */
AmlogicSocInterface::AmlogicSocInterface()
{
}

/**
 * @brief Get SoC volume property name.
 * @return Volume property name.
 */
void AmlogicSocInterface::SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume)
{
	/* Avoid mute property setting for AMLOGIC as use of "mute" property on pipeline is impacting all other players */
	/* Using "stream-volume" property of audio-sink for setting volume and mute for AMLOGIC platform */
	volume = "stream-volume";
	isSinkBinVolume = false;
}

/**
 * @brief enables the seamless switch property
 * @param object The GStreamer element to configure.
 * @param value True to enable seamless switching, false to disable.
 */
void AmlogicSocInterface::SetSeamlessSwitch(GstElement* sink, gboolean value)
{
	MW_LOG_INFO("AMLOGIC:setting seamless property");
	g_object_set(sink, "seamless-switch", value, NULL);
}

/**
 * @brief Sets the sinkbin to audio-only mode.
 *
 * This is a pure virtual function that must be implemented by derived classes.
 *
 * @param sinkbin The GStreamer sinkbin to configure.
 * @param property The name of the property to set for audio-only mode.
 */
bool AmlogicSocInterface::AudioOnlyMode(GstElement *sinkbin)
{
	gint n_audio;
	bool firstFrameReceived = false;
	g_object_get(sinkbin, "n-audio", &n_audio, NULL);

	if(n_audio > 0)
	{
		firstFrameReceived = true;
	}
	return firstFrameReceived;
}

/**
 * @brief Sets the playback rate for the given GStreamer elements.
 *
 * @param sources A vector of GStreamer source elements.
 * @param pipeline The main GStreamer pipeline.
 * @param rate The desired playback rate.
 * @param video_dec The video decoder element.
 * @param audio_dec The audio decoder element.
 * @param isRialto True if rialto sink is used.
 * @return True if the playback rate was set successfully, false otherwise.
 */
bool AmlogicSocInterface::SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec, bool isRialto)
{
	bool status = false;
        /*for gst version 1.18.0 we need to apply rate into audio/video source pad*/
        for (GstElement* source : sources)
        {
                if(source)
                {
                        GstPad* sourceEleSrcPad = gst_element_get_static_pad(GST_ELEMENT(source), "src");
                        if(!sourceEleSrcPad)
                        {
                                MW_LOG_ERR("failed to get static pad retrying");
                                continue;
                        }
                        /*
                                gboolean ret = gst_pad_send_event(sourceEleSrcPad, gst_event_new_seek (rate, GST_FORMAT_TIME,
                                static_cast<GstSeekFlags>(GST_SEEK_FLAG_INSTANT_RATE_CHANGE),
                                GST_SEEK_TYPE_NONE,0, GST_SEEK_TYPE_NONE, 0));
                                gst_object_unref(sourceEleSrcPad);
                                */
                        GstEvent* seek_event = gst_event_new_seek(rate, GST_FORMAT_TIME, static_cast<GstSeekFlags>(GST_SEEK_FLAG_INSTANT_RATE_CHANGE), GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_NONE, 0);
                        if (!seek_event)
                        {
                                MW_LOG_ERR("Failed to create seek event");
                                gst_object_unref(sourceEleSrcPad);
                                continue;
                        }
                        gboolean ret = gst_pad_send_event(sourceEleSrcPad, seek_event);
                        gst_object_unref(sourceEleSrcPad);
                        if(ret)
                        {
                                status = true;
                        }
                        else
                        {
                                MW_LOG_ERR("Vendor: failed to send the rate event to src pad");
                        }
                }
        }
        MW_LOG_MIL("Current rate: %g", rate);

	return status;
}

/**
 * @brief Retrieves the source pad of the given GStreamer element.
 *
 * This is a pure virtual function that must be implemented by derived classes.
 *
 * @param element The GStreamer element to retrieve the source pad from.
 * @return A pointer to the source pad of the element, or NULL if not found.
 */
GstPad* AmlogicSocInterface::GetSourcePad(GstElement* source)
{
        GstPad* sourceEleSrcPad = gst_element_get_static_pad(source, "src");
        return sourceEleSrcPad;  // Return the pad, or NULL if not found
}

/**
 * @brief Set AC4 tracks.
 * @param src Source element.
 * @param trackId Track ID.
 */
void AmlogicSocInterface::SetAC4Tracks(GstElement *src, int trackId)
{
	MW_LOG_INFO("Selecting AC4 Track Id : %d", trackId);
	if(src)
	{
		g_object_set(src, "ac4-presentation-group-index", trackId, NULL);
	}
	else
	{
		MW_LOG_ERR("No valid src to set ac4-presentation-group-index");
	}
}

/**
 * @brief Check if the given name is a video sink.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @return True if it's a video sink, false otherwise.
 */
bool AmlogicSocInterface::IsVideoSink(const char* name, bool isRialto)
{
	if (name == nullptr)
	{
		return false;
	}

	// Check for Westeros sink
	if (mUsingWesterosSink && StartsWith(name, "westerossink"))
	{
		return true;
	}

	// Check for Rialto sink
	if (isRialto && StartsWith(name, "rialtomsevideosink"))
	{
		return true;
	}

	return false;
}

/**
 * @brief Check if the given name is an audio sink or audio decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @return True if it's an audio sink or audio decoder, false otherwise.
 */
bool AmlogicSocInterface::IsAudioSinkOrAudioDecoder(const char* name, bool isRialto)
{
	if(name)
	{
		return StartsWith(name, "amlhalasink");
	}
	else
	{
		return false;
	}
}

/**
 * @brief Check if the given name is a video decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @param isWesteros Westeros flag.
 * @return True if it's a video decoder, false otherwise.
 */
bool AmlogicSocInterface::IsVideoDecoder(const char* name, bool isRialto)
{
	if(name)
	{
		if(mUsingWesterosSink)
		{
			return StartsWith(name, "westerossink");
		}

		else if (isRialto)
		{
			return StartsWith(name, "rialtomsevideosink");
		}
	}
	return false;
}

/**
 * @brief Configure the audio sink.
 * @param audio_sink Pointer to the audio sink element.
 * @param src Source object.
 * @param decStreamSync Decoder stream synchronization flag.
 * @return True on success, false otherwise.
 */
bool AmlogicSocInterface::ConfigureAudioSink(GstElement **audio_sink, GstObject *src, bool decStreamSync)
{
	if (!audio_sink || !src)
	{
		MW_LOG_ERR("ConfigureAudioSink: Invalid input parameters");
		return false;
	}

	bool status = false;
	const char* srcName = GST_OBJECT_NAME(src);
	if (srcName && StartsWith(srcName, "amlhalasink"))
	{
		gst_object_replace(reinterpret_cast<GstObject **>(audio_sink), src);

		if (*audio_sink)  // Ensure audio_sink was set correctly
		{
			g_object_set(*audio_sink, "disable-xrun", TRUE, NULL);
			status = true;
		}
	}
	return status;
}
/**
 * @brief Check if the given name is an audio or video decoder.
 * @param name Element name.
 * @param IsWesteros Westeros flag.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool AmlogicSocInterface::IsAudioOrVideoDecoder(const char* name, bool isRialto)
{
	bool AudioOrVideoDecoder = false;
	if(name)
	{
		if(mUsingWesterosSink && StartsWith(name, "westerossink"))
		{
			AudioOrVideoDecoder = true;
		}
		else if(isRialto && StartsWith(name, "rialtomse"))
		{
			AudioOrVideoDecoder = true;
		}
	}
	return AudioOrVideoDecoder;
}

/**
 * @brief Set playback flags.
 *
 * Sets the playback flags based on the given parameters.
 *
 * @param flags Reference to the flags integer.
 * @param isSub Flag indicating whether the content is a subtitle.
 */
void AmlogicSocInterface::SetPlaybackFlags(gint &flags, bool isSub)
{
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_NATIVE_AUDIO | PLAY_FLAG_NATIVE_VIDEO;
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_SOFT_VOLUME;
	if(isSub)
	{
		flags = PLAY_FLAG_TEXT;
	}
}

/**
 * @brief Get video sink from sinkbin.
 * @param sinkbin The GStreamer sinkbin.
 * @return Video sink element.
 */

GstElement* AmlogicSocInterface::GetVideoSink(GstElement* sinkbin)
{
	GstElement* vidsink = nullptr;
	if(!sinkbin)
	{
		MW_LOG_ERR("Invalid SinkBin");
	}
	else if(mUsingWesterosSink)
	{
		MW_LOG_INFO("using westerossink");
		vidsink = gst_element_factory_make("westerossink", NULL);
		g_object_set(sinkbin, "video-sink", vidsink, NULL);
	}
	return vidsink;
}

/**
 * @brief Retrieves the CC decoder handle.
 * @param[out] dec_handle Pointer to store the retrieved CC decoder handle.
 * @param[in] video_dec The GStreamer video decoder element from which the CC decoder handle is extracted.
 */
void AmlogicSocInterface::GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)
{
	if (video_dec)
	{
		g_object_get(video_dec, "videodecoder", dec_handle, NULL);
	}
}
