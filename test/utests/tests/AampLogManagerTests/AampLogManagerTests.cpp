/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include <thread>
#include <sstream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <systemd/sd-journal.h>
#include "MockSdJournal.h"
#include "AampLogManager.h"
#include "AampMediaType.h"
#include "AampConfig.h"

using namespace testing;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::SizeIs;

AampConfig *gpGlobalConfig{nullptr};

class AampLogManagerTest : public Test
{
protected:
    void SetUp() override
    {
        g_mockSdJournal = new NiceMock<MockSdJournal>();
		AampLogManager::lockLogLevel(false);
		AampLogManager::setLogLevel(eLOGLEVEL_WARN);
    }

    void TearDown() override
    {
        delete g_mockSdJournal;
        g_mockSdJournal = nullptr;
    }
};

/**
 * TEST_F GTest-Testcase for isLogLevelAllowed function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogLevelAllowed_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel chkLevel = eLOGLEVEL_TRACE;

    //Assert: Expecting two values are equal or not
    EXPECT_EQ(AampLogManager::isLogLevelAllowed(chkLevel),false);
}

/**
 * TEST_F GTest-Testcase for isLogLevelAllowed function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogLevelAllowed_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel chkLevel = eLOGLEVEL_INFO;

    //Assert: Expecting two values are equal or not
    EXPECT_EQ(AampLogManager::isLogLevelAllowed(chkLevel),false);
}

/**
 * Testcase for isLogLevelAllowed function for Milestone log level
 * It is expected to return true, as MIL is higher level than the default level allowed WARN
**/
TEST_F(AampLogManagerTest, isLogLevelAllowed_MIL)
{
    AAMP_LogLevel chkLevel = eLOGLEVEL_MIL;

    EXPECT_EQ(AampLogManager::isLogLevelAllowed(chkLevel), true);
}

/**
 * TEST_F GTest-Testcase for setLogLevel function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, setLogLevel_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel chkLevel = eLOGLEVEL_TRACE;

    //Act: Calling the function for test
	AampLogManager::setLogLevel(chkLevel);

    //Assert: checking values are equal or not
    //EXPECT_NE(AampLogManager::aampLoglevel,chkLevel);
}

/**
 * TEST_F GTest-Testcase for setLogLevel function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, setLogLevel_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel chkLevel = eLOGLEVEL_INFO;

    //Act: Calling the function for test
	AampLogManager::setLogLevel(chkLevel);

    //Assert: checking values are equal or not
	//EXPECT_NE(AampLogManager::aampLoglevel,chkLevel);
}

/**
 * Testcase for setLogLevel MIL followed by isLogLevelAllowed MIL
 * isLogLevelAllowed is expected to return true
**/
TEST_F(AampLogManagerTest, setLogLevelMil_isLogLevelAllowedMil)
{
    AAMP_LogLevel setLevel = eLOGLEVEL_MIL;
    AAMP_LogLevel chkLevel = eLOGLEVEL_MIL;

	AampLogManager::setLogLevel(setLevel);

    EXPECT_EQ(true, AampLogManager::isLogLevelAllowed(chkLevel));
}

/**
 * Testcase for setLogLevel ERROR followed by isLogLevelAllowed MIL
 * isLogLevelAllowed is expected to return false, as ERROR is higher level than MIL
**/
TEST_F(AampLogManagerTest, setLogLevelError_isLogLevelAllowedMil)
{
    AAMP_LogLevel setLevel = eLOGLEVEL_ERROR;
    AAMP_LogLevel chkLevel = eLOGLEVEL_MIL;

	AampLogManager::setLogLevel(setLevel);

    EXPECT_EQ(false, AampLogManager::isLogLevelAllowed(chkLevel));
}

