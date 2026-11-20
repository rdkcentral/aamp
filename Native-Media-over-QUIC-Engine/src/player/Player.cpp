#include "Player.h"

#include "common/Clock.h"
#include "common/Log.h"
#include "common/ProcessStats.h"
#include "common/Telemetry.h"

#include <algorithm>
#include <cinttypes>

namespace nmqe {

namespace {

constexpr int64_t kTickMs = 100;
constexpr int64_t kStallThresholdMs = 500;      // no position progress while playing
constexpr int64_t kLiveEdgeStableToleranceMs = 250;
constexpr int64_t kResubscribeDelayMs = 500;
constexpr uint32_t kMaxResubscribes = 5;
constexpr uint64_t kCatalogQueueBytes = 1024 * 1024;

int8_t priorityOf(media::MediaSourceType type)
{
	// Higher first: audio is cheapest to keep continuous.
	return type == media::MediaSourceType::AUDIO ? 2 : 1;
}

double nsToMs(int64_t ns)
{
	return static_cast<double>(ns) / 1e6;
}

} // namespace

const char *toString(Player::State state)
{
	switch (state)
	{
		case Player::State::Idle: return "idle";
		case Player::State::Tuning: return "tuning";
		case Player::State::Playing: return "playing";
		case Player::State::Stopped: return "stopped";
		case Player::State::Failed: return "failed";
	}
	return "unknown";
}

std::shared_ptr<Player> Player::create(PlayerConfig config, std::unique_ptr<ObjectSource> source)
{
	return std::shared_ptr<Player>(new Player(std::move(config), std::move(source)));
}

Player::Player(PlayerConfig config, std::unique_ptr<ObjectSource> source)
	: mConfig(std::move(config)), mSource(std::move(source)), mEnded(mEndedPromise.get_future().share())
{
}

Player::~Player()
{
	stop();
}

void Player::post(EventLoop::Task task)
{
	mLoop.post(std::move(task));
}

void Player::milestone(const char *name, Json fields)
{
	if (mMilestones.count(name)) return;
	const int64_t now = monoNs();
	mMilestones[name] = now;
	fields.add("milestone", name).add("t_ms", nsToMs(now - mTuneMonoNs));
	Telemetry::instance().emit("tune", "milestone", fields);
	LOG_INFO("player", "milestone %-22s +%8.1f ms", name, nsToMs(now - mTuneMonoNs));
}

void Player::tune()
{
	mTuneCalled = true;
	mLoop.start();
	post([this] {
		mTuneMonoNs = monoNs();
		mState = State::Tuning;
		Json run;
		run.raw("config", mConfig.toJson()).add("source", mSource->describe());
		Telemetry::instance().emit("session", "run_config", run);
		milestone("tune_requested");

		media::PlatformSettings settings;
		settings.videoDecoder = mConfig.platform.videoDecoder;
		settings.audioDecoder = mConfig.platform.audioDecoder;
		settings.videoSink = mConfig.platform.videoSink;
		settings.audioSink = mConfig.platform.audioSink;
		settings.sinkSync = mConfig.platform.sinkSync;
		settings.appsrcMaxBytes = mConfig.platform.appsrcMaxBytes;
		settings.sinkProcessingDeadlineMs = mConfig.platform.sinkProcessingDeadlineMs;
		settings.pipelineLatencyMs = mConfig.platform.pipelineLatencyMs;
		mPipeline = media::createMediaPipeline(mConfig.pipeline, weak_from_this(), settings);
		if (!mPipeline || !mPipeline->load(true))
		{
			finish(State::Failed, "cannot create media pipeline '" + mConfig.pipeline + "'");
			return;
		}

		{
			std::lock_guard<std::mutex> lock(mRouteMutex);
			mRoutes[kCatalogSubId] = Route{mConfig.catalogTrack, std::make_shared<BoundedTrackQueue>(kCatalogQueueBytes),
			                               std::make_shared<std::atomic<bool>>(false)};
		}
		if (!mSource->start(this))
		{
			finish(State::Failed, "cannot start object source");
			return;
		}
		TrackSubscription catalog;
		catalog.track = mConfig.catalogTrack;
		catalog.subId = kCatalogSubId;
		catalog.priority = 3;
		catalog.maxDelayMs = 0;
		catalog.liveJoin = true;
		mSource->subscribe(catalog);
		milestone("catalog_subscribe_sent");
		mLoop.postAfterMs(kTickMs, [this] { tick(); });
	});
}

void Player::stop()
{
	if (mStopped.exchange(true)) return;
	if (!mTuneCalled)
	{
		mLoop.stop();
		return;
	}
	std::promise<void> done;
	auto finished = done.get_future();
	post([this, &done] {
		for (auto &[subId, track] : mTracks) mSource->unsubscribe(subId);
		mSource->stop();
		if (mPipeline) mPipeline->stop();
		if (mState != State::Failed) mState = State::Stopped;
		if (mEndReason.empty()) mEndReason = "stopped";
		emitSummary();
		finish(mState, mEndReason);
		done.set_value();
	});
	if (finished.wait_for(std::chrono::seconds(10)) != std::future_status::ready)
	{
		LOG_ERROR("player", "stop timed out");
	}
	mLoop.stop();
}

void Player::finish(State state, const std::string &reason)
{
	if (state == State::Failed) mState = State::Failed;
	if (mEndReason.empty() || mEndReason == "stopped") mEndReason = reason;
	if (mEndedSet) return;
	mEndedSet = true;
	if (state == State::Failed) LOG_ERROR("player", "failed: %s", reason.c_str());
	else LOG_INFO("player", "ending: %s", reason.c_str());
	mEndedPromise.set_value(mState == State::Failed ? State::Failed : state);
}

// ---- source callbacks (source threads) ------------------------------------------------------

void Player::onSourceEvent(const SourceEvent &event)
{
	auto self = weak_from_this();
	post([self, event] {
		if (auto player = self.lock()) player->handleSourceEvent(event);
	});
}

void Player::onObject(uint64_t subId, ReceivedObject object)
{
	std::shared_ptr<BoundedTrackQueue> queue;
	std::shared_ptr<std::atomic<bool>> drainPosted;
	{
		std::lock_guard<std::mutex> lock(mRouteMutex);
		auto it = mRoutes.find(subId);
		if (it == mRoutes.end()) return;
		object.id.track = it->second.track;
		queue = it->second.queue;
		drainPosted = it->second.drainPosted;
	}
	if (Telemetry::instance().tracing())
	{
		Json fields;
		fields.add("track", object.id.track).add("group", object.id.group).add("object", object.id.object)
			.add("size", static_cast<uint64_t>(object.payload ? object.payload->size() : 0)).add("recv_wall_ms", object.receiveWallMs);
		Telemetry::instance().trace("object", "receive", fields);
	}
	const std::string track = object.id.track;
	BoundedTrackQueue::Dropped dropped = queue->push(std::move(object));
	if (dropped.objects)
	{
		Json fields;
		fields.add("track", track).add("objects", dropped.objects).add("bytes", dropped.bytes)
			.add("groups", static_cast<uint64_t>(dropped.groups.size())).add("first_group", dropped.groups.front())
			.add("queue_max_bytes", queue->maxBytes());
		Telemetry::instance().emit("queue", "overflow_drop", fields);
		auto self = weak_from_this();
		post([self, subId, dropped] {
			auto player = self.lock();
			if (!player) return;
			if (Track *t = player->trackBySub(subId))
			{
				t->droppedQueueObjects += dropped.objects;
				t->droppedQueueBytes += dropped.bytes;
			}
		});
	}
	if (!drainPosted->exchange(true))
	{
		auto self = weak_from_this();
		post([self, subId] {
			if (auto player = self.lock()) player->drain(subId);
		});
	}
}

// ---- media callbacks (backend threads) ------------------------------------------------------

void Player::notifyPlaybackState(media::PlaybackState state)
{
	auto self = weak_from_this();
	post([self, state] {
		auto player = self.lock();
		if (!player) return;
		Json fields;
		fields.add("state", media::toString(state));
		Telemetry::instance().emit("media", "playback_state", fields);
		if (state == media::PlaybackState::PLAYING)
		{
			player->mPipelinePlaying = true;
			player->milestone("pipeline_playing");
		}
		else if (state == media::PlaybackState::PAUSED)
		{
			player->mPipelinePlaying = false;
		}
	});
}

void Player::notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId)
{
	auto self = weak_from_this();
	post([self, sourceId, frameCount, needDataRequestId] {
		auto player = self.lock();
		if (!player) return;
		Track *track = player->trackBySource(sourceId);
		if (!track) return;
		track->needPending = true;
		track->needRequestId = needDataRequestId;
		track->needFrames = frameCount;
		player->serviceNeedData(*track);
	});
}

