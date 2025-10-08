/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#ifndef STREAM_OUTPUT_FORMAT_H
#define STREAM_OUTPUT_FORMAT_H

/**
 * @enum StreamOutputFormat
 * @brief Media output format
 */
enum StreamOutputFormat
{
    FORMAT_INVALID,         /**< Invalid format */
    FORMAT_MPEGTS,          /**< MPEG Transport Stream */
    FORMAT_ISO_BMFF,        /**< ISO Base Media File format */
    FORMAT_AUDIO_ES_MP3,    /**< MP3 Audio Elementary Stream */
    FORMAT_AUDIO_ES_AAC,    /**< AAC Audio Elementary Stream */
    FORMAT_AUDIO_ES_AC3,    /**< AC3 Audio Elementary Stream */
    FORMAT_AUDIO_ES_EC3,    /**< Dolby Digital Plus Elementary Stream */
    FORMAT_AUDIO_ES_ATMOS,  /**< ATMOS Audio stream */
    FORMAT_AUDIO_ES_AC4,    /**< AC4 Dolby Audio stream */
    FORMAT_VIDEO_ES_H264,   /**< MPEG-4 Video Elementary Stream */
    FORMAT_VIDEO_ES_HEVC,   /**< HEVC video elementary stream */
    FORMAT_VIDEO_ES_MPEG2,  /**< MPEG-2 Video Elementary Stream */
    FORMAT_SUBTITLE_WEBVTT, /**< WebVTT subtitle Stream */
    FORMAT_SUBTITLE_TTML,     /**< WebVTT subtitle Stream */
    FORMAT_SUBTITLE_MP4,     /**< Generic MP4 stream */
    FORMAT_UNKNOWN          /**< Unknown Format */
};

#endif // STREAM_OUTPUT_FORMAT_H
