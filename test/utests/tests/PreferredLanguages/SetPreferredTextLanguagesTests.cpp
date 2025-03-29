/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#include <chrono>

#include "priv_aamp.h"
#include "AampConfig.h"

#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "MockAampUtils.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Throw;
using ::testing::An;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::AnyOf;
using ::testing::StrEq;

class SetPreferredTextLanguagesTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
		g_mockStreamAbstractionAAMP = new StrictMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		mPrivateInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;
		mPrivateInstanceAAMP->SetState(eSTATE_PLAYING);

		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));

   		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
	}

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		if (g_mockStreamAbstractionAAMP != nullptr)
		{
			delete g_mockStreamAbstractionAAMP;
			g_mockStreamAbstractionAAMP = nullptr;
		}

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete g_mockAampStreamSinkManager;
		g_mockAampStreamSinkManager = nullptr;
	}

public:
	/**
	 * @brief StreamAbstractionAAMP::Stop test helper method.
	 *
	 * When TeardownStream() is called as part of a retune, the
	 * StreamAbstractionAAMP instance is stopped and deleted. Clear the global
	 * mock instance here to avoid deleting this for a second time in
	 * TearDown().
	 */
	void Stop(bool clearChannelData)
	{
		g_mockStreamAbstractionAAMP = nullptr;
	}

	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
};

class SetPreferredTextLanguagesIso639Tests : public SetPreferredTextLanguagesTests,
								   public testing::WithParamInterface<const char*>
{
protected:
	void SetUp() override
	{
		SetPreferredTextLanguagesTests::SetUp();

		g_mockAampUtils = new NiceMock<MockAampUtils>();
	}

	void TearDown() override
	{
		SetPreferredTextLanguagesTests::TearDown();

		delete g_mockAampUtils;
		g_mockAampUtils = nullptr;
	}
};

/**
 * @brief Set the preferred text languages list which matches the current
 *        setting, using various ISO-639 codes.
 */
TEST_P(SetPreferredTextLanguagesIso639Tests, LanguageListTestIso639)
{
	const char* testLanguageList = GetParam();

	std::vector<TextTrackInfo> tracks;
	tracks.push_back(TextTrackInfo("idx0", "eng", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "spa", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredTextLanguages() without changing the preferred languages
	 * list. There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));

	// AAMP is expected to normalise the language code according to the current preference
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_LanguageCodePreference))
		.WillRepeatedly(Return(ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE));
	// The number of times the language code is normalised depends on the number of languages in the
	// list. English will also be normalised twice - once as the currently selected subtitle track,
	// and once as it's in the list of available subtitle tracks (alongside Spanish).
	int normalizationCount = 2 + (strchr(testLanguageList, ',') ? 2 : 1);
	EXPECT_CALL(*g_mockAampUtils,
				Getiso639map_NormalizeLanguageCode(AnyOf(StrEq("eng"), StrEq("en")),
												   ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE))
		.Times(normalizationCount)
		.WillRepeatedly(Return("eng"));
	EXPECT_CALL(*g_mockAampUtils, Getiso639map_NormalizeLanguageCode(
									  StrEq("spa"), ISO639_PREFER_3_CHAR_BIBLIOGRAPHIC_LANGCODE))
		.WillOnce(Return("spa"));

	// No retune
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredTextLanguages(testLanguageList);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "eng");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "eng");
}

INSTANTIATE_TEST_SUITE_P(SetPreferredTextLanguagesTests, SetPreferredTextLanguagesIso639Tests,
						 ::testing::Values("eng",	  /* ISO 639-3 (3 character code) */
										   "en",	  /* ISO 639-1 (2 character code) */
										   "eng,eng", /* Duplicate language, same code */
										   "en,eng",  /* Duplicate language, different code */
										   "{\"languages\":[\"eng\"]}", "{\"languages\":[\"en\"]}",
										   "{\"languages\":[\"eng\",\"eng\"]}",
										   "{\"languages\":[\"en\",\"eng\"]}",
										   /* Alternative ways of specifying a single language code
										   supported by the SetPreferredTextLanguages JSON API */
										   "{\"languages\":\"eng\"}", "{\"language\":\"en\"}"));

/**
 * @brief Set the preferred text languages list which does not match the current
 *        setting.
 */
TEST_F(SetPreferredTextLanguagesTests, LanguageListTest2)
{
	std::vector<TextTrackInfo> tracks;
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredTextLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("lang1");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred text languages list which doesn't match the current
 *        setting but there is no matching track.
 */
TEST_F(SetPreferredTextLanguagesTests, LanguageListTest3)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredTextLanguages() passing a language which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredTextLanguages("lang2");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang2");
}

/**
 * @brief Set the preferred text languages list as a JSON string which doesn't
 *        match the current setting.
 */
TEST_F(SetPreferredTextLanguagesTests, LanguageListTest4)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredTextLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"languages\":\"lang1\"}");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred text languages list as a JSON string array which
 *        contains multiple languages
 */
