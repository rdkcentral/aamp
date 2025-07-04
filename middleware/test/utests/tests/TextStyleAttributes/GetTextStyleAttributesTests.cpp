/*
* If not stated otherwise in this file or this component's license file the
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
#include "PlayerLogManager.h"
#include "MockPlayerJsonObject.h"
#include "TextStyleAttributes.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::An;
using ::testing::DoAll;

class GetTextStyleAttributesTests : public ::testing::Test
{
protected:

    std::unique_ptr<TextStyleAttributes> mAttributes{};

    void SetUp() override
    {
        mAttributes = std::unique_ptr<TextStyleAttributes>(new TextStyleAttributes());

        g_mockPlayerJsonObject = std::make_shared<StrictMock<MockPlayerJsonObject>>();
    }

    void TearDown() override
    {
        mAttributes = nullptr;

        g_mockPlayerJsonObject = nullptr;
    }
};

ACTION(ThrowJsonException)
{
    throw PlayerJsonParseException();
}

/*
    Test the getAttributes function supplying it with empty Json string
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, EmptyJsonOptionsString)
{
    std::string options{};
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_EQ(-1, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes function when PlayerJsonObject throws exception
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, JsonExceptionThrown)
{
    std::string options = "{\"fontSize\":\"32.4px\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(ThrowJsonException());
    EXPECT_EQ(-1, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes function when PlayerJsonObject unsuccessfully retrieves value
    A wrong key in the Json object (as set in options) is used to test the function.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, JsonValueNotReturned)
{
    std::string options = "{\"fontSize\":\"32.4px\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, FontSizeRightKeyInvalidValueJsonOptionsString)
{
    std::string penSizeValue = "32.4px";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));

    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with font size small, penSizevalue expressed in lower case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueSmallLowerCase)
{
    std::string penSizeValue = "small";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_SMALL);
}

/*
    Test the getAttributes with font size small, penSizevalue expressed in Upper case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueSmallUpperCase)
{
    std::string penSizeValue = "SMALL";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_SMALL);
}

/*
    Test the getAttributes with font size Medium, penSizevalue expressed in lower case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueMediumLowerCase)
{
    std::string penSizeValue = "medium";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_STANDARD);
}

/*
    Test the getAttributes with font size Medium, penSizevalue expressed in Upper case
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
    Two test cases are sufficient to prove that Upper case Json values are handled appropriately
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueMediumUpperCase)
{
    std::string penSizeValue = "MEDIUM";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_STANDARD);
}

/*
    Test the getAttributes with font size Standard
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueStandard)
{
    std::string penSizeValue = "standard";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_STANDARD);
}

/*
    Test the getAttributes with font size large
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueLarge)
{
    std::string penSizeValue = "large";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_LARGE);
}

/*
    Test the getAttributes with font size FONT_SIZE_EXTRALARGE
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueExtralarge)
{
    std::string penSizeValue = "extra_large";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_EXTRALARGE);
}

/*
    Test the getAttributes with font size auto i.e. embedded
    This will also test the output expected from the getFontSize function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontSizeExpectedJsonOptionsStringValueAuto)
{
    std::string penSizeValue = "auto";
    std::string options =  "{\"penSize\":\"" + penSizeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(penSizeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_SIZE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_SIZE_ARR_POSITION], mAttributes->FONT_SIZE_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, FontStyleRightKeyInvalidValueJsonOptionsString)
{
    std::string fontStyleValue = "italics";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with font size monospaced serif, fontstyle value expressed in lower case
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueMonospacedserifLowerCase)
{
    std::string fontStyleValue = "monospaced_serif";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_MONOSPACED_SERIF);
}

/*
    Test the getAttributes with font size monospaced serif, fontstyle value expressed in lower case, separated by space
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueMonospacedserifLowerCaseSpaceSeparated)
{
    std::string fontStyleValue = "monospaced serif";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_MONOSPACED_SERIF);
}

/*
    Test the getAttributes with font style monospaced serif, fontStyle value expressed in Upper case
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per penSizeValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueMonospacedserifUpperCase)
{
    std::string fontStyleValue = "MONOSPACED_SERIF";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_MONOSPACED_SERIF);
}

/*
    Test the getAttributes with font style proportional serif.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueProportionalserif)
{
    std::string fontStyleValue = "proportional_serif";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_PROPORTIONAL_SERIF);
}

/*
    Test the getAttributes with font style monospaced sans serif.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueMonospacedsansserif)
{
    std::string fontStyleValue = "monospaced_sanserif";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_MONOSPACED_SANSSERIF);
}

/*
    Test the getAttributes with font style proportional sans serif.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueProportionalsansserif)
{
    std::string fontStyleValue = "proportional_sanserif";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_PROPORTIONAL_SANSSERIF);
}

/*
    Test the getAttributes with font style casual.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueCasual)
{
    std::string fontStyleValue = "casual";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_CASUAL);
}

/*
    Test the getAttributes with font style cursive.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueCursive)
{
    std::string fontStyleValue = "cursive";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_CURSIVE);
}

/*
    Test the getAttributes with font style small capital.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueSmallcapital)
{
    std::string fontStyleValue = "small capital";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_SMALL_CAPITALS);
}

/*
    Test the getAttributes with font style default.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueDefault)
{
    std::string fontStyleValue = "default";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_DEFAULT);
}

/*
    Test the getAttributes with font style auto.
    This will also test the output expected from the getFontStyle function.
    Expected values are: - a valid attributesMask and attributeValues as per fontStyleValue
*/
TEST_F(GetTextStyleAttributesTests, FontStyleExpectedJsonOptionsStringValueAuto)
{
    std::string fontStyleValue = "auto";
    std::string options =  "{\"fontStyle\":\"" + fontStyleValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontStyleValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_STYLE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_STYLE_ARR_POSITION], mAttributes->FONT_STYLE_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, FontColorRightKeyInvalidValueJsonOptionsString)
{
    std::string fontColorValue = "pink";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with font color black.
    This will also test the output expected from the getColor function. Color value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueBlackLowerCase)
{
    std::string fontColorValue = "black";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with font color black.
    This will also test the output expected from the getColor function. Color value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueBlackUpperCase)
{
    std::string fontColorValue = "BLACK";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with font color white.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueWhite)
{
    std::string fontColorValue = "white";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_WHITE);
}

