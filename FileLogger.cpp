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
#include "AampConfig.h"

// Static member definitions
std::string FileLogger::s_customFilename = "";

bool FileLogger::initializeLogFile() noexcept
{
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
			return true;
		}
	}
	catch (const std::exception&) 
	{
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
			return;
		}
		
		// Create buffer and format message
		std::vector<char> buffer(size + 1);
		vsnprintf(buffer.data(), buffer.size(), format, args);
		
		// Write to file
		if (m_fileStream && m_fileStream->is_open()) 
		{
			*m_fileStream << buffer.data();
			m_fileStream->flush(); // Ensure immediate write
		}
	}
	catch (const std::exception&) 
	{
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
		instance = new FileLogger();
	});
	
	return *instance;
}

bool FileLogger::setCustomFilename(const std::string& filename) noexcept
{
	if (!filename.empty()) {
		s_customFilename = filename;
		return true;
	}
	
	return false;
}
