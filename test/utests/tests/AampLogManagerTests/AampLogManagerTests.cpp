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

#define MAX_DEBUG_LOG_BUFF_SIZE 1024

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj = NULL;

class AampLogManagerTest : public Test
{
protected:
    AampLogManager *mAampLogManager;

    void SetUp() override
    {
        mAampLogManager = new AampLogManager();
        // mLogObj is used in AAMP log macros
        mLogObj = mAampLogManager;
        g_mockSdJournal = new NiceMock<MockSdJournal>();
    }

    void TearDown() override
    {
        delete mAampLogManager;
        mAampLogManager = nullptr;
        mLogObj = nullptr;

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
    EXPECT_EQ(mAampLogManager->isLogLevelAllowed(chkLevel),false);
}

/**
 * TEST_F GTest-Testcase for isLogLevelAllowed function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, isLogLevelAllowed_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel chkLevel = eLOGLEVEL_INFO;

    //Assert: Expecting two values are equal or not
    EXPECT_EQ(mAampLogManager->isLogLevelAllowed(chkLevel),false);
}

/**
 * Testcase for isLogLevelAllowed function for Milestone log level
 * It is expected to return true, as MIL is higher level than the default level allowed WARN
**/
TEST_F(AampLogManagerTest, isLogLevelAllowed_MIL)
{
    AAMP_LogLevel chkLevel = eLOGLEVEL_MIL;

    EXPECT_EQ(mAampLogManager->isLogLevelAllowed(chkLevel), true);
}

/**
 * TEST_F GTest-Testcase for setLogLevel function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, setLogLevel_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    AAMP_LogLevel chkLevel = eLOGLEVEL_TRACE;

    //Act: Calling the function for test
    mAampLogManager->setLogLevel(chkLevel);

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
    mAampLogManager->setLogLevel(chkLevel);

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

    mAampLogManager->setLogLevel(setLevel);

    EXPECT_EQ(true, mAampLogManager->isLogLevelAllowed(chkLevel));
}

/**
 * Testcase for setLogLevel ERROR followed by isLogLevelAllowed MIL
 * isLogLevelAllowed is expected to return false, as ERROR is higher level than MIL
**/
TEST_F(AampLogManagerTest, setLogLevelError_isLogLevelAllowedMil)
{
    AAMP_LogLevel setLevel = eLOGLEVEL_ERROR;
    AAMP_LogLevel chkLevel = eLOGLEVEL_MIL;

    mAampLogManager->setLogLevel(setLevel);

    EXPECT_EQ(false, mAampLogManager->isLogLevelAllowed(chkLevel));
}

/**
 * TEST_F GTest-Testcase for getAampCfgPath function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, getAampCfgPath_Test)
{
    //Arrange: Creating the variables for passing to arguments
    const char *gAampCfg1 = "/opt/aamp.cfg";
    
    //Act: Calling the function for test
	const char* gAampCfg2 = mAampLogManager->getAampCfgPath();

    //Assert: checking values are equal or not
    EXPECT_STREQ(gAampCfg1,gAampCfg2);
}

/**
 * TEST_F GTest-Testcase for getAampCliCfgPath function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, getAampCliCfgPath_Test)
{
    //Arrange: Creating the variables for passing to arguments
    const char *gAampCliCfg1 = "/opt/aampcli.cfg";
    
    //Act: Calling the function for test
	const char* gAampCliCfg2 = mAampLogManager->getAampCliCfgPath();

    //Assert: checking values are equal or not
    EXPECT_STREQ(gAampCliCfg1,gAampCliCfg2);
}

/**
 * TEST_F GTest-Testcase for getHexDebugStr function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, getHexDebugStr_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    const std::vector<uint8_t> &data = {'a','b'};

    //Act: Calling the function for test
	std::string getHexDebugStr_String = mAampLogManager->getHexDebugStr(data);

    //Assert: checking values are equal or not
    //EXPECT_EQ(getHexDebugStr_String,'a');
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
    MediaType type = eMEDIATYPE_AUDIO;
    std::string contentType = "cppfile";
	std::string location = "folder";
	std::string symptom = "testfile";

    //Act: Calling the function for test
    mAampLogManager->LogNetworkLatency(url,downloadTime,downloadThresholdTimeoutMs,type);
    mAampLogManager->ParseContentUrl(url, contentType, location, symptom, type);

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
    MediaType type = eMEDIATYPE_AUDIO;
    std::string contentType = "cppfile";
	std::string location = "folder";
	std::string symptom = "testfile";

    //Act: Calling the function for test
    for(int i=0;i<4;i++)
    {
        mAampLogManager->LogNetworkError(url,errorType[i],errorCode,type);
    }
    mAampLogManager->ParseContentUrl(url, contentType, location, symptom, type);

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
    MediaType type = eMEDIATYPE_AUDIO;

    //Act: Calling the function for test
    mAampLogManager->LogNetworkError(url,errorType,errorCode,type);

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
    std::string contentType = "test1";
    std::string location = "test2";
    std::string symptom = "test3";
    MediaType type = eMEDIATYPE_AUDIO;

    //Act: Calling the function for test
    mAampLogManager->ParseContentUrl(url,contentType,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_STREQ(contentType.c_str(),"AUDIO");
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
    std::string contentType = "test1";
    std::string location = "test2";
    std::string symptom = "test3";
    MediaType type[20] = {
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
        mAampLogManager->ParseContentUrl(url,contentType,location,symptom,type[i]);
    }

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_STREQ(contentType.c_str(),"PLAYLIST_AUX-AUDIO");
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
    std::string contentType = "test1";
    std::string location = "test2";
    std::string symptom = "test3";
    MediaType type = eMEDIATYPE_INIT_IFRAME;

    //Act: Calling the function for test
    mAampLogManager->ParseContentUrl(url,contentType,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//httsurl");
    EXPECT_STREQ(contentType.c_str(),"INIT_IFRAME");
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
    std::string contentType = "test1";
    std::string location = "test2";
    std::string symptom = "test3";
    MediaType type = eMEDIATYPE_INIT_IFRAME;

    //Act: Calling the function for test
    mAampLogManager->ParseContentUrl(url,contentType,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//mm.");
    EXPECT_STREQ(contentType.c_str(),"INIT_IFRAME");
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
    std::string contentType = "test1";
    std::string location = "test2";
    std::string symptom = "test3";
    MediaType type = eMEDIATYPE_INIT_IFRAME;

    //Act: Calling the function for test
    mAampLogManager->ParseContentUrl(url,contentType,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"//odol");
    EXPECT_STREQ(contentType.c_str(),"INIT_IFRAME");
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
    std::string contentType = "test1";
    std::string location = "test2";
    std::string symptom = "test3";
    MediaType type = eMEDIATYPE_DEFAULT;

    //Act: Calling the function for test
    mAampLogManager->ParseContentUrl(url,contentType,location,symptom,type);

    //Assert: checking values are equal or not
    EXPECT_STREQ(url,"127.0.0.1:9080");
    EXPECT_STREQ(contentType.c_str(),"unknown");
    EXPECT_STREQ(location.c_str(),"fog");
    EXPECT_STREQ(symptom.c_str(),"unknown");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3307;
    int minor = 268435462;
    std::string description = "Missing drm keys. Files are missing from /opt/drm. This could happen if socprovisioning fails to pull keys from fkps. This could also happen with a new box type that isn't registered with fkps. Check the /opt/logs/socprov.log for error. Contact ComSec for help.";


    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);

    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Missing drm keys. Files are missing from /opt/drm. This could happen if socprovisioning fails to pull keys from fkps. This could also happen with a new box type that isn't registered with fkps. Check the /opt/logs/socprov.log for error. Contact ComSec for help.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test2)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3307;
    int minor = 570425352;
    std::string description = "Stale cache data. There is bad data in adobe cache at /opt/persistent/adobe. This can happen if the cache isn't cleared by /lib/rdk/cleanAdobe.sh after either an FKPS key update or a firmware update. This should not be happening in the field. For engineers, they can try a factory reset to fix the problem.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);

    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Stale cache data. There is bad data in adobe cache at /opt/persistent/adobe. This can happen if the cache isn't cleared by /lib/rdk/cleanAdobe.sh after either an FKPS key update or a firmware update. This should not be happening in the field. For engineers, they can try a factory reset to fix the problem.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test3)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3307;
    int minor = 1000022;
    std::string description = "Local cache directory not readable. The Receiver running as non-root cannot access and read the adobe cache at /opt/persistent/adobe. This can happen if /lib/rdk/prepareChrootEnv.sh fails to set that folders privileges. Run ls -l /opt/persistent and check the access rights. Contact the SI team for help. Also see jira XRE-6687";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);

    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Local cache directory not readable. The Receiver running as non-root cannot access and read the adobe cache at /opt/persistent/adobe. This can happen if /lib/rdk/prepareChrootEnv.sh fails to set that folders privileges. Run ls -l /opt/persistent and check the access rights. Contact the SI team for help. Also see jira XRE-6687");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test4)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3321;
    int minor = 102;
    std::string description = "Invalid signiture request on the Adobe individualization request. Expired certs can cause this, so the first course of action is to verify if the certs, temp baked in or production fkps, have not expired.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);

    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Invalid signiture request on the Adobe individualization request. Expired certs can cause this, so the first course of action is to verify if the certs, temp baked in or production fkps, have not expired.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test5)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3321;
    int minor = 10100;
    std::string description = "Unknown Device class error from the Adobe individualization server. The drm certs may be been distributed to MSO security team for inclusion in fkps, but Adobe has not yet added the device info to their indi server.";
    
    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);

    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Unknown Device class error from the Adobe individualization server. The drm certs may be been distributed to MSO security team for inclusion in fkps, but Adobe has not yet added the device info to their indi server.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test6)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3321;
    int minor = 1107296357;
    std::string description = "Unknown Device class error from the Adobe individualization server. The drm certs may be been distributed to MSO security team for inclusion in fkps, but Adobe has not yet added the device info to their indi server.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);

    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Unknown Device class error from the Adobe individualization server. The drm certs may be been distributed to MSO security team for inclusion in fkps, but Adobe has not yet added the device info to their indi server.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test7)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3321;
    int minor = 1000595;
    std::string description = "Failed to connect to individualization server. This can happen if the network goes down. This can also happen if bad proxy settings exist in /opt/xreproxy.conf. Check the receiver.log for the last HttpRequestBegin before the error occurs and check the host name in the url, then check your proxy conf";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    
    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Failed to connect to individualization server. This can happen if the network goes down. This can also happen if bad proxy settings exist in /opt/xreproxy.conf. Check the receiver.log for the last HttpRequestBegin before the error occurs and check the host name in the url, then check your proxy conf");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test8)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3322;
    int minor = 1000595;
    std::string description = "Device binding failure. DRM data cached by the player at /opt/persistent/adobe, may be corrupt, missing, or innaccesible due to file permision. Please check this folder. A factory reset may be required to fix this and force a re-individualization of the box to reset that data.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    
    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Device binding failure. DRM data cached by the player at /opt/persistent/adobe, may be corrupt, missing, or innaccesible due to file permision. Please check this folder. A factory reset may be required to fix this and force a re-individualization of the box to reset that data.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test9)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3328;
    int minor = 1003532;
    std::string description = "Potential server issue. This could happen if drm keys are missing or bad. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    
    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Potential server issue. This could happen if drm keys are missing or bad. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test10)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3329;
    int minor = 1003532;
    std::string description = "MSO license server error response. This could happen for various reasons: bad cache data, bad session token, any license related issue. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    
    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"MSO license server error response. This could happen for various reasons: bad cache data, bad session token, any license related issue. To attempt a quick fix: Back up /opt/drm and /opt/persistent/adobe, perform a factory reset, and see if that fixes the issue. Reach out to ComSec team for help diagnosing the error.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test11)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3338;
    int minor = 1003532;
    std::string description = "Unknown connection type. Rare issue related to output protection code not being implemented on certain branches or core or for new socs. See STBI-6542 for details. Reach out to Receiver IP-Video team for help.";

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    
    //Assert: checking values are equal or not
    EXPECT_STREQ(description.c_str(),"Unknown connection type. Rare issue related to output protection code not being implemented on certain branches or core or for new socs. See STBI-6542 for details. Reach out to Receiver IP-Video team for help.");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test12)
{
    //Arrange: Creating the variables for passing to arguments
    int major = 3338;
    int minor = 1003532;

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test13)
{
    //Arrange: Creating the variables for passing to arguments
    std::string description = " ";
    int major = 3338;
    int minor = 1003532;

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    EXPECT_STREQ(description.c_str()," ");
}

/**
 * TEST_F GTest-Testcase for LogDRMError function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogDRMError_Test14)
{
    //Arrange: Creating the variables for passing to arguments
    std::string description = " ";
    int major = 0;
    int minor = 0;

    //Act: Calling the function for test
    mAampLogManager->LogDRMError(major, minor);
    EXPECT_STREQ(description.c_str()," ");
}

/**
 * TEST_F GTest-Testcase for LogABRInfo function for AampLogManagerTest file
**/
TEST_F(AampLogManagerTest, LogABRInfo_Test1)
{
    //Arrange: Creating the variables for passing to arguments
    AAMPAbrInfo *pstAbrInfo = new AAMPAbrInfo;
    AAMPAbrType abrCalledFor;
    pstAbrInfo->desiredBandwidth = 10;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrBandwidthUpdate;
    AAMPNetworkErrorType errorType = AAMPNetworkErrorHttp;
	std::string reason = "bandwidth";
	std::string profile = "higher";
	std::string symptom = "video quality may increase";
    
    //Act: Calling the function for test
    mAampLogManager->LogABRInfo(pstAbrInfo);

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
    AAMPAbrType abrCalledFor;
    pstAbrInfo->desiredBandwidth = 5;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrManifestDownloadFailed;
    AAMPNetworkErrorType errorType = AAMPNetworkErrorHttp;
	std::string reason = "manifest download failed' error='http error ";
	std::string profile = "lower";
	std::string symptom = "video quality may decrease";
    
    //Act: Calling the function for test
    mAampLogManager->LogABRInfo(pstAbrInfo);

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
    AAMPAbrType abrCalledFor;
    pstAbrInfo->desiredBandwidth = 10;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrFragmentDownloadFailed;
    AAMPNetworkErrorType errorType;
    pstAbrInfo->errorType = AAMPNetworkErrorHttp;
	std::string reason = "fragment download failed'";
	std::string profile = "higher";
	std::string symptom = "video quality may increase";
    
    //Act: Calling the function for test
    mAampLogManager->LogABRInfo(pstAbrInfo);

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
    AAMPAbrType abrCalledFor;
    pstAbrInfo->desiredBandwidth = 10;
    pstAbrInfo->currentBandwidth = 5;
    pstAbrInfo->abrCalledFor = AAMPAbrUnifiedVideoEngine;
    AAMPNetworkErrorType errorType;
    pstAbrInfo->errorType = AAMPNetworkErrorNone;
	std::string reason = "changed based on unified video engine user preferred bitrate";
	std::string profile = "higher";
	std::string symptom = "video quality may increase";
    
    //Act: Calling the function for test
    mAampLogManager->LogABRInfo(pstAbrInfo);

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
    result = mAampLogManager->isLogworthyErrorCode(errorCode);

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
    result = mAampLogManager->isLogworthyErrorCode(errorCode);

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
    result = mAampLogManager->isLogworthyErrorCode(errorCode);

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
    result = mAampLogManager->isLogworthyErrorCode(errorCode);

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
    result = mAampLogManager->isLogworthyErrorCode(errorCode);

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
    int playerId = 10;
    const char* levelstr = "s1";
    const char* file = "test.cpp";
    int line = 2;
    const char *format = "s3";
    const char *format2 = "s4";

    //Act: Calling the function for test
    logprintf(playerId,levelstr,file,line,format,format2);
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
    ASSERT_EQ(-1, mAampLogManager->getPlayerId());
}

/*
    Test setPlayerId followed by getPlayerId function
    getPlayerId is expected to return the value set
*/
TEST_F(AampLogManagerTest, setPlayerId_getPlayerId)
{
    int playerId = 73;
    mAampLogManager->setPlayerId(playerId);
    ASSERT_EQ(playerId, mAampLogManager->getPlayerId());
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
    mAampLogManager->setLogLevel(eLOGLEVEL_MIL);
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
    mAampLogManager->setLogLevel(eLOGLEVEL_ERROR);
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_MIL("%s", message.c_str());
}

/*
    Test setPlayerId followed by AAMPLOG_MIL macro
    sd_journal_print is expected to be called with a text including the level, message and the player ID
*/
TEST_F(AampLogManagerTest, setPlayerId_AAMPLOG_MIL)
{
    const std::string message{"Test MIL log line"};
    int playerId = 73;
    mAampLogManager->setPlayerId(playerId);
    /* The printed log line must contain the player ID and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(playerId) + "]"),
                                                                          HasSubstr("[MIL]"),
                                                                          HasSubstr(message.c_str()))));
    AAMPLOG_MIL("%s", message.c_str());
}

/*
    Test AAMPLOG_OBJ_TRACE macro
    No log line is expected, as TRACE level is lower than the default level (WARN)
*/
TEST_F(AampLogManagerTest, AAMPLOG_OBJ_TRACE)
{
    const std::string message{"Test TRACE log line"};
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_OBJ_TRACE(mAampLogManager, "%s", message.c_str());
}

/*
    Test AAMPLOG_OBJ_INFO macro
    No log line is expected, as INFO level is lower than the default level (WARN)
*/
TEST_F(AampLogManagerTest, AAMPLOG_OBJ_INFO)
{
    const std::string message{"Test INFO log line"};
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_OBJ_INFO(mAampLogManager, "%s", message.c_str());
}

/*
    Test AAMPLOG_OBJ_WARN macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, AAMPLOG_OBJ_WARN)
{
    const std::string message{"Test WARN log line"};
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[WARN]"), HasSubstr(message.c_str()))));
    AAMPLOG_OBJ_WARN(mAampLogManager, "%s", message.c_str());
}

/*
    Test AAMPLOG_OBJ_MIL macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, AAMPLOG_OBJ_MIL)
{
    const std::string message{"Test MIL log line"};
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[MIL]"), HasSubstr(message.c_str()))));
    AAMPLOG_OBJ_MIL(mAampLogManager, "%s", message.c_str());
}

/*
    Test AAMPLOG_OBJ_ERR macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, AAMPLOG_OBJ_ERR)
{
    const std::string message{"Test ERROR log line"};
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("[ERROR]"), HasSubstr(message.c_str()))));
    AAMPLOG_OBJ_ERR(mAampLogManager, "%s", message.c_str());
}

/*
    Test setLogLevel with MIL followed by AAMPLOG_OBJ_MIL macro
    sd_journal_print is expected to be called with a text including the level, message and the default player ID
*/
TEST_F(AampLogManagerTest, setLogLevelMil_AAMPLOG_OBJ_MIL)
{
    const std::string message{"Test MIL log line"};
    mAampLogManager->setLogLevel(eLOGLEVEL_MIL);
    /* The printed log line must contain the default player ID (-1) and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[-1]"), HasSubstr("MIL"), HasSubstr(message.c_str()))));
    AAMPLOG_OBJ_MIL(mAampLogManager, "%s", message.c_str());
}

/*
    Test setLogLevel with ERROR followed by AAMPLOG_OBJ_MIL macro
    Since ERROR is higher level than MIL, sd_journal_print is not expected to be called
*/
TEST_F(AampLogManagerTest, setLogLevelError_AAMPLOG_OBJ_MIL)
{
    const std::string message{"Test MIL log line"};
    mAampLogManager->setLogLevel(eLOGLEVEL_ERROR);
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(_, _)).Times(0);
    AAMPLOG_OBJ_MIL(mAampLogManager, "%s", message.c_str());
}

/*
    Test setPlayerId followed by AAMPLOG_OBJ_MIL macro
    sd_journal_print is expected to be called with a text including the level, message and the player ID
*/
TEST_F(AampLogManagerTest, setPlayerId_AAMPLOG_OBJ_MIL)
{
    const std::string message{"Test MIL log line"};
    int playerId = 73;
    mAampLogManager->setPlayerId(playerId);
    /* The printed log line must contain the player ID and the message. */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(playerId) + "]"),
                                                                          HasSubstr("[MIL]"),
                                                                          HasSubstr(message.c_str()))));
    AAMPLOG_OBJ_MIL(mAampLogManager, "%s", message.c_str());
}

