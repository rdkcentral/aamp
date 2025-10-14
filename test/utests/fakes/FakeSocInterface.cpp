#include "SocInterface.h"
#include "vendor/default/DefaultSocInterface.h"
#include "vendor/amlogic/AmlogicSocInterface.h"
#include "vendor/brcm/BrcmSocInterface.h"
#include "vendor/realtek/RealtekSocInterface.h"

//static local variable
static std::shared_ptr<SocInterface> socInterface = nullptr;

DefaultSocInterface::DefaultSocInterface()
{
}

std::shared_ptr<SocInterface> SocInterface::CreateSocInterface()
{
        socInterface = std::shared_ptr<SocInterface> (new DefaultSocInterface());
        return socInterface;
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
void DefaultSocInterface::SetAC4Tracks(GstElement *src, int trackId)
{
	g_object_set(src, "ac4-presentation-group-index", trackId, NULL);
}

bool DefaultSocInterface::IsVideoSink(const char* name, bool isRialto)
{
	return  (!mUsingWesterosSink && StartsWith(name, "brcmvideosink") == true) || // brcmvideosink0, brcmvideosink1, ...
        ( mUsingWesterosSink && StartsWith(name, "westerossink") == true) ||
        (isRialto && StartsWith(name, "rialtomsevideosink") == true);

}

/**
 * @brief Check if the given name is a video decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @param isWesteros Westeros flag.
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
 * @param IsWesteros Westeros flag.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool DefaultSocInterface::IsAudioOrVideoDecoder(const char* name, bool IsWesteros)
{
	bool AudioOrVideoDecoder = false;
	if(IsWesteros && StartsWith(name, "westerossink"))
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
/*
void DefaultSocInterface::PrintCdmiDecryptor_Class_Init()
{
#if defined(UBUNTU)
       printf("gst_cdmidecryptor_class_init\n");
#endif
}

void DefaultSocInterface::PrintCdmiDecryptor_Init()
{
#if defined(UBUNTU)
       printf("gst_cdmidecryptor_init\n");
#endif
}
*/

bool DefaultSocInterface::ShouldTearDownForTrickplay()
{
#if defined(__APPLE__) || defined(UBUNTU)
	return true;
#endif
	return false;
}

void SocInterface::SetWesterosSinkState(bool status)
{
	mUsingWesterosSink = status;
}
