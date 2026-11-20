#pragma once

#include "BoundedTrackQueue.h"
#include "catalog/Catalog.h"
#include "common/Config.h"
#include "common/EventLoop.h"
#include "common/Stats.h"
#include "media/api/MediaPipeline.h"
#include "mediaformat/CmafTrackParser.h"
#include "transport/ObjectSource.h"

#include <atomic>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace nmqe {

/**
 * Player core: owns lifecycle, track selection, join policy, bounded queues,
 * live-edge observation and stall/recovery state. All mutable state lives on
 * the player event loop; transport and media callbacks only post to it.
 */
class Player final : public IObjectSourceListener,
                     public media::IMediaPipelineClient,
                     public std::enable_shared_from_this<Player>
{
public:
	enum class State { Idle, Tuning, Playing, Stopped, Failed };

	static std::shared_ptr<Player> create(PlayerConfig config, std::unique_ptr<IObjectSource> source);
	~Player() override;

	/** Starts the tune; returns immediately. */
	void tune();
	/** Stops, emits the run summary and joins the loop. Idempotent. */
	void stop();
	/** Resolves once the player stops or fails on its own (e.g. session closed). */
	std::shared_future<State> ended() const { return mEnded; }

	// IObjectSourceListener (source threads)
	void onSourceEvent(const SourceEvent &event) override;
	void onObject(uint64_t subId, ReceivedObject object) override;

	// IMediaPipelineClient (backend threads)
	void notifyPlaybackState(media::PlaybackState state) override;
	void notifyNetworkState(media::NetworkState) override {}
	void notifyPosition(int64_t) override {}
	void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId) override;
	void notifyQos(int32_t sourceId, const media::QosInfo &qosInfo) override;
	void notifyBufferUnderflow(int32_t sourceId) override;
	void notifyFirstFrameReceived(int32_t sourceId) override;
	void notifyPlaybackError(int32_t sourceId, const std::string &error) override;
	void notifySourceFlushed(int32_t) override {}

private:
	static constexpr uint64_t kCatalogSubId = 1;
	static constexpr uint64_t kVideoSubId = 2;
	static constexpr uint64_t kAudioSubId = 3;

	struct Route
	{
		std::string track;
		std::shared_ptr<BoundedTrackQueue> queue;
		std::shared_ptr<std::atomic<bool>> drainPosted;
	};

	struct PresentMark
	{
		int64_t ptsNs;
		int64_t receiveMonoNs;
		uint64_t group;
	};

	struct Track
	{
		media::MediaSourceType type = media::MediaSourceType::UNKNOWN;
		std::string name;
		uint64_t subId = 0;
		std::unique_ptr<CmafTrackParser> parser;
		int32_t sourceId = -1;
		bool attached = false;
		bool joined = false;              // first random-access point seen (video) / start reached (audio)
		std::optional<uint64_t> lastGroup;
		std::deque<media::MediaSegment> pending; // demuxed, not yet submitted
		std::deque<PresentMark> marks;    // fragment starts submitted, awaiting presentation
		bool needPending = false;
		uint32_t needRequestId = 0;
		size_t needFrames = 0;
		int64_t newestEndPtsNs = INT64_MIN;
		int64_t submittedEndPtsNs = INT64_MIN;
		int64_t lastObjectMonoNs = 0;
		std::optional<ProducerReferenceTime> lastPrft;
		uint32_t resubscribes = 0;
		// counters
		uint64_t objects = 0, bytes = 0, groups = 0, groupGaps = 0, staleObjects = 0;
		uint64_t droppedQueueObjects = 0, droppedQueueBytes = 0, droppedPendingAus = 0;
		uint64_t joinDiscardedAus = 0, submittedAus = 0, parseErrors = 0;
		uint64_t qosDropped = 0, qosProcessed = 0;
	};

	Player(PlayerConfig config, std::unique_ptr<IObjectSource> source);

	void post(EventLoop::Task task);
	void milestone(const char *name, Json fields = Json());

	void handleSourceEvent(const SourceEvent &event);
	void drain(uint64_t subId);
	void handleCatalog(const ReceivedObject &object);
	void selectTracks(const Catalog &catalog);
	Track *createTrack(media::MediaSourceType type, const CatalogTrack &info, uint64_t subId);
	void subscribe(Track &track);
	void handleMediaObject(Track &track, const ReceivedObject &object);
	void tryAttach(Track &track);
	void maybeStartPlayback();
	void enforcePendingCap(Track &track);
	void serviceNeedData(Track &track);
	Track *trackBySource(int32_t sourceId);
	Track *trackBySub(uint64_t subId);

	void tick();
	void emitStatus();
	void emitSummary();
	void finish(State state, const std::string &reason);

	PlayerConfig mConfig;
	std::unique_ptr<IObjectSource> mSource;
	std::unique_ptr<media::IMediaPipeline> mPipeline;
	EventLoop mLoop;

	std::mutex mRouteMutex;
	std::map<uint64_t, Route> mRoutes;

	// Loop-owned state.
	State mState = State::Idle;
	std::optional<Catalog> mCatalog;
	std::string mCatalogText;
	uint64_t mCatalogObjects = 0;
	std::map<uint64_t, std::unique_ptr<Track>> mTracks;
	std::optional<int64_t> mStartPtsNs;
	bool mPlayRequested = false;
	bool mPipelinePlaying = false;
	int64_t mTuneMonoNs = 0;
	std::map<std::string, int64_t> mMilestones;  // name -> mono ns
	int64_t mFirstFrameMonoNs = 0;
	int64_t mLastPositionNs = INT64_MIN;
	int64_t mLastProgressMonoNs = 0;
	bool mStalled = false;
	int64_t mStallStartMonoNs = 0;
	int64_t mLastStallEndMonoNs = 0;
	uint64_t mStalls = 0;
	int64_t mStallTotalNs = 0;
	bool mStable = false;
	bool mStableLiveEdge = false;
	bool mBehindLiveEdge = false;
	int64_t mBehindStartMonoNs = 0;
	int64_t mBehindTotalNs = 0;
	std::deque<std::pair<int64_t, int64_t>> mEdgeWindow; // (mono ns, live-edge distance ns)
	int64_t mLastStatusMonoNs = 0;
	Distribution mLiveEdgeMs;          // after first frame
	Distribution mSteadyLiveEdgeMs;    // after stable live edge
	Distribution mReceiveToPresentMs;
	Distribution mAvOffsetMs;
	Distribution mPrftLatencyMs;
	double mMaxLiveEdgeMs = 0;
	double mLiveEdgeWeightedSum = 0;   // ms * s, for the time-weighted mean
	double mLiveEdgeWeightedTime = 0;  // s
	int64_t mLastEdgeSampleMonoNs = 0;
	std::string mEndReason;

	std::promise<State> mEndedPromise;
	std::shared_future<State> mEnded;
	bool mEndedSet = false;            // loop-owned
	std::atomic<bool> mStopped{false};
	std::atomic<bool> mTuneCalled{false};
};

const char *toString(Player::State state);

} // namespace nmqe