/*
    Test the getAttributes with font color red.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value.
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueRed)
{
    std::string fontColorValue = "red";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_RED);
}

/*
    Test the getAttributes with font color green.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueGreen)
{
    std::string fontColorValue = "green";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_GREEN);
}

/*
    Test the getAttributes with font color blue.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value.
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueBlue)
{
    std::string fontColorValue = "blue";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_BLUE);
}

/*
    Test the getAttributes with font color yellow.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value.
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueBlack)
{
    std::string fontColorValue = "yellow";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_YELLOW);
}

/*
    Test the getAttributes with font color magenta.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueMagenta)
{
    std::string fontColorValue = "magenta";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_MAGENTA);
}

/*
    Test the getAttributes with font color cyan.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueCyan)
{
    std::string fontColorValue = "cyan";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_CYAN);
}

/*
    Test the getAttributes with font color auto.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundColor value
*/
TEST_F(GetTextStyleAttributesTests, FontColorExpectedJsonOptionsStringValueAuto)
{
    std::string fontColorValue = "auto";
    std::string options =  "{\"textForegroundColor\":\"" + fontColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_COLOR_ARR_POSITION], mAttributes->COLOR_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorRightKeyInvalidValueJsonOptionsString)
{
    std::string backgroundColorValue = "pink";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with background color black.
    This will also test the output expected from the getColor function. Color value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueBlackLowerCase)
{
    std::string backgroundColorValue = "black";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with background color black.
    This will also test the output expected from the getColor function. Color value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueBlackUpperCase)
{
    std::string backgroundColorValue = "BLACK";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with background color black.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueWhite)
{
    std::string backgroundColorValue = "white";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_WHITE);
}

