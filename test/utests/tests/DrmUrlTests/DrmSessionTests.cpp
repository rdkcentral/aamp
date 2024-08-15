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

#include "AampDRMSessionManager.cpp"

AampLogManager *mLogObj{nullptr};

class AampDrmSessionTests : public ::testing::Test {
protected:

    void SetUp() override {

    }

    void TearDown() override {
        
    }
};

TEST_F(AampDrmSessionTests, HandlesRandomString) {
    std::string url = "6f4dssfg564";
    std::string expected = "6f4dssfg564";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesRandomString2) {
    std::string url = "super/bad";
    std::string expected = "super/bad";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesHttpUrl) {
    std::string url = "http://comcast.com/cake/cookie";
    std::string expected = "comcast.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesHttpsUrl) {
    std::string url = "https://comcast.com/cake/cookie";
    std::string expected = "comcast.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesHttpsUrl2) {
    std::string url = "https://mds.ccp.xcal.tv";
    std::string expected = "mds.ccp.xcal.tv";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesHttpsUrlWithWWW) {
    std::string url = "www.drmexample.com/path/to/resource";
    std::string expected = "www.drmexample.com/path/to/resource";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithNoScheme) {
    std::string url = "drmexample.com/path/to/resource";
    std::string expected = "drmexample.com/path/to/resource";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithTrailingSlash) {
    std::string url = "https://comcast.com";
    std::string expected = "comcast.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithMultipleSlashes) {
    std::string url = "https://drmexample.com//path/to/resource";
    std::string expected = "drmexample.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithoutPath) {
    std::string url = "https://drmexample.com";
    std::string expected = "drmexample.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithoutPathWithWWW) {
    std::string url = "www.drmexample.com";
    std::string expected = "www.drmexample.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithOnlyScheme) {
    std::string url = "https://";
    std::string expected = "";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesEmptyUrl) {
    std::string url = "";
    std::string expected = "";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithPort) {
    std::string url = "https://drmexample.com:8080";
    std::string expected = "drmexample.com:8080";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithPortWithWWW) {
    std::string url = "www.drmexample.com:8080";
    std::string expected = "www.drmexample.com:8080";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithPortWithPath) {
    std::string url = "https://drmexample.com:8080/path";
    std::string expected = "drmexample.com:8080";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithPortWithPathWithWWW) {
    std::string url = "www.drmexample.com:8080/path";
    std::string expected = "www.drmexample.com:8080/path";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithQueryParameters) {
    std::string url = "https://drmexample.com/path?query=param";
    std::string expected = "drmexample.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, HandlesUrlWithFragment) {
    std::string url = "https://drmexample.com/path#fragment";
    std::string expected = "drmexample.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, UrlEncodingInDomain) {
    std::string url = "https://drmexample.com%20with%20space";
    std::string expected = "drmexample.com%20with%20space";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};

TEST_F(AampDrmSessionTests, UrlEncodingInPath) {
    std::string url = "http://drmexample.com/path/to/%20resource";
    std::string expected = "drmexample.com";
    std::string result = getFormattedLicenseServerURL(url);
    EXPECT_EQ(result, expected);
};