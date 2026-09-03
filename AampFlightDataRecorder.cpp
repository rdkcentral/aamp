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
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <inttypes.h>
#include <sstream>

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
	, mEnabled(false)
	, mMaxEntries(5000)
	, mMaxAgeMs(15000)
	, mInitialized(false)
	, mStopping(false)
	, mEvictionThread()
{
}

AampFlightDataRecorder::~AampFlightDataRecorder()
{
	{
		std::lock_guard<std::mutex> lock(mMutex);
		mStopping = true;
		mCondition.notify_all();
	}
	if (mEvictionThread.joinable())
	{
		mEvictionThread.join();
	}
}

AampFlightDataRecorder& AampFlightDataRecorder::GetInstance()
{
	static AampFlightDataRecorder instance;
	return instance;
}

void AampFlightDataRecorder::Initialize(bool enabled, size_t maxLines, uint64_t maxSeconds)
{
	maxLines = maxLines > 0 ? maxLines : 1;
	maxLines = maxLines <= 100000 ? maxLines : 100000;
	maxSeconds = maxSeconds > 0 ? maxSeconds : 1;
	maxSeconds = maxSeconds <= 3600 ? maxSeconds : 3600;

	std::lock_guard<std::mutex> lock(mMutex);
	const uint64_t maxAgeMs = maxSeconds * 1000ULL;
	if (!mInitialized)
	{
		std::vector<FDRLogEntry> replacement(maxLines);
		std::thread evictionThread(&AampFlightDataRecorder::EvictionLoop, this);
		mBuffer.swap(replacement);
		mMaxEntries = maxLines;
		mMaxAgeMs = maxAgeMs;
		mEvictionThread = std::move(evictionThread);
		mInitialized = true;
	}
	else if (maxLines != mMaxEntries || maxAgeMs != mMaxAgeMs)
	{
		std::vector<FDRLogEntry> replacement(maxLines);
		std::vector<FDRLogEntry> evicted;
		evicted.reserve(mCount);
		auto now = std::chrono::steady_clock::now();
		size_t firstRetained = mCount > maxLines ? mCount - maxLines : 0;
		size_t replacementCount = 0;
		for (size_t i = 0; i < mCount; ++i)
		{
			const FDRLogEntry& entry = mBuffer[(mTail + i) % mMaxEntries];
			bool expired = entry.recorded_at != std::chrono::steady_clock::time_point() &&
				now - entry.recorded_at >= std::chrono::milliseconds(maxAgeMs);
			if (!enabled || expired || i < firstRetained)
			{
				evicted.push_back(entry);
			}
			else
			{
				replacement[replacementCount++] = entry;
			}
		}
		mBuffer.swap(replacement);
		mMaxEntries = maxLines;
		mMaxAgeMs = maxAgeMs;
		mTail = 0;
		mCount = replacementCount;
		mHead = replacementCount % mMaxEntries;
		for (const FDRLogEntry& entry : evicted)
		{
			EmitEntryLocked(entry);
		}
	}
	else if (!enabled)
	{
		while (mCount > 0)
		{
			EvictEldestLocked();
		}
		mHead = mTail;
	}

	mEnabled.store(enabled, std::memory_order_release);
	mCondition.notify_all();
}

void AampFlightDataRecorder::SetEnabled(bool enabled)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!enabled)
	{
		while (mCount > 0)
		{
			EvictEldestLocked();
		}
		mHead = mTail;
	}
	mEnabled.store(enabled, std::memory_order_release);
	mCondition.notify_all();
}

uint64_t AampFlightDataRecorder::GetCurrentTimeMilliseconds()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

void AampFlightDataRecorder::EmitEntryLocked(const FDRLogEntry& entry) const
{
	if (entry.timestamp_ms > 0 && (entry.log_level == eLOGLEVEL_WARN || entry.log_level == eLOGLEVEL_MIL))
	{
		std::string formatted = FormatLogEntry(entry);
		emitLogLine(entry.log_level, formatted.c_str(),
			AampLogManager::disableLogRedirection,
			AampLogManager::enableEthanLogRedirection);
	}
}

void AampFlightDataRecorder::EvictEldestLocked()
{
	if (mCount == 0)
	{
		return;
	}

	FDRLogEntry evicted = std::move(mBuffer[mTail]);
	mBuffer[mTail] = FDRLogEntry();
	mTail = (mTail + 1) % mMaxEntries;
	--mCount;
	EmitEntryLocked(evicted);
}

void AampFlightDataRecorder::EvictOldEntriesLocked(std::chrono::steady_clock::time_point now)
{
	while (mCount > 0)
	{
		const FDRLogEntry& eldest = mBuffer[mTail];
		if (eldest.recorded_at == std::chrono::steady_clock::time_point() ||
			now - eldest.recorded_at < std::chrono::milliseconds(mMaxAgeMs))
		{
			break;
		}
		EvictEldestLocked();
	}
}