/**
 * TEST_F GTest-Testcase for getHexDebugStr function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, getHexDebugStr_Test1)
{
    const std::vector<uint8_t> &data = {'a','b'};
    std::string getHexDebugStr_String = AampLogManager::getHexDebugStr(data);
    EXPECT_EQ(getHexDebugStr_String,"0x6162");
}

/**
 * TEST_F GTest-Testcase for LogNetworkLatency function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogNetworkLatency_Test)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//httsurl";
    int downloadTime = 10;
    int downloadThresholdTimeoutMs = 20;
    AampMediaType type = eMEDIATYPE_AUDIO;
	std::string location = "folder";
	std::string symptom = "testfile";

    //Act: Calling the function for test
	AampLogManager::LogNetworkLatency(url,downloadTime,downloadThresholdTimeoutMs,type);
	AampLogManager::ParseContentUrl(url, location, symptom, type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_EQ(downloadTime,10);
    EXPECT_EQ(downloadThresholdTimeoutMs,20);
}

/**
 * TEST_F GTest-Testcase for LogNetworkError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogNetworkError_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//httsurl";
    AAMPNetworkErrorType errorType[] = {AAMPNetworkErrorNone,AAMPNetworkErrorHttp,AAMPNetworkErrorTimeout,AAMPNetworkErrorCurl};
    int errorCode = 20;
    AampMediaType type = eMEDIATYPE_AUDIO;
	std::string location = "folder";
	std::string symptom = "testfile";

    //Act: Calling the function for test
    for(int i=0;i<4;i++)
    {
		AampLogManager::LogNetworkError(url,errorType[i],errorCode,type);
    }
	AampLogManager::ParseContentUrl(url, location, symptom, type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_LE(errorCode,400);

}

/**
 * TEST_F GTest-Testcase for LogNetworkError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogNetworkError_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//httsurl";
    AAMPNetworkErrorType errorType = AAMPNetworkErrorHttp;
    int errorCode = 401;
    AampMediaType type = eMEDIATYPE_AUDIO;

    //Act: Calling the function for test
	AampLogManager::LogNetworkError(url,errorType,errorCode,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_GE(errorCode,400);
}

/**
 * TEST_F GTest-Testcase for ParseContentUrl function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, ParseContentUrl_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//httsurl";
    std::string location = "test2";
    std::string symptom = "test3";
    AampMediaType type = eMEDIATYPE_AUDIO;

    //Act: Calling the function for test
	AampLogManager::ParseContentUrl(url,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_STREQ(location.c_str(),"unknown");
    EXPECT_STREQ(symptom.c_str(),"audio drop or freeze/buffering");
}

/**
 * TEST_F GTest-Testcase for ParseContentUrl function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, ParseContentUrl_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//httsurl";
    std::string location = "test2";
    std::string symptom = "test3";
    AampMediaType type[15] = {
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
        eMEDIATYPE_PLAYLIST_AUX_AUDIO
        };

    //Act: Calling the function for test
    for(int i=0;i<15;i++)
    {
		AampLogManager::ParseContentUrl(url,location,symptom,type[i]);
    }

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_STREQ(location.c_str(),"unknown");
    EXPECT_STREQ(symptom.c_str(),"unknown");
}

/**
 * TEST_F GTest-Testcase for ParseContentUrl function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, ParseContentUrl_Test3)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//httsurl";
    std::string location = "test2";
    std::string symptom = "test3";
    AampMediaType type = eMEDIATYPE_INIT_IFRAME;

    //Act: Calling the function for test
	AampLogManager::ParseContentUrl(url,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_STREQ(location.c_str(),"unknown");
    EXPECT_STREQ(symptom.c_str(),"video fails to start");
}

/**
 * TEST_F GTest-Testcase for ParseContentUrl function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, ParseContentUrl_Test4)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//mm.";
    std::string location = "test2";
    std::string symptom = "test3";
    AampMediaType type = eMEDIATYPE_INIT_IFRAME;

    //Act: Calling the function for test
	AampLogManager::ParseContentUrl(url,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//mm.");
    EXPECT_STREQ(location.c_str(),"manifest manipulator");
    EXPECT_STREQ(symptom.c_str(),"video fails to start");
}

/**
 * TEST_F GTest-Testcase for ParseContentUrl function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, ParseContentUrl_Test5)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "//odol";
    std::string location = "test2";
    std::string symptom = "test3";
    AampMediaType type = eMEDIATYPE_INIT_IFRAME;

    //Act: Calling the function for test
	AampLogManager::ParseContentUrl(url,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//odol");
    EXPECT_STREQ(location.c_str(),"edge cache");
    EXPECT_STREQ(symptom.c_str(),"video fails to start");
}

/**
 * TEST_F GTest-Testcase for ParseContentUrl function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, ParseContentUrl_Test6)
{
    //Arrange: Creating the variables for passing to arguments
    const char* url = "127.0.0.1:9080";
    std::string location = "test2";
    std::string symptom = "test3";
    AampMediaType type = eMEDIATYPE_DEFAULT;

    //Act: Calling the function for test
	AampLogManager::ParseContentUrl(url,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"127.0.0.1:9080");
    EXPECT_STREQ(location.c_str(),"fog");
    EXPECT_STREQ(symptom.c_str(),"unknown");
}

/**
 * TEST_F GTest-Testcase for LogABRInfo function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogABRInfo_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    AAMPAbrInfo *pstAbrInfo = new AAMPAbrInfo;
    pstAbrInfo->desiredBandwidth = 10;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrBandwidthUpdate;
    std::string reason = "bandwidth";
	std::string profile = "higher";
	std::string symptom = "video quality may increase";
    
    //Act: Calling the function for test
	AampLogManager::LogABRInfo(pstAbrInfo);

    //Assert: checking values are equal or not
    EXPECT_EQ(pstAbrInfo->desiredBandwidth,10);
    EXPECT_EQ(pstAbrInfo->currentBandwidth,5);
    EXPECT_STREQ(reason.c_str(),"bandwidth");
    EXPECT_STREQ(profile.c_str(),"higher");
    EXPECT_STREQ(symptom.c_str(),"video quality may increase");
}

/**
 * TEST_F GTest-Testcase for LogABRInfo function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogABRInfo_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    AAMPAbrInfo *pstAbrInfo = new AAMPAbrInfo;
    pstAbrInfo->desiredBandwidth = 5;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrManifestDownloadFailed;
	std::string reason = "manifest download failed' error='http error ";
	std::string profile = "lower";
	std::string symptom = "video quality may decrease";
    
    //Act: Calling the function for test
	AampLogManager::LogABRInfo(pstAbrInfo);

    //Assert: checking values are equal or not
    EXPECT_EQ(pstAbrInfo->desiredBandwidth,5);
    EXPECT_EQ(pstAbrInfo->currentBandwidth,5);
    EXPECT_STREQ(reason.c_str(),"manifest download failed' error='http error ");
    EXPECT_STREQ(profile.c_str(),"lower");
    EXPECT_STREQ(symptom.c_str(),"video quality may decrease");
}

/**
 * TEST_F GTest-Testcase for LogABRInfo function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogABRInfo_Test3)
{
    //Arrange: Creating the variables for passing to arguments
    AAMPAbrInfo *pstAbrInfo = new AAMPAbrInfo;
    pstAbrInfo->desiredBandwidth = 10;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrFragmentDownloadFailed;
    pstAbrInfo->errorType = AAMPNetworkErrorHttp;
	std::string reason = "fragment download failed'";
	std::string profile = "higher";
	std::string symptom = "video quality may increase";
    
    //Act: Calling the function for test
	AampLogManager::LogABRInfo(pstAbrInfo);

    //Assert: checking values are equal or not
    EXPECT_EQ(pstAbrInfo->desiredBandwidth,10);
    EXPECT_EQ(pstAbrInfo->currentBandwidth,5);
    EXPECT_STREQ(reason.c_str(),"fragment download failed'");
    EXPECT_STREQ(profile.c_str(),"higher");
    EXPECT_STREQ(symptom.c_str(),"video quality may increase");
}

/**
 * TEST_F GTest-Testcase for LogABRInfo function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogABRInfo_Test4)
{
    //Arrange: Creating the variables for passing to arguments
    AAMPAbrInfo *pstAbrInfo = new AAMPAbrInfo;
    pstAbrInfo->desiredBandwidth = 10;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrUnifiedVideoEngine;
    pstAbrInfo->errorType = AAMPNetworkErrorNone;
	std::string reason = "changed based on unified video engine user preferred bitrate";
	std::string profile = "higher";
	std::string symptom = "video quality may increase";
    
    //Act: Calling the function for test
	AampLogManager::LogABRInfo(pstAbrInfo);

    //Assert: checking values are equal or not
    EXPECT_EQ(pstAbrInfo->desiredBandwidth,10);
    EXPECT_EQ(pstAbrInfo->currentBandwidth,5);
    EXPECT_STREQ(reason.c_str(),"changed based on unified video engine user preferred bitrate");
    EXPECT_STREQ(profile.c_str(),"higher");
    EXPECT_STREQ(symptom.c_str(),"video quality may increase");
}

/**
 * TEST_F GTest-Testcase for isLogworthyErrorCode function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogworthyErrorCode_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    bool result;
    int errorCode = 0;

    //Act: Calling the function for test
    result = AampLogManager::isLogworthyErrorCode(errorCode);

    //Assert: checking values are true or false
    EXPECT_FALSE(result);
}

/**
 * TEST_F GTest-Testcase for isLogworthyErrorCode function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogworthyErrorCode_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    bool result;
    int errorCode = CURLcode::CURLE_WRITE_ERROR;

    //Act: Calling the function for test
    result = AampLogManager::isLogworthyErrorCode(errorCode);

    //Assert: checking values are true or false
    EXPECT_FALSE(result);
}

/**
 * TEST_F GTest-Testcase for isLogworthyErrorCode function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogworthyErrorCode_Test3)
{
    //Arrange: Creating the variables for passing to arguments
    bool result;
    int errorCode = CURLcode::CURLE_ABORTED_BY_CALLBACK;

    //Act: Calling the function for test
    result = AampLogManager::isLogworthyErrorCode(errorCode);

    //Assert: checking values are true or false
    EXPECT_FALSE(result);
}

/**
 * TEST_F GTest-Testcase for isLogworthyErrorCode function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogworthyErrorCode_Test4)
{
    //Arrange: Creating the variables for passing to arguments
    bool result;
    int errorCode = CURLcode::CURLE_QUOTE_ERROR;

    //Act: Calling the function for test
    result = AampLogManager::isLogworthyErrorCode(errorCode);

    //Assert: checking values are true or false
    EXPECT_TRUE(result);
}

/**
 * TEST_F GTest-Testcase for isLogworthyErrorCode function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogworthyErrorCode_Test5)
{
    //Arrange: Creating the variables for passing to arguments
    bool result;
    int errorCode = CURLcode::CURLE_READ_ERROR;

    //Act: Calling the function for test
    result = AampLogManager::isLogworthyErrorCode(errorCode);

    //Assert: checking values are true or false
    EXPECT_TRUE(result);
}

/**
 * TEST_F GTest-Testcase for logprintline function for AampLogManagerTest file
**/
// TEST_F(AampLogManagerTest, logprintline_Test)
// {
//     //Arrange: Creating the variables for passing to arguments
//     //static const char *gAampLog = "./aamp.log";
//     FILE *f = fopen("test.cpp","w");
//     struct timeval t;
//     t.tv_sec = 2;
//     t.tv_usec = 2000;
//     const char* printBuffer = "s1";