/*
    Test the getAttributes with background color red.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueRed)
{
    std::string backgroundColorValue = "red";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_RED);
}

/*
    Test the getAttributes with background color green.
    This will also test the output expected from the getColor function. Color value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueGreen)
{
    std::string backgroundColorValue = "green";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_GREEN);
}

/*
    Test the getAttributes with background color blue.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueBlue)
{
    std::string backgroundColorValue = "blue";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_BLUE);
}

/*
    Test the getAttributes with background color yellow.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueYellow)
{
    std::string backgroundColorValue = "yellow";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_YELLOW);
}

/*
    Test the getAttributes with background color magenta.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueMagenta)
{
    std::string backgroundColorValue = "magenta";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_MAGENTA);
}

/*
    Test the getAttributes with background color cyan.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueCyan)
{
    std::string backgroundColorValue = "cyan";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_CYAN);
}

/*
    Test the getAttributes with background color auto.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundColor value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundColorExpectedJsonOptionsStringValueAuto)
{
    std::string backgroundColorValue = "auto";
    std::string options =  "{\"textBackgroundColor\":\"" + backgroundColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_COLOR_ARR_POSITION], mAttributes->COLOR_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeRightKeyInvalidValueJsonOptionsString)
{
    std::string edgeTypeValue = "curved";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with edge style none.
    This will also test the output expected from the getEdgeType function. Edge type value in lower case
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueNoneLowerCase)
{
    std::string edgeTypeValue = "none";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_NONE);
}

/*
    Test the getAttributes with edge style none.
    This will also test the output expected from the getEdgeType function. Edge type value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueNoneUpperCase)
{
    std::string edgeTypeValue = "NONE";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_NONE);
}

/*
    Test the getAttributes with edge style raised.
    This will also test the output expected from the getEdgeType function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueRaised)
{
    std::string edgeTypeValue = "raised";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_RAISED);
}

/*
    Test the getAttributes with edge style depressed.
    This will also test the output expected from the getEdgeType function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueDepressed)
{
    std::string edgeTypeValue = "depressed";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_DEPRESSED);
}

/*
    Test the getAttributes with edge style uniform.
    This will also test the output expected from the getEdgeType function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueUniform)
{
    std::string edgeTypeValue = "uniform";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_UNIFORM);
}

/*
    Test the getAttributes with edge style left drop shadow.
    This will also test the output expected from the getEdgeType function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueLeftdropshadow)
{
    std::string edgeTypeValue = "Left drop shadow";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_SHADOW_LEFT);
}

/*
    Test the getAttributes with edge style right drop shadow.
    This will also test the output expected from the getEdgeType function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueRightdropshadow)
{
    std::string edgeTypeValue = "Right drop shadow";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_SHADOW_RIGHT);
}

/*
    Test the getAttributes with edge style auto.
    This will also test the output expected from the getEdgeType function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeStyle value
*/
TEST_F(GetTextStyleAttributesTests, EdgeTypeExpectedJsonOptionsStringValueAuto)
{
    std::string edgeTypeValue = "auto";
    std::string options =  "{\"textEdgeStyle\":\"" + edgeTypeValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeTypeValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_TYPE_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_TYPE_ARR_POSITION], mAttributes->EDGE_TYPE_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorRightKeyInvalidValueJsonOptionsString)
{
    std::string edgeColorValue = "pink";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with edge color black.
    This will also test the output expected from the getColor function. Color value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueBlackLowerCase)
{
    std::string edgeColorValue = "black";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with edge color black.
    This will also test the output expected from the getColor function. Color value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueBlackUpperCase)
{
    std::string edgeColorValue = "BLACK";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with edge color white.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueWhite)
{
    std::string edgeColorValue = "white";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_WHITE);
}

/*
    Test the getAttributes with edge color red.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueRed)
{
    std::string edgeColorValue = "red";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_RED);
}

/*
    Test the getAttributes with edge color green.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueGreen)
{
    std::string edgeColorValue = "green";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_GREEN);
}

/*
    Test the getAttributes with edge color blue.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueBlue)
{
    std::string edgeColorValue = "blue";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_BLUE);
}

/*
    Test the getAttributes with edge color yellow.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueYellow)
{
    std::string edgeColorValue = "yellow";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_YELLOW);
}

/*
    Test the getAttributes with edge color mageta.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueMagenta)
{
    std::string edgeColorValue = "magenta";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_MAGENTA);
}

/*
    Test the getAttributes with edge color cyan.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueCyan)
{
    std::string edgeColorValue = "cyan";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_CYAN);
}

/*
    Test the getAttributes with edge color auto.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per textEdgeColor value
*/
TEST_F(GetTextStyleAttributesTests, EdgeColorExpectedJsonOptionsStringValueAuto)
{
    std::string edgeColorValue = "auto";
    std::string options =  "{\"textEdgeColor\":\"" + edgeColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(edgeColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->EDGE_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->EDGE_COLOR_ARR_POSITION], mAttributes->COLOR_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityRightKeyInvalidValueJsonOptionsString)
{
    std::string backgroundOpacityValue = "none";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with background opacity solid.
    This will also test the output expected from the getOpacity function. Opacity value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityExpectedJsonOptionsStringValueSolidLowerCase)
{
    std::string backgroundOpacityValue = "solid";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_OPACITY_ARR_POSITION], mAttributes->OPACITY_SOLID);
}

