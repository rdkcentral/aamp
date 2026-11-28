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

/**
 * @file GstReferencePipeline.cpp
 * @brief GStreamer reference implementation of firebolt::rialto::IMediaPipeline.
 *
 * One GstPipeline, one branch per attached source:
 *
 *   video:    appsrc ! decodebin ! queue ! videoconvert ! videobalance ! <video sink>
 *   audio:    appsrc ! decodebin ! queue ! audioconvert ! audioresample ! volume ! <audio sink>
 *   subtitle: appsrc ! fakesink (data is paced and accounted for, not rendered)
 *
 * Behaviour follows the Rialto server (media/server/gstplayer):
 *  - appsrc need-data/enough-data drive notifyNeedMediaData; segments staged
 *    by addSegment() are pushed when haveData() completes the request.
 *  - setPosition() is a flushing seek of every source: SEEKING, flush, a new
 *    segment at the position, SEEK_DONE, then fresh data requests.
 *  - setSourcePosition() queues a segment that is pushed ahead of the
 *    source's next buffers; flush() flushes one source.
 *  - Buffers carry the client's timestamps unchanged; segments map them to
 *    running time, so getPosition() reports on the client's timeline.
 *  - END_OF_STREAM is reported when every branch has drained (pipeline EOS).
 *
 * Threads: GStreamer streaming threads never take m_mutex; they post to
 * m_worker (bus messages, timers, need-data). Client callbacks are delivered
 * in order on m_events and never while m_mutex is held.
 */

#include "rialto-reference/GstReferencePipeline.h"

#include "Executor.h"
#include "MediaCaps.h"
#include "ReferenceLog.h"

#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>

