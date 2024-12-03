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


bool IsoBmffHelper::ConvertToKeyFrame(AampGrowableBuffer &buffer)
{
	AAMPLOG_TRACE("Function called with len = %zu", buffer.GetLen());

	bool retval{true};
	IsoBmffBuffer isoBmffBuffer{};

	isoBmffBuffer.setBuffer(reinterpret_cast<uint8_t*>(buffer.GetPtr()), buffer.GetLen() );

	if(isoBmffBuffer.parseBuffer())
	{
		isoBmffBuffer.truncate();
		buffer.SetLen(isoBmffBuffer.getSize());
	}
	else
	{
		retval = false;
	}

	return retval;
}

bool IsoBmffHelper::RestampPts(AampGrowableBuffer &buffer, int64_t ptsOffset, std::string const &fragmentUrl, const char* trackName, uint32_t timeScale)
{
	bool retval{false};
	IsoBmffBuffer isoBmffBuffer{};

	isoBmffBuffer.setBuffer(reinterpret_cast<uint8_t*>(buffer.GetPtr()), buffer.GetLen() );

	if (!isoBmffBuffer.parseBuffer())
	{
		AAMPLOG_WARN("Failed to parse buffer");
	}
	else
	{
		isoBmffBuffer.restampPts(ptsOffset);
		// NOTE: This log line is used by the pts_restamp_check.py test tool,
		// and may be used by other tests for validation purposes (e.g. L2 tests).
		// Please check restamping tests and tools before modifying this log line.
		AAMPLOG_INFO("[%s] timeScale %u before %" PRIu64 " after %" PRIu64 " duration %" PRIu64 " %s",
					 trackName, timeScale, isoBmffBuffer.beforePTS, isoBmffBuffer.afterPTS,
					 isoBmffBuffer.getSegmentDuration(), fragmentUrl.c_str());
		retval = true;
	}

	return retval;
}

bool IsoBmffHelper::SetTimescale(AampGrowableBuffer &buffer, uint32_t timeScale)
{
	bool retval{false};
	IsoBmffBuffer isoBmffBuffer{};

	isoBmffBuffer.setBuffer(reinterpret_cast<uint8_t *>(buffer.GetPtr()), buffer.GetLen());

	if (!isoBmffBuffer.parseBuffer())
	{
		AAMPLOG_WARN("Failed to parse buffer");
	}
	else
	{
		retval = isoBmffBuffer.setTrickmodeTimescale(timeScale);
	}

	return retval;
}

bool IsoBmffHelper::SetPtsAndDuration(AampGrowableBuffer &buffer, uint64_t pts, uint64_t duration)
{
	bool retval{false};
	IsoBmffBuffer isoBmffBuffer{};

	isoBmffBuffer.setBuffer(reinterpret_cast<uint8_t *>(buffer.GetPtr()), buffer.GetLen());

	if (!isoBmffBuffer.parseBuffer())
	{
		AAMPLOG_WARN("Failed to parse buffer");
	}
	else
	{
		isoBmffBuffer.setPtsAndDuration(pts, duration);
		retval = true;
	}

	return retval;
}
