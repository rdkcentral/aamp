
#include "PerfProfiler.h"
#include "PlayerLogManager.h"

std::unordered_map<std::string, PerfStats> PerfProfiler::stats;
std::mutex PerfProfiler::statsMutex;

void PerfProfiler::AddStat(const std::string& name, long long duration) {
    std::lock_guard<std::mutex> lock(statsMutex);
    auto& s = stats[name];
    s.totalTime += duration;
    s.callCount++;
}

void PerfProfiler::PrintStats() {
    std::lock_guard<std::mutex> lock(statsMutex);
    MW_LOG_WARN("\n=== Performance Stats ===\n");
    for (const auto& [func, s] : stats) {
        long long avg = s.callCount ? (s.totalTime / s.callCount) : 0;
        MW_LOG_WARN("[PERF] %s called %zu times, avg %lld µs, total %lld µs\n",func.c_str(),s.callCount,avg,s.totalTime);

    }
}

ScopedTimer::ScopedTimer(const std::string& funcName)
    : name(funcName), start(std::chrono::high_resolution_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    PerfProfiler::AddStat(name, duration);
}