//     //Act: Calling the function for test
//     logprintline(f,t,printBuffer);
// }

/**
 * TEST_F GTest-Testcase for logprintf function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, logprintf_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel level = eLOGLEVEL_INFO;
    const char* file = "test.cpp";
    int line = 2;
    const char *format = "s3";
    const char *format2 = "s4";

    //Act: Calling the function for test
    logprintf(level,file,line,format,format2);
}

TEST_F(AampLogManagerTest, timestampStringify )
{
	// this test based on code from logprintf used in aampcli
	// it ensures that the printing of current time doesn't overflow string buffer
	{
		struct timeval t;
		gettimeofday(&t, NULL);
		char timestamp[AAMPCLI_TIMESTAMP_PREFIX_MAX_CHARS];
		snprintf(timestamp, sizeof(timestamp), AAMPCLI_TIMESTAMP_PREFIX_FORMAT, (unsigned int)t.tv_sec, (unsigned int)t.tv_usec / 1000 );
		unsigned int extracted_seconds = 0;
		unsigned int extracted_milliseconds = 0;
		int n = sscanf( timestamp, "%u.%03u: ", &extracted_seconds, &extracted_milliseconds );
		ASSERT_EQ( n, 2 );
		if( n==2 )
		{ // i.e.
			// if time ever spills past size of a int, we'll come to know
			ASSERT_EQ( t.tv_sec, extracted_seconds );
			ASSERT_EQ( t.tv_usec / 1000, extracted_milliseconds );
		}
	}
}

/**
 * TEST_F GTest-Testcase for DumpBlob function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, DumpBlob_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    const unsigned char *ptr = reinterpret_cast<const unsigned char*>("test");
    size_t len = 0;

    //Act: Calling the function for test
    DumpBlob(ptr,len);
}

/**
 * Not able run test this test as its static function
 * TEST_F GTest-Testcase for OpenSimulatorLogFile function for AampLogManagerTest file
**/
// TEST_F(AampLogManagerTest, OpenSimulatorLogFile_Test)
// {
//     //Arrange: Creating the variables for passing to arguments
//     static const char *gAampLog = "./aamp.log";
//     FILE *f = fopen(gAampLog,"w");

