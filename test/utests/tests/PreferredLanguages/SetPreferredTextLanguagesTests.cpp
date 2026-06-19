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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampTSBSessionManager.h"
#include "PlayerCCManager.h"

#include "MockTSBSessionManager.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "MockAampUtils.h"
#include "MockPlayerCCManager.h"
#include "MockStreamAbstractionAAMP_MPD.h"

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
using ::testing::Invoke;

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
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockAampGstPlayer = std::make_shared<MockAAMPGstPlayer>( mPrivateInstanceAAMP);
		auto *rawMock = new StrictMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);
		/* No-op deleter: mpStreamAbstractionAAMP (a raw pointer in PrivateInstanceAAMP)
		 * takes ownership of rawMock and deletes it via SAFE_DELETE on retune.
		 * The shared_ptr is a non-owning observation handle; reset() only clears
		 * the handle, not the object. */
		g_mockStreamAbstractionAAMP = std::shared_ptr<MockStreamAbstractionAAMP>(rawMock, [](MockStreamAbstractionAAMP*){});
		g_mockAampStreamSinkManager = std::make_shared<NiceMock<MockAampStreamSinkManager>>();
		g_mockPlayerCCManager = std::make_shared<NiceMock<MockPlayerCCManager>>();
		g_mockStreamAbstractionAAMP_MPD = std::make_shared<NiceMock<MockStreamAbstractionAAMP_MPD>>(mPrivateInstanceAAMP, 0, 0);
		mPrivateInstanceAAMP->mpStreamAbstractionAAMP = rawMock;
		mPrivateInstanceAAMP->SetState(eSTATE_PLAYING, true);

		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer.get()));
	}

	void TearDown() override
	{
		/* Production deletes mpStreamAbstractionAAMP on retune (SAFE_DELETE in
		 * TeardownStream). If no retune occurred, delete it here to avoid a leak. */
		if (mPrivateInstanceAAMP->mpStreamAbstractionAAMP != nullptr)
		{
			delete mPrivateInstanceAAMP->mpStreamAbstractionAAMP;
			mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;
		}
		g_mockStreamAbstractionAAMP.reset();

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		g_mockAampGstPlayer.reset();

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		g_mockAampConfig.reset();

		g_mockAampStreamSinkManager.reset();

		g_mockPlayerCCManager.reset();

		g_mockStreamAbstractionAAMP_MPD.reset();
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
		g_mockStreamAbstractionAAMP.reset();
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

		g_mockAampUtils = std::make_shared<NiceMock<MockAampUtils>>();
	}

	void TearDown() override
	{
		SetPreferredTextLanguagesTests::TearDown();

		g_mockAampUtils.reset();
	}
};

class SetPreferredTextLanguagesTsbSessionManager : public PrivateInstanceAAMP
	{
public:
	SetPreferredTextLanguagesTsbSessionManager(AampConfig *config):PrivateInstanceAAMP(config)
	{
	}

	void SetTsbSessionManager()
	{
		AampTSBSessionManager *aampTsbSessionManager = new AampTSBSessionManager(this);
		mTSBSessionManager = aampTsbSessionManager;
	}
	~SetPreferredTextLanguagesTsbSessionManager()
	{
    	if (mTSBSessionManager)
    	{
        	delete mTSBSessionManager;
        	mTSBSessionManager = nullptr;
    	}
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

	/* Verify SetPreferredTextTrack() was called and stored the track */
	const TextTrackInfo& preferredTrack = mPrivateInstanceAAMP->GetPreferredTextTrack();
	EXPECT_STREQ(preferredTrack.language.c_str(), "lang0");
	EXPECT_STREQ(preferredTrack.index.c_str(), "idx0");
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

	g_mockStreamAbstractionAAMP.reset();
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

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"rendition\":\"rend0\"}");

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextRenditionString.c_str(), "rend0");
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



