/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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
#include <gmock/gmock.h>
#include <cjson/cJSON.h>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "TextTrackInfo.h"
#include "MockCJsonManager.h"

// External declaration of global mock pointer
extern MockCJsonManager *g_mockCJsonManager;

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::ReturnRef;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::AtLeast;

// Fake cJSON objects for mock returns
static cJSON fake_array_object = { nullptr, nullptr, nullptr, cJSON_Array, nullptr, 0, 0.0, nullptr };
static cJSON fake_object = { nullptr, nullptr, nullptr, cJSON_Object, nullptr, 0, 0.0, nullptr };
static cJSON fake_item = { nullptr, nullptr, nullptr, cJSON_String, nullptr, 0, 0.0, nullptr };

class ClosedCaptionTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
	std::vector<TextTrackInfo> mockTextTracks;

	void SetUp() override
	{
		// Initialize config and PrivateInstanceAAMP object first
		gpGlobalConfig = new AampConfig();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		// Create mocks (StreamAbstractionAAMP requires PrivateInstanceAAMP*)
		g_mockStreamAbstractionAAMP = new MockStreamAbstractionAAMP(mPrivateInstanceAAMP);
		g_mockAampGstPlayer = new MockAAMPGstPlayer(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager = new MockAampStreamSinkManager();

		// Set the stream abstraction in the PrivateInstanceAAMP object
		mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;

		// Create and set up cJSON mock manager
		g_mockCJsonManager = new MockCJsonManager();

		// Setup the mock data
		setupMockTextTracks();
	}

	void TearDown() override
	{
		// Clear the stream abstraction pointer before deleting PrivateInstanceAAMP
		// to avoid double deletion
		if (mPrivateInstanceAAMP) {
			mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;
		}

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;

		// Clean up cJSON mock
		delete g_mockCJsonManager;
		g_mockCJsonManager = nullptr;

		mockTextTracks.clear();
	}

public:
	// Helper function to set up basic cJSON expectations (array creation, object creation, etc.)
	std::vector<cJSON*> setupBasicCJsonExpectations(cJSON* mockArray, cJSON* mockItem)
	{
		// Generate unique mock pointers for each track object
		std::vector<cJSON*> mockObjects;
		for (size_t i = 0; i < mockTextTracks.size(); i++) {
			mockObjects.push_back(reinterpret_cast<cJSON*>(0x7000 + i));
		}

		EXPECT_CALL(*g_mockCJsonManager, CreateArray())
			.WillOnce(Return(mockArray));

		// Set up object creation expectations - one for each track
		EXPECT_CALL(*g_mockCJsonManager, CreateObject())
			.WillOnce(Return(mockObjects[0]))
			.WillOnce(Return(mockObjects[1]))
			.WillOnce(Return(mockObjects[2]))
			.WillOnce(Return(mockObjects[3]));

		return mockObjects;
	}

	// Helper function to set up string field expectations for a track
	void setupTrackStringFieldExpectations(const TextTrackInfo& track, cJSON* mockObj, cJSON* mockItem)
	{
		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("name"), ::testing::StrEq(track.name)))
			.WillOnce(Return(mockItem));
		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("label"), ::testing::StrEq(track.label)))
			.WillOnce(Return(mockItem));

		// Add sub-type field based on isCC flag
		std::string expectedSubType = track.isCC ? "CLOSED-CAPTIONS" : "SUBTITLES";
		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("sub-type"), ::testing::StrEq(expectedSubType)))
			.WillOnce(Return(mockItem));

		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("language"), ::testing::StrEq(track.language)))
			.WillOnce(Return(mockItem));
		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("rendition"), ::testing::StrEq(track.rendition)))
			.WillOnce(Return(mockItem));
		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("instreamId"), ::testing::StrEq(track.instreamId)))
			.WillOnce(Return(mockItem));
		EXPECT_CALL(*g_mockCJsonManager, AddStringToObject(mockObj, ::testing::StrEq("codec"), ::testing::StrEq(track.codec)))
			.WillOnce(Return(mockItem));
	}

	// Helper function to set up availability expectation for a track
	void setupTrackAvailabilityExpectation(const TextTrackInfo& track, cJSON* mockObj, cJSON* mockItem, bool expectedAvailability)
	{
		EXPECT_CALL(*g_mockCJsonManager, AddBoolToObject(mockObj, ::testing::StrEq("availability"), expectedAvailability))
			.WillOnce(Return(mockItem));
	}

	// Helper function to set up array finalization expectations
	void setupArrayFinalizationExpectations(cJSON* mockArray, const std::vector<cJSON*>& mockObjects)
	{
		for (size_t i = 0; i < mockTextTracks.size(); i++) {
			EXPECT_CALL(*g_mockCJsonManager, AddItemToArray(mockArray, mockObjects[i]))
				.WillOnce(Return(cJSON_True));
		}

		// Set up a lenient expectation for Print() - we don't care about the exact output
		// since our real verification is in the structure building calls above
		EXPECT_CALL(*g_mockCJsonManager, Print(mockArray))
			.WillOnce(Return("[]")); // Return minimal valid JSON - actual content doesn't matter for this test

		EXPECT_CALL(*g_mockCJsonManager, Delete(mockArray))
			.Times(1);
	}

	// Helper function to calculate TSB availability for a track
	bool calculateTsbAvailability(const TextTrackInfo& track, const std::string& currentTrackIndex, bool getCurrentTrackSuccess)
	{
		if (track.isCC) {
			return true;  // CC tracks are always available in TSB mode
		} else if (!getCurrentTrackSuccess) {
			return false; // No subtitle tracks available when GetCurrentTextTrack fails
		} else {
			return (track.index == currentTrackIndex); // Only current track is available
		}
	}

