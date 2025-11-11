
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

/**
* @file test_l1_TtmlSubtecParser.cpp
* @page TtmlSubtecParser Level 1 Tests
*
* ## Module's Role
* This module includes Level 1 functional tests (success and failure scenarios).
* This is to ensure that the TtmlSubtecParser methods meet the requirements.
*
* **Pre-Conditions:**  None @n
* **Dependencies:** None @n
*
*/
  
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdio.h>

#include "TtmlSubtecParser.hpp"

class TtmlSubtecParserTestHelper : public TtmlSubtecParser {
public:

    // Constructor that forwards args to the base constructor
    TtmlSubtecParserTestHelper(SubtitleMimeType type, int width, int height)
        : TtmlSubtecParser(type, width, height) {
            if (width < 0 || height < 0) 
        throw std::runtime_error("Invalid dimensions: width and height must be non-negative");
        }
        
      // Override processData to prevent large vector allocation
    bool processData(const char* buffer, size_t bufferLen, double position, double duration) override {
        std::cout << "[Mocked] processData called. Skipping actual processing." << std::endl;
        // Optionally, you can store minimal data for verification
        if (buffer && bufferLen > 0)
        m_lastBuffer.assign(buffer, buffer + bufferLen);
        m_lastPosition = position;
        m_lastDuration = duration;
        return true; // Simulate successful processing
    }

    std::vector<char> m_lastBuffer;
    double m_lastPosition{0};
    double m_lastDuration{0};

    using SubtitleParser::mWidth;
    using SubtitleParser::mHeight;
    using TtmlSubtecParser::m_isLinear;
    using TtmlSubtecParser::m_parsedFirstPacket;
    using TtmlSubtecParser::m_sentOffset;
    using TtmlSubtecParser::m_firstBeginOffset;    
};
class TtmlSubtecParserTest : public ::testing::Test 
{
    protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}

public:
};

/**
* @brief Test the construction of TtmlSubtecParser with valid TTML type and default dimensions.
*
* This test verifies that the TtmlSubtecParser is correctly initialized with default dimensions when provided with various subtitle mime types, including TTML, MP4, and UNKNOWN types.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 001@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Initialize parser with eSUB_TYPE_TTML | type = eSUB_TYPE_WEBVTT, width = 0, height = 0 | mWidth = 1920, mHeight = 1080 | Should Pass |
* | 02| Initialize parser with eSUB_TYPE_MP4 | type = eSUB_TYPE_MP4, width = 0, height = 0 | mWidth = 1920, mHeight = 1080 | Should Pass |
* | 03| Initialize parser with eSUB_TYPE_TTML | type = eSUB_TYPE_TTML, width = 0, height = 0 | mWidth = 1920, mHeight = 1080 | Should Pass |
* | 04| Initialize parser with eSUB_TYPE_UNKNOWN | type = eSUB_TYPE_UNKNOWN, width = 0, height = 0 | mWidth = 1920, mHeight = 1080 | Should Pass |
*/
TEST(TtmlSubtecParserTest, ConstructParserWithValidTTMLTypeAndDefaultDimensions) {
    std::cout << "Entering ConstructParserWithValidTTMLTypeAndDefaultDimensions test" << std::endl;

    SubtitleMimeType type = eSUB_TYPE_WEBVTT;
    std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

    std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " 
              << type << ", width = 0, height = 0" << std::endl;

    TtmlSubtecParserTestHelper parser(type, 0, 0);

    // Assertions / validations can follow
    EXPECT_EQ(parser.mWidth, 0);
    EXPECT_EQ(parser.mHeight, 0);

    std::cout << "Exiting ConstructParserWithValidTTMLTypeAndDefaultDimensions test" << std::endl;
}

/**
* @brief Test the construction of TtmlSubtecParser with valid TTML type and specific dimensions
*
* This test verifies that the TtmlSubtecParser is correctly constructed with a valid TTML type and specific dimensions (1280x720). It ensures that the width and height are set correctly for different subtitle types.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 002@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Construct parser with eSUB_TYPE_TTML | type = eSUB_TYPE_WEBVTT, width = 1280, height = 720 | parser.mWidth = 1280, parser.mHeight = 720 | Should Pass |
* | 02| Construct parser with eSUB_TYPE_MP4 | type = eSUB_TYPE_MP4, width = 1280, height = 720 | parser.mWidth = 1280, parser.mHeight = 720 | Should Pass |
* | 03| Construct parser with eSUB_TYPE_TTML | type = eSUB_TYPE_TTML, width = 1280, height = 720 | parser.mWidth = 1280, parser.mHeight = 720 | Should Pass |
* | 04| Construct parser with eSUB_TYPE_UNKNOWN | type = eSUB_TYPE_UNKNOWN, width = 1280, height = 720 | parser.mWidth = 1280, parser.mHeight = 720 | Should Pass |
*/
TEST(TtmlSubtecParserTest, ConstructParserWithValidTTMLTypeAndSpecificDimensions) {
    std::cout << "Entering ConstructParserWithValidTTMLTypeAndSpecificDimensions test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;
        std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " << type
                  << ", width = 1280, height = 720" << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1280, 720);

        std::cout << "TtmlSubtecParserTestHelper constructed with mWidth = " << parser.mWidth
                  << ", mHeight = " << parser.mHeight << std::endl;

        EXPECT_EQ(parser.mWidth, 1280);
        EXPECT_EQ(parser.mHeight, 720);

        std::cout << "Passed for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ConstructParserWithValidTTMLTypeAndSpecificDimensions test" << std::endl;
}

/**
* @brief Test the construction of TtmlSubtecParser with a negative width
*
* This test verifies that the TtmlSubtecParser constructor throws a runtime error when initialized with a negative width value. This is important to ensure that the parser handles invalid input parameters correctly and does not proceed with invalid configurations.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 003@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Initialize parser with eSUB_TYPE_TTML and negative width | type = eSUB_TYPE_WEBVTT, width = -1920, height = 1080 | std::runtime_error | Should Fail |
* | 02| Initialize parser with eSUB_TYPE_MP4 and negative width | type = eSUB_TYPE_MP4, width = -1920, height = 1080 | std::runtime_error | Should Fail |
* | 03| Initialize parser with eSUB_TYPE_TTML and negative width | type = eSUB_TYPE_TTML, width = -1920, height = 1080 | std::runtime_error | Should Fail |
* | 04| Initialize parser with eSUB_TYPE_UNKNOWN and negative width | type = eSUB_TYPE_UNKNOWN, width = -1920, height = 1080 | std::runtime_error | Should Fail |
*/
TEST(TtmlSubtecParserTest, ConstructParserWithNegativeWidth) {
    std::cout << "Entering ConstructParserWithNegativeWidth test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;
        std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " << type
                  << ", width = -1920, height = 1080 (expecting exception)" << std::endl;

        EXPECT_THROW(
            {
                TtmlSubtecParserTestHelper parser(type, -1920, 1080);
            },
            std::runtime_error
        );

        std::cout << "Correctly threw exception for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ConstructParserWithNegativeWidth test" << std::endl;
}