TEST_F(SetPreferredTextLanguagesTests, SetTsbSessionManagerNull)
{
	std::vector<TextTrackInfo> tracks;
	std::unique_ptr<SetPreferredTextLanguagesTsbSessionManager> testp_aamp(new SetPreferredTextLanguagesTsbSessionManager(gpGlobalConfig));

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	testp_aamp->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP.get();
	testp_aamp->preferredTextLanguagesString = "lang0";
	testp_aamp->preferredTextLanguagesList.clear();
	testp_aamp->preferredTextLanguagesList.push_back("lang0");
	testp_aamp->subtitles_muted = false;
	testp_aamp->SetLocalAAMPTsb(true);
	testp_aamp->SetState(eSTATE_PLAYING, true);

	/* Call SetPreferredTextLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	// Expect that session manager is nullptr
	EXPECT_EQ(g_mockTSBSessionManager, nullptr);

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.Times(1).WillRepeatedly(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));

	// This test sets IsLocalAAMPTsb=true, so the mock is not deleted by the code-under-test.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_)).WillRepeatedly(Return());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SetCurrentTextTrackIndex(_))
		.Times(1);

	testp_aamp->SetPreferredTextLanguages("{\"languages\":\"lang1\"}");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(testp_aamp->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(testp_aamp->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(testp_aamp->preferredTextLanguagesList.at(0).c_str(), "lang1");

	// Expect that session manager is nullptr
	EXPECT_EQ(g_mockTSBSessionManager, nullptr);

	// The test must manually clean up the mock. Nullify all pointers to it BEFORE deleting
	// to prevent re-entrant calls from the mock's destructor, then delete the mock.
	auto mockToDelete = g_mockStreamAbstractionAAMP;
	g_mockStreamAbstractionAAMP.reset();
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;
	testp_aamp->mpStreamAbstractionAAMP = nullptr;
	delete mockToDelete.get();
}


/**
 * @brief TSB related test to change the preferred text languages list to a track
 *        which is not enabled.
*/

TEST_F(SetPreferredTextLanguagesTests, ChangePrefTextLangWithTSB)
{
	std::vector<TextTrackInfo> tracks;
	std::unique_ptr<SetPreferredTextLanguagesTsbSessionManager> testp_aamp(new SetPreferredTextLanguagesTsbSessionManager(gpGlobalConfig));

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	testp_aamp->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP.get();
	testp_aamp->preferredTextLanguagesString = "lang0";
	testp_aamp->preferredTextLanguagesList.clear();
	testp_aamp->preferredTextLanguagesList.push_back("lang0");
	testp_aamp->subtitles_muted = false;
	testp_aamp->SetLocalAAMPTsb(true);
	testp_aamp->SetTsbSessionManager();
	testp_aamp->SetState(eSTATE_PLAYING, true);
	g_mockTSBSessionManager = std::make_shared<NiceMock<MockTSBSessionManager>>(testp_aamp.get());

	/* Call SetPreferredTextLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]),Return(true)));
	// This test sets IsLocalAAMPTsb=true, so the mock is not deleted by the code-under-test.
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_)).Times(2).WillRepeatedly(Return());
	// SetCurrentTextTrackIndex is called in TSB scenarios
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SetCurrentTextTrackIndex(_))
		.Times(1);
	testp_aamp->SetPreferredTextLanguages("{\"languages\":\"lang1\"}");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(testp_aamp->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(testp_aamp->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(testp_aamp->preferredTextLanguagesList.at(0).c_str(), "lang1");

	// The test must manually clean up the mock. Nullify all pointers to it BEFORE deleting
	// to prevent re-entrant calls from the mock's destructor, then delete the mock.
	auto mockToDelete = g_mockStreamAbstractionAAMP;
	g_mockStreamAbstractionAAMP.reset();
	mPrivateInstanceAAMP->mpStreamAbstractionAAMP = nullptr;
	testp_aamp->mpStreamAbstractionAAMP = nullptr;
	delete mockToDelete.get();
	g_mockTSBSessionManager.reset();
}

/**
 * @brief Change between closed caption tracks
 * Check that a new closed caption track is selected in PlayerCCManagerBase
 * Check that there is no tune (Stop) called on StreamAbstractionAAMP
 */
TEST_F(SetPreferredTextLanguagesTests, ClosedCaptionTest1)
{
	std::vector<TextTrackInfo> tracks;

	//TextTrackInfo(std::string idx, std::string lang, bool cc, std::string rend, std::string trackName, std::string id, std::string cha, int pk):
	tracks.push_back(TextTrackInfo("idx0", "lang0", true, "rend0", "trackName0", "CC0", "cha0", 0));
	tracks.push_back(TextTrackInfo("idx1", "lang1", true, "rend1", "trackName1", "CC1", "cha1", 1));

	/* Set initial preferred language to lang0 */
	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_)).Times(0);
	EXPECT_CALL(*g_mockPlayerCCManager, SetTrack("CC1",eCLOSEDCAPTION_FORMAT_608)).Times(1).WillRepeatedly(Return(0));
	// SetCurrentTextTrackIndex is called for closed caption track changes
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SetCurrentTextTrackIndex(_))
		.Times(1);

	mPrivateInstanceAAMP->SetPreferredTextLanguages("lang1");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredTextLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesList.at(0).c_str(), "lang1");

	/* Verify SetPreferredTextTrack() was called and stored the track */
	const TextTrackInfo& preferredTrack = mPrivateInstanceAAMP->GetPreferredTextTrack();
	EXPECT_STREQ(preferredTrack.language.c_str(), "lang1");
	EXPECT_STREQ(preferredTrack.index.c_str(), "idx1");
	EXPECT_TRUE(preferredTrack.isCC);

}

