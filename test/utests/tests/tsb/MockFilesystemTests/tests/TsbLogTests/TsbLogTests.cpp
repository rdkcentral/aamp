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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <thread>

#include "TsbLog.h"

class TsbLogTests : public ::testing::Test
{
};

TEST_F(TsbLogTests, ValidMessage)
{
	ASSERT_TRUE(TSB::Log::MessageValid("\"Valid message\""));
	ASSERT_TRUE(TSB::Log::MessageValid("\"Valid message with data\", \"keyOne\", valueOne"));
	ASSERT_TRUE(TSB::Log::MessageValid("\"Contains 'single' quotes\""));
}

TEST_F(TsbLogTests, InvalidMessage)
{
	ASSERT_FALSE(TSB::Log::MessageValid("\"\""));
	ASSERT_FALSE(TSB::Log::MessageValid("\"Contains \\\"double\\\" quotes"));
	// Check that the user isn't trying to print out variables in the message itself,
	// which can be quite easy to do, especially if the variable converts to a string e.g.:
	// LOG("msg" + someVariable) or LOG(someVariable + "msg")
	ASSERT_FALSE(TSB::Log::MessageValid("\"Message with variable at end - \" + someVariable"));
	ASSERT_FALSE(TSB::Log::MessageValid("someVariable + \" - Message with variable at start\""));
}

TEST_F(TsbLogTests, ValidKeys)
{
	ASSERT_TRUE(TSB::Log::KeysValid("\"One key\", \"keyOne\", valueOne"));
	ASSERT_TRUE(TSB::Log::KeysValid("\"Two keys\", \"keyOne\", valueOne, \"keyTwo\", valueTwo"));
	ASSERT_TRUE(TSB::Log::KeysValid("\"All lower case\", \"lower\", value"));
	ASSERT_TRUE(TSB::Log::KeysValid("\"Ends with capital\", \"lowerC\", value"));
	ASSERT_TRUE(TSB::Log::KeysValid("\"Longer key\", \"thisCamelCaseKeyHasLotsOfHumps\", value"));
}

TEST_F(TsbLogTests, InvalidKeys)
{
	ASSERT_FALSE(TSB::Log::KeysValid("\"Empty key\", \"\", valueOne"));
	ASSERT_FALSE(TSB::Log::KeysValid("\"Starts with capital\", \"KeyOne\", valueOne"));
	ASSERT_FALSE(TSB::Log::KeysValid("\"Two consecutive capitals\", \"keYOne\", valueOne"));
	ASSERT_FALSE(TSB::Log::KeysValid("\"All capitals\", \"KEYONE\", valueOne"));
	ASSERT_FALSE(TSB::Log::KeysValid("\"Snake case\", \"key_one\", valueOne"));
	ASSERT_FALSE(TSB::Log::KeysValid("\"Numeric digit\", \"key1\", valueOne"));
}

TEST_F(TsbLogTests, KeyValidationSideEffects)
{
	// String literals embedded in values will be treated as if they are keys
	// - i.e. the camelCase check will apply.
	ASSERT_TRUE(TSB::Log::KeysValid(
		"\"Value with string literal\", \"keyOne\", value ? \"true\" : \"false\""));
	ASSERT_FALSE(TSB::Log::KeysValid(
		"\"Value with string literal\", \"keyOne\", value ? \"TRUE\" : \"FALSE\""));

	// The implementation is designed to carry out compile-time checks only - if the key is
	// itself a variable, then the check will pass.
	ASSERT_TRUE(TSB::Log::KeysValid("\"Key is itself a variable\", keyOne, valueOne"));
}

TEST_F(TsbLogTests, ValidFileName)
{
	ASSERT_STREQ(TSB::Log::FileName(static_cast<const char*>(__FILE__) + sizeof(__FILE__) - 2), "TsbLogTests.cpp");
}

TEST_F(TsbLogTests, MakeMessageSuccessTrace)
{
	std::ostringstream expectedStream;
	expectedStream << "[TSB][TRACE]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\"";

	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message"),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessWarn)
{
	std::ostringstream expectedStream;
	expectedStream << "[TSB][WARN]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\"";

	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::WARN, "Func", "File.cpp", 1, "Valid message"),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessError)
{
	std::ostringstream expectedStream;
	expectedStream << "[TSB][ERROR]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\"";

	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::ERROR, "Func", "File.cpp", 1, "Valid message"),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessMil)
{
	std::ostringstream expectedStream;
	expectedStream << "[TSB][MIL]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\"";

	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::MIL, "Func", "File.cpp", 1, "Valid message"),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessMultipleKeys)
{
	std::ostringstream expectedStream;
	expectedStream << "[TSB][TRACE]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\"";

	// Add a key
	int valueOne = 1;
	expectedStream << " keyOne=\"" << valueOne << "\"";
	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message",
									"keyOne", valueOne),
			  expectedStream.str());

	// Add a second key
	int valueTwo = 2;
	expectedStream << " keyTwo=\"" << valueTwo << "\"";
	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message",
									"keyOne", valueOne, "keyTwo", valueTwo),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessVariableKey)
{
	std::string keyVariable{"keyVariable"};
	int valueVariable = 1;
	std::ostringstream expectedStream;
	expectedStream << "[TSB][TRACE]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\""
				   << " keyVariable=\"" << valueVariable << "\"";

	// Add a key that is itself a variable
	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message",
									keyVariable, valueVariable),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessHexadecimalValue)
{
	uint8_t valueOneHex = UINT8_MAX;
	std::ostringstream expectedStream;
	expectedStream << "[TSB][TRACE]"
				   << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\""
				   << " keyOneHex=\"0xff\"";

	// Add a key with a variable to be logged as a hexadecimal
	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message",
									"keyOneHex", TSB_LOG_AS_HEX(valueOneHex)),
			  expectedStream.str());

	// Add a second key to check that the stream reverts to decimal following the hex value
	int valueTwo = 2;
	expectedStream << " keyTwo=\"" << valueTwo << "\"";
	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message",
									"keyOneHex", TSB_LOG_AS_HEX(valueOneHex), "keyTwo", valueTwo),
			  expectedStream.str());
}

TEST_F(TsbLogTests, MakeMessageSuccessFilesystemPath)
{
	std::ostringstream expectedStream;
	expectedStream << "[TSB][TRACE]" << "[" << std::this_thread::get_id() << "]"
				   << "[Func][File.cpp:1] msg=\"Valid message\""
				   << " keyOne=\"/tmp/path/file.tmp\"";

	// Add a key with a variable that is a filesystem path.
	// The path should be output with only one set of double quotes.
	ASSERT_EQ(TSB::Log::MakeMessage(TSB::LogLevel::TRACE, "Func", "File.cpp", 1, "Valid message",
									"keyOne", std::filesystem::path("/tmp/path/file.tmp")),
			  expectedStream.str());
}