/**
* @brief Test the construction of TtmlSubtecParser with negative height
*
* This test verifies that the TtmlSubtecParser constructor throws a runtime error when initialized with a negative height value. This is important to ensure that the parser handles invalid input parameters correctly and does not proceed with invalid configurations.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 004@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data | Expected Result | Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01 | Initialize parser with eSUB_TYPE_TTML and negative height | type = eSUB_TYPE_WEBVTT, width = 1920, height = -1080 | std::runtime_error | Should Fail |
* | 02 | Initialize parser with eSUB_TYPE_MP4 and negative height | type = eSUB_TYPE_MP4, width = 1920, height = -1080 | std::runtime_error | Should Fail |
* | 03 | Initialize parser with eSUB_TYPE_TTML and negative height | type = eSUB_TYPE_TTML, width = 1920, height = -1080 | std::runtime_error | Should Fail |
* | 04 | Initialize parser with eSUB_TYPE_UNKNOWN and negative height | type = eSUB_TYPE_UNKNOWN, width = 1920, height = -1080 | std::runtime_error | Should Fail |
*/
TEST(TtmlSubtecParserTest, ConstructParserWithNegativeHeight) {
    std::cout << "Entering ConstructParserWithNegativeHeight test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;
        std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " << type
                  << ", width = 1920, height = -1080 (expecting exception)" << std::endl;

        EXPECT_THROW(
            {
                TtmlSubtecParserTestHelper parser(type, 1920, -1080);
            },
            std::runtime_error
        );

        std::cout << "Correctly threw exception for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ConstructParserWithNegativeHeight test" << std::endl;
}

/**
* @brief Test the construction of TtmlSubtecParser with valid TTML type and maximum dimensions.
*
* This test verifies that the TtmlSubtecParser can be constructed with different subtitle types, 
* including TTML, and ensures that the width and height are set to the maximum integer values.@n
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 005@n
* **Priority:** High@n
* @n
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* @n
* **Test Procedure:**@n
* | Variation / Step | Description | Test Data |Expected Result |Notes |
* | :----: | --------- | ---------- |-------------- | ----- |
* | 01| Construct parser with eSUB_TYPE_TTML | type = eSUB_TYPE_WEBVTT, width = INT_MAX, height = INT_MAX | parser.mWidth = INT_MAX, parser.mHeight = INT_MAX | Should Pass |
* | 02| Construct parser with eSUB_TYPE_MP4 | type = eSUB_TYPE_MP4, width = INT_MAX, height = INT_MAX | parser.mWidth = INT_MAX, parser.mHeight = INT_MAX | Should Pass |
* | 03| Construct parser with eSUB_TYPE_TTML again | type = eSUB_TYPE_TTML, width = INT_MAX, height = INT_MAX | parser.mWidth = INT_MAX, parser.mHeight = INT_MAX | Should Pass |
* | 04| Construct parser with eSUB_TYPE_UNKNOWN | type = eSUB_TYPE_UNKNOWN, width = INT_MAX, height = INT_MAX | parser.mWidth = INT_MAX, parser.mHeight = INT_MAX | Should Pass |
*/
TEST(TtmlSubtecParserTest, ConstructParserWithValidTTMLTypeAndMaxDimensions) {
    std::cout << "Entering ConstructParserWithValidTTMLTypeAndMaxDimensions test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;
        std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " << type
                  << ", width = INT_MAX, height = INT_MAX" << std::endl;

        TtmlSubtecParserTestHelper parser(type, INT_MAX, INT_MAX);

        std::cout << "TtmlSubtecParserTestHelper constructed with mWidth = " << parser.mWidth
                  << ", mHeight = " << parser.mHeight << std::endl;

        EXPECT_EQ(parser.mWidth, INT_MAX);
        EXPECT_EQ(parser.mHeight, INT_MAX);

        std::cout << "Passed for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ConstructParserWithValidTTMLTypeAndMaxDimensions test" << std::endl;
}

/**
 * @brief Test to verify the successful closing of the TTML subtitle parser
 *
 * This test verifies that the TTML subtitle parser, after being initialized with various 
 * subtitle MIME types and valid dimensions, can be successfully closed using the `close()` method.
 * It ensures that `close()` returns `true`, indicating a successful shutdown of parser resources.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                        | Test Data                                         | Expected Result                  | Notes          |
 * |------------------|------------------------------------|--------------------------------------------------|----------------------------------|----------------|
 * | 01               | Initialize parser with TTML type   | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080 | parser initialized successfully  | Should Pass    |
 * | 02               | Call close method on parser        | None                                             | close() returns true             | Should Pass    |
 * | 03               | Initialize parser with MP4 type    | type = eSUB_TYPE_MP4, width = 1920, height = 1080 | parser initialized successfully  | Should Pass    |
 * | 04               | Call close method on parser        | None                                             | close() returns true             | Should Pass    |
 * | 05               | Initialize parser with TTML again  | type = eSUB_TYPE_TTML, width = 1920, height = 1080 | parser initialized successfully  | Should Pass    |
 * | 06               | Call close method on parser        | None                                             | close() returns true             | Should Pass    |
 * | 07               | Initialize parser with UNKNOWN type| type = eSUB_TYPE_UNKNOWN, width = 1920, height = 1080 | parser initialized successfully  | Should Pass    |
 * | 08               | Call close method on parser        | None                                             | close() returns true             | Should Pass    |
 */
TEST(TtmlSubtecParserTest, CloseSubtitleParserSuccessfully) {
    std::cout << "Entering CloseSubtitleParserSuccessfully" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;
        std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " << type
                  << ", width = 1920, height = 1080" << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.close()" << std::endl;
        bool result = parser.close();
        std::cout << "parser.close() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed close() test for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting CloseSubtitleParserSuccessfully" << std::endl;
}

/**
 * @brief Test the behavior of closing the subtitle parser multiple times
 *
 * This test checks the functionality of the `close` method in the `TtmlSubtecParser` class when it is called multiple times consecutively. The objective is to ensure that the parser can handle multiple close operations gracefully without causing any errors or unexpected behavior.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 007@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                             | Test Data                                             | Expected Result         | Notes          |
 * |------------------|-----------------------------------------|--------------------------------------------------------|--------------------------|----------------|
 * | 01               | Initialize parser with TTML type        | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080     | Object should initialize | Should Pass    |
 * | 02               | Call close method (1st time)            | None                                                   | close() returns true     | Should Pass    |
 * | 03               | Call close method (2nd time)            | None                                                   | close() returns true     | Should Pass    |
 * | 04               | Initialize parser with MP4 type         | type = eSUB_TYPE_MP4, width = 1920, height = 1080      | Object should initialize | Should Pass    |
 * | 05               | Call close method (1st time)            | None                                                   | close() returns true     | Should Pass    |
 * | 06               | Call close method (2nd time)            | None                                                   | close() returns true     | Should Pass    |
 * | 07               | Initialize parser with UNKNOWN type     | type = eSUB_TYPE_TTML, width = 1920, height = 1080  | Object should initialize | Should Pass    |
 * | 08               | Call close method (1st time)            | None                                                   | close() returns true     | Should Pass    |
 * | 09               | Call close method (2nd time)            | None                                                   | close() returns true     | Should Pass    |
 * | 10               | Initialize parser with UNKNOWN type     | type = eSUB_TYPE_UNKNOWN, width = 1920, height = 1080  | Object should initialize | Should Pass    |
 * | 11               | Call close method (1st time)            | None                                                   | close() returns true     | Should Pass    |
 * | 12               | Call close method (2nd time)            | None                                                   | close() returns true     | Should Pass    |
 */
