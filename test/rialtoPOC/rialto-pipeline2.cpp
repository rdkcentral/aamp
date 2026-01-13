/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include "rialto-pipeline2.h"
#include <assert.h>
#include <inttypes.h>
#include <gst/app/gstappsrc.h>

static const int32_t TRACK_VIDEO = 0;
static const int32_t TRACK_AUDIO = 1;

static void found_video_source_cb( GObject * object, GObject * orig, GParamSpec * pspec, class GstMediaPipeline *gstMediaPipeline )
{
	printf( "found_video_source_cb\n" );
	gstMediaPipeline->found_source(orig,pspec,TRACK_VIDEO);
}

static void found_audio_source_cb( GObject * object, GObject * orig, GParamSpec * pspec, class GstMediaPipeline *gstMediaPipeline )
{
	printf( "found_audio_source_cb\n" );
	gstMediaPipeline->found_source(orig,pspec,TRACK_AUDIO);
}

void GstMediaPipeline::found_source( GObject *orig, GParamSpec *pspec, int sourceId )
{
	g_object_get( orig, pspec->name, &track[sourceId].appsrc, NULL );
}

bool GstMediaPipeline::attachSource(const std::unique_ptr<MediaSource> &source, int32_t &sourceId ){
	MediaSourceType sourceType = source->getType();
	GstCaps * caps = gst_caps_new_empty_simple( source->getMimeType().c_str() );
	
	if( sourceType == MediaSourceType::VIDEO || sourceType == MediaSourceType::AUDIO )
	{
		const IMediaPipeline::MediaSourceAV *mediaSourceAV = dynamic_cast<IMediaPipeline::MediaSourceAV *>(source.get());
		if( mediaSourceAV )
		{
			const char *streamFormat = nullptr;
			switch( mediaSourceAV->getStreamFormat() )
			{
				case StreamFormat::HVC1:
					streamFormat = "hvc1";
					break;
				case StreamFormat::AVC:
					streamFormat = "avc";
					break;
				case StreamFormat::RAW:
					streamFormat = "raw";
					break;
				default:
					break;
			}
			if( streamFormat )
			{
				gst_caps_set_simple( caps, "stream-format", G_TYPE_STRING, streamFormat, nullptr );
			}
			const std::shared_ptr<CodecData> codecData = mediaSourceAV->getCodecData();
			GstBuffer *buf = nullptr;
			if( codecData )
			{
				buf = gst_buffer_new_and_alloc(codecData->data.size());
				gst_buffer_fill(buf, 0, codecData->data.data(), codecData->data.size() );
				gst_caps_set_simple( caps, "codec_data", GST_TYPE_BUFFER, buf, nullptr );
			}
			
			if( sourceType == MediaSourceType::VIDEO )
			{
				sourceId = TRACK_VIDEO;
				const IMediaPipeline::MediaSourceVideo *mediaSourceVideo = dynamic_cast<IMediaPipeline::MediaSourceVideo *>(source.get());
				if( mediaSourceVideo )
				{
					gst_caps_set_simple( caps,
										"alignment", G_TYPE_STRING, "au",
										"width", G_TYPE_INT, mediaSourceVideo->getWidth(),
										"height", G_TYPE_INT, mediaSourceVideo->getHeight(),
										"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
										nullptr);
				}
			}
			else if( sourceType == MediaSourceType::AUDIO )
			{
				sourceId = TRACK_AUDIO;
				const IMediaPipeline::MediaSourceAudio *mediaSourceAudio = dynamic_cast<IMediaPipeline::MediaSourceAudio *>(source.get());
				if( mediaSourceAudio )
				{
					const auto &audioConfig = mediaSourceAudio->getAudioConfig();
					gst_caps_set_simple( caps,
										"framed", G_TYPE_BOOLEAN, TRUE,
										"rate", G_TYPE_INT, audioConfig.sampleRate,
										"channels", G_TYPE_INT, audioConfig.numberOfChannels,
										nullptr );
				}
			}
			else
			{
				assert(0);
			}
			
			gchar *caps_string = gst_caps_to_string(caps);
			g_print("Negotiated caps: %s\n", caps_string);
			g_free(caps_string);
			
			gst_app_src_set_caps(GST_APP_SRC(track[sourceId].appsrc), caps);
			gst_caps_unref(caps);
			if( buf )
			{
				gst_buffer_unref(buf);
			}
		}
		printf( "attachSource() -> sourceId=%" PRId32 "\n", sourceId );
	}
	return true;
};

bool GstMediaPipeline::removeSource(int32_t id){
	printf( "removeSource(sourceId=%" PRId32 ")\n", id );
	return false;
};

bool GstMediaPipeline::allSourcesAttached(){
	return true;
};

bool GstMediaPipeline::load(MediaType type, const std::string &mimeType, const std::string &url){ return true; }

bool GstMediaPipeline::play(){
	printf( "play\n" );
	gst_element_set_state( pipeline, GST_STATE_PLAYING );
	return true;
};

AddSegmentStatus GstMediaPipeline::addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment)
{
	GstBuffer *gstBuffer = gst_buffer_new_wrapped(
												  (gpointer)mediaSegment->getData(),
												  (gsize)mediaSegment->getDataLength() );
	GST_BUFFER_PTS(gstBuffer) = (GstClockTime)(mediaSegment->getTimeStamp());
	GST_BUFFER_DURATION(gstBuffer) = (GstClockTime)(mediaSegment->getDuration());
	gst_app_src_push_buffer(GST_APP_SRC(track[mediaSegment->getId()].appsrc), gstBuffer );
	return AddSegmentStatus::OK;
}

GstMediaPipeline::GstMediaPipeline()
{
    printf("constructing GstMediaPipeline\n");
    pipeline = gst_pipeline_new("rialtoTest");

    for (int i = 0; i < 2; i++)
    {
        printf("creating playbin for track#%d\n", i);

        GstElement* playbin = gst_element_factory_make("playbin", nullptr);
        assert(playbin);

        track[i].playbin = playbin;
        track[i].appsrc = nullptr;

        gboolean rc = gst_bin_add(GST_BIN(pipeline), playbin);
        assert(rc);

        g_object_set(playbin, "uri", "appsrc://", nullptr);

        if (i == TRACK_VIDEO) {
            GstElement* videoSink = gst_element_factory_make("rialtomsevideosink", "video-sink");
            assert(videoSink);
            g_object_set(playbin, "video-sink", videoSink, nullptr);
            g_signal_connect(playbin, "deep-notify::source",
                             G_CALLBACK(found_video_source_cb), this);
        } else {
            GstElement* audioSink = gst_element_factory_make("rialtomseaudiosink", "audio-sink");
            assert(audioSink);
            g_object_set(playbin, "audio-sink", audioSink, nullptr);
            g_signal_connect(playbin, "deep-notify::source",
                             G_CALLBACK(found_audio_source_cb), this);
        }
    }
}


GstMediaPipeline::~GstMediaPipeline()
{
	printf( "destructing GstMediaPipeline\n" );
}