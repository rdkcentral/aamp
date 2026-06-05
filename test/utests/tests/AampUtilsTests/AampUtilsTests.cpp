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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// unit under test
#include <AampUtils.cpp>

#include "MockCurl.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

class AampUtilsTests : public ::testing::Test
{
protected:
	CURL *mCurlEasyHandle = nullptr;
	//CURL *mCurlEasyHandle = nullptr;

	void SetUp() override
	{
		gpGlobalConfig =  new AampConfig();

		g_mockCurl = new MockCurl();

		mCurlEasyHandle = malloc(1);		// use a valid address for the handle
	}

	void TearDown() override
	{
		free(mCurlEasyHandle);

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockCurl;
		g_mockCurl = nullptr;
	}

};

//Test string & base64 encoded test string:
const char* teststr = "abcdefghijklmnopqrstuvwxyz01234567890!\"£$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ|\\<>?,./:;@'~#{}[]-_";
const char* b64_encoded_teststr = "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXowMTIzNDU2Nzg5MCEiwqMkJV4mKigpQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVp8XDw-PywuLzo7QCd-I3t9W10tXw";

TEST(_AampUtils, convertHMMSS )
{
	long long t = convertHHMMSSToTime("9912:34:56.789");
	EXPECT_EQ( t, 35685296789 );
	std::string rc = convertTimeToHHMMSS( t );
	EXPECT_STREQ( rc.c_str(), "9912:34:56.789" );
}

TEST(_AampUtils, aamp_GetCurrentTimeMS)
{
	long long result1 = aamp_GetCurrentTimeMS();
	long long result2 = aamp_GetCurrentTimeMS();
	EXPECT_GE(result2, result1);
}


TEST(_AampUtils, getDefaultHarvestPath)
{
	std::string value;
	getDefaultHarvestPath(value);
	EXPECT_EQ("/opt/aamp",value);
}


TEST(_AampUtils, aamp_ResolveURL)
{
	//This calls ParseUriProtocol - if( ParseUriProtocol(uri) ) returns false so tests the "else" branch
	std::string base = "https://example.net/dash264/TestCasesMCA/fraunhofer/xHE-AAC_Stereo/2/Sintel/sintel_audio_video_brs.mpd";
	const char* uri = "sintel_audio_video_brs-848x386-1500kbps_init.mp4";
	std::string dst;
	aamp_ResolveURL(dst, base, uri, true);
	EXPECT_STREQ(dst.c_str(), "https://example.net/dash264/TestCasesMCA/fraunhofer/xHE-AAC_Stereo/2/Sintel/sintel_audio_video_brs-848x386-1500kbps_init.mp4");

	//Make ParseUriProtocol succeed - for coverage.
	uri = "https://dashsintel_audio_video_brs-848x386-1500kbps_init.mp4?xx";
	aamp_ResolveURL(dst, base, uri, false);

	//Make ParseUriProtocol return false and cause baseParams to be set - for coverage.
	base += "/?123";
	uri = "sintel_audio_video_brs-848x386-1500kbps_init.mp4";
	aamp_ResolveURL(dst, base, uri, true);
}

TEST(_AampUtils, aamp_ResolveURL1)
{
	const char* uri;
	std::string dst;
	std::string base; 
	aamp_ResolveURL(dst, base, uri, true);
}
TEST(_AampUtils, aamp_IsAbsoluteURL)
{
	bool result;
	std::string url;
	url = "http://aaa.bbb.com";
	result = aamp_IsAbsoluteURL(url);
	EXPECT_TRUE(result);
	url = "https://aaa.bbb.com";
	result = aamp_IsAbsoluteURL(url);
	EXPECT_TRUE(result);
	url = "";
	result = aamp_IsAbsoluteURL(url);
	EXPECT_FALSE(result);
}


