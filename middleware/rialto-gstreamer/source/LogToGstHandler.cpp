/*
 * Copyright (C) 2024   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

//
// This allows, for example, passing the following environment variable
// to the client app which will enable rialto logging via gstreamer...
//   GST_DEBUG=6
//

#include <string>

#include "LogToGstHandler.h"

#ifdef USE_ETHANLOG

#include <ethanlog.h>

#define SYSTEM_LOG_FATAL(filename, function, line, ...) ethanlog(ETHAN_LOG_FATAL, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_ERROR(filename, function, line, ...) ethanlog(ETHAN_LOG_ERROR, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_WARN(filename, function, line, ...)                                                                 \
    ethanlog(ETHAN_LOG_WARNING, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_MIL(filename, function, line, ...)                                                                  \
    ethanlog(ETHAN_LOG_MILESTONE, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_INFO(filename, function, line, ...) ethanlog(ETHAN_LOG_INFO, filename, function, line, __VA_ARGS__)
#define SYSTEM_LOG_DEBUG(filename, function, line, ...) ethanlog(ETHAN_LOG_DEBUG, filename, function, line, __VA_ARGS__)

#else

#include <gst/gst.h>

#define SYSTEM_LOG_FATAL(filename, function, line, ...) GST_CAT_ERROR(kGstRialtoCategory, __VA_ARGS__)
#define SYSTEM_LOG_ERROR(filename, function, line, ...) GST_CAT_ERROR(kGstRialtoCategory, __VA_ARGS__)
#define SYSTEM_LOG_WARN(filename, function, line, ...) GST_CAT_WARNING(kGstRialtoCategory, __VA_ARGS__)
#define SYSTEM_LOG_MIL(filename, function, line, ...) GST_CAT_INFO(kGstRialtoCategory, __VA_ARGS__)
#define SYSTEM_LOG_INFO(filename, function, line, ...) GST_CAT_INFO(kGstRialtoCategory, __VA_ARGS__)
#define SYSTEM_LOG_DEBUG(filename, function, line, ...) GST_CAT_DEBUG(kGstRialtoCategory, __VA_ARGS__)

namespace
{
GST_DEBUG_CATEGORY_STATIC(kGstRialtoCategory);
const char *kCategory = "rialto";
}; // namespace

#endif

using namespace firebolt::rialto;

LogToGstHandler::LogToGstHandler()
{
#ifndef USE_ETHANLOG
    GST_DEBUG_CATEGORY_INIT(kGstRialtoCategory, kCategory, 0, "Messages from rialto client library");
#endif
}

LogToGstHandler::~LogToGstHandler() {}

void LogToGstHandler::log(Level level, const std::string &file, int line, const std::string &function,
                          const std::string &message)
{
    std::string toReport;
    toReport += "M:" + file;
    toReport += " F:" + function;
    toReport += " L:" + std::to_string(line);
    toReport += " > " + message;

    switch (level)
    {
    case Level::Fatal:
        SYSTEM_LOG_FATAL(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;

    case Level::Error:
        SYSTEM_LOG_ERROR(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;

    case Level::Warning:
        SYSTEM_LOG_WARN(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;

    case Level::Milestone:
        SYSTEM_LOG_MIL(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;

    case Level::Info:
        SYSTEM_LOG_INFO(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;

    case Level::Debug:
        SYSTEM_LOG_DEBUG(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;

    case Level::External:
    default:
        SYSTEM_LOG_INFO(file.c_str(), function.c_str(), line, "%s", toReport.c_str());
        break;
    }
}