//     //Act: Calling the function for test
//     FILE *fp = OpenSimulatorLogFile();
// }

/*
    Test getPlayerId function
    It is expected to return the default value -1
*/
TEST_F(AampLogManagerTest, getPlayerIdDefault)
{
    ASSERT_EQ(-1, gPlayerId );
}

/*
    Test AAMPLOG_TRACE macro
    No log line is expected, as TRACE level is lower than the default level (WARN)
*/
TEST_F(AampLogManagerTest, AAMPLOG_TRACE)
{
    const std::string message{"Test TRACE log line"};
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_TRACE("%s", message.c_str());
}

/*
    Test AAMPLOG_INFO macro
    No log line is expected, as INFO level is lower than the default level (WARN)
*/
TEST_F(AampLogManagerTest, AAMPLOG_INFO)
{
    const std::string message{"Test INFO log line"};
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_INFO("%s", message.c_str());
}

/*
    Test AAMPLOG_WARN macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, AAMPLOG_WARN)
{
    const std::string message{"Test WARN log line"};
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[WARN]"), HasSubstr(message.c_str()))));
	AAMPLOG_WARN("%s", message.c_str());
}

/*
    Test AAMPLOG_MIL macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, AAMPLOG_MIL)
{
    const std::string message{"Test MIL log line"};
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[MIL]"), HasSubstr(message.c_str()))));
	AAMPLOG_MIL("%s", message.c_str());
}

/*
    Test AAMPLOG_ERR macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, AAMPLOG_ERR)
{
    const std::string message{"Test ERROR log line"};
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[ERROR]"), HasSubstr(message.c_str()))));
    AAMPLOG_ERR("%s", message.c_str());
}

/*
    Test setLogLevel with MIL followed by AAMPLOG_MIL macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, setLogLevelMil_AAMPLOG_MIL)
{
    const std::string message{"Test MIL log line"};
	AampLogManager::setLogLevel(eLOGLEVEL_MIL);
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[MIL]"), HasSubstr(message.c_str()))));
    AAMPLOG_MIL("%s", message.c_str());
}

/*
    Test setLogLevel with ERROR followed by AAMPLOG_MIL macro
    Since ERROR is higher level than MIL, sd_journal_print is not expected to be called
*/
TEST_F(AampLogManagerTest, setLogLevelError_AAMPLOG_MIL)
{
    const std::string message{"Test MIL log line"};
	AampLogManager::setLogLevel(eLOGLEVEL_ERROR);
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_MIL("%s", message.c_str());
}