TEST(_AampUtils, aamp_getHostFromURL)
{
	std::string url = "https://lin021-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/BOX_SD_SU_SKYUK_1802_0_9103338152997639163.mpd";
	std::string host;
	host = aamp_getHostFromURL(url);
	EXPECT_STREQ(host.c_str(),"lin021-gb-s8-prd-ak.cdn01.skycdp.com");
	//http instead of https - for coverage
	url = "http://lin021-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/BOX_SD_SU_SKYUK_1802_0_9103338152997639163.mpd";
	host = aamp_getHostFromURL(url);
	EXPECT_STREQ(host.c_str(),"lin021-gb-s8-prd-ak.cdn01.skycdp.com");
}

TEST(_AampUtils, aamp_getHostFromURL1)
{
	std::string url = "invalid_url";
	std::string host = aamp_getHostFromURL(url);
	EXPECT_STREQ(host.c_str(), "");
}

TEST(_AampUtils, aamp_IsLocalHost)
{
	bool result;
	std::string hostName;
	hostName = "127.0.0.1";
	result = aamp_IsLocalHost(hostName);
	EXPECT_TRUE(result);
	hostName = "192.168.1.110";
	result = aamp_IsLocalHost(hostName);
	EXPECT_FALSE(result);
}


TEST(_AampUtils, StartsWith)
{
	bool result;
	result = aamp_StartsWith(teststr, "abcde");
	EXPECT_TRUE(result);
	result = aamp_StartsWith(teststr, "bcde");
	EXPECT_FALSE(result);
}


TEST(_AampUtils, aamp_Base64_URL_Encode)
{
	char* result;
	result = aamp_Base64_URL_Encode((const unsigned char*)teststr, strlen(teststr));
	EXPECT_STREQ(result, b64_encoded_teststr);
	free(result);
	//Test 0xff to '_' conversion for coverage
	const char* str= "\xff\xff\xff\xff";
	result = aamp_Base64_URL_Encode((const unsigned char*)str, strlen(str));
	for(int i = 0; i < 4; i++)
	{
		EXPECT_EQ(result[i], '_');		
	}
	free(result);
	
}


TEST(_AampUtils, aamp_Base64_URL_Decode)
{
	unsigned char* result;
	size_t len;
	result = aamp_Base64_URL_Decode(b64_encoded_teststr, &len, strlen(b64_encoded_teststr));
	EXPECT_EQ(len,strlen(teststr));
        // These aren't null terminated strings, must use memcmp
        int cmp = memcmp(result, teststr, len);
        EXPECT_EQ(cmp, 0);
	free(result);
	//Test '_' to 0xff conversion	for coverage
	const char* str= "______";
	result = aamp_Base64_URL_Decode(str, &len, strlen(str));
	for(int i = 0; i < len; i++)
	{
		EXPECT_EQ(result[i], 0xff);		
	}
	free(result);
}


TEST_F(AampUtilsTests, aamp_DecodeUrlParameter)
{
	std::string parameter_url ("encoded url");
	std::string decoded_url ("decoded url");

	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_unescape(mCurlEasyHandle, parameter_url.c_str(), parameter_url.length(), _))
		.WillOnce(DoAll(SetArgPointee<3>(static_cast<int>(decoded_url.length())), Return(const_cast<char*>(decoded_url.c_str()))));
	EXPECT_CALL(*g_mockCurl, curl_free(_));
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));

	aamp_DecodeUrlParameter(parameter_url);

	EXPECT_STREQ(parameter_url.c_str(), decoded_url.c_str());
}


TEST(_AampUtils, ISO8601DateTimeToUTCSeconds)
{
	double seconds;
	seconds = ISO8601DateTimeToUTCSeconds("1977-05-25T18:00:00.000Z");
	EXPECT_DOUBLE_EQ(seconds, 233431200.0);
	seconds = ISO8601DateTimeToUTCSeconds("2023-05-25T18:00:00.000Z");
	EXPECT_DOUBLE_EQ(seconds, 1685037600.0);
	seconds = ISO8601DateTimeToUTCSeconds("2023-05-25T19:00:00.000Z");
	EXPECT_DOUBLE_EQ(seconds, 1685041200.0);
	seconds = ISO8601DateTimeToUTCSeconds("2023-02-25T20:00:00.000Z");
	EXPECT_DOUBLE_EQ(seconds, 1677355200.0);
}