/**
 * @brief Test changing of accessibility preference
 * Expecting tune when accessibility changes compared with value in preferredTextAccessibilityNode
 * which is set to ""
 */
TEST_F(SetPreferredTextLanguagesTests, Accessibility1)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "English", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "Spanish", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), false));

	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mPrivateInstanceAAMP->subtitles_muted = false;

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, getAccessibilityNode(_))
		.WillOnce(Return(Accessibility("something","dummy")));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"accessibility\":{\"scheme\":\"return_from_mock\",\"string_value\":\"return_from_mock\"}}");


}

/**
 * @brief Test changing of accessibility preference
 * No tune when accessibility does not change
 */
TEST_F(SetPreferredTextLanguagesTests, Accessibility2)
{
	std::vector<TextTrackInfo> tracks;

	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "English", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "Spanish", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), false));

	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_DASH;
	mPrivateInstanceAAMP->preferredTextAccessibilityNode = Accessibility("something","dummy");
	mPrivateInstanceAAMP->subtitles_muted = false;
	/*
	* No tune when no accessibility change between value in preferredTextAccessibilityNode
	* and getAccessibilityNode() mock return value
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_)).Times(0);    // Does not get called
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, getAccessibilityNode(_))
		.WillOnce(Return(Accessibility("something","dummy")));

	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"accessibility\":{\"scheme\":\"return_from_mock\",\"string_value\":\"return_from_mock\"}}");

}

/**
 * @brief Reproduce segfault when StopInternal/TeardownStream races with
 *        SetPreferredTextLanguages on separate threads.
 *
 *        Thread A: Calls SetPreferredTextLanguages("lang1")
 *        Thread B: Calls TeardownStream(true) — deletes mpStreamAbstractionAAMP
 *
 *
 *        Test strategy: Use GetAvailableTextTracks mock as synchronization point.
 *        When Thread A calls GetAvailableTextTracks, signal Thread B to run
 *        TeardownStream. With the fix, Thread B blocks on mStreamLock (held by
 *        Thread A) and never deletes mpStreamAbstractionAAMP during Thread A's
 *        execution. Without the fix, Thread B succeeds and crashes Thread A.
 */
