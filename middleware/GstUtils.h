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
#ifndef GST_UTILS_H
#define GST_UTILS_H

#include <gst/gst.h>
#include "middleware/vendor/SocInterface.h"

/**
 * @enum GstMediaType
 * @brief Media types
 * 
 * @note Please maintain the order video, audio, subtitle, and aux_audio in future.
 * This order is to be maintained across fragment, init, and playlist media types.
 * These enums are used in a lot of calculations in AAMP code and breaking the order will bring a lot of issues.
 * This order is also followed in other enums like AampCurlInstance and TrackType.
 */
enum GstMediaType
{
        eGST_MEDIATYPE_VIDEO,               /**< Type video */
        eGST_MEDIATYPE_AUDIO,               /**< Type audio */
        eGST_MEDIATYPE_SUBTITLE,            /**< Type subtitle */
        eGST_MEDIATYPE_AUX_AUDIO,           /**< Type auxiliary audio */
        eGST_MEDIATYPE_MANIFEST,            /**< Type manifest */
        eGST_MEDIATYPE_LICENCE,             /**< Type license */
        eGST_MEDIATYPE_IFRAME,              /**< Type iframe */
        eGST_MEDIATYPE_INIT_VIDEO,          /**< Type video init fragment */
        eGST_MEDIATYPE_INIT_AUDIO,          /**< Type audio init fragment */
        eGST_MEDIATYPE_INIT_SUBTITLE,       /**< Type subtitle init fragment */
        eGST_MEDIATYPE_INIT_AUX_AUDIO,      /**< Type auxiliary audio init fragment */
        eGST_MEDIATYPE_PLAYLIST_VIDEO,      /**< Type video playlist */
        eGST_MEDIATYPE_PLAYLIST_AUDIO,      /**< Type audio playlist */
        eGST_MEDIATYPE_PLAYLIST_SUBTITLE,   /**< Type subtitle playlist */
        eGST_MEDIATYPE_PLAYLIST_AUX_AUDIO,  /**< Type auxiliary audio playlist */
        eGST_MEDIATYPE_PLAYLIST_IFRAME,     /**< Type Iframe playlist */
        eGST_MEDIATYPE_INIT_IFRAME,         /**< Type IFRAME init fragment */
        eGST_MEDIATYPE_DSM_CC,              /**< Type digital storage media command and control (DSM-CC) */
        eGST_MEDIATYPE_IMAGE,               /**< Type image for thumbnail playlist */
        eGST_MEDIATYPE_DEFAULT              /**< Type unknown */
};

/**
 * @enum GstStreamOutputFormat
 * @brief Stream output formats
 */
enum GstStreamOutputFormat
{
        GST_FORMAT_INVALID,         /**< Invalid format */
        GST_FORMAT_MPEGTS,          /**< MPEG Transport Stream */
        GST_FORMAT_ISO_BMFF,        /**< ISO Base Media File format */
        GST_FORMAT_AUDIO_ES_MP3,    /**< MP3 Audio Elementary Stream */
        GST_FORMAT_AUDIO_ES_AAC,    /**< AAC Audio Elementary Stream */
        GST_FORMAT_AUDIO_ES_AC3,    /**< AC3 Audio Elementary Stream */
        GST_FORMAT_AUDIO_ES_EC3,    /**< Dolby Digital Plus Elementary Stream */
        GST_FORMAT_AUDIO_ES_ATMOS,  /**< ATMOS Audio stream */
        GST_FORMAT_AUDIO_ES_AC4,    /**< AC4 Dolby Audio stream */
        GST_FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
        GST_FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
        GST_FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
        GST_FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
        GST_FORMAT_SUBTITLE_TTML,   /**< TTML subtitle Stream */
        GST_FORMAT_SUBTITLE_MP4,    /**< Generic MP4 stream */
        GST_FORMAT_UNKNOWN          /**< Unknown Format */
};

/**
 * @fn gstGetMediaTypeName
 * @brief Get the name of the media type
 * 
 * @param[in] mediaType The media type
 * @return The name of the media type
 */
const char *gstGetMediaTypeName(GstMediaType mediaType);

/**
 * @fn PlayerCliGstInit
 * @brief Initialize GStreamer for the player CLI
 * 
 * @param[in] argc Pointer to number of args for gst_init
 * @param[in] argv Pointer to array args for gst_init
 */
void PlayerCliGstInit(int *argc, char ***argv);

/**
 * @fn PlayerCliGstTerm
 * @brief Terminate GStreamer for the player CLI
 */
void PlayerCliGstTerm();

/**
 * @fn GetCaps
 * @brief Get the GStreamer capabilities for the given format and platform
 * 
 * @param[in] format The stream output format
 * @return The GStreamer capabilities
 */
GstCaps* GetCaps(GstStreamOutputFormat format);

#endif /* GST_UTILS_H  */
