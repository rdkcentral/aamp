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

#include "main_aamp.h"
#include "AampConfig.h"

#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP.h"
#include "MockAampStreamSinkManager.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Throw;
using ::testing::An;
using ::testing::AnyNumber;

class SetPreferredLanguagesTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPlayerInstanceAAMP = new PlayerInstanceAAMP(gpGlobalConfig);
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockAampGstPlayer = new MockAAMPGstPlayer( mPlayerInstanceAAMP);
		g_mockStreamAbstractionAAMP = new StrictMock<MockStreamAbstractionAAMP>(mPlayerInstanceAAMP);
		g_mockAampStreamSinkManager = new NiceMock<MockAampStreamSinkManager>();

		mPlayerInstanceAAMP->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;
		mPlayerInstanceAAMP->SetState(eSTATE_PLAYING);

		EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));

		EXPECT_CALL(*g_mockAampStreamSinkManager, GetStreamSink(_)).WillRepeatedly(Return(g_mockAampGstPlayer));
	}

	void TearDown() override
	{
		delete mPlayerInstanceAAMP;
		mPlayerInstanceAAMP = nullptr;

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

	PlayerInstanceAAMP *mPlayerInstanceAAMP{};
};

/**
 * @brief Set the preferred languages list which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest1)
{
	mPlayerInstanceAAMP->preferredLanguagesString = "lang0";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() without changing the preferred languages
	 * list. There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);
	
	mPlayerInstanceAAMP->SetPreferredLanguages("lang0", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang0");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang0");
}

/**
 * @brief Set the preferred languages list which doesn't match the current
 *        setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest2)
{
	std::vector<AudioTrackInfo> tracks;
	
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLanguagesString = "lang0";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPlayerInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred languages list with multiple entries which don't match
 *        the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest3)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLanguagesString = "lang0,lang1";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang1");

	/* Call SetPreferredLanguages() changing the preferred languages list. There
	 * should be a retune as there are multiple languages.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPlayerInstanceAAMP->SetPreferredLanguages("lang0,lang2", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang0,lang2");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 2);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang0");
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(1).c_str(), "lang2");
}

/**
 * @brief Set the preferred languages list which doesn't match the current
 *        setting but there is no matching track.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest4)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLanguagesString = "lang0";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() passing a language which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages("lang2", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang2");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang2");
}

/**
 * @brief Set the preferred languages list as a JSON string which doesn't match
 *        the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLanguagesString = "lang0";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPlayerInstanceAAMP->SetPreferredLanguages("{\"languages\":\"lang1\"}", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred languages list as a JSON string array which matches
 *        the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest6)
{
	mPlayerInstanceAAMP->preferredLanguagesString = "lang0,lang1";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang1");

	/* Call SetPreferredLanguages() without changing the preferred languages
	 * list. There should be a no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages("{\"languages\":[\"lang0\",\"lang1\"]}", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang0,lang1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 2);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang0");
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(1).c_str(), "lang1");
}

/**
 * @brief TSB related test to change the preferred languages list.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest7)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLanguagesString = "lang0";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPlayerInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the requested manifest URL. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief TSB related test to change the preferred languages list to a track
 *        which is not enabled.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest8)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", false));

	mPlayerInstanceAAMP->preferredLanguagesString = "lang0";
	mPlayerInstanceAAMP->preferredLanguagesList.clear();
	mPlayerInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred languages list but the
	 * matching track is disabled. There should be a retune and a new TSB
	 * requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPlayerInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred rendition which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest1)
{
	mPlayerInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() without changing the preferred rendition.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, "rend0", NULL, NULL, NULL);

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredRenditionString.c_str(), "rend0");
}

/**
 * @brief Set the preferred rendition which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest2)
{
	std::vector<AudioTrackInfo> tracks;
	
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() changing the preferred rendition. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, "rend1", NULL, NULL, NULL);

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredRenditionString.c_str(), "rend1");
}

/**
 * @brief Set the preferred rendition which doesn't match the current setting
 *        but there is no matching track.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest3)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() changing the preferred rendition which is
	 * not available. There should not be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, "rend2", NULL, NULL, NULL);

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredRenditionString.c_str(), "rend2");
}

/**
 * @brief Set the preferred remdition as a JSON string which doesn't match the
 *        current setting.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest4)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages("{\"rendition\":\"rend1\"}", NULL, NULL, NULL, NULL);

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredRenditionString.c_str(), "rend1");
}

/**
 * @brief TSB related test to change the preferred rendition.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredRenditionString = "rend0";
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred rendition. There
	 * should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, "rend1", NULL, NULL, NULL);

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredRenditionString.c_str(), "rend1");
}

/**
 * @brief TSB related test to change the rendition to a track which is not
 *        enabled.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest6)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", false));

	mPlayerInstanceAAMP->preferredRenditionString = "rend0";
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the rendition. There should be a
	 * retune and a new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, "rend1", NULL, NULL, NULL);

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredRenditionString.c_str(), "rend1");
}

/**
 * @brief Set the preferred label list which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest1)
{
	mPlayerInstanceAAMP->preferredLabelsString = "label0";
	mPlayerInstanceAAMP->preferredLabelList.clear();
	mPlayerInstanceAAMP->preferredLabelList.push_back("label0");

	/* Call SetPreferredLanguages() without changing the preferred label list.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label0");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label0");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(0).c_str(), "label0");
}

/**
 * @brief Set the preferred label list which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest2)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLabelsString = "label0";
	mPlayerInstanceAAMP->preferredLabelList.clear();
	mPlayerInstanceAAMP->preferredLabelList.push_back("label0");

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(0).c_str(), "label1");
}

/**
 * @brief Set the preferred label list with multiple entries which don't match
 *        the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest3)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLabelsString = "label0,label1";
	mPlayerInstanceAAMP->preferredLabelList.clear();
	mPlayerInstanceAAMP->preferredLabelList.push_back("label0");
	mPlayerInstanceAAMP->preferredLabelList.push_back("label1");

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune as there are multiple labels.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label0,label2");

	/* Verify the preferred labels. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label0,label2");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLabelList.size(), 2);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(0).c_str(), "label0");
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(1).c_str(), "label2");
}

/**
 * @brief Set the preferred label list which doesn't match the current setting
 * but there is no matching track.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest4)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLabelsString = "label0";
	mPlayerInstanceAAMP->preferredLabelList.clear();
	mPlayerInstanceAAMP->preferredLabelList.push_back("label0");

	/* Call SetPreferredLanguages() passing a label which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label2");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label2");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(0).c_str(), "label2");
}

/**
 * @brief Set the preferred label list as a JSON string which doesn't match the
 *        current setting.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLabelsString = "label0";

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages("{\"label\":\"label1\"}", NULL, NULL, NULL, NULL);

	/* Verify the preferred label. The preferred label list is not changed in
	 * this code path.
	 */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label1");
}

