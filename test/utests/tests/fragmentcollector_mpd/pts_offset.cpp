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
#include "fragmentcollector_mpd.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockAampMPDParseHelper.h"
#include "MediaStreamContext.h"

using namespace testing;

// So we can access protected members
class ToBeTestedStub : public StreamAbstractionAAMP_MPD
{
public:
	ToBeTestedStub(class PrivateInstanceAAMP *aamp, double seekpos, float rate,
				   id3_callback_t id3Handler = nullptr) : StreamAbstractionAAMP_MPD(aamp, seekpos, rate){};
	FRIEND_TEST(fragmentcollector_mpd, UpdatePtsOffsetTest1);
};

class fragmentcollector_mpd : public ::testing::Test
{

protected:
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	const char *mManifest;
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
	ToBeTestedStub *mStreamAbstractionAAMP_MPD{};

	void SetUp() override
	{
		gpGlobalConfig = new AampConfig();

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		mStreamAbstractionAAMP_MPD = new ToBeTestedStub( mPrivateInstanceAAMP, 0, AAMP_NORMAL_PLAY_RATE);
		
		g_mockAampMPDParseHelper = new MockAampMPDParseHelper();
	}

	void TearDown() override
	{
		delete g_mockAampMPDParseHelper;
		g_mockAampMPDParseHelper = nullptr;

		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

	void addAttributesToNode(xmlTextReaderPtr *reader, Node *node)
	{
		if (xmlTextReaderHasAttributes(*reader))
		{
			while (xmlTextReaderMoveToNextAttribute(*reader))
			{
				std::string key = (const char *)xmlTextReaderConstName(*reader);
				if (!key.empty())
				{
					std::string value = (const char *)xmlTextReaderConstValue(*reader);
					node->AddAttribute(key, value);
				}
				else
				{
					AAMPLOG_WARN("key   is null"); // CID:85916 - Null Returns
				}
			}
		}
	}
	Node *processNode(xmlTextReaderPtr *reader, std::string url, bool isAd)
	{
		int type = xmlTextReaderNodeType(*reader);

		if (type != WhiteSpace && type != Text)
		{
			while (type == Comment || type == WhiteSpace)
			{
				if (!xmlTextReaderRead(*reader))
				{
					AAMPLOG_WARN("xmlTextReaderRead  failed");
				}
				type = xmlTextReaderNodeType(*reader);
			}

			Node *node = new Node();
			node->SetType(type);
			node->SetMPDPath(Path::GetDirectoryPath(url));

			const char *name = (const char *)xmlTextReaderConstName(*reader);
			if (name == NULL)
			{
				SAFE_DELETE(node);
				return NULL;
			}

			int isEmpty = xmlTextReaderIsEmptyElement(*reader);
			node->SetName(name);
			addAttributesToNode(reader, node);

			if (isAd && !strcmp("Period", name))
			{
				// Making period ids unique. It needs for playing same ad back to back.
				static int UNIQ_PID = 0;
				std::string periodId = std::to_string(UNIQ_PID++) + "-";
				if (node->HasAttribute("id"))
				{
					periodId += node->GetAttributeValue("id");
				}
				node->AddAttribute("id", periodId);
			}

			if (isEmpty)
				return node;

			Node *subnode = NULL;
			int ret = xmlTextReaderRead(*reader);
			int subnodeType = xmlTextReaderNodeType(*reader);

			while (ret == 1)
			{
				if (!strcmp(name, (const char *)xmlTextReaderConstName(*reader)))
				{
					return node;
				}

				if (subnodeType != Comment && subnodeType != WhiteSpace)
				{
					subnode = processNode(reader, url, isAd);
					if (subnode != NULL)
						node->AddSubNode(subnode);
				}

				ret = xmlTextReaderRead(*reader);
				subnodeType = xmlTextReaderNodeType(*reader);
			}

			return node;
		}
		else if (type == Text)
		{
			xmlChar *text = xmlTextReaderReadString(*reader);

			if (text != NULL)
			{
				Node *node = new Node();
				node->SetType(type);
				node->SetText((const char *)text);
				xmlFree(text);
				return node;
			}
		}
		return NULL;
	}
	void GetMPDFromManifest(ManifestDownloadResponsePtr response)
	{
		dash::mpd::MPD *mpd = nullptr;
		std::string manifestStr = std::string(response->mMPDDownloadResponse->mDownloadData.begin(), response->mMPDDownloadResponse->mDownloadData.end());

		xmlTextReaderPtr reader = xmlReaderForMemory((char *)manifestStr.c_str(), (int)manifestStr.length(), NULL, NULL, 0);
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = processNode(&reader, TEST_MANIFEST_URL, 0);
				if (response->mRootNode != NULL)
				{
					mpd = response->mRootNode->ToMPD();
					if (mpd)
					{
						std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
						response->mMPDInstance = tmp_ptr;
						response->GetMPDParseHelper()->Initialize(mpd);
					}
				}
			}
		}
		xmlFreeTextReader(reader);
	}

	ManifestDownloadResponsePtr GetManifestForMPDDownloader()
	{
		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
		response->mMPDDownloadResponse->iHttpRetValue = 200;
		response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t *)mManifest, (uint8_t *)(mManifest + strlen(mManifest)));
		GetMPDFromManifest(response);
		return response;
	}
};

