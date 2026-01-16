
/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
#include "rialto-pipeline.h"

#include <assert.h>
#include <inttypes.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <atomic>
#include <cstdio>

using namespace firebolt::rialto;

// -----------------------------------------------------------------------------
// Track IDs (fixed mapping for test compatibility)
// -----------------------------------------------------------------------------
static const int32_t TRACK_VIDEO = 0;
static const int32_t TRACK_AUDIO = 1;

// -----------------------------------------------------------------------------
// Per‑source state (pipeline3‑compatible)
// -----------------------------------------------------------------------------
struct SourceState
{
	bool     outstanding{false};
	uint32_t requestId{0};
};

static std::atomic<uint32_t> g_nextRequestId{1};

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
static void found_video_source_cb(GObject *, GObject *, GParamSpec *, class GstMediaPipeline *);
static void found_audio_source_cb(GObject *, GObject *, GParamSpec *, class GstMediaPipeline *);
static void on_need_data_cb(GstAppSrc *, guint, gpointer);

// -----------------------------------------------------------------------------
// GStreamer callbacks
// -----------------------------------------------------------------------------
static void found_video_source_cb(GObject *, GObject *orig, GParamSpec *pspec,
								  class GstMediaPipeline *pipeline)
{
	pipeline->found_source(orig, pspec, TRACK_VIDEO);
}

static void found_audio_source_cb(GObject *, GObject *orig, GParamSpec *pspec,
								  class GstMediaPipeline *pipeline)
{
	pipeline->found_source(orig, pspec, TRACK_AUDIO);
}

static void on_need_data_cb(GstAppSrc *src, guint /*length*/, gpointer user_data)
{
	auto *pipeline = static_cast<GstMediaPipeline *>(user_data);

	int sourceId =
		(src == GST_APP_SRC(pipeline->track[TRACK_VIDEO].appsrc))
			? TRACK_VIDEO
			: TRACK_AUDIO;

	auto &state = pipeline->m_sourceState[sourceId];
	if (state.outstanding)
		return;

	uint32_t reqId = g_nextRequestId.fetch_add(1);
	state.outstanding = true;
	state.requestId = reqId;

	auto client = pipeline->getClient().lock();
	if (client)
	{
		client->notifyNeedMediaData(
			sourceId,
			0 /* frameCount */,
			reqId,
			nullptr /* shmInfo */);
	}
}

// -----------------------------------------------------------------------------
// GstMediaPipeline implementation
// -----------------------------------------------------------------------------
GstMediaPipeline::GstMediaPipeline()
{
	printf("constructing GstMediaPipeline (appsrc-backed)\n");

	pipeline = gst_pipeline_new("rialtoTest");
	assert(pipeline);

	for (int i = 0; i < 2; ++i)
	{
		m_sourceState[i] = {};

		GstElement *playbin = gst_element_factory_make("playbin", nullptr);
		assert(playbin);

		track[i].playbin = playbin;
		track[i].appsrc  = nullptr;

		gst_bin_add(GST_BIN(pipeline), playbin);
		g_object_set(playbin, "uri", "appsrc://", nullptr);

		if (i == TRACK_VIDEO)
		{
			GstElement *videoSink =
				gst_element_factory_make("rialtomsevideosink", "video-sink");
			assert(videoSink);

			g_object_set(playbin, "video-sink", videoSink, nullptr);
			g_signal_connect(playbin, "deep-notify::source",
							 G_CALLBACK(found_video_source_cb), this);
		}
		else
		{
			GstElement *audioSink =
				gst_element_factory_make("rialtomseaudiosink", "audio-sink");
			assert(audioSink);

			g_object_set(playbin, "audio-sink", audioSink, nullptr);
			g_signal_connect(playbin, "deep-notify::source",
							 G_CALLBACK(found_audio_source_cb), this);
		}
	}
}

GstMediaPipeline::~GstMediaPipeline()
{
	printf("destructing GstMediaPipeline\n");
}

void GstMediaPipeline::found_source(GObject *orig, GParamSpec *pspec, int sourceId)
{
	g_object_get(orig, pspec->name, &track[sourceId].appsrc, nullptr);

	GstAppSrc *appsrc = GST_APP_SRC(track[sourceId].appsrc);
	gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_STREAM);

	g_signal_connect(appsrc, "need-data",
					 G_CALLBACK(on_need_data_cb), this);
}

bool GstMediaPipeline::play()
{
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	return true;
}

bool GstMediaPipeline::haveData(MediaSourceStatus, uint32_t requestId)
{
	for (int i = 0; i < 2; ++i)
	{
		auto &state = m_sourceState[i];
		if (state.outstanding && state.requestId == requestId)
		{
			state.outstanding = false;
			state.requestId   = 0;
			return true;
		}
	}
	return false;
}

AddSegmentStatus
GstMediaPipeline::addSegment(uint32_t requestId,
							 const std::unique_ptr<MediaSegment> &segment)
{
	int32_t sourceId = segment->getId();
	auto &state = m_sourceState[sourceId];

	if (!state.outstanding || state.requestId != requestId)
	{
		printf("addSegment rejected (src=%" PRId32 ", req=%" PRIu32 ")\n",
			   sourceId, requestId);
		return AddSegmentStatus::ERROR;
	}

	GstBuffer *buf = gst_buffer_new_wrapped(
		const_cast<uint8_t *>(segment->getData()),
		segment->getDataLength());

	GST_BUFFER_PTS(buf) =
		static_cast<GstClockTime>(segment->getTimeStamp());
	GST_BUFFER_DURATION(buf) =
		static_cast<GstClockTime>(segment->getDuration());

	gst_app_src_push_buffer(
		GST_APP_SRC(track[sourceId].appsrc), buf);

	return AddSegmentStatus::OK;
}