private:
	void setupMockTextTracks()
	{
		// Create sample text tracks for testing
		TextTrackInfo track1;
		track1.index = "0";
		track1.language = "en";
		track1.isCC = true;
		track1.rendition = "main";
		track1.name = "English CC";
		track1.instreamId = "CC1";
		track1.codec = "stpp";
		track1.label = "English Closed Captions";
		track1.isAvailable = true;

		TextTrackInfo track2;
		track2.index = "1";
		track2.language = "es";
		track2.isCC = true;
		track2.rendition = "main";
		track2.name = "Spanish CC";
		track2.instreamId = "CC2";
		track2.codec = "stpp";
		track2.label = "Spanish Closed Captions";
		track2.isAvailable = true;

		TextTrackInfo track3;
		track3.index = "2";
		track3.language = "fr";
		track3.isCC = false;  // This is a subtitle, not CC
		track3.rendition = "main";
		track3.name = "French Subtitles";
		track3.instreamId = "SUB1";
		track3.codec = "stpp";
		track3.label = "French Subtitles";
		track3.isAvailable = true;

		TextTrackInfo track4;
		track4.index = "3";
		track4.language = "de";
		track4.isCC = false;  // This is a subtitle, not CC
		track4.rendition = "main";
		track4.name = "German Subtitles";
		track4.instreamId = "SUB2";
		track4.codec = "stpp";
		track4.label = "German Subtitles";
		track4.isAvailable = true;

		mockTextTracks.push_back(track1);
		mockTextTracks.push_back(track2);
		mockTextTracks.push_back(track3);
		mockTextTracks.push_back(track4);
	}
};

TEST_F(ClosedCaptionTests, GetAvailableTextTracks_WithValidTracks_ReturnsAllAvailableTracks)
{
	cJSON *mockArray = reinterpret_cast<cJSON*>(0x1001);
	cJSON *mockItem = reinterpret_cast<cJSON*>(0x1003);

	std::vector<cJSON*> mockObjects = setupBasicCJsonExpectations(mockArray, mockItem);

	// Set up expectations for each track
	for (size_t i = 0; i < mockTextTracks.size(); i++) {
		const auto& track = mockTextTracks[i];
		cJSON* mockObj = mockObjects[i];

		// Set up string field expectations - verify production code adds correct fields with correct values
		setupTrackStringFieldExpectations(track, mockObj, mockItem);

		// In normal mode (non-TSB), all tracks are available
		setupTrackAvailabilityExpectation(track, mockObj, mockItem, true);
	}

	// Set up array finalization expectations - verify objects are added to array correctly
	setupArrayFinalizationExpectations(mockArray, mockObjects);

	// Mock StreamAbstraction to return our test tracks
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(false))
		.WillOnce(ReturnRef(mockTextTracks));

	std::string result = mPrivateInstanceAAMP->GetAvailableTextTracks(false);

	// Assert - The REAL test verification is in the EXPECT_CALL expectations above,
	// which ensure the correct cJSON structure is built with the right field names and values.

	// We only do basic sanity checks since the mock Print returns minimal JSON
	EXPECT_FALSE(result.empty()) << "Should return non-empty result when tracks are available";
}

