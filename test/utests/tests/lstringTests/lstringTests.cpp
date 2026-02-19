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
	EXPECT_DOUBLE_EQ(lstring("12.34.56",8).atof(),0.0);
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


