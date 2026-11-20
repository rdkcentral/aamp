#include "common/Base64.h"

#include <gtest/gtest.h>

using nmqe::base64Decode;

TEST(Base64, DecodesPaddedAndUnpadded)
{
	EXPECT_EQ(base64Decode("aGVsbG8=").value(), std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'}));
	EXPECT_EQ(base64Decode("aGVsbG8").value(), std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'}));
	EXPECT_EQ(base64Decode("").value(), std::vector<uint8_t>());
}

TEST(Base64, AcceptsUrlSafeAndWhitespace)
{
	EXPECT_EQ(base64Decode("-_8=").value(), std::vector<uint8_t>({0xfb, 0xff}));
	EXPECT_EQ(base64Decode("aGVs\nbG8=").value(), std::vector<uint8_t>({'h', 'e', 'l', 'l', 'o'}));
}

TEST(Base64, RejectsMalformed)
{
	EXPECT_FALSE(base64Decode("a$==").has_value());
	EXPECT_FALSE(base64Decode("aGVsbG8=x").has_value());
	EXPECT_FALSE(base64Decode("a").has_value());
}
