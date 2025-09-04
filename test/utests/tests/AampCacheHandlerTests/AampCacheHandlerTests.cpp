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

#include <gtest/gtest.h>
#include "AampCacheHandler.h"
#include "AampMediaType.h"
#include "AampConfig.h"
#include "MockAampGrowableBuffer.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

class AampCacheHandlerTest : public Test
{
protected:
	AampCacheHandler *handler = nullptr;
	void SetUp() override
	{
		handler = new AampCacheHandler(-1);
		g_mockAampGrowableBuffer = new NiceMock<MockAampGrowableBuffer>( );
	}
	void TearDown() override
	{
		delete g_mockAampGrowableBuffer;
		g_mockAampGrowableBuffer = nullptr;

		delete handler;
		handler = nullptr;
	}
};

TEST_F(AampCacheHandlerTest, SetMaxInitFragCacheSizeTest)
{
	int cacheSize = MAX_INIT_FRAGMENT_CACHE_PER_TRACK; // 1-5
	handler->SetMaxInitFragCacheSize(cacheSize);
	int retrievedsize = handler->GetMaxInitFragCacheSize();
	EXPECT_EQ(retrievedsize, cacheSize);
	handler->SetMaxInitFragCacheSize(0);
	retrievedsize = handler->GetMaxInitFragCacheSize();
	EXPECT_EQ(retrievedsize, 0);
	handler->SetMaxInitFragCacheSize(-1);
	retrievedsize = handler->GetMaxInitFragCacheSize();
	EXPECT_EQ(retrievedsize, -1);
}
TEST_F(AampCacheHandlerTest, SetMaxPlaylistCacheSizeTest)
{
	int cacheSize = MAX_PLAYLIST_CACHE_SIZE;
	handler->SetMaxPlaylistCacheSize(cacheSize);
	int retrievedsize = handler->GetMaxPlaylistCacheSize();
	EXPECT_EQ(retrievedsize, cacheSize);
	handler->SetMaxPlaylistCacheSize(0);
	retrievedsize = handler->GetMaxPlaylistCacheSize();
	EXPECT_EQ(retrievedsize, 0);
	handler->SetMaxPlaylistCacheSize(-1);
	retrievedsize = handler->GetMaxPlaylistCacheSize();
	EXPECT_EQ(retrievedsize, -1);
}
TEST_F(AampCacheHandlerTest, InitFragCache)
{
	std::string url1 = "http://example1.com";
	std::string url2 = "http://example2.com";
	std::string url3 = "http://example3.com";
	std::string url4 = "http://example4.com";
	std::string url5 = "http://example5.com";
	std::string url6 = "http://example6.com";
	std::string url7 = "http://example7.com";
	std::string eURL;

	AampGrowableBuffer *buffer;
	AampMediaType type;
	type = eMEDIATYPE_INIT_VIDEO;

	buffer = new AampGrowableBuffer("InitFragCache_Data");
	// Inserting the Url and trying to retrieve with empty buffer
	handler->InsertToInitFragCache(url1, buffer, url1, type);
	bool res01 = handler->RetrieveFromInitFragmentCache(url1, buffer, eURL);
	EXPECT_FALSE(res01);

	//initializing buffer
	const char *srcData1[30] = {"HelloWorld"};
	size_t arraySize1 = sizeof(srcData1) / sizeof(srcData1[0]);
	buffer->AppendBytes(srcData1, arraySize1);
	// Inserting the Url and trying to retrieve with non-empty buffer
	handler->InsertToInitFragCache(url1, buffer, url1, type);
	bool res1 = handler->RetrieveFromInitFragmentCache(url1, buffer, eURL);
	EXPECT_TRUE(res1);

	// Without Inserting the Url trying to retrieve
	bool res2 = handler->RetrieveFromInitFragmentCache(url2, buffer, eURL);
	EXPECT_FALSE(res2);
	// Inserting the Url beyond the maxCachedInitFragmentsPerTrack and performing the RemoveInitFragCacheEntry ,later trying to retrieve the removed Url
	handler->InsertToInitFragCache(url2, buffer, url2, type);
	handler->InsertToInitFragCache(url3, buffer, url3, type);
	handler->InsertToInitFragCache(url4, buffer, url4, type);
	handler->InsertToInitFragCache(url5, buffer, url5, type);
	handler->InsertToInitFragCache(url6, buffer, url6, type);
	bool res3 = handler->RetrieveFromInitFragmentCache(url1, buffer, eURL);
	EXPECT_FALSE(res3);
}