/*
    Test logprintf called with a very long level string
    sd_journal_print is expected to be called with the header text truncated and (...)
*/
TEST_F(AampLogManagerTest, logprintf_LongLevelString)
{
    int playerId = 10;
    std::string levelstr(MAX_DEBUG_LOG_BUFF_SIZE, '*');
    std::string file("test.cpp");
    int line = 2;
    std::string message("message");

    /* The printed log line must contain the player ID and (...) */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(playerId) + "]"),
                                                                          HasSubstr("(...)"))));
    logprintf(playerId, levelstr.c_str(), file.c_str(), line, "%s", message.c_str());
}

/*
    Test logprintf called with a very long file
    sd_journal_print is expected to be called with the header text truncated and (...)
*/
TEST_F(AampLogManagerTest, logprintf_LongFile)
{
    int playerId = 10;
    std::string levelstr("level");
    std::string file(MAX_DEBUG_LOG_BUFF_SIZE, '*');
    int line = 2;
    std::string message("message");

    /* The printed log line must contain the player ID, the level and (...) */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(playerId) + "]"),
                                                                          HasSubstr("[" + levelstr + "]"),
                                                                          HasSubstr("(...)"))));
    logprintf(playerId, levelstr.c_str(), file.c_str(), line, "%s", message.c_str());
}