TEST(_AampUtils, aamp_PostJsonRPC)
{
	std::string result;
	std::string id="0";
	std::string method="0";
	std::string params="0";
	//This calls curl_easy_perform, which isn't expected to connect to a server so test for empty string
	result = aamp_PostJsonRPC( id, method, params);
	EXPECT_STREQ(result.c_str(), "");
}


TEST(_AampUtils, aamp_GetDeferTimeMs)
{
	int result = aamp_GetDeferTimeMs(1000);
	EXPECT_LE(result, 1000*1000);
}


TEST(_AampUtils, GetDrmSystem)
{
	auto result = GetDrmSystem(WIDEVINE_UUID);
	EXPECT_EQ(result, eDRM_WideVine);
	
	result = GetDrmSystem(PLAYREADY_UUID);
	EXPECT_EQ(result, eDRM_PlayReady);

	result = GetDrmSystem(CLEARKEY_UUID);
	EXPECT_EQ(result, eDRM_ClearKey);
	
	result = GetDrmSystem("");
	EXPECT_EQ(result, eDRM_NONE);
}



TEST(_AampUtils, GetDrmSystemName)
{
	const char* name;
	name = GetDrmSystemName(eDRM_WideVine);
	EXPECT_STREQ(name, "Widevine");
	name = GetDrmSystemName(eDRM_PlayReady);
	EXPECT_STREQ(name, "PlayReady");
	name = GetDrmSystemName(eDRM_CONSEC_agnostic);
	EXPECT_STREQ(name, "Consec Agnostic");
	name = GetDrmSystemName(eDRM_MAX_DRMSystems);
	EXPECT_STREQ(name, "");
}



TEST(_AampUtils, GetDrmSystemID)
{
	const char* id;
	id = GetDrmSystemID(eDRM_WideVine);
	EXPECT_STREQ(id, WIDEVINE_UUID);
	id = GetDrmSystemID(eDRM_PlayReady);
	EXPECT_STREQ(id, PLAYREADY_UUID);
	id = GetDrmSystemID(eDRM_ClearKey);
	EXPECT_STREQ(id, CLEARKEY_UUID);
	id = GetDrmSystemID(eDRM_CONSEC_agnostic);
	EXPECT_STREQ(id, CONSEC_AGNOSTIC_UUID);
	id = GetDrmSystemID(eDRM_MAX_DRMSystems);
	EXPECT_STREQ(id, "");
}

//UrlEncode test is in UrlEncDecTests.cpp 

TEST(_AampUtils, trim)
{
	std::string test;
	test = " abcdefghijkl";
	trim(test);
	EXPECT_STREQ(test.c_str(), "abcdefghijkl");
	test = " abcde fgh ijkl   ";
	trim(test);
	EXPECT_STREQ(test.c_str(), "abcde fgh ijkl");
	test = "abcdefghijkl";
	trim(test);
	EXPECT_STREQ(test.c_str(), "abcdefghijkl");
	test = "";
	trim(test);
	EXPECT_STREQ(test.c_str(), "");
}


