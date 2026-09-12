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

#ifndef AAMP_FLIGHT_DATA_RECORDER_H
#define AAMP_FLIGHT_DATA_RECORDER_H

/**
 * @file AampFlightDataRecorder.h
 * @brief Flight Data Recorder for AAMP logging - captures recent logs in ring buffer
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "AampLogManager.h"

/**
 * @brief Log entry stored in flight data recorder buffer
 */
struct FDRLogEntry
{
	uint64_t timestamp_ms;
	int log_level;
	std::thread::id thread_id;
	uint32_t seq_num;
	int player_id;
	bool include_player_id;
	std::string file;
	std::string func;
	int line;
	std::string source;
	std::string message;
	std::chrono::steady_clock::time_point recorded_at;

	FDRLogEntry() : timestamp_ms(0), log_level(0), thread_id(), seq_num(0), player_id(-1), include_player_id(true), file(), func(), line(0), source(), message(), recorded_at() {}
};

/**
 * @class AampFlightDataRecorder
 * @brief Captures recent logs in a ring buffer for post-error diagnostics
 */
class AampFlightDataRecorder
{
public:
	/**
	 * @brief Get singleton instance
	 * @return Reference to the singleton instance
	 */
	static AampFlightDataRecorder& GetInstance();

	/**
	 * @brief Initialize or reconfigure the flight data recorder
	 * @param enabled Enable/disable FDR
	 * @param maxLines Maximum number of log lines to store
	 * @param maxSeconds Maximum age of log entries in seconds
	 */
	void Initialize(bool enabled, size_t maxLines, uint64_t maxSeconds);

	/**
	 * @brief Add a log entry to the ring buffer
	 * @param entry Log entry to add
	 */
	bool AddEntry(FDRLogEntry entry);

	/**
	 * @brief Emit all buffered log entries and leave the buffer empty
	 * @param triggerLevel Log level that triggered the dump
	 * @param triggerSource Source of the trigger
	 */
	void Dump(int triggerLevel, const char* triggerSource);

	/**
	 * @brief Emit all buffered log entries and leave the buffer empty
	 * @param triggerLevel Log level that triggered the flush
	 * @param triggerSource Source of the trigger
	 */
	void Flush(int triggerLevel = eLOGLEVEL_MIL, const char* triggerSource = "EXPLICIT");

	/**
	 * @brief Check if FDR is enabled
	 * @return true if enabled, false otherwise
	 */
	bool IsEnabled() const { return mEnabled.load(std::memory_order_acquire); }

	/**
	 * @brief Set enabled state
	 * @param enabled New enabled state
	 */
	void SetEnabled(bool enabled);

	/**
	 * @brief Get current UTC timestamp in milliseconds
	 * @return Milliseconds since epoch
	 */
	static uint64_t GetCurrentTimeMilliseconds();

private:
	AampFlightDataRecorder();
	~AampFlightDataRecorder();

	AampFlightDataRecorder(const AampFlightDataRecorder&) = delete;
	AampFlightDataRecorder& operator=(const AampFlightDataRecorder&) = delete;

	void EvictionLoop();
	void EvictOldEntriesLocked(std::chrono::steady_clock::time_point now);
	void EvictEldestLocked();
	void EmitEntryLocked(const FDRLogEntry& entry) const;
	void FlushLocked(int triggerLevel, const char* triggerSource);
	std::string FormatLogEntry(const FDRLogEntry& entry) const;
	const char* GetLogLevelString(int level) const;

	mutable std::mutex mMutex;
	std::condition_variable mCondition;
	std::vector<FDRLogEntry> mBuffer;
	size_t mHead;
	size_t mTail;
	size_t mCount;
	std::atomic<bool> mEnabled;
	size_t mMaxEntries;
	uint64_t mMaxAgeMs;
	bool mInitialized;
	bool mStopping;
	std::thread mEvictionThread;
};

#endif // AAMP_FLIGHT_DATA_RECORDER_H
