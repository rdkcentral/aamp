/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include "_base64.h"

namespace base64Test {

	// Test Vectors
	// Swap 'inp' and 'exp' when going from encode to decode, base64 is reversible
	const unsigned char base64Inp1[]  = "1234";
	const char base64Exp1[] = "MTIzNA==";
	const unsigned char base64Inp2[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789" "+/";
	const char base64Exp2[] = "QUJDRA==";
	const char base64Exp3[] = "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ejAxMjM0NTY3ODkrLw==";
	const unsigned char base64Inp4[]  = "_"; // RFC 4648 §5 URL- and filename-safe standard
	const char base64Exp4[] = "Xw==";
	const unsigned char base64Inp5[]  = "";
	const char base64Exp5[] = "";
	const unsigned char base64Inp6[] = "light work.";
	const char base64Exp6[] = "bGlnaHQgd29yay4=";

	TEST(_base64Suite, encode)
	{
		size_t size = 4;
		char *result;
		
		// padded output
		result = ::base64_Encode(base64Inp1, size);
		EXPECT_EQ(memcmp(result, base64Exp1, strlen(base64Exp1)), 0) << "The base64 encode of " << base64Inp1 << ", size " << size << " is not correct";
		free(result);
		
		// all supported characters limited length
		size = 4;
		result = ::base64_Encode(base64Inp2, size);
		EXPECT_EQ(memcmp(result, base64Exp2, strlen(base64Exp2)), 0) << "The base64 encode of " << base64Inp2 << ", size " << size << " is not correct";
		free(result);
		
		// all supported characters full length
		size = strlen((char *)base64Inp2);
		result = ::base64_Encode(base64Inp2, size);
		EXPECT_EQ(memcmp(result, base64Exp3, strlen(base64Exp3)), 0) << "The base64 encode of " << base64Inp2 << ", size " << size << " is not correct";
		free(result);
		
		// unsupported character should return empty result?
		size = 1;
		result = ::base64_Encode(base64Inp4, size);
		EXPECT_EQ(memcmp(result, base64Exp4, strlen(base64Exp4)), 0) << "The base64 encode of " << base64Inp4 << ", size " << size << " is not correct";
		free(result);
		
		// empty input
		size = 0;
		result = ::base64_Encode(base64Inp5, size);
		EXPECT_EQ(memcmp(result, base64Exp5, size), 0) << "The base64 encode of " << base64Inp5 << ", size " << size << " is not correct";
		free(result);
		
		// NULL doesn't upset us if size is 0, crashes if > 0
		size = 0;
		result = ::base64_Encode(NULL, size);
		EXPECT_EQ(memcmp(result, "", size), 0) << "The base64 encode of NULL, size 1 is not correct";
		free(result);
		
		size = 0;
		for (int i=7; i < 12; i++)
		{
			char exp[17]; // max expected output is 16 bytes
			switch (i) {
				case 7:
					strncpy(exp, "bGlnaHQgdw==", sizeof(exp));
					break;
				case 8:
					strncpy(exp, "bGlnaHQgd28=", sizeof(exp));
					break;
				case 9:
					strncpy(exp, "bGlnaHQgd29y", sizeof(exp));
					break;
				case 10:
					strncpy(exp, "bGlnaHQgd29yaw==", sizeof(exp));
					break;
				default:
				case 11:
					strncpy(exp, "bGlnaHQgd29yay4=", sizeof(exp));
					break;
			}
			size = i;
			result = ::base64_Encode(base64Inp6, size);
			EXPECT_EQ(memcmp(result, exp, strlen(exp)), 0) << "The base64 encode of " << base64Inp6 << ", size " << size << " is not correct";
			free(result);
		}
	}

	TEST(_base64Suite, decode)
	{
		size_t len = 0;
		size_t size = 0;
		char *result;
		
		// padded output used truncated input of length 4
		result = (char *)::base64_Decode(base64Exp1, &len);
		int cmp = strncmp(result, (char*)base64Inp1, len);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << base64Exp1 << " is not correct";
		EXPECT_EQ(len, strlen((char *)base64Inp1)) << "The base64 decode of " << base64Exp1 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		// all supported characters full length
		result = (char *)::base64_Decode(base64Exp3, &len);
		cmp = strncmp(result, (char*)base64Inp2, len);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << base64Exp2 << " is not correct";
		EXPECT_EQ(len, strlen((char *)base64Inp2)) << "The base64 decode of " << base64Exp2 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		// unsupported character should return empty result?
		result = (char *)::base64_Decode(base64Exp4, &len);
		EXPECT_EQ(memcmp(result, base64Inp4, strlen((char *)base64Inp4)), 0) << "The base64 decode of " << base64Exp4 << " is not correct";
		EXPECT_EQ(len, strlen((char *)base64Inp4)) << "The base64 decode of " << base64Exp4 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		// Check for invalid sizes, should not trigger address sanitizer.
		size = 1;
		result = (char *)::base64_Decode(base64Exp4, &len, size);
		EXPECT_EQ( len, 0 );
		free(result);
		
		size = 2;
		result = (char *)::base64_Decode(base64Exp4, &len, size);
		EXPECT_EQ( len, 1 );
		EXPECT_EQ(result[0], '_') << "The base64 decode of " << base64Exp4 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		size = 3;
		result = (char *)::base64_Decode(base64Exp4, &len, size);
		EXPECT_EQ( len, 1 );
		EXPECT_EQ(result[0], '_') << "The base64 decode of " << base64Exp4 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		// NULL crashes
		//result = (char *)::base64_Decode(NULL, &len);
		//EXPECT_STREQ(result, (char *)base64Inp2) << "The base64 decode of NULL is not correct";
		//free(result);
		
		result = (char *)::base64_Decode(base64Exp6, &len);
		cmp = strncmp(result, (char*)base64Inp6, len);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << base64Exp6 << " is not correct";
		EXPECT_EQ(len, strlen((char *)base64Inp6)) << "The base64 decode of " << base64Exp6 << " is not correct";
		memset(result, 0, len);
		free(result);
		
	}
	TEST(_base64Suite, decodeLen)
	{
		size_t len = 0;
		size_t size = 4;
		char *result;
		
		// Not checking returned len, checked in decode()
		
		// padded output
		result = (char *)::base64_Decode(base64Exp1, &len, size);
		int cmp = strncmp(result, (char *)base64Inp1, len);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << base64Exp1 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		// all supported characters limited length
		size = 3;
		result = (char *)::base64_Decode(base64Exp2, &len, size);
		cmp = strncmp(result, (char *)base64Inp2, len);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << base64Exp2 << " is not correct";
		free(result);
		
		// all supported characters full length
		size = sizeof(base64Exp3);
		result = (char *)::base64_Decode(base64Exp3, &len, size);
		cmp = strncmp(result, (char *)base64Inp2, len);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << base64Exp3 << " is not correct";
		memset(result, 0, len);
		free(result);
				
		// empty input
		size = 0;
		result = (char *)::base64_Decode(base64Exp5, &len, size);
		EXPECT_EQ(len, 0) << "The base64 decode of " << base64Exp5 << " is not correct";
		memset(result, 0, len);
		free(result);
		
		// NULL doesn't upset us if size is 0, crashes if > 0
		size = 0;
		result = (char *)::base64_Decode(NULL, &len, size);
		EXPECT_EQ(memcmp(result, base64Inp5, strlen((char *)base64Inp5)), 0) << "The base64 decode of NULL is not correct";
		memset(result, 0, len);
		free(result);
		
		size = 0;
		for (int i=7; i < 12; i++)
		{
			char exp[17]; // max expected output is 16 bytes
			switch (i) {
				case 7:
					strncpy(exp, "bGlnaHQgdw==", sizeof(exp));
					break;
				case 8:
					strncpy(exp, "bGlnaHQgd28=", sizeof(exp));
					break;
				case 9:
					strncpy(exp, "bGlnaHQgd29y", sizeof(exp));
					break;
				case 10:
					strncpy(exp, "bGlnaHQgd29yaw==", sizeof(exp));
					break;
				default:
				case 11:
					strncpy(exp, "bGlnaHQgd29yay4=", sizeof(exp));
					break;
			}
			size = strlen(exp);
			result = (char *)::base64_Decode(exp, &len, size);
			int cmp = strncmp(result, (char *)base64Inp6, i);
			EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp << " is not correct";
			memset(result, 0, len);
			free(result);
		}
	}

TEST(_base64Suite, bad )
{
	const char *badBase64 = "!@#*(";
	size_t srcLen = strlen(badBase64);
	size_t len = 999;
	unsigned char *result = ::base64_Decode( badBase64, &len, srcLen);
	EXPECT_TRUE( result==NULL );
	EXPECT_EQ( len, 0 );
}

TEST(_base64Suite, encodedecode )
{
	struct
	{
		const char *base64;
		const char *expected;
		size_t expectedLen;
		bool oldSyntax;
	} testData[] =
	{
		{"",""},
		{"Zg==","f"},
		{"Zm8=","fo"},
		{"Zm9v","foo"},
		{"Zm9vYg==","foob"},
		{"Zm9vYmE=","fooba"},
		{"Zm9vYmFy","foobar"},
		{
			"YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXowMTIzNDU2Nzg5MCEiwqMkJV4mKigpQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVp8XDw+PywuLzo7QCd+I3t9W10tXw==",
			"abcdefghijklmnopqrstuvwxyz01234567890!\"£$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ|\\<>?,./:;@'~#{}[]-_"
		},
		{ "bGlnaHQgd29yay4=", "light work." },
		
		{ "AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoIARIQL38/m5MtSFqBGT0XY2KM4yIUTkJDVTA0Mzg3NjU0MDA1NjMwMDc=",
			"\000\000\000Jpssh\000\000\000\000\355\357\213\251y\326J\316\243\310'\334\325\035!\355\000\000\000*\010\001\022\020/?\233\223-HZ\201\031=\027cb\214\343\042\024NBCU0438765400563007",
			74 // binary data size - can't use strlen
		},
		{ "AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoIARIQL38-m5MtSFqBGT0XY2KM4yIUTkJDVTA0Mzg3NjU0MDA1NjMwMDc=",
			"\000\000\000Jpssh\000\000\000\000\355\357\213\251y\326J\316\243\310'\334\325\035!\355\000\000\000*\010\001\022\020/?\233\223-HZ\201\031=\027cb\214\343\042\024NBCU0438765400563007",
			74, // binary data size - can't use strlen
			true // old syntax, with "-" instead of "/"
		},
	};
	
	for( int i=0; i<sizeof(testData)/sizeof(testData[0]); i++ )
	{
		const char *base64 = testData[i].base64;
		const char *expected = testData[i].expected;
		size_t expectedLen = testData[i].expectedLen;
		if( !expectedLen )
		{
			expectedLen = strlen(expected);
		}
		printf( "test#%d\n%s\n", i, expected );
		
		size_t len = 0;
		unsigned char *result = ::base64_Decode(base64, &len, strlen(base64));
		EXPECT_EQ(len,expectedLen);
		
		for( int j=0; j<len; j++ )
		{
			unsigned char c;
			c = expected[j];
			ASSERT_EQ( result[j], c );
		}
		free(result);
		
		if( !testData[i].oldSyntax )
		{ // old syntax is not reversible
			size_t encodedLen = strlen(base64);
			char *rc = ::base64_Encode( (const unsigned char *)expected, expectedLen );
			ASSERT_EQ( strlen(rc), encodedLen );
			for( int j=0; j<encodedLen; j++ )
			{
				ASSERT_EQ( rc[j], base64[j] );
			}
		}
	} // next testData
}
} // namespace
