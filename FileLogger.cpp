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
 * @file FileLogger.cpp
 * @brief File Logger implementation
 */

#include "FileLogger.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include "AampConfig.h"

// Static member definitions
std::string FileLogger::s_customPath = "";

bool FileLogger::createFileWithPermissions(const std::string& filePath) noexcept
{
	int fd = open(filePath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 
	              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	
	if (fd != -1) {
		close(fd);
		std::cout << "[FileLogger::createFileWithPermissions] File created/opened with 666 permissions: " << filePath << std::endl;
		return true;
	} else {
		std::cout << "[FileLogger::createFileWithPermissions] Warning: Failed to create file, errno: " << errno << " for path: " << filePath << std::endl;
		return false;
	}
}

bool FileLogger::initializeStream(const std::string& filePath) noexcept
{
	try {
		m_fileStream.reset(new std::ofstream(filePath, std::ios::out | std::ios::app));
		
		if (m_fileStream && m_fileStream->is_open()) {
			// Set unbuffered mode for immediate flushing
			m_fileStream->rdbuf()->pubsetbuf(nullptr, 0);
			m_logFilePath = filePath;
			std::cout << "[FileLogger::initializeStream] Successfully opened log file: " << filePath << std::endl;
			return true;
		} else {
			std::cout << "[FileLogger::initializeStream] Failed to open log file: " << filePath << std::endl;
			if (m_fileStream) {
				std::cout << "[FileLogger::initializeStream] Stream created but is_open() returned false" << std::endl;
				std::cout << "[FileLogger::initializeStream] Stream state - good(): " << m_fileStream->good() 
				          << ", fail(): " << m_fileStream->fail() 
				          << ", bad(): " << m_fileStream->bad() 
				          << ", eof(): " << m_fileStream->eof() << std::endl;
			} else {
				std::cout << "[FileLogger::initializeStream] Failed to create stream object" << std::endl;
			}
		}
	} catch (const std::exception& e) {
		std::cout << "[FileLogger::initializeStream] Exception opening log file: " << e.what() << std::endl;
	}
	
	return false;
}

bool FileLogger::initializeTempLogFile() noexcept
{
	const std::string tempLogPath = "/tmp/aamp_log_start.txt";
	
	std::cout << "[FileLogger::initializeTempLogFile] Attempting to open temporary log file: " << tempLogPath << std::endl;
	
	// Check if we already have a valid file open
	if (m_fileStream && m_fileStream->is_open()) {
		std::cout << "[FileLogger::initializeTempLogFile] Log file already open and valid" << std::endl;
		return true;
	}
	
	if (createFileWithPermissions(tempLogPath)) {
		return initializeStream(tempLogPath);
	}
	
	return false;
}

bool FileLogger::initializeLogFile() noexcept
{
	// Construct full path with timestamped filename
	std::string targetPath = s_customPath;
	if (targetPath.back() != '/') {
		targetPath += "/";
	}
	targetPath += generateTimestampedFilename();
	
	std::cout << "[FileLogger::initializeLogFile] Attempting to open log file: " << targetPath << std::endl;
	
	if (createFileWithPermissions(targetPath)) {
		return initializeStream(targetPath);
	}
	
	return false;
}

FileLogger::FileLogger() noexcept
	: m_fileStream(nullptr)
	, m_logFilePath("")
{
	std::cout << "[FileLogger::FileLogger] Constructor called" << std::endl;
	std::cout << "[FileLogger::FileLogger] Custom path " << (s_customPath.empty() ? "NOT SET" : ("SET to: " + s_customPath)) << std::endl;
	
	// Initialize with temporary file for startup logging
	// This ensures we capture logs from the very beginning
	std::cout << "[FileLogger::FileLogger] Initializing with temporary log file for startup logging" << std::endl;
	if (initializeTempLogFile()) {
		m_state = LoggerState::TEMP_FILE_ACTIVE;
	} else {
		m_state = LoggerState::ERROR_STATE;
	}
}

FileLogger::~FileLogger() noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_fileStream && m_fileStream->is_open()) 
	{
		m_fileStream->flush();
		m_fileStream->close();
	}
}

void FileLogger::writeLog(const char* format, va_list args) const noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	
	// Format message once, outside state logic
	std::string message = formatMessage(format, args);
	if (message.empty()) {
		return; // Invalid format or formatting failed
	}
	
	// State machine dispatch - single switch statement!
	FileLogger* mutableThis = const_cast<FileLogger*>(this);
	switch (m_state) {
		case LoggerState::TEMP_FILE_ACTIVE:
			mutableThis->handleTempFileWrite(message);
			break;
			
		case LoggerState::TARGET_FILE_ACTIVE:
			mutableThis->writeToCurrentFile(message);
			break;
			
		case LoggerState::ERROR_STATE:
			mutableThis->handleErrorWrite(message);
			break;
	}
}