TEST_F(ClosedCaptionTests, GetAvailableTextTracks_WithEmptyTracks_ReturnsEmptyString)
{
	// Create empty tracks vector
	std::vector<TextTrackInfo> emptyTracks;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(false))
		.WillOnce(ReturnRef(emptyTracks));

	// No cJSON expectations needed - function returns early when no tracks available

	std::string result = mPrivateInstanceAAMP->GetAvailableTextTracks(false);

	// Should return empty string when no tracks are available
	EXPECT_EQ(result, "") << "Should return empty string when no tracks are available";
}

TEST_F(ClosedCaptionTests, GetAvailableTextTracks_WithNullStreamAbstraction_ReturnsEmptyString)
{
	// Set stream abstraction to null
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;

	std::string result = mPrivateInstanceAAMP->GetAvailableTextTracks(false);

	// Should return empty string when stream abstraction is null
	EXPECT_TRUE(result.empty());
}

TEST_F(ClosedCaptionTests, GetAvailableTextTracks_WithLocalAAMPTsb_CallsGetCurrentTextTrack)
{
	cJSON *mockArray = reinterpret_cast<cJSON*>(0x5001);
	cJSON *mockItem = reinterpret_cast<cJSON*>(0x5003);

	// Set up basic cJSON expectations (array, object creation)
	std::vector<cJSON*> mockObjects = setupBasicCJsonExpectations(mockArray, mockItem);

	// Set up expectations for each track - verify TSB availability logic
	std::string currentTrackIndex = "2"; // French Subtitles is the current track
	for (size_t i = 0; i < mockTextTracks.size(); i++) {
		const auto& track = mockTextTracks[i];
		cJSON* mockObj = mockObjects[i];

		// Set up string field expectations - verify production code adds correct fields
		setupTrackStringFieldExpectations(track, mockObj, mockItem);

		// Calculate and verify TSB availability logic: CC tracks always available,
		// subtitle tracks only available if they match current track
		bool expectedAvailability = calculateTsbAvailability(track, currentTrackIndex, true);
		setupTrackAvailabilityExpectation(track, mockObj, mockItem, expectedAvailability);
	}

	// Set up array finalization expectations - verify objects are added to array correctly
	setupArrayFinalizationExpectations(mockArray, mockObjects);

	// Enable local AAMP TSB mode
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	// Setup current text track info (this will be the "currently selected" track)
	TextTrackInfo currentTrack;
	currentTrack.index = currentTrackIndex;
	currentTrack.language = "fr";
	currentTrack.isCC = false;

	// Mock the GetCurrentTextTrack call to succeed
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetCurrentTextTrack(::testing::_))
		.WillOnce(::testing::DoAll(
			::testing::SetArgReferee<0>(currentTrack),
			::testing::Return(true)
		));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(false))
		.WillOnce(ReturnRef(mockTextTracks));

	std::string result = mPrivateInstanceAAMP->GetAvailableTextTracks(false);

	// Basic sanity check since the mock Print returns minimal JSON
	EXPECT_FALSE(result.empty()) << "Should return non-empty result in TSB mode";
}

