#include "Log.h"

#include "Clock.h"

#include <atomic>
#include <cstdio>
#include <mutex>

namespace nmqe::log {

namespace {
std::atomic<Level> gLevel{Level::Info};
std::mutex gMutex;
const char *kNames[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "OFF"};
} // namespace

void setLevel(Level value) { gLevel = value; }

Level level() { return gLevel; }

bool parseLevel(const std::string &text, Level &out)
{
	for (int i = 0; i <= static_cast<int>(Level::Off); ++i)
	{
		std::string name = kNames[i];
		for (auto &c : name) c = static_cast<char>(tolower(c));
		if (text == name)
		{
			out = static_cast<Level>(i);
			return true;
		}
	}
	return false;
}

void writeV(Level lvl, const char *tag, const char *format, va_list args)
{
	if (lvl < gLevel) return;
	char message[2048];
	vsnprintf(message, sizeof(message), format, args);
	const int64_t ms = wallMs();
	std::lock_guard<std::mutex> lock(gMutex);
	fprintf(stderr, "%lld.%03lld %-5s [%s] %s\n", static_cast<long long>(ms / 1000),
		static_cast<long long>(ms % 1000), kNames[static_cast<int>(lvl)], tag, message);
}

void write(Level lvl, const char *tag, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	writeV(lvl, tag, format, args);
	va_end(args);
}

} // namespace nmqe::log
