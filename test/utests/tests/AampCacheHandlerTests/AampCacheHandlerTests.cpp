#include <gtest/gtest.h>
#include "AampCacheHandler.h"
#include "AampMediaType.h"

using namespace testing;
AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};
class AampCacheHandlerTest : public Test
{
protected:
    AampCacheHandler *handler = nullptr;
    void SetUp() override
    {
        handler = new AampCacheHandler(gpGlobalConfig->GetLoggerInstance());
        
    }
    void TearDown() override
    {
        delete handler;
        handler = nullptr;
    }
};

TEST_F(AampCacheHandlerTest, SetMaxInitFragCacheSizeTest)
{
    std::size_t retrievedsize;

    std::size_t cacheSize = MAX_INIT_FRAGMENT_CACHE_PER_TRACK; // 1-5
    handler->SetMaxInitFragCacheSize(cacheSize);
    retrievedsize = handler->GetMaxInitFragCacheSize();
    EXPECT_EQ(retrievedsize, cacheSize);
    handler->SetMaxInitFragCacheSize(0);
    retrievedsize = handler->GetMaxInitFragCacheSize();
    EXPECT_EQ(retrievedsize, 0);
    handler->SetMaxInitFragCacheSize(-1);
    retrievedsize = handler->GetMaxInitFragCacheSize();
    EXPECT_EQ(retrievedsize, -1);
    //
}
TEST_F(AampCacheHandlerTest, SetMaxPlaylistCacheSizeTest)
{
    std::size_t retrievedsize;
    std::size_t cacheSize = MAX_PLAYLIST_CACHE_SIZE;
    handler->SetMaxPlaylistCacheSize(cacheSize);
    retrievedsize = handler->GetMaxPlaylistCacheSize();
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

    AampGrowableBuffer *buffer;
    std::string effectiveUrl;
    MediaType type;
    type = eMEDIATYPE_INIT_VIDEO;

    buffer = new AampGrowableBuffer("InitFragCache_Data");
    // Inserting the Url and trying to retrieve with empty buffer
    handler->InsertToInitFragCache(url1, buffer, effectiveUrl, type);
    bool res01 = handler->RetrieveFromInitFragCache(url1, buffer, effectiveUrl);
    EXPECT_FALSE(res01);

    //initializing buffer
    const char *srcData1[30] = {"HelloWorld"};
    size_t arraySize1 = sizeof(srcData1) / sizeof(srcData1[0]);
    buffer->AppendBytes(srcData1, arraySize1);
    // Inserting the Url and trying to retrieve with non-empty buffer
    handler->InsertToInitFragCache(url1, buffer, effectiveUrl, type);
    bool res1 = handler->RetrieveFromInitFragCache(url1, buffer, effectiveUrl);
    EXPECT_TRUE(res1);


    // Without Inserting the Url trying to retrieve
    bool res2 = handler->RetrieveFromInitFragCache(url2, buffer, effectiveUrl);
    EXPECT_FALSE(res2);
    // Inserting the Url beyond the MaxInitCacheSlot and performing the RemoveInitFragCacheEntry ,later trying trying to retrieve the removed Url
    handler->InsertToInitFragCache(url2, buffer, effectiveUrl, type);
    handler->InsertToInitFragCache(url3, buffer, effectiveUrl, type);
    handler->InsertToInitFragCache(url4, buffer, effectiveUrl, type);
    handler->InsertToInitFragCache(url5, buffer, effectiveUrl, type);
    handler->InsertToInitFragCache(url6, buffer, effectiveUrl, type);
    bool res3 = handler->RetrieveFromInitFragCache(url1, buffer, effectiveUrl);
    EXPECT_FALSE(res3);
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

    AampGrowableBuffer *buffer;
    std::string effectiveUrl;
    bool trackLiveStatus;
    MediaType type;

    buffer = new AampGrowableBuffer("PlaylistCache_Data");


    // Setting the tracklivestatus as true and inserting Init playlist
    handler->InsertToPlaylistCache(url1, buffer, effectiveUrl, true, eMEDIATYPE_INIT_VIDEO);
    bool res1 = handler->IsUrlCached(url1);
    EXPECT_FALSE(res1);

    // Inserting the Url and trying to retrieve with empty buffer
    handler->InsertToPlaylistCache(url2, buffer, effectiveUrl, false, eMEDIATYPE_INIT_VIDEO);
    bool res01 = handler->IsUrlCached(url2);
    EXPECT_FALSE(res01);

    //initializing buffer
    const char *srcData3[30] = {"HelloWorld"};
    size_t arraySize3 = sizeof(srcData3) / sizeof(srcData3[0]);
    buffer->AppendBytes(srcData3, arraySize3);
    // Inserting the playlist and trying to retrieve with non-empty buffer
    handler->InsertToPlaylistCache(url2, buffer, effectiveUrl, false, eMEDIATYPE_INIT_VIDEO);
    bool res2 = handler->IsUrlCached(url2);
    EXPECT_TRUE(res2);
    // If new Manifest is inserted which is not present in the cache , flush out other playlist files related with old manifest,
    handler->InsertToPlaylistCache(mpdurl, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
    bool res3 = handler->IsUrlCached(url2);
    EXPECT_FALSE(res3);
    // Removing the Url and trying to check whether the Url is present or not
    handler->RemoveFromPlaylistCache(mpdurl);
    bool res4 = handler->IsUrlCached(mpdurl);
    EXPECT_FALSE(res4);

    // Inserting the manifest and trying to retrieve it
    handler->InsertToPlaylistCache(url3, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
    bool res5 = handler->RetrieveFromPlaylistCache(url3, buffer, effectiveUrl);
    EXPECT_TRUE(res5);

    // Trying to Insert Url when the buffer size is greater than MaxPlaylistCacheSize
    const char *srcData1[30] = {"HelloWorld"};
    size_t arraySize1 = sizeof(srcData1) / sizeof(srcData1[0]);
    buffer->AppendBytes(srcData1, arraySize1);
    handler->SetMaxPlaylistCacheSize(20);
    handler->InsertToPlaylistCache(url4, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
    bool res6 = handler->IsUrlCached(url4);
    EXPECT_FALSE(res6);

    buffer->Clear();

    // Trying to Insert Url when the buffer size is lesser than MaxPlaylistCacheSize
    const char *srcData2[20] = {"HelloWorld"};
    size_t arraySize2 = sizeof(srcData2) / sizeof(srcData2[0]);
    buffer->AppendBytes(srcData2, arraySize2);
    handler->SetMaxPlaylistCacheSize(30);
    handler->InsertToPlaylistCache(url5, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
    bool res7 = handler->IsUrlCached(url5);
    EXPECT_TRUE(res7);

    // when effectiveUrl and Url is same
    effectiveUrl = "http://example6.com";

    handler->InsertToPlaylistCache(url6, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
    bool res8 = handler->IsUrlCached(url6);
    EXPECT_TRUE(res8);

    // when effectiveUrl and Url is not same
    effectiveUrl = "http://notsameurl.com";
    handler->InsertToPlaylistCache(url7, buffer, effectiveUrl, false, eMEDIATYPE_MANIFEST);
    bool res9 = handler->IsUrlCached(url7);
    EXPECT_TRUE(res9);
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
        TestableAampCacheHandler(AampLogManager *logObj)
            : AampCacheHandler(logObj)
        {
        }

        // Expose the protected functions for testing
        void CallInit()
        {
            Init();
        }

        void CallClearCacheHandler()
        {
            ClearCacheHandler();
        }

        void CallAsyncCacheCleanUpTask()
        {
            AsyncCacheCleanUpTask();
        }

        void CallClearPlaylistCache()
        {
            ClearPlaylistCache();
        }

        bool CallAllocatePlaylistCacheSlot(MediaType fileType, size_t newLen)
        {
            return AllocatePlaylistCacheSlot(fileType, newLen);
        }

        void CallClearInitFragCache()
        {
            ClearInitFragCache();
        }

        void CallRemoveInitFragCacheEntry(MediaType fileType)
        {
            RemoveInitFragCacheEntry(fileType);
        }
    };

    AampLogManager *mLogObj;
    TestableAampCacheHandler *mTestableAampCacheHandler;

    void SetUp() override
    {
        mLogObj = new AampLogManager();
        mTestableAampCacheHandler = new TestableAampCacheHandler(mLogObj);
    }

    void TearDown() override
    {
        delete mTestableAampCacheHandler;
        mTestableAampCacheHandler = nullptr;

        delete mLogObj;
        mLogObj = nullptr;
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

TEST_F(AampCacheHandlerTest_1, TestClearPlaylistCache)
{
    mTestableAampCacheHandler->CallClearPlaylistCache();

}

TEST_F(AampCacheHandlerTest_1, TestAllocatePlaylistCacheSlot)
{
    MediaType fileType = MediaType::eMEDIATYPE_DEFAULT; // or any other valid media type
    size_t newLen = 100; // or any other valid size
    bool result = mTestableAampCacheHandler->CallAllocatePlaylistCacheSlot(fileType, newLen);

}

TEST_F(AampCacheHandlerTest_1, TestClearInitFragCache)
{
    mTestableAampCacheHandler->CallClearInitFragCache();

}

TEST_F(AampCacheHandlerTest_1, TestRemoveInitFragCacheEntry)
{
    MediaType fileType = MediaType::eMEDIATYPE_DEFAULT; // or any other valid media type
    mTestableAampCacheHandler->CallRemoveInitFragCacheEntry(fileType);

}
