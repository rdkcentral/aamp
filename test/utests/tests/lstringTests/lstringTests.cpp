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
#include <cmath>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include "lstring.hpp"
#include <assert.h>
#include <string.h>

struct ParseAttrListExpected
{
	const char *attr;
	const char *value;
};

#define CONTEXT_NAME "My Context"
#define CONTEXT_COUNT 8

struct ParseAttrListContext
{
	const char *name;
	int count;
	const ParseAttrListExpected *expected;
};

static void ParseAttrListCb( lstring attr, lstring value, void *context )
{
	ParseAttrListContext *ctx = (ParseAttrListContext *)context;
	const ParseAttrListExpected *expected = ctx->expected;
	assert( strcmp(ctx->name,CONTEXT_NAME)==0 );
	assert( ctx->count < CONTEXT_COUNT );
	std::string attrString = attr.tostring();
	std::string valueString = value.tostring();
	std::cout << "\t" << attrString << "\n";
	std::cout << "\t\t" << valueString << "\n";
	ASSERT_TRUE( attrString == expected[ctx->count].attr );
	ASSERT_TRUE( valueString == expected[ctx->count].value );
	ctx->count++;
	std::cout << "\t\t\t" << value.GetAttributeValueString() << "\n";
}

// ------------------------------
// SUCCESS CASES
// ------------------------------
TEST(LStringAtofTest, BasicIntegers)
{
	EXPECT_DOUBLE_EQ(lstring("123",3).atof(), 123.0);
	EXPECT_DOUBLE_EQ(lstring("0",1).atof(), 0.0);
	EXPECT_DOUBLE_EQ(lstring("-123",4).atof(), -123.0);
}

TEST(LStringAtofTest, BasicDecimals)
{
	EXPECT_DOUBLE_EQ(lstring("45.67",5).atof(), 45.67);
	EXPECT_DOUBLE_EQ(lstring("45.",3).atof(), 45.0);
	EXPECT_DOUBLE_EQ(lstring(".25",3).atof(), 0.25);
	EXPECT_DOUBLE_EQ(lstring("-.75",4).atof(), -0.75);
}

TEST(LStringAtofTest, LeadingWhitespace)
{
	EXPECT_DOUBLE_EQ(lstring("   3.14",7).atof(), 3.14);
}

TEST(LStringAtofTest, StopsOnNonNumericAfterNumber)
{
	EXPECT_DOUBLE_EQ(lstring("25,extra",8).atof(), 25.0);
	EXPECT_DOUBLE_EQ(lstring("3.25e",5).atof(), 3.25);
	EXPECT_DOUBLE_EQ(lstring("3.25xyz",7).atof(), 3.25);
	EXPECT_DOUBLE_EQ(lstring("3.1x5",5).atof(), 3.1);
	EXPECT_DOUBLE_EQ(lstring("3.x",3).atof(), 3.0);
	EXPECT_DOUBLE_EQ(lstring("4.55    ",8).atof(), 4.55);

	EXPECT_NEAR(lstring("4.55NaN    ",4).atof(), 4.55,1e-12);
	EXPECT_NEAR(lstring("4.55NaN    ",7).atof(), 4.55,1e-12);
	EXPECT_NEAR(lstring("4.55NaN",7).atof(), 4.55,1e-12);
	EXPECT_NEAR(lstring("4.55NaN    ",11).atof(), 4.55,1e-12);
}

// ------------------------------
// FAILURE CASES
// ------------------------------
TEST(LStringAtofTest, OnlyDecimalPoint)
{
	EXPECT_DOUBLE_EQ(lstring(".",1).atof(),0.0);
}

TEST(LStringAtofTest, MultipleDecimalPoints)
{
	// New atof() uses best-effort parsing: stops at second '.', returns the
	// partial value accumulated so far rather than failing the entire conversion.
	// "12.34.56" parses "12.34", stops at the second '.', returns 12.34.
	EXPECT_NEAR(lstring("12.34.56",8).atof(),12.34,1e-12);
	EXPECT_NEAR(lstring("12.34.56",5).atof(),12.34,1e-12);
}

