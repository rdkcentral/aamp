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

#include "RealtekSocInterface.h"

#define DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS 12	/**< Currently MAX FRAG DURATION + 2 per Realtek */

RealtekSocInterface::RealtekSocInterface()
{
}

/**
 * Sets buffer size and duration for the given GstElement.
 *
 * @param sink The GstElement to configure.
 * @param size The desired buffer size.
 */
void RealtekSocInterface::SetVideoBufferSize(GstElement *sink, int size)
{
	g_object_set(sink, "buffer-size", (guint64)size, NULL);
	g_object_set(sink, "buffer-duration", 3000000000, NULL); //3000000000(ns), 3s
}

/**
 * Sets asynchronous mode for the given Sink.
 *
 * @param sink element.
 * @param status Enable (TRUE) or disable (FALSE) asynchronous mode.
 */
void RealtekSocInterface::SetSinkAsync(GstElement *sink, gboolean status)
{
	gst_base_sink_set_async_enabled(GST_BASE_SINK(sink), status);
}

/**
 * @brief Get SoC volume property name.
 * @return Volume property name.
 */
void RealtekSocInterface::SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume)
{
	volume = "volume";
	mute = "mute";
	isSinkBinVolume = true;		/*volume/mute property should be applied on sinkbin*/
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
bool RealtekSocInterface::SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec, bool isRialto)
{
    //SOC specific code will  be executed for rialto sink at default soc 
	if(!pipeline)
	{
		MW_LOG_ERR("Failed to set playback rate");
		return false;
	}
	MW_LOG_MIL("=send custom-instant-rate-change : %f ...", rate);
	GstStructure *structure = gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, rate, NULL);
	if(!structure)
	{
		MW_LOG_ERR("Failed to create custom-instant-rate-change structure");
		return false;
	}
	/* The above statement creates a new GstStructure with the name
	   'custom-instant-rate-change' that has a member variable
	   'rate' of G_TYPE_DOUBLE and a value of rate i.e. second last parameter */
	GstEvent * rate_event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure);
	if (!rate_event)
	{
		MW_LOG_ERR("Failed to create rate_event");
		gst_structure_free (structure);
		return false;
	}
	int ret = gst_element_send_event(pipeline, rate_event );
	if(!ret)
	{
		MW_LOG_ERR("Rate change failed : %g [gst_element_send_event]", rate);
		return false;
	}
	MW_LOG_MIL("Current rate: %g", rate);
	return true;
}

/**
 * @brief Set AC4 tracks.
 * @param src Source element.
 * @param trackId Track ID.
 */
void RealtekSocInterface::SetAC4Tracks(GstElement *src, int trackId)
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
bool RealtekSocInterface::IsVideoSink(const char* name, bool isRialto)
{
	if(name)
		return (StartsWith(name, "westerossink") || StartsWith(name, "rtkv1sink") || (isRialto && StartsWith(name, "rialtomsevideosink") == true));
	else
		return false;
}

/**
 * @brief Check if the given name is an audio sink or audio decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @return True if it's an audio sink or audio decoder, false otherwise.
 */
