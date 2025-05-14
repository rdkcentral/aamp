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
struct ParseAttrListExpected mParseAttrListExpected[CONTEXT_COUNT]
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

struct ParseAttrListContext
{
	const char *name;
	int count;
};

static void ParseAttrListCb( lstring attr, lstring value, void *context )
{
	ParseAttrListContext *ctx = (ParseAttrListContext *)context;
	assert( strcmp(ctx->name,CONTEXT_NAME)==0 );
	assert( ctx->count < CONTEXT_COUNT );
	std::string attrString = attr.tostring();
	std::string valueString = value.tostring();
	std::cout << "\t" << attrString << "\n";
	std::cout << "\t\t" << valueString << "\n";
	ASSERT_TRUE( attrString == mParseAttrListExpected[ctx->count].attr );
	ASSERT_TRUE( valueString == mParseAttrListExpected[ctx->count].value );
	ctx->count++;
	std::cout << "\t\t\t" << value.GetAttributeValueString() << "\n";
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
	
	lstring fstring("314.159",7);
	double fval = fstring.atof();
	ASSERT_TRUE( fabs(fval-314.159)< epsilon );
	
	lstring fstring2("-123.456",8);
	double fval2 = fstring2.atof();
	ASSERT_TRUE( fabs(-123.456 - fval2)< epsilon );
	
	lstring fstring3("-267,xx",7);
	double fval3 = fstring3.atof();
	ASSERT_TRUE( fabs(-267 - fval3)< epsilon );
	
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
	
	const char *attrString = "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"playlist_a-eng-0384k-aac-6c.mp4.m3u8\",GROUP-ID=\"default-audio-group\",LANGUAGE=\"en\",NAME=\"stream_6\",DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"6\"";
	lstring attrList( attrString,strlen(attrString) );
	struct ParseAttrListContext context;
	context.name = CONTEXT_NAME;
	context.count = 0;
	attrList.ParseAttrList( ParseAttrListCb, &context );
}


