/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/**
 * @file FakeLstringDeps.cpp
 * @brief Minimal linker-level stubs for the symbols that lstring.cpp
 *        references from AampLogManager.h / aamplogging.cpp.
 *
 * lstring itself has no GStreamer, priv_aamp, or middleware dependency.
 * The full `fakes` library is not needed here; including it would drag in
 * FakeAampLogManager.cpp → priv_aamp.h → AampDemuxDataTypes.h →
 * middleware/DemuxDataTypes.h → middleware/GstUtils.h → gst/gst.h,
 * breaking the build on Mac developer machines that do not have GStreamer
 * headers installed.
 *
 * The only AampLogManager symbols lstring.cpp ever touches are:
 *   - AampLogManager::aampLoglevel  (log-level gate in AAMPLOG macro)
 *   - logprintf()                   (called when the gate passes)
 *   - gPlayerId                     (thread-local, used inside logprintf)
 *   - GetMediaTypeName()            (extern declaration in AampLogManager.h;
 *                                    not called by lstring, but must be
 *                                    resolvable at link time)
 *
 * All other AampLogManager static members are defined here to satisfy ODR
 * requirements when the translation unit that includes AampLogManager.h is
 * compiled as part of this target.
 */

#include <cstdarg>
#include <cstdio>

#include "AampLogManager.h"

// ---------------------------------------------------------------------------
// AampLogManager static member definitions
// ---------------------------------------------------------------------------

std::atomic<AAMP_LogLevel> AampLogManager::aampLoglevel(eLOGLEVEL_WARN);
std::atomic<bool> AampLogManager::locked(false);
std::atomic<bool> AampLogManager::logFilename(false);
std::atomic<bool> AampLogManager::disableLogRedirection(false);
std::atomic<bool> AampLogManager::enableEthanLogRedirection(false);

// ---------------------------------------------------------------------------
// Thread-local player-id used by the logging framework
// ---------------------------------------------------------------------------

thread_local int gPlayerId = -1;

// ---------------------------------------------------------------------------
// logprintf — minimal stderr sink; sufficient for unit-test diagnostics
// ---------------------------------------------------------------------------

void logprintf(AAMP_LogLevel /*level*/, const char* /*file*/, const char* func, int line,
               const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[lstring-test][%s:%d] ", func, line);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
}

// ---------------------------------------------------------------------------
// GetMediaTypeName — not called by lstring; stub satisfies extern declaration
// ---------------------------------------------------------------------------

const char* GetMediaTypeName(AampMediaType /*mediaType*/)
{
    return "unknown";
}
