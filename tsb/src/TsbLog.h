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

#ifndef __TSB_LOG__
#define __TSB_LOG__

#include <sstream>
#include <string_view>
#include <ostream>
#include <filesystem>

#include "TsbApi.h"

// ENABLE_TSB_LOGGER must be defined to enable logging at build-time
#ifdef ENABLE_TSB_LOGGER
#define EMIT_TSB_LOGS true
#else
#define EMIT_TSB_LOGS false
#endif

/**
 *  @fn TSB_LOG_ERROR
 *  @brief Logs an error message. The message should be meaningful to non-developers working
 *         with production environments. It should contain contextual information about the
 *         error as key-value pairs.
 *
 *         Format:
 *           - Arguments must start with a message string, followed by zero or more key-value pairs.
 *           - Keys must be camelCase strings, but values can be anything that a stream can output
 *             using the << operator.
 *           - To format an integral type in hexadecimal, use TSB_LOG_AS_HEX().
 *           - There is no fixed limit to the number of key-value pairs, but be mindful of client
 *             log line limits.
 *
 *         Example:
 *             TSB_LOG_ERROR(mLogger, "A meaningful error message",
 *                 "myFirstKey", first_value, "mySecondKey", TSB_LOG_AS_HEX(second_value));
 *
 *  @param[in] logger - client logger instance where the message should be sent.
 *  @param[in] ... - message followed by zero or more key-value pairs.
 */
#define TSB_LOG_ERROR(logger, ...) TSB_LOG(logger, TSB::LogLevel::ERROR, __VA_ARGS__)

/**
 *  @fn TSB_LOG_MIL
 *  @brief Logs a milestone message. The message should be meaningful to non-developers working
 *         with production environments. It should contain contextual information about the
 *         milestone as key-value pairs.
 *
 *         NOTE: MIL logs must be used sparingly, for example after a significant operation
 *         has completed. They should be discussed and agreed with an architect.
 *
 *         Format:
 *           - Arguments must start with a message string, followed by zero or more key-value pairs.
 *           - Keys must be camelCase strings, but values can be anything that a stream can output
 *             using the << operator.
 *           - To format an integral type in hexadecimal, use TSB_LOG_AS_HEX().
 *           - There is no fixed limit to the number of key-value pairs, but be mindful of client
 *             log line limits.
 *
 *         Example:
 *             TSB_LOG_MIL(mLogger, "A meaningful milestone message",
 *                 "myFirstKey", first_value, "mySecondKey", TSB_LOG_AS_HEX(second_value));
 *
 *  @param[in] logger - client logger instance where the message should be sent.
 *  @param[in] ... - message followed by zero or more key-value pairs.
 */
#define TSB_LOG_MIL(logger, ...) TSB_LOG(logger, TSB::LogLevel::MIL, __VA_ARGS__)

/**
 *  @fn TSB_LOG_WARN
 *  @brief Logs a warning message. The message should be meaningful to non-developers working
 *         with production environments. It should contain contextual information about the
 *         warning as key-value pairs.
 *
 *         Format:
 *           - Arguments must start with a message string, followed by zero or more key-value pairs.
 *           - Keys must be camelCase strings, but values can be anything that a stream can output
 *             using the << operator.
 *           - To format an integral type in hexadecimal, use TSB_LOG_AS_HEX().
 *           - There is no fixed limit to the number of key-value pairs, but be mindful of client
 *             log line limits.
 *
 *         Example:
 *             TSB_LOG_WARN(mLogger, "A meaningful warning message",
 *                 "myFirstKey", first_value, "mySecondKey", TSB_LOG_AS_HEX(second_value));
 *
 *  @param[in] logger - client logger instance where the message should be sent.
 *  @param[in] ... - message followed by zero or more key-value pairs.
 */
#define TSB_LOG_WARN(logger, ...) TSB_LOG(logger, TSB::LogLevel::WARN, __VA_ARGS__)

/**
 *  @fn TSB_LOG_TRACE
 *  @brief Logs a debug trace message. Trace messages are to help developers with debugging,
 *         and do not appear in production builds.
 *
 *         Format:
 *           - Arguments must start with a message string, followed by zero or more key-value pairs.
 *           - Keys must be camelCase strings, but values can be anything that a stream can output
 *             using the << operator.
 *           - To format an integral type in hexadecimal, use TSB_LOG_AS_HEX().
 *           - There is no fixed limit to the number of key-value pairs, but be mindful of client
 *             log line limits.
 *
 *         Example:
 *             TSB_LOG_TRACE(mLogger, "A meaningful trace message",
 *                 "myFirstKey", first_value, "mySecondKey", TSB_LOG_AS_HEX(second_value));
 *
 *  @param[in] logger - client logger instance where the message should be sent.
 *  @param[in] ... - message followed by zero or more key-value pairs.
 */
#define TSB_LOG_TRACE(logger, ...) TSB_LOG(logger, TSB::LogLevel::TRACE, __VA_ARGS__)

