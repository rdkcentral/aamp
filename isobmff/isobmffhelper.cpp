/*
 * If not stated otherwise in this file or this component's LICENSE file the
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

/* AAMP config header file is needed for the log level configuration.
 * It also includes AAMP log manager header file. */
#include "AampConfig.h"
#include "isobmffbuffer.h"
#include "isobmffhelper.h"

#include <cinttypes>


bool IsoBmffHelper::InitAndParse(IsoBmffBuffer& isoBmffBuffer, std::vector<uint8_t> &buffer)
{
	isoBmffBuffer.setBuffer(buffer.data(), buffer.size());
	if (!isoBmffBuffer.parseBuffer())
	{
		AAMPLOG_WARN("Failed to parse buffer");
		return false;
	}
	return true;
}

bool IsoBmffHelper::ConvertToKeyFrame(std::vector<uint8_t> &buffer)
{
	AAMPLOG_TRACE("Function called with len = %zu", buffer.size());

	IsoBmffBuffer isoBmffBuffer{};
	if (!InitAndParse(isoBmffBuffer, buffer))
	{
		return false;
	}

	isoBmffBuffer.truncate();
	buffer.resize(isoBmffBuffer.getSize());
	buffer.shrink_to_fit(); // GCC (libstdc++), Clang (libc++), and MSVC (STL) all reallocate to fit.
	return true;
}

bool IsoBmffHelper::RestampPts(std::vector<uint8_t> &buffer, int64_t ptsOffset, std::string const &fragmentUrl, const char* trackName, uint32_t timeScale)
{
	IsoBmffBuffer isoBmffBuffer{};
	if (!InitAndParse(isoBmffBuffer, buffer))
	{
		return false;
	}

	isoBmffBuffer.restampPts(ptsOffset);
	// NOTE: This log line is used by the pts_restamp_check.py test tool,
	// and may be used by other tests for validation purposes (e.g. L2 tests).
	// Please check restamping tests and tools before modifying this log line.
	AAMPLOG_INFO("[%s] timeScale %u before %" PRIu64 " after %" PRIu64 " duration %" PRIu64 " %s",
				 trackName, timeScale, isoBmffBuffer.beforePTS, isoBmffBuffer.afterPTS,
				 isoBmffBuffer.getSegmentDuration(), fragmentUrl.c_str());
	return true;
}

bool IsoBmffHelper::SetTimescale(std::vector<uint8_t> &buffer, uint32_t timeScale)
{
	IsoBmffBuffer isoBmffBuffer{};
	if (!InitAndParse(isoBmffBuffer, buffer))
	{
		return false;
	}

	return isoBmffBuffer.setTrickmodeTimescale(timeScale);
}

bool IsoBmffHelper::SetPtsAndDuration(std::vector<uint8_t> &buffer, uint64_t pts, uint32_t duration)
{
	IsoBmffBuffer isoBmffBuffer{};
	if (!InitAndParse(isoBmffBuffer, buffer))
	{
		return false;
	}

	isoBmffBuffer.setPtsAndDuration(pts, duration);
	return true;
}

bool IsoBmffHelper::ClearMediaHeaderDuration(std::vector<uint8_t> &buffer)
{
	IsoBmffBuffer isoBmffBuffer{};
	if (!InitAndParse(isoBmffBuffer, buffer))
	{
		return false;
	}

	if (!isoBmffBuffer.isInitSegment())
	{
		AAMPLOG_DEBUG("Buffer is not an initialization segment");
		return false;
	}

	return isoBmffBuffer.setMediaHeaderDuration(0);
}
