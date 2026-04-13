/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file aamp_stubs.cpp
 * @brief Minimal AAMP infrastructure stubs for standalone ABR testing
 * 
 * Provides stub implementations of AAMP logging and config functions
 * required by ABRManager, allowing abrsim to build without full AAMP.
 */

#include <cstdio>
#include <cstdarg>

// Stub AAMP log level enum
enum AAMP_LogLevel {
	eLOGLEVEL_TRACE = 0,
	eLOGLEVEL_DEBUG,
	eLOGLEVEL_INFO,
	eLOGLEVEL_WARN,
	eLOGLEVEL_ERROR,
	eLOGLEVEL_FATAL
};

// Stub AampLogManager
class AampLogManager {
public:
	static AAMP_LogLevel aampLoglevel;
};

// Initialize static member
AAMP_LogLevel AampLogManager::aampLoglevel = eLOGLEVEL_WARN;

// Stub logging function - must match AAMP's declaration exactly
void logprintf(AAMP_LogLevel level, const char* file, const char* func, int line, const char *format, ...) {
	// Simple implementation for simulation - could be enhanced if needed
	if (level < AampLogManager::aampLoglevel) {
		return;
	}
	
	// Uncomment for verbose ABR logging during development:
	// const char* levelStr = "UNKNOWN";
	// switch (level) {
	// 	case eLOGLEVEL_TRACE: levelStr = "TRACE"; break;
	// 	case eLOGLEVEL_DEBUG: levelStr = "DEBUG"; break;
	// 	case eLOGLEVEL_INFO:  levelStr = "INFO"; break;
	// 	case eLOGLEVEL_WARN:  levelStr = "WARN"; break;
	// 	case eLOGLEVEL_ERROR: levelStr = "ERROR"; break;
	// 	case eLOGLEVEL_FATAL: levelStr = "FATAL"; break;
	// }
	// 
	// fprintf(stderr, "[ABR:%s] ", levelStr);
	// 
	// va_list args;
	// va_start(args, format);
	// vfprintf(stderr, format, args);
	// va_end(args);
	// 
	// fprintf(stderr, "\n");
	
	// For now, suppress ABR logging in simulation
	(void)file;
	(void)line;
	(void)format;
}
