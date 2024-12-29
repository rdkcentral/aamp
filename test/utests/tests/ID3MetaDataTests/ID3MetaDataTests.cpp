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
#include "ID3Metadata.hpp"
#include "AampMediaType.h" 

using namespace testing;

class ID3MetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up code, if needed
    }
};

constexpr size_t id3v2_header_size = 10u;  //  should be defined in ID3Metadata.hpp

// Test case for IsValidMediaType function
TEST_F(ID3MetadataTest, IsValidMediaTypeTest) {
    AampMediaType type = eMEDIATYPE_AUDIO;
    bool s1 = aamp::id3_metadata::helpers::IsValidMediaType(type);
    EXPECT_TRUE(s1);
    AampMediaType type1 = eMEDIATYPE_VIDEO;
    bool s2 = aamp::id3_metadata::helpers::IsValidMediaType(type1);
    EXPECT_TRUE(s2);
    AampMediaType type2 = eMEDIATYPE_DSM_CC;
    bool s3 = aamp::id3_metadata::helpers::IsValidMediaType(type2);
    EXPECT_TRUE(s3);
}

// Test case for IsValidHeader function
TEST_F(ID3MetadataTest, IsValidHeaderTest) {
    uint8_t validHeader[id3v2_header_size]     = { 'I', 'D', '3', 2, 2,0,0,0,0,0};
    uint8_t invalidHeader[id3v2_header_size]   = { 'a', 'b', 'c', 2, 1,4,0,0,0,0};
    uint8_t wrongidentifier[id3v2_header_size] = { 'I', 'D', '4', 2, 2,0,0,0,0,0};

    EXPECT_TRUE(aamp::id3_metadata::helpers::IsValidHeader(validHeader, sizeof(validHeader)));
    EXPECT_FALSE(aamp::id3_metadata::helpers::IsValidHeader(invalidHeader, sizeof(invalidHeader)));
    EXPECT_FALSE(aamp::id3_metadata::helpers::IsValidHeader(wrongidentifier, sizeof(wrongidentifier)));

    // Test with data length less than min_id3_header_length
    uint8_t shortHeader[] = { 'I', 'D' };
    EXPECT_FALSE(aamp::id3_metadata::helpers::IsValidHeader(shortHeader, sizeof(shortHeader)));

    // Test with exactly min_id3_header_length
    //While calling IsValidHeader function for below test case it is getting fail.

    uint8_t exactlyMinHeader[] = { 'I', 'D', '3'};
    EXPECT_FALSE(aamp::id3_metadata::helpers::IsValidHeader(exactlyMinHeader,3));

}

TEST(DataSizeTest, ValidSyncSafeSize) {
    uint8_t validHeader[id3v2_header_size] = { 'I', 'D', '3', 2, 0, 0, 0x7F, 0, 0, 0 };
    size_t dataSize = aamp::id3_metadata::helpers::DataSize(validHeader);
    
    size_t expectedSize = (0x7F << 21) | (0 << 14) | (0 << 7) | 0 | id3v2_header_size;
    EXPECT_EQ(dataSize, expectedSize);
}

// Test case for invalid syncsafe-encoded tag size (bit 7 set)
TEST(DataSizeTest, InvalidSyncSafeSize) {
    uint8_t invalidHeader[id3v2_header_size] = { 'I', 'D', '3', 2, 0, 0, 0x8F, 0, 0, 0 };
    size_t dataSize = aamp::id3_metadata::helpers::DataSize(invalidHeader);
    EXPECT_EQ(dataSize, 0);
}

// Test case for zero tag size
TEST(DataSizeTest, ZeroSize) {
    uint8_t zeroSizeHeader[id3v2_header_size] = { 'I', 'D', '3', 2, 0, 0, 0, 0, 0, 0 };
    size_t dataSize = aamp::id3_metadata::helpers::DataSize(zeroSizeHeader);
    size_t expectedSize = id3v2_header_size;
    EXPECT_EQ(dataSize, expectedSize);
}

