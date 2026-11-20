#include "ProcessStats.h"

#include <sys/resource.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace nmqe {

ProcessSample sampleProcess()
{
	ProcessSample sample;
	rusage usage{};
	if (getrusage(RUSAGE_SELF, &usage) == 0)
	{
		sample.cpuUserUs = static_cast<int64_t>(usage.ru_utime.tv_sec) * 1000000 + usage.ru_utime.tv_usec;
		sample.cpuSystemUs = static_cast<int64_t>(usage.ru_stime.tv_sec) * 1000000 + usage.ru_stime.tv_usec;
	}
#if defined(__APPLE__)
	mach_task_basic_info info{};
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
	{
		sample.rssBytes = info.resident_size;
	}
#else
	if (FILE *statm = fopen("/proc/self/statm", "r"))
	{
		unsigned long pages = 0, resident = 0;
		if (fscanf(statm, "%lu %lu", &pages, &resident) == 2)
		{
			sample.rssBytes = static_cast<uint64_t>(resident) * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
		}
		fclose(statm);
	}
#endif
	return sample;
}

} // namespace nmqe
