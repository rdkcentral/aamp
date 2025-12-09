#include "TimingUtils.h"

// Define static member
std::map<std::string, std::chrono::high_resolution_clock::time_point> PerfTimer::s_startTimes;
