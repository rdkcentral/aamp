
#ifndef PERF_PROFILER_H
#define PERF_PROFILER_H

#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>

struct PerfStats {
    long long totalTime = 0;
    size_t callCount = 0;
};

class PerfProfiler {
public:
    static void AddStat(const std::string& name, long long duration);
    static void PrintStats();

private:
    static std::unordered_map<std::string, PerfStats> stats;
    static std::mutex statsMutex;
};

class ScopedTimer {
public:
    ScopedTimer(const std::string& funcName);
    ~ScopedTimer();

private:
    std::string name;
    std::chrono::high_resolution_clock::time_point start;
};

#define MW_PROFILE_FUNCTION() ScopedTimer timer(__FUNCTION__)

#endif // PERF_PROFILER_H

