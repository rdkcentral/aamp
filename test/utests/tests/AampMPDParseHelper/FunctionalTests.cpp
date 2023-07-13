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
#include "downloader/AampCurlDownloader.h"
#include "AampMPDDownloader.h"
#include "AampMPDParseHelper.h"
#include "AampDefine.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include <thread>
#include <unistd.h>


using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
using ::testing::AnyNumber;
using ::testing::DoAll;

AampMPDDownloader *mAampMPDDownloader{nullptr};
AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
	AampMPDParseHelper *ParseHelper= nullptr;
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	const char *mManifest;

	void SetUp() override
	{
		mLogObj = new AampLogManager();
		mAampMPDDownloader = new AampMPDDownloader();

		ParseHelper  = new AampMPDParseHelper();
	}

	void TearDown() override
	{
		delete mAampMPDDownloader;
		mAampMPDDownloader = nullptr;

		delete mLogObj;
		mLogObj=nullptr;

		delete ParseHelper;
		ParseHelper=nullptr;

	}
public:

	void addAttributesToNode(xmlTextReaderPtr *reader, Node *node)
	{
		//FN_TRACE_F_MPD( __FUNCTION__ );
		if (xmlTextReaderHasAttributes(*reader))
		{
			while (xmlTextReaderMoveToNextAttribute(*reader))
			{
				std::string key = (const char *)xmlTextReaderConstName(*reader);
				if(!key.empty())
				{
					std::string value = (const char *)xmlTextReaderConstValue(*reader);
					node->AddAttribute(key, value);
				}
				else
				{
					AAMPLOG_WARN("key   is null");  //CID:85916 - Null Returns
				}
			}
		}
	}
	Node* processNode(xmlTextReaderPtr *reader, std::string url, bool isAd)
	{
		int type = xmlTextReaderNodeType(*reader);

		if (type != WhiteSpace && type != Text)
		{
			while (type == Comment || type == WhiteSpace)
			{
				if(!xmlTextReaderRead(*reader))
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

			int	isEmpty = xmlTextReaderIsEmptyElement(*reader);
			node->SetName(name);
			addAttributesToNode(reader, node);

			if(isAd && !strcmp("Period", name))
			{
				//Making period ids unique. It needs for playing same ad back to back.
				static int UNIQ_PID = 0;
				std::string periodId = std::to_string(UNIQ_PID++) + "-";
				if(node->HasAttribute("id"))
				{
					periodId += node->GetAttributeValue("id");
				}
				node->AddAttribute("id", periodId);
			}

			if (isEmpty)
				return node;

			Node    *subnode = NULL;
			int     ret = xmlTextReaderRead(*reader);
			int subnodeType = xmlTextReaderNodeType(*reader);

			while (ret == 1)
			{
				if (!strcmp(name, (const char *)xmlTextReaderConstName(*reader)))
				{
					return node;
				}

				if(subnodeType != Comment && subnodeType != WhiteSpace)
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
			xmlChar * text = xmlTextReaderReadString(*reader);

			if (text != NULL)
			{
				Node *node = new Node();
				node->SetType(type);
				node->SetText((const char*)text);
				xmlFree(text);
				return node;
			}
		}
		return NULL;
	}
	void GetMPDFromManifest(std::shared_ptr<ManifestDownloadResponse> response)
	{
		dash::mpd::MPD* mpd = nullptr;
		std::string manifestStr = std::string( response->mMPDDownloadResponse->mDownloadData.begin(), response->mMPDDownloadResponse->mDownloadData.end());

		xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = processNode(&reader, TEST_MANIFEST_URL,0);
				if(response->mRootNode != NULL)
				{
					mpd = response->mRootNode->ToMPD();
					if (mpd)
					{
						std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
						response->mMPDInstance	=	tmp_ptr;
						response->GetMPDParseHelper()->Initialize(mpd);
					}
				}
			}
		}
		xmlFreeTextReader(reader);
	}

	std::shared_ptr<ManifestDownloadResponse> GetManifestForMPDDownloader()
	{
		std::shared_ptr<ManifestDownloadResponse> response = std::make_shared<ManifestDownloadResponse> ();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_OK;
		response->mMPDDownloadResponse->iHttpRetValue = 200;
		response->mMPDDownloadResponse->sEffectiveUrl = std::string(TEST_MANIFEST_URL);
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)mManifest, (uint8_t*)(mManifest + strlen(mManifest)));
		GetMPDFromManifest(response);
		return response;
	}
};