TEST(LStringAtofTest, InvalidStartingCharacter)
{
	EXPECT_DOUBLE_EQ(lstring("abc",3).atof(),0.0);
}

TEST(LStringAtofTest, EmptyString)
{
	EXPECT_DOUBLE_EQ(lstring("",0).atof(),0.0);
}

TEST(LStringAtofTest, OnlyWhitespace)
{
	EXPECT_DOUBLE_EQ(lstring("    ",4).atof(),0.0);
	EXPECT_DOUBLE_EQ(lstring("    ",3).atof(),0.0);
}

TEST(LStringAtofTest, DoubleDot)
{
	EXPECT_DOUBLE_EQ(lstring("..25",4).atof(),0.0);
}

TEST(lstring, test1)
{
	const double epsilon = 0.00001;
	lstring emptystring;
	ASSERT_TRUE( emptystring.empty() );
	ASSERT_TRUE( emptystring.peekLastChar()==0 );
	ASSERT_TRUE( emptystring.popFirstChar()==0 );
	
	lstring simplestring("foo",3);
	ASSERT_TRUE( !simplestring.empty() );
	ASSERT_TRUE( simplestring.length()==3 );
	ASSERT_TRUE( simplestring.peekLastChar()=='o' );
	ASSERT_TRUE( simplestring.popFirstChar()=='f' );
	ASSERT_TRUE( simplestring.length()==2 );
	
	lstring trimstring("happy birthday",5);
	assert( trimstring.length()==5 );
	std::string temp = trimstring.tostring();
	ASSERT_TRUE( temp.length() == 5 );
	ASSERT_TRUE( temp == "happy" );
	
	lstring istring("314",3);
	ASSERT_TRUE( istring.atoll() == 314 );

	const char *text = "the quick brown fox jumped over the lazy dog";
	lstring searchText(text,strlen(text));
	ASSERT_TRUE( 0 == searchText.find('t') );
	ASSERT_TRUE( 16 == searchText.find('f') );
	ASSERT_TRUE( 25 == searchText.find('d') );
	ASSERT_TRUE( 44 == searchText.find('?') );
	
	ASSERT_TRUE( searchText.removePrefix("tx") == false );
	ASSERT_TRUE( searchText.removePrefix("the quick") == true );
	ASSERT_TRUE( searchText.getLen() == 44-9 );
	ASSERT_TRUE( searchText.tostring() == text+9);
	
	int line = 0;
	const char *lineText = "apple\r\n"
	"banana\rcake\n"
	"donut\n"
	"\regg\r\r\r\n"
	"food";
	lstring lines( lineText,strlen(lineText) );
	while( !lines.empty() )
	{
		lstring part = lines.mystrpbrk();
		std::cout << "#" << line++ << ": '" << part.tostring() << "'\n";
	}
	
	lstring string1("hello, hi",9);
	
	std::cout << string1.tostring() << "\n";
	
	{
		const char *attrString = "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"playlist_a-eng-0384k-aac-6c.mp4.m3u8\",GROUP-ID=\"default-audio-group\",LANGUAGE=\"en\",NAME=\"stream_6\",DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"6\"";
		const struct ParseAttrListExpected expected[CONTEXT_COUNT]
		{
			{"#EXT-X-MEDIA:TYPE","AUDIO"},
			{"URI","\"playlist_a-eng-0384k-aac-6c.mp4.m3u8\""},
			{"GROUP-ID","\"default-audio-group\""},
			{"LANGUAGE","\"en\""},
			{"NAME","\"stream_6\""},
			{"DEFAULT","YES"},
			{"AUTOSELECT","YES"},
			{"CHANNELS","\"6\""}
		};
		lstring attrList( attrString,strlen(attrString) );
		struct ParseAttrListContext context;
		context.name = CONTEXT_NAME;
		context.count = 0;
		context.expected = expected;
		attrList.ParseAttrList( ParseAttrListCb, &context );
	}
	
	
	{
		const char *attrString = "#EXT-X-MEDIA:TYPE=SUBTITLES,GROUP-ID=\"captions\",NAME=\"ENG\",DEFAULT=YES,AUTOSELECT=YES,FORCED=NO,LANGUAGE=\"eng\",URI=\"vtt/playlist.m3u8\"\0\0";
		const struct ParseAttrListExpected expected[CONTEXT_COUNT]
		{
			{"#EXT-X-MEDIA:TYPE","SUBTITLES"},
			{"GROUP-ID","\"captions\""},
			{"NAME","\"ENG\""},
			{"DEFAULT","YES"},
			{"AUTOSELECT","YES"},
			{"FORCED","NO"},
			{"LANGUAGE","\"eng\""},
			{"URI","\"vtt/playlist.m3u8\""},
		};
		lstring attrList( attrString,strlen(attrString)+2 );
		struct ParseAttrListContext context;
		context.name = CONTEXT_NAME;
		context.count = 0;
		context.expected = expected;
		attrList.ParseAttrList( ParseAttrListCb, &context );
	}
}