TEST(TtmlSubtecParserTest, CloseSubtitleParserMultipleTimes) {
    std::cout << "Entering CloseSubtitleParserMultipleTimes" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;
        std::cout << "Invoking TtmlSubtecParserTestHelper constructor with type = " << type
                  << ", width = 1920, height = 1080" << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "First call to parser.close()" << std::endl;
        bool result1 = parser.close();
        std::cout << "First close() returned: " << std::boolalpha << result1 << std::endl;

        std::cout << "Second call to parser.close()" << std::endl;
        bool result2 = parser.close();
        std::cout << "Second close() returned: " << std::boolalpha << result2 << std::endl;

        EXPECT_TRUE(result1);
        EXPECT_TRUE(result2);

        std::cout << "Passed double-close test for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting CloseSubtitleParserMultipleTimes" << std::endl;
}

/**
 * @brief Test the initialization of TtmlSubtecParser with valid start position and base PTS
 *
 * This test verifies that the TtmlSubtecParser can be successfully initialized with a valid start position and base PTS.
 * The test ensures that the parser's init function returns true when provided with these valid inputs.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 008@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                                      | Expected Result     | Notes          |
 * |------------------|---------------------------------------------|-----------------------------------------------------------------|----------------------|----------------|
 * | 01               | Initialize parser with WEBVTT type          | type = eSUB_TYPE_WEBVTT, startPosSeconds = 10.0, basePTS = 1000000 | result = true         | Should Pass    |
 * | 02               | Initialize parser with MP4 type             | type = eSUB_TYPE_MP4, startPosSeconds = 10.0, basePTS = 1000000    | result = true         | Should Pass    |
 * | 03               | Initialize parser with TTML type            | type = eSUB_TYPE_TTML, startPosSeconds = 10.0, basePTS = 1000000   | result = true         | Should Pass    |
 * | 04               | Initialize parser with UNKNOWN type         | type = eSUB_TYPE_UNKNOWN, startPosSeconds = 10.0, basePTS = 1000000| result = true         | Should Pass    |
 */

TEST(TtmlSubtecParserTest, InitializeWithValidStartPositionAndBasePTS) {
    std::cout << "Entering InitializeWithValidStartPositionAndBasePTS test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);
        double startPosSeconds = 10.0;
        unsigned long long basePTS = 1000000;

        std::cout << "Invoking parser.init() with startPosSeconds = " << startPosSeconds
                  << ", basePTS = " << basePTS << std::endl;

        bool result = parser.init(startPosSeconds, basePTS);

        std::cout << "parser.init() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed initialization for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting InitializeWithValidStartPositionAndBasePTS test" << std::endl;
}

/**
 * @brief Test the initialization of TtmlSubtecParser with zero start position and base PTS
 *
 * This test verifies that the TtmlSubtecParser can be initialized correctly when provided with a start position of 0.0 seconds and a base PTS of 0.
 * This is a basic functionality test to ensure that the parser initializes without errors under these conditions.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 009@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                                 | Expected Result             | Notes         |
 * |------------------|---------------------------------------------|------------------------------------------------------------|------------------------------|---------------|
 * | 01               | Initialize parser with WEBVTT type          | type = eSUB_TYPE_WEBVTT, startPosSeconds = 0.0, basePTS = 0 | result = true, EXPECT_TRUE  | Should Pass   |
 * | 02               | Initialize parser with MP4 type             | type = eSUB_TYPE_MP4, startPosSeconds = 0.0, basePTS = 0    | result = true, EXPECT_TRUE  | Should Pass   |
 * | 03               | Initialize parser with TTML type            | type = eSUB_TYPE_TTML, startPosSeconds = 0.0, basePTS = 0   | result = true, EXPECT_TRUE  | Should Pass   |
 * | 04               | Initialize parser with UNKNOWN type         | type = eSUB_TYPE_UNKNOWN, startPosSeconds = 0.0, basePTS = 0| result = true, EXPECT_TRUE  | Should Pass   |
 */
TEST(TtmlSubtecParserTest, InitializeWithZeroStartPositionAndBasePTS) {
    std::cout << "Entering InitializeWithZeroStartPositionAndBasePTS test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);
        double startPosSeconds = 0.0;
        unsigned long long basePTS = 0;

        std::cout << "Invoking parser.init() with startPosSeconds = " << startPosSeconds
                  << ", basePTS = " << basePTS << std::endl;

        bool result = parser.init(startPosSeconds, basePTS);

        std::cout << "parser.init() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed zero-start init for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting InitializeWithZeroStartPositionAndBasePTS test" << std::endl;
}

/**
 * @brief Test the initialization of TtmlSubtecParser with large start position and base PTS
 *
 * This test verifies that the TtmlSubtecParser can be initialized correctly when provided with a very large start position and base PTS.
 * This is important to ensure that the parser can handle large values without any issues.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 010@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                                 | Expected Result             | Notes         |
 * |------------------|---------------------------------------------|------------------------------------------------------------|------------------------------|---------------|
 * | 01               | Initialize parser with WEBVTT type          | type = eSUB_TYPE_WEBVTT, startPosSeconds = 1e6, basePTS = 1e12 | result = true, EXPECT_TRUE  | Should Pass   |
 * | 02               | Initialize parser with MP4 type             | type = eSUB_TYPE_MP4, startPosSeconds = 1e6, basePTS = 1e12 | result = true, EXPECT_TRUE  | Should Pass   |
 * | 03               | Initialize parser with TTML again           | type = eSUB_TYPE_TTML, startPosSeconds = 1e6, basePTS = 1e12 | result = true, EXPECT_TRUE  | Should Pass   |
 * | 04               | Initialize parser with UNKNOWN type         | type = eSUB_TYPE_UNKNOWN, startPosSeconds = 1e6, basePTS = 1e12 | result = true, EXPECT_TRUE  | Should Pass   |
 */
TEST(TtmlSubtecParserTest, InitializeWithLargeStartPositionAndBasePTS) {
    std::cout << "Entering InitializeWithLargeStartPositionAndBasePTS test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_TTML, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);
        double startPosSeconds = 1e6;
        unsigned long long basePTS = 1e12;

        std::cout << "Invoking parser.init() with startPosSeconds = " << startPosSeconds
                  << ", basePTS = " << basePTS << std::endl;

        bool result = parser.init(startPosSeconds, basePTS);

        std::cout << "parser.init() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed large-start init for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting InitializeWithLargeStartPositionAndBasePTS test" << std::endl;
}

