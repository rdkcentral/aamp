// Lean GStreamer backend for the Media Pipeline API. The only code allowed to
// include GStreamer headers.
//
//   appsrc(video) ! h264parse|h265parse ! <decoder> ! queue ! videoconvert ! <video sink>
//   appsrc(audio) ! aacparse ! <decoder> ! queue ! audioconvert ! audioresample ! <audio sink>
//
// Buffers are timestamped relative to the shared start position from
// setPosition(), so both branches share one running time and the pipeline
// clock provides A/V presentation.

#include "GstMediaPipeline.h"

#include "common/Clock.h"
#include "common/Log.h"
#include "common/Telemetry.h"

#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasesink.h>
#include <gst/gst.h>
#if defined(__APPLE__)
#include <gst/gstmacos.h>
#endif

#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace nmqe::media {

namespace {

constexpr int64_t kTickMs = 10;
constexpr size_t kFramesPerRequest = 16;

const std::vector<const char *> kH264Decoders = {"vtdec_hw", "vtdec", "v4l2h264dec", "avdec_h264", "openh264dec"};
const std::vector<const char *> kH265Decoders = {"vtdec_hw", "vtdec", "v4l2h265dec", "avdec_h265"};
const std::vector<const char *> kAacDecoders = {"avdec_aac", "fdkaacdec", "faad"};

std::string firstAvailable(const std::string &preferred, const std::vector<const char *> &candidates)
{
	if (!preferred.empty()) return preferred;
	for (const char *name : candidates)
	{
		if (GstElementFactory *factory = gst_element_factory_find(name))
		{
			gst_object_unref(factory);
			return name;
		}
	}
	return {};
}

GstBuffer *wrap(const std::shared_ptr<const uint8_t> &data, size_t size)
{
	// The access unit's storage is shared and immutable; the GstBuffer holds a reference.
	auto *holder = new std::shared_ptr<const uint8_t>(data);
	return gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY, const_cast<uint8_t *>(data.get()), size, 0, size, holder,
	                                   [](gpointer user) { delete static_cast<std::shared_ptr<const uint8_t> *>(user); });
}

