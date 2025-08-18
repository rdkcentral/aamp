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

bool FileLogger::initializeTempLogFile() noexcept
{
	const std::string tempLogPath = "/tmp/aamp_log_start.txt";
	
	std::cout << "[FileLogger::initializeTempLogFile] Attempting to open temporary log file: " << tempLogPath << std::endl;
	try 
	{
		// Create the temporary file with proper permissions (666) if it doesn't exist
		int fd = open(tempLogPath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 
		              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		
		if (fd != -1) {
			close(fd);  // Close the file descriptor, we'll use ofstream
			std::cout << "[FileLogger::initializeTempLogFile] Temporary file created/opened with 666 permissions" << std::endl;
		} else {
			std::cout << "[FileLogger::initializeTempLogFile] Warning: Failed to create temporary file, errno: " << errno << std::endl;
			return false;
		}
		
		// Open with ofstream
		m_fileStream.reset(new std::ofstream(
			tempLogPath, 
			std::ios::out | std::ios::app
		));
		
		if (m_fileStream && m_fileStream->is_open()) 
		{
			// Set unbuffered mode for immediate flushing
			m_fileStream->rdbuf()->pubsetbuf(nullptr, 0);
			m_isValid = true;
			m_logFilePath = tempLogPath;
			std::cout << "[FileLogger::initializeTempLogFile] Successfully opened temporary log file: " << tempLogPath << std::endl;
			return true;
		}
		else
		{
			std::cout << "[FileLogger::initializeTempLogFile] Failed to open temporary log file: " << tempLogPath << std::endl;
		}
	}
	catch (const std::exception& e) 
	{
		std::cout << "[FileLogger::initializeTempLogFile] Exception opening temporary log file: " << e.what() << std::endl;
	}
	return false;
}

bool FileLogger::initializeLogFile() noexcept
{
	// Cannot initialize without a custom path set
	if (s_customPath.empty()) {
		std::cout << "[FileLogger::initializeLogFile] Cannot initialize: no custom path set" << std::endl;
		return false;
	}
	
	// Construct full path with constant filename
	m_logFilePath = s_customPath;
	if (m_logFilePath.back() != '/') {
		m_logFilePath += "/";
	}
	m_logFilePath += LOG_FILENAME;
	
	std::cout << "[FileLogger::initializeLogFile] Attempting to open log file: " << m_logFilePath << std::endl;
	try 
	{
		// Create the file with proper permissions (666) if it doesn't exist
		int fd = open(m_logFilePath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 
		              S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		
		if (fd != -1) {
			close(fd);  // Close the file descriptor, we'll use ofstream
			std::cout << "[FileLogger::initializeLogFile] File created/opened with 666 permissions" << std::endl;
		} else {
			std::cout << "[FileLogger::initializeLogFile] Warning: Failed to create file with proper permissions, errno: " << errno << std::endl;
		}
		
		// Now open with ofstream
		m_fileStream.reset(new std::ofstream(
			m_logFilePath, 
			std::ios::out | std::ios::app
		));
		
		if (m_fileStream && m_fileStream->is_open()) 
		{
			// Set unbuffered mode for immediate flushing
			m_fileStream->rdbuf()->pubsetbuf(nullptr, 0);
			m_isValid = true;
			std::cout << "[FileLogger::initializeLogFile] Successfully opened log file: " << m_logFilePath << std::endl;
			return true;
		}
		else
		{
			std::cout << "[FileLogger::initializeLogFile] Failed to open log file: " << m_logFilePath << std::endl;
			if (m_fileStream) {
				std::cout << "[FileLogger::initializeLogFile] Stream created but is_open() returned false" << std::endl;
				std::cout << "[FileLogger::initializeLogFile] Stream state - good(): " << m_fileStream->good() 
				          << ", fail(): " << m_fileStream->fail() 
				          << ", bad(): " << m_fileStream->bad() 
				          << ", eof(): " << m_fileStream->eof() << std::endl;
			} else {
				std::cout << "[FileLogger::initializeLogFile] Failed to create stream object" << std::endl;
			}
		}
	}
	catch (const std::exception& e) 
	{
		std::cout << "[FileLogger::initializeLogFile] Exception opening log file: " << e.what() << std::endl;
		// Silent failure - logging should not crash the application
	}
	m_isValid = false;
	return false;
}

FileLogger::FileLogger() noexcept
	: m_fileStream(nullptr)
	, m_logFilePath("")
	, m_isValid(false)
{
	std::cout << "[FileLogger::FileLogger] Constructor called" << std::endl;
	std::cout << "[FileLogger::FileLogger] Custom path " << (s_customPath.empty() ? "NOT SET" : ("SET to: " + s_customPath)) << std::endl;
	
	// Initialize with temporary file for startup logging
	// This ensures we capture logs from the very beginning
	std::cout << "[FileLogger::FileLogger] Initializing with temporary log file for startup logging" << std::endl;
	initializeTempLogFile();
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
	
	// If custom path is set, try to switch from temp file to target location
	if (!s_customPath.empty()) {
		// Check if we're currently using the temp file and need to switch
		if (m_isValid && m_logFilePath == "/tmp/aamp_log_start.txt") {
			std::cout << "[FileLogger::writeLog] Switching from temporary file to target location: " << s_customPath << std::endl;
			
			// Close current temp file
			FileLogger* mutableThis = const_cast<FileLogger*>(this);
			if (mutableThis->m_fileStream && mutableThis->m_fileStream->is_open()) {
				mutableThis->m_fileStream->close();
			}
			mutableThis->m_isValid = false;
			
			// Try to initialize target file
			if (!mutableThis->initializeLogFile()) {
				std::cout << "[FileLogger::writeLog] Failed to initialize target log file, continuing with temporary file" << std::endl;
				// Re-open temporary file since we closed it
				if (!mutableThis->initializeTempLogFile()) {
					std::cout << "[FileLogger::writeLog] Failed to re-open temporary log file, discarding write" << std::endl;
					return;
				}
			}
		}
		// If target file is not initialized but custom path is set, try to initialize it
		else if (!m_isValid || !m_fileStream || !m_fileStream->is_open()) {
			std::cout << "[FileLogger::writeLog] Target file not initialized, attempting to initialize with path: " << s_customPath << std::endl;
			
			FileLogger* mutableThis = const_cast<FileLogger*>(this);
			if (!mutableThis->initializeLogFile()) {
				std::cout << "[FileLogger::writeLog] Failed to initialize target log file, falling back to temporary file" << std::endl;
				// Fall back to temporary file
				if (!mutableThis->initializeTempLogFile()) {
					std::cout << "[FileLogger::writeLog] Failed to initialize temporary log file, discarding write" << std::endl;
					return;
				}
			}
		}
	}
	// If no custom path is set, we should already have the temp file from constructor
	// No action needed - just proceed with writing
	
	// Check if file is still not valid after any initialization attempts
	if (!m_isValid || !m_fileStream || !m_fileStream->is_open()) 
	{
		std::cout << "[FileLogger::writeLog] FileLogger is not valid after initialization, discarding write" << std::endl;
		return;
	}
	
	try 
	{
		// Calculate required buffer size
		va_list args_copy;
		va_copy(args_copy, args);
		int size = vsnprintf(nullptr, 0, format, args_copy);
		va_end(args_copy);
		
		if (size <= 0) 
		{
			std::cout << "[FileLogger::writeLog] Invalid format string or size" << std::endl;
			return;
		}
		
		// Create buffer and format message
		std::vector<char> buffer(size + 1);
		vsnprintf(buffer.data(), buffer.size(), format, args);
		
		// Write to file
		if (m_fileStream && m_fileStream->is_open()) 
		{
			std::string timestamp = getCurrentTimestamp();
			*m_fileStream << timestamp << " " << buffer.data() << "\n";
			m_fileStream->flush(); // Ensure immediate write
		}
		else
		{
			std::cout << "[FileLogger::writeLog] File stream is not open for writing" << std::endl;
		}
	}
	catch (const std::exception& e) 
	{
		std::cout << "[FileLogger::writeLog] Exception during write: " << e.what() << std::endl;
		// Silent failure - logging should not crash the application
	}
}

bool FileLogger::isValid() const noexcept
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_isValid && m_fileStream && m_fileStream->is_open();
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
	
	// Check if custom path is already set
	if (!s_customPath.empty()) {
		std::cout << "[FileLogger::setCustomFilename] Custom path already set to: " << s_customPath << ", rejecting new path: " << path << std::endl;
	} else if (!path.empty()) {
		s_customPath = path;
		success = true;
		std::cout << "[FileLogger::setCustomFilename] Custom path set successfully" << std::endl;
	} else {
		std::cout << "[FileLogger::setCustomFilename] Empty path provided, ignoring" << std::endl;
	}
	
	return success;
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