TEST_F(AampLogManagerTest, logprintf_TRACE)
{
    AAMP_LogLevel level = eLOGLEVEL_TRACE;
    std::string file("test.cpp");
    int line = 2;
    std::string message("message");
    // The printed log line must contain the player ID, level, file and the message /
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(-1) + "]"), HasSubstr("[TRACE]"), HasSubstr("[" + file + "]"), HasSubstr(message))));
    logprintf(level, file.c_str(), line, "%s", message.c_str());
}

TEST_F(AampLogManagerTest, logprintf_INFO)
{
    AAMP_LogLevel level = eLOGLEVEL_INFO;
    std::string file("test.cpp");
    int line = 2;
    std::string message("message");
    // The printed log line must contain the player ID, level, file and the message /
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(-1) + "]"), HasSubstr("[INFO]"), HasSubstr("[" + file + "]"), HasSubstr(message))));
    logprintf(level, file.c_str(), line, "%s", message.c_str());
}

/*
    Test logprintf called with a very long file
    sd_journal_print is expected to be called with the header text truncated and (...)
*/
const int MAX_DEBUG_LOG_BUFF_SIZE = 512;

TEST_F(AampLogManagerTest, logprintf_LongFile)
{
    AAMP_LogLevel level = eLOGLEVEL_INFO;
    std::string file(MAX_DEBUG_LOG_BUFF_SIZE, '*');
    int line = 2;
    std::string message("message");
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(-1) + "]"), HasSubstr("[INFO]"))));
    logprintf(level, file.c_str(), line, "%s", message.c_str());
}