class GstMediaPipeline final : public IMediaPipeline
{
public:
	GstMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, PlatformSettings settings)
		: mClient(std::move(client)), mSettings(std::move(settings))
	{
	}

	~GstMediaPipeline() override { stop(); }

	bool load(bool) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mPipeline) return true;
		mPipeline = gst_pipeline_new("nmqe");
		if (!mPipeline) return false;
		if (mSettings.pipelineLatencyMs > 0)
		{
			gst_pipeline_set_latency(GST_PIPELINE(mPipeline), static_cast<GstClockTime>(mSettings.pipelineLatencyMs) * GST_MSECOND);
		}
		mBus = gst_element_get_bus(mPipeline);
		mStopping = false;
		mThread = std::thread(&GstMediaPipeline::run, this);
		return true;
	}

	bool attachSource(MediaSource &source) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (!mPipeline) return false;
		auto branch = std::make_unique<Branch>();
		branch->owner = this;
		branch->type = source.type;
		branch->id = mNextId;

		GstCaps *caps = makeCaps(source);
		if (!caps)
		{
			LOG_ERROR("gst", "unsupported source %s", source.mimeType.c_str());
			return false;
		}
		const bool video = source.type == MediaSourceType::VIDEO;
		const std::string parserName = source.mimeType == "video/h265" ? "h265parse" : video ? "h264parse" : "aacparse";
		branch->decoderName = video ? firstAvailable(mSettings.videoDecoder, source.mimeType == "video/h265" ? kH265Decoders : kH264Decoders)
		                            : firstAvailable(mSettings.audioDecoder, kAacDecoders);
		branch->sinkName = video ? mSettings.videoSink : mSettings.audioSink;
		if (branch->sinkName.empty()) branch->sinkName = video ? "autovideosink" : "autoaudiosink";

		std::vector<std::string> names = {"appsrc", parserName, branch->decoderName, "queue"};
		if (video) names.insert(names.end(), {"videoconvert", branch->sinkName});
		else names.insert(names.end(), {"audioconvert", "audioresample", branch->sinkName});

		std::vector<GstElement *> elements;
		for (const auto &name : names)
		{
			GstElement *element = name.empty() ? nullptr : gst_element_factory_make(name.c_str(), nullptr);
			if (!element)
			{
				LOG_ERROR("gst", "missing element '%s' for %s", name.c_str(), toString(source.type));
				for (GstElement *made : elements) gst_object_unref(made);
				gst_caps_unref(caps);
				return false;
			}
			elements.push_back(element);
		}
		branch->appsrc = elements.front();
		branch->sink = elements.back();
		g_object_set(branch->appsrc, "caps", caps, "format", GST_FORMAT_TIME, "is-live", FALSE, "block", FALSE,
		             "max-bytes", static_cast<guint64>(mSettings.appsrcMaxBytes ? mSettings.appsrcMaxBytes : 4 * 1024 * 1024),
		             "stream-type", GST_APP_STREAM_TYPE_STREAM, nullptr);
		branch->caps = gst_caps_to_string(caps);
		gst_caps_unref(caps);
		// Keep the queue small: this is a live player, buffering stays in the player's own queues.
		g_object_set(elements[3], "max-size-buffers", 0u, "max-size-bytes", 0u, "max-size-time", static_cast<guint64>(GST_SECOND), nullptr);
		configureSink(branch->sink);

		GstAppSrcCallbacks callbacks{};
		callbacks.need_data = [](GstAppSrc *, guint, gpointer user) { static_cast<Branch *>(user)->needing = true; };
		callbacks.enough_data = [](GstAppSrc *, gpointer user) { static_cast<Branch *>(user)->needing = false; };
		gst_app_src_set_callbacks(GST_APP_SRC(branch->appsrc), &callbacks, branch.get(), nullptr);

		for (GstElement *element : elements) gst_bin_add(GST_BIN(mPipeline), element);
		for (size_t i = 0; i + 1 < elements.size(); ++i)
		{
			if (!gst_element_link(elements[i], elements[i + 1]))
			{
				LOG_ERROR("gst", "cannot link %s -> %s", names[i].c_str(), names[i + 1].c_str());
				return false;
			}
		}

		if (GstPad *pad = gst_element_get_static_pad(branch->sink, "sink"))
		{
			gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, &GstMediaPipeline::onSinkBuffer, branch.get(), nullptr);
			gst_object_unref(pad);
		}

		source.id = mNextId++;
		mBranches[source.id] = std::move(branch);
		return true;
	}

	bool removeSource(int32_t) override { return false; }

	bool allSourcesAttached() override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mPipeline && gst_element_set_state(mPipeline, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE;
	}

	bool play() override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mPipeline && gst_element_set_state(mPipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
	}

	bool pause() override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mPipeline && gst_element_set_state(mPipeline, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE;
	}

	bool stop() override
	{
		{
			std::lock_guard<std::mutex> lock(mMutex);
			if (!mPipeline) return true;
			mStopping = true;
		}
		if (mThread.joinable()) mThread.join();
		std::lock_guard<std::mutex> lock(mMutex);
		gst_element_set_state(mPipeline, GST_STATE_NULL);
		mBranches.clear();
		if (mBus) gst_object_unref(mBus);
		gst_object_unref(mPipeline);
		mBus = nullptr;
		mPipeline = nullptr;
		return true;
	}

	bool setPlaybackRate(double) override { return false; }

	bool setPosition(int64_t positionNs) override
	{
		mBaseNs = positionNs;
		return true;
	}

	bool getPosition(int64_t &positionNs) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		gint64 position = 0;
		if (!mPipeline || !gst_element_query_position(mPipeline, GST_FORMAT_TIME, &position)) return false;
		positionNs = mBaseNs + position;
		return true;
	}

	bool getRenderedTimestamp(int32_t sourceId, int64_t &timeStampNs) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mBranches.find(sourceId);
		if (it == mBranches.end() || it->second->sinkBuffers == 0) return false;
		timeStampNs = mBaseNs + it->second->lastSinkPtsNs;
		return true;
	}

	bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mBranches.find(sourceId);
		if (it == mBranches.end()) return false;
		renderedFrames = 0;
		droppedFrames = 0;
		forEachBaseSink(it->second->sink, [&](GstElement *sink) {
			GstStructure *stats = nullptr;
			g_object_get(sink, "stats", &stats, nullptr);
			if (!stats) return;
			guint64 rendered = 0, dropped = 0;
			gst_structure_get_uint64(stats, "rendered", &rendered);
			gst_structure_get_uint64(stats, "dropped", &dropped);
			renderedFrames += rendered;
			droppedFrames += dropped;
			gst_structure_free(stats);
		});
		return true;
	}

	bool getQueuedBytes(int32_t sourceId, uint64_t &queuedBytes) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		auto it = mBranches.find(sourceId);
		if (it == mBranches.end()) return false;
		queuedBytes = gst_app_src_get_current_level_bytes(GST_APP_SRC(it->second->appsrc));
		return true;
	}

	bool haveData(MediaSourceStatus, uint32_t needDataRequestId) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		for (auto &[id, branch] : mBranches)
		{
			if (branch->outstanding == needDataRequestId) branch->outstanding = 0;
		}
		return true;
	}

	AddSegmentStatus addSegment(uint32_t, const MediaSegment &segment) override
	{
		GstElement *appsrc = nullptr;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			auto it = mBranches.find(segment.sourceId);
			if (it == mBranches.end() || !segment.data) return AddSegmentStatus::ERROR;
			appsrc = it->second->appsrc;
		}
		const int64_t pts = segment.timeStampNs - mBaseNs;
		if (pts < 0) return AddSegmentStatus::OK; // before the shared start: trimmed
		GstBuffer *buffer = wrap(segment.data, segment.size);
		GST_BUFFER_PTS(buffer) = static_cast<GstClockTime>(pts);
		const int64_t dts = segment.decodeTimeStampNs - mBaseNs;
		GST_BUFFER_DTS(buffer) = dts >= 0 ? static_cast<GstClockTime>(dts) : GST_CLOCK_TIME_NONE;
		GST_BUFFER_DURATION(buffer) = segment.durationNs > 0 ? static_cast<GstClockTime>(segment.durationNs) : GST_CLOCK_TIME_NONE;
		if (!segment.keyFrame) GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
		return gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer) == GST_FLOW_OK ? AddSegmentStatus::OK : AddSegmentStatus::ERROR;
	}

	bool flush(int32_t sourceId, bool resetTime) override
	{
		GstElement *appsrc = nullptr;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			auto it = mBranches.find(sourceId);
			if (it == mBranches.end()) return false;
			appsrc = it->second->appsrc;
		}
		gst_element_send_event(appsrc, gst_event_new_flush_start());
		gst_element_send_event(appsrc, gst_event_new_flush_stop(resetTime));
		if (auto client = mClient.lock()) client->notifySourceFlushed(sourceId);
		return true;
	}

	std::string describe() override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		std::string branches = "[";
		for (auto &[id, branch] : mBranches)
		{
			if (branches.size() > 1) branches += ',';
			Json entry;
			entry.add("source_id", id).add("type", toString(branch->type)).add("caps", branch->caps).add("decoder", branch->decoderName).add("sink", branch->sinkName);
			std::string actual;
			forEachBaseSink(branch->sink, [&](GstElement *sink) {
				if (!actual.empty()) actual += ',';
				actual += GST_OBJECT_NAME(gst_element_get_factory(sink));
			});
			entry.add("actual_sink", actual);
			branches += entry.object();
		}
		branches += "]";
		Json out;
		out.add("backend", "gstreamer").add("gst_version", gst_version_string() ? std::string(gst_version_string()) : std::string());
		out.add("sink_sync", mSettings.sinkSync).add("appsrc_max_bytes", mSettings.appsrcMaxBytes);
		out.add("pipeline_latency_ms", mSettings.pipelineLatencyMs).add("processing_deadline_ms", mSettings.sinkProcessingDeadlineMs);
		if (mPipeline)
		{
			if (GstClock *clock = gst_element_get_clock(mPipeline))
			{
				out.add("clock", GST_OBJECT_NAME(clock));
				gst_object_unref(clock);
			}
		}
		out.raw("branches", branches);
		return out.object();
	}

