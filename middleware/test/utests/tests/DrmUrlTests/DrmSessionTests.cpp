/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include <string>
#include <gtest/gtest.h>
#include "DrmJsonObject.h"


/**
 *  @brief Get formatted URL of license server
 *
 *  @param[in] url URL of license server
 *  @return		formatted url for secclient license acquisition.
 */
std::string playerGetFormattedLicenseServerURL( const std::string &url)
{
	size_t startpos = 0;
	size_t len = url.length();
	if( url.rfind( "https://",0)==0 )
	{
		startpos = 8;
	}
	else if( url.rfind( "http://",0)==0 )
	{
		startpos = 7;
	}
	if( startpos!=0 )
	{
		size_t endpos = url.find('/', startpos);
		if( endpos != std::string::npos )
		{
			len = endpos - startpos;
		}
	}
	return url.substr(startpos, len);
}

class DrmSessionTests : public ::testing::Test {
protected:

    void SetUp() override {

    }

    void TearDown() override {
        
    }
};

TEST_F(DrmSessionTests, HandlesRandomString) {
    std::string url = "6f4dssfg564";
    std::string expected = "6f4dssfg564";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesRandomString2) {
    std::string url = "super/bad";
    std::string expected = "super/bad";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesHttpUrl) {
    std::string url = "http://example.com/path/to/resource";
    std::string expected = "example.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesHttpsUrl) {
    std::string url = "http://example.com/path/to/resource";
    std::string expected = "example.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesHttpsUrl2) {
    std::string url = "https://example.com";
    std::string expected = "example.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesHttpsUrlWithWWW) {
    std::string url = "www.drmexample.com/path/to/resource";
    std::string expected = "www.drmexample.com/path/to/resource";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithNoScheme) {
    std::string url = "drmexample.com/path/to/resource";
    std::string expected = "drmexample.com/path/to/resource";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithTrailingSlash) {
    std::string url = "https://example.com";
    std::string expected = "example.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithMultipleSlashes) {
    std::string url = "https://drmexample.com//path/to/resource";
    std::string expected = "drmexample.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithoutPath) {
    std::string url = "https://drmexample.com";
    std::string expected = "drmexample.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithoutPathWithWWW) {
    std::string url = "www.drmexample.com";
    std::string expected = "www.drmexample.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithOnlyScheme) {
    std::string url = "https://";
    std::string expected = "";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesEmptyUrl) {
    std::string url = "";
    std::string expected = "";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithPort) {
    std::string url = "https://drmexample.com:8080";
    std::string expected = "drmexample.com:8080";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithPortWithWWW) {
    std::string url = "www.drmexample.com:8080";
    std::string expected = "www.drmexample.com:8080";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithPortWithPath) {
    std::string url = "https://drmexample.com:8080/path";
    std::string expected = "drmexample.com:8080";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithPortWithPathWithWWW) {
    std::string url = "www.drmexample.com:8080/path";
    std::string expected = "www.drmexample.com:8080/path";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithQueryParameters) {
    std::string url = "https://drmexample.com/path?query=param";
    std::string expected = "drmexample.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, HandlesUrlWithFragment) {
    std::string url = "https://drmexample.com/path#fragment";
    std::string expected = "drmexample.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, UrlEncodingInDomain) {
    std::string url = "https://drmexample.com%20with%20space";
    std::string expected = "drmexample.com%20with%20space";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(DrmSessionTests, UrlEncodingInPath) {
    std::string url = "http://drmexample.com/path/to/%20resource";
    std::string expected = "drmexample.com";
    std::string result = playerGetFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};
