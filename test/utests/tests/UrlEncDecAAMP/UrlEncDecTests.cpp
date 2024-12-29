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
/*
 !! Note we are including the source file to access static functions !!
 */
#include <AampUtils.cpp>

/*
 
 Reference: https://www.urlencoder.org/
 
 RFC 3986 section 2.2 Reserved Characters (January 2005)
 !	#	$	&	'	(	)	*	+	,	/	:	;	=	?	@	[	]
 %21 %23 %24 %26 %27 %28 %29 %2A %2B %2C %2F %3A %3B %3D %3F %40 %5B %5D

 RFC 3986 section 2.3 Unreserved Characters (January 2005)
 A	B	C	D	E	F	G	H	I	J	K	L	M	N	O	P	Q	R	S	T	U	V	W	X	Y	Z
 a	b	c	d	e	f	g	h	i	j	k	l	m	n	o	p	q	r	s	t	u	v	w	x	y	z
 0	1	2	3	4	5	6	7	8	9	-	_	.	~
 
 Common characters after percent-encoding (ASCII or UTF-8 based)
 newline	space	"	%	-	.	<	>	\	^	_	`	{	|	}	~
 %0A or %0D or %0D%0A	%20	%22	%25	%2D	%2E	%3C	%3E	%5C	%5E	%5F	%60	%7B	%7C	%7D	%7E

 */


// Fakes to allow linkage
AampConfig *gpGlobalConfig=NULL;

TEST(UrlEncDecSuite, encode)
{
	std::string inStr(""), outStr, expStr;
	
	// empty input
	::UrlEncode(inStr, outStr);
	EXPECT_EQ(outStr, inStr) << "The url encode of " << inStr << ", was " << outStr << " is not correct";

	// RFC 3986 section 2.3 Unreserved Characters
	inStr.assign("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~");
	expStr = inStr;
	outStr.clear();
	::UrlEncode(inStr, outStr);
	EXPECT_EQ(outStr, expStr) << "The url encode of " << inStr << ", was " << outStr << " is not correct";
	
	//RFC 3986 section 2.2 Reserved Characters
	
	inStr.assign(" !#$&'()*+,/:;=?@[]");
	expStr.assign("%20%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D");
	outStr.clear();
	::UrlEncode(inStr, outStr);
	EXPECT_EQ(outStr, expStr) << "The url encode of " << inStr << ", was " << outStr << " is not correct";

	
#if 0
	// Binary data not supported
	inStr.assign("0f00000dbeef");
	outStr.empty();
	expStr.assign("%0f&00%00%0d%be%ef");
	::UrlEncode(inStr, outStr);
	EXPECT_EQ(outStr, expStr) << "The url encode of " << inStr << ", was " << outStr << " is not correct";
#endif
}


/* Reference wikipedia Uniform Resource Identifier
 A scheme component followed by a colon (:), consisting of a sequence of characters beginning with a letter and followed by any combination of letters, digits, plus (+), period (.), or hyphen (-). Although schemes are case-insensitive, the canonical form is lowercase and documents that specify schemes must do so with lowercase letters. Examples of popular schemes include http, https, ftp, mailto, file, data and irc. URI schemes should be registered with the Internet Assigned Numbers Authority (IANA), although non-registered schemes are used in practice.
 */
TEST(UrlEncDecSuite, parseUriDecode)
{
	const char *in1 = "", *out;
	// empty input
	out = ParseUriProtocol(in1);
	EXPECT_TRUE(out == NULL) << "The ParseUriProtocol " << in1 << ", was " << out << " is not correct";

	// we don't recognize scheme without // returns null
	const char *in2 = "1http:";
	out = ParseUriProtocol(in2);
	EXPECT_TRUE(out == NULL) << "The ParseUriProtocol " << in2 << ", was " << out << " is not correct";
	
	// return empty string
	const char *in3 = "1http://";
	out = ParseUriProtocol(in3);
	EXPECT_STREQ("", out) << "The ParseUriProtocol " << in3 << ", was " << out << " is not correct";
	
	// return "something"
	const char *in4 = "http://something";
	out = ParseUriProtocol(in4);
	EXPECT_STREQ("something", out) << "The ParseUriProtocol " << in4 << ", was " << out << " is not correct";
	
	// return "something" scheme has all valid characters
	const char *in5 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+.-://something";
	out = ParseUriProtocol(in5);
	EXPECT_STREQ("something", out) << "The ParseUriProtocol " << in5 << ", was " << out << " is not correct";
	
	const char invalids[] = {'!', '#', '$', '&', '\'', '(', ')',	'*', ',', ':', ';', '=', '?', '@', '[', ']'};
	
	for (int i=0; i < sizeof(invalids)/sizeof(char); i++)
	{
		// scheme has invalid character
		char in6[] = "http~://something";
		in6[4] = invalids[i];
		out = ParseUriProtocol(in6);
		EXPECT_TRUE(out == NULL)  << "The ParseUriProtocol " << in6 << ", was " << out << " is not correct";
	}
}