TEST_F(FunctionalTests, Live_TSBEmpty_PerioDurationTest)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
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
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 1);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);

	// Check period duration
	EXPECT_NE(periodDuration, 0) << "Period duration is zero.";
}

TEST_F(FunctionalTests, SinglePeriod_withDurationTag)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
		R"(<?xml version="1.0"?>
<!-- MPD file Generated with GPAC version 0.5.2-DEV-rev1067-g9cfa0d1-master  at 2016-10-11T16:34:50.559Z-->
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT1.500S" type="static" mediaPresentationDuration="PT0H11M58.998S" maxSegmentDuration="PT0H0M2.005S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
 <ProgramInformation moreInformationURL="http://gpac.sourceforge.net">
  <Title>../DASHed/2b/11/MultiRate.mpd generated by GPAC</Title>
 </ProgramInformation>

 <Period duration="PT0H11M58.998S">
  <AdaptationSet segmentAlignment="true" maxWidth="3840" maxHeight="2160" maxFrameRate="60000/1001" par="16:9" lang="und">
   <Representation id="1" mimeType="video/mp4" codecs="hev1.2.4.L153.90" width="3840" height="2160" frameRate="60000/1001" sar="1:1" startWithSAP="1" bandwidth="5678742">
    <SegmentTemplate timescale="60000" media="video_8000k_$Number$.mp4" startNumber="1" duration="119952" initialization="video_8000k_init.mp4"/>
   </Representation>
   <Representation id="2" mimeType="video/mp4" codecs="hev1.2.4.L153.90" width="3840" height="2160" frameRate="60000/1001" sar="1:1" startWithSAP="1" bandwidth="8308466">
    <SegmentTemplate timescale="60000" media="video_10400k_$Number$.mp4" startNumber="1" duration="119952" initialization="video_10400k_init.mp4"/>
   </Representation>
</AdaptationSet>
 </Period>
</MPD>
)";
	
	mManifest = manifest;
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 1);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);

	// Check period duration
	// duration from the format "PT0H11M58.998S" to milliseconds is 718998
	EXPECT_EQ(periodDuration, 718998) << "Period duration is zero.";
}
/*Single period without Duration Tag
*To calculate the duration of the stream in milliseconds based on the  mediaPresentationDuration attribute.
*/
TEST_F(FunctionalTests, SinglePeriod_withoutDurationTag)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
		R"(<?xml version="1.0" encoding="utf-8"?>
<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="urn:mpeg:dash:schema:mpd:2011" xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 DASH-MPD.xsd" profiles="urn:mpeg:dash:profile:isoff-on-demand:2011" minBufferTime="PT15S" type="static" mediaPresentationDuration="PT14M48S">
  <Period start="PT0S">
    <AdaptationSet codecs="mp4a.40.42" contentType="audio" mimeType="audio/mp4" lang="en" segmentAlignment="true" bitstreamSwitching="true" audioSamplingRate="48000">
      <AudioChannelConfiguration schemeIdUri="urn:mpeg:mpegB:cicp:ChannelConfiguration" value="2"/>
      <SegmentTemplate timescale="48000" duration="245760" initialization="sintel_audio_video_brs-$RepresentationID$_init.mp4" media="sintel_audio_video_brs-$RepresentationID$_$Number$.m4s" startNumber="0"/>
      <Representation id="eng-2-xheaac-16kbps" bandwidth="17188"/>
      <Representation id="eng-2-xheaac-32kbps" bandwidth="33538"/>
      <Representation id="eng-2-xheaac-64kbps" bandwidth="66115"/>
      <Representation id="eng-2-xheaac-128kbps" bandwidth="129717"/>
    </AdaptationSet>
    <AdaptationSet codecs="avc1.42c01e" contentType="video" frameRate="24/1" width="848" height="386" mimeType="video/mp4" par="21:9" segmentAlignment="true" bitstreamSwitching="true">
      <SegmentTemplate timescale="24" duration="120" initialization="sintel_audio_video_brs-$RepresentationID$_init.mp4" media="sintel_audio_video_brs-$RepresentationID$_$Number$.m4s" startNumber="0"/>
      <Representation id="848x386-1500kbps" bandwidth="1422029" sar="1:1"/>
    </AdaptationSet>
  </Period>
</MPD>
)";
	
	mManifest = manifest;
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 1);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);

	/* Check period duration
	 *   In the given MPD file, the mediaPresentationDuration is specified as PT14M48S, which represents a duration of 14 minutes and 48 seconds.

	 *  Total duration in milliseconds = (14 * 60 * 1000) + (48 * 1000)

	 * Therefore, the duration of the stream is 888,000 milliseconds*/
	EXPECT_EQ(periodDuration, 888000);
}