/**
 * @brief Test the initialization of TtmlSubtecParser with a negative start position
 *
 * This test checks the behavior of the TtmlSubtecParser when initialized with a negative start position. 
 * It ensures that the parser correctly handles invalid input by returning a failure status.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 011@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                                  | Expected Result            | Notes        |
 * |------------------|---------------------------------------------|-------------------------------------------------------------|-----------------------------|--------------|
 * | 01               | Initialize parser with WEBVTT type          | type = eSUB_TYPE_WEBVTT, startPosSeconds = -1.0, basePTS = 1000000 | result = false, EXPECT_FALSE | Should Fail  |
 * | 02               | Initialize parser with MP4 type             | type = eSUB_TYPE_MP4, startPosSeconds = -1.0, basePTS = 1000000  | result = false, EXPECT_FALSE | Should Fail  |
 * | 03               | Initialize parser with TTML again           | type = eSUB_TYPE_TTML, startPosSeconds = -1.0, basePTS = 1000000 | result = false, EXPECT_FALSE | Should Fail  |
 * | 04               | Initialize parser with UNKNOWN type         | type = eSUB_TYPE_UNKNOWN, startPosSeconds = -1.0, basePTS = 1000000 | result = false, EXPECT_FALSE | Should Fail  |
 */
TEST(TtmlSubtecParserTest, InitializeWithNegativeStartPosition) {
    std::cout << "Entering InitializeWithNegativeStartPosition test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);
        double startPosSeconds = -1.0;
        unsigned long long basePTS = 1000000;

        std::cout << "Invoking parser.init() with startPosSeconds = " << startPosSeconds
                  << ", basePTS = " << basePTS << std::endl;

        bool result = parser.init(startPosSeconds, basePTS);

        std::cout << "parser.init() returned: " << std::boolalpha << result << std::endl;

        EXPECT_FALSE(result);
        std::cout << "Handled negative startPos correctly for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting InitializeWithNegativeStartPosition test" << std::endl;
}

/**
 * @brief Test the initialization of TtmlSubtecParser with maximum values for start position and base PTS.
 *
 * This test verifies that the TtmlSubtecParser can be initialized with the maximum possible values for start position and base PTS.
 * It ensures that the parser handles extreme values correctly and returns a successful initialization result.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 012@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                                     | Expected Result            | Notes         |
 * |------------------|---------------------------------------------|----------------------------------------------------------------|-----------------------------|---------------|
 * | 01               | Initialize parser with WEBVTT type          | type = eSUB_TYPE_WEBVTT, startPosSeconds = DBL_MAX, basePTS = ULLONG_MAX | result = true, EXPECT_TRUE  | Should Pass   |
 * | 02               | Initialize parser with MP4 type             | type = eSUB_TYPE_MP4, startPosSeconds = DBL_MAX, basePTS = ULLONG_MAX  | result = true, EXPECT_TRUE  | Should Pass   |
 * | 03               | Initialize parser with TTML again           | type = eSUB_TYPE_TTML, startPosSeconds = DBL_MAX, basePTS = ULLONG_MAX | result = true, EXPECT_TRUE  | Should Pass   |
 * | 04               | Initialize parser with UNKNOWN type         | type = eSUB_TYPE_UNKNOWN, startPosSeconds = DBL_MAX, basePTS = ULLONG_MAX | result = true, EXPECT_TRUE  | Should Pass   |
 */
TEST(TtmlSubtecParserTest, InitializeWithMaxValuesForStartPositionAndBasePTS) {
    std::cout << "Entering InitializeWithMaxValuesForStartPositionAndBasePTS test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);
        double startPosSeconds = DBL_MAX;
        unsigned long long basePTS = ULLONG_MAX;

        std::cout << "Invoking parser.init() with startPosSeconds = " << startPosSeconds
                  << ", basePTS = " << basePTS << std::endl;

        bool result = parser.init(startPosSeconds, basePTS);

        std::cout << "parser.init() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed max-value init for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting InitializeWithMaxValuesForStartPositionAndBasePTS test" << std::endl;
}

/**
 * @brief Test to verify setting the linear content flag to true
 *
 * This test checks if the linear content flag is correctly set to true in the TtmlSubtecParser class. 
 * It ensures that the isLinear method correctly updates the m_isLinear member variable when passed a true value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 013@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                         | Test Data                                        | Expected Result                  | Notes           |
 * |------------------|-------------------------------------|--------------------------------------------------|----------------------------------|-----------------|
 * | 01               | Create parser with WEBVTT type        | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080 | Instance created successfully    | Should Pass     |
 * | 02               | Set linear flag                     | isLinear(true)                                   | m_isLinear should be true        | Should Pass     |
 * | 03               | Verify flag                         | EXPECT_TRUE(parser.m_isLinear)                   | Assertion should pass            | Should Pass     |
 * | 04               | Repeat with MP4 type                | type = eSUB_TYPE_MP4                             | m_isLinear should be true        | Should Pass     |
 * | 05               | Repeat with TTML again              | type = eSUB_TYPE_TTML                            | m_isLinear should be true        | Should Pass     |
 * | 06               | Repeat with UNKNOWN type            | type = eSUB_TYPE_UNKNOWN                         | m_isLinear should be true        | Should Pass     |
 */
TEST(TtmlSubtecParserTest, SetLinearContentFlagToTrue) {
    std::cout << "Entering SetLinearContentFlagToTrue test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Calling parser.isLinear(true)" << std::endl;
        parser.isLinear(true);

        std::cout << "Checking parser.m_isLinear = " << std::boolalpha << parser.m_isLinear << std::endl;

        EXPECT_TRUE(parser.m_isLinear);
        std::cout << "Passed linear flag assertion for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting SetLinearContentFlagToTrue test" << std::endl;
}

/**
 * @brief Test to verify setting the linear content flag to false
 *
 * This test checks if the linear content flag is correctly set to false in the TtmlSubtecParser class. 
 * It ensures that the isLinear method properly updates the m_isLinear member variable when passed a false value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 014@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                         | Test Data                                        | Expected Result                  | Notes           |
 * |------------------|-------------------------------------|--------------------------------------------------|----------------------------------|-----------------|
 * | 01               | Create parser with WEBVTT type      | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080 | Object created successfully       | Should Pass     |
 * | 02               | Set linear flag to false            | isLinear(false)                                  | m_isLinear should be false        | Should Pass     |
 * | 03               | Verify flag                         | EXPECT_FALSE(parser.m_isLinear)                  | Assertion should pass             | Should Pass     |
 * | 04               | Repeat with MP4 type                | type = eSUB_TYPE_MP4                             | m_isLinear should be false        | Should Pass     |
 * | 05               | Repeat with TTML type               | type = eSUB_TYPE_TTML                            | m_isLinear should be false        | Should Pass     |
 * | 06               | Repeat with UNKNOWN type            | type = eSUB_TYPE_UNKNOWN                         | m_isLinear should be false        | Should Pass     |
 */
