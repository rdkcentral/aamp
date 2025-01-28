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

#include <iostream>
#include <string>
#include <string.h>
#include <gtest/gtest.h>
#include "AampConfig.h"
#include "scte35/AampSCTE35.h"
#include "_base64.h"
#include "MockAampConfig.h"

class Scte3SectionTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
	}

	void TearDown() override
	{
		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

public:
	/**
	 * @brief Create an SCTE-35 signal decoder
	 *
	 * @param[in] data SCTE-35 signal section data
	 * @return An SCTE-35 signal decoder instance
	 */
	SCTE35Section *CreateDecoder(std::vector<uint8_t> &data)
	{
		SCTE35Decoder *decoder = NULL;
		char *encoded = base64_Encode((const unsigned char *)data.data(), data.size());
		if (encoded)
		{
			decoder = new SCTE35Decoder(std::string(encoded));
			free(encoded);
		}

		return decoder;
	}
};

/**
 * @brief One bit flag test
 */
TEST_F(Scte3SectionTests, Bool)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x82};
	int i;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	for (i = 0; i < 8; i++)
	{
		if (data[0] & (0x80 >> i))
		{
			EXPECT_TRUE(decoder->Bool("bool"));
		}
		else
		{
			EXPECT_FALSE(decoder->Bool("bool"));
		}
	}

	/* Test for overflow. */
	EXPECT_THROW(decoder->Bool("bool"), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Byte test
 */
TEST_F(Scte3SectionTests, Byte)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
	int i;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	for (i = 0; i < data.size(); i++)
	{
		ASSERT_EQ(data[i], decoder->Byte("byte"));
	}

	/* Test for overflow. */
	EXPECT_THROW(decoder->Byte("byte"), SCTE35DataException);

	delete decoder;

	/* Unaligned access test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	(void)decoder->Bool("bool");

	EXPECT_THROW(decoder->Byte("byte"), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Short test
 */
TEST_F(Scte3SectionTests, Short)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
	uint16_t expected;
	int i;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	for (i = 0; i < data.size(); i += 2)
	{
		expected = (((uint16_t)data[i]) << 8) + ((uint16_t)data[i + 1]);
		ASSERT_EQ(expected, decoder->Short("short"));
	}

	/* Test for overflow. */
	EXPECT_THROW(decoder->Short("short"), SCTE35DataException);

	delete decoder;

	/* Unaligned access test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	(void)decoder->Bool("bool");

	EXPECT_THROW(decoder->Short("short"), SCTE35DataException);

	delete decoder;
}

/**
 * @brief 32 bit integer test
 */
TEST_F(Scte3SectionTests, Integer)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
	uint32_t expected;
	int i;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	for (i = 0; i < data.size(); i += 4)
	{
		expected = (((uint32_t)data[i]) << 24) +
					(((uint32_t)data[i + 1]) << 16) +
					(((uint32_t)data[i + 2]) << 8) +
					((uint32_t)data[i + 3]);
		ASSERT_EQ(expected, decoder->Integer("integer"));
	}

	/* Test for overflow. */
	EXPECT_THROW(decoder->Integer("integer"), SCTE35DataException);

	delete decoder;

	/* Unaligned access test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	(void)decoder->Bool("bool");

	EXPECT_THROW(decoder->Short("integer"), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Multiple bits test
 */
TEST_F(Scte3SectionTests, Bits)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
	uint64_t fullValue = 0x0123456789abcdef;
	int i;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	ASSERT_EQ(fullValue, decoder->Bits("bits", 64));

	/* Test for overflow. */
	EXPECT_THROW(decoder->Bits("bits", 1), SCTE35DataException);

	delete decoder;

	/* Unaligned access tests. */
	for (i = 1; i < 64; i++)
	{
		decoder = CreateDecoder(data);
		ASSERT_TRUE(decoder != NULL);

		EXPECT_EQ(fullValue >> (64 - i), decoder->Bits("bits", i));
		EXPECT_EQ(fullValue & (0xffffffffffffffff >> i), decoder->Bits("bits", 64 - i));

		delete decoder;
	}
}