void Player::notifyQos(int32_t sourceId, const media::QosInfo &qosInfo)
{
	auto self = weak_from_this();
	post([self, sourceId, qosInfo] {
		auto player = self.lock();
		if (!player) return;
		if (Track *track = player->trackBySource(sourceId))
		{
			track->qosProcessed = qosInfo.processed;
			track->qosDropped = qosInfo.dropped;
		}
		Json fields;
		fields.add("source_id", sourceId).add("processed", qosInfo.processed).add("dropped", qosInfo.dropped);
		Telemetry::instance().trace("media", "qos", fields);
	});
}

void Player::notifyBufferUnderflow(int32_t sourceId)
{
	auto self = weak_from_this();
	post([self, sourceId] {
		auto player = self.lock();
		if (!player) return;
		Track *track = player->trackBySource(sourceId);
		Json fields;
		fields.add("track", track ? track->name : std::string()).add("source_id", sourceId);
		Telemetry::instance().emit("media", "buffer_underflow", fields);
	});
}

void Player::notifyFirstFrameReceived(int32_t sourceId)
{
	auto self = weak_from_this();
	post([self, sourceId] {
		auto player = self.lock();
		if (!player) return;
		Track *track = player->trackBySource(sourceId);
		if (!track) return;
		const bool video = track->type == media::MediaSourceType::VIDEO;
		player->milestone(video ? "first_video_at_sink" : "first_audio_at_sink");
		const bool hasVideo = player->mTracks.count(kVideoSubId) > 0;
		if (video || !hasVideo)
		{
			player->milestone("first_frame");
			player->mFirstFrameMonoNs = monoNs();
			player->mLastProgressMonoNs = player->mFirstFrameMonoNs;
			player->mState = State::Playing;
		}
	});
}

void Player::notifyPlaybackError(int32_t sourceId, const std::string &error)
{
	auto self = weak_from_this();
	post([self, sourceId, error] {
		auto player = self.lock();
		if (!player) return;
		Json fields;
		fields.add("source_id", sourceId).add("error", error);
		Telemetry::instance().emit("media", "error", fields);
		player->finish(State::Failed, "media error: " + error);
	});
}

// ---- loop-side handling ---------------------------------------------------------------------

Player::Track *Player::trackBySource(int32_t sourceId)
{
	for (auto &[subId, track] : mTracks)
	{
		if (track->sourceId == sourceId) return track.get();
	}
	return nullptr;
}

Player::Track *Player::trackBySub(uint64_t subId)
{
	auto it = mTracks.find(subId);
	return it == mTracks.end() ? nullptr : it->second.get();
}