TEST_F(SetPreferredTextLanguagesTests, LanguageListTest5)
{
	std::vector<TextTrackInfo> tracks;
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredTextLanguages() changing the preferred languages list.
	 * There should be a retune as multiple languages are specified.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(AtLeast(1));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"languages\":[\"lang0\",\"lang1\"]}");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang0,lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 2);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang0");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(1).c_str(), "lang1");

	g_mockStreamAbstractionAAMP = nullptr;
}

/**
 * @brief TSB related test to change the preferred text languages list.
 */
TEST_F(SetPreferredTextLanguagesTests, LanguageListTest6)
{
	std::vector<TextTrackInfo> tracks;
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredTextLanguages() changing the preferred languages list.
	 * There should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));
	mPrivateInstanceAAMP->SetPreferredTextLanguages("lang1");

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief TSB related test to change the preferred text languages list to a track
 *        which is not enabled.
 */
TEST_F(SetPreferredTextLanguagesTests, LanguageListTest7)
{
	std::vector<TextTrackInfo> tracks;
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), false));

	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredTextLanguages() changing the preferred languages list but
	 * the matching track is disabled. There should be a retune and a new TSB
	 * requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("lang1");

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred text rendition which matches the current setting.
 */
TEST_F(SetPreferredTextLanguagesTests, RenditionTest1)
{
	std::vector<TextTrackInfo> tracks;

        tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	mPrivateInstanceAAMP->preferredTextRenditionString = "rend0";
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(1);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(2);
	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"rendition\":\"rend0\"}");

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextRenditionString.c_str(), "rend0");
	g_mockStreamAbstractionAAMP = NULL;
}

/**
 * @brief Set the preferred text rendition which doesn't match the current
 *        setting.
 */
TEST_F(SetPreferredTextLanguagesTests, RenditionTest2)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextRenditionString = "rend0";
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredLanguages() changing the preferred rendition. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"rendition\":\"rend1\"}");

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextRenditionString.c_str(), "rend1");
}

/**
 * @brief TSB related test to change the preferred text rendition.
 */
TEST_F(SetPreferredTextLanguagesTests, RenditionTest3)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextRenditionString = "rend0";
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredTextLanguages() changing the preferred rendition. There
	 * should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"rendition\":\"rend1\"}");

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextRenditionString.c_str(), "rend1");
}

/**
 * @brief TSB related test to change the preferred text rendition to a track
 *        which is not enabled.
 */
TEST_F(SetPreferredTextLanguagesTests, RenditionTest4)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), false));

	mPrivateInstanceAAMP->preferredTextRenditionString = "rend0";
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredTextLanguages() changing the preferred renditon but the
	 * matching track is disabled. There should be a retune and a new TSB
	 * requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"rendition\":\"rend1\"}");

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextRenditionString.c_str(), "rend1");
}

TEST_F(SetPreferredTextLanguagesTests, TextTrackNameTest1)
{
	mPrivateInstanceAAMP->preferredTextNameString = "English";
	//when Local TSB playback is in progress!!. SetPreferredTextLanguages() will be ignored
	mPrivateInstanceAAMP->SetLocalAAMPTsb(true);
	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"name\":\"Spanish\"}");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextNameString.c_str(), "English");

}

TEST_F(SetPreferredTextLanguagesTests, TextTrackNameTest2)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "English", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "Spanish", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));
	mPrivateInstanceAAMP->preferredTextNameString = "English";
	mPrivateInstanceAAMP->subtitles_muted = false;
	/* Call SetPreferredLanguages() without changing the preferred Name.
	* There should be no retune.
	*/
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"name\":\"English\"}");
	// Verify the preferred Name list.
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextNameString.c_str(), "English");
}

TEST_F(SetPreferredTextLanguagesTests, TextTrackNameTest3)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "English", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "Spanish", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextNameString = "English";
	mPrivateInstanceAAMP->subtitles_muted = false;

	/* Call SetPreferredLanguages() changing the preferred name. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"name\":\"Spanish\"}");

	/* Verify the preferred name list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextNameString.c_str(), "Spanish");

	/* Verify the preferred language is not set to an incorrect value */
	EXPECT_STRNE(mPrivateInstanceAAMP->preferredTextNameString.c_str(), "English");
}

TEST_F(SetPreferredTextLanguagesTests, TextTrackNameTest4)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "English", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "Spanish", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextNameString = "English";
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredTextLanguages() changing the preferred name. There
	 * should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"name\":\"Spanish\"}");

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred name list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextNameString.c_str(), "Spanish");
}

TEST_F(SetPreferredTextLanguagesTests, TextTrackNameTest5)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "English", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "Spanish", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), false));

	mPrivateInstanceAAMP->preferredTextNameString = "English";
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredTextLanguages() changing the preferred name but the
	 * matching track is disabled. There should be a retune and a new TSB
	 * requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"name\":\"Spanish\"}");

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred name list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextNameString.c_str(), "Spanish");
}
