/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include "GstUtils.h"
#include <inttypes.h>
#include "SocUtils.h"
#include "PlayerUtils.h"

/**
 * @brief Get the GStreamer Caps based on the provided format and platform.
 *
 * @param format The format of the GStreamer stream output.
 * @param platform The platform type for which the caps are being generated.
 * @return GstCaps* A pointer to the GstCaps object describing the capabilities of the media stream.
 */
GstCaps* GetCaps(GstStreamOutputFormat format)
{
	GstCaps * caps = NULL;
	std::shared_ptr<SocInterface> socInterface = SocInterface::CreateSocInterface();

	switch (format)
	{
		case GST_FORMAT_MPEGTS:
			caps = gst_caps_new_simple ("video/mpegts",
					"systemstream", G_TYPE_BOOLEAN, TRUE,
					"packetsize", G_TYPE_INT, 188, NULL);
			break;
		case GST_FORMAT_ISO_BMFF:
			caps = gst_caps_new_simple("video/quicktime", NULL, NULL);
			break;
		case GST_FORMAT_AUDIO_ES_MP3:
			caps = gst_caps_new_simple ("audio/mpeg",
					"mpegversion", G_TYPE_INT, 1, NULL);
			break;
		case GST_FORMAT_AUDIO_ES_AAC:
			caps = gst_caps_new_simple ("audio/mpeg",
					"mpegversion", G_TYPE_INT, 2,
					"stream-format", G_TYPE_STRING, "adts", NULL);
			break;
		case GST_FORMAT_AUDIO_ES_AC3:
			caps = gst_caps_new_simple ("audio/x-ac3", NULL, NULL);
			break;

		case GST_FORMAT_AUDIO_ES_AC4:
			caps = gst_caps_new_simple ("audio/x-ac4", NULL, NULL);
			break;

		case GST_FORMAT_SUBTITLE_TTML:
			caps = gst_caps_new_simple("application/ttml+xml", NULL, NULL);
			break;
		case GST_FORMAT_SUBTITLE_WEBVTT:
			caps = gst_caps_new_simple("text/vtt", NULL, NULL);
			break;
		case GST_FORMAT_SUBTITLE_MP4:
			caps = gst_caps_new_simple("application/mp4", NULL, NULL);
			break;
		case GST_FORMAT_AUDIO_ES_ATMOS:
			// Todo :: a) Test with all platforms if atmos works 
			//	   b) Test to see if x-eac3 config is enough for atmos stream.
			//	 	if x-eac3 is enough then both switch cases can be combined
			caps = gst_caps_new_simple ("audio/x-eac3", NULL, NULL);
                        break;
		case GST_FORMAT_AUDIO_ES_EC3:
			caps = gst_caps_new_simple ("audio/x-eac3", NULL, NULL);
			break;
		case GST_FORMAT_VIDEO_ES_H264:
			caps = gst_caps_new_simple ("video/x-h264", NULL, NULL);
			socInterface->SetH264Caps(caps);
			break;
		case GST_FORMAT_VIDEO_ES_HEVC:
			caps = gst_caps_new_simple("video/x-h265", NULL, NULL);
			socInterface->SetHevcCaps(caps);
			break;
		case GST_FORMAT_VIDEO_ES_MPEG2:
			caps = gst_caps_new_simple ("video/mpeg",
					"mpegversion", G_TYPE_INT, 2,
					"systemstream", G_TYPE_BOOLEAN, FALSE, NULL);
			break;  //CID:81305 - Using break statement
		case GST_FORMAT_UNKNOWN:
			g_print("Unknown format %d", format);
			break;
		case GST_FORMAT_INVALID:
		default:
			g_print("Unsupported format %d", format);
			break;
	}
	return caps;
}

/**
 * @brief Initialize the GStreamer library for the player CLI.
 * @param argc A pointer to the argument count.
 * @param argv A pointer to the argument vector.
 */

void PlayerCliGstInit(int *argc, char ***argv)
{
	gst_init(argc,argv);
}

/**
 * @brief Terminate the GStreamer library for the player CLI.
 */
void PlayerCliGstTerm()
{
	gst_deinit();
}

