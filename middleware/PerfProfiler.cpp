#include "PerfProfiler.h"
#include <iostream>
#include <thread>

ScopedTimer::ScopedTimer(const std::string& funcName, const std::string& fileName, int line)
    : name(funcName + " [" + fileName + ":" + std::to_string(line) + "]"),
      start(std::chrono::high_resolution_clock::now()) {}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    long long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "[PERF] " << name << " (Thread " << std::this_thread::get_id()
              << ") took " << duration << " µs\n";

    // Future scope: Add global stats for averages here
}

