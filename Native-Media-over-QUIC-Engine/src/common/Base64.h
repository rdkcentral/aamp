#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nmqe {

/** Decodes standard or URL-safe base64, ignoring whitespace; nullopt on malformed input. */
std::optional<std::vector<uint8_t>> base64Decode(const std::string &text);

} // namespace nmqe
