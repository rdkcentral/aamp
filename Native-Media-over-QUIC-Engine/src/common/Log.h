#pragma once

#include <cstdarg>
#include <string>

namespace nmqe::log {

enum class Level { Trace = 0, Debug, Info, Warn, Error, Off };

void setLevel(Level level);
Level level();
bool parseLevel(const std::string &text, Level &out);
void write(Level level, const char *tag, const char *format, ...) __attribute__((format(printf, 3, 4)));
void writeV(Level level, const char *tag, const char *format, va_list args);

} // namespace nmqe::log

#define NMQE_LOG(LEVEL, TAG, ...) \
	do { if ((LEVEL) >= ::nmqe::log::level()) ::nmqe::log::write((LEVEL), (TAG), __VA_ARGS__); } while (0)
#define LOG_DEBUG(TAG, ...) NMQE_LOG(::nmqe::log::Level::Debug, TAG, __VA_ARGS__)
#define LOG_INFO(TAG, ...) NMQE_LOG(::nmqe::log::Level::Info, TAG, __VA_ARGS__)
#define LOG_WARN(TAG, ...) NMQE_LOG(::nmqe::log::Level::Warn, TAG, __VA_ARGS__)
#define LOG_ERROR(TAG, ...) NMQE_LOG(::nmqe::log::Level::Error, TAG, __VA_ARGS__)
