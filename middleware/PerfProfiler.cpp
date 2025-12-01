
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
    // Take a snapshot under lock
    std::unordered_map<std::string, PerfStats> snapshot;
    {
        std::lock_guard<std::mutex> lock(statsMutex);
        snapshot = stats; // Copy current stats
    }

    // Print outside lock to avoid blocking updates
    printf("\n=== Performance Stats ===\n");
    for (const auto& [func, s] : snapshot) {
        long long avg = s.callCount ? (s.totalTime / s.callCount) : 0;
        
	printf("[PERF] %s called %llu times, avg %lld µs, total %lld µs\n",
       	func.c_str(),static_cast<unsigned long long>(s.callCount), static_cast<long long>(avg), static_cast<long long>(s.totalTime));

    }
    fflush(stdout);
}

ScopedTimer::ScopedTimer(const std::string& funcName)
    : name(funcName), start(std::chrono::high_resolution_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    PerfProfiler::AddStat(name, duration);
}