/*
    Test the getAttributes with background opacity solid.
    This will also test the output expected from the getOpacity function. Opacity value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityExpectedJsonOptionsStringValueSolidUpperCase)
{
    std::string backgroundOpacityValue = "SOLID";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_OPACITY_ARR_POSITION], mAttributes->OPACITY_SOLID);
}

/*
    Test the getAttributes with background opacity flash.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityExpectedJsonOptionsStringValueFlash)
{
    std::string backgroundOpacityValue = "flash";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_OPACITY_ARR_POSITION], mAttributes->OPACITY_FLASHING);
}

/*
    Test the getAttributes with background opacity translucent.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityExpectedJsonOptionsStringValueTranslucent)
{
    std::string backgroundOpacityValue = "translucent";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_OPACITY_ARR_POSITION], mAttributes->OPACITY_TRANSLUCENT);
}

/*
    Test the getAttributes with background opacity transparent.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityExpectedJsonOptionsStringValueTransparent)
{
    std::string backgroundOpacityValue = "transparent";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_OPACITY_ARR_POSITION], mAttributes->OPACITY_TRANSPARENT);
}

/*
    Test the getAttributes with background opacity auto.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textBackgroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, BackgroundOpacityExpectedJsonOptionsStringValueAuto)
{
    std::string backgroundOpacityValue = "auto";
    std::string options =  "{\"textBackgroundOpacity\":\"" + backgroundOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(backgroundOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->BACKGROUND_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->BACKGROUND_OPACITY_ARR_POSITION], mAttributes->OPACITY_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityRightKeyInvalidValueJsonOptionsString)
{
    std::string fontOpacityValue = "none";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with font opacity solid.
    This will also test the output expected from the getOpacity function. Opacity value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityExpectedJsonOptionsStringValueSolidLowerCase)
{
    std::string fontOpacityValue = "solid";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_OPACITY_ARR_POSITION], mAttributes->OPACITY_SOLID);
}

/*
    Test the getAttributes with font opacity solid.
    This will also test the output expected from the getOpacity function. Opacity value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityExpectedJsonOptionsStringValueSolidUpperCase)
{
    std::string fontOpacityValue = "SOLID";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_OPACITY_ARR_POSITION], mAttributes->OPACITY_SOLID);
}

/*
    Test the getAttributes with font opacity flash.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityExpectedJsonOptionsStringValueFlash)
{
    std::string fontOpacityValue = "flash";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_OPACITY_ARR_POSITION], mAttributes->OPACITY_FLASHING);
}

/*
    Test the getAttributes with font opacity translucent.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityExpectedJsonOptionsStringValueTranslucent)
{
    std::string fontOpacityValue = "translucent";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_OPACITY_ARR_POSITION], mAttributes->OPACITY_TRANSLUCENT);
}

/*
    Test the getAttributes with font opacity transparent.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityExpectedJsonOptionsStringValueTransparent)
{
    std::string fontOpacityValue = "transparent";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_OPACITY_ARR_POSITION], mAttributes->OPACITY_TRANSPARENT);
}

/*
    Test the getAttributes with font opacity auto.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per textForegroundOpacity value
*/
TEST_F(GetTextStyleAttributesTests, FontOpacityExpectedJsonOptionsStringValueAuto)
{
    std::string fontOpacityValue = "auto";
    std::string options =  "{\"textForegroundOpacity\":\"" + fontOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(fontOpacityValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->FONT_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->FONT_OPACITY_ARR_POSITION], mAttributes->OPACITY_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, WindowColorRightKeyInvalidValueJsonOptionsString)
{
    std::string windowColorValue = "pink";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with window color black.
    This will also test the output expected from the getColor function. Color value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueBlackLowerCase)
{
    std::string windowColorValue = "black";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with window color black.
    This will also test the output expected from the getColor function. Color value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueBlackUpperCase)
{
    std::string windowColorValue = "BLACK";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_BLACK);
}