TEST_F(FunctionalTests, Multiperiod_with_duration)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
		R"(<?xml version="1.0"?>
<!-- MPD file Generated with GPAC version 0.6.2-DEV-rev698-g8cee692-master  at 2016-09-13T09:49:14.312Z-->
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT1.500S" type="static" mediaPresentationDuration="PT0H12M14.167S" maxSegmentDuration="PT0H0M2.000S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
 <ProgramInformation moreInformationURL="http://gpac.io">
  <Title>manifest.mpd generated by GPAC</Title>
 </ProgramInformation>
 <BaseURL>../../Content/</BaseURL>

 <Period id="0" duration="PT60S">
  <AssetIdentifier schemeIdUri="urn:org:dashif:asset-id:2013" value="md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W"></AssetIdentifier>
  <AdaptationSet id="1" segmentAlignment="true" group="1" maxWidth="1920" maxHeight="1080" maxFrameRate="24" par="16:9" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>video/h264/</BaseURL>
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="999120">
    <SegmentTemplate timescale="12288" media="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_$Number$.m4s" startNumber="1" duration="24576" initialization="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
   <Representation id="2" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="2003095">
    <SegmentTemplate timescale="12288" media="2000k/2second/tears_of_steel_1080p_2000k_h264_dash_track1_$Number$.m4s" startNumber="1" duration="24576" initialization="2000k/2second/tears_of_steel_1080p_2000k_h264_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" lang="eng">
	<SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
	<BaseURL>audio/mp4a/</BaseURL>
   <Representation id="3" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" startWithSAP="1" bandwidth="34189">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <SegmentTemplate timescale="48000" media="2second/tears_of_steel_1080p_audio_32k_dash_track1_$Number$.mp4" startNumber="1" duration="95232" initialization="2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
  </AdaptationSet>
 </Period>

 <Period id="1" duration="PT60S">
  <AssetIdentifier schemeIdUri="urn:org:dashif:asset-id:2013" value="md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W"></AssetIdentifier>
  <AdaptationSet id="1" segmentAlignment="true" group="1" maxWidth="1920" maxHeight="1080" maxFrameRate="24" par="16:9" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>video/h264/</BaseURL>
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="999120">
    <SegmentTemplate timescale="12288" media="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_$Number$.m4s" startNumber="31" duration="24576" initialization="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_init.mp4" presentationTimeOffset="737280"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>audio/mp4a/</BaseURL>
   <Representation id="3" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" startWithSAP="1" bandwidth="34189">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <SegmentTemplate timescale="48000" media="2second/tears_of_steel_1080p_audio_32k_dash_track1_$Number$.mp4" startNumber="31" duration="95232" initialization="2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4" presentationTimeOffset="2880000"/>
   </Representation>
  </AdaptationSet>
 </Period>
</MPD>
)";

	mManifest = manifest;
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 2);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);
	// Check period duration
	ASSERT_TRUE(periodDuration == 60000 && mpd->GetPeriods().at(0)->GetId() == "0") << "Period duration is not equal to 60000, or period ID is not equal to '0'.";

}


TEST_F(FunctionalTests, Multiperiod_without_Duration_Live)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
		R"(<?xml version="1.0"?>
<!-- MPD file Generated with GPAC version 0.6.2-DEV-rev698-g8cee692-master  at 2016-09-13T09:49:14.312Z-->
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT1.500S" type="dynamic" mediaPresentationDuration="PT0H12M14.167S" maxSegmentDuration="PT0H0M2.000S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
 <ProgramInformation moreInformationURL="http://gpac.io">
  <Title>manifest.mpd generated by GPAC</Title>
 </ProgramInformation>
 <BaseURL>../../Content/</BaseURL>

 <Period id="0" start="PT0S">
  <AssetIdentifier schemeIdUri="urn:org:dashif:asset-id:2013" value="md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W"></AssetIdentifier>
  <AdaptationSet id="1" segmentAlignment="true" group="1" maxWidth="1920" maxHeight="1080" maxFrameRate="24" par="16:9" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>video/h264/</BaseURL>
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="999120">
    <SegmentTemplate timescale="12288" media="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_$Number$.m4s" startNumber="1" duration="24576" initialization="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
   <Representation id="2" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="2003095">
    <SegmentTemplate timescale="12288" media="2000k/2second/tears_of_steel_1080p_2000k_h264_dash_track1_$Number$.m4s" startNumber="1" duration="24576" initialization="2000k/2second/tears_of_steel_1080p_2000k_h264_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" lang="eng">
	<SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
	<BaseURL>audio/mp4a/</BaseURL>
   <Representation id="3" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" startWithSAP="1" bandwidth="34189">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <SegmentTemplate timescale="48000" media="2second/tears_of_steel_1080p_audio_32k_dash_track1_$Number$.mp4" startNumber="1" duration="95232" initialization="2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
  </AdaptationSet>
 </Period>

 <Period id="1" start="PT60S">
  <AssetIdentifier schemeIdUri="urn:org:dashif:asset-id:2013" value="md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W"></AssetIdentifier>
  <AdaptationSet id="1" segmentAlignment="true" group="1" maxWidth="1920" maxHeight="1080" maxFrameRate="24" par="16:9" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>video/h264/</BaseURL>
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="999120">
    <SegmentTemplate timescale="12288" media="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_$Number$.m4s" startNumber="31" duration="24576" initialization="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_init.mp4" presentationTimeOffset="737280"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>audio/mp4a/</BaseURL>
   <Representation id="3" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" startWithSAP="1" bandwidth="34189">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <SegmentTemplate timescale="48000" media="2second/tears_of_steel_1080p_audio_32k_dash_track1_$Number$.mp4" startNumber="31" duration="95232" initialization="2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4" presentationTimeOffset="2880000"/>
   </Representation>
  </AdaptationSet>
 </Period>
