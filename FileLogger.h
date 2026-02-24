/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file FileLogger.h
 */

#ifndef FILELOGGER_H
#define FILELOGGER_H

#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <cstdarg>

/**
 * @brief File Logger class
 * 
 * This class provides thread-safe file logging capabilities with automatic
 * resource management and proper error handling.
 */
class FileLogger final
{
public:
	/**
	 * @brief Logger state enumeration for state machine
	 */
	enum class LoggerState {
		TEMP_FILE_ACTIVE,   // Using /tmp/aamp_log_start.txt
		TARGET_FILE_ACTIVE, // Using timestamped file in custom path
		ERROR_STATE         // File operations failed
	};

private:
	mutable std::mutex m_mutex;
	std::unique_ptr<std::ofstream> m_fileStream;
	std::string m_logFilePath;
	LoggerState m_state;
	
	// Static members for custom path support
	static std::string s_customPath;
	
	/**
	 * @brief Generate timestamped log filename
	 * @return Timestamped filename string
	 */
	std::string generateTimestampedFilename() const noexcept;
	
	/**
	 * @brief Initialize the log file stream
	 * @return true if initialization successful, false otherwise
	 */
	bool initializeLogFile() noexcept;
	
	/**
	 * @brief Initialize temporary log file stream for startup logging
	 * @return true if initialization successful, false otherwise
	 */
	bool initializeTempLogFile() noexcept;
	
	/**
	 * @brief Create file with proper permissions (666)
	 * @param filePath Path to the file to create
	 * @return true if file created successfully, false otherwise
	 */
	bool createFileWithPermissions(const std::string& filePath) noexcept;
	
	/**
	 * @brief Initialize ofstream with the given file path
	 * @param filePath Path to the file to open
	 * @return true if stream initialized successfully, false otherwise
	 */
	bool initializeStream(const std::string& filePath) noexcept;
	
	// State machine handlers
	/**
	 * @brief Handle write when in TEMP_FILE_ACTIVE state
	 * @param message Formatted log message
	 */
	void handleTempFileWrite(const std::string& message) noexcept;
	
	/**
	 * @brief Handle write when in ERROR_STATE
	 * @param message Formatted log message
	 */
	void handleErrorWrite(const std::string& message) noexcept;
	
	/**
	 * @brief Attempt to switch from temp file to target file
	 */
	void attemptSwitchToTarget() noexcept;
	
	/**
	 * @brief Write message to current file stream
	 * @param message Formatted log message
	 */
	void writeToCurrentFile(const std::string& message) noexcept;
	
	/**
	 * @brief Format variadic arguments into string message
	 * @param format Printf-style format string
	 * @param args Variadic arguments
	 * @return Formatted message string
	 */
	std::string formatMessage(const char* format, va_list args) const noexcept;
	
	/**
	 * @brief Get current timestamp in ISO 8601 format
	 * @return Timestamp string in format YYYY-MM-DDTHH:MM:SS.sssZ
	 */
	std::string getCurrentTimestamp() const noexcept;
	
public:
	// Base filename constant (timestamp will be appended)
	static constexpr const char* LOG_FILENAME_BASE = "aamp_log";
	
	/**
	 * @brief Constructor - Initialize with default location
	 */
	FileLogger() noexcept;
	
	/**
	 * @brief Destructor - cleanup
	 */
	~FileLogger() noexcept;
	
	// Delete copy constructor and assignment operator
	FileLogger(const FileLogger&) = delete;
	FileLogger& operator=(const FileLogger&) = delete;
	
	// Allow move semantics
	FileLogger(FileLogger&&) noexcept = default;
	FileLogger& operator=(FileLogger&&) noexcept = default;
	
	/**
	 * @brief Write formatted log message to file
	 * @param format Printf-style format string
	 * @param args Variadic arguments
	 */
	void writeLog(const char* format, va_list args) const noexcept;
	
	/**
	 * @brief Write formatted log message to file with errno information
	 * @param format Printf-style format string
	 * @param args Variadic arguments
	 * @param errnoValue The errno value to log
	 */
	void writeLog(const char* format, va_list args, int errnoValue) const noexcept;
	
	/**
	 * @brief Get singleton instance with lazy initialization
	 * @return Reference to the singleton FileLogger instance
	 */
	static FileLogger& getInstance() noexcept;
	
	/**
	 * @brief Set custom path for log file (must be called before first getInstance)
	 * @param path Custom directory path to use (without filename)
	 * @return true if path was set successfully, false if instance already created
	 */
	static bool setCustomFilename(const std::string& path) noexcept;
};

#endif // FILELOGGER_H
