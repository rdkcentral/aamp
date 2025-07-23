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
	const unsigned char inputData[]={'H','e','l','l','o'};
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "48656c6c6f";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData2)
{
	const unsigned char inputData[]={'r','a','j','a','j','j','a','a','m'};
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "72616a616a6a61616d";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData3)
{
	const unsigned char inputData[]={'H','e','l','l','o','1','2','3','W','o','r','l','d'};
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "48656c6c6f313233576f726c64";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData4)
{
	const unsigned char inputData[]={'1','2','3','4','5','6','7','8'};
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "3132333435363738";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}

TEST_F(Base16EncodeDecodeTest, EncodeValidData5)
{
	const unsigned char inputData[]={'@','#','$',' ','%','&'};
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "402324202526";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}
TEST_F(Base16EncodeDecodeTest, EncodeValidData6)
{
	const unsigned char inputData[]={' ',' ',' ',' ',' ','H'};
    size_t inputLength = sizeof(inputData);
    const char* expectedOutput = "202020202048";
    char* encodedData = base16_Encode(inputData, inputLength);
    EXPECT_STREQ(encodedData, expectedOutput);

}

TEST_F(Base16EncodeDecodeTest, DecodeEmptyString) {
    size_t decodedLength = 0;

    const char *emptyData="";
    base16_Decode(emptyData, 0, &decodedLength);
    EXPECT_EQ(decodedLength, 0);
}

TEST_F(Base16EncodeDecodeTest, DecodeValidData1)
{
    size_t decodedLength = 0;

	const char inputData[]= {'4','8','6','5','6','c','6','c','6','f'};
	size_t inputLength = sizeof(inputData);
    unsigned char outputData[]="Hello";
    unsigned char *decodeData= base16_Decode(inputData, inputLength, &decodedLength);
    for(size_t i=0;i<decodedLength;i++)

    {

        EXPECT_EQ(decodeData[i],outputData[i]);

    }

}
TEST_F(Base16EncodeDecodeTest, DecodeValidData3)
{
    size_t decodedLength = 0;
	const char *inputData="48656c6c6f313233576f726c64";
	size_t inputLength = strlen(inputData);
	unsigned char outputData[]={'H','e','l','l','o','1','2','3','W','o','r','l','d'};
    unsigned char *decodeData= base16_Decode(inputData, inputLength, &decodedLength);
    for(size_t i=0;i<decodedLength;i++)
    {
        EXPECT_EQ(decodeData[i],outputData[i]);
    }
}
TEST_F(Base16EncodeDecodeTest, DecodeValidData4)
{
    size_t decodedLength = 0;

	const char inputData[]={'4','0','2','3','2','4','2','0','2','5','2','6'};
	size_t inputLength = sizeof(inputData);
    unsigned char outputData[]="@#$ %&";

    unsigned char *decodeData= base16_Decode(inputData, inputLength, &decodedLength);

    for(size_t i=0;i<decodedLength;i++)
    {
        EXPECT_EQ(decodeData[i],outputData[i]);
    }
}

// TEST_F(Base16EncodeDecodeTest, DecodeMemoryAllocationFailure)
// {
//     Malloc test unnecssary here, because malloc never fails on 64 bits device.

// }