// ===========================================================================
// equalsCString tests
// Covers the bug where attrName="KEYFORMATVERSIONS=" and cstring="KEYFORMAT"
// caused an OOB read past cstring's null terminator in the old equal() impl.
// ===========================================================================

TEST(LStringEqualsCString, ExactMatch)
{
	// lstring content equals cstring exactly
	EXPECT_TRUE(  lstring("KEYFORMAT", 9).equalsCString("KEYFORMAT") );
	EXPECT_TRUE(  lstring("METHOD",    6).equalsCString("METHOD")    );
	EXPECT_TRUE(  lstring("",          0).equalsCString("")           );
}

TEST(LStringEqualsCString, LstringLongerThanCstring)
{
	// Regression: attrName="KEYFORMATVERSIONS=" (18 chars), cstring="KEYFORMAT" (9 chars).
	// Old code read past the null terminator of "KEYFORMAT" — must return false, not crash.
	EXPECT_FALSE( lstring("KEYFORMATVERSIONS=", 18).equalsCString("KEYFORMAT") );
	EXPECT_FALSE( lstring("BANDWIDTH=1000",     14).equalsCString("BANDWIDTH")  );
}

TEST(LStringEqualsCString, LstringShorterThanCstring)
{
	// lstring is a prefix of cstring — must return false
	EXPECT_FALSE( lstring("KEY", 3).equalsCString("KEYFORMAT") );
	EXPECT_FALSE( lstring("",    0).equalsCString("KEYFORMAT") );
}

TEST(LStringEqualsCString, ContentMismatch)
{
	// Same length, different content
	EXPECT_FALSE( lstring("METHODS", 7).equalsCString("METHODX") );
	EXPECT_FALSE( lstring("aac",     3).equalsCString("mp4")      );
}

TEST(LStringEqualsCString, SingleCharacter)
{
	EXPECT_TRUE(  lstring("X", 1).equalsCString("X") );
	EXPECT_FALSE( lstring("X", 1).equalsCString("Y") );
	EXPECT_FALSE( lstring("X", 1).equalsCString("XY") );
	EXPECT_FALSE( lstring("XY", 2).equalsCString("X") );
}

TEST(LStringEqualsCString, EmptyLstring)
{
	EXPECT_TRUE(  lstring("", 0).equalsCString("") );
	EXPECT_FALSE( lstring("", 0).equalsCString("A") );
}

TEST(LStringEqualsCString, EmptyCstring)
{
	// Non-empty lstring vs empty cstring — must return false
	EXPECT_FALSE( lstring("A", 1).equalsCString("") );
}

// ===========================================================================
// isSameView tests
// isSameView() is a pointer-identity check, NOT a content-equality check.
// ===========================================================================

TEST(LStringIsSameView, IdenticalView)
{
	const char buf[] = "hello";
	lstring a(buf, 5);
	lstring b(buf, 5);
	// Same pointer and same length — must be true
	EXPECT_TRUE( a.isSameView(b) );
}

TEST(LStringIsSameView, SameContentDifferentBuffer)
{
	const char buf1[] = "hello";
	const char buf2[] = "hello";
	lstring a(buf1, 5);
	lstring b(buf2, 5);
	// Content identical but different backing pointers — must be false
	// (isSameView is intentionally pointer-identity only)
	EXPECT_FALSE( a.isSameView(b) );
}