void AampFlightDataRecorder::EvictionLoop()
{
	std::unique_lock<std::mutex> lock(mMutex);
	while (!mStopping)
	{
		if (!mInitialized || !mEnabled.load(std::memory_order_acquire) || mCount == 0)
		{
			mCondition.wait(lock);
			continue;
		}

		auto now = std::chrono::steady_clock::now();
		const FDRLogEntry& eldest = mBuffer[mTail];
		auto age = now - eldest.recorded_at;
		if (age >= std::chrono::milliseconds(mMaxAgeMs))
		{
			EvictOldEntriesLocked(now);
			continue;
		}

		mCondition.wait_for(lock, std::chrono::milliseconds(mMaxAgeMs) - age);
	}
}

bool AampFlightDataRecorder::AddEntry(FDRLogEntry entry)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mInitialized || !mEnabled.load(std::memory_order_acquire))
	{
		return false;
	}

	auto steadyNow = std::chrono::steady_clock::now();
	uint64_t utcNowMs = GetCurrentTimeMilliseconds();
	if (entry.timestamp_ms == 0)
	{
		entry.timestamp_ms = utcNowMs;
		entry.recorded_at = steadyNow;
	}
	else if (entry.recorded_at == std::chrono::steady_clock::time_point())
	{
		uint64_t ageMs = entry.timestamp_ms < utcNowMs ? utcNowMs - entry.timestamp_ms : 0;
		entry.recorded_at = steadyNow - std::chrono::milliseconds(ageMs);
	}

	EvictOldEntriesLocked(steadyNow);
	if (mCount == mMaxEntries)
	{
		EvictEldestLocked();
	}

	mBuffer[mHead] = std::move(entry);
	mHead = (mHead + 1) % mMaxEntries;
	++mCount;
	mCondition.notify_all();
	return true;
}

const char* AampFlightDataRecorder::GetLogLevelString(int level) const
{
	if (level >= eLOGLEVEL_TRACE && level <= eLOGLEVEL_ERROR)
	{
		return LOG_LEVEL_STRINGS[level];
	}
	return "UNKNOWN";
}

std::string AampFlightDataRecorder::FormatLogEntry(const FDRLogEntry& entry) const
{
	std::ostringstream oss;
	uint64_t sec = entry.timestamp_ms / 1000;
	uint64_t msec = entry.timestamp_ms % 1000;
	oss << sec << "." << std::setfill('0') << std::setw(3) << msec << ": ";
	oss << "[" << entry.source << "]";
	oss << "[" << std::setfill('0') << std::setw(3) << entry.seq_num << "]";
	if (entry.include_player_id)
	{
		oss << "[" << entry.player_id << "]";
	}
	oss << "[" << GetLogLevelString(entry.log_level) << "]";
	std::hash<std::thread::id> hasher;
	oss << "[" << std::hex << hasher(entry.thread_id) << std::dec << "]";
	if (AampLogManager::logFilename && !entry.file.empty())
	{
		const char* basename = strrchr(entry.file.c_str(), '/');
		oss << "[" << (basename ? basename + 1 : entry.file.c_str()) << "]";
	}
	oss << "[" << entry.func << "][" << entry.line << "]" << entry.message;
	return oss.str();
}

void AampFlightDataRecorder::FlushLocked(int triggerLevel, const char* triggerSource)
{
	if (mCount == 0)
	{
		return;
	}

	bool disableRedir = AampLogManager::disableLogRedirection;
	bool enableEthan = AampLogManager::enableEthanLogRedirection;
	char header[256];
	snprintf(header, sizeof(header),
		"[FDR] FLIGHT DATA RECORDER DUMP (triggered by %s %s) %zu entries from last %" PRIu64 "s",
		triggerSource, GetLogLevelString(triggerLevel), mCount, mMaxAgeMs / 1000);
	emitLogLine(triggerLevel, header, disableRedir, enableEthan);

	while (mCount > 0)
	{
		FDRLogEntry entry = std::move(mBuffer[mTail]);
		mBuffer[mTail] = FDRLogEntry();
		mTail = (mTail + 1) % mMaxEntries;
		--mCount;
		if (entry.timestamp_ms > 0)
		{
			std::string formatted = FormatLogEntry(entry);
			emitLogLine(entry.log_level, formatted.c_str(), disableRedir, enableEthan);
		}
	}
	mHead = mTail;
	emitLogLine(triggerLevel, "[FDR] END FLIGHT DATA RECORDER DUMP", disableRedir, enableEthan);
}

void AampFlightDataRecorder::Dump(int triggerLevel, const char* triggerSource)
{
	Flush(triggerLevel, triggerSource);
}

void AampFlightDataRecorder::Flush(int triggerLevel, const char* triggerSource)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (!mInitialized || !mEnabled.load(std::memory_order_acquire))
	{
		return;
	}
	FlushLocked(triggerLevel, triggerSource);
	mCondition.notify_all();
}