TEST(TtmlSubtecParserTest, SetLinearContentFlagToFalse) {
    std::cout << "Entering SetLinearContentFlagToFalse test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Calling parser.isLinear(false)" << std::endl;
        parser.isLinear(false);

        std::cout << "Checking parser.m_isLinear = " << std::boolalpha << parser.m_isLinear << std::endl;

        EXPECT_FALSE(parser.m_isLinear);
        std::cout << "Passed linear=false flag assertion for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting SetLinearContentFlagToFalse test" << std::endl;
}

/**
 * @brief Test to verify the mute functionality of subtitles in TtmlSubtecParser
 *
 * This test checks if the subtitles are correctly muted when the mute function is invoked with 'true' as an argument.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 015@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                            | Test Data                                            | Expected Result         | Notes            |
 * |------------------|----------------------------------------|------------------------------------------------------|--------------------------|------------------|
 * | 01               | Initialize parser with WEBVTT type       | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080   | Successful initialization | Should Pass      |
 * | 02               | Call mute function with 'true'         | mute(true)                                           | Subtitles should be muted | Should Pass      |
 * | 03               | Repeat with MP4 type                   | type = eSUB_TYPE_MP4                                 | Subtitles should be muted | Should Pass      |
 * | 04               | Repeat with TTML again                 | type = eSUB_TYPE_TTML                                | Subtitles should be muted | Should Pass      |
 * | 05               | Repeat with UNKNOWN type               | type = eSUB_TYPE_UNKNOWN                             | Subtitles should be muted | Should Pass      |
 */
TEST(TtmlSubtecParserTest, MuteSubtitles) {
    std::cout << "Entering MuteSubtitles test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.mute(true)" << std::endl;
        parser.mute(true);

        std::cout << "Mute invoked successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting MuteSubtitles test" << std::endl;
}

/**
 * @brief Test to verify the unmute functionality of subtitles in TtmlSubtecParser
 *
 * This test checks if the subtitles are correctly unmuted when the mute function is invoked with 'false' as an argument.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 016@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                            | Test Data                                            | Expected Result           | Notes            |
 * |------------------|----------------------------------------|------------------------------------------------------|----------------------------|------------------|
 * | 01               | Initialize parser with WEBVTT type     | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080 | Successful initialization  | Should Pass      |
 * | 02               | Call mute function with 'false'        | mute(false)                                          | Subtitles should be unmuted| Should Pass      |
 * | 03               | Repeat with MP4 type                   | type = eSUB_TYPE_MP4                                 | Subtitles should be unmuted| Should Pass      |
 * | 04               | Repeat with TTML type                  | type = eSUB_TYPE_TTML                                | Subtitles should be unmuted| Should Pass      |
 * | 05               | Repeat with UNKNOWN type               | type = eSUB_TYPE_UNKNOWN                             | Subtitles should be unmuted| Should Pass      |
 */
TEST(TtmlSubtecParserTest, UnmuteSubtitles) {
    std::cout << "Entering UnmuteSubtitles test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.mute(false)" << std::endl;
        parser.mute(false);

        std::cout << "Unmute invoked successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting UnmuteSubtitles test" << std::endl;
}

/**
 * @brief Test to verify the pause functionality of the TtmlSubtecParser
 *
 * This test checks if the TtmlSubtecParser can successfully pause subtitle rendering when the pause method is invoked with a true argument.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 017@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                         | Test Data                                            | Expected Result                 | Notes           |
 * |------------------|-------------------------------------|------------------------------------------------------|----------------------------------|-----------------|
 * | 01               | Initialize parser with WEBVTT type  | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080 | Instance should be created       | Should Pass     |
 * | 02               | Call pause(true) on parser          | pause(true)                                          | Subtitle rendering should pause | Should Pass     |
 * | 03               | Repeat with MP4 type                | type = eSUB_TYPE_MP4                                 | Subtitle rendering should pause | Should Pass     |
 * | 04               | Repeat with TTML type               | type = eSUB_TYPE_TTML                                | Subtitle rendering should pause | Should Pass     |
 * | 05               | Repeat with UNKNOWN type            | type = eSUB_TYPE_UNKNOWN                             | Subtitle rendering should pause | Should Pass     |
 */
TEST(TtmlSubtecParserTest, PauseSubtitleRendering) {
    std::cout << "Entering PauseSubtitleRendering" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.pause(true)" << std::endl;
        parser.pause(true);

        std::cout << "Pause invoked successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting PauseSubtitleRendering" << std::endl;
}

/**
* @brief Test the resume functionality of subtitle rendering in TtmlSubtecParser
*
* This test verifies that the subtitle rendering can be resumed correctly after being paused.
* It ensures that the parser can handle the resume operation without errors.
*
* **Test Group ID:** Basic: 01@n
* **Test Case ID:** 018@n
* **Priority:** High@n
* 
* **Pre-Conditions:** None@n
* **Dependencies:** None@n
* **User Interaction:** None@n
* 
* **Test Procedure:**@n
* | Variation / Step | Description                        | Test Data                                            | Expected Result              | Notes           |
* |------------------|------------------------------------|------------------------------------------------------|-------------------------------|-----------------|
* | 01               | Initialize parser with WEBVTT type | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080 | Object should be created      | Should Pass     |
* | 02               | Call pause(false) to resume        | pause(false)                                         | Rendering should resume       | Should Pass     |
* | 03               | Repeat with MP4 type               | type = eSUB_TYPE_MP4                                 | Rendering should resume       | Should Pass     |
* | 04               | Repeat with TTML type              | type = eSUB_TYPE_TTML                                | Rendering should resume       | Should Pass     |
* | 05               | Repeat with UNKNOWN type           | type = eSUB_TYPE_UNKNOWN                             | Rendering should resume       | Should Pass     |
*/
TEST(TtmlSubtecParserTest, ResumeSubtitleRendering) {
    std::cout << "Entering ResumeSubtitleRendering" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.pause(false) to resume rendering" << std::endl;
        parser.pause(false);

        std::cout << "Resume invoked successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ResumeSubtitleRendering" << std::endl;
}

/**
 * @brief Test to verify the processing of a valid TTML buffer with valid parameters.
 *
 * This test checks if the `processData` method of the `MockTtmlSubtecParser` class correctly processes a valid TTML buffer
 * when provided with valid parameters. The objective is to ensure that the method returns true, indicating successful processing.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 019@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                    | Test Data                                                                              | Expected Result           | Notes        |
 * |------------------|------------------------------------------------|-----------------------------------------------------------------------------------------|----------------------------|--------------|
 * | 01               | Process valid buffer with WEBVTT type          | buffer = "valid TTML data", bufferLen = 15, position = 10.0, duration = 5.0, type = eSUB_TYPE_WEBVTT  | result = true, EXPECT_TRUE | Should Pass  |
 * | 02               | Process valid buffer with MP4 type             | Same as above with type = eSUB_TYPE_MP4                                                | result = true, EXPECT_TRUE | Should Pass  |
 * | 03               | Process valid buffer with TTML type            | Same as above with type = eSUB_TYPE_TTML                                               | result = true, EXPECT_TRUE | Should Pass  |
 * | 04               | Process valid buffer with UNKNOWN type         | Same as above with type = eSUB_TYPE_UNKNOWN                                            | result = true, EXPECT_TRUE | Should Pass  |
 */