TEST(LStringIsSameView, DifferentLength)
{
	const char buf[] = "hello";
	lstring a(buf, 5);
	lstring b(buf, 3);
	EXPECT_FALSE( a.isSameView(b) );
}

TEST(LStringIsSameView, EmptyVsEmpty)
{
	lstring a, b;
	// Both NULL ptr, len 0 — same view
	EXPECT_TRUE( a.isSameView(b) );
}

// ===========================================================================
// SubStringMatch tests
// Old code read past the end of the lstring when cstring was longer than len.
// ===========================================================================

TEST(LStringSubStringMatch, ExactMatch)
{
	EXPECT_TRUE( lstring("KEYFORMAT", 9).SubStringMatch("KEYFORMAT") );
}

TEST(LStringSubStringMatch, CstringIsPrefix)
{
	// cstring shorter than lstring — should match (lstring starts with cstring)
	EXPECT_TRUE( lstring("KEYFORMATVERSIONS", 17).SubStringMatch("KEYFORMAT") );
}

TEST(LStringSubStringMatch, CstringLongerThanLstring)
{
	// Old code would walk past the end of the lstring buffer here.
	EXPECT_FALSE( lstring("KEY", 3).SubStringMatch("KEYFORMAT") );
}

TEST(LStringSubStringMatch, EmptyCstring)
{
	// Empty needle always matches
	EXPECT_TRUE( lstring("anything", 8).SubStringMatch("") );
	EXPECT_TRUE( lstring("", 0).SubStringMatch("") );
}

TEST(LStringSubStringMatch, EmptyLstring)
{
	EXPECT_FALSE( lstring("", 0).SubStringMatch("A") );
}

// ===========================================================================
// removePrefix(const char*) tests
// Old code walked past the end of the lstring when prefix was longer than len.
// ===========================================================================

TEST(LStringRemovePrefixStr, MatchAndRemove)
{
	lstring s("KEYFORMAT=identity", 18);
	EXPECT_TRUE( s.removePrefix("KEYFORMAT=") );
	EXPECT_EQ( s.tostring(), "identity" );
}

TEST(LStringRemovePrefixStr, PrefixLongerThanLstring)
{
	// Old code would walk past the end of the lstring buffer here.
	lstring s("KEY", 3);
	EXPECT_FALSE( s.removePrefix("KEYFORMAT") );
	// View must be unchanged on failure
	EXPECT_EQ( s.tostring(), "KEY" );
}

TEST(LStringRemovePrefixStr, NoMatch)
{
	lstring s("METHOD=AES-128", 14);
	EXPECT_FALSE( s.removePrefix("KEYFORMAT") );
	EXPECT_EQ( s.tostring(), "METHOD=AES-128" );
}

TEST(LStringRemovePrefixStr, EmptyPrefix)
{
	lstring s("hello", 5);
	EXPECT_TRUE( s.removePrefix("") );
	EXPECT_EQ( s.tostring(), "hello" );
}

// ===========================================================================
// substr tests
// Old code had no bounds check; negative or out-of-range offsets caused UB.
// ===========================================================================

TEST(LStringSubstr, NormalCase)
{
	lstring s("KEYFORMATVERSIONS=1", 19);
	lstring sub = s.substr(9);
	EXPECT_EQ( sub.tostring(), "VERSIONS=1" );
}

TEST(LStringSubstr, OffsetZero)
{
	lstring s("hello", 5);
	EXPECT_EQ( s.substr(0).tostring(), "hello" );
}

TEST(LStringSubstr, OffsetEqualsLength)
{
	// Offset == len: should return empty, not UB
	lstring s("hello", 5);
	EXPECT_TRUE( s.substr(5).empty() );
}

TEST(LStringSubstr, OffsetBeyondLength)
{
	// Out-of-range: should return empty, not UB
	lstring s("hello", 5);
	EXPECT_TRUE( s.substr(99).empty() );
}

TEST(LStringSubstr, NegativeOffset)
{
	// Negative offset: should return empty, not UB
	lstring s("hello", 5);
	EXPECT_TRUE( s.substr(-1).empty() );
}