</MPD>
)";

	mManifest = manifest;
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 2);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);
	// Check period duration
	EXPECT_EQ(periodDuration, 60000);
}

TEST_F(FunctionalTests, Multiperiod_without_Duration_NonLive)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
		R"(<?xml version="1.0"?>
<!-- MPD file Generated with GPAC version 0.6.2-DEV-rev698-g8cee692-master  at 2016-09-13T09:49:14.312Z-->
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT1.500S" type="static" mediaPresentationDuration="PT0H12M14.167S" maxSegmentDuration="PT0H0M2.000S" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
 <ProgramInformation moreInformationURL="http://gpac.io">
  <Title>manifest.mpd generated by GPAC</Title>
 </ProgramInformation>
 <BaseURL>../../Content/</BaseURL>

 <Period id="0" start="PT0S">
  <AssetIdentifier schemeIdUri="urn:org:dashif:asset-id:2013" value="md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W"></AssetIdentifier>
  <AdaptationSet id="1" segmentAlignment="true" group="1" maxWidth="1920" maxHeight="1080" maxFrameRate="24" par="16:9" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>video/h264/</BaseURL>
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="999120">
    <SegmentTemplate timescale="12288" media="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_$Number$.m4s" startNumber="1" duration="24576" initialization="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
   <Representation id="2" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="2003095">
    <SegmentTemplate timescale="12288" media="2000k/2second/tears_of_steel_1080p_2000k_h264_dash_track1_$Number$.m4s" startNumber="1" duration="24576" initialization="2000k/2second/tears_of_steel_1080p_2000k_h264_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" lang="eng">
	<SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
	<BaseURL>audio/mp4a/</BaseURL>
   <Representation id="3" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" startWithSAP="1" bandwidth="34189">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <SegmentTemplate timescale="48000" media="2second/tears_of_steel_1080p_audio_32k_dash_track1_$Number$.mp4" startNumber="1" duration="95232" initialization="2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4" presentationTimeOffset="0"/>
   </Representation>
  </AdaptationSet>
 </Period>

 <Period id="1" start="PT60S">
  <AssetIdentifier schemeIdUri="urn:org:dashif:asset-id:2013" value="md:cid:EIDR:10.5240%2f0EFB-02CD-126E-8092-1E49-W"></AssetIdentifier>
  <AdaptationSet id="1" segmentAlignment="true" group="1" maxWidth="1920" maxHeight="1080" maxFrameRate="24" par="16:9" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>video/h264/</BaseURL>
   <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="1920" height="1080" frameRate="24" sar="1:1" startWithSAP="1" bandwidth="999120">
    <SegmentTemplate timescale="12288" media="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_$Number$.m4s" startNumber="31" duration="24576" initialization="1000k/2second/tears_of_steel_1080p_1000k_h264_dash_track1_init.mp4" presentationTimeOffset="737280"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" lang="eng">
   <SupplementalProperty schemeIdUri="urn:mpeg:dash:period-continuity:2015" value="0,1,2"/>
   <BaseURL>audio/mp4a/</BaseURL>
   <Representation id="3" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" startWithSAP="1" bandwidth="34189">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <SegmentTemplate timescale="48000" media="2second/tears_of_steel_1080p_audio_32k_dash_track1_$Number$.mp4" startNumber="31" duration="95232" initialization="2second/tears_of_steel_1080p_audio_32k_dash_track1_init.mp4" presentationTimeOffset="2880000"/>
   </Representation>
  </AdaptationSet>
 </Period>
