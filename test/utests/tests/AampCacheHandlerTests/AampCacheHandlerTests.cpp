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
#include <cstring>
#include "AampCacheHandler.h"
#include "AampMediaType.h"
#include "AampConfig.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};

class AampCacheHandlerTest : public Test
{
protected:
	AampCacheHandler *handler = nullptr;
	void SetUp() override
	{
		handler = new AampCacheHandler(-1);
	}
	void TearDown() override
	{
		delete handler;
		handler = nullptr;
	}

	// Helper to encapsulate the "Try Get or Insert" logic
	std::string ProcessUrl(int idx)
	{
		const std::string url = "url" + std::to_string(idx);
		std::vector<uint8_t> buffer;
		std::string effectiveUrl;

		if (handler->RetrieveFromInitFragmentCache(url, buffer, effectiveUrl))
		{
			return "!";
		}

		// RAII: Initialize buffer directly from string
		const std::string data = "data" + std::to_string(idx);
		buffer.assign(data.begin(), data.end());

		// Logic: half segments get distinct effective URLs
		const std::string eURL = (idx % 2 != 0) ? url + "-redirect" : url;

		handler->InsertToInitFragCache(url, buffer, eURL, eMEDIATYPE_INIT_VIDEO);
		return std::to_string(idx);
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

	const std::string helloStr = "HelloWorld";
	std::vector<uint8_t> dataBuffer(helloStr.begin(), helloStr.end());
	std::vector<uint8_t> emptyBuffer{};
	AampMediaType type = eMEDIATYPE_INIT_VIDEO;

	// Inserting the Url and trying to retrieve with empty buffer
	handler->InsertToInitFragCache(url1, emptyBuffer, url1, type);
	bool res01 = handler->RetrieveFromInitFragmentCache(url1, emptyBuffer, eURL);
	EXPECT_FALSE(res01);

	// Inserting the Url and trying to retrieve with non-empty buffer
	handler->InsertToInitFragCache(url1, dataBuffer, url1, type);
	bool res1 = handler->RetrieveFromInitFragmentCache(url1, dataBuffer, eURL);
	EXPECT_TRUE(res1);

	// Without Inserting the Url trying to retrieve
	bool res2 = handler->RetrieveFromInitFragmentCache(url2, dataBuffer, eURL);
	EXPECT_FALSE(res2);
	// Inserting the Url beyond the maxCachedInitFragmentsPerTrack and performing the RemoveInitFragCacheEntry ,later trying to retrieve the removed Url
	handler->InsertToInitFragCache(url2, dataBuffer, url2, type);
	handler->InsertToInitFragCache(url3, dataBuffer, url3, type);
	handler->InsertToInitFragCache(url4, dataBuffer, url4, type);
	handler->InsertToInitFragCache(url5, dataBuffer, url5, type);
	handler->InsertToInitFragCache(url6, dataBuffer, url6, type);
	bool res3 = handler->RetrieveFromInitFragmentCache(url1, dataBuffer, eURL);
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

	const std::string helloStr = "HelloWorld";
	std::vector<uint8_t> dataBuffer(helloStr.begin(), helloStr.end());
	std::vector<uint8_t> emptyBuffer{};
	AampMediaType type = eMEDIATYPE_INIT_VIDEO;
	// Inserting the Url and trying to retrieve with empty buffer
	handler->InsertToInitFragCache(url1, emptyBuffer, eURL1, type);
	EXPECT_FALSE(handler->RetrieveFromInitFragmentCache(url1, emptyBuffer, eURL1));

	// Inserting the Url and trying to retrieve with non-empty buffer
	handler->InsertToInitFragCache(url1, dataBuffer, eURL1, type);
	EXPECT_TRUE(handler->RetrieveFromInitFragmentCache(url1, dataBuffer, eURL1));
	EXPECT_EQ(eURL1, "http://example1.com-redirect");

	// Without Inserting the Url trying to retrieve
	EXPECT_FALSE(handler->RetrieveFromInitFragmentCache(url2, dataBuffer, eURL1));
	// Inserting the Url beyond the maxCachedInitFragmentsPerTrack (5) and performing the RemoveInitFragCacheEntry, later trying to retrieve the removed Url
	// Adding url6 will remove url1 and adding url7 will remove url2, confirming that the effective URL is still valid when url2 is removed
	handler->InsertToInitFragCache(url2, dataBuffer, eURL1, type);
	handler->InsertToInitFragCache(url3, dataBuffer, eURL2, type);
	handler->InsertToInitFragCache(url4, dataBuffer, eURL2, type);
	EXPECT_TRUE(handler->RetrieveFromInitFragmentCache(url3, dataBuffer, ret_eURL)); // switch the order of url3 and 4 in the cache
	EXPECT_EQ(ret_eURL, "http://example2.com-redirect");
	handler->InsertToInitFragCache(url5, dataBuffer, eURL3, type);
	handler->InsertToInitFragCache(url6, dataBuffer, eURL3, type); // Removes url1 from cache, but since eURL1 is still referenced by url2, it is not removed from the cache
	EXPECT_FALSE(handler->RetrieveFromInitFragmentCache(url1, dataBuffer, eURL1));
	EXPECT_TRUE(handler->RetrieveFromInitFragmentCache(eURL1, dataBuffer, eURL1));
	EXPECT_EQ(eURL1, "http://example1.com-redirect");
	handler->InsertToInitFragCache(url7, dataBuffer, eURL3, type); // Removes url2, and eURL1 as there are no longer any references to it
	EXPECT_FALSE(handler->RetrieveFromInitFragmentCache(eURL1, dataBuffer, eURL1));

	handler->InsertToInitFragCache(url1, dataBuffer, eURL1, type); // Removes url4, leaving eURL2 as it is still referenced by url3
	EXPECT_FALSE(handler->RetrieveFromInitFragmentCache(url4, dataBuffer, eURL2));
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

	const std::string helloStr = "HelloWorld";
	const std::string appleStr = "apple";

	std::vector<uint8_t> buffer{};
	std::vector<uint8_t> fetchBuffer{};

	// expected failure inserting empty buffer
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url1));
	handler->InsertToPlaylistCache(url1, buffer, url1, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url1));

	// expected failure caching non-empty playlist for live playback
	buffer.assign(appleStr.begin(), appleStr.end());
	handler->InsertToPlaylistCache(url1, buffer, url1, true, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url1));

	// expected success caching non-empty playlist for non-live (vod)
	handler->InsertToPlaylistCache(url2, buffer, url2, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url2));

	// initialize buffer with HelloWorld
	buffer.assign(helloStr.begin(), helloStr.end());
	// Inserting the playlist and trying to retrieve with non-empty buffer
	handler->InsertToPlaylistCache(url2, buffer, url2, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_TRUE(handler->IsPlaylistUrlCached(url2));
	EXPECT_TRUE(handler->RetrieveFromPlaylistCache(url2, fetchBuffer, url2, eMEDIATYPE_MANIFEST));
	// Insert would have done nothing as same url is being inserted again, so fetchBuffer should still have appleStr
	EXPECT_EQ(fetchBuffer.size(), appleStr.size());

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
	buffer.assign(helloStr.begin(), helloStr.end());
	handler->SetMaxPlaylistCacheSize(18); // "HelloWorld" * 2 = 20 bytes (No null in the vector)
	handler->InsertToPlaylistCache(url4, buffer, url4, false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(handler->IsPlaylistUrlCached(url4));

	// Trying to Insert Url when the buffer size is lesser than MaxPlaylistCacheSize
	buffer.assign(helloStr.begin(), helloStr.end());
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

	// C++17 string_view for static data
	using namespace std::string_view_literals;
	constexpr auto expected = "7938024839052!73!90239!70398657!2!039!!172365!8147!384!0!6!326941378!!!153!4!659!4!175!418!352!6!784"sv;
	constexpr auto randSeq = "7938024839052273790239970398657627039991723655814713848046032694137883815354365954917554188352266784"sv;

	std::string actual;
	actual.reserve(expected.size()); // Optimization: Avoid reallocations

	for (const char c : randSeq)
	{
		actual += ProcessUrl(c - '0');
	}

	EXPECT_EQ(actual, expected);
}

/**
 * @brief Test fixture for testing AampCache::reduceCacheSize() functionality
 *
 * This test fixture tests the reduceCacheSize() private method indirectly through
 * public API methods (InsertToPlaylistCache) that trigger cache reduction when
 * size limits are exceeded. This follows L1 testing best practices.
 */
class AampCacheReduceSizeTest : public ::testing::Test
{
protected:
	AampCacheHandler* handler;

	void SetUp() override
	{
		handler = new AampCacheHandler(-1);
	}

	void TearDown() override
	{
		delete handler;
		handler = nullptr;
	}

	/**
	 * @brief Helper to insert test playlist data
	 */
	void InsertTestPlaylist(const std::string& url, AampMediaType mediaType, size_t dataSize)
	{
		std::vector<uint8_t> buffer(dataSize, 'x');
		handler->InsertToPlaylistCache(url, buffer, url, false, mediaType);
	}

	/**
	 * @brief Helper to check if URL is cached
	 */
	bool IsCached(const std::string& url)
	{
		return handler->IsPlaylistUrlCached(url);
	}
};

/**
 * @test AampCache_reduceCacheSize_FirstPassRemovesSpecificType
 * @brief Verify that reduceCacheSize() first pass removes only playlists of the specified media type
 *
 * Test validates that when cache reduction is triggered (via exceeding max cache size):
 * - Playlists of the triggering type are removed first
 * - Manifest entries are preserved
 * - Playlists of other types are preserved
 *
 * Tests reduceCacheSize() indirectly by filling cache to capacity and inserting new item.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_FirstPassRemovesSpecificType)
{
	// Set small cache size to trigger reduction
	handler->SetMaxPlaylistCacheSize(500);

	// Insert manifest (should always be preserved)
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 50);
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));

	// Insert audio playlists
	InsertTestPlaylist("http://example.com/audio1.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);
	InsertTestPlaylist("http://example.com/audio2.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);
	EXPECT_TRUE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/audio2.m3u8"));

	// Insert video playlist (different type - should be preserved)
	InsertTestPlaylist("http://example.com/video1.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 100);
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));

	// Insert another audio that will exceed cache size
	// This should trigger reduceCacheSize() for AUDIO type, removing audio1 and audio2
	InsertTestPlaylist("http://example.com/audio3.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 200);

	// Verify: New audio playlist was added
	EXPECT_TRUE(IsCached("http://example.com/audio3.m3u8"));

	// Verify: Manifest preserved
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));

	// Verify: Video (different type) preserved
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));
}

/**
 * @test AampCache_reduceCacheSize_PreservesManifestAlways
 * @brief Verify that manifest entries are never removed by reduceCacheSize()
 *
 * Test validates that regardless of cache pressure, manifest entries
 * (eMEDIATYPE_MANIFEST) are always preserved during cache reduction.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_PreservesManifestAlways)
{
	// Set very small cache size
	handler->SetMaxPlaylistCacheSize(200);

	// Insert manifest
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 100);
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));

	// Insert video playlists that fill cache
	InsertTestPlaylist("http://example.com/video1.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 100);
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));

	// Insert another video exceeding cache - should trigger reduction
	InsertTestPlaylist("http://example.com/video2.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 150);

	// Verify: Manifest still preserved after cache reduction
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
}

/**
 * @test AampCache_reduceCacheSize_RemovesOldest
 * @brief Verify that when same media type playlists exist, reduction works correctly
 *
 * Test validates cache reduction logic when multiple playlists of the same type
 * exist and cache size limit is exceeded.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_RemovesOldest)
{
	// Set cache size to hold about 3 items
	handler->SetMaxPlaylistCacheSize(350);

	// Insert manifest (50 bytes)
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 50);

	// Insert audio playlists (100 bytes each)
	InsertTestPlaylist("http://example.com/audio1.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);
	InsertTestPlaylist("http://example.com/audio2.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);
	InsertTestPlaylist("http://example.com/audio3.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);

	// All should be cached at this point (350 bytes total)
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
	EXPECT_TRUE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/audio2.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/audio3.m3u8"));

	// Insert large audio that exceeds cache - triggers reduction
	InsertTestPlaylist("http://example.com/audio4.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 200);

	// New audio should be added
	EXPECT_TRUE(IsCached("http://example.com/audio4.m3u8"));

	// Verify oldest items are removed
	EXPECT_FALSE(IsCached("http://example.com/audio1.m3u8")); // Oldest audio - should be removed
	EXPECT_FALSE(IsCached("http://example.com/audio2.m3u8")); // Second oldest - should be removed

	// Manifest should remain
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
}

/**
 * @test AampCache_reduceCacheSize_MultipleMediaTypes
 * @brief Verify correct behavior with multiple media types in cache
 *
 * Test validates that reduceCacheSize() correctly handles cache with multiple
 * different media types (audio, video, subtitle) and only removes the
 * type that triggered the reduction.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_MultipleMediaTypes)
{
	// Set medium cache size
	handler->SetMaxPlaylistCacheSize(500);

	// Insert manifest
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 50);

	// Insert playlists of different types
	InsertTestPlaylist("http://example.com/audio.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);
	InsertTestPlaylist("http://example.com/video.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 100);
	InsertTestPlaylist("http://example.com/subtitle.m3u8", eMEDIATYPE_PLAYLIST_SUBTITLE, 100);

	// All should be cached (350 bytes total)
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
	EXPECT_TRUE(IsCached("http://example.com/audio.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/subtitle.m3u8"));

	// Insert large video that exceeds cache - triggers reduction for VIDEO type
	InsertTestPlaylist("http://example.com/video2.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 250);

	// Verify: Different media types preserved
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
	EXPECT_TRUE(IsCached("http://example.com/audio.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/subtitle.m3u8"));

	// New video should be present
	EXPECT_TRUE(IsCached("http://example.com/video2.m3u8"));
	// Old video is removed
	EXPECT_FALSE(IsCached("http://example.com/video.m3u8"));
}

/**
 * @test AampCache_reduceCacheSize_UnlimitedCacheSize
 * @brief Verify behavior when cache size is set to unlimited
 *
 * Test validates that when maxPlaylistCacheBytes is set to PLAYLIST_CACHE_SIZE_UNLIMITED,
 * no cache reduction occurs regardless of cache size.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_UnlimitedCacheSize)
{
	// Set unlimited cache size
	handler->SetMaxPlaylistCacheSize(PLAYLIST_CACHE_SIZE_UNLIMITED);

	// Insert many large playlists
	for (int i = 0; i < 20; i++)
	{
		std::string url = "http://example.com/audio" + std::to_string(i) + ".m3u8";
		InsertTestPlaylist(url, eMEDIATYPE_PLAYLIST_AUDIO, 1000);
	}

	// All should be cached (no reduction with unlimited size)
	for (int i = 0; i < 20; i++)
	{
		std::string url = "http://example.com/audio" + std::to_string(i) + ".m3u8";
		EXPECT_TRUE(IsCached(url));
	}
}

/**
 * @test AampCache_reduceCacheSize_ManifestInsertion
 * @brief Verify that inserting new manifest clears all playlists
 *
 * Test validates special behavior: when a new manifest is inserted,
 * all old playlists associated with previous manifest are cleared.
 * This is different from reduceCacheSize() but related cache management.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_ManifestInsertion)
{
	// Insert initial manifest and playlists
	InsertTestPlaylist("http://example.com/manifest1.mpd", eMEDIATYPE_MANIFEST, 100);
	InsertTestPlaylist("http://example.com/audio1.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);
	InsertTestPlaylist("http://example.com/video1.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 100);

	EXPECT_TRUE(IsCached("http://example.com/manifest1.mpd"));
	EXPECT_TRUE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));

	// Insert new manifest - should clear old playlists
	InsertTestPlaylist("http://example.com/manifest2.mpd", eMEDIATYPE_MANIFEST, 100);

	// Old playlists should be cleared
	EXPECT_FALSE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_FALSE(IsCached("http://example.com/video1.m3u8"));

	// Old manifest should also be cleared
	EXPECT_FALSE(IsCached("http://example.com/manifest1.mpd"));

	// New manifest should be present
	EXPECT_TRUE(IsCached("http://example.com/manifest2.mpd"));
}

/**
 * @test AampCache_reduceCacheSize_ZeroCacheSize
 * @brief Verify behavior when max cache size is set to zero
 *
 * Test validates edge case where maxPlaylistCacheBytes is 0.
 * Manifest should still be cached but playlists may not be.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_ZeroCacheSize)
{
	// Set cache size to 0
	handler->SetMaxPlaylistCacheSize(0);

	// Try to insert manifest (special type, should succeed even with size 0)
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 100);

	// Manifest insertion triggers Clear() for manifest type, so it will be inserted
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));

	// Try to insert regular playlist - should fail due to size constraint
	InsertTestPlaylist("http://example.com/audio.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 100);

	// Audio playlist should not be cached (exceeds maxPlaylistCacheBytes of 0)
	EXPECT_FALSE(IsCached("http://example.com/audio.m3u8"));
}

/**
 * @test AampCache_reduceCacheSize_LiveVsVOD
 * @brief Verify that live playlists are not cached (reduceCacheSize not triggered)
 *
 * Test validates that isLive=true prevents caching, so reduceCacheSize() is never
 * triggered for live playlists.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_LiveVsVOD)
{
	handler->SetMaxPlaylistCacheSize(300);

	// Insert VOD playlist (isLive=false) - should be cached
	std::vector<uint8_t> buffer(100, 'x');
	handler->InsertToPlaylistCache("http://example.com/vod.m3u8", buffer,
		"http://example.com/vod.m3u8", false, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_TRUE(IsCached("http://example.com/vod.m3u8"));

	// Insert live playlist (isLive=true) - should NOT be cached
	handler->InsertToPlaylistCache("http://example.com/live.m3u8", buffer,
		"http://example.com/live.m3u8", true, eMEDIATYPE_PLAYLIST_VIDEO);
	EXPECT_FALSE(IsCached("http://example.com/live.m3u8"));

	// VOD should still be cached
	EXPECT_TRUE(IsCached("http://example.com/vod.m3u8"));
}

/**
 * @test AampCache_reduceCacheSize_SecondPassNotTriggered
 * @brief Verify second pass does NOT trigger when first pass reduces cache sufficiently
 *
 * This test validates that when the first pass removes enough playlists to meet the
 * target cache size (totalCachedBytes <= targetCacheSize), the second pass is skipped.
 * Video and subtitle playlists should remain cached when only audio cleanup is sufficient.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_SecondPassNotTriggered)
{
	handler->SetMaxPlaylistCacheSize(500);

	// Insert manifest (50 bytes)
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 50);

	// Insert audio playlists (150 bytes total)
	InsertTestPlaylist("http://example.com/audio1.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 75);
	InsertTestPlaylist("http://example.com/audio2.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 75);

	// Insert video playlists (150 bytes total)
	InsertTestPlaylist("http://example.com/video1.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 75);
	InsertTestPlaylist("http://example.com/video2.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 75);

	// Insert subtitle (100 bytes)
	InsertTestPlaylist("http://example.com/subtitle.m3u8", eMEDIATYPE_PLAYLIST_SUBTITLE, 100);

	// Current: manifest(50) + audio(150) + video(150) + subtitle(100) = 450 bytes
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
	EXPECT_TRUE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/audio2.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video2.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/subtitle.m3u8"));

	// Insert new audio (150 bytes): total would be 600, exceeding 500 limit
	// Target for reduction: 500 - 150 = 350 bytes
	// First pass will remove all AUDIO playlists (150 bytes)
	// After first pass BEFORE inserting new: 50 + 150 + 100 = 300 bytes
	// 300 <= 350? YES - second pass should NOT trigger
	InsertTestPlaylist("http://example.com/audio3.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 150);

	// Verify: New audio added, old audio removed
	EXPECT_TRUE(IsCached("http://example.com/audio3.m3u8"));
	EXPECT_FALSE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_FALSE(IsCached("http://example.com/audio2.m3u8"));

	// CRITICAL: Verify second pass did NOT trigger (video and subtitle still present)
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video2.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/subtitle.m3u8"));

	// Manifest always preserved
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
}

/**
 * @test AampCache_reduceCacheSize_SecondPassTriggered
 * @brief Verify second pass DOES trigger when first pass cannot reduce cache sufficiently
 *
 * This test validates that when the first pass doesn't remove enough playlists to meet
 * the target cache size (totalCachedBytes > targetCacheSize), the second pass executes
 * and removes all non-manifest playlists to make room for the new playlist.
 */
TEST_F(AampCacheReduceSizeTest, AampCache_reduceCacheSize_SecondPassTriggered)
{
	handler->SetMaxPlaylistCacheSize(300);

	// Insert manifest (50 bytes)
	InsertTestPlaylist("http://example.com/manifest.mpd", eMEDIATYPE_MANIFEST, 50);

	// Insert only one small audio playlist (50 bytes)
	InsertTestPlaylist("http://example.com/audio1.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 50);

	// Insert large video playlists (200 bytes total)
	InsertTestPlaylist("http://example.com/video1.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 100);
	InsertTestPlaylist("http://example.com/video2.m3u8", eMEDIATYPE_PLAYLIST_VIDEO, 100);

	// Current: manifest(50) + audio(50) + video(200) = 300 bytes
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
	EXPECT_TRUE(IsCached("http://example.com/audio1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video1.m3u8"));
	EXPECT_TRUE(IsCached("http://example.com/video2.m3u8"));

	// Insert large audio (200 bytes): total would be 500, exceeding 300 limit
	// Target for reduction: 300 - 200 = 100 bytes
	// First pass will remove AUDIO playlists (only 50 bytes removed)
	// After first pass BEFORE new insert: manifest(50) + video(200) = 250 bytes
	// 250 > 100? YES - second pass SHOULD trigger
	// Second pass will remove ALL non-manifest (video playlists removed)
	InsertTestPlaylist("http://example.com/audio2.m3u8", eMEDIATYPE_PLAYLIST_AUDIO, 200);

	// Verify: New audio added
	EXPECT_TRUE(IsCached("http://example.com/audio2.m3u8"));

	// Verify: Old audio removed by first pass
	EXPECT_FALSE(IsCached("http://example.com/audio1.m3u8"));

	// CRITICAL: Verify second pass DID trigger (videos removed)
	EXPECT_FALSE(IsCached("http://example.com/video1.m3u8"));
	EXPECT_FALSE(IsCached("http://example.com/video2.m3u8"));

	// Manifest always preserved
	EXPECT_TRUE(IsCached("http://example.com/manifest.mpd"));
}
