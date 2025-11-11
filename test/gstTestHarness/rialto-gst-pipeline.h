#ifndef GST_MEDIA_PIPELINE_H
#define GST_MEDIA_PIPELINE_H

#include "IMediaPipeline.h"
// Rialto gstreamer client side helpers
#include "GStreamerMSEMediaPlayerClient.h"
#include "MediaPlayerClientBackend.h"
#include "IMessageQueue.h"
#include <gst/gst.h>

using namespace firebolt::rialto;

class GstMediaPipeline:IMediaPipeline
{
private:
    // New Rialto-backed objects replacing manual playbin/appsrc construction
    std::shared_ptr<IMessageQueueFactory> m_messageQueueFactory; // creates async queues used by client
    std::shared_ptr<firebolt::rialto::client::MediaPlayerClientBackendInterface> m_clientBackend; // thin wrapper over IMediaPipeline
    std::shared_ptr<GStreamerMSEMediaPlayerClient> m_mseClient; // implements IMediaPipelineClient callbacks

protected:
    std::weak_ptr<IMediaPipelineClient> client; // legacy handle if external code sets it
    std::weak_ptr<IMediaPipelineClient> getClient(){ return client; }

public:
    // Legacy helper retained for compatibility (now unused)
    void found_source( GObject *orig, GParamSpec *pspec, int sourceId ){};
    GstMediaPipeline();
    ~GstMediaPipeline();
	
	bool pause(){ return false; };
	bool stop(){ return false; };
	bool setPlaybackRate(double rate){ return false; };
	bool setPosition(int64_t position){ return false; };
	bool getPosition(int64_t &position){ return false; };
	bool getStats(int32_t sourceId, uint64_t &renderedFrames, uint64_t &droppedFrames){ return false; };
	bool setImmediateOutput(int32_t sourceId, bool immediateOutput){ return false; };
	bool getImmediateOutput(int32_t sourceId, bool &immediateOutput){ return false; };
	bool setVideoWindow(uint32_t x, uint32_t y, uint32_t width, uint32_t height){ return false; };
	bool haveData(MediaSourceStatus status, uint32_t needDataRequestId){ return false; };
	bool renderFrame(){ return false; };
	bool setVolume(double targetVolume, uint32_t volumeDuration = 0, EaseType easeType = EaseType::EASE_LINEAR){ return false; };
	bool getTextTrackIdentifier(std::string &textTrackIdentifier){ return false; };
	bool setLowLatency(bool lowLatency){ return false; };
	bool setSync(bool sync){ return false; };
	bool getSync(bool &sync){ return false; };
	bool setSyncOff(bool syncOff){ return false; };
	bool setStreamSyncMode(int32_t sourceId, int32_t streamSyncMode){ return false; };
	bool getStreamSyncMode(int32_t &streamSyncMode){ return false; };
	bool flush(int32_t sourceId, bool resetTime, bool &async){ return false; };
	bool setSourcePosition(int32_t sourceId, int64_t position, bool resetTime = false, double appliedRate = 1.0, uint64_t stopPosition = kUndefinedPosition){ return false; };
	bool setSubtitleOffset(int32_t sourceId, int64_t position){ return false; };
	bool processAudioGap(int64_t position, uint32_t duration, int64_t discontinuityGap, bool audioAac){ return false; };
	bool setBufferingLimit(uint32_t limitBufferingMs){ return false; };
	bool getBufferingLimit(uint32_t &limitBufferingMs){ return false; };
	bool setUseBuffering(bool useBuffering){ return false; };
	bool getUseBuffering(bool &useBuffering){ return false; };
	bool switchSource(const std::unique_ptr<MediaSource> &source){ return false; };
	bool getVolume(double &currentVolume){ return false; }
	bool setMute(int32_t sourceId, bool mute){ return false; }
	bool getMute(int32_t sourceId, bool &mute){ return false; }
	bool setTextTrackIdentifier(const std::string &textTrackIdentifier){ return false; }
	
	// Rialto native pattern: attach source; after true, caller reads source->getId()
	bool attachSource(const std::unique_ptr<MediaSource> &source);
	bool removeSource(int32_t id);
	bool allSourcesAttached();
	bool load(MediaType type, const std::string &mimeType, const std::string &url);
	bool play();
	AddSegmentStatus addSegment(uint32_t needDataRequestId, const std::unique_ptr<MediaSegment> &mediaSegment);
};

#endif // GST_MEDIA_PIPELINE_H