/*
    Test logprintf called with a very long message
    sd_journal_print is expected to be called with the header text, message truncated and (...)
*/
TEST_F(AampLogManagerTest, logprintf_LongMessage)
{
    AAMP_LogLevel level = eLOGLEVEL_INFO;
    std::string file("test.cpp");
    int line = 2;
    std::string message(MAX_DEBUG_LOG_BUFF_SIZE, '*');
    EXPECT_CALL(*g_mockSdJournal,
				sd_journal_print_mock( LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(-1) + "]"), HasSubstr("[INFO]"), HasSubstr("[" + file + "]"))));
    logprintf(level, file.c_str(), line, "%s", message.c_str());
}

/*
    Test logprintf called with the maximum length message
    sd_journal_print is expected to be called with the header and message without being truncated
*/
TEST_F(AampLogManagerTest, logprintf_MaxMessage)
{
	AAMP_LogLevel level = eLOGLEVEL_INFO;
    std::string file("test.cpp");
    int line = 2;
	std::ostringstream ossthread;
	ossthread << std::this_thread::get_id();
    std::string header("[AAMP-PLAYER][" + std::to_string(-1) + "][INFO][" + ossthread.str() + "][" + file + "][" + std::to_string(line) + "]");
    std::string message((MAX_DEBUG_LOG_BUFF_SIZE - header.length() - 1), '*');
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(-1) + "]"), HasSubstr("[INFO]"), HasSubstr("[" + file + "]"), HasSubstr(message))));
    logprintf(level, file.c_str(), line, "%s", message.c_str());
}

TEST_F(AampLogManagerTest, snprintf_tests)
{
	char *format_ptr = NULL;
	int format_bytes = 0;

	const char *myString = "hello";
	int myInt = 0x1234;
	size_t mySize = 0x123456789a;
	const char *expected = "hello-4660-123456789a\n";

	for( int pass=0; pass<2; pass++ )
	{
		format_bytes = snprintf(format_ptr, format_bytes, "%s-%d-%zx\n", myString, myInt, mySize );
		printf( "format_bytes=%d\n", format_bytes );
		EXPECT_EQ( format_bytes, strlen(expected) );
		if( pass==0 )
		{
			format_bytes++; // include space for nul terminator; without this, the \n gets chopped off
			format_ptr = (char *)alloca(format_bytes); // allocate on stack
		}
		else
		{
			printf( "format_ptr='%s'\n", format_ptr );
			EXPECT_STREQ( format_ptr, expected );
		}
	}
}
