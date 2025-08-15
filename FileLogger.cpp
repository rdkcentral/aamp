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
#include "AampConfig.h"

// Static member definitions
std::string FileLogger::s_customFilename = "";

bool FileLogger::initializeLogFile() noexcept
{
	std::cout << "[FileLogger::initializeLogFile] Attempting to open log file: " << m_logFilePath << std::endl;
	try 
	{
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
	, m_logFilePath(s_customFilename.empty() ? "/tmp/aamp_log.txt" : s_customFilename)
	, m_isValid(false)
{
	std::cout << "[FileLogger::FileLogger] Constructor called with path: " << m_logFilePath << std::endl;
	std::cout << "[FileLogger::FileLogger] Custom filename " << (s_customFilename.empty() ? "NOT SET" : ("SET to: " + s_customFilename)) << std::endl;
	initializeLogFile();
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
	if (!isValid()) 
	{
		std::cout << "[FileLogger::writeLog] FileLogger is not valid, skipping write" << std::endl;
		return;
	}
	
	std::lock_guard<std::mutex> lock(m_mutex);
	
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

bool FileLogger::setCustomFilename(const std::string& filename) noexcept
{
	std::cout << "[FileLogger::setCustomFilename] Called with filename: " << filename << std::endl;
	if (!filename.empty()) {
		s_customFilename = filename;
		std::cout << "[FileLogger::setCustomFilename] Custom filename set successfully" << std::endl;
		return true;
	}
	
	std::cout << "[FileLogger::setCustomFilename] Empty filename provided, ignoring" << std::endl;
	return false;
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
