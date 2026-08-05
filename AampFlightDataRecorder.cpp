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
 * @file AampFlightDataRecorder.cpp
 * @brief Implementation of Flight Data Recorder for AAMP logging
 */

#include "AampFlightDataRecorder.h"
#include <cstdio>
#include <sys/time.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <inttypes.h>

static const char* LOG_LEVEL_STRINGS[] = {
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"MIL",
	"ERROR"
};

AampFlightDataRecorder::AampFlightDataRecorder()
	: mBuffer()
	, mHead(0)
	, mTail(0)
	, mCount(0)
	, mEnabled{false}
	, mDumping{false}
	, mMaxEntries(5000)
	, mMaxAgeUs(60000000)
	, mInitialized{false}
{
}

AampFlightDataRecorder::~AampFlightDataRecorder()
{
}

AampFlightDataRecorder& AampFlightDataRecorder::GetInstance()
{
	static AampFlightDataRecorder instance;
	return instance;
}

void AampFlightDataRecorder::Initialize(bool enabled, size_t maxLines, uint64_t maxSeconds)
{
	if (mInitialized.load(std::memory_order_acquire))
	{
		return;
	}
	
	mMaxEntries = maxLines;
	mMaxAgeUs = maxSeconds * 1000000ULL;
	mEnabled.store(enabled, std::memory_order_relaxed);
	
	mBuffer.resize(mMaxEntries);
	
	mHead.store(0, std::memory_order_relaxed);
	mTail.store(0, std::memory_order_relaxed);
	mCount.store(0, std::memory_order_relaxed);
	
	mInitialized.store(true, std::memory_order_release);
}

uint64_t AampFlightDataRecorder::GetCurrentTimeMicroseconds()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

void AampFlightDataRecorder::EvictOldEntries()
{
	if (mCount.load(std::memory_order_relaxed) == 0)
	{
		return;
	}
	
	uint64_t now = GetCurrentTimeMicroseconds();
	uint64_t cutoff = (now > mMaxAgeUs) ? (now - mMaxAgeUs) : 0;
	
	while (mCount.load(std::memory_order_acquire) > 0)
	{
		size_t tail_pos = mTail.load(std::memory_order_acquire);
		size_t read_pos = tail_pos % mMaxEntries;
		
		if (mBuffer[read_pos].timestamp_us >= cutoff)
		{
			break;
		}
		
		size_t new_tail = tail_pos + 1;
		if (mTail.compare_exchange_weak(tail_pos, new_tail, std::memory_order_release))
		{
			mCount.fetch_sub(1, std::memory_order_relaxed);
		}
	}
}

void AampFlightDataRecorder::AddEntry(const FDRLogEntry& entry)
{
	if (!mEnabled.load(std::memory_order_relaxed) || !mInitialized.load(std::memory_order_acquire))
	{
		return;
	}
	
	if (mDumping.load(std::memory_order_relaxed))
	{
		return;
	}
	
	EvictOldEntries();
	
	// Atomically claim a write slot.  fetch_add gives each producer a unique
	// monotonically-increasing index, so concurrent writers never share a slot.
	size_t write_pos_idx = mHead.fetch_add(1, std::memory_order_acq_rel);
	size_t write_pos = write_pos_idx % mMaxEntries;
	
	mBuffer[write_pos] = entry;
	
	// After claiming head, advance tail if the buffer is now full so that the
	// fill level (head - tail) never exceeds mMaxEntries.  Use a CAS loop so
	// that concurrent producers converge on the correct tail position without
	// double-advancing it.  mCount is kept consistent as a derived quantity.
	size_t new_head = write_pos_idx + 1;
	size_t current_tail = mTail.load(std::memory_order_acquire);
	if (new_head - current_tail > mMaxEntries)
	{
		// Try to advance tail by exactly one slot.  If another producer already
		// advanced it (or EvictOldEntries did), the CAS will fail and we leave
		// it alone — the buffer is no longer over-full from our perspective.
		size_t expected_tail = current_tail;
		if (mTail.compare_exchange_strong(expected_tail, current_tail + 1,
		                                   std::memory_order_release,
		                                   std::memory_order_relaxed))
		{
			// We advanced tail, so one entry was implicitly evicted — keep
			// mCount capped at mMaxEntries.
			mCount.store(mMaxEntries, std::memory_order_relaxed);
		}
		// else: another thread already advanced tail; count unchanged.
	}
	else
	{
		// Buffer was not full; this entry is a net addition.
		size_t old_count = mCount.fetch_add(1, std::memory_order_relaxed);
		if (old_count >= mMaxEntries)
		{
			// Clamp — can happen when a concurrent EvictOldEntries races.
			mCount.store(mMaxEntries, std::memory_order_relaxed);
		}
	}
}