void Player::handleSourceEvent(const SourceEvent &event)
{
	const bool perGroup = event.kind == SourceEvent::Kind::GroupBegin || event.kind == SourceEvent::Kind::GroupEnd;
	Json fields;
	fields.add("sub_id", event.subId);
	if (perGroup) fields.add("group", event.group);
	if (event.kind == SourceEvent::Kind::GroupEnd) fields.add("objects", event.objectCount);
	if (!event.detail.empty()) fields.add("detail", event.detail);
	if (perGroup && event.detail.empty()) Telemetry::instance().trace("transport", toString(event.kind), fields);
	else Telemetry::instance().emit("transport", toString(event.kind), fields);

	switch (event.kind)
	{
		case SourceEvent::Kind::Connecting:
			milestone("transport_connecting");
			break;
		case SourceEvent::Kind::Connected:
		{
			Json m;
			m.add("version", event.detail);
			milestone("setup_complete", m);
			break;
		}
		case SourceEvent::Kind::BroadcastReady:
			milestone("broadcast_ready");
			break;
		case SourceEvent::Kind::Subscribed:
		{
			const std::string name = event.subId == kCatalogSubId ? "subscribed_catalog"
			                         : event.subId == kVideoSubId ? "subscribed_video"
			                                                      : "subscribed_audio";
			milestone(name.c_str());
			break;
		}
		case SourceEvent::Kind::SubscriptionEnded:
		{
			if (event.detail.empty() || mStopped) break;
			Track *track = trackBySub(event.subId);
			const bool catalog = event.subId == kCatalogSubId;
			uint32_t attempts = track ? ++track->resubscribes : 0;
			if (!catalog && !track) break;
			if (attempts > kMaxResubscribes)
			{
				finish(State::Failed, "subscription " + std::to_string(event.subId) + " failed: " + event.detail);
				break;
			}
			Json r;
			r.add("sub_id", event.subId).add("action", "resubscribe").add("attempt", attempts).add("cause", event.detail);
			Telemetry::instance().emit("recovery", "action", r);
			const uint64_t subId = event.subId;
			mLoop.postAfterMs(kResubscribeDelayMs, [this, subId] {
				if (mStopped) return;
				if (Track *t = trackBySub(subId))
				{
					subscribe(*t);
				}
				else if (subId == kCatalogSubId)
				{
					TrackSubscription catalog;
					catalog.track = mConfig.catalogTrack;
					catalog.subId = kCatalogSubId;
					catalog.priority = 3;
					catalog.liveJoin = true;
					mSource->subscribe(catalog);
				}
			});
			break;
		}
		case SourceEvent::Kind::Closed:
			if (!mStopped) finish(State::Failed, event.detail.empty() ? "session closed" : event.detail);
			break;
		default:
			break;
	}
}

void Player::drain(uint64_t subId)
{
	Route route;
	{
		std::lock_guard<std::mutex> lock(mRouteMutex);
		auto it = mRoutes.find(subId);
		if (it == mRoutes.end()) return;
		route = it->second;
	}
	route.drainPosted->store(false);
	for (auto &object : route.queue->drain())
	{
		if (subId == kCatalogSubId)
		{
			handleCatalog(object);
		}
		else if (Track *track = trackBySub(subId))
		{
			handleMediaObject(*track, object);
		}
	}
}

void Player::handleCatalog(const ReceivedObject &object)
{
	++mCatalogObjects;
	if (!object.payload || object.payload->empty()) return;
	std::string text(object.payload->begin(), object.payload->end());
	if (text == mCatalogText) return; // the catalog is republished unchanged in every Group
	std::string error;
	auto catalog = Catalog::parse(object.payload->data(), object.payload->size(), error);
	if (!catalog || catalog->isDelta)
	{
		Json fields;
		fields.add("group", object.id.group).add("error", error);
		Telemetry::instance().emit("catalog", "parse_error", fields);
		if (!mCatalog) finish(State::Failed, "catalog: " + error);
		return;
	}

	std::string tracks = "[";
	for (const auto &track : catalog->tracks)
	{
		if (tracks.size() > 1) tracks += ',';
		Json t;
		t.add("name", track.name).add("packaging", track.packaging).add("role", track.role).add("codec", track.codec)
			.add("width", track.width).add("height", track.height).add("samplerate", track.samplerate)
			.add("init_bytes", static_cast<uint64_t>(track.initData.size())).add("target_latency_ms", track.targetLatencyMs)
			.add("is_live", track.isLive);
		if (track.altGroup) t.add("alt_group", *track.altGroup);
		if (track.renderGroup) t.add("render_group", *track.renderGroup);
		std::string unknown = "[";
		for (const auto &field : track.unknownFields) unknown += (unknown.size() > 1 ? ",\"" : "\"") + Json::escape(field) + "\"";
		t.raw("unknown_fields", unknown + "]");
		tracks += t.object();
	}
	tracks += "]";
	Json fields;
	fields.add("version", catalog->version).add("group", object.id.group).add("bytes", static_cast<uint64_t>(object.payload->size()))
		.raw("tracks", tracks).add("update", mCatalog.has_value());
	Telemetry::instance().emit("catalog", mCatalog ? "changed" : "received", fields);

	const bool first = !mCatalog;
	mCatalogText = std::move(text);
	mCatalog = std::move(catalog);
	if (first)
	{
		milestone("catalog_received");
		selectTracks(*mCatalog);
	}
	else
	{
		// Catalog updates are recorded; the selected tracks are kept (no switching in the POC).
		LOG_WARN("player", "catalog changed in group %" PRIu64 "; keeping the current selection", object.id.group);
	}
}