bool RealtekSocInterface::IsAudioSinkOrAudioDecoder(const char* name, bool isRialto)
{
	if(name)
	{
		return (StartsWith(name, "rtkaudiosink")
				|| StartsWith(name, "alsasink")
				|| StartsWith(name, "fakesink")
				|| (isRialto && StartsWith(name, "rialtomseaudiosink") == true));
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
 * @return True if it's a video decoder, false otherwise.
 */
bool RealtekSocInterface::IsVideoDecoder(const char* name, bool isRialto)
{
	if(name)
	{
		return (StartsWith(name, "omxwmvdec") || StartsWith(name, "omxh26")
				|| StartsWith(name, "omxav1dec") || StartsWith(name, "omxvp") || StartsWith(name, "omxmpeg"));
	}
	else
	{
		return false;
	}
}

/**
 * @brief Configure the audio sink.
 * @param audio_sink Pointer to the audio sink element.
 * @param src Source object.
 * @param decStreamSync Decoder stream synchronization flag.
 * @return True on success, false otherwise.
 */
bool RealtekSocInterface::ConfigureAudioSink(GstElement **audio_sink, GstObject *src, bool decStreamSync)
{
	if (!audio_sink || !src)
	{
		MW_LOG_ERR("ConfigureAudioSink: Invalid input parameters");
		return false;
	}

	bool status = false;
	const char* srcName = GST_OBJECT_NAME(src);

	if (srcName && (StartsWith(srcName, "rtkaudiosink")
				|| StartsWith(srcName, "alsasink")
				|| StartsWith(srcName, "fakesink")))
	{
		gst_object_replace(reinterpret_cast<GstObject **>(audio_sink), src);
		status = true;
	}

	return status;
}

/**
 * @brief Check if the given name is an audio or video decoder.
 * @param name Element name.
 * @param IsWesteros Westeros flag.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool RealtekSocInterface::IsAudioOrVideoDecoder(const char* name, bool isRialto)
{
	bool AudioOrVideoDecoder = false;
	if(name)
	{
		if(StartsWith(name, "omx"))
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
 * @brief Disable asynchronous audio.
 * @param audio_sink Audio sink element.
 * @param rate Playback rate.
 * @param isSeeking True if seeking is in progress, false otherwise.
 * @return True if async changed from enabled to disabled, false otherwise.
 */
bool RealtekSocInterface::DisableAsyncAudio(GstElement *audio_sink, int rate, bool isSeeking)
{
	bool bAsyncModify = false;
	if (audio_sink)
	{
		if (rate > 1 || rate < 0 || isSeeking)
		{
			MW_LOG_MIL("Disable async for audio stream at trickplay");
			if (gst_base_sink_is_async_enabled(GST_BASE_SINK(audio_sink)) == TRUE)
			{
				gst_base_sink_set_async_enabled(GST_BASE_SINK(audio_sink), FALSE);
				bAsyncModify = true;
			}
		}
	}
	return bAsyncModify;
}

/**
 * Gets the decoder handle from the video decoder element.
 *
 * @param dec_handle Pointer to store the decoder handle.
 * @param video_dec The video decoder element.
 */
void RealtekSocInterface::GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)
{
	*dec_handle = video_dec; // Realtek directly returns the decoder element
}

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
long long RealtekSocInterface::GetVideoPts(GstElement *video_sink, GstElement *video_dec, bool isWesteros)
{
	gint64 currentPTS = 0;
	GstElement *element = nullptr;
	if(video_sink)
	{
		element = video_sink;
	}

	if (element)
	{
		g_object_get(element, "video-pts", &currentPTS, NULL);	/* Gets the 'video-pts' from the element into the currentPTS */
	}
	return (long long)currentPTS;
}

/**
 * @brief Set freerun threshold on source.
 *
 * Sets the freerun threshold on the given source object.
 *
 * @param src The source object.
 */
void RealtekSocInterface::SetFreerunThreshold(GstObject* src)
{
	if(src)
	{
		g_object_set(src, "freerun-threshold", DEFAULT_AVSYNC_FREERUN_THRESHOLD_SECS, NULL);
	}
}

/**
 * @brief Set audio routing properties on source.
 *
 * Sets audio routing properties on the given source element.
 *
 * @param source The source element.
 */
void RealtekSocInterface::SetAudioRoutingProperties(GstElement *source)
{
	if ((strstr(GST_ELEMENT_NAME(source), "omxaacdec") != NULL) ||
			(strstr(GST_ELEMENT_NAME(source), "omxac3dec") != NULL) ||
			(strstr(GST_ELEMENT_NAME(source), "omxeac3dec") != NULL) ||
			(strstr(GST_ELEMENT_NAME(source), "omxmp3dec") != NULL) ||
			(strstr(GST_ELEMENT_NAME(source), "omxvorbisdec") != NULL) ||
			(strstr(GST_ELEMENT_NAME(source), "omxac4dec") != NULL))
	{
		g_object_set(source, "audio-tunnel-mode", FALSE, NULL );
		MW_LOG_INFO("callback_element_added audio-tunnel-mode FALSE");
		g_object_set(source, "aux-audio", TRUE, NULL );
		MW_LOG_INFO("callback_element_added aux-audio TRUE");
	}
}

/**
 * @brief Set playback flags.
 *
 * Sets the playback flags based on the given parameters.
 * @param flags Reference to the flags integer.
 * @param isSub Flag indicating whether the content is a subtitle.
 */
void RealtekSocInterface::SetPlaybackFlags(gint &flags, bool isSub)
{
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_NATIVE_AUDIO | PLAY_FLAG_NATIVE_VIDEO;

	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO |  PLAY_FLAG_NATIVE_AUDIO | PLAY_FLAG_NATIVE_VIDEO | PLAY_FLAG_SOFT_VOLUME;
	if(isSub)
	{
		flags = PLAY_FLAG_TEXT;
	}
}

/**
 *@brief Sets the platform specific H264 caps
 */
void RealtekSocInterface::SetH264Caps(GstCaps *caps)
{
	gst_caps_set_simple (caps, "enable-fastplayback", G_TYPE_STRING, "true", NULL);
}


/**
 * @brief Get video sink from sinkbin.
 * @param sinkbin The GStreamer sinkbin.
 */
GstElement* RealtekSocInterface::GetVideoSink(GstElement* sinkbin)
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

void RealtekSocInterface::SetHevcCaps(GstCaps *caps)
{
	gst_caps_set_simple (caps, "enable-fastplayback", G_TYPE_STRING, "true", NULL);
}
