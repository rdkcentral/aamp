#pragma once

/*
 * Stand-in for AAMP's AampLogManager.h when compiling AAMP's mp4demux sources
 * standalone: routes AAMPLOG_* to this project's logger without pulling in
 * libaamp, curl or player state.
 */

#include "common/Log.h"

#define AAMPLOG_TRACE(FORMAT, ...) NMQE_LOG(::nmqe::log::Level::Trace, "mp4demux", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_DEBUG(FORMAT, ...) NMQE_LOG(::nmqe::log::Level::Debug, "mp4demux", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_INFO(FORMAT, ...) NMQE_LOG(::nmqe::log::Level::Debug, "mp4demux", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_MIL(FORMAT, ...) NMQE_LOG(::nmqe::log::Level::Info, "mp4demux", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_WARN(FORMAT, ...) NMQE_LOG(::nmqe::log::Level::Warn, "mp4demux", FORMAT, ##__VA_ARGS__)
#define AAMPLOG_ERR(FORMAT, ...) NMQE_LOG(::nmqe::log::Level::Error, "mp4demux", FORMAT, ##__VA_ARGS__)