void Player::selectTracks(const Catalog &catalog)
{
	const CatalogTrack *video = mConfig.videoEnabled ? catalog.select("video", mConfig.videoTrack) : nullptr;
	const CatalogTrack *audio = mConfig.audioEnabled ? catalog.select("audio", mConfig.audioTrack) : nullptr;
	if (!video && !audio)
	{
		finish(State::Failed, "no playable track selected from the catalog");
		return;
	}
	if (mConfig.targetLatencyMs < 0)
	{
		mConfig.targetLatencyMs = std::max(video ? video->targetLatencyMs : 0, audio ? audio->targetLatencyMs : 0);
	}
	Json fields;
	fields.add("video", video ? video->name : std::string()).add("audio", audio ? audio->name : std::string())
		.add("target_latency_ms", mConfig.targetLatencyMs);
	Telemetry::instance().emit("player", "track_selection", fields);

	std::vector<Track *> created;
	if (video) created.push_back(createTrack(media::MediaSourceType::VIDEO, *video, kVideoSubId));
	if (audio) created.push_back(createTrack(media::MediaSourceType::AUDIO, *audio, kAudioSubId));
	for (Track *track : created) subscribe(*track);
	for (Track *track : created) tryAttach(*track);
}

Player::Track *Player::createTrack(media::MediaSourceType type, const CatalogTrack &info, uint64_t subId)
{
	auto track = std::make_unique<Track>();
	track->type = type;
	track->name = info.name;
	track->subId = subId;
	track->parser = std::make_unique<CmafTrackParser>(info.name, type);
	if (!info.initData.empty())
	{
		std::string error;
		if (track->parser->setInit(info.initData, error))
		{
			const media::MediaSource &source = track->parser->source();
			Json m;
			m.add("track", info.name).add("mime", source.mimeType).add("timescale", track->parser->timescale())
				.add("codec_data_bytes", static_cast<uint64_t>(source.codecData.size())).add("init_from", "catalog");
			milestone(type == media::MediaSourceType::VIDEO ? "init_video" : "init_audio", m);
		}
		else
		{
			Json e;
			e.add("track", info.name).add("error", error);
			Telemetry::instance().emit("media", "init_error", e);
		}
	}
	{
		std::lock_guard<std::mutex> lock(mRouteMutex);
		mRoutes[subId] = Route{info.name, std::make_shared<BoundedTrackQueue>(mConfig.queueMaxBytes),
		                       std::make_shared<std::atomic<bool>>(false)};
	}
	Track *raw = track.get();
	mTracks[subId] = std::move(track);
	return raw;
}

void Player::subscribe(Track &track)
{
	TrackSubscription subscription;
	subscription.track = track.name;
	subscription.subId = track.subId;
	subscription.priority = priorityOf(track.type);
	subscription.maxDelayMs = mConfig.transportMaxDelayMs;
	subscription.liveJoin = true;
	mSource->subscribe(subscription);
	milestone(track.type == media::MediaSourceType::VIDEO ? "subscribe_video_sent" : "subscribe_audio_sent");
}

void Player::tryAttach(Track &track)
{
	if (track.attached || !track.parser->hasInit() || !mPipeline) return;
	media::MediaSource source = track.parser->source();
	if (!mPipeline->attachSource(source))
	{
		finish(State::Failed, "cannot attach " + track.name + " (" + source.mimeType + ")");
		return;
	}
	track.sourceId = source.id;
	track.parser->setSourceId(source.id);
	track.attached = true;
	Json fields;
	fields.add("track", track.name).add("source_id", source.id).add("mime", source.mimeType).add("width", source.width)
		.add("height", source.height).add("rate", source.sampleRate).add("channels", source.numberOfChannels);
	Telemetry::instance().emit("media", "source_attached", fields);
	maybeStartPlayback();
}

