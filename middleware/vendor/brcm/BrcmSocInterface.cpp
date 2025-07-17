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

#include "BrcmSocInterface.h"

BrcmSocInterface::BrcmSocInterface()
{
}

/**
 * @brief Get SoC volume property name.
 * @return Volume property name.
 */
void BrcmSocInterface::SetAudioProperty(const char * &volume, const char * &mute, bool& isSinkBinVolume)
{
	volume = "volume";
	mute = "mute";
	isSinkBinVolume = false;	/*volume/mute property should be applied on audio_sink*/
}

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
bool BrcmSocInterface::SetPlaybackRate(const std::vector<GstElement*>& sources, GstElement *pipeline, double rate, GstElement *video_dec, GstElement *audio_dec)
{
	bool status = true;
	MW_LOG_MIL("send custom-instant-rate-change : %f ...", rate);
	GstStructure *structure = gst_structure_new("custom-instant-rate-change", "rate", G_TYPE_DOUBLE, rate, NULL);
	if (!structure)
	{
		MW_LOG_ERR("failed to create custom-instant-rate-change structure");
		return false;
	}
	GstEvent * rate_event = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure);
	if (!rate_event)
	{
		MW_LOG_ERR("failed to create rate_event");
		/* cleanup */
		gst_structure_free (structure);
		return false;
	}
	MW_LOG_MIL("rate_event %p video_decoder %p audio_decoder %p", (void*)rate_event, (void*)video_dec, (void *)audio_dec);
	if(video_dec)
	{
		if (!gst_element_send_event(video_dec,  gst_event_ref(rate_event)))
		{
			MW_LOG_ERR("failed to push rate_event %p to video sink %p", (void*)rate_event, (void*)video_dec);
			status = false;
		}
	}

	if(audio_dec)
	{
		if (!gst_element_send_event(audio_dec,  gst_event_ref(rate_event)))
		{
			MW_LOG_ERR("failed to push rate_event %p to audio decoder %p", (void*)rate_event, (void*)audio_dec);
			status = false;
		}
	}
	// Unref since we have explicitly increased ref count
	gst_event_unref(rate_event);
	MW_LOG_MIL("Current rate: %g", rate);

	return status;
}

/**
 * @brief Get video sink from sinkbin.
 * @param sinkbin The GStreamer sinkbin.
 * @return Video sink element.
 */
GstElement* BrcmSocInterface::GetVideoSink(GstElement* sinkbin)
{
	GstElement* vidsink = nullptr;
	
	if(mUsingWesterosSink)
	{
		MW_LOG_INFO("using westerossink");
		vidsink = gst_element_factory_make("westerossink", NULL);
	}
	else
	{
		vidsink = gst_element_factory_make("brcmvideosink", NULL);
	}

	g_object_set(vidsink, "secure-video", TRUE, NULL);
	g_object_set(sinkbin, "video-sink", vidsink, NULL);
	return vidsink;
}

/**
 * @brief Check if the given name is a video sink.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @return True if it's a video sink, false otherwise.
 */
bool BrcmSocInterface::IsVideoSink(const char* name, bool isRialto)
{
	if(name)
	{
		return  (!mUsingWesterosSink && StartsWith(name, "brcmvideosink") == true) ||
			( mUsingWesterosSink && StartsWith(name, "westerossink") == true) ||
			(isRialto && StartsWith(name, "rialtomsevideosink") == true);
	}
	else
	{
		return false;
	}
}

/**
 * @brief Check if the given name is an audio sink or audio decoder.
 * @param name Element name.
 * @param isRialto Rialto flag.
 * @return True if it's an audio sink or audio decoder, false otherwise.
 */
bool BrcmSocInterface::IsAudioSinkOrAudioDecoder(const char* name, bool isRialto)
{
	if(name)
	{
		return StartsWith(name, "brcmaudiodecoder");
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
bool BrcmSocInterface::IsVideoDecoder(const char* name, bool isRialto)
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
		else
		{
			return StartsWith(name, "brcmvideodecoder");
		}
	}
	else
	{
		return false;
	}
}

bool BrcmSocInterface::ConfigureAudioSink(GstElement **audio_sink, GstObject *src, bool decStreamSync)
{
	bool status = false;
	if ((StartsWith(GST_OBJECT_NAME(src), "brcmaudiosink") == true))
	{
		gst_object_replace((GstObject **)audio_sink, src);
		status = true;
	}
	else if(strstr(GST_OBJECT_NAME(src), "brcmaudiodecoder"))
	{
		// this reduces amount of data in the fifo, which is flushed/lost when transition from expert to normal modes
		g_object_set(src, "limit_buffering_ms", 1500, NULL);   /* default 500ms was a bit low.. try 1500ms */
		g_object_set(src, "limit_buffering", 1, NULL);
		MW_LOG_MIL("Found audiodecoder, limiting audio decoder buffering");

		/* if mAudioDecoderStreamSync==false, tell decoder not to look for 2nd/next frame sync, decode if it finds a single frame sync */
		g_object_set(src, "stream_sync_mode", (decStreamSync)? 1 : 0, NULL);
		MW_LOG_MIL("For audiodecoder set 'stream_sync_mode': %d", decStreamSync);

	}
	return status;
}

/**
 * @brief Check if the given name is an audio or video decoder.
 * @param name Element name.
 * @param IsWesteros Westeros flag.
 * @return True if it's an audio or video decoder, false otherwise.
 */
bool BrcmSocInterface::IsAudioOrVideoDecoder(const char* name, bool isRialto)
{
	bool AudioOrVideoDecoder = false;
	if(name)
	{
		if(!mUsingWesterosSink && StartsWith(name, "brcmvideodecoder"))
		{
			AudioOrVideoDecoder = true;
		}
		else if (StartsWith(name, "brcmaudiodecoder"))
		{
			AudioOrVideoDecoder = true;
		}
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

void BrcmSocInterface::GetCCDecoderHandle(gpointer *dec_handle, GstElement *video_dec)
{
    if (video_dec)
    {
        g_object_get(video_dec, "videodecoder", dec_handle, NULL);
    }
}

/**
 * @brief Set playback flags.
 *
 * Sets the playback flags based on the given parameters.
 * @param flags Reference to the flags integer.
 * @param isSub Flag indicating whether the content is a subtitle.
 */
void BrcmSocInterface::SetPlaybackFlags(gint &flags, bool isSub)
{
	flags = PLAY_FLAG_VIDEO | PLAY_FLAG_AUDIO | PLAY_FLAG_NATIVE_AUDIO | PLAY_FLAG_NATIVE_VIDEO;
	if(isSub)
	{
		flags = PLAY_FLAG_TEXT;
	}
}