// Getiso639map_NormalizeLanguageCode either copies lang passed in & returns it or passes it to iso639map_NormalizeLanguageCode.
// This either leaves it as is, or calls ConvertLanguage2to3 or ConvertLanguage3to2.
TEST(_AampUtils, Getiso639map_NormalizeLanguageCode)
{
	std::string result;
	std::string lang;
	
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_NO_LANGCODE_PREFERENCE);
	EXPECT_STREQ(result.c_str(), "");
	
	lang = "aa";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE);
	EXPECT_STREQ(result.c_str(), "aar");
	
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE);
	EXPECT_STREQ(result.c_str(), "aar");
	
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_2_CHAR_LANGCODE);
	EXPECT_STREQ(result.c_str(), "aa");
	
	lang = "aar";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_2_CHAR_LANGCODE);
	EXPECT_STREQ(result.c_str(), "aa");
	
	lang = "";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_2_CHAR_LANGCODE);
	EXPECT_STREQ(result.c_str(), "");
	
	lang = "English";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE);
	EXPECT_STREQ(result.c_str(), "eng");
	
	lang = "Xuponia";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE);
	EXPECT_STREQ(result.c_str(), "xup");
	
	lang = "Xuponia";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_3_CHAR_TERMINOLOGY_LANGCODE);
	EXPECT_STREQ(result.c_str(), "xup");
	
	lang = "Xuponia";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_2_CHAR_LANGCODE);
	EXPECT_STREQ(result.c_str(), "un");
	
	lang = "xX";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE);
	EXPECT_STREQ(result.c_str(), "und");
	
	lang = "xX";
	result = Getiso639map_NormalizeLanguageCode(lang, ISO639_PREFER_2_CHAR_LANGCODE);
	EXPECT_STREQ(result.c_str(), "xx");
}


TEST(_AampUtils, aamp_GetTimespec)
{
	timespec tm1, tm2;
	tm1 = aamp_GetTimespec(1);
	tm2 = aamp_GetTimespec(1000);
	EXPECT_GT(tm2.tv_sec, tm1.tv_sec);
}


TEST(_AampUtils, getHarvestConfigForMedia)
{
	HarvestConfigType harvestType[] = {eHARVEST_ENABLE_VIDEO, eHARVEST_ENABLE_INIT_VIDEO, eHARVEST_ENABLE_AUDIO, eHARVEST_ENABLE_INIT_AUDIO, eHARVEST_ENABLE_SUBTITLE,
		eHARVEST_ENABLE_INIT_SUBTITLE, eHARVEST_ENABLE_MANIFEST, eHARVEST_ENABLE_LICENCE, eHARVEST_ENABLE_IFRAME, eHARVEST_ENABLE_INIT_IFRAME,
		eHARVEST_ENABLE_PLAYLIST_VIDEO, eHARVEST_ENABLE_PLAYLIST_AUDIO, eHARVEST_ENABLE_PLAYLIST_SUBTITLE, eHARVEST_ENABLE_PLAYLIST_IFRAME,
		eHARVEST_ENABLE_DSM_CC, eHARVEST_DISABLE_DEFAULT, eHARVEST_DISABLE_DEFAULT};
	AampMediaType mediaType[] = {eMEDIATYPE_VIDEO, eMEDIATYPE_INIT_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_INIT_AUDIO, eMEDIATYPE_SUBTITLE,
		eMEDIATYPE_INIT_SUBTITLE, eMEDIATYPE_MANIFEST, eMEDIATYPE_LICENCE, eMEDIATYPE_IFRAME, eMEDIATYPE_INIT_IFRAME,
		eMEDIATYPE_PLAYLIST_VIDEO, eMEDIATYPE_PLAYLIST_AUDIO, eMEDIATYPE_PLAYLIST_SUBTITLE,	eMEDIATYPE_PLAYLIST_IFRAME,
		eMEDIATYPE_DSM_CC, eMEDIATYPE_IMAGE, eMEDIATYPE_DEFAULT};
		
	for(int i=0; i < ARRAY_SIZE(mediaType); i++)
	{
		int result = getHarvestConfigForMedia(mediaType[i]);
		EXPECT_EQ(result, harvestType[i]);
	}	
}