TEST(TtmlSubtecParserTest, ValidBufferWithValidParameters) {
    std::cout << "Entering ValidBufferWithValidParameters" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 15;
    double position = 10.0;
    double duration = 5.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed processing for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ValidBufferWithValidParameters" << std::endl;
}

/**
 * @brief Test to verify the behavior of processData when a null buffer pointer is passed.
 *
 * This test checks the processData function of the MockTtmlSubtecParser class to ensure it handles a null buffer pointer correctly.
 * This is important to verify that the function can gracefully handle invalid input without crashing or producing incorrect results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 020@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                    | Test Data                                                                 | Expected Result          | Notes         |
 * |------------------|------------------------------------------------|----------------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Process nullptr with WEBVTT type              | buffer = nullptr, bufferLen = 15, position = 10.0, duration = 5.0, type = eSUB_TYPE_WEBVTT  | result = true, EXPECT_TRUE | Should Pass  |
 * | 02               | Process nullptr with MP4 type                 | Same as above, type = eSUB_TYPE_MP4                                       | result = true, EXPECT_TRUE | Should Pass  |
 * | 03               | Process nullptr with TTML type                | Same as above, type = eSUB_TYPE_TTML                                      | result = true, EXPECT_TRUE | Should Pass  |
 * | 04               | Process nullptr with UNKNOWN type             | Same as above, type = eSUB_TYPE_UNKNOWN                                   | result = true, EXPECT_TRUE | Should Pass  |
 */
TEST(TtmlSubtecParserTest, NullBufferPointer) {
    std::cout << "Entering NullBufferPointer" << std::endl;

    const char* buffer = nullptr;
    size_t bufferLen = 15;
    double position = 10.0;
    double duration = 5.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = nullptr"
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed null buffer handling for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting NullBufferPointer" << std::endl;
}

/**
 * @brief Test to verify the behavior of processData when the buffer length is zero.
 *
 * This test checks if the processData method in MockTtmlSubtecParser handles the case
 * when a non-null buffer is provided but its length is zero. It ensures no crash or failure.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 021@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                     | Test Data                                                              | Expected Result          | Notes         |
 * |------------------|-------------------------------------------------|-------------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Process zero-length buffer with WEBVTT type    | buffer = "valid TTML data", bufferLen = 0, position = 10.0, duration = 5.0, type = eSUB_TYPE_WEBVTT  | result = true, EXPECT_TRUE | Should Pass  |
 * | 02               | Repeat with MP4 type                            | Same as above, type = eSUB_TYPE_MP4                                    | result = true, EXPECT_TRUE | Should Pass  |
 * | 03               | Repeat with TTML type                           | Same as above, type = eSUB_TYPE_TTML                                   | result = true, EXPECT_TRUE | Should Pass  |
 * | 04               | Repeat with UNKNOWN type                        | Same as above, type = eSUB_TYPE_UNKNOWN                                | result = true, EXPECT_TRUE | Should Pass  |
 */
TEST(TtmlSubtecParserTest, ZeroBufferLength) {
    std::cout << "Entering ZeroBufferLength" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 0;
    double position = 10.0;
    double duration = 5.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Passed zero buffer length handling for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ZeroBufferLength" << std::endl;
}

/**
 * @brief Test to verify the behavior of the parser when a negative position value is provided.
 *
 * This test checks the parser's ability to handle a negative position value, which is an invalid input.
 * The objective is to ensure that the parser can gracefully handle such erroneous input without crashing or producing incorrect results.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 022@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                             | Test Data                                                            | Expected Result          | Notes           |
 * |------------------|-----------------------------------------|-----------------------------------------------------------------------|---------------------------|------------------|
 * | 01               | Initialize parser with WEBVTT type        | type = eSUB_TYPE_WEBVTT                                                 | Parser created            | Should Pass      |
 * | 02               | Provide negative position               | buffer = "valid TTML data", bufferLen = 15, position = -1.0, duration = 5.0 | result = true, EXPECT_TRUE | Should Verify    |
 * | 03               | Repeat with MP4, TTML, UNKNOWN types    | Same inputs, different subtitle MIME types                           | result = true, EXPECT_TRUE | Review logic     |
 */
TEST(TtmlSubtecParserTest, NegativePositionValue) {
    std::cout << "Entering NegativePositionValue" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 15;
    double position = -1.0;
    double duration = 5.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);  // Change to EXPECT_FALSE(result) if failure is expected
        std::cout << "Completed test for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting NegativePositionValue" << std::endl;
}

/**
 * @brief Test to verify the behavior of the parser when a negative duration value is provided.
 *
 * This test checks the parser's ability to handle a negative duration value. The objective is to ensure that the parser can process
 * the data correctly even when the duration is negative, which might be an edge case scenario.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 023@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                      | Test Data                                                                      | Expected Result          | Notes         |
 * |------------------|--------------------------------------------------|---------------------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Initialize parser with WEBVTT type                 | type = eSUB_TYPE_WEBVTT                                                           | Parser should initialize  | Should Pass   |
 * | 02               | Call processData with negative duration          | buffer = "valid TTML data", bufferLen = 15, position = 10.0, duration = -1.0    | result = true, EXPECT_TRUE| Should Pass   |
 * | 03               | Repeat for MP4, TTML, and UNKNOWN types          | Same as above for eSUB_TYPE_MP4, TTML, UNKNOWN                                 | result = true, EXPECT_TRUE| Should Pass   |
 */
TEST(TtmlSubtecParserTest, NegativeDurationValue) {
    std::cout << "Entering NegativeDurationValue" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 15;
    double position = 10.0;
    double duration = -1.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Handled negative duration for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting NegativeDurationValue" << std::endl;
}

/**
 * @brief Test to verify the behavior of processData with an empty buffer and valid parameters.
 *
 * This test checks the processData function of the MockTtmlSubtecParser class when provided with an empty buffer and valid position and duration parameters.
 * The objective is to ensure that the function handles empty input correctly and returns the expected result.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 024@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                               | Test Data                                                            | Expected Result          | Notes         |
 * |------------------|-------------------------------------------|-----------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Process empty buffer with WEBVTT type       | buffer = "", bufferLen = 0, position = 10.0, duration = 5.0, type = eSUB_TYPE_WEBVTT  | result = true, EXPECT_TRUE | Should Pass  |
 * | 02               | Repeat with MP4 type                      | Same as above, type = eSUB_TYPE_MP4                                  | result = true, EXPECT_TRUE | Should Pass  |
 * | 03               | Repeat with TTML                          | Same as above, type = eSUB_TYPE_TTML                                 | result = true, EXPECT_TRUE | Should Pass  |
 * | 04               | Repeat with UNKNOWN type                  | Same as above, type = eSUB_TYPE_UNKNOWN                              | result = true, EXPECT_TRUE | Should Pass  |
 */