const char* AampFlightDataRecorder::GetLogLevelString(int level) const
{
	if (level >= 0 && level <= 5)
	{
		return LOG_LEVEL_STRINGS[level];
	}
	return "UNKNOWN";
}

std::string AampFlightDataRecorder::FormatLogEntry(const FDRLogEntry& entry) const
{
	std::ostringstream oss;
	
	uint64_t sec = entry.timestamp_us / 1000000;
	uint64_t msec = (entry.timestamp_us % 1000000) / 1000;
	
	oss << sec << "." << std::setfill('0') << std::setw(3) << msec << ": ";
	
	oss << "[" << entry.source << "]";
	oss << "[" << std::setfill('0') << std::setw(3) << entry.seq_num << "]";
	
	if (entry.player_id >= 0)
	{
		oss << "[" << entry.player_id << "]";
	}
	
	oss << "[" << GetLogLevelString(entry.log_level) << "]";
	
	std::hash<std::thread::id> hasher;
	oss << "[" << std::hex << hasher(entry.thread_id) << std::dec << "]";
	
	oss << entry.message;
	
	return oss.str();
}

void AampFlightDataRecorder::Dump(int triggerLevel, const char* triggerSource)
{
	if (!mInitialized.load(std::memory_order_acquire))
	{
		printf( "not mInitialized\n" );
		return;
	}
	
	bool expected = false;
	if (!mDumping.compare_exchange_strong(expected, true, std::memory_order_acquire))
	{
		printf( "mDumping broken\n" );
		return;
	}
	
	size_t entries_to_read = mCount.load(std::memory_order_acquire);
	
	if (entries_to_read == 0)
	{
		printf( "no entries to log\n" );
		mDumping.store(false, std::memory_order_release);
		return;
	}
	
	// these need to be cleaned up and routed to ethanlogger
	printf("\n");
	printf("================================================================================\n");
	printf("[FDR] FLIGHT DATA RECORDER DUMP (triggered by %s %s)\n", 
	       triggerSource, GetLogLevelString(triggerLevel));
	printf("[FDR] Captured %zu log entries from last %" PRIu64 " seconds\n", 
	       entries_to_read, mMaxAgeUs / 1000000);
	printf("================================================================================\n");
	
	size_t tail_pos = mTail.load(std::memory_order_acquire);
	printf( "entries_to_read=%zu\n", entries_to_read );
	for (size_t i = 0; i < entries_to_read; i++)
	{
		size_t read_pos = (tail_pos + i) % mMaxEntries;
		const FDRLogEntry& entry = mBuffer[read_pos];
		
		if (entry.timestamp_us > 0)
		{
			printf("[FDR] %s\n", FormatLogEntry(entry).c_str());
		}
	}
	
	printf("================================================================================\n");
	printf("[FDR] END FLIGHT DATA RECORDER DUMP\n");
	printf("================================================================================\n");
	printf("\n");
	
	mDumping.store(false, std::memory_order_release);
}

void AampFlightDataRecorder::Flush()
{
	if (!mInitialized.load(std::memory_order_acquire))
	{
		return;
	}
	
	size_t head_pos = mHead.load(std::memory_order_relaxed);
	mTail.store(head_pos, std::memory_order_release);
	mCount.store(0, std::memory_order_release);
}