TEST(_AampUtils, aamp_WriteFile)
{
	bool result;
	std::string fileName = "http://aamp_utils_test";
	int count=10;
	AampMediaType mediaType = eMEDIATYPE_PLAYLIST_VIDEO;
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), mediaType, count, "prefix");
	EXPECT_TRUE(result);

	//For coverage, expect to be rejected because no "/" or"." in filename
	mediaType = eMEDIATYPE_MANIFEST;
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), mediaType, count, "prefix");
	EXPECT_FALSE(result);

	fileName +="/MANIFEST.EXT";
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), mediaType, count, "prefix");
	EXPECT_TRUE(result);

        //For coverage - attempt to create folder in "/"
        mediaType = eMEDIATYPE_PLAYLIST_VIDEO;
        result = aamp_WriteFile(fileName, teststr, strlen(teststr), mediaType, count, "/");
        auto me = getuid();
        if (me == 0) // i am (g)root
        {
            EXPECT_TRUE(result);
            std::remove(fileName.c_str());  // don't leave it if successful
        }
        else
        {
            EXPECT_FALSE(result);
        }
}
TEST(_AampUtils, aamp_WriteFile1)
{
	std::string fileName = "example.txt?param=value";
	AampMediaType filetype = eMEDIATYPE_MANIFEST;
	unsigned int count  = 1;
	bool result = aamp_WriteFile(fileName, teststr, strlen(teststr), filetype, count, "prefix");
	EXPECT_FALSE(result);
}
TEST(_AampUtils, aamp_WriteFile2)
{
	std::string fileName = "http://www.example.com/manifest.mpd";
    const char* data = "Manifest Data";
    size_t len = strlen(data);
    AampMediaType mediaType = eMEDIATYPE_MANIFEST;
    unsigned int count = 0;
    const char* prefix = "prefix_";

    bool result = aamp_WriteFile(fileName, data, len, mediaType, count, prefix);
	EXPECT_TRUE(result);
}

TEST(_AampUtils, getWorkingTrickplayRate)
{
	float rate[]        = { 4, 16, 32,  -4, -16, -32, 100};
	float workingrate[] = {25, 32, 48, -25, -32, -48, 100};
	for(int i = 0; i < ARRAY_SIZE(rate); i++)
	{
		float result = getWorkingTrickplayRate(rate[i]);
		EXPECT_DOUBLE_EQ(result, workingrate[i]);
	}
}


TEST(_AampUtils, getPseudoTrickplayRate)
{
	float pseudorate[] = { 4, 16, 32,  -4, -16, -32, 100};
	float rate[]       = {25, 32, 48, -25, -32, -48, 100};
	for(int i = 0; i < ARRAY_SIZE(rate); i++)
	{
		float result = getPseudoTrickplayRate(rate[i]);
		EXPECT_DOUBLE_EQ(result, pseudorate[i]);
	}
}


TEST(_AampUtils, stream2hex)
{
	std::string hexstr;	
	stream2hex(teststr, hexstr, true);
	EXPECT_STREQ(hexstr.c_str(), 		"6162636465666768696A6B6C6D6E6F707172737475767778797A30313233343536373839302122C2A324255E262A28294142434445464748494A4B4C4D4E4F505152535455565758595A7C5C3C3E3F2C2E2F3A3B40277E237B7D5B5D2D5F"); 
}


TEST(_AampUtils, mssleep)
{
	mssleep(1);
}


