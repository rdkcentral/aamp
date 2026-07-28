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
#include <vector>
#include <string>
#include <thread>
#include <cstdint>
#include "AampLogManager.h"

/**
 * @brief Log entry stored in flight data recorder buffer
 */
struct FDRLogEntry
{
	uint64_t timestamp_us;
	int log_level;
	std::thread::id thread_id;
	uint32_t seq_num;
	int player_id;
	std::string source;
	std::string message;
	
	FDRLogEntry() : timestamp_us(0), log_level(0), thread_id(), seq_num(0), player_id(-1), source(), message() {}
};

/**
 * @class AampFlightDataRecorder
 * @brief Captures recent logs in a ring buffer for post-error diagnostics
 * 
 * This class implements a lock-free circular buffer that stores recent log entries
 * regardless of the current log level. When an ERROR occurs, the buffer contents
 * are dumped to provide context about what led to the error.
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
	 * @brief Initialize the flight data recorder
	 * @param enabled Enable/disable FDR
	 * @param maxLines Maximum number of log lines to store
	 * @param maxSeconds Maximum age of log entries in seconds
	 */
	void Initialize(bool enabled, size_t maxLines, uint64_t maxSeconds);
	
	/**
	 * @brief Add a log entry to the ring buffer
	 * @param entry Log entry to add
	 */
	void AddEntry(const FDRLogEntry& entry);
	
	/**
	 * @brief Dump all buffered log entries
	 * @param triggerLevel Log level that triggered the dump
	 * @param triggerSource Source of the trigger (AAMP-PLAYER or PLAYER_IF)
	 */
	void Dump(int triggerLevel, const char* triggerSource);
	
	/**
	 * @brief Flush the ring buffer
	 */
	void Flush();
	
	/**
	 * @brief Check if FDR is enabled
	 * @return true if enabled, false otherwise
	 */
	bool IsEnabled() const { return mEnabled.load(std::memory_order_relaxed); }
	
	/**
	 * @brief Set enabled state
	 * @param enabled New enabled state
	 */
	void SetEnabled(bool enabled) { mEnabled.store(enabled, std::memory_order_relaxed); }
	
	/**
	 * @brief Get current timestamp in microseconds
	 * @return Timestamp in microseconds since epoch
	 */
	static uint64_t GetCurrentTimeMicroseconds();

private:
	AampFlightDataRecorder();
	~AampFlightDataRecorder();
	
	AampFlightDataRecorder(const AampFlightDataRecorder&) = delete;
	AampFlightDataRecorder& operator=(const AampFlightDataRecorder&) = delete;
	
	/**
	 * @brief Evict old entries based on time threshold
	 */
	void EvictOldEntries();
	
	/**
	 * @brief Format a log entry for output
	 * @param entry Log entry to format
	 * @return Formatted string
	 */
	std::string FormatLogEntry(const FDRLogEntry& entry) const;
	
	/**
	 * @brief Get log level string
	 * @param level Log level
	 * @return String representation
	 */
	const char* GetLogLevelString(int level) const;

	std::vector<FDRLogEntry> mBuffer;
	std::atomic<size_t> mHead;
	std::atomic<size_t> mTail;
	std::atomic<size_t> mCount;
	std::atomic<bool> mEnabled;
	std::atomic<bool> mDumping;
	size_t mMaxEntries;
	uint64_t mMaxAgeUs;
	std::atomic<bool> mInitialized;
};

#endif // AAMP_FLIGHT_DATA_RECORDER_H
