/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include "base16.h"
#include <gtest/gtest.h>
using namespace testing;

class Base16EncodeDecodeTest : public ::testing::Test {
protected:
    char *encoded;

    void SetUp() override {
        const unsigned char *inputData = (const unsigned char *)"hello";
        encoded = base16_Encode(inputData, 5);
        ASSERT_NE(encoded, nullptr);
    }

    void TearDown() override {
        if (encoded != nullptr) {
            free(encoded);
        }
    }
};
TEST_F(Base16EncodeDecodeTest, EncodeValidData1)
{
    const unsigned char inputData[]="Hello";
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "48656c6c6f00";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData2)
{
    const unsigned char inputData[]="rajajjaam";
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "72616a616a6a61616d00";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData3)

{
    const unsigned char inputData[]="Hello123World";

    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "48656c6c6f313233576f726c6400";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData4)
{
    const unsigned char inputData[]="12345678";
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "313233343536373800";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}

TEST_F(Base16EncodeDecodeTest, EncodeValidData5)
{
    const unsigned char inputData[]="@#$ %&";
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "40232420252600";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData6)
{
    const unsigned char inputData[]="     H";
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "20202020204800";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}

TEST_F(Base16EncodeDecodeTest, DecodeEmptyString) {
    size_t decodedLength = 0;

    const char *emptyData="";
    unsigned char *decodedData = base16_Decode(emptyData, 0, &decodedLength);
    EXPECT_EQ(decodedLength, 0);
}

TEST_F(Base16EncodeDecodeTest, DecodeValidData1)
{
    size_t decodedLength = 0;

    const char inputData[]= "48656c6c6f00";

    unsigned char outputData[]="Hello";

    size_t inputLength = sizeof(inputData);

    unsigned char *decodeData= base16_Decode(inputData, inputLength, &decodedLength);
    for(size_t i=0;i<decodedLength;i++)

    {

        EXPECT_EQ(decodeData[i],outputData[i]);

    }

}
TEST_F(Base16EncodeDecodeTest, DecodeValidData3)
{
    size_t decodedLength = 0;
    const char inputData[]="48656c6c6f313233576f726c6400";
    unsigned char outputData[]="Hello123World";
    size_t inputLength = sizeof(inputData);
    unsigned char *decodeData= base16_Decode(inputData, inputLength, &decodedLength);
    for(size_t i=0;i<decodedLength;i++)
    {
        EXPECT_EQ(decodeData[i],outputData[i]);
    }
}
TEST_F(Base16EncodeDecodeTest, DecodeValidData4)
{
    size_t decodedLength = 0;

    const char inputData[]="40232420252600";

    unsigned char outputData[]="@#$ %&";

    size_t inputLength = sizeof(inputData);

    unsigned char *decodeData= base16_Decode(inputData, inputLength, &decodedLength);

    for(size_t i=0;i<decodedLength;i++)
    {
        EXPECT_EQ(decodeData[i],outputData[i]);
    }
}

TEST_F(Base16EncodeDecodeTest, DecodeMemoryAllocationFailure)
{
    size_t decodedLength = 0;
    const char inputData[]="48656c6c6f00";
    unsigned char *decodeData= base16_Decode(inputData, SIZE_MAX, &decodedLength);
}