/**
 *  @fn TSB_LOG_AS_HEX
 *  @brief Converts an integral value to a type that will be logged in hexadecimal format.
 *
 *         Example:
 *             TSB_LOG_TRACE("My message", "myKey", TSB_LOG_AS_HEX(integral_value));
 *
 *  @param[in] integralValue - integral value to convert
 */
#define TSB_LOG_AS_HEX(integralValue)                                                              \
	(TSB::Log::AsBase<decltype(integralValue)>(std::hex, integralValue))

/**
 *  @fn TSB_LOG
 *
 *  @brief Main macro for logging output, used to verify the log message format
 *         and capture the standard predefined macros for file, function and line,
 *         before sending these on to be logged.
 *
 *         Minimal processing should be done here aside from compile-time validation.
 *
 *  @param[in] logger - Client's logger to send the message to, if the minimum level is met.
 *  @param[in] logLevel - Log level (type) of the log message
 *  @param[in] __VA_ARGS__ - Variadic macro arguments containing the log message itself
 */
#define TSB_LOG(logger, logLevel, ...)                                                             \
	do                                                                                             \
	{                                                                                              \
		static_assert(TSB::Log::MessageValid(#__VA_ARGS__), "Message must be a string literal");   \
		static_assert(TSB::Log::KeysValid(#__VA_ARGS__), "Message keys must be camelCase");        \
                                                                                                   \
		if (EMIT_TSB_LOGS && (logLevel >= (logger).minLevel))                                      \
		{                                                                                          \
			constexpr auto srcFileName_ = TSB::Log::FileName(static_cast<const char*>(__FILE__) + sizeof(__FILE__) - 2);     \
			static_assert(srcFileName_[0] != '\0', "File name not evaluated at compile time");     \
                                                                                                   \
			/* Create the message and send to the client's registered logger */                    \
			(logger).func(                                                                         \
				TSB::Log::MakeMessage(logLevel, __func__, srcFileName_, __LINE__, __VA_ARGS__));   \
		}                                                                                          \
	} while (0)

namespace TSB
{

/**
 * @brief Struct that groups together client-specific logging information
 */
struct Logger
{
	LogFunction func;
	LogLevel minLevel;
};

namespace Log
{

/**
 * @brief Returns whether the given character is upper case
 */
constexpr bool IsUpper(char c)
{
	return (c >= 'A' && c <= 'Z');
}

/**
 * @brief Returns whether the given character is lower case
 */
constexpr bool IsLower(char c)
{
	return (c >= 'a' && c <= 'z');
}

/**
 *  @fn IsCamelCase
 *
 *  @brief Checks that the given key is camelCase.
 *         Only alphabetical characters are supported; number digits in keys
 *         are not currently supported.
 *
 *  @param[in] key - key string to check
 *
 *  @retval true if key is camelCase; false otherwise
 */
constexpr bool IsCamelCase(const std::string_view& key)
{
	bool newWord = true;

	for (char c : key)
	{
		if (newWord)
		{
			// Very first or second character of new word must be lower-case
			if (!IsLower(c))
				return false;
			newWord = false;
		}
		else if (IsUpper(c))
		{
			// Upper-case character indicates start of new word
			newWord = true;
		}
		else if (IsLower(c))
		{
			// Word continues with lower-case character
		}
		else
		{
			// Not an alphabetical character
			return false;
		}
	}

	return true;
}

/**
 *  @fn MessageValid
 *
 *  @brief Checks that the message string in a log message is valid.
 *         The message must consist of a single non-empty string literal.
 *         The string literal must not include escaped double quotes;
 *         single quotes are valid.
 *
 *  @param[in] fullMsg - Full log message to check
 *
 *  @retval true if message is valid; false otherwise
 */
constexpr bool MessageValid(const std::string_view& fullMsg)
{
	// Escaped double quotes anywhere in the message are not supported
	// Single quotes should be used instead
	if (fullMsg.find("\\\"") != std::string_view::npos)
		return false;

	// Message does not start with string literal
	if (fullMsg[0] != '\"')
		return false;

	std::size_t startMsg = 1;
	std::size_t endQuote = fullMsg.find('\"', startMsg);

	// Check for empty string message ""
	if (endQuote == startMsg)
		return false;

	// Message does not have any key-value pairs after it (OK)
	if (endQuote == fullMsg.length() - 1)
		return true;

	// Message does not end with string literal
	if (fullMsg[endQuote + 1] != ',')
		return false;

	return true;
}

/**
 *  @fn KeysValid
 *
 *  @brief Checks that keys in a log message are valid
 *         Keys must be non-empty and use camelCase.
 *
 *         Note that keys that are variables instead of string literals will be ignored.
 *         There is no run-time camelCase check, as printing a malformed key is better
 *         than throwing an exception when it comes to logging.
 *
 *         A side-effect of this check is that any string literals included directly in the value
 *         must also be camelCase - e.g. LOG("msg, "key", value ? "true" : "false").
 *
 *  @param[in] fullMsg - Full log message to check
 *
 *  @retval true if keys are valid; false otherwise
 */
constexpr bool KeysValid(const std::string_view& fullMsg)
{
	// Skip the free-form message at the start
	std::size_t startQuote = fullMsg.find('\"');
	std::size_t endQuote = fullMsg.find('\"', startQuote + 1);

	// Search the message for keys
	while ((startQuote = fullMsg.find('\"', endQuote + 1)) != std::string_view::npos)
	{
		std::size_t startKey = startQuote + 1;
		endQuote = fullMsg.find('\"', startKey);

		// Check for empty string key ""
		if (startKey == endQuote)
			return false;

		std::size_t keyLen = endQuote - startKey;
		std::string_view key = fullMsg.substr(startKey, keyLen);

		// Check for non-camelCase key
		if (!IsCamelCase(key))
			return false;
	}

	// Either the keys found were camelCase, or there were no keys
	return true;
}

/**
 *  @fn FileName
 *
 *  @brief Extracts a file name from the end of an absolute path
 *
 *  @param[in] pathEnd - pointer to the final character in the path before the null-terminator
 *
 *  @retval A pointer to the file name, or null if there was no file name (path ended with a /)
 */
constexpr const char* FileName(const char* pathEnd)
{
	return *pathEnd == '/' ? (pathEnd + 1) : FileName(pathEnd - 1);
}

/**
 * @brief Base-aware type to facilitate printing of integer values in alternative bases to decimal
 */
template <typename T> struct AsBase
{
	AsBase(std::ios_base& (*base)(std::ios_base&), T value) : mBase(base), mValue(value)
	{
		static_assert(std::is_integral<T>::value, "Value type must be integral");
	}

	std::ios_base& (*const mBase)(std::ios_base&);
	const T mValue;
};

/**
 *  @fn operator<<
 *
 *  @brief Writes a base-formatted integer to the given output stream
 *         using the correct base prefix - e.g. "0x" for hexadecimal.
 *
 *  @param[in] logStream - Stream to write the base-formatted integer to
 *  @param[in] logAs - integer converted to base-aware AsBase type
 *
 *  @return The output stream
 */
template <typename T> std::ostream& operator<<(std::ostream& logStream, const AsBase<T>& logAs)
{
	// Chars can't be output as hex, so cast value to the widest integral type
	return logStream << logAs.mBase << std::showbase << static_cast<uintmax_t>(logAs.mValue)
					 << std::noshowbase << std::dec;
}

/**
 *  @fn operator<<
 *
 *  @brief Custom path output operator that doesn't output the path in double quotes,
 *         since the logging implementation adds quotes anyway.
 *
 *  @param[in] logStream - Stream to write the path to
 *  @param[in] path - filesystem path
 *
 *  @return The output stream
 */
std::ostream& operator<<(std::ostream& logStream, const std::filesystem::path& path);

/**
 *  @fn FormatMetadata
 *
 *  @brief Formats the metadata associated with a log message and adds it to a string stream.
 *
 *         The implementation of this function may add extra metadata beyond the
 *         given parameters, for example thread or process ID.
 *
 *  @param[in] logStream - String stream to add the metadata to
 *  @param[in] level - Log level (type)
 *  @param[in] func - Function emitting the log message
 *  @param[in] file - Name of source file emitting the log message
 *  @param[in] line - Line number of log message in source file
 */
void FormatMetadata(std::ostringstream& logStream, LogLevel level, const char* func,
					const char* file, const int line);

/**
 *  @fn FormatMessage
 *
 *  @brief Formats the log message itself, as a sequence of key-value pairs, and adds it to a
 *         string stream using a recursive variadic template.
 *
 *         Values can be anything that can be converted to a string with the output operator (<<).
 *         A compile-time error will result if an operator is not defined for the value's type.
 *
 *         Values are delimited by double quotes, which makes them easier to parse if they happen
 *         to contain spaces.
 *
 *  @param[in] logStream - String stream to add the message to
 *  @param[in] key - key string to log
 *  @param[in] value - value to log
 *  @param[in] args - parameter pack of key-value pairs
 */
template <typename V>
void FormatMessage(std::ostringstream& logStream, const std::string_view& key, const V& value)
{
	logStream << " " << key << "=\"" << value << "\"";
}

template <typename V, typename... Args>
void FormatMessage(std::ostringstream& logStream, const std::string_view& key, const V& value,
				   Args&&... args)
{
	logStream << " " << key << "=\"" << value << "\"";
	FormatMessage(logStream, std::forward<Args>(args)...);
}

/**
 *  @fn MakeMessage
 *
 *  @brief Creates the full log message string ready for sending to the client for output.
 *
 *  @param[in] logLevel - Log level (type)
 *  @param[in] func - Function emitting the log message
 *  @param[in] fileName - Name of source file emitting the log message
 *  @param[in] line - Line number of log message in source file
 *  @param[in] args - parameter pack of key-value pairs
 *
 *  @return The full log message string
 */
template <typename... Args>
std::string MakeMessage(LogLevel logLevel, const char* func, const char* fileName, const int line,
						Args&&... args)
{
	std::ostringstream logStream;
	FormatMetadata(logStream, logLevel, func, fileName, line);
	FormatMessage(logStream, "msg", std::forward<Args>(args)...);
	return logStream.str();
}

} // namespace Log

} // namespace TSB

#endif // __TSB_LOG__
