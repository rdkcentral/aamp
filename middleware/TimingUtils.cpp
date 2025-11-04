#include "TimingUtils.h"

// Define static member
std::map<std::string, std::chrono::high_resolution_clock::time_point> PerfTimer::s_startTimes;

void PerfTimer::perf_Start(const std::string& name)
{
    s_startTimes[name] = std::chrono::high_resolution_clock::now();
}

void PerfTimer::perf_Stop(const std::string& name)
{
    auto it = s_startTimes.find(name);
    if (it == s_startTimes.end())
    {
        MW_LOG_INFO("[PERF] perf_Stop called without matching perf_Start for '%s'", name.c_str());
        return;
    }

    auto end = std::chrono::high_resolution_clock::now();
    long long duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - it->second).count();

    MW_LOG_INFO("[PERF] %s took %lld ns", name.c_str(), duration);

    s_startTimes.erase(it);  // optional: remove after logging
}
