/*
 * Copyright (C) 2022   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "GstreamerCatLog.h"
#include "RialtoGStreamerMSEAudioSink.h"
#include "RialtoGStreamerMSESubtitleSink.h"
#include "RialtoGStreamerMSEVideoSink.h"
#include "RialtoGStreamerWebAudioSink.h"
#include <cstring>
#include <limits>

#ifndef PACKAGE
#define PACKAGE "rialto-gstreamer"
#endif

static gboolean rialto_mse_sinks_init(GstPlugin *plugin)
{
    init_gst_debug_category();

    const char kSrcRev[] = "N";
    const char kTags[] = "N";

    if (std::strlen(kSrcRev) > 0)
    {
        if (std::strlen(kTags) > 0)
        {
            GST_INFO("Release Tag(s): %s (Commit ID: %s)", kTags, kSrcRev);
        }
        else
        {
            GST_INFO("Release Tag(s): No Release Tags! (Commit ID: %s)", kSrcRev);
        }
    }
    else
    {
        GST_WARNING("Failed to get git commit ID!");
    }

    const char *socketPathStr = getenv("RIALTO_SOCKET_PATH");
    guint sinkRank = socketPathStr ? std::numeric_limits<int>::max() : 0;

    const char *sinkRankStr = getenv("RIALTO_SINKS_RANK");
    if (sinkRankStr)
    {
        char *end;
        unsigned long val = strtoul(sinkRankStr, &end, 10);
        if (*end != '\0' || errno == ERANGE)
            GST_WARNING("Failed to parse 'RIALTO_SINKS_RANK' env variable - '%s'", sinkRankStr);
        else
            sinkRank = val;
    }

    if (sinkRank == 0)
    {
        GST_INFO("sinkRank has a value of 0");
        return true;
    }

    GST_INFO("Registering plugins with rank %u", sinkRank);

    return gst_element_register(plugin, "rialtomsevideosink", sinkRank, RIALTO_TYPE_MSE_VIDEO_SINK) &&
           gst_element_register(plugin, "rialtomseaudiosink", sinkRank, RIALTO_TYPE_MSE_AUDIO_SINK) &&
           gst_element_register(plugin, "rialtomsesubtitlesink", sinkRank, RIALTO_TYPE_MSE_SUBTITLE_SINK) &&
           gst_element_register(plugin, "rialtowebaudiosink", sinkRank, RIALTO_TYPE_WEB_AUDIO_SINK);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, rialtosinks, "Sinks which communicate with RialtoServer",
                  rialto_mse_sinks_init, "1.0", "LGPL", PACKAGE, "https://gstreamer.net")
