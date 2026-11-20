// The only translation unit that includes the selected MOQT library's API.

#include "ObjectSource.h"

#include "common/Clock.h"
#include "common/Log.h"

#include "moq_shim.h"

#include <mutex>

namespace nmqe {

namespace {

class MoqObjectSource final : public IObjectSource
{
public:
	explicit MoqObjectSource(LiveSourceSettings settings) : mSettings(std::move(settings)) {}
	~MoqObjectSource() override { stop(); }

	bool start(IObjectSourceListener *listener) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (mSession) return true;
		mListener = listener;
		moqshim_config config{};
		config.url = mSettings.url.c_str();
		config.version = mSettings.version.empty() ? nullptr : mSettings.version.c_str();
		config.ns = mSettings.ns.c_str();
		config.log_filter = mSettings.libraryLog.empty() ? nullptr : mSettings.libraryLog.c_str();
		config.on_event = &MoqObjectSource::onEvent;
		config.on_object = &MoqObjectSource::onObject;
		config.ctx = this;
		mSession = moqshim_session_open(&config);
		if (!mSession) LOG_ERROR("transport", "invalid live source settings (url=%s version=%s)", mSettings.url.c_str(), mSettings.version.c_str());
		return mSession != nullptr;
	}

	bool subscribe(const TrackSubscription &subscription) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		if (!mSession) return false;
		moqshim_subscribe request{};
		request.track = subscription.track.c_str();
		request.sub_id = subscription.subId;
		request.priority = subscription.priority;
		request.max_delay_ms = subscription.maxDelayMs;
		request.live_join = subscription.liveJoin ? 1 : 0;
		return moqshim_subscribe_track(mSession, &request) == 0;
	}

	bool unsubscribe(uint64_t subId) override
	{
		std::lock_guard<std::mutex> lock(mMutex);
		return mSession && moqshim_unsubscribe_track(mSession, subId) == 0;
	}

	void stop() override
	{
		moqshim_session *session = nullptr;
		{
			std::lock_guard<std::mutex> lock(mMutex);
			session = mSession;
			mSession = nullptr;
		}
		// Joins the shim's runtime: no callback runs after this returns.
		if (session) moqshim_session_close(session);
	}

	std::string describe() const override
	{
		return std::string(moqshim_implementation()) + " " + mSettings.version + " " + mSettings.url;
	}

private:
	static void onEvent(void *ctx, const moqshim_event *raw)
	{
		auto *self = static_cast<MoqObjectSource *>(ctx);
		SourceEvent event;
		switch (raw->kind)
		{
			case MOQSHIM_EVENT_CONNECTING: event.kind = SourceEvent::Kind::Connecting; break;
			case MOQSHIM_EVENT_CONNECTED: event.kind = SourceEvent::Kind::Connected; break;
			case MOQSHIM_EVENT_BROADCAST_READY: event.kind = SourceEvent::Kind::BroadcastReady; break;
			case MOQSHIM_EVENT_SUBSCRIBED: event.kind = SourceEvent::Kind::Subscribed; break;
			case MOQSHIM_EVENT_GROUP_BEGIN: event.kind = SourceEvent::Kind::GroupBegin; break;
			case MOQSHIM_EVENT_GROUP_END: event.kind = SourceEvent::Kind::GroupEnd; break;
			case MOQSHIM_EVENT_SUBSCRIPTION_ENDED: event.kind = SourceEvent::Kind::SubscriptionEnded; break;
			case MOQSHIM_EVENT_CLOSED: event.kind = SourceEvent::Kind::Closed; break;
			default: event.kind = SourceEvent::Kind::Error; break;
		}
		event.subId = raw->sub_id;
		event.group = raw->group;
		event.objectCount = raw->object;
		if (raw->detail) event.detail = raw->detail;
		event.monoNs = monoNs();
		event.wallMs = wallMs();
		if (self->mListener) self->mListener->onSourceEvent(event);
	}

	static void onObject(void *ctx, const moqshim_object *raw)
	{
		auto *self = static_cast<MoqObjectSource *>(ctx);
		ReceivedObject object;
		object.receiveMonoNs = monoNs();
		object.receiveWallMs = wallMs();
		object.id.group = raw->group;
		object.id.object = raw->object;
		object.payload = std::make_shared<const ByteBuffer>(raw->data, raw->data + raw->size);
		if (self->mListener) self->mListener->onObject(raw->sub_id, std::move(object));
	}

	LiveSourceSettings mSettings;
	std::mutex mMutex;
	moqshim_session *mSession = nullptr;
	IObjectSourceListener *mListener = nullptr;
};

} // namespace

std::unique_ptr<IObjectSource> createLiveObjectSource(const LiveSourceSettings &settings)
{
	return std::make_unique<MoqObjectSource>(settings);
}

} // namespace nmqe