void Player::handleMediaObject(Track &track, const ReceivedObject &object)
{
	const bool video = track.type == media::MediaSourceType::VIDEO;
	const size_t size = object.payload ? object.payload->size() : 0;
	if (track.objects == 0)
	{
		Json m;
		m.add("track", track.name).add("group", object.id.group).add("object", object.id.object).add("size", static_cast<uint64_t>(size))
			.add("boxes", object.payload ? CmafTrackParser::boxSummary(object.payload->data(), size) : std::string());
		milestone(video ? "first_object_video" : "first_object_audio", m);
	}
	++track.objects;
	track.bytes += size;
	track.lastObjectMonoNs = object.receiveMonoNs;

	if (track.lastGroup)
	{
		if (object.id.group < *track.lastGroup)
		{
			++track.staleObjects;
			Json fields;
			fields.add("track", track.name).add("group", object.id.group).add("last_group", *track.lastGroup);
			Telemetry::instance().emit("discontinuity", "stale_object", fields);
			return;
		}
		if (object.id.group > *track.lastGroup + 1)
		{
			const uint64_t missing = object.id.group - *track.lastGroup - 1;
			track.groupGaps += missing;
			Json fields;
			fields.add("track", track.name).add("from_group", *track.lastGroup).add("to_group", object.id.group).add("missing", missing);
			Telemetry::instance().emit("discontinuity", "group_gap", fields);
		}
	}
	if (!track.lastGroup || object.id.group != *track.lastGroup) ++track.groups;
	track.lastGroup = object.id.group;

	const std::string previousSignature = track.parser->codecSignature();
	CmafFragment fragment;
	std::string error;
	if (!track.parser->parseFragment(object.payload, fragment, error))
	{
		++track.parseErrors;
		Json fields;
		fields.add("track", track.name).add("group", object.id.group).add("object", object.id.object).add("error", error)
			.add("boxes", CmafTrackParser::boxSummary(object.payload->data(), size));
		Telemetry::instance().emit("media", "parse_error", fields);
		return;
	}
	if (fragment.hadInit)
	{
		if (!track.attached)
		{
			Json m;
			m.add("track", track.name).add("mime", track.parser->source().mimeType).add("init_from", "in-band");
			milestone(video ? "init_video" : "init_audio", m);
			tryAttach(track);
		}
		else if (track.parser->codecSignature() != previousSignature)
		{
			Json fields;
			fields.add("track", track.name).add("group", object.id.group);
			Telemetry::instance().emit("discontinuity", "codec_changed_unsupported", fields);
		}
	}
	if (fragment.prft)
	{
		if (!track.lastPrft)
		{
			Json m;
			m.add("track", track.name).add("reference_track", fragment.prft->referenceTrackId).add("flags", fragment.prft->flags);
			milestone(video ? "first_prft_video" : "first_prft_audio", m);
		}
		track.lastPrft = fragment.prft;
	}
	if (Telemetry::instance().tracing())
	{
		Json fields;
		fields.add("track", track.name).add("group", object.id.group).add("object", object.id.object)
			.add("samples", static_cast<uint64_t>(fragment.segments.size())).add("key_frames", static_cast<uint64_t>(fragment.keyFrames))
			.add("starts_with_key", fragment.startsWithKeyFrame).add("start_pts_ms", nsToMs(fragment.startPtsNs))
			.add("end_pts_ms", nsToMs(fragment.endPtsNs)).add("dur_ms", nsToMs(fragment.endPtsNs - fragment.startPtsNs))
			.add("tfdt", fragment.baseMediaDecodeTime).add("prft", fragment.prft.has_value());
		if (fragment.prft) fields.add("prft_wall_ms", fragment.prft->wallMs()).add("prft_media", fragment.prft->mediaTime);
		Telemetry::instance().trace("media", "fragment", fields);
	}
	if (fragment.segments.empty()) return;
	if (video && !fragment.startsWithKeyFrame && track.objects <= 3)
	{
		// Random-access evidence for the integration contract: Groups are expected to start at a SAP.
		Json fields;
		fields.add("track", track.name).add("group", object.id.group).add("object", object.id.object);
		Telemetry::instance().emit("media", "fragment_without_leading_sap", fields);
	}
	track.newestEndPtsNs = std::max(track.newestEndPtsNs, fragment.endPtsNs);

	bool markPending = true;
	for (auto &au : fragment.segments)
	{
		if (!track.joined)
		{
			if (video && !au.keyFrame)
			{
				++track.joinDiscardedAus;
				continue;
			}
			track.joined = true;
			Json m;
			m.add("track", track.name).add("group", object.id.group).add("object", object.id.object).add("pts_ms", nsToMs(au.timeStampNs))
				.add("discarded_aus", track.joinDiscardedAus);
			milestone(video ? "join_video" : "join_audio", m);
			if (!mStartPtsNs && (video || !mTracks.count(kVideoSubId))) mStartPtsNs = au.timeStampNs;
		}
		au.group = object.id.group;
		au.object = object.id.object;
		au.receiveMonoNs = object.receiveMonoNs;
		if (markPending)
		{
			track.marks.push_back(PresentMark{au.timeStampNs, object.receiveMonoNs, object.id.group});
			markPending = false;
		}
		track.pending.push_back(std::move(au));
	}
	enforcePendingCap(track);
	maybeStartPlayback();
	serviceNeedData(track);
}

void Player::enforcePendingCap(Track &track)
{
	const int64_t capNs = mConfig.queueMaxDurationMs * kNsPerMs;
	uint64_t dropped = 0;
	const bool video = track.type == media::MediaSourceType::VIDEO;
	while (track.pending.size() > 1 && track.pending.back().timeStampNs - track.pending.front().timeStampNs > capNs)
	{
		// Drop the oldest access units; for video, through to the next random-access point.
		track.pending.pop_front();
		++dropped;
		while (video && !track.pending.empty() && !track.pending.front().keyFrame)
		{
			track.pending.pop_front();
			++dropped;
		}
	}
	if (!dropped) return;
	track.droppedPendingAus += dropped;
	Json fields;
	fields.add("track", track.name).add("aus", dropped).add("cap_ms", mConfig.queueMaxDurationMs);
	Telemetry::instance().emit("queue", "duration_drop", fields);
}

void Player::maybeStartPlayback()
{
	if (mPlayRequested || !mStartPtsNs || !mPipeline || mTracks.empty()) return;
	for (auto &[subId, track] : mTracks)
	{
		if (!track->attached) return;
	}
	// Audio that precedes the video start is trimmed so both branches share one origin.
	for (auto &[subId, track] : mTracks)
	{
		uint64_t trimmed = 0;
		while (!track->pending.empty() && track->pending.front().timeStampNs + track->pending.front().durationNs <= *mStartPtsNs)
		{
			track->pending.pop_front();
			++trimmed;
		}
		while (!track->marks.empty() && track->marks.front().ptsNs < *mStartPtsNs) track->marks.pop_front();
		track->joinDiscardedAus += trimmed;
	}
	mPipeline->setPosition(*mStartPtsNs);
	if (!mPipeline->allSourcesAttached() || !mPipeline->play())
	{
		finish(State::Failed, "pipeline refused to play");
		return;
	}
	mPlayRequested = true;
	Json m;
	m.add("start_pts_ms", nsToMs(*mStartPtsNs));
	milestone("play_requested", m);
	Telemetry::instance().emit("media", "pipeline", Json().raw("describe", mPipeline->describe()));
	for (auto &[subId, track] : mTracks) serviceNeedData(*track);
}

