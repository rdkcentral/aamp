#pragma once
#include <chrono>
#include <map>
#include <string>
#include "PlayerLogManager.h"  // for MW_LOG_INFO / MW_LOG_ERR macros

class PerfTimer
{
public:
    static void perf_Start(const std::string& name);
    static void perf_Stop(const std::string& name);

private:
    static std::map<std::string, std::chrono::high_resolution_clock::time_point> s_startTimes;
};
