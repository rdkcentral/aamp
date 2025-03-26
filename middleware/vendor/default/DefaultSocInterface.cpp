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

#include "DefaultSocInterface.h"

DefaultSocInterface::DefaultSocInterface()
{
}

/**
 * @brief Check if AppSrc should be used for progressive playback.
 *
 * Determines whether the AppSrc element should be used in the current context.
 *
 * @return True if AppSrc should be used, false otherwise.
 */
bool DefaultSocInterface::UseAppSrc()
{
#if defined (__APPLE__)
	return true;
#endif
	return false;
}

/**
 * @brief Get SoC volume property name.
 * @return Volume property name.
 */
void DefaultSocInterface::SetAudioProperty(const char **volume, const char **mute, bool& isSinkBinVolume)
{
	isSinkBinVolume = false;
	*volume = "volume";
	*mute = "mute";
#if defined(__APPLE__)
	isSinkBinVolume = true;
#endif
}

/**
 * @brief Set AC4 tracks.
 * @param src Source element.
 * @param trackId Track ID.
 */
void DefaultSocInterface::SetAC4Tracks(GstElement *src, int trackId)
{
	MW_LOG_INFO("Selecting AC4 Track Id : %d", trackId);
	g_object_set(src, "ac4-presentation-group-index", trackId, NULL);
}

bool DefaultSocInterface::IsVideoSink(const char* name, bool isRialto)
{
	return (mUsingWesterosSink && StartsWith(name, "westerossink") == true);
}

/**
 * @brief Check if the given name is a video decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @return True if it's a video decoder, false otherwise.
 */
bool DefaultSocInterface::IsVideoDecoder(const char* name, bool isRialto)
{
	if(mUsingWesterosSink)
	{
		return StartsWith(name, "westerossink");
	}
	else if (isRialto)
	{
		return StartsWith(name, "rialtomsevideosink");
	}
	return false;
}

/**
 * @brief Check if the given name is an audio or video decoder.
 * @param name Element name.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool DefaultSocInterface::IsAudioOrVideoDecoder(const char* name, bool isRialto)
{
	bool AudioOrVideoDecoder = false;
	if(mUsingWesterosSink && StartsWith(name, "westerossink"))
	{
		AudioOrVideoDecoder = true;
	}
	else if(isRialto && StartsWith(name, "rialtomse"))
	{
		AudioOrVideoDecoder = true;
	}
	return AudioOrVideoDecoder;
}

/**
 * @brief Set playback flags.
 *
 * Sets the playback flags based on the given parameters.
 * @param flags Reference to the flags integer.
 * @param noNativeAV Flag indicating whether to disable native AV decoding.
 * @param isSub Flag indicating whether the content is a subtitle.
 */
void DefaultSocInterface::SetPlaybackFlags(gint &flags, bool isSub)
{
#if  (defined(__APPLE__))
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_SOFT_VOLUME;
#else
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_NATIVE_AUDIO | PLAY_FLAG_NATIVE_VIDEO;
#endif
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_SOFT_VOLUME;
	if(isSub)
	{
		flags = PLAY_FLAG_TEXT;
	}
}

bool DefaultSocInterface::IsSimulatorFirstFrame()
{
	return true;
}

bool DefaultSocInterface::IsSimulatorSink()
{
#if !defined(UBUNTU)
	return false;
#endif
	return true;
}

void DefaultSocInterface::ConfigurePluginPriority()
{
#ifdef UBUNTU
	GstPluginFeature* pluginFeature = gst_registry_lookup_feature(gst_registry_get(), "pulsesink");
	if (pluginFeature != NULL)
	{
		MW_LOG_INFO("InterfacePlayerRDK: pulsesink plugin priority set to GST_RANK_SECONDARY");
		gst_plugin_feature_set_rank(pluginFeature, GST_RANK_SECONDARY);
		gst_object_unref(pluginFeature);
	}
#endif
}

bool DefaultSocInterface::ShouldTearDownForTrickplay()
{
#if defined(__APPLE__) || defined(UBUNTU)
	return true;
#endif
	return false;
}

bool DefaultSocInterface::IsSimulatorVideoSample()
{
#if defined(__APPLE__)
	return true;
#endif
	return true;
}

void DefaultSocInterface::SetH264Caps(GstCaps *caps)
{
#ifdef UBUNTU
	// below required on Ubuntu - harmless on OSX, but breaks RPI
	gst_caps_set_simple (caps,
			"alignment", G_TYPE_STRING, "au",
			"stream-format", G_TYPE_STRING, "avc",
			NULL);
#endif
}

void DefaultSocInterface::SetHevcCaps(GstCaps *caps)
{
#ifdef UBUNTU
	// below required on Ubuntu - harmless on OSX, but breaks RPI
	gst_caps_set_simple(caps,
			"alignment", G_TYPE_STRING, "au",
			"stream-format", G_TYPE_STRING, "hev1",
			NULL);
#endif
}

/**
 * @brief Configure the audio sink.
 * @param audio_sink Pointer to the audio sink element.
 * @param src Source object.
 * @param decStreamSync Decoder stream synchronization flag.
 * @return True on success, false otherwise.
 */
bool DefaultSocInterface::ConfigureAudioSink(GstElement **audio_sink, GstObject *src, bool decStreamSync)
{
        bool status = false;
        if (StartsWith(GST_OBJECT_NAME(src), "amlhalasink") == true)
        {
                gst_object_replace((GstObject **)audio_sink, src);
                g_object_set(audio_sink, "disable-xrun", TRUE, NULL);
                status = true;
        }
        return status;
}