TEST(_AampUtils, GetAudioFormatStringForCodec)
{
	//N.B. GetAudioFormatStringForCodec returns the first matching string for the format. There may be other matching strings for the same format.
	const char* result;
	result = GetAudioFormatStringForCodec(FORMAT_INVALID);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_MPEGTS);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_ISO_BMFF);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_AUDIO_ES_MP3);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_AUDIO_ES_AAC);
	EXPECT_STREQ(result, "mp4a.40.2");
	result = GetAudioFormatStringForCodec(FORMAT_AUDIO_ES_AC3);
	EXPECT_STREQ(result, "ac-3");
	result = GetAudioFormatStringForCodec(FORMAT_AUDIO_ES_EC3);
	EXPECT_STREQ(result, "ec-3");
	result = GetAudioFormatStringForCodec(FORMAT_AUDIO_ES_ATMOS);
	EXPECT_STREQ(result, "ec+3");
	result = GetAudioFormatStringForCodec(FORMAT_AUDIO_ES_AC4);
	EXPECT_STREQ(result, "ac-4.02.01.01");
	result = GetAudioFormatStringForCodec(FORMAT_VIDEO_ES_H264);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_VIDEO_ES_HEVC);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_VIDEO_ES_MPEG2);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_SUBTITLE_WEBVTT);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_SUBTITLE_TTML);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_SUBTITLE_MP4);
	EXPECT_STREQ(result, "UNKNOWN");
	result = GetAudioFormatStringForCodec(FORMAT_UNKNOWN);
	EXPECT_STREQ(result, "UNKNOWN");
}


TEST(_AampUtils, GetAudioFormatForCodec)
{
	const FormatMap* result;
	result = GetAudioFormatForCodec("mp4a.40.2");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_AAC);
	result = GetAudioFormatForCodec("mp4a.40.5");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_AAC);
	result = GetAudioFormatForCodec("ac-3");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_AC3);
	result = GetAudioFormatForCodec("mp4a.a5");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_AC3);
	result = GetAudioFormatForCodec("ac-4.02.01.01");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_AC4);
	result = GetAudioFormatForCodec("ac-4.02.01.02");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_AC4);
	result = GetAudioFormatForCodec("ec-3");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_EC3);
	result = GetAudioFormatForCodec("ec+3");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_ATMOS);
	result = GetAudioFormatForCodec("eac3");
	EXPECT_EQ(result->format, FORMAT_AUDIO_ES_EC3);
	result = GetAudioFormatForCodec(nullptr);
	EXPECT_EQ(result, nullptr);
}


TEST(_AampUtils, GetVideoFormatForCodec)
{
	const FormatMap* result;
	result = GetVideoFormatForCodec("avc1.");
	EXPECT_EQ(result->format, FORMAT_VIDEO_ES_H264);	
	result = GetVideoFormatForCodec("hvc1.");
	EXPECT_EQ(result->format, FORMAT_VIDEO_ES_HEVC);	
	result = GetVideoFormatForCodec("hev1.");
	EXPECT_EQ(result->format, FORMAT_VIDEO_ES_HEVC);	
	result = GetVideoFormatForCodec("mpeg2v");
	EXPECT_EQ(result->format, FORMAT_VIDEO_ES_MPEG2);
	result = GetVideoFormatForCodec("");
	EXPECT_EQ(result, nullptr);
}

static void PrintableStdThreadHelper( size_t *out )
{
	*out = GetPrintableThreadID();
	sleep(1);
}
static void *PrintablePosixThreadHelper( void *arg )
{
	sleep(1);
	return NULL;
}
TEST(_AampUtils, GetPrintableThreadID)
{
	size_t internalThreadId = 0;
	std::thread myStdThread = std::thread( PrintableStdThreadHelper, &internalThreadId );
	size_t externalThreadId = GetPrintableThreadID(myStdThread);
	myStdThread.join();
	printf( "thread id: [%zx]\n", externalThreadId );
	EXPECT_EQ( internalThreadId, externalThreadId );
	
	pthread_t myPosixThread;
	int rc = pthread_create(&myPosixThread, NULL, PrintablePosixThreadHelper, NULL );
	EXPECT_EQ( rc, 0 );
	size_t posixThreadId = GetPrintableThreadID(myPosixThread);
	pthread_join( myPosixThread,NULL );
	printf( "posix thread id: [%zx]\n", posixThreadId );
	EXPECT_TRUE( posixThreadId!=0 );
}

