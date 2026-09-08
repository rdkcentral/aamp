#include "SocInterface.h"

// Fake implementation of DefaultSocInterface for unit tests
// Note: vendor-specific headers are internal to middleware and not exposed by external middleware
class DefaultSocInterface : public SocInterface
{
public:
	DefaultSocInterface();
	bool UseAppSrc() override;
	void SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume) override;
	bool IsVideoSink(const char* name) override;
	bool IsVideoDecoder(const char* name) override;
	bool IsAudioOrVideoDecoder(const char* name) override;
	void SetPlaybackFlags(gint &flags, bool isSub) override;
	bool IsSimulatorFirstFrame() override;
	bool IsSimulatorSink() override;
	void ConfigurePluginPriority() override;
	bool IsSimulatorVideoSample() override;
	void SetH264Caps(GstCaps *caps) override;
	void SetHevcCaps(GstCaps *caps) override;
	bool ConfigureAudioSink(GstElement **audio_sink, GstObject *src, bool decStreamSync) override;
	bool ShouldTearDownForTrickplay() override;
	
	// Additional pure virtual methods from SocInterface
	bool SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec) override { return false; }
	bool SetRateCorrection() override { return false; }
	bool IsAudioSinkOrAudioDecoder(const char* name) override { return false; }
	void GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec) override {}
	bool IsVideoMaster(GstElement *videoSink) override { return false; }
};

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
	return false;
}

/**
 * @brief Check if the given name is a video decoder.
 * @param name Element name.
 * @param isWesteros Westeros flag.
 * @return True if it's a video decoder, false otherwise.
 */
bool DefaultSocInterface::IsVideoDecoder(const char* name)
{
	return false;
}

/**
 * @brief Check if the given name is an audio or video decoder.
 * @param name Element name.
 * @param IsWesteros Westeros flag.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool DefaultSocInterface::IsAudioOrVideoDecoder(const char* name)
{
	return false;
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

long long SocInterface::ReadVideoPts(GstElement */*element*/)
{
	return 0;
}

long long SocInterface::GetVideoPts(GstElement */*video_sink*/, GstElement */*video_dec*/, bool /*isWesteros*/)
{
	return 0;
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

void SocInterface::CheckVideoPtsPropertySupport(GstElement */*element*/)
{
}

void SocInterface::DiscoverVideoDecoderProperties(GstElement */*element*/)
{
}

void SocInterface::DiscoverVideoSinkProperties(GstElement */*element*/)
{
}
