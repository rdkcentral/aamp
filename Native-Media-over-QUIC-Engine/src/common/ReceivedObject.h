#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace nmqe {

using ByteBuffer = std::vector<uint8_t>;

/** MOQT Object identity. Group and Object IDs are the publisher's, verbatim. */
struct ObjectId
{
	std::string track;
	uint64_t group = 0;
	uint64_t object = 0;
	std::optional<uint64_t> subgroup;
};

/**
 * The internal boundary between transport (live or replayed) and media.
 * Payloads are immutable and shared; receive times are stamped where the
 * Object crosses into the player.
 */
struct ReceivedObject
{
	ObjectId id;
	uint8_t priority = 0;
	std::shared_ptr<const ByteBuffer> payload;
	int64_t receiveMonoNs = 0;
	int64_t receiveWallMs = 0;
};

} // namespace nmqe