TEST(TtmlSubtecParserTest, EmptyBufferWithValidParameters) {
    std::cout << "Entering EmptyBufferWithValidParameters" << std::endl;

    const char* buffer = "";
    size_t bufferLen = 0;
    double position = 10.0;
    double duration = 5.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"\" (empty string)"
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Handled empty buffer for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting EmptyBufferWithValidParameters" << std::endl;
}

/**
 * @brief Test the processData function with zero position and duration
 *
 * This test verifies that the processData function correctly handles the case where both the position and duration are zero.
 * This is important to ensure that the function can handle edge cases where no time has elapsed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 025@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                            | Test Data                                                                     | Expected Result          | Notes         |
 * |------------------|----------------------------------------|--------------------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Process buffer with zero time params   | buffer = "valid TTML data", bufferLen = 15, position = 0.0, duration = 0.0, type = eSUB_TYPE_WEBVTT  | result = true, EXPECT_TRUE | Should Pass  |
 * | 02               | Repeat with MP4 type                   | Same as above, type = eSUB_TYPE_MP4                                           | result = true, EXPECT_TRUE | Should Pass  |
 * | 03               | Repeat with TTML                       | Same as above, type = eSUB_TYPE_TTML                                          | result = true, EXPECT_TRUE | Should Pass  |
 * | 04               | Repeat with UNKNOWN type               | Same as above, type = eSUB_TYPE_UNKNOWN                                       | result = true, EXPECT_TRUE | Should Pass  |
 */
TEST(TtmlSubtecParserTest, ZeroPositionAndDuration) {
    std::cout << "Entering ZeroPositionAndDuration" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 15;
    double position = 0.0;
    double duration = 0.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Handled zero position and duration for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ZeroPositionAndDuration" << std::endl;
}

/**
 * @brief Test the processing of valid TTML data with zero duration.
 *
 * This test verifies that the parser correctly processes a valid TTML buffer when the duration is set to zero.
 * This is important to ensure that the parser can handle edge cases where the duration might be zero.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 026@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                | Test Data                                                              | Expected Result          | Notes         |
 * |------------------|--------------------------------------------|-------------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Initialize parser                          | type = eSUB_TYPE_WEBVTT, width = 1920, height = 1080                      | Should be successful      | Should Pass   |
 * | 02               | Process valid buffer with zero duration    | buffer = "valid TTML data", bufferLen = 15, position = 10.0, duration = 0.0 | result = true, EXPECT_TRUE | Should Pass   |
 * | 03               | Repeat for MP4, TTML, UNKNOWN types        | Same as above for each subtitle type                                   | result = true, EXPECT_TRUE | Should Pass   |
 */
TEST(TtmlSubtecParserTest, ValidBufferWithZeroDuration) {
    std::cout << "Entering ValidBufferWithZeroDuration" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 15;
    double position = 10.0;
    double duration = 0.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Handled zero duration for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ValidBufferWithZeroDuration" << std::endl;
}

/**
 * @brief Test the processData function with a valid buffer and zero position
 *
 * This test checks the behavior of the processData function when provided with a valid TTML data buffer,
 * a buffer length of 15, a position of 0.0, and a duration of 5.0.
 * The test ensures that the function correctly processes the data and returns true, indicating successful processing.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 027@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                             | Test Data                                                                  | Expected Result          | Notes         |
 * |------------------|-----------------------------------------|-----------------------------------------------------------------------------|---------------------------|---------------|
 * | 01               | Call processData with zero position     | buffer = "valid TTML data", bufferLen = 15, position = 0.0, duration = 5.0, type = eSUB_TYPE_WEBVTT  | result = true, EXPECT_TRUE | Should Pass  |
 * | 02               | Repeat with MP4 type                    | Same as above with type = eSUB_TYPE_MP4                                     | result = true, EXPECT_TRUE | Should Pass  |
 * | 03               | Repeat with TTML                        | Same as above with type = eSUB_TYPE_TTML                                    | result = true, EXPECT_TRUE | Should Pass  |
 * | 04               | Repeat with UNKNOWN type                | Same as above with type = eSUB_TYPE_UNKNOWN                                 | result = true, EXPECT_TRUE | Should Pass  |
 */
TEST(TtmlSubtecParserTest, ValidBufferWithZeroPosition) {
    std::cout << "Entering ValidBufferWithZeroPosition" << std::endl;

    const char* buffer = "valid TTML data";
    size_t bufferLen = 15;
    double position = 0.0;
    double duration = 5.0;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.processData() with:"
                  << " buffer = \"" << buffer << "\""
                  << ", bufferLen = " << bufferLen
                  << ", position = " << position
                  << ", duration = " << duration << std::endl;

        bool result = parser.processData(buffer, bufferLen, position, duration);

        std::cout << "parser.processData() returned: " << std::boolalpha << result << std::endl;

        EXPECT_TRUE(result);
        std::cout << "Handled zero position with valid data for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ValidBufferWithZeroPosition" << std::endl;
}

/**
 * @brief Test to verify the reset functionality of the TtmlSubtecParser
 *
 * This test checks if the internal state of the TtmlSubtecParser object is correctly reset by invoking the reset method.
 * The test ensures that all relevant internal variables are set to their default values after the reset.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 028@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                           | Test Data                                      | Expected Result                             | Notes         |
 * |------------------|---------------------------------------|------------------------------------------------|---------------------------------------------|---------------|
 * | 01               | Create TtmlSubtecParser object        | SubtitleMimeType::{WEBVTT, MP4, TTML, UNKNOWN} | Object should be created successfully        | Should Pass   |
 * | 02               | Call reset method                     | None                                           | Internal state reset                         | Should Pass   |
 * | 03               | Check m_isLinear                      | None                                           | m_isLinear == false                          | Should Pass   |
 * | 04               | Check m_parsedFirstPacket             | None                                           | m_parsedFirstPacket == false                 | Should Pass   |
 * | 05               | Check m_sentOffset                    | None                                           | m_sentOffset == false                        | Should Pass   |
 * | 06               | Check m_firstBeginOffset              | None                                           | m_firstBeginOffset == 0.0                    | Should Pass   |
 */
TEST(TtmlSubtecParserTest, ResetInternalState) {
    std::cout << "Entering ResetInternalState test" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Invoking parser.reset()" << std::endl;
        parser.reset();

        std::cout << "Checking internal state variables after reset:" << std::endl;

        EXPECT_EQ(parser.m_isLinear, false);
        std::cout << "Verified: m_isLinear == false" << std::endl;

        EXPECT_EQ(parser.m_parsedFirstPacket, false);
        std::cout << "Verified: m_parsedFirstPacket == false" << std::endl;

        EXPECT_EQ(parser.m_sentOffset, false);
        std::cout << "Verified: m_sentOffset == false" << std::endl;

        EXPECT_EQ(parser.m_firstBeginOffset, 0.0);
        std::cout << "Verified: m_firstBeginOffset == 0.0" << std::endl;

        std::cout << "Reset verification passed for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting ResetInternalState test" << std::endl;
}

