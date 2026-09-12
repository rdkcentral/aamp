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

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Throw;
using ::testing::An;
using ::testing::AnyNumber;

class TestablePrivateInstanceAAMP : public PrivateInstanceAAMP
{
public:
	explicit TestablePrivateInstanceAAMP(AampConfig *config)
		: PrivateInstanceAAMP(config)
	{
	}

	void SetFirstTune(bool value) { mFirstTune = value; }
};

class SetPreferredLanguagesTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPrivateInstanceAAMP = new TestablePrivateInstanceAAMP(gpGlobalConfig);
		g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
		g_mockAampGstPlayer = std::make_shared<MockAAMPGstPlayer>( mPrivateInstanceAAMP);
		auto *rawMock = new StrictMock<MockStreamAbstractionAAMP>(mPrivateInstanceAAMP);
		/* No-op deleter: mpStreamAbstractionAAMP (a raw pointer in PrivateInstanceAAMP)
		 * takes ownership of rawMock and deletes it via SAFE_DELETE on retune.
		 * The shared_ptr is a non-owning observation handle; reset() only clears
		 * the handle, not the object. */
		g_mockStreamAbstractionAAMP = std::shared_ptr<MockStreamAbstractionAAMP>(rawMock, [](MockStreamAbstractionAAMP*){});
		g_mockAampStreamSinkManager = std::make_shared<NiceMock<MockAampStreamSinkManager>>();

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

	TestablePrivateInstanceAAMP *mPrivateInstanceAAMP{};
};

/**
 * @brief Set the preferred languages list which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest1)
{
	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() without changing the preferred languages
	 * list. There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);
	
	mPrivateInstanceAAMP->SetPreferredLanguages("lang0", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang0");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang0");
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

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
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

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0,lang1";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang1");

	/* Call SetPreferredLanguages() changing the preferred languages list. There
	 * should be a retune as there are multiple languages.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPrivateInstanceAAMP->SetPreferredLanguages("lang0,lang2", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang0,lang2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 2);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang0");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(1).c_str(), "lang2");
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

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() passing a language which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages("lang2", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang2");
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

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPrivateInstanceAAMP->SetPreferredLanguages("{\"languages\":\"lang1\"}", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred languages list as a JSON string array which matches
 *        the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest6)
{
	mPrivateInstanceAAMP->preferredLanguagesString = "lang0,lang1";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang1");

	/* Call SetPreferredLanguages() without changing the preferred languages
	 * list. There should be a no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages("{\"languages\":[\"lang0\",\"lang1\"]}", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang0,lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 2);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang0");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(1).c_str(), "lang1");
}

/**
 * @brief TSB related test to change the preferred languages list.
 */
TEST_F(SetPreferredLanguagesTests, LanguageListTest7)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
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

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred languages list but the
	 * matching track is disabled. There should be a retune and a new TSB
	 * requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(1);
	
	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Set the preferred rendition which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest1)
{
	mPrivateInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() without changing the preferred rendition.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, "rend0", NULL, NULL, NULL);

	/* Verify the preferred rendition list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredRenditionString.c_str(), "rend0");
}

/**
 * @brief Set the preferred rendition which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest2)
{
	std::vector<AudioTrackInfo> tracks;
	
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() changing the preferred rendition. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, "rend1", NULL, NULL, NULL);

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredRenditionString.c_str(), "rend1");
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

	mPrivateInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() changing the preferred rendition which is
	 * not available. There should not be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, "rend2", NULL, NULL, NULL);

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredRenditionString.c_str(), "rend2");
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

	mPrivateInstanceAAMP->preferredRenditionString = "rend0";

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("{\"rendition\":\"rend1\"}", NULL, NULL, NULL, NULL);

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredRenditionString.c_str(), "rend1");
}

/**
 * @brief TSB related test to change the preferred rendition.
 */
TEST_F(SetPreferredLanguagesTests, RenditionTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredRenditionString = "rend0";
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred rendition. There
	 * should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, "rend1", NULL, NULL, NULL);

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredRenditionString.c_str(), "rend1");
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

	mPrivateInstanceAAMP->preferredRenditionString = "rend0";
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the rendition. There should be a
	 * retune and a new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, "rend1", NULL, NULL, NULL);

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred rendition. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredRenditionString.c_str(), "rend1");
}

