#ifndef PERF_PROFILER_H
#define PERF_PROFILER_H

#include <string>
#include <chrono>

#ifdef ENABLE_MW_PROFILING

class ScopedTimer {
public:
    ScopedTimer(const std::string& funcName, const std::string& fileName, int line);
    ~ScopedTimer();

private:
    std::string name;
    std::chrono::high_resolution_clock::time_point start;
};

// Macro for easy integration
#define MW_PROFILE_FUNCTION() ScopedTimer timer(__FUNCTION__, __FILE__, __LINE__)

#else

#define MW_PROFILE_FUNCTION() //profiling disabled

#endif //ENABLE_MW_PROFILING
#endif // PERF_PROFILER_H