TEST_F(AampCacheHandlerTest, InitFragCacheWithEffectiveURL)
{
	std::string url1 = "http://example1.com";
	std::string url2 = "http://example2.com";
	std::string url3 = "http://example3.com";
	std::string url4 = "http://example4.com";
	std::string url5 = "http://example5.com";
	std::string url6 = "http://example6.com";
	std::string url7 = "http://example7.com";
	std::string eURL1 = "http://example1.com-redirect";
	std::string eURL2 = "http://example2.com-redirect";
	std::string eURL3 = "http://example3.com-redirect";
	std::string ret_eURL;

	AampGrowableBuffer *buffer;
	AampMediaType type;
	type = eMEDIATYPE_INIT_VIDEO;

	buffer = new AampGrowableBuffer("InitFragCache_Data");
	// Inserting the Url and trying to retrieve with empty buffer
	handler->InsertToInitFragCache(url1, buffer,eURL1, type);
	EXPECT_FALSE( handler->RetrieveFromInitFragmentCache(url1, buffer, eURL1) );

	//initializing buffer
	const char *srcData1[30] = {"HelloWorld"};
	size_t arraySize1 = sizeof(srcData1) / sizeof(srcData1[0]);
	buffer->AppendBytes(srcData1, arraySize1);
	// Inserting the Url and trying to retrieve with non-empty buffer
	handler->InsertToInitFragCache(url1, buffer, eURL1, type);
	EXPECT_TRUE( handler->RetrieveFromInitFragmentCache(url1, buffer, eURL1) );
	EXPECT_EQ(eURL1, "http://example1.com-redirect");

	EXPECT_CALL(*g_mockAampGrowableBuffer, dtor()).Times(2); // Removing url4 won't free its growable buffer as it is still referenced by eURL2

	// Without Inserting the Url trying to retrieve
	EXPECT_FALSE( handler->RetrieveFromInitFragmentCache(url2, buffer, eURL1) );
	// Inserting the Url beyond the maxCachedInitFragmentsPerTrack (5) and performing the RemoveInitFragCacheEntry ,later trying to retrieve the removed Url
	// Adding url6 will remove url1 and adding url7 will remove url2, confirming that the effective URL is still valid when url2 is removed
	handler->InsertToInitFragCache(url2, buffer, eURL1, type);
	handler->InsertToInitFragCache(url3, buffer, eURL2, type);
	handler->InsertToInitFragCache(url4, buffer, eURL2, type);
	EXPECT_TRUE( handler->RetrieveFromInitFragmentCache(url3, buffer, ret_eURL) ); // switch the order of url3 and 4 un the cache
	EXPECT_EQ(ret_eURL, "http://example2.com-redirect");
	handler->InsertToInitFragCache(url5, buffer, eURL3, type);
	handler->InsertToInitFragCache(url6, buffer, eURL3, type); // Removes url1 from cache, but since eURL1 is still referenced by url2, it is not removed from the cache
	EXPECT_FALSE( handler->RetrieveFromInitFragmentCache(url1, buffer, eURL1) );
	EXPECT_TRUE( handler->RetrieveFromInitFragmentCache(eURL1, buffer, eURL1) );
	EXPECT_EQ(eURL1, "http://example1.com-redirect");
	handler->InsertToInitFragCache(url7, buffer, eURL3, type); // Removes url2, and eURL1 as there are no longer any references to it
	EXPECT_FALSE( handler->RetrieveFromInitFragmentCache(eURL1, buffer, eURL1) );

	handler->InsertToInitFragCache(url1, buffer, eURL1, type); // Removes url4, leaving eURL2 as it is still referenced by url3
	EXPECT_FALSE( handler->RetrieveFromInitFragmentCache(url4, buffer, eURL2) );
}

