#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <AampUtils.cpp>

// Fakes to allow linkage
AampConfig *gpGlobalConfig=NULL;
AampLogManager *mLogObj=NULL;

//Test string & base64 encoded test string:
const char* teststr = "abcdefghijklmnopqrstuvwxyz01234567890!\"£$%^&*()ABCDEFGHIJKLMNOPQRSTUVWXYZ|\\<>?,./:;@'~#{}[]-_";
const char* b64_encoded_teststr = "YWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXowMTIzNDU2Nzg5MCEiwqMkJV4mKigpQUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVp8XDw-PywuLzo7QCd-I3t9W10tXw";

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
	EXPECT_FALSE(value.length() == 0);
}


TEST(_AampUtils, aamp_ResolveURL)
{
	//This calls ParseUriProtocol - if( ParseUriProtocol(uri) ) returns false so tests the "else" branch
	std::string base = "https://dash.akamaized.net/dash264/TestCasesMCA/fraunhofer/xHE-AAC_Stereo/2/Sintel/sintel_audio_video_brs.mpd";
	const char* uri = "sintel_audio_video_brs-848x386-1500kbps_init.mp4";
	std::string dst;
	aamp_ResolveURL(dst, base, uri, true);
	EXPECT_STREQ(dst.c_str(), "https://dash.akamaized.net/dash264/TestCasesMCA/fraunhofer/xHE-AAC_Stereo/2/Sintel/sintel_audio_video_brs-848x386-1500kbps_init.mp4");

	//Make ParseUriProtocol succeed - for coverage.
	uri = "https://dashsintel_audio_video_brs-848x386-1500kbps_init.mp4?xx";
	aamp_ResolveURL(dst, base, uri, false);

	//Make ParseUriProtocol return false and cause baseParams to be set - for coverage.
	base += "/?123";
	uri = "sintel_audio_video_brs-848x386-1500kbps_init.mp4";
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
	EXPECT_STREQ((char*)result, teststr);
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


TEST(_AampUtils, aamp_DecodeUrlParameter)
{
	std::string test1, test2;
	test1 = "https%3A%2F%2Flin021-gb-s8-prd-ak.cdn01.skycdp.com%2Fv1%2Ffrag%2Fbmff%2Fenc%2Fcenc%2Ft%2FBOX_SD_SU_SKYUK_1802_0_9103338152997639163.mpd";
	aamp_DecodeUrlParameter(test1);
	//If this is built with the fake curl implementation, curl_easy_init returns false. A real curl implementation should return the following:
	EXPECT_STREQ(test1.c_str(), "https://lin021-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/BOX_SD_SU_SKYUK_1802_0_9103338152997639163.mpd");
	//EXPECT_STREQ(test1.c_str(), test1.c_str());
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


TEST(_AampUtils, MyRpcWriteFunction)
{
	std::string context = "context";
	char buffer[] = {"buffer"};
	int numBytes = MyRpcWriteFunction((void*)buffer, 5, 7,(void*)&context);
	EXPECT_EQ(numBytes, 35);
	EXPECT_STREQ(context.c_str(), "contextbuffer");
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
	HarvestConfigType harvestType[] = {eHARVEST_ENAABLE_VIDEO, eHARVEST_ENAABLE_INIT_VIDEO, eHARVEST_ENAABLE_AUDIO, eHARVEST_ENAABLE_INIT_AUDIO, eHARVEST_ENAABLE_SUBTITLE,
		eHARVEST_ENAABLE_INIT_SUBTITLE, eHARVEST_ENAABLE_MANIFEST, eHARVEST_ENAABLE_LICENCE, eHARVEST_ENAABLE_IFRAME, eHARVEST_ENAABLE_INIT_IFRAME,
		eHARVEST_ENAABLE_PLAYLIST_VIDEO, eHARVEST_ENAABLE_PLAYLIST_AUDIO, eHARVEST_ENAABLE_PLAYLIST_SUBTITLE, eHARVEST_ENAABLE_PLAYLIST_IFRAME,
		eHARVEST_ENAABLE_DSM_CC, eHARVEST_DISABLE_DEFAULT, eHARVEST_DISABLE_DEFAULT};
	MediaType fileType[] = {eMEDIATYPE_VIDEO, eMEDIATYPE_INIT_VIDEO, eMEDIATYPE_AUDIO, eMEDIATYPE_INIT_AUDIO, eMEDIATYPE_SUBTITLE,
		eMEDIATYPE_INIT_SUBTITLE, eMEDIATYPE_MANIFEST, eMEDIATYPE_LICENCE, eMEDIATYPE_IFRAME, eMEDIATYPE_INIT_IFRAME,
		eMEDIATYPE_PLAYLIST_VIDEO, eMEDIATYPE_PLAYLIST_AUDIO, eMEDIATYPE_PLAYLIST_SUBTITLE,	eMEDIATYPE_PLAYLIST_IFRAME,
		eMEDIATYPE_DSM_CC, eMEDIATYPE_IMAGE, eMEDIATYPE_DEFAULT};
		
	for(int i=0; i < ARRAY_SIZE(fileType); i++)
	{
		int result = getHarvestConfigForMedia(fileType[i]);
		EXPECT_EQ(result, harvestType[i]);
	}	
}


TEST(_AampUtils, aamp_WriteFile)
{
	bool result;
	std::string fileName = "http://aamp_utils_test";
	int count=10;
	MediaType fileType = eMEDIATYPE_PLAYLIST_VIDEO;
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), fileType, count, "prefix");
	EXPECT_TRUE(result);
	//This test currently commented out as it depends on delia-60598
	/*
	//For coverage, expect to be rejected because no "/" or"." in filename
	fileType = eMEDIATYPE_MANIFEST;
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), fileType, count, "prefix");
	EXPECT_FALSE(result);
	*/
	fileName +="/MANIFEST.EXT";
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), fileType, count, "prefix");
	EXPECT_TRUE(result);
	//For coverage - attempt to create folder in "/"
	fileType = eMEDIATYPE_PLAYLIST_VIDEO;
	result = aamp_WriteFile(fileName, teststr, strlen(teststr), fileType, count, "/");
	EXPECT_FALSE(result);
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


TEST(_AampUtils, GetPrintableThreadID)
{
	const std::thread t1, t2;
	size_t result1 = GetPrintableThreadID(t1);
	size_t result2 = GetPrintableThreadID(t2);
	EXPECT_EQ(result1, result2);
}