TEST(_AampUtils, CRC32)
{
	/* 8 bytes of data followed by 4 bytes CRC32. */
	static const uint8_t data[12] =
	{
		0x01, 0x23, 0x45, 0x67,
		0x89, 0xab, 0xcd, 0xef,
		0x09, 0xee, 0xde, 0x06
	};
	uint32_t expected;
	uint32_t value;

	/* Test the CRC32 of the first 8 bytes. */
	expected = ((uint32_t)data[8] << 24) + 
			   ((uint32_t)data[9] << 16) +
			   ((uint32_t)data[10] << 8) +
			   ((uint32_t)data[11]);
	value = aamp_ComputeCRC32(data, 8);
	EXPECT_EQ(expected, value);

	/* Test the CRC32 of the full 12 bytes. */
	value = aamp_ComputeCRC32(data, 12);
	EXPECT_EQ(0, value);
}
TEST(_AampUtils, GetNetworkTimeTest1)
{
    std::string remoteUrl = "";
    int *http_error = nullptr;
    std::string NetworkProxy = "";
    double result = GetNetworkTime(remoteUrl, http_error , NetworkProxy);
	EXPECT_DOUBLE_EQ(result,0);
}
TEST(_AampUtils, GetNetworkTimeTest2)
{
	CURL *mCurlEasyHandle = nullptr;
	std::string mUrl = "https://some.server/manifest.mpd";
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	respData->iHttpRetValue = 204;
    std::string remoteUrl = "https://example.com";
    int http_error = 0;
    std::string NetworkProxy = "https://proxy.com";

	//respData->iHttpRetValue == 204;
    double result = GetNetworkTime(remoteUrl, &http_error , NetworkProxy);
}
TEST(_AampUtils, GetMediaTypeNameTest)
{
    AampMediaType mediaType[21] = {
    eMEDIATYPE_DEFAULT,
    eMEDIATYPE_VIDEO,
    eMEDIATYPE_AUDIO,
    eMEDIATYPE_SUBTITLE,
    eMEDIATYPE_AUX_AUDIO,
    eMEDIATYPE_MANIFEST,
    eMEDIATYPE_LICENCE,
    eMEDIATYPE_IFRAME,
    eMEDIATYPE_INIT_VIDEO,
    eMEDIATYPE_INIT_AUDIO,
    eMEDIATYPE_INIT_SUBTITLE,
    eMEDIATYPE_INIT_AUX_AUDIO,
    eMEDIATYPE_PLAYLIST_VIDEO,
    eMEDIATYPE_PLAYLIST_AUDIO,
    eMEDIATYPE_PLAYLIST_SUBTITLE,
    eMEDIATYPE_PLAYLIST_AUX_AUDIO,
    eMEDIATYPE_PLAYLIST_IFRAME,
    eMEDIATYPE_INIT_IFRAME,
    eMEDIATYPE_DSM_CC,
    eMEDIATYPE_IMAGE,
    eMEDIATYPE_DEFAULT
    };

	for(int i=0; i<=eMEDIATYPE_DEFAULT; i++){
		const char* type = GetMediaTypeName(mediaType[i]);
	}
}

TEST(_AampUtils, ParseISO8601DurationTest1)
{
	const char* duration = "InvalidDuration";
	double result = ParseISO8601Duration(duration);
	EXPECT_DOUBLE_EQ(result,0.0);
}
TEST(_AampUtils, ParseISO8601DurationTest2)
{
	const char* duration = "P2Y3M4DT5H30M15.5S";
	double result = ParseISO8601Duration(duration);
}
TEST(_AampUtils, ParseISO8601DurationTest3)
{
	const char* duration = "PT3H30M15.5S";
	double result = ParseISO8601Duration(duration);
}

