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
#include "SocInterface.h"
#include "vendor/default/DefaultSocInterface.h"
#include "vendor/amlogic/AmlogicSocInterface.h"
#include "vendor/brcm/BrcmSocInterface.h"
#include "vendor/realtek/RealtekSocInterface.h"
DefaultSocInterface::DefaultSocInterface()
{
}
std::shared_ptr<SocInterface> SocInterface::CreateSocInterface()
{
        std::shared_ptr<SocInterface> obj = std::make_shared<DefaultSocInterface>();
        return obj;
}
std::shared_ptr<SocInterface> SocInterface::CreateSocInterface(bool isRialto)
{
        std::shared_ptr<SocInterface> obj = std::make_shared<DefaultSocInterface>();
        return obj;
}
bool DefaultSocInterface::UseAppSrc()
{
#if defined (__APPLE__)
	return true;
#endif
	return false;
}

void DefaultSocInterface::SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume)
{
	isSinkBinVolume = false;
	volume = "volume";
	mute = "mute";
#if defined(__APPLE__)
	isSinkBinVolume = true;
#endif
}
/**
 * @brief Set AC4 tracks.
 * @param src Source element.
 * @param trackId Track ID.
 */
void SocInterface::SetAC4Tracks(GstElement *src, int trackId)
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
bool DefaultSocInterface::IsVideoSink(const char* name)
{
	return name && (
					StartsWith(name,"rialtomsevideosink") ||
					StartsWith(name, "brcmvideosink") ||
					StartsWith(name, "westerossink") );
}
/**
 * @brief Check if the given name is a video decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @param isWesteros Westeros flag.
 * @return True if it's a video decoder, false otherwise.
 */
bool DefaultSocInterface::IsVideoDecoder(const char* name)
{
	return name && (
					StartsWith(name,"rialtomsevideosink") ||
					StartsWith(name, "brcmvideosink") ||
					StartsWith(name, "westerossink") );
}

/**
 * @brief Check if the given name is an audio or video decoder.
 * @param name Element name.
 * @param IsWesteros Westeros flag.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool DefaultSocInterface::IsAudioOrVideoDecoder(const char* name)
{
	return StartsWith(name,"rialtomsevideosink") || StartsWith(name,"rialtomseaudiosink");
}

/**
 * @brief Set playback flags.
 *
 * Sets the playback flags based on the given parameters.
 * @param flags Reference to the flags integer.
 * @param isSub Flag indicating whether the content is a subtitle.
 */
void DefaultSocInterface::SetPlaybackFlags(gint &flags,  bool isSub)
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
#if (defined(RPI) || defined(__APPLE__) || defined(UBUNTU))
	return true;
#endif
	return false;
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
void SocInterface::SetDecodeError(GstObject* src)
{
        g_object_set(src, "report_decode_errors", TRUE, NULL);
}
void SocInterface::SetWesterosSinkState(bool status)
{
	mUsingWesterosSink = true;
}
long long SocInterface::GetVideoPts(GstElement *video_sink, GstElement *video_dec, bool isWesteros)
{
        gint64 currentPTS = 0;
        GstElement *element;
        element = video_dec;
        if(element)
        {
                g_object_get(element, "video-pts", &currentPTS, NULL);/* Gets the 'video-pts' from the element into the currentPTS */
                if(!isWesteros)
                {
                        currentPTS = currentPTS * 2;
                }
        }
        return (long long)currentPTS;
}
bool SocInterface::StartsWith( const char *inputStr, const char *prefix )
{
        bool rc = true;
        while( *prefix )
        {
                if( *inputStr++ != *prefix++ )
                {
                        rc = false;
                        break;
                }
        }
        return rc;
}
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
	  
bool SocInterface::IsVideoMaster(GstElement *videoSink)
{
	return true;
}

/**
 * @brief Sets the playback rate for the given GStreamer elements.
 *
 * @param sources A vector of GStreamer source elements.
 * @param pipeline The main GStreamer pipeline.
 * @param rate The desired playback rate.
 * @param video_dec The video decoder element.
 * @param audio_dec The audio decoder element.
 * @param isRialto True if rialtosink is used.
 * @return True if the playback rate was set successfully, false otherwise.
 */
bool DefaultSocInterface::SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec)
{
	return false;
}

/**
 * @brief Configure Capability Acceptance for GStreamer Transform
 *
 * Sets up the accept_caps function pointer for a GStreamer base transform class.
 * This allows the transform element to decide whether it can accept a given set of capabilities (caps),
 * which is essential for negotiating media formats during pipeline setup.
 *
 * @param base_transform_class Pointer to the GStreamer base transform class to configure.
 * @param accept_caps_func Function used to determine if the transform accepts specific caps.
 */
void SocInterface::ConfigureAcceptCaps(GstBaseTransformClass* base_transform_class ,
													AcceptCapsFunc accept_caps_func)
{
}

bool DefaultSocInterface::IsVideoMaster(GstElement *videoSink)
{
	return true;
}
