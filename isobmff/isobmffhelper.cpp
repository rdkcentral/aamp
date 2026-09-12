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

/*static*/ size_t IsoBmffHelper::GetIframeByteCap(const uint8_t *buf, size_t len)
{
	// Need at least 8 bytes to read MOOF box size and four-character type.
	if (len < 8) return 0;

	// Read MOOF box size (big-endian uint32) and verify type == 'moof'.
	uint32_t moofSize = (uint32_t(buf[0]) << 24) | (uint32_t(buf[1]) << 16) |
	                    (uint32_t(buf[2]) <<  8) |  uint32_t(buf[3]);
	if (moofSize < 8 ||
	    buf[4] != 'm' || buf[5] != 'o' || buf[6] != 'o' || buf[7] != 'f')
	{
		return 0;
	}
	// Wait until the complete MOOF box has been received.
	if (len < moofSize) return 0;

	// TRUN flags — matching the constants defined in isobmffbox.cpp.
	static constexpr uint32_t kTrunDataOffsetPresent       = 0x0001;
	static constexpr uint32_t kTrunFirstSampleFlagsPresent = 0x0004;
	static constexpr uint32_t kTrunSampleDurationPresent   = 0x0100;
	static constexpr uint32_t kTrunSampleSizePresent       = 0x0200;
	// TFHD flags
	static constexpr uint32_t kTfhdBaseDataOffsetPresent        = 0x00001;
	static constexpr uint32_t kTfhdSampleDescIndexPresent       = 0x00002;
	static constexpr uint32_t kTfhdDefaultSampleDurationPresent = 0x00008;
	static constexpr uint32_t kTfhdDefaultSampleSizePresent     = 0x00010;

	// Helper: read a big-endian uint32 from an arbitrary unaligned pointer.
	auto readBE32 = [](const uint8_t *p) -> uint32_t {
		return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
		       (uint32_t(p[2]) <<  8) |  uint32_t(p[3]);
	};
	// Helper: read a big-endian uint24 (3 bytes) as a uint32.
	auto readBE24 = [](const uint8_t *p) -> uint32_t {
		return (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | uint32_t(p[2]);
	};

	uint32_t tfhdDefaultSampleSize = 0;
	uint32_t trunFirstSampleSize   = 0;

	// Walk the MOOF children.  Each box: [4-byte size][4-byte type][...].
	size_t pos = 8; // skip MOOF size+type
	while (pos + 8 <= moofSize)
	{
		uint32_t childSize = readBE32(buf + pos);
		if (childSize < 8 || pos + childSize > moofSize) break;

		// Look for the Track Fragment box ('traf').
		if (buf[pos+4]=='t' && buf[pos+5]=='r' && buf[pos+6]=='a' && buf[pos+7]=='f')
		{
			size_t trafEnd = pos + childSize;
			size_t trafPos = pos + 8; // skip traf size+type

			// Walk TRAF children looking for 'tfhd' and 'trun'.
			while (trafPos + 8 <= trafEnd)
			{
				uint32_t boxSize = readBE32(buf + trafPos);
				if (boxSize < 8 || trafPos + boxSize > trafEnd) break;

				if (buf[trafPos+4]=='t' && buf[trafPos+5]=='f' &&
				    buf[trafPos+6]=='h' && buf[trafPos+7]=='d')
				{
					// Track Fragment Header (FullBox): size(4)+type(4)+version(1)+flags(3)+track_id(4) = 16 bytes minimum.
					if (trafPos + 16 <= trafEnd)
					{
						uint32_t tfhdFlags = readBE24(buf + trafPos + 9); // flags after version byte
						size_t p = trafPos + 16; // after size+type+version+flags+track_id
						if (tfhdFlags & kTfhdBaseDataOffsetPresent)         p += 8;
						if (tfhdFlags & kTfhdSampleDescIndexPresent)        p += 4;
						if (tfhdFlags & kTfhdDefaultSampleDurationPresent)  p += 4;
						if ((tfhdFlags & kTfhdDefaultSampleSizePresent) && p + 4 <= trafEnd)
							tfhdDefaultSampleSize = readBE32(buf + p);
					}
				}
				else if (buf[trafPos+4]=='t' && buf[trafPos+5]=='r' &&
				         buf[trafPos+6]=='u' && buf[trafPos+7]=='n')
				{
					// Track Fragment Run (FullBox): size(4)+type(4)+version(1)+flags(3)+sample_count(4) = 16 bytes minimum.
					if (trafPos + 16 <= trafEnd)
					{
						uint32_t trunFlags   = readBE24(buf + trafPos + 9);
						uint32_t sampleCount = readBE32(buf + trafPos + 12);
						size_t p = trafPos + 16; // after size+type+version+flags+count
						if (trunFlags & kTrunDataOffsetPresent)       p += 4;
						if (trunFlags & kTrunFirstSampleFlagsPresent) p += 4;
						// Now at the start of the first sample's per-sample fields.
						if (sampleCount > 0)
						{
							if ((trunFlags & kTrunSampleDurationPresent) && p + 4 <= trafEnd) p += 4;
							if ((trunFlags & kTrunSampleSizePresent) && p + 4 <= trafEnd)
								trunFirstSampleSize = readBE32(buf + p);
						}
					}
				}
				trafPos += boxSize;
			}

			// Prefer TRUN per-sample size; fall back to TFHD default_sample_size.
			uint32_t firstSampleSize = (trunFirstSampleSize > 0) ? trunFirstSampleSize
			                                                      : tfhdDefaultSampleSize;
			if (firstSampleSize > 0)
			{
				// Total bytes needed: complete MOOF + MDAT box header (8 bytes) + first sample payload.
				return static_cast<size_t>(moofSize) + 8u + static_cast<size_t>(firstSampleSize);
			}
			break; // TRAF found but size undeterminable — fall back to full download.
		}
		pos += childSize;
	}

	return 0; // First-sample size could not be determined; caller falls back to full download.
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