TEST(_AampUtils, GetConfigPath1)
{
	//L1 tests build without #define AAMP_SIMULATOR_BUILD
	//so paths modified as with HW build I.E they are not modified
	std::string rtn;
	rtn = aamp_GetConfigPath("/opt/abc.cfg");
	EXPECT_EQ("/opt/abc.cfg", rtn );

	setenv("AAMP_ENABLE_OPT_OVERRIDE","1",1);   //Should not change result because AAMP_CPC not defined
	rtn = aamp_GetConfigPath("/opt/abc.cfg");
	EXPECT_EQ("/opt/abc.cfg", rtn );
}


// Tests the parseAndValidateSCTE35 utility function with different scte35 signals
// The function is expected to return true for valid signals and false for invalid signals
// This test covers DISTRIBUTOR_PLACEMENT_OPPORTUNITY_START
TEST(_AampUtils, parseAndValidateSCTE35_1)
{
	std::string scte35 = "/DBLAABmGpRcAAAABQb+AAAAAAA1AjNDVUVJABs/hn//AABSZcAJH1NJR05BTDpWUmctLWdCRWRkbzQ3UThZOHF1QVlRQUE2AAD/6DMz";
	bool result = parseAndValidateSCTE35(scte35);
	EXPECT_TRUE(result);
}

// Tests the parseAndValidateSCTE35 utility function with different scte35 signals
// The function is expected to return true for valid signals and false for invalid signals
// This test covers PROVIDER_ADVERTISEMENT_START
TEST(_AampUtils, parseAndValidateSCTE35_2)
{
	std::string scte35 = "/DB3AACRDm31AP/wBQb++u5TsABhAl9DVUVJAABhSH/AAAANu6ANSw4pYXZhaWxpZD04OTc1NTc3OTkmYml0bWFwPSZpbmFjdGl2aXR5PTM0ODAPHnVybjpjb21jYXN0OmFsdGNvbjphZGRyZXNzYWJsZTAAAC2N6xw=";
	bool result = parseAndValidateSCTE35(scte35);
	EXPECT_TRUE(result);
}

// Tests the parseAndValidateSCTE35 utility function with different scte35 signals
// The function is expected to return true for valid signals and false for invalid signals
// This test covers DISTRIBUTOR_PLACEMENT_OPPORTUNITY_START with sub_segment_num and sub_segment_expected
TEST(_AampUtils, parseAndValidateSCTE35_3)
{
	std::string scte35 = "/ABNAAAAAAAAAAAABQb+AAAAAAA3AjVDVUVJGB0TN3//AABSZcAJH1NJR05BTDpKczhnN0FETXJXZy1FeDBZYkxHY21BQUE2AAAAAGy2Xpc=";
	bool result = parseAndValidateSCTE35(scte35);
	EXPECT_TRUE(result);
}

TEST(_AampUtils, strstr )
{
	const char *haystack_ptr = "bob is my name. you can call me bobby bob";
	const char *haystack_fin = haystack_ptr + strlen(haystack_ptr);
	EXPECT_TRUE( mystrstr(haystack_ptr, haystack_fin,"xxx") == NULL );
	EXPECT_TRUE( mystrstr(haystack_ptr, haystack_fin,"bob") == &haystack_ptr[0] );
	EXPECT_TRUE( mystrstr(haystack_ptr, haystack_fin, "name") == &haystack_ptr[10]);
	EXPECT_TRUE( mystrstr(haystack_ptr+10,haystack_fin,"bob") == &haystack_ptr[32] );
	EXPECT_TRUE( mystrstr(haystack_ptr+34,haystack_fin,"bob") == &haystack_ptr[38] );
	EXPECT_TRUE( mystrstr(haystack_ptr,&haystack_ptr[14],"name") == &haystack_ptr[10] );
	EXPECT_TRUE( mystrstr(haystack_ptr,&haystack_ptr[13],"name") == NULL );
	EXPECT_TRUE( mystrstr(haystack_ptr,&haystack_ptr[9],"is") == &haystack_ptr[4] );
}