private:
	struct Branch
	{
		GstMediaPipeline *owner = nullptr;
		int32_t id = -1;
		MediaSourceType type = MediaSourceType::UNKNOWN;
		GstElement *appsrc = nullptr; // owned by the pipeline
		GstElement *sink = nullptr;   // owned by the pipeline
		std::string decoderName;
		std::string sinkName;
		std::string caps;
		std::atomic<bool> needing{false};
		uint32_t outstanding = 0;
		std::atomic<uint64_t> sinkBuffers{0};
		std::atomic<int64_t> lastSinkPtsNs{0};
		std::atomic<bool> firstFrameReported{false};
	};

	static GstCaps *makeCaps(const MediaSource &source)
	{
		GstBuffer *codecData = nullptr;
		if (!source.codecData.empty())
		{
			codecData = gst_buffer_new_memdup(source.codecData.data(), source.codecData.size());
		}
		GstCaps *caps = nullptr;
		if (source.mimeType == "video/h264" || source.mimeType == "video/h265")
		{
			const bool hevc = source.mimeType == "video/h265";
			caps = gst_caps_new_simple(hevc ? "video/x-h265" : "video/x-h264", "alignment", G_TYPE_STRING, "au", "stream-format",
			                           G_TYPE_STRING, hevc ? "hvc1" : (source.streamFormat == StreamFormat::BYTE_STREAM ? "byte-stream" : "avc"),
			                           nullptr);
			if (source.width && source.height)
			{
				gst_caps_set_simple(caps, "width", G_TYPE_INT, static_cast<int>(source.width), "height", G_TYPE_INT, static_cast<int>(source.height), nullptr);
			}
		}
		else if (source.mimeType == "audio/mp4")
		{
			caps = gst_caps_new_simple("audio/mpeg", "mpegversion", G_TYPE_INT, 4, "stream-format", G_TYPE_STRING, "raw", nullptr);
			if (source.sampleRate) gst_caps_set_simple(caps, "rate", G_TYPE_INT, static_cast<int>(source.sampleRate), nullptr);
			if (source.numberOfChannels) gst_caps_set_simple(caps, "channels", G_TYPE_INT, static_cast<int>(source.numberOfChannels), nullptr);
		}
		if (caps && codecData) gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, codecData, nullptr);
		if (codecData) gst_buffer_unref(codecData);
		return caps;
	}

	template <typename Visit>
	static void forEachBaseSink(GstElement *element, Visit visit)
	{
		if (GST_IS_BASE_SINK(element))
		{
			visit(element);
			return;
		}
		if (!GST_IS_BIN(element)) return;
		GstIterator *it = gst_bin_iterate_recurse(GST_BIN(element));
		GValue item = G_VALUE_INIT;
		while (gst_iterator_next(it, &item) == GST_ITERATOR_OK)
		{
			auto *child = GST_ELEMENT(g_value_get_object(&item));
			if (GST_IS_BASE_SINK(child)) visit(child);
			g_value_reset(&item);
		}
		g_value_unset(&item);
		gst_iterator_free(it);
	}

	void applySinkProperties(GstElement *sink) const
	{
		g_object_set(sink, "sync", mSettings.sinkSync ? TRUE : FALSE, nullptr);
		if (mSettings.sinkProcessingDeadlineMs > 0)
		{
			g_object_set(sink, "processing-deadline", static_cast<guint64>(mSettings.sinkProcessingDeadlineMs) * GST_MSECOND, nullptr);
		}
	}

	void configureSink(GstElement *sink)
	{
		if (GST_IS_BASE_SINK(sink)) applySinkProperties(sink);
		// auto*sink bins create their child on state change; configure it when it appears.
		if (GST_IS_BIN(sink))
		{
			g_signal_connect(sink, "deep-element-added", G_CALLBACK(+[](GstBin *, GstBin *, GstElement *element, gpointer user) {
				                 if (GST_IS_BASE_SINK(element)) static_cast<GstMediaPipeline *>(user)->applySinkProperties(element);
			                 }),
			                 this);
		}
	}

	static GstPadProbeReturn onSinkBuffer(GstPad *, GstPadProbeInfo *info, gpointer user)
	{
		auto *branch = static_cast<Branch *>(user);
		GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
		if (buffer && GST_BUFFER_PTS_IS_VALID(buffer)) branch->lastSinkPtsNs = static_cast<int64_t>(GST_BUFFER_PTS(buffer));
		branch->sinkBuffers++;
		if (!branch->firstFrameReported.exchange(true))
		{
			if (auto client = branch->owner->mClient.lock()) client->notifyFirstFrameReceived(branch->id);
		}
		return GST_PAD_PROBE_OK;
	}

	void handleMessage(GstMessage *message, const std::shared_ptr<IMediaPipelineClient> &client)
	{
		switch (GST_MESSAGE_TYPE(message))
		{
			case GST_MESSAGE_ERROR:
			{
				GError *error = nullptr;
				gchar *debug = nullptr;
				gst_message_parse_error(message, &error, &debug);
				const std::string text = std::string(GST_OBJECT_NAME(GST_MESSAGE_SRC(message))) + ": " + (error ? error->message : "error") +
				                         (debug ? std::string(" (") + debug + ")" : std::string());
				LOG_ERROR("gst", "%s", text.c_str());
				if (client) client->notifyPlaybackError(-1, text);
				if (error) g_error_free(error);
				g_free(debug);
				break;
			}
			case GST_MESSAGE_WARNING:
			{
				GError *error = nullptr;
				gchar *debug = nullptr;
				gst_message_parse_warning(message, &error, &debug);
				LOG_WARN("gst", "%s: %s", GST_OBJECT_NAME(GST_MESSAGE_SRC(message)), error ? error->message : "warning");
				if (error) g_error_free(error);
				g_free(debug);
				break;
			}
			case GST_MESSAGE_STATE_CHANGED:
			{
				if (GST_MESSAGE_SRC(message) != GST_OBJECT(mPipeline)) break;
				GstState oldState, newState, pending;
				gst_message_parse_state_changed(message, &oldState, &newState, &pending);
				if (!client) break;
				if (newState == GST_STATE_PLAYING) client->notifyPlaybackState(PlaybackState::PLAYING);
				else if (newState == GST_STATE_PAUSED && oldState == GST_STATE_PLAYING) client->notifyPlaybackState(PlaybackState::PAUSED);
				break;
			}
			case GST_MESSAGE_QOS:
			{
				guint64 processed = 0, dropped = 0;
				GstFormat format;
				gst_message_parse_qos_stats(message, &format, &processed, &dropped);
				if (client) client->notifyQos(sourceOf(GST_MESSAGE_SRC(message)), QosInfo{processed, dropped});
				break;
			}
			case GST_MESSAGE_LATENCY:
				gst_bin_recalculate_latency(GST_BIN(mPipeline));
				break;
			case GST_MESSAGE_EOS:
				if (client) client->notifyPlaybackState(PlaybackState::END_OF_STREAM);
				break;
			default:
				break;
		}
	}

	int32_t sourceOf(GstObject *object)
	{
		std::lock_guard<std::mutex> lock(mMutex);
		for (auto &[id, branch] : mBranches)
		{
			for (GstObject *o = object; o; o = GST_OBJECT_PARENT(o))
			{
				if (o == GST_OBJECT(branch->sink)) return id;
			}
		}
		return -1;
	}

	void run()
	{
		auto client = mClient.lock();
		for (;;)
		{
			{
				std::unique_lock<std::mutex> lock(mMutex);
				if (mStopping) return;
			}
			while (GstMessage *message = gst_bus_timed_pop(mBus, kTickMs * GST_MSECOND))
			{
				handleMessage(message, client);
				gst_message_unref(message);
				if (mStopping) return;
			}
			// appsrc emits need-data once per empty queue; re-ask while it still needs data.
			std::vector<std::pair<int32_t, uint32_t>> requests;
			{
				std::lock_guard<std::mutex> lock(mMutex);
				for (auto &[id, branch] : mBranches)
				{
					if (branch->needing && branch->outstanding == 0)
					{
						branch->outstanding = ++mRequestId;
						requests.emplace_back(id, branch->outstanding);
					}
				}
			}
			if (client)
			{
				for (auto [id, request] : requests) client->notifyNeedMediaData(id, kFramesPerRequest, request);
			}
		}
	}

	std::weak_ptr<IMediaPipelineClient> mClient;
	PlatformSettings mSettings;
	std::mutex mMutex;
	std::thread mThread;
	GstElement *mPipeline = nullptr;
	GstBus *mBus = nullptr;
	std::map<int32_t, std::unique_ptr<Branch>> mBranches;
	int32_t mNextId = 1;
	uint32_t mRequestId = 0;
	std::atomic<bool> mStopping{false};
	std::atomic<int64_t> mBaseNs{0};
};

} // namespace

std::unique_ptr<IMediaPipeline> createGstMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, const PlatformSettings &settings)
{
	return std::make_unique<GstMediaPipeline>(std::move(client), settings);
}

int runPlatformMain(int argc, char **argv, int (*body)(int, char **, void *), void *user)
{
	gst_init(&argc, &argv);
#if defined(__APPLE__)
	// Video sinks need an NSApplication run loop on the main thread.
	return gst_macos_main(reinterpret_cast<GstMainFunc>(body), argc, argv, user);
#else
	return body(argc, argv, user);
#endif
}

} // namespace nmqe::media
