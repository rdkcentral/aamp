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

#include <cstdlib>
#include <iostream>
#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "priv_aamp.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::WithParamInterface;

class PrivAampTests : public ::testing::Test
{
public:
	PrivateInstanceAAMP *p_aamp{nullptr};
	AampConfig *config{nullptr};

protected:
	void SetUp() override
	{
		config=new AampConfig();
		p_aamp = new PrivateInstanceAAMP(config);
		g_mockStreamAbstractionAAMP = new NiceMock<MockStreamAbstractionAAMP>(p_aamp);
	}

	void TearDown() override
	{
		delete g_mockStreamAbstractionAAMP;
		g_mockStreamAbstractionAAMP = nullptr;

		delete p_aamp;
		p_aamp = nullptr;

		delete config;
		config = nullptr;
	}
};

// Track injection status structure
struct TrackInjectionParams
{
	bool videoInjection;
	bool audioInjection;
	bool subtitleInjection;
	bool auxAudioInjection;
	bool videoTrackEnabled;

	// For test name generation
	std::string ToString() const
	{
		std::stringstream ss;
		ss << "V" << videoInjection
		   << "_A" << audioInjection
		   << "_S" << subtitleInjection
		   << "_X" << auxAudioInjection
		   << "_VE" << videoTrackEnabled;
		return ss.str();
	}
};

class TestPrivateInstanceAAMPTracks : public PrivAampTests,
									  public WithParamInterface<TrackInjectionParams>
{
protected:
	void SetUp() override
	{
		PrivAampTests::SetUp();
		p_aamp->SetLocalAAMPTsbInjection(true);
		p_aamp->mpStreamAbstractionAAMP = g_mockStreamAbstractionAAMP;
	}

	// Helper to create mock track
	std::shared_ptr<MockMediaTrack> CreateMockTrack(TrackType type, const char* name)
	{
		return std::make_shared<NiceMock<MockMediaTrack>>(type, p_aamp, name);
	}

	// Helper to setup track expectations
	void SetupTrackExpectations(TrackType type, std::shared_ptr<MockMediaTrack> track,
								bool injection, bool enabled, int expectGetTrackCalls, int expectedCalls)
	{
		if (expectGetTrackCalls > 0)
		{
			EXPECT_CALL(*g_mockStreamAbstractionAAMP, GetMediaTrack(type))
				.Times(expectGetTrackCalls)
				.WillRepeatedly(Return(track.get()));

			if (track)
			{
				EXPECT_CALL(*track, Enabled())
					.Times(expectGetTrackCalls)
					.WillRepeatedly(Return((enabled) ? true : false));

				if (enabled)
				{
					EXPECT_CALL(*track, IsLocalTSBInjection())
						.Times(expectedCalls)
						.WillRepeatedly(Return(injection));
				}
			}
		}
	}
};

TEST_P(TestPrivateInstanceAAMPTracks, UpdateLocalAAMPTsbInjection)
{
	const auto& params = GetParam();
	const bool hasActiveVideoInjection = params.videoInjection && params.videoTrackEnabled;

	const bool expectedInjection = hasActiveVideoInjection || params.audioInjection ||
								   params.subtitleInjection || params.auxAudioInjection;

	// Calculate expected call counts based on short-circuit evaluation
	const int expectVideoCheck = (hasActiveVideoInjection) ? 1 : 0;
	const int expectAudioCheck = (hasActiveVideoInjection) ? 0 : 1;
	const int expectSubtitleCheck = (hasActiveVideoInjection || params.audioInjection) ? 0 : 1;
	const int expectAuxCheck = (hasActiveVideoInjection || params.audioInjection || params.subtitleInjection) ? 0 : 1;

	// Create tracks
	auto videoTrack = CreateMockTrack(eTRACK_VIDEO, "VIDEO");
	auto audioTrack = CreateMockTrack(eTRACK_AUDIO, "AUDIO");
	auto subtitleTrack = params.subtitleInjection ? CreateMockTrack(eTRACK_SUBTITLE, "SUBTITLE") : nullptr;
	auto auxTrack = CreateMockTrack(eTRACK_AUX_AUDIO, "AUX_AUDIO");

	// Setup expectations
	SetupTrackExpectations(eTRACK_VIDEO, videoTrack, params.videoInjection, params.videoTrackEnabled, 1, expectVideoCheck);
	SetupTrackExpectations(eTRACK_AUDIO, audioTrack, params.audioInjection, true, expectAudioCheck, expectAudioCheck);
	SetupTrackExpectations(eTRACK_SUBTITLE, subtitleTrack, params.subtitleInjection, true, expectSubtitleCheck, expectSubtitleCheck);
	SetupTrackExpectations(eTRACK_AUX_AUDIO, auxTrack, params.auxAudioInjection, true, expectAuxCheck, expectAuxCheck);

	// Execute test
	p_aamp->UpdateLocalAAMPTsbInjection();

	// Verify results
	EXPECT_EQ(p_aamp->IsLocalAAMPTsbInjection(), expectedInjection);
}

// Generate test cases for all track combinations
std::vector<TrackInjectionParams> GenerateTestCases() {
	std::vector<TrackInjectionParams> cases;
	for (int i = 0; i < 16; i++) {
		cases.push_back({
			static_cast<bool>(i & 0x8),
			static_cast<bool>(i & 0x4),
			static_cast<bool>(i & 0x2),
			static_cast<bool>(i & 0x1),
			static_cast<bool>(i & 0x8) // Video track enabled if video injection is true
		});
	}

	//test track is not enabled, but the injection is enabled
	cases.push_back({
		true,
		false,
		false,
		false,
		false // Video track not enabled
	});

	return cases;
}

INSTANTIATE_TEST_SUITE_P(
	TSBInjectionTests,
	TestPrivateInstanceAAMPTracks,
	::testing::ValuesIn(GenerateTestCases()),
	[](const testing::TestParamInfo<TrackInjectionParams>& info) {
		return info.param.ToString();
	}
);