TEST_F(AampCacheHandlerTest, PlaylistCache)
{
	std::string url1 = "http://example1.com";
	std::string url2 = "http://example2.com";
	std::string url3 = "http://example3.com";
	std::string url4 = "http://example4.com";
	std::string url5 = "http://example5.com";
	std::string url6 = "http://example6.com";
	std::string url7 = "http://example7.com";
	std::string mpdurl = "http://example.mpd";

	AampGrowableBuffer *buffer = new AampGrowableBuffer("PlaylistCache_Data");

	// expected failure inserting empty buffer
	buffer->Clear();
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url1));
	handler->InsertToPlaylistCache(url1, buffer, url1, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url1));

	// expected failure caching non-empty playlist for live playback
	buffer->AppendBytes("apple",5);
	handler->InsertToPlaylistCache(url1, buffer, url1, true, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url1));

	// expected success caching non-empty playlist for non-live (vod)
	handler->InsertToPlaylistCache(url2, buffer, url2, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url2));

	buffer->Clear();

	//initializing buffer
	const char *srcData3[30] = {"HelloWorld"};
	size_t arraySize3 = sizeof(srcData3) / sizeof(srcData3[0]);
	buffer->AppendBytes(srcData3, arraySize3);
	// Inserting the playlist and trying to retrieve with non-empty buffer
	handler->InsertToPlaylistCache(url2, buffer, url2, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url2));

	// If new Manifest is inserted which is not present in the cache , flush out other playlist files related with old manifest,
	handler->InsertToPlaylistCache(mpdurl, buffer, mpdurl, false, eMEDIATYPE_MANIFEST);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url2));
	EXPECT_TRUE(handler->IsPlaylistUrlCached(mpdurl));

	// Removing the Url and trying to check whether the Url is present or not
	handler->RemoveFromPlaylistCache(mpdurl);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(mpdurl));

	// Inserting the manifest and trying to retrieve it
	handler->InsertToPlaylistCache(url3, buffer, url3, false, eMEDIATYPE_MANIFEST);
	EXPECT_TRUE(handler->RetrieveFromPlaylistCache(url3, buffer, url3, eMEDIATYPE_MANIFEST));

	// Trying to Insert Url when the buffer size is greater than MaxPlaylistCacheSize
	const char *srcData1[30] = {"HelloWorld"};
	size_t arraySize1 = sizeof(srcData1) / sizeof(srcData1[0]);
	buffer->AppendBytes(srcData1, arraySize1);
	handler->SetMaxPlaylistCacheSize(20);
	handler->InsertToPlaylistCache(url4, buffer, url4, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url4));

	buffer->Clear();

	// Trying to Insert Url when the buffer size is lesser than MaxPlaylistCacheSize
	const char *srcData2[20] = {"HelloWorld"};
	size_t arraySize2 = sizeof(srcData2) / sizeof(srcData2[0]);
	buffer->AppendBytes(srcData2, arraySize2);
	handler->SetMaxPlaylistCacheSize(30);
	handler->InsertToPlaylistCache(url5, buffer, url5, false, eMEDIATYPE_MANIFEST);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url5));

	// when effectiveUrl and Url is same
	handler->InsertToPlaylistCache(url6, buffer, url6, false, eMEDIATYPE_MANIFEST);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url6));

	// when effectiveUrl and Url is not same
	std::string effectiveUrl = "http://notsameurl.com";
	handler->InsertToPlaylistCache(url7, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url7));
	EXPECT_TRUE(handler->IsPlaylistUrlCached(effectiveUrl));
}

TEST_F(AampCacheHandlerTest, StartPlaylistCachetest)
{
	handler->StartPlaylistCache();
}

TEST_F(AampCacheHandlerTest, StopPlaylistCachetest)
{
	handler->StopPlaylistCache();
}

class AampCacheHandlerTest_1 : public ::testing::Test
{
protected:
	class TestableAampCacheHandler : public AampCacheHandler
	{
	public:
		TestableAampCacheHandler()
			: AampCacheHandler(-1)
		{
		}

		// Expose the protected functions for testing
		void CallInit()
		{
			InitializeIfNeeded();
		}

		void CallClearCacheHandler()
		{
			ClearCacheHandler();
		}

		void CallAsyncCacheCleanUpTask()
		{
			AsyncCacheCleanUpTask();
		}
	};
	TestableAampCacheHandler *mTestableAampCacheHandler;

	void SetUp() override
	{
		mTestableAampCacheHandler = new TestableAampCacheHandler();
	}

	void TearDown() override
	{
		delete mTestableAampCacheHandler;
		mTestableAampCacheHandler = nullptr;
	}
};

TEST_F(AampCacheHandlerTest_1, TestInit)
{
	mTestableAampCacheHandler->CallInit();
}

TEST_F(AampCacheHandlerTest_1, TestClearCacheHandler)
{
	mTestableAampCacheHandler->CallClearCacheHandler();
}

TEST_F(AampCacheHandlerTest_1, TestAsyncCacheCleanUpTask)
{
	mTestableAampCacheHandler->CallAsyncCacheCleanUpTask();
}

TEST_F(AampCacheHandlerTest, InitFragCacheLRU)
{
	handler->SetMaxInitFragCacheSize(3);
	std::string expected = "7938024839052!73!90239!70398657!2!039!!172365!8147!384!0!6!326941378!!!153!4!659!4!175!418!352!6!784";
	std::string randSeq = "7938024839052273790239970398657627039991723655814713848046032694137883815354365954917554188352266784";
	std::string ret;
	for( int i=0; i<100; i++ )
	{
		int idx = randSeq[i] - '0';
		std::string url = "url"+std::to_string(idx);
		AampGrowableBuffer *buffer = new AampGrowableBuffer("InitFragCache_Data");
		std::string effectiveUrl;
		bool hit = handler->RetrieveFromInitFragmentCache( url, buffer, effectiveUrl );
		if( hit )
		{ // signal init cache hit
			ret += "!";
		}
		else
		{
			std::string eURL = url;
			if( idx&1 )
			{ // give half of the segments a distinct effective URL
				eURL += "-redirect";
			}
			std::string data = "data" + std::to_string(idx);
			buffer->AppendBytes(data.c_str(), data.size() );
			handler->InsertToInitFragCache( url, buffer, eURL, eMEDIATYPE_INIT_VIDEO );
			ret += std::to_string(idx);
		}
		delete buffer;
	}
	EXPECT_TRUE( ret == expected );
}