void Player::serviceNeedData(Track &track)
{
	if (!track.needPending || !mPlayRequested || !track.attached) return;
	// An empty queue keeps the request open; it is answered as soon as data arrives.
	if (track.pending.empty()) return;
	size_t submitted = 0;
	const bool video = track.type == media::MediaSourceType::VIDEO;
	while (submitted < track.needFrames && !track.pending.empty())
	{
		media::MediaSegment &au = track.pending.front();
		au.sourceId = track.sourceId;
		const media::AddSegmentStatus status = mPipeline->addSegment(track.needRequestId, au);
		if (status == media::AddSegmentStatus::NO_SPACE) break;
		if (status == media::AddSegmentStatus::ERROR)
		{
			Json fields;
			fields.add("track", track.name).add("pts_ms", nsToMs(au.timeStampNs));
			Telemetry::instance().emit("media", "add_segment_error", fields);
		}
		else
		{
			if (track.submittedAus == 0) milestone(video ? "first_video_submitted" : "first_audio_submitted");
			++track.submittedAus;
			track.submittedEndPtsNs = std::max(track.submittedEndPtsNs, au.timeStampNs + au.durationNs);
			if (Telemetry::instance().tracing())
			{
				Json fields;
				fields.add("track", track.name).add("pts_ms", nsToMs(au.timeStampNs)).add("key", au.keyFrame).add("size", static_cast<uint64_t>(au.size))
					.add("group", au.group).add("queue_ms", nsToMs(monoNs() - au.receiveMonoNs));
				Telemetry::instance().trace("media", "submit", fields);
			}
		}
		track.pending.pop_front();
		++submitted;
	}
	mPipeline->haveData(submitted ? media::MediaSourceStatus::OK : media::MediaSourceStatus::NO_AVAILABLE_SAMPLES, track.needRequestId);
	track.needPending = false;
}

void Player::tick()
{
	if (mStopped) return;
	const int64_t now = monoNs();
	int64_t position = 0;
	const bool havePosition = mPipeline && mFirstFrameMonoNs && mPipeline->getPosition(position);

	if (havePosition)
	{
		if (mLastPositionNs == INT64_MIN || position > mLastPositionNs + kNsPerMs)
		{
			mLastProgressMonoNs = now;
			if (mStalled)
			{
				mStalled = false;
				mLastStallEndMonoNs = now;
				const int64_t duration = now - mStallStartMonoNs;
				mStallTotalNs += duration;
				Json fields;
				fields.add("duration_ms", nsToMs(duration)).add("position_ms", nsToMs(position));
				Telemetry::instance().emit("stall", "end", fields);
				mEdgeWindow.clear();
			}
		}
		else if (!mStalled && mPipelinePlaying && now - mLastProgressMonoNs > kStallThresholdMs * kNsPerMs)
		{
			mStalled = true;
			mStallStartMonoNs = mLastProgressMonoNs;
			++mStalls;
			Json fields;
			fields.add("position_ms", nsToMs(position));
			for (auto &[subId, track] : mTracks)
			{
				const int64_t ahead = track->pending.empty() ? 0 : track->pending.back().timeStampNs - position;
				fields.add(track->type == media::MediaSourceType::VIDEO ? "video_pending_ms" : "audio_pending_ms", nsToMs(ahead));
				fields.add(track->type == media::MediaSourceType::VIDEO ? "video_since_object_ms" : "audio_since_object_ms",
				           nsToMs(now - track->lastObjectMonoNs));
			}
			Telemetry::instance().emit("stall", "start", fields);
		}
		mLastPositionNs = position;

		// Live edge: rendered distance from the newest media known to this player.
		Track *reference = trackBySub(kVideoSubId) ? trackBySub(kVideoSubId) : trackBySub(kAudioSubId);
		if (reference && reference->newestEndPtsNs != INT64_MIN)
		{
			const int64_t distanceNs = reference->newestEndPtsNs - position;
			const double distanceMs = nsToMs(distanceNs);
			mLiveEdgeMs.add(distanceMs);
			mMaxLiveEdgeMs = std::max(mMaxLiveEdgeMs, distanceMs);
			if (mLastEdgeSampleMonoNs)
			{
				const double elapsedSec = static_cast<double>(now - mLastEdgeSampleMonoNs) / 1e9;
				mLiveEdgeWeightedSum += distanceMs * elapsedSec;
				mLiveEdgeWeightedTime += elapsedSec;
			}
			mLastEdgeSampleMonoNs = now;
			mEdgeWindow.emplace_back(now, distanceNs);
			while (!mEdgeWindow.empty() && now - mEdgeWindow.front().first > 2 * mConfig.stableWindowMs * kNsPerMs) mEdgeWindow.pop_front();
			if (mStableLiveEdge) mSteadyLiveEdgeMs.add(distanceMs);

			const bool behind = distanceMs > static_cast<double>(mConfig.catchUpThresholdMs);
			if (behind != mBehindLiveEdge)
			{
				mBehindLiveEdge = behind;
				Json fields;
				fields.add("state", behind ? "behind" : "within").add("live_edge_ms", distanceMs).add("threshold_ms", mConfig.catchUpThresholdMs)
					.add("action", "none");
				if (behind) mBehindStartMonoNs = now;
				else
				{
					mBehindTotalNs += now - mBehindStartMonoNs;
					fields.add("behind_ms", nsToMs(now - mBehindStartMonoNs));
				}
				Telemetry::instance().emit("catch_up", "state", fields);
			}

			// The distance saw-tooths by one fragment duration as whole fragments arrive, so stability
			// is judged on its floor: the minimum over each of two consecutive windows must agree.
			if (mStable && !mStableLiveEdge && !mStalled && !mEdgeWindow.empty() &&
			    now - mEdgeWindow.front().first >= (2 * mConfig.stableWindowMs - kTickMs) * kNsPerMs)
			{
				const int64_t split = now - mConfig.stableWindowMs * kNsPerMs;
				int64_t olderFloor = INT64_MAX, recentFloor = INT64_MAX;
				for (auto &[t, d] : mEdgeWindow)
				{
					int64_t &floor = t < split ? olderFloor : recentFloor;
					floor = std::min(floor, d);
				}
				const int64_t drift = recentFloor - olderFloor;
				if (std::abs(drift) <= kLiveEdgeStableToleranceMs * kNsPerMs && !behind)
				{
					mStableLiveEdge = true;
					Json m;
					m.add("live_edge_ms", distanceMs).add("floor_ms", nsToMs(recentFloor)).add("floor_drift_ms", nsToMs(drift));
					milestone("stable_live_edge", m);
				}
			}
		}

		// Receive-to-present for each fragment start, at tick resolution.
		for (auto &[subId, track] : mTracks)
		{
			while (!track->marks.empty() && track->marks.front().ptsNs <= position)
			{
				const PresentMark mark = track->marks.front();
				track->marks.pop_front();
				if (track->type != media::MediaSourceType::VIDEO && mTracks.count(kVideoSubId)) continue;
				const double latencyMs = nsToMs(now - mark.receiveMonoNs);
				mReceiveToPresentMs.add(latencyMs);
				Json fields;
				fields.add("track", track->name).add("group", mark.group).add("pts_ms", nsToMs(mark.ptsNs)).add("receive_to_present_ms", latencyMs);
				Telemetry::instance().trace("latency", "fragment_presented", fields);
			}
			if (track->lastPrft && track->parser->timescale())
			{
				const ProducerReferenceTime &prft = *track->lastPrft;
				const int64_t mediaNs = static_cast<int64_t>(prft.mediaTime / track->parser->timescale()) * kNsPerSec +
				                        static_cast<int64_t>(prft.mediaTime % track->parser->timescale()) * kNsPerSec / track->parser->timescale();
				const double producedWallMs = static_cast<double>(prft.wallMs()) + nsToMs(position - mediaNs);
				if (track->type == media::MediaSourceType::VIDEO || !mTracks.count(kVideoSubId))
				{
					mPrftLatencyMs.add(static_cast<double>(wallMs()) - producedWallMs);
				}
			}
		}

		// A/V offset at the sink inputs (not a presentation measurement; see docs/telemetry.md).
		Track *v = trackBySub(kVideoSubId);
		Track *a = trackBySub(kAudioSubId);
		int64_t vts = 0, ats = 0;
		if (v && a && v->attached && a->attached && mPipeline->getRenderedTimestamp(v->sourceId, vts) &&
		    mPipeline->getRenderedTimestamp(a->sourceId, ats))
		{
			mAvOffsetMs.add(nsToMs(vts - ats));
		}

		// Stable playback: a full window of presentation progress since the first frame or the last stall.
		const int64_t progressSince = std::max(mFirstFrameMonoNs, mLastStallEndMonoNs);
		if (!mStable && !mStalled && now - progressSince >= mConfig.stableWindowMs * kNsPerMs)
		{
			mStable = true;
			milestone("stable_playback");
		}
	}

	if (now - mLastStatusMonoNs >= mConfig.statusIntervalMs * kNsPerMs)
	{
		mLastStatusMonoNs = now;
		emitStatus();
	}
	if (mConfig.runDurationSec > 0 && now - mTuneMonoNs >= mConfig.runDurationSec * kNsPerSec)
	{
		finish(mState == State::Failed ? State::Failed : State::Stopped, "run duration reached");
		return;
	}
	mLoop.postAfterMs(kTickMs, [this] { tick(); });
}