TEST_F(fragmentcollector_mpd, UpdatePtsOffsetTest1)
{
	/*All this to get a value in mStreamAbstractionAAMP_MPD->mpd*/
	static const char *manifest =
		R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" availabilityEndTime="2023-07-11T10:28:30Z" availabilityStartTime="2023-07-11T10:20:00Z" id="Config part of url maybe?" maxSegmentDuration="PT2S" mediaPresentationDuration="PT8M" minBufferTime="PT2S" minimumUpdatePeriod="PT30S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash-if-simple" publishTime="2023-07-11T10:26:01Z" type="dynamic" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
   <ProgramInformation>
      <Title>Media Presentation Description from DASH-IF live simulator</Title>
   </ProgramInformation>
   <BaseURL>https://livesim.dashif.org/livesim/sts_1689071160/sid_09befd74/modulo_10/testpic_2s/</BaseURL>
<Period id="p0" start="PT0S">
      <AdaptationSet contentType="video" maxFrameRate="60/2" maxHeight="360" maxWidth="640" mimeType="video/mp4" minHeight="360" minWidth="640" par="16:9" segmentAlignment="true" startWithSAP="1">
         <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main" />
         <SegmentTemplate duration="2" initialization="$RepresentationID$/init.mp4" media="$RepresentationID$/$Number$.m4s" startNumber="0" />
         <Representation bandwidth="300000" codecs="avc1.64001e" frameRate="60/2" height="360" id="V300" sar="1:1" width="640" />
      </AdaptationSet>
   </Period>
</MPD>
)";
	mManifest = manifest;
	ManifestDownloadResponsePtr respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mStreamAbstractionAAMP_MPD->mpd = respData->mMPDInstance.get();

	PrivateInstanceAAMP *privateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
	StreamAbstractionAAMP_MPD *streamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(privateInstanceAAMP, 123.45, 12.34);

	MediaStreamContext ms(eTRACK_VIDEO, streamAbstractionAAMP_MPD, privateInstanceAAMP, "SAMPLETEXT");
	mStreamAbstractionAAMP_MPD->mMediaStreamContext[eMEDIATYPE_AUDIO] = &ms;
	mStreamAbstractionAAMP_MPD->mMediaStreamContext[eMEDIATYPE_VIDEO] = &ms;

	/* Here we have a table which holds the values read from the manifest
	 * for each period and the PTSOffset that is expected to be calculated.
	 * All values are in seconds.
	 */
	struct
	{
		double aStart;
		double vStart;
		double aDuration;
		double vDuration;
		double expectedOffset;
	} tbl[] = {                         // mPTSOffset(n) = mPTSOffset(n-1) + duration(n-1) + timelineStart(n-1) - timelineStart(n);
		{0, 0, 9.6, 9.6, 0},
		{0, 0, 9.6, 9.6, 9.6},
		{0, 0, 9.6, 9.6, 19.2},
		{10, 0, 9.6, 9.6, 28.8},		// 28.8 = 19.2 + max(9.6,9.6) + min(0,0) - min(10,0)
		{100, 100, 9.6, 9.0, -61.6},	// -61.6 = 28.8 + max(9.6,9.6) + min(10,0) - min(100,100)
		{0, 0, 5.0, 5.0, 48.0},			//  48 = -61.6 + max(9.6,9.0) + min(100,100) - min(0,0)
		{123, 123, 0, 0, -70},		    //  -70 = 48.0 + max(5.0,5.0) + min(0,0) - min(123,123)
	};

	/* Configure the mocks to return values in the order that they appear in the table */
	Sequence s1, s2, s3, s4;
	for (int p = 0; p < (sizeof(tbl) / sizeof(tbl[0])); p++)
	{

//for isNewPeriod == true
		EXPECT_CALL(*g_mockAampMPDParseHelper, GetStartAndDurationFromTimeline(_, _, _, _, _))
			.InSequence(s1)
			.WillOnce(DoAll(SetArgReferee<3>(tbl[p].aStart), SetArgReferee<4>(tbl[p].aDuration)));

		EXPECT_CALL(*g_mockAampMPDParseHelper, GetStartAndDurationFromTimeline(_, _, _, _, _))
			.InSequence(s1)
			.WillOnce(DoAll(SetArgReferee<3>(tbl[p].vStart), SetArgReferee<4>(tbl[p].vDuration)));

//for isNewPeriod == false
					EXPECT_CALL(*g_mockAampMPDParseHelper, GetStartAndDurationFromTimeline(_, _, _, _, _))
			.InSequence(s1)
			.WillOnce(DoAll(SetArgReferee<3>(tbl[p].aStart), SetArgReferee<4>(tbl[p].aDuration)));

		EXPECT_CALL(*g_mockAampMPDParseHelper, GetStartAndDurationFromTimeline(_, _, _, _, _))
			.InSequence(s1)
			.WillOnce(DoAll(SetArgReferee<3>(tbl[p].vStart), SetArgReferee<4>(tbl[p].vDuration)));

	}

	// This EXPECT for the last entry in the table where there is no segment timeline
	EXPECT_CALL(*g_mockAampMPDParseHelper, GetPeriodDuration(_, _, _, _))
		.WillOnce(Return(45)).WillOnce(Return(45));

	mStreamAbstractionAAMP_MPD->mNextPts = 0;
	mStreamAbstractionAAMP_MPD->mPTSOffset = 0;
	mStreamAbstractionAAMP_MPD->mCurrentPeriod = mStreamAbstractionAAMP_MPD->mpd->GetPeriods().at(0);

	/* Call the method under test and confirm
	 *  the returned vale matches expected value from the table
	 */
	for (int p = 0; p < (sizeof(tbl) / sizeof(tbl[0])); p++)
	{
		// Beginning of period call to calc PTSoffset
		mStreamAbstractionAAMP_MPD->UpdatePtsOffset(true);

		EXPECT_DOUBLE_EQ(mStreamAbstractionAAMP_MPD->mPTSOffset.inSeconds(), tbl[p].expectedOffset);

		// End of period call to read duration of the period to feed into calc for next period
		mStreamAbstractionAAMP_MPD->UpdatePtsOffset(false);
	}
}
