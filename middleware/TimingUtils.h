#pragma once
#include <chrono>
#include <map>
#include <string>
#include "PlayerLogManager.h"  // for MW_LOG_INFO / MW_LOG_ERR macros

class PerfTimer
{
public:
    static void perf_Start(const std::string& name)
    {
        s_startTimes[name] = std::chrono::high_resolution_clock::now();
    }

    static void perf_Stop(const std::string& name)
    {
        auto it = s_startTimes.find(name);
        if (it == s_startTimes.end())
        {
            MW_LOG_ERR("[PERF] perf_Stop called without matching perf_Start for '%s'", name.c_str());
            return;
        }

        auto end = std::chrono::high_resolution_clock::now();
        long long duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - it->second).count();

        MW_LOG_ERR("[PERF] %s took %lld ms", name.c_str(), duration);

        s_startTimes.erase(it);  // optional: remove after logging
    }

private:
    static std::map<std::string, std::chrono::high_resolution_clock::time_point> s_startTimes;
};
