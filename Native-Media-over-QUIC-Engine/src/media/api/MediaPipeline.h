#pragma once

/*
 * Rialto-shaped Media Pipeline API: a deliberately small subset of
 * firebolt::rialto::IMediaPipeline / IMediaPipelineClient owned in this
 * project's namespace. Names and semantics follow Rialto where they exist;
 * timestamps are nanoseconds as in Rialto MediaSegment.
 *
 * No MOQT, MSF/CMSF, CMAF-container or GStreamer type may appear here.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nmqe::media {

enum class MediaSourceType { UNKNOWN, AUDIO, VIDEO };

enum class PlaybackState { UNKNOWN, IDLE, PLAYING, PAUSED, SEEKING, SEEK_DONE, STOPPED, END_OF_STREAM, FAILURE };

enum class NetworkState { UNKNOWN, IDLE, BUFFERING, BUFFERING_PROGRESS, BUFFERED, STALLED, FORMAT_ERROR, NETWORK_ERROR, DECODE_ERROR };

enum class MediaSourceStatus { OK, EOS, ERROR, CODEC_CHANGED, NO_AVAILABLE_SAMPLES, NO_SPACE_FOR_SAMPLES };

enum class AddSegmentStatus { OK, NO_SPACE, ERROR };

enum class SegmentAlignment { UNDEFINED, NAL, AU };

enum class StreamFormat { UNDEFINED, RAW, AVC, BYTE_STREAM, HVC1, HEV1 };

const char *toString(MediaSourceType type);
const char *toString(PlaybackState state);

/** Source configuration, derived from the CMAF header above this API. */
struct MediaSource
{
	int32_t id = -1;                    // assigned by attachSource()
	MediaSourceType type = MediaSourceType::UNKNOWN;
	std::string mimeType;               // "video/h264", "video/h265", "audio/mp4"
	std::vector<uint8_t> codecData;     // avcC/hvcC record or AAC AudioSpecificConfig
	StreamFormat streamFormat = StreamFormat::UNDEFINED;
	SegmentAlignment alignment = SegmentAlignment::AU;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t sampleRate = 0;
	uint32_t numberOfChannels = 0;
};

/** One access unit. Payload ownership is shared and immutable. */
struct MediaSegment
{
	int32_t sourceId = -1;
	MediaSourceType type = MediaSourceType::UNKNOWN;
	int64_t timeStampNs = 0;            // presentation time on the media timeline
	int64_t decodeTimeStampNs = 0;
	int64_t durationNs = 0;
	bool keyFrame = false;
	std::shared_ptr<const uint8_t> data;
	size_t size = 0;
	// Correlation for telemetry only; not interpreted by a backend.
	uint64_t group = 0;
	uint64_t object = 0;
	int64_t receiveMonoNs = 0;
};

struct QosInfo
{
	uint64_t processed = 0;
	uint64_t dropped = 0;
};

/** Callbacks from a pipeline. May arrive on backend threads; implementations post, never re-enter. */
class IMediaPipelineClient
{
public:
	virtual ~IMediaPipelineClient() = default;
	virtual void notifyPlaybackState(PlaybackState state) = 0;
	virtual void notifyNetworkState(NetworkState state) = 0;
	virtual void notifyPosition(int64_t positionNs) = 0;
	virtual void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId) = 0;
	virtual void notifyQos(int32_t sourceId, const QosInfo &qosInfo) = 0;
	virtual void notifyBufferUnderflow(int32_t sourceId) = 0;
	virtual void notifyFirstFrameReceived(int32_t sourceId) = 0;
	virtual void notifyPlaybackError(int32_t sourceId, const std::string &error) = 0;
	virtual void notifySourceFlushed(int32_t sourceId) = 0;
};

class IMediaPipeline
{
public:
	virtual ~IMediaPipeline() = default;

	virtual bool load(bool isLive) = 0;
	virtual bool attachSource(MediaSource &source) = 0;
	virtual bool removeSource(int32_t id) = 0;
	virtual bool allSourcesAttached() = 0;
	virtual bool play() = 0;
	virtual bool pause() = 0;
	virtual bool stop() = 0;
	virtual bool setPlaybackRate(double rate) = 0;
	/** The media time, shared by every source, that presentation starts at. Call before play(). */
	virtual bool setPosition(int64_t positionNs) = 0;
	/** Current presentation position on the media timeline, in ns. */
	virtual bool getPosition(int64_t &positionNs) = 0;
	/** Media time of the latest access unit to reach the source's sink input (not yet necessarily shown). */
	virtual bool getRenderedTimestamp(int32_t sourceId, int64_t &timeStampNs) = 0;
	virtual bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames) = 0;
	/** Bytes accepted by addSegment() and not yet consumed downstream of the source. */
	virtual bool getQueuedBytes(int32_t sourceId, uint64_t &queuedBytes) = 0;
	virtual bool haveData(MediaSourceStatus status, uint32_t needDataRequestId) = 0;
	virtual AddSegmentStatus addSegment(uint32_t needDataRequestId, const MediaSegment &segment) = 0;
	virtual bool flush(int32_t sourceId, bool resetTime) = 0;
	/** Startup properties, element names, caps and clocks, as one JSON object for the run record. */
	virtual std::string describe() = 0;
};

/** Backend-neutral platform settings, filled from the platform profile. */
struct PlatformSettings
{
	std::string videoDecoder;
	std::string audioDecoder;
	std::string videoSink;
	std::string audioSink;
	bool sinkSync = true;
	uint64_t appsrcMaxBytes = 0;
	int64_t sinkProcessingDeadlineMs = 0;
	int64_t pipelineLatencyMs = 0;
};

/** Creates a pipeline by kind ("gst" or "null"); nullptr when unknown or unavailable. */
std::unique_ptr<IMediaPipeline> createMediaPipeline(const std::string &kind, std::weak_ptr<IMediaPipelineClient> client,
                                                    const PlatformSettings &settings);

/**
 * Runs `body` with whatever main-thread event loop the platform's media
 * backend needs (an NSApplication loop on macOS for GStreamer video sinks).
 * Returns body's result.
 */
int runPlatformMain(int argc, char **argv, int (*body)(int, char **, void *), void *user);

} // namespace nmqe::media