TEST_F(SetPreferredTextLanguagesTests, CrashWhenTeardownRacesWithSetPreferredText)
{
	std::vector<TextTrackInfo> tracks;
	tracks.push_back(TextTrackInfo("idx0", "lang0", false, "rend0", "trackName0", "codecStr0", "cha0", "typ0", "lab0", "type0", Accessibility(), true));
	tracks.push_back(TextTrackInfo("idx1", "lang1", false, "rend1", "trackName1", "codecStr1", "cha1", "typ1", "lab1", "type1", Accessibility(), true));

	mPrivateInstanceAAMP->preferredTextLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->subtitles_muted = false;
	/* Set HLS format so the code path reaches SelectPreferredTextTrack */
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS;

	std::mutex syncMtx;
	std::condition_variable cvThreadAInside;  /* Thread A -> Thread B: inside mock */
	std::condition_variable cvThreadBReady;   /* Thread B -> Thread A: about to call TeardownStream */
	bool threadAInside = false;
	bool threadBReady = false;
	std::atomic<bool> teardownDone{false};


	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(Invoke([&](bool) -> std::vector<TextTrackInfo>& {
			/* Signal Thread B that Thread A is inside SetPreferredTextLanguages */
			{
				std::lock_guard<std::mutex> lk(syncMtx);
				threadAInside = true;
			}
			cvThreadAInside.notify_one();

			/* Wait for Thread B to signal it is about to call TeardownStream,
			 * creating the race window deterministically without any wall-clock
			 * delay. */
			{
				std::unique_lock<std::mutex> lk(syncMtx);
				cvThreadBReady.wait(lk, [&] { return threadBReady; });
			}

			return tracks;
		}));


	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(::testing::DoAll(::testing::SetArgReferee<0>(tracks[0]), Return(true)));


	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));

	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_, _, _))
		.Times(::testing::AnyNumber());

	/* Thread B: waits for Thread A to be inside GetAvailableTextTracks, then
	 * signals it is about to call TeardownStream before actually doing so.
	 * This replaces the fixed sleep with a deterministic two-way handshake. */
	std::thread threadB([&]() {
		{
			std::unique_lock<std::mutex> lk(syncMtx);
			cvThreadAInside.wait(lk, [&] { return threadAInside; });
		}

		/* Signal Thread A (still inside the mock) that TeardownStream is
		 * imminent, then immediately call it to create the race. */
		{
			std::lock_guard<std::mutex> lk(syncMtx);
			threadBReady = true;
		}
		cvThreadBReady.notify_one();

		mPrivateInstanceAAMP->TeardownStream(true);
		teardownDone.store(true);
	});

	/* Thread A: calls SetPreferredTextLanguages. */
	mPrivateInstanceAAMP->SetPreferredTextLanguages("lang1");

	threadB.join();

	/* Verify preferred language was updated */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "lang1");


	EXPECT_TRUE(teardownDone.load());
}

/**
 * @brief Reproduce crash when PopulateAudioAndTextTracks races with
 *        SetPreferredTextLanguages on separate threads.
 */
TEST_F(SetPreferredTextLanguagesTests, CrashWhenPopulateTracksRacesWithSetPreferredText)
{

	std::vector<TextTrackInfo> emptyTracks;

	mPrivateInstanceAAMP->preferredTextLanguagesString = "eng";
	mPrivateInstanceAAMP->preferredTextLanguagesList.clear();
	mPrivateInstanceAAMP->preferredTextLanguagesList.push_back("eng");
	mPrivateInstanceAAMP->subtitles_muted = false;
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS;
	mPrivateInstanceAAMP->mCurrentTextTrackIndex = 0;


	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableTextTracks(_))
		.WillOnce(ReturnRef(emptyTracks));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, SelectPreferredTextTrack(_))
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredTextLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_, _, _))
		.Times(::testing::AnyNumber());


	mPrivateInstanceAAMP->SetPreferredTextLanguages("{\"languages\":[\"eng\",\"\"],\"sub-type\":\"SUBTITLES\"}");

	/* If we reach here, the bounds check prevented the crash */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTextLanguagesString.c_str(), "eng,");
}