#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rialto_reference
{

using namespace firebolt::rialto;

namespace
{

constexpr uint32_t kFramesPerRequest = 24;
constexpr auto kPositionReportInterval = std::chrono::milliseconds(250);
constexpr auto kNeedDataRetryDelay = std::chrono::milliseconds(50);
constexpr guint64 kVideoAppSrcMaxBytes = 8 * 1024 * 1024;
constexpr guint64 kAudioAppSrcMaxBytes = 512 * 1024;
constexpr guint64 kSubtitleAppSrcMaxBytes = 256 * 1024;
constexpr guint kAppSrcMinPercent = 20;
constexpr guint64 kAppSrcMaxTime = 1 * GST_SECOND;
constexpr guint64 kAppSrcMaxBuffers = 250;

const char *toString(MediaSourceType type)
{
	switch (type)
	{
		case MediaSourceType::AUDIO:
			return "audio";
		case MediaSourceType::VIDEO:
			return "video";
		case MediaSourceType::SUBTITLE:
			return "subtitle";
		default:
			return "unknown";
	}
}

const char *toString(PlaybackState state)
{
	switch (state)
	{
		case PlaybackState::IDLE:
			return "IDLE";
		case PlaybackState::PLAYING:
			return "PLAYING";
		case PlaybackState::PAUSED:
			return "PAUSED";
		case PlaybackState::SEEKING:
			return "SEEKING";
		case PlaybackState::SEEK_DONE:
			return "SEEK_DONE";
		case PlaybackState::STOPPED:
			return "STOPPED";
		case PlaybackState::END_OF_STREAM:
			return "END_OF_STREAM";
		case PlaybackState::FAILURE:
			return "FAILURE";
		default:
			return "UNKNOWN";
	}
}

const char *toString(MediaSourceStatus status)
{
	switch (status)
	{
		case MediaSourceStatus::OK:
			return "OK";
		case MediaSourceStatus::EOS:
			return "EOS";
		case MediaSourceStatus::ERROR:
			return "ERROR";
		case MediaSourceStatus::CODEC_CHANGED:
			return "CODEC_CHANGED";
		case MediaSourceStatus::NO_AVAILABLE_SAMPLES:
			return "NO_AVAILABLE_SAMPLES";
		default:
			return "UNKNOWN";
	}
}

GstElement *makeElement(const char *factory)
{
	GstElement *element = gst_element_factory_make(factory, nullptr);
	if (!element)
	{
		RREF_LOG("missing GStreamer element '%s'", factory);
	}
	return element;
}

/** Sink from an environment override (gst-launch syntax), else the default factory. */
GstElement *makeSink(const char *envName, const char *defaultFactory)
{
	const char *description = std::getenv(envName);
	if (!description || !*description)
	{
		return makeElement(defaultFactory);
	}
	GError *error = nullptr;
	GstElement *sink = gst_parse_bin_from_description(description, TRUE, &error);
	if (!sink)
	{
		RREF_LOG("%s='%s' invalid: %s", envName, description, error ? error->message : "unknown error");
		g_clear_error(&error);
		return makeElement(defaultFactory);
	}
	RREF_LOG("using %s='%s'", envName, description);
	return sink;
}

template <typename Visit> void forEachBaseSink(GstElement *element, Visit visit)
{
	if (GST_IS_BASE_SINK(element))
	{
		visit(element);
		return;
	}
	if (!GST_IS_BIN(element))
	{
		return;
	}
	GstIterator *it = gst_bin_iterate_recurse(GST_BIN(element));
	GValue item = G_VALUE_INIT;
	while (gst_iterator_next(it, &item) == GST_ITERATOR_OK)
	{
		auto *child = GST_ELEMENT(g_value_get_object(&item));
		if (GST_IS_BASE_SINK(child))
		{
			visit(child);
		}
		g_value_reset(&item);
	}
	g_value_unset(&item);
	gst_iterator_free(it);
}

struct SegmentData
{
	int64_t position;
	bool resetTime;
	double appliedRate;
	uint64_t stopPosition;
};

struct StagedSample
{
	GstBuffer *buffer;
	GstCaps *caps; // caps to apply before this buffer, or nullptr
};

class GstReferencePipeline;

struct Stream
{
	GstReferencePipeline *owner{nullptr};
	int32_t id{-1};
	MediaSourceType type{MediaSourceType::UNKNOWN};
	std::string mimeType;

	// Owned by the pipeline bin; valid until the pipeline is destroyed.
	GstElement *appsrc{nullptr};
	GstElement *tailHead{nullptr}; // element decodebin's output links to
	GstElement *volume{nullptr};
	GstElement *balance{nullptr};
	GstElement *sink{nullptr};

	GstCaps *caps{nullptr}; // caps committed to appsrc, owned

	// Written by appsrc callbacks on streaming threads.
	std::atomic<bool> needData{false};
	std::atomic<bool> firstFrameReported{false};

	// Guarded by GstReferencePipeline::m_mutex.
	uint32_t pendingRequestId{0};
	std::vector<StagedSample> staged;
	std::vector<SegmentData> initialPositions;
	bool segmentPushed{false};
	bool eos{false};
	bool removed{false};
	bool muted{false};
	bool encryptedWarned{false};

	~Stream()
	{
		dropStaged();
		if (caps)
		{
			gst_caps_unref(caps);
		}
	}

	void dropStaged()
	{
		for (auto &sample : staged)
		{
			gst_buffer_unref(sample.buffer);
			if (sample.caps)
			{
				gst_caps_unref(sample.caps);
			}
		}
		staged.clear();
	}

	const GstCaps *latestCaps() const
	{
		for (auto it = staged.rbegin(); it != staged.rend(); ++it)
		{
			if (it->caps)
			{
				return it->caps;
			}
		}
		return caps;
	}
};

class GstReferencePipeline final : public IMediaPipeline
{
public:
	GstReferencePipeline(std::weak_ptr<IMediaPipelineClient> client, const VideoRequirements &requirements)
		: m_client(std::move(client))
	{
		RREF_LOG("created (maxWidth=%u maxHeight=%u)", requirements.maxWidth, requirements.maxHeight);
	}

	~GstReferencePipeline() override
	{
		teardownPipeline();
		m_worker.stop();
		m_events.stop();
		RREF_LOG("destroyed");
	}

	std::weak_ptr<IMediaPipelineClient> getClient() override { return m_client; }

	bool load(MediaType type, const std::string &mimeType, const std::string &url, bool isLive) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		RREF_LOG("load: type=%d mime=%s url=%s isLive=%d", static_cast<int>(type), mimeType.c_str(), url.c_str(), isLive);
		if (m_pipeline)
		{
			return true;
		}
		m_pipeline = gst_pipeline_new("rialto-reference");
		if (!m_pipeline)
		{
			return false;
		}
		m_bus = gst_element_get_bus(m_pipeline);
		gst_bus_set_sync_handler(m_bus, &GstReferencePipeline::onBusSync, this, nullptr);
		return true;
	}

	bool attachSource(const std::unique_ptr<MediaSource> &source) override
	{
		if (!source)
		{
			return false;
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_pipeline)
		{
			RREF_LOG("attachSource: not loaded");
			return false;
		}
		auto stream = std::make_unique<Stream>();
		stream->owner = this;
		stream->id = m_nextSourceId;
		stream->type = source->getType();
		stream->mimeType = source->getMimeType();
		stream->caps = buildSourceCaps(*source);
		if (!stream->caps)
		{
			if (stream->type != MediaSourceType::SUBTITLE)
			{
				RREF_LOG("attachSource: unsupported %s mime '%s'", toString(stream->type), stream->mimeType.c_str());
				return false;
			}
			stream->caps = gst_caps_new_empty_simple("application/x-unknown-subtitle");
		}
		if (!buildBranch(*stream))
		{
			return false;
		}

		const_cast<MediaSource &>(*source).setId(stream->id);
		gchar *capsText = gst_caps_to_string(stream->caps);
		RREF_LOG("attachSource: sourceId=%d type=%s caps=%s", stream->id, toString(stream->type), capsText);
		g_free(capsText);

		// A source attached after a pipeline-level seek starts at that position,
		// so every source shares one timeline.
		if (m_lastSeekPositionNs >= 0)
		{
			stream->initialPositions.push_back(SegmentData{m_lastSeekPositionNs, true, 1.0, kUndefinedPosition});
		}
		m_streams[m_nextSourceId++] = std::move(stream);
		return true;
	}

	bool removeSource(int32_t id) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(id);
		if (!stream)
		{
			return false;
		}
		RREF_LOG("removeSource: sourceId=%d", id);
		// The branch stays in the bin; it is ended so it no longer holds up EOS.
		stream->removed = true;
		stream->pendingRequestId = 0;
		stream->dropStaged();
		gst_app_src_end_of_stream(GST_APP_SRC(stream->appsrc));
		return true;
	}

	bool allSourcesAttached() override
	{
		GstState target;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			RREF_LOG("allSourcesAttached: %zu sources", m_streams.size());
			if (!m_pipeline || m_allSourcesAttached)
			{
				return m_pipeline != nullptr;
			}
			m_allSourcesAttached = true;
			target = m_targetState;
		}
		notifyState(PlaybackState::IDLE);
		return changeState(target);
	}

	bool play(bool &async) override
	{
		async = true;
		RREF_LOG("play");
		return requestState(GST_STATE_PLAYING);
	}

	bool pause() override
	{
		RREF_LOG("pause");
		return requestState(GST_STATE_PAUSED);
	}

	bool stop() override
	{
		RREF_LOG("stop");
		GstElement *pipeline = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_targetState = GST_STATE_PAUSED;
			m_allSourcesAttached = false;
			m_isPlaying = false;
			++m_positionGeneration;
			for (auto &entry : m_streams)
			{
				entry.second->pendingRequestId = 0;
				entry.second->dropStaged();
			}
			pipeline = m_pipeline ? GST_ELEMENT(gst_object_ref(m_pipeline)) : nullptr;
		}
		if (pipeline)
		{
			gst_element_set_state(pipeline, GST_STATE_NULL);
			gst_object_unref(pipeline);
		}
		notifyState(PlaybackState::STOPPED);
		return true;
	}

	bool setPlaybackRate(double rate) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		RREF_LOG("setPlaybackRate: rate=%f", rate);
		if (rate <= 0.0)
		{
			RREF_LOG("setPlaybackRate: only positive rates are supported");
			return false;
		}
		if (rate == m_rate)
		{
			m_pendingRate = 0.0;
			return true;
		}
		if (!m_pipeline || GST_STATE(m_pipeline) < GST_STATE_PLAYING)
		{
			RREF_LOG("setPlaybackRate: postponed until PLAYING");
			m_pendingRate = rate;
			return true;
		}
		return applyRateLocked(rate);
	}

	bool setPosition(int64_t position) override
	{
		RREF_LOG("setPosition: position=%" PRId64, position);
		notifyState(PlaybackState::SEEKING);
		std::lock_guard<std::mutex> lock(m_mutex);
		m_lastSeekPositionNs = position;
		m_seekRenderPending = true;
		m_eosNotified = false;
		++m_requestGeneration;
		for (auto &entry : m_streams)
		{
			Stream &stream = *entry.second;
			if (stream.removed)
			{
				continue;
			}
			stream.pendingRequestId = 0;
			stream.dropStaged();
			stream.eos = false;
			stream.firstFrameReported = false;
			stream.initialPositions = {SegmentData{position, true, 1.0, kUndefinedPosition}};
			flushAppSrcLocked(stream, true);
		}
		// Posted under the lock so it precedes any data request issued after it.
		notifyState(PlaybackState::SEEK_DONE);
		for (auto &entry : m_streams)
		{
			issueRequestLocked(*entry.second);
		}
		return true;
	}

	bool getPosition(int64_t &position) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return getPositionLocked(position);
	}

	bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(sourceId);
		if (!stream || !stream->sink)
		{
			return false;
		}
		renderedFrames = 0;
		droppedFrames = 0;
		forEachBaseSink(stream->sink, [&](GstElement *sink) {
			GstStructure *stats = nullptr;
			g_object_get(sink, "stats", &stats, nullptr);
			if (!stats)
			{
				return;
			}
			guint64 rendered = 0;
			guint64 dropped = 0;
			gst_structure_get_uint64(stats, "rendered", &rendered);
			gst_structure_get_uint64(stats, "dropped", &dropped);
			renderedFrames += rendered;
			droppedFrames += dropped;
			gst_structure_free(stats);
		});
		return true;
	}

	bool setImmediateOutput(int32_t sourceId, bool immediateOutput) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_immediateOutput[sourceId] = immediateOutput;
		return true;
	}

	bool getImmediateOutput(int32_t sourceId, bool &immediateOutput) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_immediateOutput.find(sourceId);
		immediateOutput = it != m_immediateOutput.end() && it->second;
		return true;
	}

	bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override
	{
		RREF_LOG("setVideoWindow: x=%u y=%u w=%u h=%u (not applied; the sink owns its window)", x, y, width, height);
		return true;
	}

	bool haveData(MediaSourceStatus status, uint32_t needDataRequestId) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamByRequestLocked(needDataRequestId);
		if (!stream)
		{
			RREF_VERBOSE("haveData: ignoring stale requestId=%u status=%s", needDataRequestId, toString(status));
			return true;
		}
		RREF_VERBOSE("haveData: sourceId=%d requestId=%u status=%s samples=%zu", stream->id, needDataRequestId,
		             toString(status), stream->staged.size());
		stream->pendingRequestId = 0;

		if (status != MediaSourceStatus::OK && status != MediaSourceStatus::EOS)
		{
			if (status != MediaSourceStatus::NO_AVAILABLE_SAMPLES)
			{
				RREF_LOG("haveData: sourceId=%d status=%s", stream->id, toString(status));
			}
			stream->dropStaged();
			scheduleRetryLocked(*stream);
			return true;
		}

		pushStagedLocked(*stream);
		if (status == MediaSourceStatus::EOS)
		{
			RREF_LOG("haveData: EOS for sourceId=%d (%s)", stream->id, toString(stream->type));
			stream->eos = true;
			gst_app_src_end_of_stream(GST_APP_SRC(stream->appsrc));
			return true;
		}
		issueRequestLocked(*stream);
		return true;
	}

	AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment) override
	{
		if (!mediaSegment || !mediaSegment->getData() || mediaSegment->getDataLength() == 0)
		{
			return AddSegmentStatus::ERROR;
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(mediaSegment->getId());
		if (!stream || stream->pendingRequestId != needDataRequestId)
		{
			RREF_VERBOSE("addSegment: discarding segment for stale requestId=%u", needDataRequestId);
			return AddSegmentStatus::OK;
		}
		if (stream->staged.size() >= kFramesPerRequest)
		{
			return AddSegmentStatus::NO_SPACE;
		}
		if (mediaSegment->isEncrypted() && !stream->encryptedWarned)
		{
			RREF_LOG("addSegment: sourceId=%d carries encrypted samples; decryption is not supported, output will "
			         "be corrupt",
			         stream->id);
			stream->encryptedWarned = true;
		}

		StagedSample sample{nullptr, updateCapsForSegment(stream->latestCaps(), *mediaSegment)};
		if (sample.caps)
		{
			gchar *capsText = gst_caps_to_string(sample.caps);
			RREF_LOG("sourceId=%d caps changed: %s", stream->id, capsText);
			g_free(capsText);
		}
		const uint32_t size = mediaSegment->getDataLength();
		sample.buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
		gst_buffer_fill(sample.buffer, 0, mediaSegment->getData(), size);
		GST_BUFFER_PTS(sample.buffer) = static_cast<GstClockTime>(mediaSegment->getTimeStamp());
		GST_BUFFER_DURATION(sample.buffer) = mediaSegment->getDuration() > 0
		                                         ? static_cast<GstClockTime>(mediaSegment->getDuration())
		                                         : GST_CLOCK_TIME_NONE;
		stream->staged.push_back(sample);
		return AddSegmentStatus::OK;
	}

	bool renderFrame() override { return true; }

	bool setVolume(double targetVolume, uint32_t volumeDuration, EaseType easeType) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		RREF_LOG("setVolume: volume=%f duration=%u (applied immediately)", targetVolume, volumeDuration);
		m_volume = targetVolume;
		for (auto &entry : m_streams)
		{
			if (entry.second->volume)
			{
				g_object_set(entry.second->volume, "volume", m_volume, nullptr);
			}
		}
		return true;
	}

	bool getVolume(double &currentVolume) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		currentVolume = m_volume;
		return true;
	}

	bool setMute(int32_t sourceId, bool mute) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(sourceId);
		if (!stream)
		{
			return false;
		}
		RREF_LOG("setMute: sourceId=%d (%s) mute=%d", sourceId, toString(stream->type), mute);
		stream->muted = mute;
		applyMuteLocked(*stream);
		return true;
	}

	bool getMute(int32_t sourceId, bool &mute) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(sourceId);
		mute = stream && stream->muted;
		return stream != nullptr;
	}

	bool setTextTrackIdentifier(const std::string &textTrackIdentifier) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_textTrackIdentifier = textTrackIdentifier;
		return true;
	}

	bool getTextTrackIdentifier(std::string &textTrackIdentifier) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		textTrackIdentifier = m_textTrackIdentifier;
		return true;
	}

	bool setLowLatency(bool) override { return true; }
	bool setSync(bool sync) override
	{
		m_sync = sync;
		return true;
	}
	bool getSync(bool &sync) override
	{
		sync = m_sync;
		return true;
	}
	bool setSyncOff(bool) override { return true; }
	bool setStreamSyncMode(int32_t, int32_t streamSyncMode) override
	{
		m_streamSyncMode = streamSyncMode;
		return true;
	}
	bool getStreamSyncMode(int32_t &streamSyncMode) override
	{
		streamSyncMode = m_streamSyncMode;
		return true;
	}

	bool flush(int32_t sourceId, bool resetTime, bool &async) override
	{
		async = false;
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(sourceId);
		if (!stream)
		{
			return false;
		}
		RREF_LOG("flush: sourceId=%d resetTime=%d", sourceId, resetTime);
		stream->pendingRequestId = 0;
		stream->dropStaged();
		stream->initialPositions.clear();
		stream->eos = false;
		stream->firstFrameReported = false;
		m_eosNotified = false;
		flushAppSrcLocked(*stream, resetTime);
		notify([sourceId](IMediaPipelineClient &client) { client.notifySourceFlushed(sourceId); });
		issueRequestLocked(*stream);
		return true;
	}

	bool setSourcePosition(int32_t sourceId, int64_t position, bool resetTime, double appliedRate,
	                       uint64_t stopPosition) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Stream *stream = findStreamLocked(sourceId);
		if (!stream)
		{
			return false;
		}
		RREF_LOG("setSourcePosition: sourceId=%d position=%" PRId64 " resetTime=%d appliedRate=%f", sourceId, position,
		         resetTime, appliedRate);
		stream->initialPositions.push_back(SegmentData{position, resetTime, appliedRate, stopPosition});
		return true;
	}

	bool setSubtitleOffset(int32_t, int64_t) override { return true; }
	bool processAudioGap(int64_t, uint32_t, int64_t, bool) override { return true; }

	bool setBufferingLimit(uint32_t limitBufferingMs) override
	{
		m_bufferingLimitMs = limitBufferingMs;
		return true;
	}
	bool getBufferingLimit(uint32_t &limitBufferingMs) override
	{
		limitBufferingMs = m_bufferingLimitMs;
		return true;
	}
	bool setUseBuffering(bool useBuffering) override
	{
		m_useBuffering = useBuffering;
		return true;
	}
	bool getUseBuffering(bool &useBuffering) override
	{
		useBuffering = m_useBuffering;
		return true;
	}

	bool switchSource(const std::unique_ptr<MediaSource> &source) override
	{
		RREF_LOG("switchSource: not supported (type=%s)", source ? toString(source->getType()) : "null");
		return false;
	}

	bool getDuration(int64_t &duration) override
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		gint64 value = -1;
		if (!m_pipeline || !gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &value) || value < 0)
		{
			return false;
		}
		duration = value;
		return true;
	}