void Player::emitStatus()
{
	int64_t position = 0;
	const bool havePosition = mPipeline && mFirstFrameMonoNs && mPipeline->getPosition(position);
	const ProcessSample process = sampleProcess();
	Json fields;
	fields.add("state", toString(mState)).add("stalled", mStalled).add("stalls", mStalls);
	if (havePosition) fields.add("position_ms", nsToMs(position));
	double edgeMs = -1;
	std::string line;
	for (auto &[subId, track] : mTracks)
	{
		const char *prefix = track->type == media::MediaSourceType::VIDEO ? "video" : "audio";
		uint64_t backendBytes = 0, rendered = 0, droppedFrames = 0;
		if (track->attached)
		{
			mPipeline->getQueuedBytes(track->sourceId, backendBytes);
			mPipeline->getStats(track->sourceId, rendered, droppedFrames);
		}
		uint64_t queueBytes = 0;
		{
			std::lock_guard<std::mutex> lock(mRouteMutex);
			auto it = mRoutes.find(subId);
			if (it != mRoutes.end()) queueBytes = it->second.queue->bytes();
		}
		const int64_t pendingNs = track->pending.empty() ? 0 : track->pending.back().timeStampNs + track->pending.back().durationNs - track->pending.front().timeStampNs;
		const int64_t aheadNs = havePosition && track->submittedEndPtsNs != INT64_MIN ? track->submittedEndPtsNs - position : 0;
		Json t;
		t.add("objects", track->objects).add("groups", track->groups).add("group_gaps", track->groupGaps)
			.add("pending_ms", nsToMs(pendingNs)).add("pending_aus", static_cast<uint64_t>(track->pending.size()))
			.add("submitted_ahead_ms", nsToMs(aheadNs)).add("queue_bytes", queueBytes).add("backend_bytes", backendBytes)
			.add("rendered", rendered).add("dropped", droppedFrames).add("submitted_aus", track->submittedAus);
		if (havePosition && track->newestEndPtsNs != INT64_MIN) t.add("newest_end_ms", nsToMs(track->newestEndPtsNs));
		fields.raw(prefix, t.object());
		if (havePosition && track->newestEndPtsNs != INT64_MIN && (track->type == media::MediaSourceType::VIDEO || !mTracks.count(kVideoSubId)))
		{
			edgeMs = nsToMs(track->newestEndPtsNs - position);
		}
		char text[160];
		snprintf(text, sizeof(text), " %s[obj=%" PRIu64 " pend=%.0fms ahead=%.0fms rend=%" PRIu64 " drop=%" PRIu64 "]", prefix, track->objects,
		         nsToMs(pendingNs), nsToMs(aheadNs), rendered, droppedFrames);
		line += text;
	}
	if (edgeMs >= 0) fields.add("live_edge_ms", edgeMs);
	fields.add("behind_live_edge", mBehindLiveEdge).add("rss_bytes", process.rssBytes).add("cpu_user_us", process.cpuUserUs)
		.add("cpu_system_us", process.cpuSystemUs).add("telemetry_dropped", Telemetry::instance().dropped());
	Telemetry::instance().emit("status", "periodic", fields);
	LOG_INFO("status", "%s pos=%.3fs edge=%.0fms stalls=%" PRIu64 " rss=%.1fMB%s", toString(mState), havePosition ? nsToMs(position) / 1000 : 0.0, edgeMs,
	         mStalls, static_cast<double>(process.rssBytes) / 1048576.0, line.c_str());
}