/**
 * @brief Byte string test
 */
TEST_F(Scte3SectionTests, String)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {(uint8_t)'H', (uint8_t)'e', (uint8_t)'l', (uint8_t)'l', (uint8_t)'o'};
	std::string string;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	string = decoder->String("string", 5);
	EXPECT_STREQ("Hello", string.c_str());

	/* Zero length string. */
	string = decoder->String("string", 0);
	EXPECT_STREQ("", string.c_str());

	/* Test for overflow. */
	EXPECT_THROW(decoder->String("string", 1), SCTE35DataException);

	delete decoder;

	/* Unaligned access test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	(void)decoder->Bool("bool");

	EXPECT_THROW(decoder->String("string", 4), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Reserved bits test
 */
TEST_F(Scte3SectionTests, ReservedBits)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data;
	int i;

	data = {0x78};
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	/* Read the first bit as a boolean. */
	EXPECT_FALSE(decoder->Bool("bool"));

	/* Four reserved bits. */
	EXPECT_NO_THROW(decoder->ReservedBits(4));

	/* Unset reserved bits. */
	EXPECT_THROW(decoder->ReservedBits(3), SCTE35DataException);

	delete decoder;

	/* Overflow test. */
	data = {0xff};
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	EXPECT_THROW(decoder->ReservedBits(9), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Skip bytes test
 */
TEST_F(Scte3SectionTests, SkipBytes)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
	int i;

	for (i = 0; i < (data.size() - 1); i++)
	{
		decoder = CreateDecoder(data);
		ASSERT_TRUE(decoder != NULL);

		decoder->SkipBytes(i);
		EXPECT_EQ(data[i], decoder->Byte("byte"));
		decoder->SkipBytes((data.size() - i) - 1);
		delete decoder;
	}

	/* Test for overflow. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);
	EXPECT_THROW(decoder->SkipBytes(data.size() + 1), SCTE35DataException);
	delete decoder;

	/* Unaligned access test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	(void)decoder->Bool("bool");

	EXPECT_THROW(decoder->SkipBytes(1), SCTE35DataException);

	delete decoder;
}

/**
 * @brief CRC32 test
 */
TEST_F(Scte3SectionTests, CRC32)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data =
	{
		0x01, 0x23, 0x45, 0x67,
		0x89, 0xab, 0xcd, 0xef,
		0x09, 0xee, 0xde, 0x06
	};
	uint32_t expected = 0x09eede06;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	/* Read 64 bits of data. */
	(void)decoder->Bits("bits", 64);

	/* Read the CRC32 value. */
	EXPECT_EQ(expected, decoder->CRC32());

	/* Test for overflow. */
	EXPECT_THROW(decoder->CRC32(), SCTE35DataException);

	delete decoder;

	/* Unaligned access test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	(void)decoder->Bool("bool");

	EXPECT_THROW(decoder->CRC32(), SCTE35DataException);

	delete decoder;

	/* Corrupt the data. */
	data[0] = data[0] ^ 0xff;
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	/* Read 64 bits of data. */
	(void)decoder->Bits("bits", 64);

	/* Read the incorrect CRC32 value. */
	EXPECT_THROW(decoder->CRC32(), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Section end test
 */
TEST_F(Scte3SectionTests, End)
{
	SCTE35Section *decoder;
	std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
	uint64_t fullValue = 0x0123456789abcdef;

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	ASSERT_EQ(fullValue, decoder->Bits("bits", 64));
	ASSERT_TRUE(decoder->isEnd());
	EXPECT_NO_THROW(decoder->End());

	delete decoder;

	/* Underflow test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);
	ASSERT_FALSE(decoder->isEnd());
	EXPECT_THROW(decoder->End(), SCTE35DataException);

	delete decoder;
}

/**
 * @brief Subsection test
 */
TEST_F(Scte3SectionTests, Subsection)
{
	SCTE35Section *decoder;
	SCTE35Section *object;
	std::vector<uint8_t> data = {0x04, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	/* Decode the first byte - the object length. */
	EXPECT_EQ(4, decoder->Byte("length"));

	/* Define the next 4 bytes as an object. */
	object = decoder->Subsection("object", 4);

	/* Decode the object bytes. */
	EXPECT_EQ(0x23456789, object->Integer("integer"));

	EXPECT_NO_THROW(object->End());

	/* Decode the last three bytes */
	EXPECT_EQ(0xabcdef, decoder->Bits("end", 24));

	EXPECT_NO_THROW(decoder->End());

	delete decoder;

	/* Unaligned test. */
	decoder = CreateDecoder(data);
	EXPECT_EQ(4, decoder->Byte("length"));
	(void)decoder->Bool("bit");
	EXPECT_THROW(decoder->Subsection("object", 4), SCTE35DataException);

	delete decoder;

	/* Overflow test. */
	decoder = CreateDecoder(data);
	EXPECT_EQ(4, decoder->Byte("length"));
	EXPECT_THROW(decoder->Subsection("object", 8), SCTE35DataException);

	delete decoder;

	/* Underflow test. */
	decoder = CreateDecoder(data);
	EXPECT_EQ(4, decoder->Byte("length"));
	object = decoder->Subsection("object", 5);
	EXPECT_EQ(0x23456789, object->Integer("integer"));
	EXPECT_THROW(object->End(), SCTE35DataException);

	delete decoder;
}

/**
 * Descriptor loop test
 */
TEST_F(Scte3SectionTests, DescriptorLoop)
{
	SCTE35Section *decoder;
	SCTE35DescriptorLoop *descriptorLoop;
	SCTE35Section *descriptor;
	int length;
	int idx = 0;
	std::vector<uint8_t> data = {0x04, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);

	/* Decode the first byte - the descriptor loop length. */
	length = decoder->Byte("length");
	EXPECT_EQ(data[idx++], length);

	/* Decode a descriptor loop - two descriptors of two bytes. */
	descriptorLoop = decoder->DescriptorLoop("descriptors", length);
	while (descriptorLoop->hasAnotherDescriptor())
	{
		SCTE35Section *descriptor = descriptorLoop->Descriptor();
		EXPECT_EQ(data[idx++], descriptor->Byte("tag"));
		EXPECT_EQ(data[idx++], descriptor->Byte("byte"));
		descriptor->End();
		delete descriptor;
	}
	EXPECT_EQ(idx, length + 1);
	descriptorLoop->End();
	delete descriptorLoop;

	EXPECT_EQ(0xabcdef, decoder->Bits("bits", 24));
	decoder->End();

	delete decoder;

	/* Unaligned test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);
	length = decoder->Byte("length");
	(void)decoder->Bool("bit");
	EXPECT_THROW(decoder->DescriptorLoop("descriptors", length), SCTE35DataException);
	delete decoder;

	/* Overflow tests. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);
	length = decoder->Byte("length");
	descriptorLoop = decoder->DescriptorLoop("descriptors", length);

	while (descriptorLoop->hasAnotherDescriptor())
	{
		descriptor = descriptorLoop->Descriptor();
		(void)descriptor->Byte("tag");
		(void)descriptor->Byte("byte");
		descriptor->End();
		delete descriptor;
	}

	EXPECT_THROW(descriptorLoop->Descriptor(), SCTE35DataException);
	delete descriptorLoop;
	delete decoder;

	/* Underflow test. */
	decoder = CreateDecoder(data);
	ASSERT_TRUE(decoder != NULL);
	length = decoder->Byte("length");
	descriptorLoop = decoder->DescriptorLoop("descriptors", length);

	EXPECT_TRUE(descriptorLoop->hasAnotherDescriptor());
	descriptor = descriptorLoop->Descriptor();
	(void)descriptor->Byte("tag");
	descriptor->End();

	EXPECT_TRUE(descriptorLoop->hasAnotherDescriptor());
	EXPECT_THROW(descriptorLoop->End(), SCTE35DataException);
	delete descriptor;
	delete descriptorLoop;
	delete decoder;
}