/*
    Test logprintf called with a very long message
    sd_journal_print is expected to be called with the header text, message truncated and (...)
*/
TEST_F(AampLogManagerTest, logprintf_LongMessage)
{
    int playerId = 10;
    std::string levelstr("level");
    std::string file("test.cpp");
    int line = 2;
    std::string message(MAX_DEBUG_LOG_BUFF_SIZE, '*');

    /* The printed log line must contain the player ID, level, file and (...) */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(playerId) + "]"),
                                                                          HasSubstr("[" + levelstr + "]"),
                                                                          HasSubstr("[" + file + "]"),
                                                                          HasSubstr("(...)"))));
    logprintf(playerId, levelstr.c_str(), file.c_str(), line, "%s", message.c_str());
}

/*
    Test logprintf called with the maximum length message
    sd_journal_print is expected to be called with the header and message without being truncated
*/
TEST_F(AampLogManagerTest, logprintf_MaxMessage)
{
    int playerId = 10;
    std::string levelstr("level");
    std::string file("test.cpp");
    int line = 2;
	std::ostringstream ossthread;
	ossthread << std::this_thread::get_id();
    std::string header("[AAMP-PLAYER][" + std::to_string(playerId) + "][" + levelstr + "][" + ossthread.str() + "][" + file + "][" + std::to_string(line) + "]");
    std::string message((MAX_DEBUG_LOG_BUFF_SIZE - header.length() - 1), '*');

    /* The printed log line must contain the player ID, level, file and message, with no (...) */
    EXPECT_CALL(*g_mockSdJournal, sd_journal_print_mock(LOG_NOTICE, AllOf(HasSubstr("[" + std::to_string(playerId) + "]"),
                                                                          HasSubstr("[" + levelstr + "]"),
                                                                          HasSubstr("[" + file + "]"),
                                                                          HasSubstr(message),
                                                                          Not(HasSubstr("(...)")))));
    logprintf(playerId, levelstr.c_str(), file.c_str(), line, "%s", message.c_str());
}
