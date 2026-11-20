#pragma once

#include "common/ReceivedObject.h"

#include <cstdint>
#include <memory>
#include <string>

namespace nmqe {

/** Session- and subscription-level events from an Object source. */
struct SourceEvent
{
	enum class Kind
	{
		Connecting,
		Connected,          // transport up and MOQT setup complete; detail = version
		BroadcastReady,     // namespace resolved; detail = namespace
		Subscribed,         // subId; detail = track
		GroupBegin,         // subId, group
		GroupEnd,           // subId, group, objectCount; detail = error or empty
		SubscriptionEnded,  // subId; detail = error or empty
		Error,              // detail = error
		Closed,             // detail = error or empty
	};
	Kind kind = Kind::Error;
	uint64_t subId = 0;
	uint64_t group = 0;
	uint64_t objectCount = 0;
	std::string detail;
	int64_t monoNs = 0;
	int64_t wallMs = 0;
};

const char *toString(SourceEvent::Kind kind);

/** Receives events and Objects. Called on source threads: implementations must not block. */
class ObjectSourceListener
{
public:
	virtual ~ObjectSourceListener() = default;
	virtual void onSourceEvent(const SourceEvent &event) = 0;
	virtual void onObject(uint64_t subId, ReceivedObject object) = 0;
};

struct TrackSubscription
{
	std::string track;
	uint64_t subId = 0;
	int8_t priority = 0;
	uint32_t maxDelayMs = 0;
	bool liveJoin = true;
};

/**
 * Where Objects come from: the live MOQT adapter or a capture replay. Both
 * emit the same ReceivedObject model.
 */
class ObjectSource
{
public:
	virtual ~ObjectSource() = default;
	virtual bool start(ObjectSourceListener *listener) = 0;
	virtual bool subscribe(const TrackSubscription &subscription) = 0;
	virtual bool unsubscribe(uint64_t subId) = 0;
	/** Idempotent; no listener call happens after it returns. */
	virtual void stop() = 0;
	/** Implementation identity for the run record. */
	virtual std::string describe() const = 0;
};

struct LiveSourceSettings
{
	std::string url;
	std::string version;
	std::string ns;
	std::string libraryLog;
};

/** The live MOQT adapter (implemented in src/transport over the selected library). */
std::unique_ptr<ObjectSource> createLiveObjectSource(const LiveSourceSettings &settings);

} // namespace nmqe