/**
 * @brief Set the preferred label list which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest1)
{
	mPrivateInstanceAAMP->preferredLabelsString = "label0";
	mPrivateInstanceAAMP->preferredLabelList.clear();
	mPrivateInstanceAAMP->preferredLabelList.push_back("label0");

	/* Call SetPreferredLanguages() without changing the preferred label list.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label0");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label0");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(0).c_str(), "label0");
}

/**
 * @brief Set the preferred label list which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest2)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLabelsString = "label0";
	mPrivateInstanceAAMP->preferredLabelList.clear();
	mPrivateInstanceAAMP->preferredLabelList.push_back("label0");

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(0).c_str(), "label1");
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

	mPrivateInstanceAAMP->preferredLabelsString = "label0,label1";
	mPrivateInstanceAAMP->preferredLabelList.clear();
	mPrivateInstanceAAMP->preferredLabelList.push_back("label0");
	mPrivateInstanceAAMP->preferredLabelList.push_back("label1");

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune as there are multiple labels.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label0,label2");

	/* Verify the preferred labels. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label0,label2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLabelList.size(), 2);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(0).c_str(), "label0");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(1).c_str(), "label2");
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

	mPrivateInstanceAAMP->preferredLabelsString = "label0";
	mPrivateInstanceAAMP->preferredLabelList.clear();
	mPrivateInstanceAAMP->preferredLabelList.push_back("label0");

	/* Call SetPreferredLanguages() passing a label which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label2");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(0).c_str(), "label2");
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

	mPrivateInstanceAAMP->preferredLabelsString = "label0";

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("{\"label\":\"label1\"}", NULL, NULL, NULL, NULL);

	/* Verify the preferred label. The preferred label list is not changed in
	 * this code path.
	 */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label1");
}

/**
 * @brief TSB related test to change the preferred label list.
 */
TEST_F(SetPreferredLanguagesTests, LabelListTest6)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLabelsString = "label0";
	mPrivateInstanceAAMP->preferredLabelList.clear();
	mPrivateInstanceAAMP->preferredLabelList.push_back("label0");
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred label list. There
	 * should be a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(0).c_str(), "label1");
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

	mPrivateInstanceAAMP->preferredLabelsString = "label0";
	mPrivateInstanceAAMP->preferredLabelList.clear();
	mPrivateInstanceAAMP->preferredLabelList.push_back("label0");
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred languages list.
	 * There should be a retune and a new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLabelList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelList.at(0).c_str(), "label1");
}

/**
 * @brief Set the preferred type which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest1)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredTypeString = "type0";

	/* Call SetPreferredLanguages() without changing the preferred type. There
	 * should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type0", NULL, NULL);

	/* Verify the preferred type. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTypeString.c_str(), "type0");
}

/**
 * @brief Set the preferred type which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest2)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredTypeString = "type0";

	/* Call SetPreferredLanguages() changing the preferred type. There should be
	 * a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type1", NULL, NULL);

	/* Verify the preferred type. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTypeString.c_str(), "type1");
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

	mPrivateInstanceAAMP->preferredTypeString = "type0";

	/* Call SetPreferredLanguages() passing a type which is not available. There
	 * should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type2", NULL, NULL);

	/* Verify the preferred type. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTypeString.c_str(), "type2");
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

	mPrivateInstanceAAMP->preferredAudioAccessibilityNode = Accessibility("schemId0", "val0");

	/* Call SetPreferredLanguages() changing the preferred type. There should be
	 * a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("{\"accessibility\":{}}", NULL, NULL, NULL, NULL);

	/* Verify the (default) preferred type. */
	Accessibility expectedAccessibility;
	EXPECT_EQ(mPrivateInstanceAAMP->preferredAudioAccessibilityNode, expectedAccessibility);
}

/**
 * @brief TSB related test to change the preferred type.
 */
