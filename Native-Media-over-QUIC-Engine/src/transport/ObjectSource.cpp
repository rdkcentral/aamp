#include "ObjectSource.h"

namespace nmqe {

const char *toString(SourceEvent::Kind kind)
{
	switch (kind)
	{
		case SourceEvent::Kind::Connecting: return "connecting";
		case SourceEvent::Kind::Connected: return "connected";
		case SourceEvent::Kind::BroadcastReady: return "broadcast_ready";
		case SourceEvent::Kind::Subscribed: return "subscribed";
		case SourceEvent::Kind::GroupBegin: return "group_begin";
		case SourceEvent::Kind::GroupEnd: return "group_end";
		case SourceEvent::Kind::SubscriptionEnded: return "subscription_ended";
		case SourceEvent::Kind::Error: return "error";
		case SourceEvent::Kind::Closed: return "closed";
	}
	return "unknown";
}

} // namespace nmqe