/**
 * @brief TSB related test to change the preferred label list.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest6)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredLabelsString = "label0";
	mPlayerInstanceAAMP->preferredLabelList.clear();
	mPlayerInstanceAAMP->preferredLabelList.push_back("label0");
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(0).c_str(), "label1");
}

/**
 * @brief TSB related test to change the preferred label list to a track which
 *        is not enabled.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest7)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", false));

	mPlayerInstanceAAMP->preferredLabelsString = "label0";
	mPlayerInstanceAAMP->preferredLabelList.clear();
	mPlayerInstanceAAMP->preferredLabelList.push_back("label0");
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune and a new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelsString.c_str(), "label1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredLabelList.at(0).c_str(), "label1");
}

/**
 * @brief Set the preferred type which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest1)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredTypeString = "type0";

	/* Call SetPreferredLanguages() without changing the preferred type. There
	 * should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type0", NULL, NULL);

	/* Verify the preferred type. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredTypeString.c_str(), "type0");
}

/**
 * @brief Set the preferred type which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest2)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredTypeString = "type0";

	/* Call SetPreferredLanguages() changing the preferred type. There should be
	 * a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type1", NULL, NULL);

	/* Verify the preferred type. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredTypeString.c_str(), "type1");
}

/**
 * @brief Set the preferred type which doesn't match the current setting but
 *        there is no matching track.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest3)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredTypeString = "type0";

	/* Call SetPreferredLanguages() passing a type which is not available. There
	 * should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type2", NULL, NULL);

	/* Verify the preferred type. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredTypeString.c_str(), "type2");
}

/**
 * @brief Set the preferred type in a JSON object which doesn't match the
 *        current setting.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest4)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredAudioAccessibilityNode = Accessibility("schemId0", "val0");

	/* Call SetPreferredLanguages() changing the preferred type. There should be
	 * a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages("{\"accessibility\":{}}", NULL, NULL, NULL, NULL);

	/* Verify the (default) preferred type. */
	Accessibility expectedAccessibility;
	EXPECT_EQ(mPlayerInstanceAAMP->preferredAudioAccessibilityNode, expectedAccessibility);
}