TEST_F(SetPreferredLanguagesTests, TypeTest5)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredTypeString = "type0";
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred type. There should be
	 * a retune but no new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type1", NULL, NULL);

	/* Verified the requested manifest URL. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/Manifest.mpd");

	/* Verify the preferred type. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTypeString.c_str(), "type1");
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

	mPrivateInstanceAAMP->preferredTypeString = "type0";
	mPrivateInstanceAAMP->mFogTSBEnabled = true;
	mPrivateInstanceAAMP->mManifestUrl = "http://host/Manifest.mpd";
	mPrivateInstanceAAMP->mTsbSessionRequestUrl = "http://host/TsbSessionRequest.mpd";

	/* Call SetPreferredLanguages() changing the preferred tupe. There should be
	 * a retune and a new TSB requested.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, "type1", NULL, NULL);

	/* The manifest URL should be changed to reload the TSB. */
	EXPECT_STREQ(mPrivateInstanceAAMP->mManifestUrl.c_str(), "http://host/TsbSessionRequest.mpd&reloadTSB=true");

	/* Verify the preferred type. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredTypeString.c_str(), "type1");
}

/**
 * @brief Set the preferred codec list which matches the current setting.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest1)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));

	mPrivateInstanceAAMP->preferredCodecString = "codec0";
	mPrivateInstanceAAMP->preferredCodecList.clear();
	mPrivateInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() without changing the preferred codec list.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec0", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecString.c_str(), "codec0");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecList.at(0).c_str(), "codec0");
}

/**
 * @brief Set the preferred codec list which doesn't match the current setting.
 */
TEST_F(SetPreferredLanguagesTests, CodecListTest2)
{
	std::vector<AudioTrackInfo> tracks;

	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredCodecString = "codec0";
	mPrivateInstanceAAMP->preferredCodecList.clear();
	mPrivateInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() changing the preferred codec list. There
	 * should be a retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec1", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecString.c_str(), "codec1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecList.at(0).c_str(), "codec1");
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

	mPrivateInstanceAAMP->preferredCodecString = "codec0,codec1";
	mPrivateInstanceAAMP->preferredCodecList.clear();
	mPrivateInstanceAAMP->preferredCodecList.push_back("codec0");
	mPrivateInstanceAAMP->preferredCodecList.push_back("codec1");

	/* Call SetPreferredLanguages() changing the preferred codec list. There
	 * should be a retune as there are multiple codecs.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec0,codec2", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecString.c_str(), "codec0,codec2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredCodecList.size(), 2);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecList.at(0).c_str(), "codec0");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecList.at(1).c_str(), "codec2");
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

	mPrivateInstanceAAMP->preferredCodecString = "codec0";
	mPrivateInstanceAAMP->preferredCodecList.clear();
	mPrivateInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() passing a codec which is not available.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec2", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecString.c_str(), "codec2");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecList.at(0).c_str(), "codec2");
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

	mPrivateInstanceAAMP->preferredCodecString = "codec0";
	mPrivateInstanceAAMP->preferredCodecList.clear();
	mPrivateInstanceAAMP->preferredCodecList.push_back("codec0");

	/* Call SetPreferredLanguages() passing a codec which is not enabled.
	 * There should be no retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, "codec1", NULL);

	/* Verify the preferred codec list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecString.c_str(), "codec1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredCodecList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredCodecList.at(0).c_str(), "codec1");
}

/**
 * @brief seamlessAudioSwitch should work when switching audio
 *        language without explicit codec preference when codecs are the same.
 */
TEST_F(SetPreferredLanguagesTests, LanguageSwitchSameCodecNoExplicitPreference)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec0", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.WillOnce(Return(true));

	/* Call SetPreferredLanguages() changing language but codec is the same.
	 * With seamless audio switch enabled and no codec change, RefreshTrack should
	 * be called instead of Stop (retune).
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsSeamlessAudioSwitchPossible())
		.WillOnce(Return(true));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(1);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.Times(0);
	EXPECT_CALL(*g_mockAampGstPlayer, Flush(_,_,_))
		.Times(0);

	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief seamlessAudioSwitch should NOT be used when switching
 *        audio language without explicit codec preference when codecs differ.
 */
TEST_F(SetPreferredLanguagesTests, LanguageSwitchDifferentCodecNoExplicitPreference)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.WillOnce(Return(true));

	/* Call SetPreferredLanguages() changing language and codec differs.
	 * Even with seamless audio switch enabled, codec change requires retune.
	 */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief Test a seamless switch must not be attempted once the
 *        fetcher has reached end of stream.
 *
 * RefreshTrack() only raises a flag that the fetcher loop polls; if the fetcher has
 * already exited, the request is silently dropped and the audio track never changes
 * (the failure mode seen in L3 TST_2023 switch 2 on a fully-downloaded short VOD).
 * Even with matching codecs, AAMP must retune in that situation.
 */
