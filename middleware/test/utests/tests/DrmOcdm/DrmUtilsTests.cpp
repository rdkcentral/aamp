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

#include <vector>
#include <gtest/gtest.h>

#include "DrmUtils.h"
#include "PlayerUtils.h"

class DrmUtilsTests : public ::testing::Test
{

protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}

public:
};

TEST_F(DrmUtilsTests, TestGetAbsoluteKeyUri)
{
	struct GetAbsoluteKeyUriTestData
	{
		std::string manifestUrl;
		std::string keyUri;
		std::string expectedUri;
	};

	const std::vector<GetAbsoluteKeyUriTestData> testData = {
		{"http://stream.example/manifest.m3u8", "basic1.key", "http://stream.example/basic1.key"},
		{"http://stream.example/manifest.m3u8", "basic2", "http://stream.example/basic2"},
		{"http://stream.example/manifest", "basic3", "http://stream.example/basic3"},
		{"http://stream.example/assets/hls/manifest.m3u8", "basic4.key",
		 "http://stream.example/assets/hls/basic4.key"},

		// Relative path which refers to a parent directory. We copy ../ instances as-is, these can
		// be resolved by curl
		{"http://stream.example/asset/1080p/manifest.m3u8", "../relkey1.key",
		 "http://stream.example/asset/1080p/../relkey1.key"},
		{"http://stream.example/asset/1080p/../manifest.m3u8", "../relkey2.key",
		 "http://stream.example/asset/1080p/../../relkey2.key"},

		// Port number included
		{"http://stream.example:1234/manifest.m3u8", "port1.key",
		 "http://stream.example:1234/port1.key"},

		// HTTPS
		{"https://stream.example/manifest.m3u8", "secure1.key",
		 "https://stream.example/secure1.key"},

		// Absolute path reference on key URI
		{"http://stream.example/manifest.m3u8", "/absref1.key",
		 "http://stream.example/absref1.key"},
		{"http://stream.example/assets/hls/manifest.m3u8", "/absref2.key",
		 "http://stream.example/absref2.key"},

		// Absolute key URIs
		{"http://stream.example/manifest.m3u8", "http://key.example/abs1.key",
		 "http://key.example/abs1.key"},
		{"", "http://key.example/abs2.key", "http://key.example/abs2.key"},

		// Keys with arguments
		{"http://stream.example/manifest.m3u8", "arg1.key?a=test",
		 "http://stream.example/arg1.key?a=test"},
		{"http://stream.example/manifest.m3u8", "arg2.key?a=http://",
		 "http://stream.example/arg2.key?a=http://"},

		// Only domain present on manifest URL (not normally expected, but should be handled OK)
		{"http://stream.example", "domonly1.key", "http://stream.example/domonly1.key"},
		{"http://stream.example/", "domonly2.key", "http://stream.example/domonly2.key"},
		{"http://stream.example", "/domonly3.key", "http://stream.example/domonly3.key"},

		// Other protocols for the key (HTTP expected, but this utility is expected to be protocol
		// agnostic)
		{"http://example/manifest.m3u8", "file://prot1.key", "file://prot1.key"},
		// Still a valid scheme (- . and + are allowed)
		{"http://example/manifest.m3u8", "a-b.c+d://prot2.key", "a-b.c+d://prot2.key"},
		// Not a valid scheme - will default to relative
		{"http://example/manifest.m3u8", "#abc!://prot3.key", "http://example/#abc!://prot3.key"},
		// Capitalised protocol accepted
		{"http://example/manifest.m3u8", "HTTP://key.example/prot4.key",
		 "HTTP://key.example/prot4.key"},
	};

	for (auto test : testData)
	{
		std::string keyURI;
		ResolveURL(keyURI, test.manifestUrl, test.keyUri.c_str(), false);
		ASSERT_EQ(test.expectedUri, keyURI);
	}
}