private:
	// ---- client notifications ------------------------------------------

	template <typename Fn> void notify(Fn &&fn)
	{
		m_events.post([client = m_client, fn = std::forward<Fn>(fn)]() {
			if (auto locked = client.lock())
			{
				fn(*locked);
			}
		});
	}

	void notifyState(PlaybackState state)
	{
		RREF_LOG("notifyPlaybackState(%s)", toString(state));
		notify([state](IMediaPipelineClient &client) { client.notifyPlaybackState(state); });
	}

	// ---- branch construction --------------------------------------------

	bool buildBranch(Stream &stream)
	{
		const bool isVideo = stream.type == MediaSourceType::VIDEO;
		const bool isAudio = stream.type == MediaSourceType::AUDIO;

		std::vector<GstElement *> chain;
		auto add = [&chain](GstElement *element) {
			chain.push_back(element);
			return element != nullptr;
		};
		auto discard = [&chain]() {
			for (GstElement *element : chain)
			{
				if (element)
				{
					gst_object_unref(gst_object_ref_sink(element));
				}
			}
			return false;
		};

		GstElement *appsrc = makeElement("appsrc");
		GstElement *decoder = (isVideo || isAudio) ? makeElement("decodebin") : nullptr;
		if (!appsrc || ((isVideo || isAudio) && !decoder))
		{
			chain = {appsrc, decoder};
			return discard();
		}

		bool ok = true;
		if (isVideo)
		{
			ok = add(makeElement("queue")) && add(makeElement("videoconvert"));
			if (ok)
			{
				// videobalance is only used to blank the picture for setMute(video).
				if (GstElement *balance = gst_element_factory_make("videobalance", nullptr))
				{
					stream.balance = balance;
					chain.push_back(balance);
				}
				ok = add(makeSink("RIALTO_REF_VIDEO_SINK", "autovideosink"));
			}
		}
		else if (isAudio)
		{
			ok = add(makeElement("queue")) && add(makeElement("audioconvert")) && add(makeElement("audioresample")) &&
			     add(makeElement("volume")) && add(makeSink("RIALTO_REF_AUDIO_SINK", "autoaudiosink"));
			if (ok)
			{
				stream.volume = chain[chain.size() - 2];
			}
		}
		else
		{
			ok = add(makeElement("fakesink"));
			if (ok)
			{
				g_object_set(chain.back(), "sync", TRUE, "async", FALSE, nullptr);
			}
		}
		if (!ok)
		{
			chain.push_back(appsrc);
			chain.push_back(decoder);
			stream.volume = nullptr;
			stream.balance = nullptr;
			return discard();
		}

		g_object_set(appsrc, "caps", stream.caps, "format", GST_FORMAT_TIME, "stream-type", GST_APP_STREAM_TYPE_SEEKABLE,
		             "block", FALSE, "min-percent", kAppSrcMinPercent, "emit-signals", FALSE, nullptr);
		GObjectClass *appsrcClass = G_OBJECT_GET_CLASS(appsrc);
		if (g_object_class_find_property(appsrcClass, "handle-segment-change"))
		{
			g_object_set(appsrc, "handle-segment-change", TRUE, nullptr);
		}
		// Queue limits as the server sets them (GstSrc::setupAndAddAppSrc), which
		// determine how far ahead of playback the client is asked for data.
		gst_app_src_set_max_bytes(GST_APP_SRC(appsrc), isVideo   ? kVideoAppSrcMaxBytes
		                                                : isAudio ? kAudioAppSrcMaxBytes
		                                                          : kSubtitleAppSrcMaxBytes);
		if ((isVideo || isAudio) && g_object_class_find_property(appsrcClass, "max-time"))
		{
			g_object_set(appsrc, "max-time", kAppSrcMaxTime, nullptr);
		}
		else if ((isVideo || isAudio) && g_object_class_find_property(appsrcClass, "max-buffers"))
		{
			g_object_set(appsrc, "max-buffers", kAppSrcMaxBuffers, nullptr);
		}
		GstAppSrcCallbacks callbacks{};
		callbacks.need_data = &GstReferencePipeline::onNeedData;
		callbacks.enough_data = &GstReferencePipeline::onEnoughData;
		callbacks.seek_data = &GstReferencePipeline::onSeekData;
		gst_app_src_set_callbacks(GST_APP_SRC(appsrc), &callbacks, &stream, nullptr);

		stream.appsrc = appsrc;
		stream.tailHead = chain.front();
		stream.sink = chain.back();
		if (stream.sink && (isVideo || isAudio))
		{
			g_object_set(stream.sink, "sync", m_sync ? TRUE : FALSE, nullptr);
		}

		gst_bin_add(GST_BIN(m_pipeline), appsrc);
		if (decoder)
		{
			gst_bin_add(GST_BIN(m_pipeline), decoder);
			g_signal_connect(decoder, "pad-added", G_CALLBACK(&GstReferencePipeline::onDecodedPad), &stream);
		}
		for (GstElement *element : chain)
		{
			gst_bin_add(GST_BIN(m_pipeline), element);
		}
		bool linked = decoder ? gst_element_link(appsrc, decoder) : gst_element_link(appsrc, chain.front());
		for (size_t i = 0; linked && i + 1 < chain.size(); ++i)
		{
			linked = gst_element_link(chain[i], chain[i + 1]);
		}
		if (!linked)
		{
			RREF_LOG("attachSource: failed to link %s branch", toString(stream.type));
			for (GstElement *element : chain)
			{
				gst_bin_remove(GST_BIN(m_pipeline), element);
			}
			if (decoder)
			{
				gst_bin_remove(GST_BIN(m_pipeline), decoder);
			}
			gst_bin_remove(GST_BIN(m_pipeline), appsrc);
			stream.appsrc = nullptr;
			stream.tailHead = nullptr;
			stream.volume = nullptr;
			stream.balance = nullptr;
			stream.sink = nullptr;
			return false;
		}

		if (GstPad *pad = gst_element_get_static_pad(stream.sink, "sink"))
		{
			gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, &GstReferencePipeline::onSinkBuffer, &stream, nullptr);
			if (isVideo)
			{
				gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, &GstReferencePipeline::onSinkEvent,
				                  &stream, nullptr);
			}
			gst_object_unref(pad);
		}

		applyMuteLocked(stream);
		if (stream.volume)
		{
			g_object_set(stream.volume, "volume", m_volume, nullptr);
		}
		if (GST_STATE(m_pipeline) > GST_STATE_NULL)
		{
			gst_element_sync_state_with_parent(appsrc);
			if (decoder)
			{
				gst_element_sync_state_with_parent(decoder);
			}
			for (GstElement *element : chain)
			{
				gst_element_sync_state_with_parent(element);
			}
		}
		return true;
	}

	void applyMuteLocked(Stream &stream)
	{
		if (stream.volume)
		{
			g_object_set(stream.volume, "mute", stream.muted ? TRUE : FALSE, nullptr);
		}
		if (stream.balance)
		{
			g_object_set(stream.balance, "brightness", stream.muted ? -1.0 : 0.0, "contrast", stream.muted ? 0.0 : 1.0,
			             nullptr);
		}
	}

	// ---- data flow --------------------------------------------------------

	Stream *findStreamLocked(int32_t id)
	{
		auto it = m_streams.find(id);
		return it == m_streams.end() ? nullptr : it->second.get();
	}

	Stream *findStreamByRequestLocked(uint32_t requestId)
	{
		for (auto &entry : m_streams)
		{
			if (requestId != 0 && entry.second->pendingRequestId == requestId)
			{
				return entry.second.get();
			}
		}
		return nullptr;
	}

	void issueRequestLocked(Stream &stream)
	{
		if (!m_allSourcesAttached || stream.removed || stream.eos || stream.pendingRequestId != 0 || !stream.needData)
		{
			return;
		}
		stream.pendingRequestId = m_nextRequestId++;
		if (m_nextRequestId == 0)
		{
			m_nextRequestId = 1;
		}
		const int32_t sourceId = stream.id;
		const uint32_t requestId = stream.pendingRequestId;
		RREF_VERBOSE("notifyNeedMediaData: sourceId=%d requestId=%u frames=%u", sourceId, requestId, kFramesPerRequest);
		notify([sourceId, requestId](IMediaPipelineClient &client) {
			client.notifyNeedMediaData(sourceId, kFramesPerRequest, requestId, nullptr);
		});
	}

	void scheduleRetryLocked(Stream &stream)
	{
		const int32_t sourceId = stream.id;
		const uint64_t generation = m_requestGeneration;
		m_worker.postDelayed(kNeedDataRetryDelay, [this, sourceId, generation]() {
			std::lock_guard<std::mutex> lock(m_mutex);
			Stream *retry = findStreamLocked(sourceId);
			if (retry && generation == m_requestGeneration)
			{
				issueRequestLocked(*retry);
			}
		});
	}

	void pushStagedLocked(Stream &stream)
	{
		// A source that never got a position would start its segment at 0 and
		// wait until its first PTS before presenting; anchor at the first PTS
		// seen instead, shared by all sources.
		static const bool strictPosition = [] {
			const char *value = std::getenv("RIALTO_REF_STRICT_POSITION");
			return value && std::strcmp(value, "1") == 0;
		}();
		if (!stream.segmentPushed && stream.initialPositions.empty() && !stream.staged.empty() && strictPosition)
		{
			RREF_LOG("sourceId=%d (%s): no position set before data (strict); segment starts at 0, first PTS %" PRId64,
			         stream.id, toString(stream.type), static_cast<int64_t>(GST_BUFFER_PTS(stream.staged.front().buffer)));
			stream.segmentPushed = true;
		}
		else if (!stream.segmentPushed && stream.initialPositions.empty() && !stream.staged.empty())
		{
			if (m_autoAnchorNs < 0)
			{
				m_autoAnchorNs = static_cast<int64_t>(GST_BUFFER_PTS(stream.staged.front().buffer));
				RREF_LOG("no position set before data; anchoring all sources at first PTS %" PRId64, m_autoAnchorNs);
			}
			stream.initialPositions.push_back(SegmentData{m_autoAnchorNs, true, 1.0, kUndefinedPosition});
		}

		for (const SegmentData &data : stream.initialPositions)
		{
			GstSegment segment;
			gst_segment_init(&segment, GST_FORMAT_TIME);
			if (!gst_segment_do_seek(&segment, m_rate, GST_FORMAT_TIME,
			                         data.resetTime ? GST_SEEK_FLAG_FLUSH : GST_SEEK_FLAG_NONE, GST_SEEK_TYPE_SET,
			                         static_cast<guint64>(data.position), GST_SEEK_TYPE_SET, data.stopPosition, nullptr))
			{
				RREF_LOG("sourceId=%d: invalid segment position %" PRId64, stream.id, data.position);
				continue;
			}
			segment.applied_rate = data.appliedRate;
			RREF_LOG("sourceId=%d (%s) new segment start=%" PRId64 " appliedRate=%f resetTime=%d", stream.id,
			         toString(stream.type), data.position, data.appliedRate, data.resetTime);
			GstCaps *caps = gst_app_src_get_caps(GST_APP_SRC(stream.appsrc));
			GstSample *sample = gst_sample_new(nullptr, caps, &segment, nullptr);
			gst_app_src_push_sample(GST_APP_SRC(stream.appsrc), sample);
			gst_sample_unref(sample);
			if (caps)
			{
				gst_caps_unref(caps);
			}
			stream.segmentPushed = true;
		}
		stream.initialPositions.clear();

		for (auto &sample : stream.staged)
		{
			if (sample.caps)
			{
				gst_app_src_set_caps(GST_APP_SRC(stream.appsrc), sample.caps);
				gst_caps_take(&stream.caps, sample.caps);
			}
			gst_app_src_push_buffer(GST_APP_SRC(stream.appsrc), sample.buffer);
		}
		stream.staged.clear();
	}

	void flushAppSrcLocked(Stream &stream, bool resetTime)
	{
		// Below PAUSED nothing is streaming, so there is nothing to flush. Unlike
		// the server, a pipeline still prerolling into PAUSED is flushed too, so
		// data pushed before the seek cannot be presented after it.
		if (!m_pipeline ||
		    (GST_STATE(m_pipeline) < GST_STATE_PAUSED && GST_STATE_PENDING(m_pipeline) < GST_STATE_PAUSED))
		{
			return;
		}
		gst_element_send_event(stream.appsrc, gst_event_new_flush_start());
		gst_element_send_event(stream.appsrc, gst_event_new_flush_stop(resetTime ? TRUE : FALSE));
		// The appsrc queue is now empty, so it wants data again.
		stream.needData = true;
	}

	bool getPositionLocked(int64_t &position)
	{
		if (!m_pipeline)
		{
			return false;
		}
		// Until the first frame after a seek is presented, report the seek target.
		if (m_seekRenderPending && m_lastSeekPositionNs >= 0)
		{
			position = m_lastSeekPositionNs;
			return true;
		}
		gint64 value = -1;
		if (!gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &value) || value < 0)
		{
			return false;
		}
		position = value;
		return true;
	}

	bool applyRateLocked(double rate)
	{
		// Instant rate change (GStreamer >= 1.18): no flush, timestamps unchanged.
		const bool ok = gst_element_seek(m_pipeline, rate, GST_FORMAT_TIME, GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
		                                 GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_NONE, 0);
		RREF_LOG("instant rate change to %f %s", rate, ok ? "applied" : "failed");
		if (ok)
		{
			m_rate = rate;
		}
		return ok;
	}

	// ---- state ------------------------------------------------------------

	bool requestState(GstState state)
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_targetState = state;
			if (state != GST_STATE_PLAYING)
			{
				m_isPlaying = false;
				++m_positionGeneration;
			}
			if (!m_allSourcesAttached)
			{
				// Applied by allSourcesAttached().
				return m_pipeline != nullptr;
			}
		}
		return changeState(state);
	}

	bool changeState(GstState state)
	{
		GstElement *pipeline = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			pipeline = m_pipeline ? GST_ELEMENT(gst_object_ref(m_pipeline)) : nullptr;
		}
		if (!pipeline)
		{
			return false;
		}
		const GstStateChangeReturn result = gst_element_set_state(pipeline, state);
		gst_object_unref(pipeline);
		if (result == GST_STATE_CHANGE_FAILURE)
		{
			RREF_LOG("state change to %s failed", gst_element_state_get_name(state));
			notifyState(PlaybackState::FAILURE);
			return false;
		}
		return true;
	}

	void teardownPipeline()
	{
		GstElement *pipeline = nullptr;
		GstBus *bus = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			pipeline = m_pipeline;
			bus = m_bus;
			m_pipeline = nullptr;
			m_bus = nullptr;
		}
		if (pipeline)
		{
			gst_element_set_state(pipeline, GST_STATE_NULL);
		}
		if (bus)
		{
			gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
			gst_object_unref(bus);
		}
		// The worker may still hold queued tasks that touch streams; stop it
		// before the streams go.
		m_worker.stop();
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_streams.clear();
		}
		if (pipeline)
		{
			gst_object_unref(pipeline);
		}
	}

	// ---- bus --------------------------------------------------------------

	static GstBusSyncReply onBusSync(GstBus *, GstMessage *message, gpointer user)
	{
		auto *self = static_cast<GstReferencePipeline *>(user);
		std::shared_ptr<GstMessage> held(message, [](GstMessage *m) { gst_message_unref(m); });
		self->m_worker.post([self, held]() { self->handleMessage(held.get()); });
		return GST_BUS_DROP;
	}

	void handleMessage(GstMessage *message)
	{
		switch (GST_MESSAGE_TYPE(message))
		{
			case GST_MESSAGE_STATE_CHANGED:
				handleStateChanged(message);
				break;
			case GST_MESSAGE_EOS:
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (!m_eosNotified)
				{
					m_eosNotified = true;
					notifyState(PlaybackState::END_OF_STREAM);
				}
				break;
			}
			case GST_MESSAGE_ERROR:
				handleError(message);
				break;
			case GST_MESSAGE_WARNING:
			{
				GError *error = nullptr;
				gchar *debug = nullptr;
				gst_message_parse_warning(message, &error, &debug);
				RREF_LOG("warning from %s: %s", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)),
				         error ? error->message : "unknown");
				g_clear_error(&error);
				g_free(debug);
				break;
			}
			case GST_MESSAGE_QOS:
			{
				guint64 processed = 0;
				guint64 dropped = 0;
				GstFormat format;
				gst_message_parse_qos_stats(message, &format, &processed, &dropped);
				int32_t sourceId = -1;
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					sourceId = sourceOfLocked(GST_MESSAGE_SRC(message));
				}
				if (sourceId >= 0)
				{
					notify([sourceId, processed, dropped](IMediaPipelineClient &client) {
						client.notifyQos(sourceId, QosInfo{processed, dropped});
					});
				}
				break;
			}
			case GST_MESSAGE_LATENCY:
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_pipeline)
				{
					gst_bin_recalculate_latency(GST_BIN(m_pipeline));
				}
				break;
			}
			default:
				break;
		}
	}

	void handleStateChanged(GstMessage *message)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_pipeline || GST_MESSAGE_SRC(message) != GST_OBJECT(m_pipeline))
		{
			return;
		}
		GstState oldState;
		GstState newState;
		GstState pending;
		gst_message_parse_state_changed(message, &oldState, &newState, &pending);
		RREF_LOG("pipeline state %s -> %s (pending %s)", gst_element_state_get_name(oldState),
		         gst_element_state_get_name(newState), gst_element_state_get_name(pending));
		if (newState == GST_STATE_PAUSED && pending != GST_STATE_PAUSED)
		{
			// As the server: a PAUSED with PAUSED pending is a re-preroll after a flush.
			m_isPlaying = false;
			++m_positionGeneration;
			notifyState(PlaybackState::PAUSED);
		}
		else if (newState == GST_STATE_PLAYING)
		{
			m_isPlaying = true;
			notifyState(PlaybackState::PLAYING);
			if (m_pendingRate > 0.0)
			{
				const double rate = m_pendingRate;
				m_pendingRate = 0.0;
				if (rate != m_rate)
				{
					applyRateLocked(rate);
				}
			}
			schedulePositionReportLocked(++m_positionGeneration);
		}
	}

	void handleError(GstMessage *message)
	{
		GError *error = nullptr;
		gchar *debug = nullptr;
		gst_message_parse_error(message, &error, &debug);
		const bool streamError = error && error->domain == GST_STREAM_ERROR;
		RREF_LOG("error from %s: %s (%s)", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)), error ? error->message : "unknown",
		         debug ? debug : "");
		g_clear_error(&error);
		g_free(debug);

		std::lock_guard<std::mutex> lock(m_mutex);
		bool allEos = !m_streams.empty();
		for (auto &entry : m_streams)
		{
			if (!entry.second->removed && entry.second->type != MediaSourceType::SUBTITLE && !entry.second->eos)
			{
				allEos = false;
			}
		}
		// As the server: a stream error once every source has ended is reported as EOS.
		if (streamError && allEos)
		{
			if (!m_eosNotified)
			{
				m_eosNotified = true;
				notifyState(PlaybackState::END_OF_STREAM);
			}
			return;
		}
		notifyState(PlaybackState::FAILURE);
	}

	int32_t sourceOfLocked(GstObject *object)
	{
		for (auto &entry : m_streams)
		{
			for (GstObject *o = object; o; o = GST_OBJECT_PARENT(o))
			{
				if (o == GST_OBJECT(entry.second->sink))
				{
					return entry.first;
				}
			}
		}
		return -1;
	}

	void schedulePositionReportLocked(uint64_t generation)
	{
		m_worker.postDelayed(kPositionReportInterval, [this, generation]() {
			std::lock_guard<std::mutex> lock(m_mutex);
			if (generation != m_positionGeneration || !m_isPlaying)
			{
				return;
			}
			int64_t position = 0;
			if (getPositionLocked(position))
			{
				notify([position](IMediaPipelineClient &client) { client.notifyPosition(position); });
			}
			schedulePositionReportLocked(generation);
		});
	}

	// ---- streaming-thread callbacks (must not take m_mutex) ---------------

	static void onNeedData(GstAppSrc *, guint, gpointer user)
	{
		auto *stream = static_cast<Stream *>(user);
		if (stream->needData.exchange(true))
		{
			return;
		}
		GstReferencePipeline *self = stream->owner;
		self->m_worker.post([self, stream]() {
			std::lock_guard<std::mutex> lock(self->m_mutex);
			self->issueRequestLocked(*stream);
		});
	}

	static void onEnoughData(GstAppSrc *, gpointer user) { static_cast<Stream *>(user)->needData = false; }

	static gboolean onSeekData(GstAppSrc *, guint64, gpointer)
	{
		// Seeks are driven through setPosition(); accept so upstream events
		// such as an instant rate change are not refused.
		return TRUE;
	}

	static void onDecodedPad(GstElement *, GstPad *pad, gpointer user)
	{
		auto *stream = static_cast<Stream *>(user);
		GstPad *sinkPad = gst_element_get_static_pad(stream->tailHead, "sink");
		if (!sinkPad)
		{
			return;
		}
		if (!gst_pad_is_linked(sinkPad))
		{
			const GstPadLinkReturn result = gst_pad_link(pad, sinkPad);
			RREF_LOG("sourceId=%d decoder output %s", stream->id,
			         result == GST_PAD_LINK_OK ? "linked" : gst_pad_link_get_name(result));
		}
		gst_object_unref(sinkPad);
	}

	static GstPadProbeReturn onSinkBuffer(GstPad *, GstPadProbeInfo *, gpointer user)
	{
		auto *stream = static_cast<Stream *>(user);
		if (stream->type != MediaSourceType::SUBTITLE && !stream->firstFrameReported.exchange(true))
		{
			GstReferencePipeline *self = stream->owner;
			const int32_t sourceId = stream->id;
			self->m_worker.post([self]() {
				std::lock_guard<std::mutex> lock(self->m_mutex);
				self->m_seekRenderPending = false;
			});
			RREF_LOG("first frame at sink for sourceId=%d", sourceId);
			self->notify([sourceId](IMediaPipelineClient &client) { client.notifyFirstFrameReceived(sourceId); });
		}
		return GST_PAD_PROBE_OK;
	}

	static GstPadProbeReturn onSinkEvent(GstPad *, GstPadProbeInfo *info, gpointer user)
	{
		GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
		if (!event || GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
		{
			return GST_PAD_PROBE_OK;
		}
		GstCaps *caps = nullptr;
		gst_event_parse_caps(event, &caps);
		const GstStructure *structure = caps ? gst_caps_get_structure(caps, 0) : nullptr;
		int width = 0;
		int height = 0;
		if (structure && gst_structure_get_int(structure, "width", &width) &&
		    gst_structure_get_int(structure, "height", &height))
		{
			int parN = 1;
			int parD = 1;
			gst_structure_get_fraction(structure, "pixel-aspect-ratio", &parN, &parD);
			const double aspect = parD ? static_cast<double>(parN) / parD : 1.0;
			auto *stream = static_cast<Stream *>(user);
			stream->owner->notify([width, height, aspect](IMediaPipelineClient &client) {
				client.notifyNativeSize(static_cast<uint32_t>(width), static_cast<uint32_t>(height), aspect);
			});
		}
		return GST_PAD_PROBE_OK;
	}

	// ---- members ------------------------------------------------------------

	std::weak_ptr<IMediaPipelineClient> m_client;

	std::mutex m_mutex;
	GstElement *m_pipeline{nullptr};
	GstBus *m_bus{nullptr};
	std::map<int32_t, std::unique_ptr<Stream>> m_streams;
	int32_t m_nextSourceId{1};
	uint32_t m_nextRequestId{1};
	uint64_t m_requestGeneration{0};
	bool m_allSourcesAttached{false};
	GstState m_targetState{GST_STATE_PAUSED};
	bool m_isPlaying{false};
	uint64_t m_positionGeneration{0};
	bool m_eosNotified{false};
	double m_rate{1.0};
	double m_pendingRate{0.0};
	double m_volume{1.0};
	int64_t m_lastSeekPositionNs{-1};
	bool m_seekRenderPending{false};
	int64_t m_autoAnchorNs{-1};
	std::map<int32_t, bool> m_immediateOutput;
	std::string m_textTrackIdentifier;
	std::atomic<bool> m_sync{true};
	std::atomic<int32_t> m_streamSyncMode{0};
	std::atomic<uint32_t> m_bufferingLimitMs{kInvalidLimitBuffering};
	std::atomic<bool> m_useBuffering{false};

	// Declared last: destroyed first, after their threads are stopped.
	Executor m_events;
	Executor m_worker;
};

class GstReferencePipelineFactory final : public IMediaPipelineFactory
{
public:
	std::unique_ptr<IMediaPipeline> createMediaPipeline(std::weak_ptr<IMediaPipelineClient> client,
	                                                    const VideoRequirements &videoRequirements) const override
	{
		return createGstMediaPipeline(std::move(client), videoRequirements);
	}
};

} // namespace

std::unique_ptr<IMediaPipeline> createGstMediaPipeline(std::weak_ptr<IMediaPipelineClient> client,
                                                       const VideoRequirements &videoRequirements)
{
	if (!gst_is_initialized())
	{
		GError *error = nullptr;
		if (!gst_init_check(nullptr, nullptr, &error))
		{
			RREF_LOG("GStreamer initialisation failed: %s", error ? error->message : "unknown");
			g_clear_error(&error);
			return nullptr;
		}
	}
	return std::make_unique<GstReferencePipeline>(std::move(client), videoRequirements);
}

std::shared_ptr<IMediaPipelineFactory> createGstMediaPipelineFactory()
{
	return std::make_shared<GstReferencePipelineFactory>();
}

} // namespace rialto_reference