TEST_F(ID3MetadataTest, ToStringTest1) {
    uint8_t id3Data[] = {
        'I', 'D', '3', 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	size_t data_len = sizeof(id3Data);
    std::string result = aamp::id3_metadata::helpers::ToString(id3Data, data_len);

}


class MetadataCacheTest : public testing::Test {
protected:
    aamp::id3_metadata::MetadataCache Cache;

    void SetUp() override {
       
        Cache.Reset();
    }
};

TEST_F(MetadataCacheTest, UpdateAndCheckMetadata) {
    // Test updating and checking metadata for various media types and data
    
    AampMediaType videoMediaType = eMEDIATYPE_VIDEO;
    std::vector<uint8_t> videoData = {10, 11, 12, 13, 14};
    
    AampMediaType audioMediaType = eMEDIATYPE_AUDIO;
    std::vector<uint8_t> audioData = {20, 21, 22};
    
    // Update and check video metadata
    Cache.UpdateMetadataCache(videoMediaType, videoData);
    EXPECT_FALSE(Cache.CheckNewMetadata(videoMediaType, videoData)); // Existing data
    
    std::vector<uint8_t> newVideoData = {10, 11, 12, 13, 14, 15}; // Different length
    EXPECT_TRUE(Cache.CheckNewMetadata(videoMediaType, newVideoData)); // New data
    
    // Update and check audio metadata
    Cache.UpdateMetadataCache(audioMediaType, audioData);
    EXPECT_FALSE(Cache.CheckNewMetadata(audioMediaType, audioData)); // Existing data
    
    std::vector<uint8_t> newAudioData = {20, 21, 22, 23}; // Different data
    EXPECT_TRUE(Cache.CheckNewMetadata(audioMediaType, newAudioData)); // New data
}

TEST_F(MetadataCacheTest, CheckEmptyCache) {
    // Test checking empty cache for various media types
    
    AampMediaType videoMediaType = eMEDIATYPE_VIDEO;
    std::vector<uint8_t> videoData = {10, 11, 12, 13, 14};
    
    AampMediaType audioMediaType = eMEDIATYPE_AUDIO;
    std::vector<uint8_t> audioData = {20, 21, 22};
    
    // Check empty cache for video media type
    EXPECT_TRUE(Cache.CheckNewMetadata(videoMediaType, videoData)); // Cache is initially empty
    
    // Update cache with video data
    Cache.UpdateMetadataCache(videoMediaType, videoData);
    
    // Check empty cache for audio media type
    EXPECT_TRUE(Cache.CheckNewMetadata(audioMediaType, audioData)); // Cache is empty for audio media type

    Cache.UpdateMetadataCache(audioMediaType, audioData);

}

// Test fixture for CallbackData class
class CallbackDataTest : public testing::Test {
protected:
    void SetUp() override {
        // Initialize objects before each test if needed
        
    }
   
};

// Test cases for CallbackData methods
TEST_F(CallbackDataTest, ConstructorWithDataTest) {
    std::vector<uint8_t> data = {1,2,3,4};
    const char* schemeIdURI = "";
    const char* id3Value = "value1";
    uint64_t presTime = 12345;
    uint32_t id3ID = 567;
    uint32_t eventDur = 678;
    uint32_t tScale = 789;
    uint64_t tStampOffset = 98765;

    aamp::id3_metadata::CallbackData callbackData(data, schemeIdURI, id3Value, presTime, id3ID, eventDur, tScale, tStampOffset);
    
    EXPECT_EQ(callbackData.schemeIdUri, schemeIdURI);
    EXPECT_EQ(callbackData.value, id3Value);
    EXPECT_EQ(callbackData.presentationTime, presTime);
    EXPECT_EQ(callbackData.id, id3ID);
    EXPECT_EQ(callbackData.eventDuration, eventDur);
    EXPECT_EQ(callbackData.timeScale, tScale);
    EXPECT_EQ(callbackData.timestampOffset, tStampOffset);
    EXPECT_EQ(callbackData.mData, data);

 
}
TEST_F(CallbackDataTest, ConstructorWithNegativePresTimeTest) {
    std::vector<uint8_t> data = {1, 2, 3, 4};
    const char* schemeIdURI = "scheme1";
    const char* id3Value = "value1";
    uint64_t presTime = -12345;  // Negative presentation time
    uint32_t id3ID = -567;
    uint32_t eventDur = -678;
    uint32_t tScale = -789;
    uint64_t tStampOffset = -98765;

    aamp::id3_metadata::CallbackData callbackData(data, schemeIdURI, id3Value, presTime, id3ID, eventDur, tScale, tStampOffset);

    EXPECT_EQ(callbackData.schemeIdUri, schemeIdURI);
    EXPECT_EQ(callbackData.value, id3Value);
    EXPECT_EQ(callbackData.presentationTime, presTime); // Expecting the negative value
    EXPECT_EQ(callbackData.id, id3ID);
    EXPECT_EQ(callbackData.eventDuration, eventDur);
    EXPECT_EQ(callbackData.timeScale, tScale);
    EXPECT_EQ(callbackData.timestampOffset, tStampOffset);
    EXPECT_EQ(callbackData.mData, data);
}

// Test with long schemeIdURI and id3Value strings
TEST_F(CallbackDataTest, ConstructorWithLongStringsTest) {
    std::vector<uint8_t> data = {1, 2, 3, 4};
    const char* schemeIdURI = "ThisIsAVeryLongSchemeIdURIThatExceedsTheLimit";
    const char* id3Value = "ThisIsAVeryLongId3ValueThatExceedsTheLimit";
    uint64_t presTime = 12345;
    uint32_t id3ID = 567;
    uint32_t eventDur = 678;
    uint32_t tScale = 789;
    uint64_t tStampOffset = 98765;

    aamp::id3_metadata::CallbackData callbackData(data, schemeIdURI, id3Value, presTime, id3ID, eventDur, tScale, tStampOffset);

    EXPECT_EQ(callbackData.schemeIdUri, schemeIdURI); // Expecting the long string
    EXPECT_EQ(callbackData.value, id3Value);          // Expecting the long string
    EXPECT_EQ(callbackData.presentationTime, presTime);
    EXPECT_EQ(callbackData.id, id3ID);
    EXPECT_EQ(callbackData.eventDuration, eventDur);
    EXPECT_EQ(callbackData.timeScale, tScale);
    EXPECT_EQ(callbackData.timestampOffset, tStampOffset);
    EXPECT_EQ(callbackData.mData, data);
}

TEST_F(CallbackDataTest, ConstructorWithPtrTest) {
    const char* schemeIdURI = "scheme2";
    const char* id3Value = "value2";
    uint64_t presTime = 54321;
    uint32_t id3ID = 876;
    uint32_t eventDur = 987;
    uint32_t tScale = 876;
    uint64_t tStampOffset = 65432;

    uint8_t data[] = {4, 5, 6, 7};

    aamp::id3_metadata::CallbackData callbackData(data, sizeof(data), schemeIdURI, id3Value, presTime, id3ID, eventDur, tScale, tStampOffset);

    EXPECT_EQ(callbackData.schemeIdUri, schemeIdURI);
    EXPECT_EQ(callbackData.value, id3Value);
    EXPECT_EQ(callbackData.presentationTime, presTime);
    EXPECT_EQ(callbackData.id, id3ID);
    EXPECT_EQ(callbackData.eventDuration, eventDur);
    EXPECT_EQ(callbackData.timeScale, tScale);
    EXPECT_EQ(callbackData.timestampOffset, tStampOffset);

    // Check that the data attribute contains the same data as provided
    EXPECT_EQ(callbackData.mData.size(), sizeof(data));
    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(callbackData.mData[i], data[i]);
    }
  
}
