/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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

/**
 * @file AampGstUtils.cpp
 * @brief Impl for utility functions defined in AampGstUtils.h
 */

#include "AampGstUtils.h"
#include "priv_aamp.h" // Included for AAMPLOG
//TODO: Fix cyclic dependency btw GlobalConfig and AampLogManager

/**
 * @brief Parse format to generate GstCaps
 */
GstCaps* GetGstCaps(StreamOutputFormat format, PlatformType platform)
{
	GstCaps * caps = NULL;
	switch (format)
	{
		case FORMAT_MPEGTS:
			caps = gst_caps_new_simple ("video/mpegts", "systemstream", G_TYPE_BOOLEAN, TRUE, "packetsize", G_TYPE_INT, 188, NULL);
			break;
			
		case FORMAT_ISO_BMFF:
			caps = gst_caps_new_simple("video/quicktime", NULL, NULL);
			break;
			
		case FORMAT_AUDIO_ES_MP3:
			caps = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 1, NULL);
			break;
			
		case FORMAT_AUDIO_ES_AAC:
			caps = gst_caps_new_simple ("audio/mpeg", "mpegversion", G_TYPE_INT, 2, "stream-format", G_TYPE_STRING, "adts", NULL);
			break;
			
		case FORMAT_AUDIO_ES_AC3:
			caps = gst_caps_new_simple ("audio/x-ac3", NULL, NULL);
			break;

		case FORMAT_AUDIO_ES_AC4:
			caps = gst_caps_new_simple ("audio/x-ac4", NULL, NULL);
			break;

		case FORMAT_SUBTITLE_TTML:
			caps = gst_caps_new_simple("application/ttml+xml", NULL, NULL);
			break;
			
		case FORMAT_SUBTITLE_WEBVTT:
			caps = gst_caps_new_simple("text/vtt", NULL, NULL);
			break;
			
		case FORMAT_SUBTITLE_MP4:
			caps = gst_caps_new_simple("application/mp4", NULL, NULL);
			break;
			
		case FORMAT_AUDIO_ES_ATMOS:
		case FORMAT_AUDIO_ES_EC3:
			caps = gst_caps_new_simple("audio/x-eac3", NULL, NULL );
			break;
			
		case FORMAT_VIDEO_ES_H264:
			caps = gst_caps_new_simple("video/x-h264", NULL, NULL);
			if( platform == ePLATFORM_REALTEK )
			{
				gst_caps_set_simple (caps, "enable-fastplayback", G_TYPE_STRING, "true", NULL);
			}
#ifdef UBUNTU
			// below required on Ubuntu - harmless on OSX, but breaks RPI
			gst_caps_set_simple (caps,
								"alignment", G_TYPE_STRING, "au",
								"stream-format", G_TYPE_STRING, "avc",
								NULL);
#endif
			break;
			
		case FORMAT_VIDEO_ES_HEVC:
			caps = gst_caps_new_simple("video/x-h265", NULL, NULL );
			if( platform == ePLATFORM_REALTEK )
			{
				gst_caps_set_simple (caps, "enable-fastplayback", G_TYPE_STRING, "true", NULL);
			}
#ifdef UBUNTU
			// below required on Ubuntu - harmless on OSX, but breaks RPI
			gst_caps_set_simple(caps,
								"alignment", G_TYPE_STRING, "au",
								"stream-format", G_TYPE_STRING, "hev1",
								NULL);
#endif
			break;
			
		case FORMAT_VIDEO_ES_MPEG2:
			caps = gst_caps_new_simple("video/mpeg", "mpegversion", G_TYPE_INT, 2, "systemstream", G_TYPE_BOOLEAN, FALSE, NULL );
			break;
			
		case FORMAT_UNKNOWN:
			AAMPLOG_WARN("Unknown format %d", format);
			break;
			
		case FORMAT_INVALID:
		default:
			AAMPLOG_WARN("Unsupported format %d", format);
			break;
	}
	return caps;
}