TEST_F(ClosedCaptionTests, GetAvailableTextTracks_WithLocalAAMPTsb_GetCurrentTextTrackFails)
{
	cJSON *mockArray = reinterpret_cast<cJSON*>(0x6001);
	cJSON *mockItem = reinterpret_cast<cJSON*>(0x6003);

	// Set up basic cJSON expectations (array, object creation)
	std::vector<cJSON*> mockObjects = setupBasicCJsonExpectations(mockArray, mockItem);

	// Set up expectations for each track - verify TSB failure availability logic
	for (size_t i = 0; i < mockTextTracks.size(); i++) {
		const auto& track = mockTextTracks[i];
		cJSON* mockObj = mockObjects[i];

		// Set up string field expectations - verify production code adds correct fields
		setupTrackStringFieldExpectations(track, mockObj, mockItem);

		// Calculate and verify TSB failure availability logic: CC tracks still available,
		// but no subtitle tracks available when GetCurrentTextTrack fails
		bool expectedAvailability = calculateTsbAvailability(track, "", false);
		setupTrackAvailabilityExpectation(track, mockObj, mockItem, expectedAvailability);
	}

	// Set up array finalization expectations - verify objects are added to array correctly
	setupArrayFinalizationExpectations(mockArray, mockObjects);

	// Enable local AAMP TSB mode
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);

	// Mock GetCurrentTextTrack to return false (failure case)
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetCurrentTextTrack(::testing::_))
		.WillOnce(::testing::Return(false));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(false))
		.WillOnce(ReturnRef(mockTextTracks));

	std::string result = mPrivateInstanceAAMP->GetAvailableTextTracks(false);

	// Basic sanity check since the mock Print returns minimal JSON
	EXPECT_FALSE(result.empty()) << "Should return non-empty result even when GetCurrentTextTrack fails";
}

TEST_F(ClosedCaptionTests, GetAvailableTextTracks_WithDisableWebVTT_ReturnsOnlyCCTracks)
{
    // Enable DisableWebVTT so only Closed Caption (CC) tracks should be returned
    gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_DisableWebVTT, true);

    // Copy only CC tracks from the mock list into a separate vector
    std::vector<TextTrackInfo> textTracksCopy;
    std::copy_if(begin(mockTextTracks), end(mockTextTracks), back_inserter(textTracksCopy),
                 [](const TextTrackInfo& e){ return e.isCC; });

    // Mock JSON array + item pointers (fake addresses used only for mocking)
    cJSON* mockArray = reinterpret_cast<cJSON*>(0x3000);
    cJSON* mockItem  = reinterpret_cast<cJSON*>(0x3003);

    // Create a list of fake JSON object pointers, one per CC track
    std::vector<cJSON*> mockObjects;
    for (size_t i = 0; i < textTracksCopy.size(); i++) {
        mockObjects.push_back(reinterpret_cast<cJSON*>(0x4000 + i));
    }

    // Expect CreateArray() to be called once and return our mock array pointer
    EXPECT_CALL(*g_mockCJsonManager, CreateArray())
        .WillOnce(Return(mockArray));

    // Expect CreateObject() to be called once per CC track and return mock objects
    EXPECT_CALL(*g_mockCJsonManager, CreateObject())
        .WillOnce(Return(mockObjects[0]))
        .WillOnce(Return(mockObjects[1]));

    // For each CC track, set expectations for JSON field creation and availability flags
    for (size_t i = 0; i < textTracksCopy.size(); i++) {
        const auto& track = textTracksCopy[i];
        cJSON* mockObj = mockObjects[i];

        // Expect production code to add correct string fields (e.g., name, language)
        setupTrackStringFieldExpectations(track, mockObj, mockItem);

        // Expect production code to mark the track as available (non-TSB mode)
        setupTrackAvailabilityExpectation(track, mockObj, mockItem, true);
    }

    // Expect AddItemToArray() to be called for each CC track
    for (size_t i = 0; i < textTracksCopy.size(); i++) {
        EXPECT_CALL(*g_mockCJsonManager, AddItemToArray(mockArray, mockObjects[i]))
            .WillOnce(Return(cJSON_True));
    }

    // Allow Print() to be called and return minimal JSON (actual content not validated here)
    EXPECT_CALL(*g_mockCJsonManager, Print(mockArray))
        .WillOnce(Return("[]"));

    // Expect Delete() to be called once to free the JSON array
    EXPECT_CALL(*g_mockCJsonManager, Delete(mockArray))
        .Times(1);

    // Mock StreamAbstraction to return only CC tracks
    EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(false))
        .WillOnce(ReturnRef(textTracksCopy));

    // Execute the function under test
    std::string result = mPrivateInstanceAAMP->GetAvailableTextTracks(false);

    // Validate that output is not empty (should contain CC-only JSON)
    EXPECT_FALSE(result.empty()) << "Expected CC-only JSON output";

    // Reset config to default
    gpGlobalConfig->SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_DisableWebVTT, false);
}