void Player::emitSummary()
{
	const ProcessSample process = sampleProcess();
	Json milestones;
	for (auto &[name, mono] : mMilestones) milestones.add(name.c_str(), nsToMs(mono - mTuneMonoNs));
	auto since = [&](const char *name) -> double {
		auto it = mMilestones.find(name);
		return it == mMilestones.end() ? -1.0 : nsToMs(it->second - mTuneMonoNs);
	};
	auto between = [&](const char *from, const char *to) -> double {
		auto a = mMilestones.find(from);
		auto b = mMilestones.find(to);
		return a == mMilestones.end() || b == mMilestones.end() ? -1.0 : nsToMs(b->second - a->second);
	};
	if (mStalled) mStallTotalNs += monoNs() - mStallStartMonoNs;
	if (mBehindLiveEdge) mBehindTotalNs += monoNs() - mBehindStartMonoNs;

	Json fields;
	fields.add("end_state", toString(mState)).add("end_reason", mEndReason).add("run_ms", nsToMs(monoNs() - mTuneMonoNs))
		.raw("milestones_ms", milestones.object())
		.add("tune_to_first_frame_ms", since("first_frame"))
		.add("tune_to_stable_playback_ms", since("stable_playback"))
		.add("tune_to_stable_live_edge_ms", since("stable_live_edge"))
		.add("post_tune_catch_up_ms", between("first_frame", "stable_live_edge"))
		.add("stalls", mStalls).add("stall_total_ms", nsToMs(mStallTotalNs))
		.add("behind_live_edge_total_ms", nsToMs(mBehindTotalNs)).add("catch_up_actions", 0)
		.raw("live_edge_ms", mLiveEdgeMs.json()).raw("steady_live_edge_ms", mSteadyLiveEdgeMs.json())
		.add("max_live_edge_ms", mMaxLiveEdgeMs)
		.add("time_weighted_live_edge_ms", mLiveEdgeWeightedTime > 0 ? mLiveEdgeWeightedSum / mLiveEdgeWeightedTime : -1.0)
		.raw("receive_to_present_ms", mReceiveToPresentMs.json())
		.raw("av_offset_sink_input_ms", mAvOffsetMs.json())
		.raw("prft_latency_ms", mPrftLatencyMs.json())
		.add("catalog_objects", mCatalogObjects)
		.add("rss_bytes", process.rssBytes).add("cpu_user_us", process.cpuUserUs).add("cpu_system_us", process.cpuSystemUs)
		.add("telemetry_dropped", Telemetry::instance().dropped());
	for (auto &[subId, track] : mTracks)
	{
		uint64_t highWater = 0;
		{
			std::lock_guard<std::mutex> lock(mRouteMutex);
			auto it = mRoutes.find(subId);
			if (it != mRoutes.end()) highWater = it->second.queue->highWaterBytes();
		}
		Json t;
		t.add("track", track->name).add("objects", track->objects).add("bytes", track->bytes).add("groups", track->groups)
			.add("group_gaps", track->groupGaps).add("stale_objects", track->staleObjects).add("parse_errors", track->parseErrors)
			.add("queue_dropped_objects", track->droppedQueueObjects).add("queue_dropped_bytes", track->droppedQueueBytes)
			.add("pending_dropped_aus", track->droppedPendingAus).add("join_discarded_aus", track->joinDiscardedAus)
			.add("submitted_aus", track->submittedAus).add("queue_high_water_bytes", highWater)
			.add("resubscribes", static_cast<uint64_t>(track->resubscribes)).add("qos_dropped", track->qosDropped)
			.add("prft_seen", track->lastPrft.has_value());
		fields.raw(track->type == media::MediaSourceType::VIDEO ? "video" : "audio", t.object());
	}
	Telemetry::instance().emit("summary", "run", fields);
	LOG_INFO("summary", "first_frame=%.0fms stable=%.0fms stable_live_edge=%.0fms stalls=%" PRIu64 " live_edge p50=%.0fms",
	         since("first_frame"), since("stable_playback"), since("stable_live_edge"), mStalls, mLiveEdgeMs.percentile(50));
}

} // namespace nmqe