TEST_F(SetPreferredLanguagesTests, LanguageSwitchSameCodecFetcherAtEosRetunes)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec0", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.WillOnce(Return(true));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	/* Fetcher can no longer service the request. */
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, IsSeamlessAudioSwitchPossible())
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief a label-only change carries no codec information, so it
 *        must retune rather than assume the codec is unchanged.
 *
 * With no preferred codec and no preferred language set, there is nothing to compare
 * the current codec against. The target track may well use a different codec, so the
 * seamless path is unsafe here.
 */
TEST_F(SetPreferredLanguagesTests, LabelSwitchNoLanguageOrCodecPreferenceRetunes)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec1", 0, "type1", false, "label1", "type1", true));

	mPrivateInstanceAAMP->preferredLanguagesString.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLabelsString = "label0";
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages(NULL, NULL, NULL, NULL, "label1");

	/* Verify the preferred label. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLabelsString.c_str(), "label1");
}

/**
 * @brief when the requested language is offered in more than one
 *        codec, the codec of the track that SelectAudioTrack() will pick cannot be
 *        predicted here, so AAMP must retune.
 */
TEST_F(SetPreferredLanguagesTests, LanguageSwitchAmbiguousCodecRetunes)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	/* lang1 is available as both codec0 and codec1. */
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec0", 0, "type1", false, "label1", "type1", true));
	tracks.push_back(AudioTrackInfo("idx2", "lang1", "rend2", "trackName2", "codec1", 0, "type2", false, "label2", "type2", true));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief when the requested language is present but not flagged
 *        available in the manifest, no codec can be established for it and AAMP must
 *        retune rather than take the seamless path on an uninspected track.
 */
TEST_F(SetPreferredLanguagesTests, LanguageSwitchTargetNotAvailableRetunes)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	/* Same codec as the current track, but not available in the manifest. */
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec0", 0, "type1", false, "label1", "type1", false));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	mPrivateInstanceAAMP->SetPreferredLanguages("lang1", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesString.c_str(), "lang1");
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 1);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
}

/**
 * @brief multiple preferred languages always retune. The language
 *        that ends up selected is decided by SelectAudioTrack() scoring, so inferring
 *        a codec from the first entry alone would be a guess.
 */
TEST_F(SetPreferredLanguagesTests, MultipleLanguagesNoExplicitCodecRetunes)
{
	std::vector<AudioTrackInfo> tracks;
	tracks.push_back(AudioTrackInfo("idx0", "lang0", "rend0", "trackName0", "codec0", 0, "type0", false, "label0", "type0", true));
	tracks.push_back(AudioTrackInfo("idx1", "lang1", "rend1", "trackName1", "codec0", 0, "type1", false, "label1", "type1", true));
	tracks.push_back(AudioTrackInfo("idx2", "lang2", "rend2", "trackName2", "codec1", 0, "type2", false, "label2", "type2", true));

	mPrivateInstanceAAMP->preferredLanguagesString = "lang0";
	mPrivateInstanceAAMP->preferredLanguagesList.clear();
	mPrivateInstanceAAMP->preferredLanguagesList.push_back("lang0");
	mPrivateInstanceAAMP->SetFirstTune(false);
	mPrivateInstanceAAMP->mMediaFormat = eMEDIAFORMAT_HLS_MP4;

	/* Enable seamless audio switch config */
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_SeamlessAudioSwitch))
		.Times(AnyNumber())
		.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetAvailableAudioTracks(_))
		.WillOnce(ReturnRef(tracks));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, StopUnderflowMonitor());
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, RefreshTrack(eMEDIATYPE_AUDIO))
		.Times(0);
	EXPECT_CALL(*g_mockStreamAbstractionAAMP, Stop(_))
		.WillOnce(Invoke(this, &SetPreferredLanguagesTests::Stop));

	/* lang1 uses the same codec as the current track, lang2 does not. */
	mPrivateInstanceAAMP->SetPreferredLanguages("lang1,lang2", NULL, NULL, NULL, NULL);

	/* Verify the preferred languages list. */
	EXPECT_EQ(mPrivateInstanceAAMP->preferredLanguagesList.size(), 2);
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(0).c_str(), "lang1");
	EXPECT_STREQ(mPrivateInstanceAAMP->preferredLanguagesList.at(1).c_str(), "lang2");
}