/*
    Test the getAttributes with window color white.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueWhite)
{
    std::string windowColorValue = "white";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_WHITE);
}

/*
    Test the getAttributes with window color red.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueRed)
{
    std::string windowColorValue = "red";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_RED);
}

/*
    Test the getAttributes with window color green.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueGreen)
{
    std::string windowColorValue = "green";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_GREEN);
}

/*
    Test the getAttributes with window color blue.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueBlue)
{
    std::string windowColorValue = "blue";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_BLUE);
}

/*
    Test the getAttributes with window color yellow.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueYellow)
{
    std::string windowColorValue = "yellow";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_YELLOW);
}

/*
    Test the getAttributes with window color magenta.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueMagenta)
{
    std::string windowColorValue = "Magenta";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_MAGENTA);
}

/*
    Test the getAttributes with window color cyan.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueCyan)
{
    std::string windowColorValue = "cyan";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_CYAN);
}

/*
    Test the getAttributes with window color auto.
    This will also test the output expected from the getColor function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillColor value
*/
TEST_F(GetTextStyleAttributesTests, WindowColorExpectedJsonOptionsStringValueAuto)
{
    std::string windowColorValue = "auto";
    std::string options =  "{\"windowFillColor\":\"" + windowColorValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowColorValue), Return(true)));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>())).WillOnce(Return(false));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_COLOR_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_COLOR_ARR_POSITION], mAttributes->COLOR_EMBEDDED);
}

/*
    Test the getAttributes function supplying it with Right Key but invalid corresponding value.
    In this case getAttributes must set the attributeMask to 0; informing caller nothing to proceed
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityRightKeyInvalidValueJsonOptionsString)
{
    std::string windowOpacityValue = "none";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0x1234;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));
    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, 0);
}

/*
    Test the getAttributes with font opacity solid.
    This will also test the output expected from the getOpacity function. Opacity value in lower case.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillOpacity value
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityExpectedJsonOptionsStringValueSolidLowerCase)
{
    std::string windowOpacityValue = "solid";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_OPACITY_ARR_POSITION], mAttributes->OPACITY_SOLID);
}

/*
    Test the getAttributes with font opacity solid.
    This will also test the output expected from the getOpacity function. Opacity value in upper case.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillOpacity value
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityExpectedJsonOptionsStringValueSolidUpperCase)
{
    std::string windowOpacityValue = "SOLID";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_OPACITY_ARR_POSITION], mAttributes->OPACITY_SOLID);
}

/*
    Test the getAttributes with font opacity flash.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillOpacity value
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityExpectedJsonOptionsStringValueFlash)
{
    std::string windowOpacityValue = "flash";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_OPACITY_ARR_POSITION], mAttributes->OPACITY_FLASHING);
}

/*
    Test the getAttributes with font opacity translucent.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillOpacity value
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityExpectedJsonOptionsStringValueTranslucent)
{
    std::string windowOpacityValue = "translucent";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_OPACITY_ARR_POSITION], mAttributes->OPACITY_TRANSLUCENT);
}

/*
    Test the getAttributes with font opacity transparent.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillOpacity value
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityExpectedJsonOptionsStringValueTransparent)
{
    std::string windowOpacityValue = "transparent";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_OPACITY_ARR_POSITION], mAttributes->OPACITY_TRANSPARENT);
}

/*
    Test the getAttributes with font opacity auto.
    This will also test the output expected from the getOpacity function.
    Expected values are: - a valid attributesMask and attributeValues as per windowFillOpacity value
*/
TEST_F(GetTextStyleAttributesTests, WindowOpacityExpectedJsonOptionsStringValueAuto)
{
    std::string windowOpacityValue = "auto";
    std::string options =  "{\"windowFillOpacity\":\"" + windowOpacityValue + "\"}";
    std::uint32_t attributesMask = 0;
    attributesType attributesValues = {0};

    EXPECT_CALL(*g_mockPlayerJsonObject, get("penSize", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("fontStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeStyle", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textEdgeColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textBackgroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("textForegroundOpacity", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillColor", An<std::string&>())).WillOnce(Return(false));
    EXPECT_CALL(*g_mockPlayerJsonObject, get("windowFillOpacity", An<std::string&>()))
        .WillOnce(DoAll(SetArgReferee<1>(windowOpacityValue), Return(true)));

    EXPECT_EQ(0, mAttributes->getAttributes(options, attributesValues, attributesMask));
    EXPECT_EQ(attributesMask, (1<<mAttributes->WIN_OPACITY_ARR_POSITION));
    EXPECT_EQ(attributesValues[mAttributes->WIN_OPACITY_ARR_POSITION], mAttributes->OPACITY_EMBEDDED);
}