void FileLogger::writeLog(const char* format, va_list args, int errnoValue) const noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	
	// Format message with errno information appended
	std::string message = formatMessage(format, args);
	if (message.empty()) {
		return; // Invalid format or formatting failed
	}
	
	// Append errno information
	char errnoBuffer[256];
	snprintf(errnoBuffer, sizeof(errnoBuffer), " [errno=%d: %s]", errnoValue, strerror(errnoValue));
	message += errnoBuffer;
	
	// State machine dispatch - single switch statement!
	FileLogger* mutableThis = const_cast<FileLogger*>(this);
	switch (m_state) {
		case LoggerState::TEMP_FILE_ACTIVE:
			mutableThis->handleTempFileWrite(message);
			break;
			
		case LoggerState::TARGET_FILE_ACTIVE:
			mutableThis->writeToCurrentFile(message);
			break;
			
		case LoggerState::ERROR_STATE:
			mutableThis->handleErrorWrite(message);
			break;
	}
}

void FileLogger::handleTempFileWrite(const std::string& message) noexcept
{
	// Attempt to switch to target file if custom path is available
	if (!s_customPath.empty()) {
		attemptSwitchToTarget();
	}
	
	// Write to current file unless we're in error state
	if (m_state != LoggerState::ERROR_STATE) {
		writeToCurrentFile(message);
	}
}

void FileLogger::handleErrorWrite(const std::string& message) noexcept
{
	// Try to recover by initializing temp file
	std::cout << "[FileLogger::handleErrorWrite] Attempting recovery" << std::endl;
	if (initializeTempLogFile()) {
		m_state = LoggerState::TEMP_FILE_ACTIVE;
		writeToCurrentFile(message);
	}
	// If recovery fails, message is lost
}

void FileLogger::attemptSwitchToTarget() noexcept
{
	// Close current temp file
	if (m_fileStream && m_fileStream->is_open()) {
		m_fileStream->close();
	}
	
	// Try to initialize target file
	if (initializeLogFile()) {
		m_state = LoggerState::TARGET_FILE_ACTIVE;
		std::cout << "[FileLogger::attemptSwitchToTarget] Successfully switched to target file" << std::endl;
	} else {
		std::cout << "[FileLogger::attemptSwitchToTarget] Failed to switch, falling back to temp file" << std::endl;
		// Fall back to temporary file
		if (initializeTempLogFile()) {
			m_state = LoggerState::TEMP_FILE_ACTIVE;
		} else {
			m_state = LoggerState::ERROR_STATE;
		}
	}
}

void FileLogger::writeToCurrentFile(const std::string& message) noexcept
{
	if (!m_fileStream || !m_fileStream->is_open()) {
		m_state = LoggerState::ERROR_STATE;
		return;
	}
	
	try {
		// Direct write to reduce string copies
		*m_fileStream << getCurrentTimestamp() << " " << message;
		m_fileStream->flush(); // Ensure immediate write
	} catch (const std::exception& e) {
		std::cout << "[FileLogger::writeToCurrentFile] Exception during write: " << e.what() << std::endl;
		if (m_fileStream) {
			m_fileStream->close();
		}
		m_state = LoggerState::ERROR_STATE;
	}
}

FileLogger& FileLogger::getInstance() noexcept
{
	// Thread-safe singleton with lazy initialization
	static FileLogger* instance = nullptr;
	static std::once_flag initialized;
	
	std::call_once(initialized, []() {
		std::cout << "[FileLogger::getInstance] Creating singleton instance" << std::endl;
		instance = new FileLogger();
	});
	
	return *instance;
}

bool FileLogger::setCustomFilename(const std::string& path) noexcept
{
	bool success = false;
	
	if (!s_customPath.empty()) {
		std::cout << "[FileLogger::setCustomFilename] Custom path already set to: " << s_customPath << ", rejecting new path: " << path << std::endl;
	} else if (path.empty()) {
		std::cout << "[FileLogger::setCustomFilename] Empty path provided, ignoring" << std::endl;
	} else {
		s_customPath = path;
		success = true;
		std::cout << "[FileLogger::setCustomFilename] Custom path set successfully" << std::endl;
	}
	
	return success;
}

std::string FileLogger::formatMessage(const char* format, va_list args) const noexcept
{
	try 
	{
		// Calculate required buffer size
		va_list args_copy;
		va_copy(args_copy, args);
		int size = vsnprintf(nullptr, 0, format, args_copy);
		va_end(args_copy);
		
		if (size <= 0) 
		{
			return "";
		}
		
		// Use thread-local static buffer that grows as needed
		static thread_local std::vector<char> buffer;
		size_t requiredSize = static_cast<size_t>(size) + 1;
		
		// Only resize if we need more space
		if (buffer.size() < requiredSize) {
			buffer.resize(requiredSize + 128); // Add some extra space to reduce future allocations
		}
		
		vsnprintf(buffer.data(), requiredSize, format, args);
		
		return std::string(buffer.data());
	}
	catch (...)
	{
		return "";
	}
}

std::string FileLogger::generateTimestampedFilename() const noexcept
{
	try 
	{
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		
		std::stringstream ss;
		ss << LOG_FILENAME_BASE << "_" 
		   << std::put_time(std::gmtime(&time_t), "%Y%m%d_%H%M%S") 
		   << ".txt";
		
		return ss.str();
	}
	catch (...)
	{
		// Fallback to simple filename if timestamp generation fails
		return std::string(LOG_FILENAME_BASE) + "_fallback.txt";
	}
}

std::string FileLogger::getCurrentTimestamp() const noexcept
{
	try 
	{
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()) % 1000;
		
		std::stringstream ss;
		ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
		ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
		
		return ss.str();
	}
	catch (...)
	{
		return "1970-01-01T00:00:00.000Z"; // Fallback timestamp
	}
}