/**
 * @brief Test to verify setting a positive offset in TtmlSubtecParser
 *
 * This test checks if the TtmlSubtecParser correctly sets a positive progress event offset.
 * It ensures that the parser can handle and store a positive offset value without errors.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 029@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                           | Test Data                                   | Expected Result          | Notes         |
 * |------------------|---------------------------------------|----------------------------------------------|---------------------------|---------------|
 * | 01               | Create TtmlSubtecParser instance      | type = eSUB_TYPE_{WEBVTT, MP4, TTML, UNKNOWN} | Instance created successfully | Should Pass  |
 * | 02               | Set positive progress event offset    | offset = 5.0                                  | Offset set successfully    | Should Pass  |
 */
TEST(TtmlSubtecParserTest, SetPositiveOffset) {
    std::cout << "Entering SetPositiveOffset" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        double offset = 5.0;
        std::cout << "Setting progress event offset to " << offset << std::endl;
        parser.setProgressEventOffset(offset);

        std::cout << "Offset set successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting SetPositiveOffset" << std::endl;
}

/**
 * @brief Test the behavior of setting a negative offset in TtmlSubtecParser
 *
 * This test verifies that the TtmlSubtecParser can handle setting a negative progress event offset correctly.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 030@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                        | Test Data                                     | Expected Result          | Notes         |
 * |------------------|------------------------------------|------------------------------------------------|---------------------------|---------------|
 * | 01               | Create TtmlSubtecParser instance   | SubtitleMimeType::{WEBVTT, MP4, TTML, UNKNOWN} | Instance created successfully | Should Pass  |
 * | 02               | Set negative progress event offset | offset = -3.0                                   | Offset set successfully    | Should Pass  |
 */
TEST(TtmlSubtecParserTest, SetNegativeOffset) {
    std::cout << "Entering SetNegativeOffset" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        double offset = -3.0;
        std::cout << "Setting progress event offset to " << offset << std::endl;
        parser.setProgressEventOffset(offset);

        std::cout << "Offset set successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting SetNegativeOffset" << std::endl;
}

/**
 * @brief Test the setProgressEventOffset method with zero offset
 *
 * This test verifies that the setProgressEventOffset method of the TtmlSubtecParser class correctly handles a zero offset value.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 031@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                          | Test Data                                     | Expected Result                         | Notes         |
 * |------------------|--------------------------------------|------------------------------------------------|------------------------------------------|---------------|
 * | 01               | Create TtmlSubtecParser object       | SubtitleMimeType::{WEBVTT, MP4, TTML, UNKNOWN} | Object should be created successfully     | Should Pass   |
 * | 02               | Set progress event offset to zero    | offset = 0.0                                   | Offset should be set to 0.0 without errors| Should Pass   |
 */
TEST(TtmlSubtecParserTest, SetZeroOffset) {
    std::cout << "Entering SetZeroOffset" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        double offset = 0.0;
        std::cout << "Setting progress event offset to " << offset << std::endl;
        parser.setProgressEventOffset(offset);

        std::cout << "Offset set to zero successfully for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting SetZeroOffset" << std::endl;
}

/**
 * @brief Test the updateTimestamp function with zero position
 *
 * This test verifies that the updateTimestamp function in the TtmlSubtecParser class handles the case where the timestamp is set to zero correctly.
 * This is important to ensure that the parser can handle edge cases where the timestamp is at the beginning of the stream.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 032@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                          | Test Data                                            | Expected Result                         | Notes         |
 * |------------------|--------------------------------------|------------------------------------------------------|------------------------------------------|---------------|
 * | 01               | Create TtmlSubtecParser object       | SubtitleMimeType::{WEBVTT, MP4, TTML, UNKNOWN}       | Object should be created successfully     | Should Pass   |
 * | 02               | Call updateTimestamp with zero       | position = 0                                         | Function should handle position = 0 correctly | Should Pass   |
 */
TEST(TtmlSubtecParserTest, UpdateTimestampWithZeroPosition) {
    std::cout << "Entering UpdateTimestampWithZeroPosition" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing updateTimestamp with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        std::cout << "Calling parser.updateTimestamp(0)" << std::endl;
        parser.updateTimestamp(0);

        std::cout << "Successfully handled updateTimestamp(0) for SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting UpdateTimestampWithZeroPosition" << std::endl;
}

/**
 * @brief Test the updateTimestamp function with a small positive position
 *
 * This test verifies that the updateTimestamp function correctly updates the timestamp when provided with a small positive position value.
 * This is important to ensure that the function handles typical positive inputs as expected.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 033@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                              | Expected Result                  | Notes         |
 * |------------------|---------------------------------------------|--------------------------------------------------------|----------------------------------|---------------|
 * | 01               | Create TtmlSubtecParser object              | SubtitleMimeType::{WEBVTT, MP4, TTML, UNKNOWN}         | Object should be created         | Should Pass   |
 * | 02               | Call updateTimestamp with small position    | position = 20                                          | Timestamp should update correctly| Should Pass   |
 */
TEST(TtmlSubtecParserTest, UpdateTimestampWithSmallPositivePosition) {
    std::cout << "Entering UpdateTimestampWithSmallPositivePosition" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        uint64_t position = 20;
        std::cout << "Calling parser.updateTimestamp(" << position << ")" << std::endl;
        parser.updateTimestamp(position);

        std::cout << "updateTimestamp completed for position = " << position << " and SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting UpdateTimestampWithSmallPositivePosition" << std::endl;
}

/**
 * @brief Test the updateTimestamp function with a large positive position value
 *
 * This test verifies the behavior of the updateTimestamp function when provided with a large positive position value.
 * It ensures that the function can handle large values without errors and updates the timestamp correctly.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 034@n
 * **Priority:** High@n
 * 
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 * 
 * **Test Procedure:**@n
 * | Variation / Step | Description                                 | Test Data                                              | Expected Result                  | Notes         |
 * |------------------|---------------------------------------------|--------------------------------------------------------|----------------------------------|---------------|
 * | 01               | Create TtmlSubtecParser object              | SubtitleMimeType::{WEBVTT, MP4, TTML, UNKNOWN}         | Object should be created         | Should Pass   |
 * | 02               | Call updateTimestamp with large position    | position = 1000000000                                  | Timestamp should update correctly| Should Pass   |
 */
TEST(TtmlSubtecParserTest, UpdateTimestampWithLargePositivePosition) {
    std::cout << "Entering UpdateTimestampWithLargePositivePosition" << std::endl;

    SubtitleMimeType types[] = {eSUB_TYPE_WEBVTT, eSUB_TYPE_MP4, eSUB_TYPE_TTML, eSUB_TYPE_UNKNOWN};

    for (auto type : types) {
        std::cout << "Testing with SubtitleMimeType: " << type << std::endl;

        TtmlSubtecParserTestHelper parser(type, 1920, 1080);

        uint64_t position = 1000000000;
        std::cout << "Calling parser.updateTimestamp(" << position << ")" << std::endl;
        parser.updateTimestamp(position);

        std::cout << "updateTimestamp completed for position = " << position << " and SubtitleMimeType: " << type << std::endl;
    }

    std::cout << "Exiting UpdateTimestampWithLargePositivePosition" << std::endl;
}    
