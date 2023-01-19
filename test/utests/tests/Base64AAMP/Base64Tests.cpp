#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <_base64.h>

// Test Vectors
// Swap 'inp' and 'exp' when going from encode to decode, base64 is reversible
const unsigned char inp1[]  = "12345";
const char exp1[] = "MTIzNA==";
const unsigned char inp2[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789" "+/";
const char exp2[] = "QUJDRA==";
const char exp3[] = "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVphYmNkZWZnaGlqa2xtbm9wcXJzdHV2d3h5ejAxMjM0NTY3ODkrLw==";
const unsigned char inp4[]  = "_"; // RFC 4648 §5 URL- and filename-safe standard
const char exp4[] = "Xw==";
const unsigned char inp5[]  = "";
const char exp5[] = "";
const unsigned char inp6[] = "light work.";
const char exp6[] = "bGlnaHQgd29yay4=";

TEST(_base64Suite, encode)
{
	int size = 4;
	char *result;
	
	// padded output
	result = ::base64_Encode(inp1, size);
	EXPECT_STREQ(result, exp1) << "The base64 encode of " << inp1 << ", size " << size << " is not correct";
	free(result);
	
	// all supported characters limited length
	size = 4;
	result = ::base64_Encode(inp2, size);
	EXPECT_STREQ(result, exp2) << "The base64 encode of " << inp2 << ", size " << size << " is not correct";
	free(result);
	
	// all supported characters full length
	size = strlen((char *)inp2);
	result = ::base64_Encode(inp2, size);
	EXPECT_STREQ(result, exp3) << "The base64 encode of " << inp2 << ", size " << size << " is not correct";
	free(result);
	
	// unsupported character should return empty result?
	size = 1;
	result = ::base64_Encode(inp4, size);
	EXPECT_STREQ(result, exp4) << "The base64 encode of " << inp4 << ", size " << size << " is not correct";
	free(result);
	
	// empty input
	size = 0;
	result = ::base64_Encode(inp5, size);
	EXPECT_STREQ(result, exp5) << "The base64 encode of " << inp5 << ", size " << size << " is not correct";
	free(result);
	
	// NULL doesn't upset us if size is 0, crashes if > 0
	size = 0;
	result = ::base64_Encode(NULL, size);
	EXPECT_STREQ(result, "") << "The base64 encode of NULL, size 1 is not correct";
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
		result = ::base64_Encode(inp6, size);
		EXPECT_STREQ(result, exp) << "The base64 encode of " << inp6 << ", size " << size << " is not correct";
		free(result);
	}
}

TEST(_base64Suite, decode)
{
	size_t len = 0;
	char *result;
	
	// padded output used truncated input of length 4
	result = (char *)::base64_Decode(exp1, &len);
	int cmp = strncmp(result, (char*)inp1, len);
	EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp1 << " is not correct";
	EXPECT_EQ(len, 4) << "The base64 decode of " << exp1 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// all supported characters full length
	result = (char *)::base64_Decode(exp3, &len);
	cmp = strncmp(result, (char*)inp2, len);
	EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp2 << " is not correct";
	EXPECT_EQ(len, strlen((char *)inp2)) << "The base64 decode of " << exp2 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// unsupported character should return empty result?
	result = (char *)::base64_Decode(exp4, &len);
	EXPECT_STREQ(result, (char *)inp4) << "The base64 decode of " << exp4 << " is not correct";
	EXPECT_EQ(len, strlen((char *)inp4)) << "The base64 decode of " << exp4 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// empty input
	result = (char *)::base64_Decode(exp5, &len);
	EXPECT_EQ(len, strlen((char *)inp5)) << "The base64 decode of " << exp5 << " is not correct";
	EXPECT_EQ(len, strlen((char *)inp5)) << "The base64 decode of " << exp5 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// NULL crashes
	//result = (char *)::base64_Decode(NULL, &len);
	//EXPECT_STREQ(result, (char *)inp2) << "The base64 decode of NULL is not correct";
	//free(result);
	
	result = (char *)::base64_Decode(exp6, &len);
	cmp = strncmp(result, (char*)inp6, len);
	EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp6 << " is not correct";
	EXPECT_EQ(len, strlen((char *)inp6)) << "The base64 decode of " << exp6 << " is not correct";
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
	result = (char *)::base64_Decode(exp1, &len, size);
	int cmp = strncmp(result, (char *)inp1, len);
	EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp1 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// all supported characters limited length
	size = 3;
	result = (char *)::base64_Decode(exp2, &len, size);
	cmp = strncmp(result, (char *)inp2, size);
	EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp2 << " is not correct";
	free(result);
	
	// all supported characters full length
	size = sizeof(exp3);
	result = (char *)::base64_Decode(exp3, &len, size);
	cmp = strncmp(result, (char *)inp2, len);
	EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp3 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// unsupported character should return empty result?
	size = 1;
	result = (char *)::base64_Decode(exp4, &len, size);
	EXPECT_STREQ(result, (char *)inp4) << "The base64 decode of " << exp4 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// empty input
	size = 0;
	result = (char *)::base64_Decode(exp5, &len, size);
	EXPECT_EQ(len, 0) << "The base64 decode of " << exp5 << " is not correct";
	memset(result, 0, len);
	free(result);
	
	// NULL doesn't upset us if size is 0, crashes if > 0
	size = 0;
	result = (char *)::base64_Decode(NULL, &len, size);
	EXPECT_STREQ(result, (char *)inp5) << "The base64 decode of NULL is not correct";
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
		int cmp = strncmp(result, (char *)inp6, i);
		EXPECT_EQ(cmp, 0) << "The base64 decode of " << exp << " is not correct";
		memset(result, 0, len);
		free(result);
	}
}