</MPD>
)";

	mManifest = manifest;
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 2);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);
	// Check period duration
	EXPECT_EQ(periodDuration, 60000);
}




TEST_F(FunctionalTests, SegmentTimeline)
{
	AAMPStatusType status;
	std::string fragmentUrl;
	dash::mpd::IMPD *mpd;
	static const char *manifest =
		R"(<?xml version="1.0"?>
<!-- MPD file Generated with GPAC version 1.1.0-DEV-rev973-gd699e586a-master at 2021-06-07T20:37:44.086Z -->
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minBufferTime="PT1.920S" type="static"  maxSegmentDuration="PT0H0M1.920S" profiles="urn:mpeg:dash:profile:full:2011,urn:mpeg:dash:profile:cmaf:2019">
 <ProgramInformation moreInformationURL="http://gpac.io">
  <Title>ad-insertion-testcase1.mpd generated by GPAC</Title>
 </ProgramInformation>

 <Period id="P1" start="PT0S">
  <AdaptationSet id="1" segmentAlignment="true" lang="eng" startWithSAP="1">
   <SegmentTemplate media="m1_audio_$Number$.m4s" initialization="m1_audio_init.mp4" timescale="48000" startNumber="1">
    <SegmentTimeline>
     <S t="0" d="92160" r="4"/>
    </SegmentTimeline>
   </SegmentTemplate>
   <Representation id="1" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" bandwidth="128000">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
   </Representation>
  </AdaptationSet>
  <AdaptationSet id="2" segmentAlignment="true" maxWidth="960" maxHeight="426" maxFrameRate="25" par="480:213" lang="eng" startWithSAP="1">
   <SegmentTemplate media="m1_video_$Number$.m4s" initialization="m1_video_init.mp4" timescale="12800" startNumber="1">
    <SegmentTimeline>
     <S t="0" d="24576" r="4"/>
    </SegmentTimeline>
   </SegmentTemplate>
   <Representation id="3" mimeType="video/mp4" codecs="avc1.64001E" width="960" height="426" frameRate="25" sar="1:1" bandwidth="100000">
   </Representation>
  </AdaptationSet>
 </Period>
 <Period start="PT0H0M9.600S" duration="PT0H0M9.600S">
  <AdaptationSet segmentAlignment="true" lang="und" startWithSAP="1">
   <SegmentTemplate media="m2_audio_$Number$.m4s" initialization="m2_audio_init.mp4" timescale="48000" startNumber="1">
    <SegmentTimeline>
     <S t="0" d="92160" r="4"/>
    </SegmentTimeline>
   </SegmentTemplate>
   <Representation id="2" mimeType="audio/mp4" codecs="mp4a.40.2" audioSamplingRate="48000" bandwidth="128000">
    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
    <BaseURL>https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/</BaseURL>
   </Representation>
  </AdaptationSet>
  <AdaptationSet segmentAlignment="true" maxWidth="960" maxHeight="426" maxFrameRate="25" par="480:213" lang="und" startWithSAP="1">
   <SegmentTemplate media="m2_video_$Number$.m4s" initialization="m2_video_init.mp4" timescale="12800" startNumber="1">
    <SegmentTimeline>
     <S t="0" d="24576" r="4"/>
    </SegmentTimeline>
   </SegmentTemplate>
   <Representation id="4" mimeType="video/mp4" codecs="avc1.64001E" width="960" height="426" frameRate="25" sar="1:1" bandwidth="100000">
    <BaseURL>https://dash.akamaized.net/dashif/ad-insertion-testcase1/batch5/real/a/</BaseURL>
   </Representation>
  </AdaptationSet>
 </Period>
</MPD>


)";

	mManifest = manifest;
	string currentTimeISO = "2023-01-01T00:10:00Z";
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	respData = GetManifestForMPDDownloader();
	mpd = respData->mMPDInstance.get();
	ParseHelper->Initialize(mpd);
	EXPECT_NE(mpd, nullptr);
	EXPECT_EQ(mpd->GetPeriods().size(), 2);

	uint64_t mpdDownloadTime ;
	mpdDownloadTime  = ISO8601DateTimeToUTCSeconds(currentTimeISO.c_str());
	double periodDuration = ParseHelper->aamp_GetPeriodDuration(0,mpdDownloadTime);

	// Check period duration
	EXPECT_EQ(periodDuration, 9600);
}


