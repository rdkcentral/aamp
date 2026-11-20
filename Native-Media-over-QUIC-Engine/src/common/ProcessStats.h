#pragma once

#include <cstdint>

namespace nmqe {

struct ProcessSample
{
	uint64_t rssBytes = 0;
	int64_t cpuUserUs = 0;
	int64_t cpuSystemUs = 0;
};

/** Current resident set size and cumulative CPU time of this process. */
ProcessSample sampleProcess();

} // namespace nmqe