/**
 * @brief TSB related test to change the preferred type.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredTypeString = "type0";
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred type. There should be
	 * a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type1", NULL, NULL);

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred type. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredTypeString.c_str(), "type1");
}

/**
 * @brief TSB related test to change the preferred type to a track which is not
 *        enabled.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest6)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", false));

	mPlayerInstanceAAMP->preferredTypeString = "type0";
	mPlayerInstanceAAMP->mFogTSBEnabled = true;
	mPlayerInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPlayerInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred tupe. There should be
	 * a retune and a new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type1", NULL, NULL);

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPlayerInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred type. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredTypeString.c_str(), "type1");
}

/**
 * @brief Set the preferred codec list which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest1)
{
	mPlayerInstanceAAMP->preferredCodecString = "codec0";
	mPlayerInstanceAAMP->preferredCodecList.clear();
	mPlayerInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() without changing the preferred codec list.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec0", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecString.c_str(), "codec0");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecList.at(0).c_str(), "codec0");
}

/**
 * @brief Set the preferred codec list which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest2)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredCodecString = "codec0";
	mPlayerInstanceAAMP->preferredCodecList.clear();
	mPlayerInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() changing the preferred codec list. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec1", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecString.c_str(), "codec1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecList.at(0).c_str(), "codec1");
}

/**
 * @brief Set the preferred codec list with multiple entries which don't match
 *        the current setting.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest3)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredCodecString = "codec0,codec1";
	mPlayerInstanceAAMP->preferredCodecList.clear();
	mPlayerInstanceAAMP->preferredCodecList.push_back("codec0");
	mPlayerInstanceAAMP->preferredCodecList.push_back("codec1");

	/* Call SetPreferredLanguages() changing the preferred codec list. There
	 * should be a retune as there are multiple codecs.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec0,codec2", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecString.c_str(), "codec0,codec2");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredCodecList.size(), 2);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecList.at(0).c_str(), "codec0");
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecList.at(1).c_str(), "codec2");
}

/**
 * @brief Set the preferred codec list which doesn't match the current setting
 * but there is no matching track.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest4)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPlayerInstanceAAMP->preferredCodecString = "codec0";
	mPlayerInstanceAAMP->preferredCodecList.clear();
	mPlayerInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() passing a codec which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec2", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecString.c_str(), "codec2");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecList.at(0).c_str(), "codec2");
}

/**
 * @brief Set the preferred codec list which doesn't match the current setting
 * but the matching track is not enabled.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", false));

	mPlayerInstanceAAMP->preferredCodecString = "codec0";
	mPlayerInstanceAAMP->preferredCodecList.clear();
	mPlayerInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() passing a codec which is not enabled.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPlayerInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec1", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecString.c_str(), "codec1");
	EXPECT_EQ(mPlayerInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPlayerInstanceAAMP->preferredCodecList.at(0).c_str(), "codec1");
}
