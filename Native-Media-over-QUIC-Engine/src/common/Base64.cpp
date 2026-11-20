#include "Base64.h"

namespace nmqe {

std::optional<std::vector<uint8_t>> base64Decode(const std::string &text)
{
	auto value = [](char c) -> int {
		if (c >= 'A' && c <= 'Z') return c - 'A';
		if (c >= 'a' && c <= 'z') return c - 'a' + 26;
		if (c >= '0' && c <= '9') return c - '0' + 52;
		if (c == '+' || c == '-') return 62;
		if (c == '/' || c == '_') return 63;
		return -1;
	};
	std::vector<uint8_t> out;
	out.reserve(text.size() * 3 / 4);
	uint32_t accumulator = 0;
	int bits = 0;
	bool padding = false;
	for (char c : text)
	{
		if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
		if (c == '=')
		{
			padding = true;
			continue;
		}
		const int v = value(c);
		if (v < 0 || padding) return std::nullopt;
		accumulator = (accumulator << 6) | static_cast<uint32_t>(v);
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			out.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xff));
		}
	}
	if (bits >= 6) return std::nullopt;
	return out;
}

} // namespace nmqe
