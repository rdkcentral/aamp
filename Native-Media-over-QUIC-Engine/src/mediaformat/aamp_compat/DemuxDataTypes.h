#pragma once

/*
 * Wraps middleware-player-interface's DemuxDataTypes.h without its GstUtils.h
 * dependency, so the CMAF layer compiles AAMP's mp4demux with no GStreamer
 * headers above src/media/gst/. Only the GstStreamOutputFormat enum is needed;
 * it is mirrored here verbatim and the real GstUtils.h is fenced off by its
 * include guard. NMQE_MIDDLEWARE_DEMUX_DATA_TYPES is set by CMake to the
 * absolute path of the real header.
 */

#ifndef GST_UTILS_H
#define GST_UTILS_H

enum GstStreamOutputFormat
{
	GST_FORMAT_INVALID,
	GST_FORMAT_MPEGTS,
	GST_FORMAT_ISO_BMFF,
	GST_FORMAT_AUDIO_ES_MP3,
	GST_FORMAT_AUDIO_ES_AAC,
	GST_FORMAT_AUDIO_ES_AAC_RAW,
	GST_FORMAT_AUDIO_ES_AC3,
	GST_FORMAT_AUDIO_ES_EC3,
	GST_FORMAT_AUDIO_ES_ATMOS,
	GST_FORMAT_AUDIO_ES_AC4,
	GST_FORMAT_VIDEO_ES_H264,
	GST_FORMAT_VIDEO_ES_HEVC,
	GST_FORMAT_VIDEO_ES_MPEG2,
	GST_FORMAT_SUBTITLE_WEBVTT,
	GST_FORMAT_SUBTITLE_TTML,
	GST_FORMAT_SUBTITLE_MP4,
	GST_FORMAT_UNKNOWN
};

#endif

#include NMQE_MIDDLEWARE_DEMUX_DATA_TYPES
