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
#include "AampConfig.h"
#include "AampScheduler.h"
#include "AampLogManager.h"
#include "MPDModel.h"
#include "MockAampConfig.h"

using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::AnyNumber;
using ::testing::DoAll;
using namespace std;
AampConfig *gpGlobalConfig{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
    std::shared_ptr<DashMPDDocument> mpdDocument;
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		mpdDocument = nullptr;
	}

	void TearDown() override
	{
		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
		mpdDocument = nullptr;
	}

public:
     /**
     * @brief   Dump manifest file into specific location
     * @param   mpdDocument Dash MPD Document and file name
     * @retval  none
     */
    void dumpManifest(shared_ptr<DashMPDDocument> &newDocument, string filename) const
    {
        if(filename.empty())
        filename = "mpd-dump.mpd";
        static int seqNum = 1;
        string completeFilePath = "/tmp/dump/" + filename + "-" + to_string(seqNum);
        AAMPLOG_INFO("dumping manifest %s", completeFilePath.c_str());
        FILE *outputFile = fopen(completeFilePath.c_str(), "w");
        seqNum++;
        fwrite(newDocument->toString().c_str(), newDocument->toString().length(), 1, outputFile);
        fclose(outputFile);
    }
    /**
     * @brief   Merges Periods on update
     * @param   newDocument Dash MPD Document
     * @retval  Dash Mpd Document
     */
    shared_ptr<DashMPDDocument> mergePeriodsOnUpdate(shared_ptr<DashMPDDocument> &newDocument, shared_ptr<DashMPDDocument> &mpdDocument)
    {
        // To update MPD publish time
        double pubTime = newDocument->getRoot()->getPublishTime();
        if (pubTime == MPD_UNSET_DOUBLE)
            pubTime = 0;

        mpdDocument->getRoot()->setPublishTime(pubTime);
        // Collect new periods
        unordered_map<string, shared_ptr<DashMPDPeriod>> mpdPeriods;
        for (auto &period: mpdDocument->getRoot()->getPeriods()) {
            mpdPeriods[period->getId()] = period;
        }
        // Add any existing period not in new periods
        for (auto &period: newDocument->getRoot()->getPeriods()) {
            auto id = period->getId();
            if (mpdPeriods.find(id) == mpdPeriods.end()) {
                mpdDocument->getRoot()->addPeriod(period);
            } else {
                // Merge periods
                mpdPeriods[id]->mergePeriod(period);
            }
        }
        return mpdDocument;
    }
    /**
     * @brief process input manifest files and create mpd document
     * @param manifest files to merge
     * @retval none
    */
    void ProcessMPDMerge (const char* manifest1, const char *manifest2)
    {
        std::shared_ptr<DashMPDDocument> newDocument = NULL;
        std::string str1, str2;
        str1.assign(manifest1, strlen(manifest1));
        str2.assign(manifest2, strlen(manifest2));
        mpdDocument = make_shared <DashMPDDocument>(str1);
        newDocument = make_shared <DashMPDDocument>(str2);
        // To merhe newDocument with mpdDocument
        mpdDocument = mergePeriodsOnUpdate (newDocument, mpdDocument);
    }
};

/**
 * @brief ManifestStitchingBasic test.
 *
 * To verify two manifest file stitching with SegmentTimeline range and updated MPD contains sum of 
 * both MPD files with additionally added segments
 */
TEST_F(FunctionalTests, ManifestStitchingBasic_Test)
{
    static const char *manifest1 =
        R"(<?xml version="1.0" encoding="utf-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
                <Period id="1234">
                    <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                    <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                        <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                        <SegmentTimeline>
                            <S t="0" d="4" r="3599" />
                        </SegmentTimeline>
                        </SegmentTemplate>
                    </Representation>
                    </AdaptationSet>
                </Period>
            </MPD>
        )";
	static const char *manifest2 =
        R"(<?xml version="1.0" encoding="utf-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
                <Period id="1234">
                    <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                    <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                        <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                        <SegmentTimeline>
                            <S t="14388" d="4" r="9" />
                        </SegmentTimeline>
                        </SegmentTemplate>
                    </Representation>
                    </AdaptationSet>
                </Period>
            </MPD>
        )";
    // perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto period = root->getPeriods().at(0);
    auto adaptationSet = period->getAdaptationSets().at(0);
    auto representations = adaptationSet->getRepresentations();
    if (representations.size() > 0)
    {
        auto segTemplate = representations.at(0)->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long total_dur = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    auto rep = stoll(S.attribute("r", "0"));
                    total_dur += dur;
                    if (rep > 0) {
                        for (int i = 0; i < rep; i++) {
                            total_dur += dur;
                        }
                    }    
                    S = S.nextSiblingElement("S");
                }
                EXPECT_EQ(total_dur/timeScale, 3607);
                EXPECT_EQ(startTime, 0);
            }
        }
    }
    EXPECT_EQ(root->getPublishTime(), 0);
}
/**
 * @brief ManifestStitching_test.
 *
 * The add two MPD files with linear stream and final MPD contains newly added segment
 * details and verify total period and MPD file duration
 */
TEST_F(FunctionalTests, ManifestStitching_Comcast_Linear)
{
	static const char *manifest1 =
        R"(<?xml version="1.0" encoding="UTF-8"?>
        <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="5939026565177792163" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M2.016S" minimumUpdatePeriod="PT0H0M2.002S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2023-05-18T11:40:00.511Z">
            <Period id="841360827" start="PT403048H6M16.765S">
                <AdaptationSet id="2" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
                    <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                    <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                    </ContentProtection>
                    <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                    </ContentProtection>
                    <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
                    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
                    <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-video-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-video-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363616" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                        <S t="6672456473" d="180180" r="13"/>
                    </SegmentTimeline>
                    </SegmentTemplate>
                    <Representation id="root_video3" bandwidth="571600" codecs="avc1.4d4015" width="512" height="288" frameRate="30000/1001"/>
                    <Representation id="root_video2" bandwidth="828800" codecs="avc1.4d401e" width="640" height="360" frameRate="30000/1001"/>
                    <Representation id="root_video1" bandwidth="2107200" codecs="avc1.4d401f" width="960" height="540" frameRate="30000/1001"/>
                    <Representation id="root_video0" bandwidth="3375200" codecs="avc1.640020" width="1280" height="720" frameRate="60000/1001"/>
                </AdaptationSet>
                <AdaptationSet id="3" contentType="audio" mimeType="audio/mp4" lang="en">
                    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                    <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                    <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                    </ContentProtection>
                    <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                    </ContentProtection>
                    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
                    <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363616" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                        <S t="6672456944" d="180480" r="10"/>
                        <S t="6674442224" d="176640" r="0"/>
                        <S t="6674618864" d="180480" r="1"/>
                    </SegmentTimeline>
                    </SegmentTemplate>
                    <Representation id="root_audio102" bandwidth="117600" codecs="mp4a.40.5" audioSamplingRate="24000"/>
                </AdaptationSet>
                <AdaptationSet id="4" contentType="audio" mimeType="audio/mp4" lang="es">
                    <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                    <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                    <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                    </ContentProtection>
                    <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                    </ContentProtection>
                    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="dub"/>
                    <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363616" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                        <S t="6672458864" d="180480" r="3"/>
                        <S t="6673180784" d="176640" r="0"/>
                        <S t="6673357424" d="180480" r="8"/>
                    </SegmentTimeline>
                    </SegmentTemplate>
                    <Representation id="root_audio103" bandwidth="117600" codecs="mp4a.40.5" audioSamplingRate="24000"/>
                </AdaptationSet>
                <AdaptationSet id="5" contentType="audio" mimeType="audio/mp4" lang="en">
                    <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="f801"/>
                    <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                    <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                    </ContentProtection>
                    <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                    </ContentProtection>
                    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
                    <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363616" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                        <S t="6672457354" d="181440" r="0"/>
                        <S t="6672638794" d="178560" r="0"/>
                        <S t="6672817354" d="181440" r="0"/>
                        <S t="6672998794" d="178560" r="0"/>
                        <S t="6673177354" d="181440" r="1"/>
                        <S t="6673540234" d="178560" r="0"/>
                        <S t="6673718794" d="181440" r="0"/>
                        <S t="6673900234" d="178560" r="0"/>
                        <S t="6674078794" d="181440" r="0"/>
                        <S t="6674260234" d="178560" r="0"/>
                        <S t="6674438794" d="181440" r="1"/>
                        <S t="6674801674" d="178560" r="0"/>
                    </SegmentTimeline>
                    </SegmentTemplate>
                    <Representation id="root_audio104" bandwidth="213600" codecs="ec-3" audioSamplingRate="48000"/>
                </AdaptationSet>
                <AdaptationSet id="6" contentType="audio" mimeType="audio/mp4" lang="es">
                    <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
                    <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                    <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                    </ContentProtection>
                    <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                    </ContentProtection>
                    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="dub"/>
                    <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363616" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                        <S t="6672457354" d="181440" r="0"/>
                        <S t="6672638794" d="178560" r="0"/>
                        <S t="6672817354" d="181440" r="0"/>
                        <S t="6672998794" d="178560" r="0"/>
                        <S t="6673177354" d="181440" r="1"/>
                        <S t="6673540234" d="178560" r="0"/>
                        <S t="6673718794" d="181440" r="0"/>
                        <S t="6673900234" d="178560" r="0"/>
                        <S t="6674078794" d="181440" r="0"/>
                        <S t="6674260234" d="178560" r="0"/>
                        <S t="6674438794" d="181440" r="1"/>
                        <S t="6674801674" d="178560" r="0"/>
                    </SegmentTimeline>
                    </SegmentTemplate>
                    <Representation id="root_audio105" bandwidth="116000" codecs="ec-3" audioSamplingRate="48000"/>
                </AdaptationSet>
                <AdaptationSet id="50" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1" codingDependency="false" maxPlayoutRate="24">
                    <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                    <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                    </ContentProtection>
                    <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                    </ContentProtection>
                    <EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="2" />
                    <Role schemeIdUri="urn:mpeg:dash:role:2011" value="alternate"/>
                    <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-iframe-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-iframe-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363616" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                        <S t="6672456473" d="180180" r="13"/>
                    </SegmentTimeline>
                    </SegmentTemplate>
                    <Representation id="iframe0" bandwidth="337520" codecs="avc1.640020" width="1280" height="720" frameRate="60000/1001"/>
                </AdaptationSet>
            </Period>
        <SupplementalProperty schemeIdUri="urn:scte:dash:powered-by" value="example-mod_super8-4.9.0-1"/>
        </MPD>
        )";

	static const char *manifest2 =
        R"(<?xml version="1.0" encoding="UTF-8"?>
            <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:scte214="scte214" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:mspr="mspr" type="dynamic" id="5939026565177792163" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M2.016S" minimumUpdatePeriod="PT0H0M2.002S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2023-05-18T11:40:10.523Z">
            <Period id="841360827" start="PT403048H6M16.765S">
            <AdaptationSet id="2" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
                <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                </ContentProtection>
                <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                </ContentProtection>
                <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=en"/>
                <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
                <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-video-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-video-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363621" presentationTimeOffset="6169393913">
                <SegmentTimeline>
                    <S t="6673357373" d="180180" r="13"/>
                </SegmentTimeline>
                </SegmentTemplate>
                <Representation id="root_video3" bandwidth="571600" codecs="avc1.4d4015" width="512" height="288" frameRate="30000/1001"/>
                <Representation id="root_video2" bandwidth="828800" codecs="avc1.4d401e" width="640" height="360" frameRate="30000/1001"/>
                <Representation id="root_video1" bandwidth="2107200" codecs="avc1.4d401f" width="960" height="540" frameRate="30000/1001"/>
                <Representation id="root_video0" bandwidth="3375200" codecs="avc1.640020" width="1280" height="720" frameRate="60000/1001"/>
            </AdaptationSet>
            <AdaptationSet id="3" contentType="audio" mimeType="audio/mp4" lang="en">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                </ContentProtection>
                <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                </ContentProtection>
                <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
                <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363621" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                    <S t="6673359344" d="180480" r="5"/>
                    <S t="6674442224" d="176640" r="0"/>
                    <S t="6674618864" d="180480" r="6"/>
                    </SegmentTimeline>
                </SegmentTemplate>
                <Representation id="root_audio102" bandwidth="117600" codecs="mp4a.40.5" audioSamplingRate="24000"/>
                </AdaptationSet>
            <AdaptationSet id="4" contentType="audio" mimeType="audio/mp4" lang="es">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                </ContentProtection>
                <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                </ContentProtection>
                <Role schemeIdUri="urn:mpeg:dash:role:2011" value="dub"/>
                <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363620" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                    <S t="6673180784" d="176640" r="0"/>
                    <S t="6673357424" d="180480" r="11"/>
                    <S t="6675523184" d="176640" r="0"/>
                    <S t="6675699824" d="180480" r="0"/>
                    </SegmentTimeline>
                </SegmentTemplate>
                <Representation id="root_audio103" bandwidth="117600" codecs="mp4a.40.5" audioSamplingRate="24000"/>
            </AdaptationSet>
            <AdaptationSet id="5" contentType="audio" mimeType="audio/mp4" lang="en">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="f801"/>
                <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                </ContentProtection>
                <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                </ContentProtection>
                <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
                <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-audio-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363621" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                    <S t="6673358794" d="181440" r="0"/>
                    <S t="6673540234" d="178560" r="0"/>
                    <S t="6673718794" d="181440" r="0"/>
                    <S t="6673900234" d="178560" r="0"/>
                    <S t="6674078794" d="181440" r="0"/>
                    <S t="6674260234" d="178560" r="0"/>
                    <S t="6674438794" d="181440" r="1"/>
                    <S t="6674801674" d="178560" r="0"/>
                    <S t="6674980234" d="181440" r="0"/>
                    <S t="6675161674" d="178560" r="0"/>
                    <S t="6675340234" d="181440" r="0"/>
                    <S t="6675521674" d="178560" r="0"/>
                    <S t="6675700234" d="181440" r="0"/>
                    </SegmentTimeline>
                </SegmentTemplate>
                <Representation id="root_audio104" bandwidth="213600" codecs="ec-3" audioSamplingRate="48000"/>
            </AdaptationSet>
            <AdaptationSet id="6" contentType="audio" mimeType="audio/mp4" lang="es">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="a000"/>
                <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                </ContentProtection>
                <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                </ContentProtection>
                <Role schemeIdUri="urn:mpeg:dash:role:2011" value="dub"/>
                <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163-eac3/track-sap-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363621" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                    <S t="6673358794" d="181440" r="0"/>
                    <S t="6673540234" d="178560" r="0"/>
                    <S t="6673718794" d="181440" r="0"/>
                    <S t="6673900234" d="178560" r="0"/>
                    <S t="6674078794" d="181440" r="0"/>
                    <S t="6674260234" d="178560" r="0"/>
                    <S t="6674438794" d="181440" r="1"/>
                    <S t="6674801674" d="178560" r="0"/>
                    <S t="6674980234" d="181440" r="0"/>
                    <S t="6675161674" d="178560" r="0"/>
                    <S t="6675340234" d="181440" r="0"/>
                    <S t="6675521674" d="178560" r="0"/>
                    <S t="6675700234" d="181440" r="0"/>
                    </SegmentTimeline>
                </SegmentTemplate>
                <Representation id="root_audio105" bandwidth="116000" codecs="ec-3" audioSamplingRate="48000"/>
            </AdaptationSet>
            <AdaptationSet id="50" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1" codingDependency="false" maxPlayoutRate="24">
                <ContentProtection schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc" cenc:default_KID="f3dff538-b8c9-58e4-e8cd-96cf811d32dc"/>
                <ContentProtection schemeIdUri="urn:uuid:afbcb50e-bf74-3d13-be8f-13930c783962">
                    <cenc:pssh>AAAHzHBzc2gAAAAAr7y1Dr90PRO+jxOTDHg5YgAAB6xleUo0TlhRalV6STFOaUk2SWxsU1MxSkJaRmxRVjBRMlQyNXpTRnBDV0UwMFQwb3RURWxUWDA1TGNUZ3hSbGg2V0RjdFpUZFBjV2NpTENKaGJHY2lPaUpTVXpJMU5pSjkuZXlKa2NtMVVlWEJsSWpvaVkyVnVZeUlzSW1SeWJVNWhiV1VpT2lKalpXNWpJaXdpWkhKdFVISnZabWxzWlNJNklrTlBUVU5CVTFRdFEwVk9ReTFVVmtsTVNFUWlMQ0pqYjI1MFpXNTBWSGx3WlNJNkluUjJhV3hvWkNJc0ltTnZiblJsYm5SRGJHRnpjMmxtYVdOaGRHbHZiaUk2SW5RMlRHbHVaV0Z5SWl3aVkydHRVRzlzYVdONUlqb2lRMDlOUTBGVFZDMURSVTVETFZSV1NVeElSQ0lzSW1OdmJuUmxiblJKWkNJNklqVTVNemt3TWpZMU5qVXhOemMzT1RJeE5qTWlMQ0pqYjI1MFpXNTBTMlY1Y3lJNklsY3pjMmxhU0VwMFV6SldOVk5YVVdsUGFVcHRUVEpTYlZwcVZYcFBRekZwVDBkTk5VeFVWVFJhVkZGMFdsUm9hbHBETURWT2JVNXRUMFJGZUZwRVRYbGFSMDFwVEVOS2FtRXlNVTVhV0ZKb1drZEdNRmxUU1RaSmJFWnhVVEJPUWxaV2JFNVJWbEpLVjBWU1ZWTlliRTVTUlRFMlZGVlNRazVWTVRaU1ZFSk9UVmM1VGxGV1VsSlNWVlpNVkRCT2FsSnRTazFUYWtaaFRtdHNVRTF0YjNsU2JGRXpXbTVrUmxSVlJsVldWVlpIVVZoak5WbHFhRVZPYmtwNVV6SlZNMDlFYURKWGJtaDBZVlpDYUdWWGRIcGtXRXBDVWtWR1JrMXJTa3BUUm5CT1UxVm9XRkpGUm5kaGJVVjVUVVJhYWxKNmJIcFpWbVJQVGxWU1ExUnRjR2xOYWtaeFYxWm9UMDFGZUZoVWJYaHBZbFV4TUZwRmFHRmpSMHBJWVVkMFJWRllhSEZaVkVsM1RtMU9TRTlZVG1oV01EUXhWVEZrVWxSVlZYbFVibHBwVmpBMWIxbDZUbEprUm10NVZtNVdXbVZVUlhkYVJ6RnpZekpHU0ZWVk1VUmlWVFV5V1cwMVUySkhTblZWVkZwb1ZqRkdUbEpZY0ZaT1ZURTJZVE5rVG1Gc2EzaFViWEJXWlVVMU5sbDZUbEJXUld3MFZHMXdUbFJWVGxoVmJteHBWa2hDZVZkc2FITlRiSEJDWkRKMFlXRnJOWEpYYlRGYVRWVXhObG96VWxwaGJXaHhWREZOZDAxVk9VaFdWRUpOVmpGVk1GZFVTbEprUlRsVlYyMXdZV0Z0WkRSVVZtUlNaV3N4ZEZWdGNFVlJia0p4V1dwSk1VMUdjRmhPVkVKUVlsaFNjMXBXVmxOaVIwNTBZa1JLV2xkR1NuZFpha2t4VkVad1dXSkZjR0ZSV0dSd1YxUkpOV1JHYTNsU2JuQnJVWHBHTTFSVlVrcGtSbXQ1WkVoU1RWZEZTbkZYYkdNeFlXMVNTVmR1UW1sUmVrSXpWRlZTUm1SR2EzbGtSM1JvWkROa1ExUnVaRE5oVm10NVQxaFNXazFyV2paYVJVMTRaREF4UlZOWVVscE5ibEl3VkVab1EyRnNjRmhPVjNCclUwWndkMWxyVFhka01ERkZVMWhTYVZZeFNqQlpXR05wWmxZd1BTSXNJbWx6Y3lJNklrTk9QVkF3TWpBd01EQXdNRGt3TENCUFZUMTFjbTQ2WTI5dFkyRnpkRHBqWTNBNmNHdHBMV056TFdsdWREcGphMjBzSUU4OVEyOXRZMkZ6ZENCRGIyNTJaWEpuWldRZ1VISnZaSFZqZEhNZ1RFeERMQ0JNUFZCb2FXeGhaR1ZzY0docFlTd2dVMVE5VUVFc0lFTTlWVk1pTENKdVltWWlPakUyTkRnMk16STNNRE1zSW1saGRDSTZNVFkwT0RZek1qY3dNeXdpZG1WeWMybHZiaUk2SWpJaUxDSnRaWE56WVdkbFZIbHdaU0k2SW1OdmJuUmxiblJOWlhSaFpHRjBZU0lzSW1GMVpDSTZJbVJzY3lJc0ltcDBhU0k2SW5aWlREbDBXVXczYmtsSE9UTlpVVmwwT1c5Mk9HYzlQU0o5LkU4aC1leXZ5Y1AxN2Y5UTI4cDlBWTVEZS14X2tiVng3ZTlaQWlOSnVwTWd6dUoxbzBfQWFwMTdIY1V3MjBVWUtkRHlFQmFPVGJSNzNTR1o4X2lYcjhGaEx4QWktdkJDaEpQSzU2aE1ua1RBOWtlQU9sYjktV3lFdmdLNExlTmZYczdwdkZfd3djM2ppRFNqR3FMMWd5Sk41eW5sNkptU2pOUkRWTXBRVERwZWRsOTduXzBVUVRKLVNWNXEzWjA0cno5VV9ianMxYUFfZG9uUVB3N1hSZEhKajg5VERwS1lscTk0R0FzNHJWWTN2V0pDc2ttd2NaUDRwZmFfZ090Wl82azRhMnVWUVVhcFlfX21HSnJQd1dFaVJZbVo5STc1aUMxOXpMUjcyVGFhajlnTmpkeTJ6QU1EajI1b3pNODZYNHctS1NteGNxR2xJSXUzOTY5T0NNUQ==</cenc:pssh>
                </ContentProtection>
                <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                    <cenc:pssh>AAAAW3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAADsSJGYzZGZmNTM4LWI4YzktNThlNC1lOGNkLTk2Y2Y4MTFkMzJkYyITNTkzOTAyNjU2NTE3Nzc5MjE2Mw==</cenc:pssh>
                </ContentProtection>
                <EssentialProperty schemeIdUri="http://dashif.org/guidelines/trickmode" value="2" />
                <Role schemeIdUri="urn:mpeg:dash:role:2011" value="alternate"/>
                <SegmentTemplate initialization="HBOHD_HD_NAT_15152_0_5939026565177792163/track-iframe-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-header.mp4" media="HBOHD_HD_NAT_15152_0_5939026565177792163/track-iframe-periodid-841360827-repid-$RepresentationID$-tc-0-enc-cenc-frag-$Number$.mp4" timescale="90000" startNumber="841363621" presentationTimeOffset="6169393913">
                    <SegmentTimeline>
                    <S t="6673357373" d="180180" r="13"/>
                    </SegmentTimeline>
                </SegmentTemplate>
                <Representation id="iframe0" bandwidth="337520" codecs="avc1.640020" width="1280" height="720" frameRate="60000/1001"/>
            </AdaptationSet>
        </Period>
        <SupplementalProperty schemeIdUri="urn:scte:dash:powered-by" value="example-mod_super8-4.9.0-1"/>
        </MPD>
        )";
    // perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto period = root->getPeriods().at(0);
    auto adaptationSet = period->getAdaptationSets().at(0);
    auto segTemplate = adaptationSet->getSegmentTemplate();
    if (segTemplate)
    {
        auto segTimeline = segTemplate->getSegmentTimeline();
        long timeScale = segTemplate->getTimeScale();
        if (segTimeline)
        {
            DomElement S = segTimeline->elem.firstChildElement("S");
            long long curTime = 0;
            long long startTime;
            // calcuate duration of 0th period, since manifest contains only one period entry
            while (!S.isNull())
            {
                startTime = stoll(S.attribute("t", "0"));
                auto dur = stoll(S.attribute("d", "0"));
                curTime += dur;
                auto rep = stoll(S.attribute("r", "0"));
                if (rep > 0) {
                    for (int i = 0; i < rep; i++) {
                        curTime += dur;
                    }
                }
                S = S.nextSiblingElement("S");
            }
            long duration = curTime/timeScale;
            EXPECT_EQ(duration, 38);
            EXPECT_EQ(startTime, 6672456473);
        }
    }
    EXPECT_EQ((long)(root->getPublishTime()), 1684410010);
}

/**
 * @brief ManifestStitching_SpecificApp_2Hour_MPD test.
 *
 * To verify segment stitching with peacocok unlimited content of two hour duration and verify total periods and
 * MPD duration is properly updated.
 */
TEST_F(FunctionalTests, ManifestStitching_SpecificApp_2Hour_MPD)
{
	AAMPStatusType status;
	static const char *manifest1 =
R"(<?xml version="1.0" encoding="UTF-8"?>
  <MPD availabilityStartTime="2023-02-07T07:50:25Z" minBufferTime="PT9.6S" minimumUpdatePeriod="PT4.8S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2023-05-17T10:08:41.349Z" timeShiftBufferDepth="PT2H" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:dolby="http://www.dolby.com/ns/online/DASH" xmlns:mspr="urn:microsoft:playready" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
    <BaseURL>https://cfrt.stream.exampletv.com/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/</BaseURL>
    <Location>https://dc0c37fd4d014d2a92bb602c3ede9c88.mediatailor.us-east-1.amazonaws.com/v1/dash/6f3f45fea6332a47667932dede90d20a96f2690c/example-cmaf-dash-linear-4s-021821/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/master_2hr.mpd?c3.ri=3779723265365281424&amp;aws.sessionId=cc7de321-1e82-42a5-9a55-8b4f4c5a6f46</Location>
    <Period id="21556" start="PT2376H11M47.8716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="2">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/QNB79QAmAiRDVUVJAAAAAn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAvaLJO</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="1">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/QNB79QAmAiRDVUVJAAAAAX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACvkejp</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078716340" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088716340"/>
                    <S d="27333333" t="85550640716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543079001950" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088921950"/>
                    <S d="27200000" t="85550640921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078788616" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088815283"/>
                    <S d="27306667" t="85550640815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21557" start="PT2376H24M26.8049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="4">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ROK4dQArAilDVUVJAAAABH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAHpKN3I=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="3">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ROK4dQArAilDVUVJAAAAA3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAK0ICNg=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20666667" t="85550668049673"/>
                    <S d="48000000" r="10" t="85550688716340"/>
                    <S d="52333333" t="85551216716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20800000" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688921950"/>
                    <S d="52160000" t="85551216921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20800000" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688921950"/>
                        <S d="52160000" t="85551216921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20693333" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688815283"/>
                    <S d="52266667" t="85551216815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20693333" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688815283"/>
                        <S d="52266667" t="85551216815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21558" start="PT2376H25M26.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="4">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/RTVBXQAmAiRDVUVJAAAABH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAC3Rc/J</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="3">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/RTVBXQAmAiRDVUVJAAAAA3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAADlhJIF</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269049673" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43666667" t="85551269049673"/>
                    <S d="48000000" r="137" t="85551312716340"/>
                    <S d="41666666" t="85557936716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269081950" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43840000" t="85551269081950"/>
                    <S d="48000000" r="137" t="85551312921950"/>
                    <S d="41600000" t="85557936921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269081950" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43733333" t="85551269081950"/>
                    <S d="48000000" r="137" t="85551312815283"/>
                    <S d="41600000" t="85557936815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21559" start="PT2376H36M37.8383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="6">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/SM6kXQArAilDVUVJAAAABn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAHBDkIM=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="5">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/SM6kXQArAilDVUVJAAAABX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAMXNmfs=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                <SegmentTimeline>
                    <S d="54333334" t="85557978383006"/>
                    <S d="48000000" r="10" t="85558032716340"/>
                    <S d="18666666" t="85558560716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978521950" startNumber="1789670" timescale="10000000">
                <SegmentTimeline>
                    <S d="54400000" t="85557978521950"/>
                    <S d="48000000" r="10" t="85558032921950"/>
                    <S d="18560000" t="85558560921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978521950" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54400000" t="85557978521950"/>
                        <S d="48000000" r="10" t="85558032921950"/>
                        <S d="18560000" t="85558560921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978415283" startNumber="1789670" timescale="10000000">
                <SegmentTimeline>
                    <S d="54400000" t="85557978415283"/>
                    <S d="48000000" r="10" t="85558032815283"/>
                    <S d="18773333" t="85558560815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978415283" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54400000" t="85557978415283"/>
                        <S d="48000000" r="10" t="85558032815283"/>
                        <S d="18773333" t="85558560815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21560" start="PT2376H37M37.9383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="6">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/SSEtRQAmAiRDVUVJAAAABn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAUkpfh</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="5">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/SSEtRQAmAiRDVUVJAAAABX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACUa81G</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85558579383006" startNumber="1789683" timescale="10000000">
                <SegmentTimeline>
                    <S d="29333334" t="85558579383006"/>
                    <S d="48000000" r="168" t="85558608716340"/>
                    <S d="24000000" t="85566720716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85558579481950" startNumber="1789683" timescale="10000000">
                <SegmentTimeline>
                    <S d="29440000" t="85558579481950"/>
                    <S d="48000000" r="168" t="85558608921950"/>
                    <S d="24000000" t="85566720921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85558579588616" startNumber="1789683" timescale="10000000">
                <SegmentTimeline>
                    <S d="29226667" t="85558579588616"/>
                    <S d="48000000" r="168" t="85558608815283"/>
                    <S d="24106667" t="85566720815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21561" start="PT2376H51M14.471634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="8">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/TYKDxQArAilDVUVJAAAACH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAMwkqTM=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="7">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/TYKDxQArAilDVUVJAAAAB3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAN7++z0=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                <SegmentTimeline>
                    <S d="24000000" t="85566744716340"/>
                    <S d="48000000" r="10" t="85566768716340"/>
                    <S d="48666666" t="85567296716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                <SegmentTimeline>
                    <S d="24000000" t="85566744921950"/>
                    <S d="48000000" r="10" t="85566768921950"/>
                    <S d="48640000" t="85567296921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744921950"/>
                        <S d="48000000" r="10" t="85566768921950"/>
                        <S d="48640000" t="85567296921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                <SegmentTimeline>
                    <S d="23893333" t="85566744921950"/>
                    <S d="48000000" r="10" t="85566768815283"/>
                    <S d="48640000" t="85567296815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23893333" t="85566744921950"/>
                        <S d="48000000" r="10" t="85566768815283"/>
                        <S d="48640000" t="85567296815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21562" start="PT2376H52M14.5383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="8">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/TdUA9QAmAiRDVUVJAAAACH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAABN21Rl</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="7">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/TdUA9QAmAiRDVUVJAAAAB3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAAC/qxrI</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85567345383006" startNumber="1789867" timescale="10000000">
                <SegmentTimeline>
                    <S d="47333334" t="85567345383006"/>
                    <S d="48000000" r="114" t="85567392716340"/>
                    <S d="22000000" t="85572912716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85567345561950" startNumber="1789867" timescale="10000000">
                <SegmentTimeline>
                    <S d="47360000" t="85567345561950"/>
                    <S d="48000000" r="114" t="85567392921950"/>
                    <S d="22080000" t="85572912921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85567345455283" startNumber="1789867" timescale="10000000">
                <SegmentTimeline>
                    <S d="47360000" t="85567345455283"/>
                    <S d="48000000" r="114" t="85567392815283"/>
                    <S d="21973333" t="85572912815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21563" start="PT2377H1M33.471634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="10">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/UNSU9QArAilDVUVJAAAACn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAO8r7hc=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="9">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/UNSU9QArAilDVUVJAAAACX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAFql528=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                <SegmentTimeline>
                    <S d="26000000" t="85572934716340"/>
                    <S d="48000000" r="10" t="85572960716340"/>
                    <S d="47000000" t="85573488716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572935001950" startNumber="1789984" timescale="10000000">
                <SegmentTimeline>
                    <S d="25920000" t="85572935001950"/>
                    <S d="48000000" r="10" t="85572960921950"/>
                    <S d="47040000" t="85573488921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572935001950" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="25920000" t="85572935001950"/>
                        <S d="48000000" r="10" t="85572960921950"/>
                        <S d="47040000" t="85573488921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934788616" startNumber="1789984" timescale="10000000">
                <SegmentTimeline>
                    <S d="26026667" t="85572934788616"/>
                    <S d="48000000" r="10" t="85572960815283"/>
                    <S d="46933333" t="85573488815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934788616" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26026667" t="85572934788616"/>
                        <S d="48000000" r="10" t="85572960815283"/>
                        <S d="46933333" t="85573488815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21564" start="PT2377H2M33.5716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="10">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/UScd3QAmAiRDVUVJAAAACn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAADZGSTl</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="9">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/UScd3QAmAiRDVUVJAAAACX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAABZ4H5C</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85573535716340" startNumber="1789997" timescale="10000000">
                <SegmentTimeline>
                    <S d="49000000" t="85573535716340"/>
                    <S d="48000000" r="165" t="85573584716340"/>
                    <S d="36333333" t="85581552716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85573535961950" startNumber="1789997" timescale="10000000">
                <SegmentTimeline>
                    <S d="48960000" t="85573535961950"/>
                    <S d="48000000" r="165" t="85573584921950"/>
                    <S d="36160000" t="85581552921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85573535748616" startNumber="1789997" timescale="10000000">
                <SegmentTimeline>
                    <S d="49066667" t="85573535748616"/>
                    <S d="48000000" r="165" t="85573584815283"/>
                    <S d="36266667" t="85581552815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21565" start="PT2377H15M58.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="12">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/VXkS3QArAilDVUVJAAAADH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAExnM/4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="11">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/VXkS3QArAilDVUVJAAAAC3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAJslDFQ=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                <SegmentTimeline>
                    <S d="59666667" t="85581589049673"/>
                    <S d="48000000" r="9" t="85581648716340"/>
                    <S d="61333333" t="85582128716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                <SegmentTimeline>
                    <S d="59840000" t="85581589081950"/>
                    <S d="48000000" r="9" t="85581648921950"/>
                    <S d="61440000" t="85582128921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59840000" t="85581589081950"/>
                        <S d="48000000" r="9" t="85581648921950"/>
                        <S d="61440000" t="85582128921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                <SegmentTimeline>
                    <S d="59733333" t="85581589081950"/>
                    <S d="48000000" r="9" t="85581648815283"/>
                    <S d="61440000" t="85582128815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59733333" t="85581589081950"/>
                        <S d="48000000" r="9" t="85581648815283"/>
                        <S d="61440000" t="85582128815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21566" start="PT2377H16M59.0049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="12">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/VcubxQAmAiRDVUVJAAAADH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAClPQ6r</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="11">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/VcubxQAmAiRDVUVJAAAAC3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAAD3/FNn</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85582190049673" startNumber="1790177" timescale="10000000">
                <SegmentTimeline>
                    <S d="34666667" t="85582190049673"/>
                    <S d="48000000" r="135" t="85582224716340"/>
                    <S d="17000000" t="85588752716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85582190361950" startNumber="1790177" timescale="10000000">
                <SegmentTimeline>
                    <S d="34560000" t="85582190361950"/>
                    <S d="48000000" r="135" t="85582224921950"/>
                    <S d="16960000" t="85588752921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85582190255283" startNumber="1790177" timescale="10000000">
                <SegmentTimeline>
                    <S d="34560000" t="85582190255283"/>
                    <S d="48000000" r="135" t="85582224815283"/>
                    <S d="17066667" t="85588752815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21567" start="PT2377H27M56.971634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="14">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/WVMwLQArAilDVUVJAAAADn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAAQQPV4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="13">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/WVMwLQArAilDVUVJAAAADX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAALGeNCY=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                <SegmentTimeline>
                    <S d="31000000" t="85588769716340"/>
                    <S d="48000000" r="10" t="85588800716340"/>
                    <S d="42000000" t="85589328716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                <SegmentTimeline>
                    <S d="31040000" t="85588769881950"/>
                    <S d="48000000" r="10" t="85588800921950"/>
                    <S d="41920000" t="85589328921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31040000" t="85588769881950"/>
                        <S d="48000000" r="10" t="85588800921950"/>
                        <S d="41920000" t="85589328921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                <SegmentTimeline>
                    <S d="30933333" t="85588769881950"/>
                    <S d="48000000" r="10" t="85588800815283"/>
                    <S d="42026667" t="85589328815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="30933333" t="85588769881950"/>
                        <S d="48000000" r="10" t="85588800815283"/>
                        <S d="42026667" t="85589328815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21568" start="PT2377H28M57.0716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="14">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/WaW5FQAmAiRDVUVJAAAADn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAEO5XX</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="13">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/WaW5FQAmAiRDVUVJAAAADX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACEws9w</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85589370716340" startNumber="1790328" timescale="10000000">
                <SegmentTimeline>
                    <S d="54000000" t="85589370716340"/>
                    <S d="48000000" r="134" t="85589424716340"/>
                    <S d="46000000" t="85595904716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85589370841950" startNumber="1790328" timescale="10000000">
                <SegmentTimeline>
                    <S d="54080000" t="85589370841950"/>
                    <S d="48000000" r="134" t="85589424921950"/>
                    <S d="46080000" t="85595904921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85589370841950" startNumber="1790328" timescale="10000000">
                <SegmentTimeline>
                    <S d="53973333" t="85589370841950"/>
                    <S d="48000000" r="134" t="85589424815283"/>
                    <S d="46080000" t="85595904815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21569" start="PT2377H39M55.071634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="16">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/XS1ZNQArAilDVUVJAAAAEH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAABSpxw4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="15">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/XS1ZNQArAilDVUVJAAAAD3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAImCU/8=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                <SegmentTimeline>
                    <S d="50000000" t="85595950716340"/>
                    <S d="48000000" r="10" t="85596000716340"/>
                    <S d="23000000" t="85596528716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595951001950" startNumber="1790465" timescale="10000000">
                <SegmentTimeline>
                    <S d="49920000" t="85595951001950"/>
                    <S d="48000000" r="10" t="85596000921950"/>
                    <S d="23040000" t="85596528921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595951001950" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="49920000" t="85595951001950"/>
                        <S d="48000000" r="10" t="85596000921950"/>
                        <S d="23040000" t="85596528921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950895283" startNumber="1790465" timescale="10000000">
                <SegmentTimeline>
                    <S d="49920000" t="85595950895283"/>
                    <S d="48000000" r="10" t="85596000815283"/>
                    <S d="23040000" t="85596528815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950895283" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="49920000" t="85595950895283"/>
                        <S d="48000000" r="10" t="85596000815283"/>
                        <S d="23040000" t="85596528815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21570" start="PT2377H40M55.1716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="16">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/XX/iHQAmAiRDVUVJAAAAEH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAADMyPqb</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="15">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/XX/iHQAmAiRDVUVJAAAAD3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAAB7G49D</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85596551716340" startNumber="1790478" timescale="10000000">
                <SegmentTimeline>
                    <S d="25000000" t="85596551716340"/>
                    <S d="48000000" r="135" t="85596576716340"/>
                    <S d="24666666" t="85603104716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85596551961950" startNumber="1790478" timescale="10000000">
                <SegmentTimeline>
                    <S d="24960000" t="85596551961950"/>
                    <S d="48000000" r="135" t="85596576921950"/>
                    <S d="24640000" t="85603104921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85596551855283" startNumber="1790478" timescale="10000000">
                <SegmentTimeline>
                    <S d="24960000" t="85596551855283"/>
                    <S d="48000000" r="135" t="85596576815283"/>
                    <S d="24746667" t="85603104815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21571" start="PT2377H51M52.9383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="18">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/YQcwNQArAilDVUVJAAAAEn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAANaHnLM=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="17">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/YQcwNQArAilDVUVJAAAAEX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAGMJlcs=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                <SegmentTimeline>
                    <S d="23333334" t="85603129383006"/>
                    <S d="48000000" r="10" t="85603152716340"/>
                    <S d="49333333" t="85603680716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                <SegmentTimeline>
                    <S d="23360000" t="85603129561950"/>
                    <S d="48000000" r="10" t="85603152921950"/>
                    <S d="49280000" t="85603680921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23360000" t="85603129561950"/>
                        <S d="48000000" r="10" t="85603152921950"/>
                        <S d="49280000" t="85603680921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                <SegmentTimeline>
                    <S d="23253333" t="85603129561950"/>
                    <S d="48000000" r="10" t="85603152815283"/>
                    <S d="49280000" t="85603680815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23253333" t="85603129561950"/>
                        <S d="48000000" r="10" t="85603152815283"/>
                        <S d="49280000" t="85603680815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21572" start="PT2377H52M53.0049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="18">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/YVmtZQAmAiRDVUVJAAAAEn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAsAzeZ</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="17">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/YVmtZQAmAiRDVUVJAAAAEX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACs+m0+</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603730049673" startNumber="1790629" timescale="10000000">
                <SegmentTimeline>
                    <S d="46666667" t="85603730049673"/>
                    <S d="48000000" r="131" t="85603776716340"/>
                    <S d="35333333" t="85610112716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603730201950" startNumber="1790629" timescale="10000000">
                <SegmentTimeline>
                    <S d="46720000" t="85603730201950"/>
                    <S d="48000000" r="131" t="85603776921950"/>
                    <S d="35200000" t="85610112921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603730095283" startNumber="1790629" timescale="10000000">
                <SegmentTimeline>
                    <S d="46720000" t="85603730095283"/>
                    <S d="48000000" r="131" t="85603776815283"/>
                    <S d="35413333" t="85610112815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21573" start="PT2378H3M34.8049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="20">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAFH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAEOV0w4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="19">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAE3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAJTX7KQ=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60666667" t="85610148049673"/>
                    <S d="48000000" r="9" t="85610208716340"/>
                    <S d="60333333" t="85610688716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60800000" t="85610148121950"/>
                    <S d="48000000" r="9" t="85610208921950"/>
                    <S d="60160000" t="85610688921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60800000" t="85610148121950"/>
                        <S d="48000000" r="9" t="85610208921950"/>
                        <S d="60160000" t="85610688921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60586667" t="85610148228616"/>
                    <S d="48000000" r="9" t="85610208815283"/>
                    <S d="60373333" t="85610688815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60586667" t="85610148228616"/>
                        <S d="48000000" r="9" t="85610208815283"/>
                        <S d="60373333" t="85610688815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21574" start="PT2378H4M34.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="20">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAFH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAD/c8cV</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="19">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAE3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACtsprZ</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749049673" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35666667" t="85610749049673"/>
                    <S d="48000000" r="172" t="85610784716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749081950" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35840000" t="85610749081950"/>
                    <S d="48000000" r="172" t="85610784921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749188616" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35626667" t="85610749188616"/>
                    <S d="48000000" r="172" t="85610784815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <UTCTiming schemeIdUri="urn:mpeg:dash:utc:http-iso:2014" value="https://time.akamai.com/?iso&amp;ms"/>
  </MPD>
)";

	static const char *manifest2 =
  R"(<?xml version="1.0" encoding="UTF-8"?>
    <MPD availabilityStartTime="2023-02-07T07:50:25Z" minBufferTime="PT9.6S" minimumUpdatePeriod="PT4.8S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2023-05-17T10:09:19.749Z" timeShiftBufferDepth="PT2H" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:dolby="http://www.dolby.com/ns/online/DASH" xmlns:mspr="urn:microsoft:playready" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
    <BaseURL>https://cfrt.stream.exampletv.com/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/</BaseURL>
    <Location>https://dc0c37fd4d014d2a92bb602c3ede9c88.mediatailor.us-east-1.amazonaws.com/v1/dash/6f3f45fea6332a47667932dede90d20a96f2690c/example-cmaf-dash-linear-4s-021821/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/master_2hr.mpd?c3.ri=3779723265365281424&amp;aws.sessionId=cc7de321-1e82-42a5-9a55-8b4f4c5a6f46</Location>
    <Period id="21556" start="PT2376H11M47.8716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="2">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/QNB79QAmAiRDVUVJAAAAAn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAvaLJO</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="1">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/QNB79QAmAiRDVUVJAAAAAX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACvkejp</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078716340" startNumber="1789450" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="65" t="85547472716340"/>
                    <S d="27333333" t="85550640716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543079001950" startNumber="1789450" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="65" t="85547472921950"/>
                    <S d="27200000" t="85550640921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078788616" startNumber="1789450" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="65" t="85547472815283"/>
                    <S d="27306667" t="85550640815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21557" start="PT2376H24M26.8049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="4">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ROK4dQArAilDVUVJAAAABH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAHpKN3I=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="3">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ROK4dQArAilDVUVJAAAAA3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAK0ICNg=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20666667" t="85550668049673"/>
                    <S d="48000000" r="10" t="85550688716340"/>
                    <S d="52333333" t="85551216716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20800000" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688921950"/>
                    <S d="52160000" t="85551216921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20800000" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688921950"/>
                        <S d="52160000" t="85551216921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20693333" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688815283"/>
                    <S d="52266667" t="85551216815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20693333" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688815283"/>
                        <S d="52266667" t="85551216815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21558" start="PT2376H25M26.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="4">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/RTVBXQAmAiRDVUVJAAAABH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAC3Rc/J</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="3">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/RTVBXQAmAiRDVUVJAAAAA3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAADlhJIF</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269049673" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43666667" t="85551269049673"/>
                    <S d="48000000" r="137" t="85551312716340"/>
                    <S d="41666666" t="85557936716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269081950" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43840000" t="85551269081950"/>
                    <S d="48000000" r="137" t="85551312921950"/>
                    <S d="41600000" t="85557936921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269081950" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43733333" t="85551269081950"/>
                    <S d="48000000" r="137" t="85551312815283"/>
                    <S d="41600000" t="85557936815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21559" start="PT2376H36M37.8383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="6">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/SM6kXQArAilDVUVJAAAABn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAHBDkIM=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="5">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/SM6kXQArAilDVUVJAAAABX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAMXNmfs=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                <SegmentTimeline>
                    <S d="54333334" t="85557978383006"/>
                    <S d="48000000" r="10" t="85558032716340"/>
                    <S d="18666666" t="85558560716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978383006" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54333334" t="85557978383006"/>
                        <S d="48000000" r="10" t="85558032716340"/>
                        <S d="18666666" t="85558560716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978521950" startNumber="1789670" timescale="10000000">
                <SegmentTimeline>
                    <S d="54400000" t="85557978521950"/>
                    <S d="48000000" r="10" t="85558032921950"/>
                    <S d="18560000" t="85558560921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978521950" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54400000" t="85557978521950"/>
                        <S d="48000000" r="10" t="85558032921950"/>
                        <S d="18560000" t="85558560921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978415283" startNumber="1789670" timescale="10000000">
                <SegmentTimeline>
                    <S d="54400000" t="85557978415283"/>
                    <S d="48000000" r="10" t="85558032815283"/>
                    <S d="18773333" t="85558560815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85557978415283" startNumber="1789670" timescale="10000000">
                    <SegmentTimeline>
                        <S d="54400000" t="85557978415283"/>
                        <S d="48000000" r="10" t="85558032815283"/>
                        <S d="18773333" t="85558560815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21560" start="PT2376H37M37.9383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="6">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/SSEtRQAmAiRDVUVJAAAABn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAUkpfh</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="5">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/SSEtRQAmAiRDVUVJAAAABX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACUa81G</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85558579383006" startNumber="1789683" timescale="10000000">
                <SegmentTimeline>
                    <S d="29333334" t="85558579383006"/>
                    <S d="48000000" r="168" t="85558608716340"/>
                    <S d="24000000" t="85566720716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85558579481950" startNumber="1789683" timescale="10000000">
                <SegmentTimeline>
                    <S d="29440000" t="85558579481950"/>
                    <S d="48000000" r="168" t="85558608921950"/>
                    <S d="24000000" t="85566720921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85558579588616" startNumber="1789683" timescale="10000000">
                <SegmentTimeline>
                    <S d="29226667" t="85558579588616"/>
                    <S d="48000000" r="168" t="85558608815283"/>
                    <S d="24106667" t="85566720815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21561" start="PT2376H51M14.471634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="8">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/TYKDxQArAilDVUVJAAAACH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAMwkqTM=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="7">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/TYKDxQArAilDVUVJAAAAB3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAN7++z0=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                <SegmentTimeline>
                    <S d="24000000" t="85566744716340"/>
                    <S d="48000000" r="10" t="85566768716340"/>
                    <S d="48666666" t="85567296716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744716340" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744716340"/>
                        <S d="48000000" r="10" t="85566768716340"/>
                        <S d="48666666" t="85567296716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                <SegmentTimeline>
                    <S d="24000000" t="85566744921950"/>
                    <S d="48000000" r="10" t="85566768921950"/>
                    <S d="48640000" t="85567296921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="24000000" t="85566744921950"/>
                        <S d="48000000" r="10" t="85566768921950"/>
                        <S d="48640000" t="85567296921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                <SegmentTimeline>
                    <S d="23893333" t="85566744921950"/>
                    <S d="48000000" r="10" t="85566768815283"/>
                    <S d="48640000" t="85567296815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85566744921950" startNumber="1789854" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23893333" t="85566744921950"/>
                        <S d="48000000" r="10" t="85566768815283"/>
                        <S d="48640000" t="85567296815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21562" start="PT2376H52M14.5383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="8">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/TdUA9QAmAiRDVUVJAAAACH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAABN21Rl</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="7">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/TdUA9QAmAiRDVUVJAAAAB3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAAC/qxrI</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85567345383006" startNumber="1789867" timescale="10000000">
                <SegmentTimeline>
                    <S d="47333334" t="85567345383006"/>
                    <S d="48000000" r="114" t="85567392716340"/>
                    <S d="22000000" t="85572912716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85567345561950" startNumber="1789867" timescale="10000000">
                <SegmentTimeline>
                    <S d="47360000" t="85567345561950"/>
                    <S d="48000000" r="114" t="85567392921950"/>
                    <S d="22080000" t="85572912921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85567345455283" startNumber="1789867" timescale="10000000">
                <SegmentTimeline>
                    <S d="47360000" t="85567345455283"/>
                    <S d="48000000" r="114" t="85567392815283"/>
                    <S d="21973333" t="85572912815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21563" start="PT2377H1M33.471634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="10">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/UNSU9QArAilDVUVJAAAACn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAO8r7hc=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="9">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/UNSU9QArAilDVUVJAAAACX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAFql528=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                <SegmentTimeline>
                    <S d="26000000" t="85572934716340"/>
                    <S d="48000000" r="10" t="85572960716340"/>
                    <S d="47000000" t="85573488716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934716340" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26000000" t="85572934716340"/>
                        <S d="48000000" r="10" t="85572960716340"/>
                        <S d="47000000" t="85573488716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572935001950" startNumber="1789984" timescale="10000000">
                <SegmentTimeline>
                    <S d="25920000" t="85572935001950"/>
                    <S d="48000000" r="10" t="85572960921950"/>
                    <S d="47040000" t="85573488921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572935001950" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="25920000" t="85572935001950"/>
                        <S d="48000000" r="10" t="85572960921950"/>
                        <S d="47040000" t="85573488921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934788616" startNumber="1789984" timescale="10000000">
                <SegmentTimeline>
                    <S d="26026667" t="85572934788616"/>
                    <S d="48000000" r="10" t="85572960815283"/>
                    <S d="46933333" t="85573488815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85572934788616" startNumber="1789984" timescale="10000000">
                    <SegmentTimeline>
                        <S d="26026667" t="85572934788616"/>
                        <S d="48000000" r="10" t="85572960815283"/>
                        <S d="46933333" t="85573488815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21564" start="PT2377H2M33.5716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="10">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/UScd3QAmAiRDVUVJAAAACn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAADZGSTl</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="9">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/UScd3QAmAiRDVUVJAAAACX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAABZ4H5C</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85573535716340" startNumber="1789997" timescale="10000000">
                <SegmentTimeline>
                    <S d="49000000" t="85573535716340"/>
                    <S d="48000000" r="165" t="85573584716340"/>
                    <S d="36333333" t="85581552716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85573535961950" startNumber="1789997" timescale="10000000">
                <SegmentTimeline>
                    <S d="48960000" t="85573535961950"/>
                    <S d="48000000" r="165" t="85573584921950"/>
                    <S d="36160000" t="85581552921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85573535748616" startNumber="1789997" timescale="10000000">
                <SegmentTimeline>
                    <S d="49066667" t="85573535748616"/>
                    <S d="48000000" r="165" t="85573584815283"/>
                    <S d="36266667" t="85581552815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21565" start="PT2377H15M58.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="12">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/VXkS3QArAilDVUVJAAAADH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAExnM/4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="11">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/VXkS3QArAilDVUVJAAAAC3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAJslDFQ=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                <SegmentTimeline>
                    <S d="59666667" t="85581589049673"/>
                    <S d="48000000" r="9" t="85581648716340"/>
                    <S d="61333333" t="85582128716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589049673" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59666667" t="85581589049673"/>
                        <S d="48000000" r="9" t="85581648716340"/>
                        <S d="61333333" t="85582128716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                <SegmentTimeline>
                    <S d="59840000" t="85581589081950"/>
                    <S d="48000000" r="9" t="85581648921950"/>
                    <S d="61440000" t="85582128921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59840000" t="85581589081950"/>
                        <S d="48000000" r="9" t="85581648921950"/>
                        <S d="61440000" t="85582128921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                <SegmentTimeline>
                    <S d="59733333" t="85581589081950"/>
                    <S d="48000000" r="9" t="85581648815283"/>
                    <S d="61440000" t="85582128815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85581589081950" startNumber="1790165" timescale="10000000">
                    <SegmentTimeline>
                        <S d="59733333" t="85581589081950"/>
                        <S d="48000000" r="9" t="85581648815283"/>
                        <S d="61440000" t="85582128815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21566" start="PT2377H16M59.0049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="12">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/VcubxQAmAiRDVUVJAAAADH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAClPQ6r</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="11">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/VcubxQAmAiRDVUVJAAAAC3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAAD3/FNn</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85582190049673" startNumber="1790177" timescale="10000000">
                <SegmentTimeline>
                    <S d="34666667" t="85582190049673"/>
                    <S d="48000000" r="135" t="85582224716340"/>
                    <S d="17000000" t="85588752716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85582190361950" startNumber="1790177" timescale="10000000">
                <SegmentTimeline>
                    <S d="34560000" t="85582190361950"/>
                    <S d="48000000" r="135" t="85582224921950"/>
                    <S d="16960000" t="85588752921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85582190255283" startNumber="1790177" timescale="10000000">
                <SegmentTimeline>
                    <S d="34560000" t="85582190255283"/>
                    <S d="48000000" r="135" t="85582224815283"/>
                    <S d="17066667" t="85588752815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21567" start="PT2377H27M56.971634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="14">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/WVMwLQArAilDVUVJAAAADn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAAQQPV4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="13">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/WVMwLQArAilDVUVJAAAADX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAALGeNCY=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                <SegmentTimeline>
                    <S d="31000000" t="85588769716340"/>
                    <S d="48000000" r="10" t="85588800716340"/>
                    <S d="42000000" t="85589328716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769716340" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31000000" t="85588769716340"/>
                        <S d="48000000" r="10" t="85588800716340"/>
                        <S d="42000000" t="85589328716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                <SegmentTimeline>
                    <S d="31040000" t="85588769881950"/>
                    <S d="48000000" r="10" t="85588800921950"/>
                    <S d="41920000" t="85589328921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="31040000" t="85588769881950"/>
                        <S d="48000000" r="10" t="85588800921950"/>
                        <S d="41920000" t="85589328921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                <SegmentTimeline>
                    <S d="30933333" t="85588769881950"/>
                    <S d="48000000" r="10" t="85588800815283"/>
                    <S d="42026667" t="85589328815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85588769881950" startNumber="1790315" timescale="10000000">
                    <SegmentTimeline>
                        <S d="30933333" t="85588769881950"/>
                        <S d="48000000" r="10" t="85588800815283"/>
                        <S d="42026667" t="85589328815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21568" start="PT2377H28M57.0716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="14">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/WaW5FQAmAiRDVUVJAAAADn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAEO5XX</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="13">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/WaW5FQAmAiRDVUVJAAAADX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACEws9w</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85589370716340" startNumber="1790328" timescale="10000000">
                <SegmentTimeline>
                    <S d="54000000" t="85589370716340"/>
                    <S d="48000000" r="134" t="85589424716340"/>
                    <S d="46000000" t="85595904716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85589370841950" startNumber="1790328" timescale="10000000">
                <SegmentTimeline>
                    <S d="54080000" t="85589370841950"/>
                    <S d="48000000" r="134" t="85589424921950"/>
                    <S d="46080000" t="85595904921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85589370841950" startNumber="1790328" timescale="10000000">
                <SegmentTimeline>
                    <S d="53973333" t="85589370841950"/>
                    <S d="48000000" r="134" t="85589424815283"/>
                    <S d="46080000" t="85595904815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21569" start="PT2377H39M55.071634S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="16">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/XS1ZNQArAilDVUVJAAAAEH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAABSpxw4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="15">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/XS1ZNQArAilDVUVJAAAAD3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAImCU/8=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                <SegmentTimeline>
                    <S d="50000000" t="85595950716340"/>
                    <S d="48000000" r="10" t="85596000716340"/>
                    <S d="23000000" t="85596528716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950716340" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="50000000" t="85595950716340"/>
                        <S d="48000000" r="10" t="85596000716340"/>
                        <S d="23000000" t="85596528716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595951001950" startNumber="1790465" timescale="10000000">
                <SegmentTimeline>
                    <S d="49920000" t="85595951001950"/>
                    <S d="48000000" r="10" t="85596000921950"/>
                    <S d="23040000" t="85596528921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595951001950" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="49920000" t="85595951001950"/>
                        <S d="48000000" r="10" t="85596000921950"/>
                        <S d="23040000" t="85596528921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950895283" startNumber="1790465" timescale="10000000">
                <SegmentTimeline>
                    <S d="49920000" t="85595950895283"/>
                    <S d="48000000" r="10" t="85596000815283"/>
                    <S d="23040000" t="85596528815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85595950895283" startNumber="1790465" timescale="10000000">
                    <SegmentTimeline>
                        <S d="49920000" t="85595950895283"/>
                        <S d="48000000" r="10" t="85596000815283"/>
                        <S d="23040000" t="85596528815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21570" start="PT2377H40M55.1716340S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="16">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/XX/iHQAmAiRDVUVJAAAAEH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAADMyPqb</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="15">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/XX/iHQAmAiRDVUVJAAAAD3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAAB7G49D</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85596551716340" startNumber="1790478" timescale="10000000">
                <SegmentTimeline>
                    <S d="25000000" t="85596551716340"/>
                    <S d="48000000" r="135" t="85596576716340"/>
                    <S d="24666666" t="85603104716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85596551961950" startNumber="1790478" timescale="10000000">
                <SegmentTimeline>
                    <S d="24960000" t="85596551961950"/>
                    <S d="48000000" r="135" t="85596576921950"/>
                    <S d="24640000" t="85603104921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85596551855283" startNumber="1790478" timescale="10000000">
                <SegmentTimeline>
                    <S d="24960000" t="85596551855283"/>
                    <S d="48000000" r="135" t="85596576815283"/>
                    <S d="24746667" t="85603104815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21571" start="PT2377H51M52.9383006S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="18">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/YQcwNQArAilDVUVJAAAAEn//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAANaHnLM=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="17">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/YQcwNQArAilDVUVJAAAAEX//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAGMJlcs=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                <SegmentTimeline>
                    <S d="23333334" t="85603129383006"/>
                    <S d="48000000" r="10" t="85603152716340"/>
                    <S d="49333333" t="85603680716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129383006" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23333334" t="85603129383006"/>
                        <S d="48000000" r="10" t="85603152716340"/>
                        <S d="49333333" t="85603680716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                <SegmentTimeline>
                    <S d="23360000" t="85603129561950"/>
                    <S d="48000000" r="10" t="85603152921950"/>
                    <S d="49280000" t="85603680921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23360000" t="85603129561950"/>
                        <S d="48000000" r="10" t="85603152921950"/>
                        <S d="49280000" t="85603680921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                <SegmentTimeline>
                    <S d="23253333" t="85603129561950"/>
                    <S d="48000000" r="10" t="85603152815283"/>
                    <S d="49280000" t="85603680815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603129561950" startNumber="1790616" timescale="10000000">
                    <SegmentTimeline>
                        <S d="23253333" t="85603129561950"/>
                        <S d="48000000" r="10" t="85603152815283"/>
                        <S d="49280000" t="85603680815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21572" start="PT2377H52M53.0049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="18">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/YVmtZQAmAiRDVUVJAAAAEn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAsAzeZ</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="17">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/YVmtZQAmAiRDVUVJAAAAEX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACs+m0+</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603730049673" startNumber="1790629" timescale="10000000">
                <SegmentTimeline>
                    <S d="46666667" t="85603730049673"/>
                    <S d="48000000" r="131" t="85603776716340"/>
                    <S d="35333333" t="85610112716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603730201950" startNumber="1790629" timescale="10000000">
                <SegmentTimeline>
                    <S d="46720000" t="85603730201950"/>
                    <S d="48000000" r="131" t="85603776921950"/>
                    <S d="35200000" t="85610112921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85603730095283" startNumber="1790629" timescale="10000000">
                <SegmentTimeline>
                    <S d="46720000" t="85603730095283"/>
                    <S d="48000000" r="131" t="85603776815283"/>
                    <S d="35413333" t="85610112815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21573" start="PT2378H3M34.8049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="20">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAFH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAEOV0w4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="19">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAE3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAJTX7KQ=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60666667" t="85610148049673"/>
                    <S d="48000000" r="9" t="85610208716340"/>
                    <S d="60333333" t="85610688716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60800000" t="85610148121950"/>
                    <S d="48000000" r="9" t="85610208921950"/>
                    <S d="60160000" t="85610688921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60800000" t="85610148121950"/>
                        <S d="48000000" r="9" t="85610208921950"/>
                        <S d="60160000" t="85610688921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60586667" t="85610148228616"/>
                    <S d="48000000" r="9" t="85610208815283"/>
                    <S d="60373333" t="85610688815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60586667" t="85610148228616"/>
                        <S d="48000000" r="9" t="85610208815283"/>
                        <S d="60373333" t="85610688815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21574" start="PT2378H4M34.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="20">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAFH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAD/c8cV</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="19">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAE3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACtsprZ</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749049673" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35666667" t="85610749049673"/>
                    <S d="48000000" r="180" t="85610784716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749081950" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35840000" t="85610749081950"/>
                    <S d="48000000" r="180" t="85610784921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">c
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749188616" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35626667" t="85610749188616"/>
                    <S d="48000000" r="180" t="85610784815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <UTCTiming schemeIdUri="urn:mpeg:dash:utc:http-iso:2014" value="https://time.akamai.com/?iso&amp;ms"/>
</MPD>
  )";

    // perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto periods = root->getPeriods();
    long long mpdDuration = 0;
    for (auto period : periods)
    {
        auto adaptationSet = period->getAdaptationSets().at(0);
        auto segTemplate = adaptationSet->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long curTime = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    curTime += dur;
                    auto rep = stoll(S.attribute("r", "0"));
                    if (rep > 0) {
                        for (int i = 0; i < rep; i++) {
                            curTime += dur;
                        }
                    }
                    S = S.nextSiblingElement("S");
                }
                mpdDuration += curTime/timeScale;
            }
        }
    }
    EXPECT_EQ(mpdDuration, 7231);
}
/**
 * @brief ManifestStitching_Period_Append test.
 *
 * To verify two manifest file stitching with SegmentTimeline range and updated MPD contains sum of 
 * both MPD files with additionally added segments with new period
 */
TEST_F(FunctionalTests, ManifestStitching_Period_Append)
{
  AAMPStatusType status;
	static const char *manifest1 =
  R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD availabilityStartTime="2023-02-07T07:50:25Z" minBufferTime="PT9.6S" minimumUpdatePeriod="PT4.8S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2023-05-17T10:08:41.349Z" timeShiftBufferDepth="PT2H" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:dolby="http://www.dolby.com/ns/online/DASH" xmlns:mspr="urn:microsoft:playready" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
    <BaseURL>https://cfrt.stream.exampletv.com/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/</BaseURL>
    <Location>https://dc0c37fd4d014d2a92bb602c3ede9c88.mediatailor.us-east-1.amazonaws.com/v1/dash/6f3f45fea6332a47667932dede90d20a96f2690c/example-cmaf-dash-linear-4s-021821/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/master_2hr.mpd?c3.ri=3779723265365281424&amp;aws.sessionId=cc7de321-1e82-42a5-9a55-8b4f4c5a6f46</Location>
    <Period id="21556" start="PT2376H11M47.8716340S">
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078716340" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088716340"/>
                    <S d="27333333" t="85550640716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543079001950" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088921950"/>
                    <S d="27200000" t="85550640921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078788616" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088815283"/>
                    <S d="27306667" t="85550640815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21557" start="PT2376H24M26.8049673S">
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20666667" t="85550668049673"/>
                    <S d="48000000" r="10" t="85550688716340"/>
                    <S d="52333333" t="85551216716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20800000" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688921950"/>
                    <S d="52160000" t="85551216921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20800000" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688921950"/>
                        <S d="52160000" t="85551216921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20693333" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688815283"/>
                    <S d="52266667" t="85551216815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20693333" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688815283"/>
                        <S d="52266667" t="85551216815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <UTCTiming schemeIdUri="urn:mpeg:dash:utc:http-iso:2014" value="https://time.akamai.com/?iso&amp;ms"/>
  </MPD>  
  )";
	static const char *manifest2 =
  R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD availabilityStartTime="2023-02-07T07:50:25Z" minBufferTime="PT9.6S" minimumUpdatePeriod="PT4.8S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2023-05-17T10:08:41.349Z" timeShiftBufferDepth="PT2H" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:dolby="http://www.dolby.com/ns/online/DASH" xmlns:mspr="urn:microsoft:playready" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
    <BaseURL>https://cfrt.stream.exampletv.com/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/</BaseURL>
    <Location>https://dc0c37fd4d014d2a92bb602c3ede9c88.mediatailor.us-east-1.amazonaws.com/v1/dash/6f3f45fea6332a47667932dede90d20a96f2690c/example-cmaf-dash-linear-4s-021821/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/master_2hr.mpd?c3.ri=3779723265365281424&amp;aws.sessionId=cc7de321-1e82-42a5-9a55-8b4f4c5a6f46</Location>
    <Period id="21556" start="PT2376H11M47.8716340S">
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078716340" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088716340"/>
                    <S d="27333333" t="85550640716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543079001950" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088921950"/>
                    <S d="27200000" t="85550640921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85543078788616" startNumber="1789442" timescale="10000000">
                <SegmentTimeline>
                    <S d="48000000" r="73" t="85547088815283"/>
                    <S d="27306667" t="85550640815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21557" start="PT2376H24M26.8049673S">
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20666667" t="85550668049673"/>
                    <S d="48000000" r="10" t="85550688716340"/>
                    <S d="52333333" t="85551216716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668049673" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20666667" t="85550668049673"/>
                        <S d="48000000" r="10" t="85550688716340"/>
                        <S d="52333333" t="85551216716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20800000" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688921950"/>
                    <S d="52160000" t="85551216921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20800000" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688921950"/>
                        <S d="52160000" t="85551216921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                <SegmentTimeline>
                    <S d="20693333" t="85550668121950"/>
                    <S d="48000000" r="10" t="85550688815283"/>
                    <S d="52266667" t="85551216815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85550668121950" startNumber="1789517" timescale="10000000">
                    <SegmentTimeline>
                        <S d="20693333" t="85550668121950"/>
                        <S d="48000000" r="10" t="85550688815283"/>
                        <S d="52266667" t="85551216815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21558" start="PT2376H25M26.9049673S">
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269049673" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43666667" t="85551269049673"/>
                    <S d="48000000" r="137" t="85551312716340"/>
                    <S d="41666666" t="85557936716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269081950" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43840000" t="85551269081950"/>
                    <S d="48000000" r="137" t="85551312921950"/>
                    <S d="41600000" t="85557936921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85551269081950" startNumber="1789530" timescale="10000000">
                <SegmentTimeline>
                    <S d="43733333" t="85551269081950"/>
                    <S d="48000000" r="137" t="85551312815283"/>
                    <S d="41600000" t="85557936815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    </MPD>
  )";
    // perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto periods = root->getPeriods();
    long long mpdDuration = 0;
    for (auto period : periods)
    {
        auto adaptationSet = period->getAdaptationSets().at(0);
        auto segTemplate = adaptationSet->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long curTime = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    curTime += dur;
                    auto rep = stoll(S.attribute("r", "0"));
                    if (rep > 0) {
                        for (int i = 0; i < rep; i++) {
                            curTime += dur;
                        }
                    }
                    S = S.nextSiblingElement("S");
                }
                mpdDuration += curTime/timeScale;
            }
        }
    }
    EXPECT_EQ(mpdDuration, 1087);
}
/**
 * @brief ManifestStitching_NO_MPD_CHANGE test.
 *
 * The MPD duration not changed when there is no change in the timeline of two MPD files 
 * final MPD file duration should match with 1st MPD file duration
 */
TEST_F(FunctionalTests, ManifestStitching_No_MPD_Change)
{
    static const char *manifest1 =
  R"(<?xml version="1.0" encoding="utf-8"?>
  <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
    <Period id="1234">
      <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
        <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
          <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
            <SegmentTimeline>
              <S t="0" d="4" r="3599" />
            </SegmentTimeline>
          </SegmentTemplate>
        </Representation>
      </AdaptationSet>
    </Period>
  </MPD>  
  )";
	static const char *manifest2 =
  R"(<?xml version="1.0" encoding="utf-8"?>
  <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
    <Period id="1234">
      <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
        <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
          <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
            <SegmentTimeline>
              <S t="0" d="4" r="3599" />
            </SegmentTimeline>
          </SegmentTemplate>
        </Representation>
      </AdaptationSet>
    </Period>
  </MPD>
  )";

    // perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto period = root->getPeriods().at(0);
    auto adaptationSet = period->getAdaptationSets().at(0);
    auto representations = adaptationSet->getRepresentations();
    if (representations.size() > 0)
    {
        auto segTemplate = representations.at(0)->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long curTime = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    auto rep = stoll(S.attribute("r", "0"));
                    curTime += dur;
                    if (rep > 0) {
                        for (int i = 0; i < rep; i++) {
                            curTime += dur;
                        }
                    }
                    S = S.nextSiblingElement("S");
                }
                long duration = curTime/timeScale;
                EXPECT_EQ(duration, 3600);
                EXPECT_EQ(startTime, 0);
            }
        }
    }
}
/**
 * @brief ManifestStitching_NO_TIMELINE_MATCH test.
 *
 * The $SegmentTimeLine$ range id not matched b/w two MPD file then new MPD timeline should not 
 *  added with final MPD and duration is always matched with 1st MPD.
 */
TEST_F(FunctionalTests, ManifestStitching_No_Timeline_Match)
{
    static const char *manifest1 =
    R"(<?xml version="1.0" encoding="utf-8"?>
    <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
        <Period id="1234">
            <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                    <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                    <SegmentTimeline>
                        <S t="0" d="4" r="3599" />
                    </SegmentTimeline>
                    </SegmentTemplate>
                </Representation>
            </AdaptationSet>
        </Period>
    </MPD>
  )";
	static const char *manifest2 =
    R"(<?xml version="1.0" encoding="utf-8"?>
    <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
        <Period id="1234">
            <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                    <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                    <SegmentTimeline>
                        <S t="14401" d="4" r="3599" />
                    </SegmentTimeline>
                    </SegmentTemplate>
                </Representation>
            </AdaptationSet>
        </Period>
    </MPD>
  )";

	// perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto period = root->getPeriods().at(0);
    auto adaptationSet = period->getAdaptationSets().at(0);
    auto representations = adaptationSet->getRepresentations();
    if (representations.size() > 0)
    {
        auto segTemplate = representations.at(0)->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long curTime = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    auto rep = stoll(S.attribute("r", "0"));
                    if (rep > 0) {
                        for (int i = 0; i <= rep; i++) {
                            curTime += dur;
                        }
                    }
                    S = S.nextSiblingElement("S");
                }
                long duration = curTime/timeScale;
                EXPECT_EQ(duration, 3600);
                EXPECT_EQ(startTime, 0);
            }
        }
    }
}
/**
 * @brief ManifestStitching_TIMELINE_REPEATED test.
 *
 * The segment timeline repeated count alone added on the new MPD file and verify final steched MPD
 * duration is updated properly
 */
TEST_F(FunctionalTests, ManifestStitching_TimeLine_Repeat)
{
    static const char *manifest1 =
    R"(<?xml version="1.0" encoding="utf-8"?>
    <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
        <Period id="1234">
            <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                    <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                    <SegmentTimeline>
                        <S t="0" d="4" r="3599" />
                    </SegmentTimeline>
                    </SegmentTemplate>
                </Representation>
            </AdaptationSet>
        </Period>
    </MPD>
    )";
	static const char *manifest2 =
    R"(<?xml version="1.0" encoding="utf-8"?>
    <MPD xmlns="urn:mpeg:dash:schema:mpd:2011" minimumUpdatePeriod="PT0H0M1.920S" availabilityStartTime="2023-05-09T18:00:00.000Z" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011,http://dashif.org/guidelines/dash264">
        <Period id="1234">
            <AdaptationSet id="1" maxWidth="1920" maxHeight="1080" maxFrameRate="25" par="16:9">
                <Representation id="1" mimeType="video/mp4" codecs="avc1.640028" width="640" height="360" frameRate="25" sar="1:1" bandwidth="1000000">
                    <SegmentTemplate timescale="4" media="video_$Time$.mp4" initialization="video_init.mp4" presentationTimeOffset="0">
                    <SegmentTimeline>
                        <S t="0" d="4" r="3605" />
                    </SegmentTimeline>
                    </SegmentTemplate>
                </Representation>
            </AdaptationSet>
        </Period>
    </MPD>
  )";

	// perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto period = root->getPeriods().at(0);
    auto adaptationSet = period->getAdaptationSets().at(0);
    auto representations = adaptationSet->getRepresentations();
    if (representations.size() > 0)
    {
        auto segTemplate = representations.at(0)->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long curTime = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    auto rep = stoll(S.attribute("r", "0"));
                    if (rep > 0) {
                        for (int i = 0; i <= rep; i++) {
                            curTime += dur;
                        }
                    }
                    S = S.nextSiblingElement("S");
                }
                long duration = curTime/timeScale;
                EXPECT_EQ(duration, 3606);
                EXPECT_EQ(startTime, 0);
            }
        }
    }
}
///
TEST_F(FunctionalTests, ManifestStitching_SpecificApp_EventStream)
{
	AAMPStatusType status;
	static const char *manifest1 =
R"(<?xml version="1.0" encoding="UTF-8"?>
  <MPD availabilityStartTime="2023-02-07T07:50:25Z" minBufferTime="PT9.6S" minimumUpdatePeriod="PT4.8S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2023-05-17T10:08:41.349Z" timeShiftBufferDepth="PT2H" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:dolby="http://www.dolby.com/ns/online/DASH" xmlns:mspr="urn:microsoft:playready" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
    <BaseURL>https://cfrt.stream.exampletv.com/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/</BaseURL>
    <Location>https://dc0c37fd4d014d2a92bb602c3ede9c88.mediatailor.us-east-1.amazonaws.com/v1/dash/6f3f45fea6332a47667932dede90d20a96f2690c/example-cmaf-dash-linear-4s-021821/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/master_2hr.mpd?c3.ri=3779723265365281424&amp;aws.sessionId=cc7de321-1e82-42a5-9a55-8b4f4c5a6f46</Location>
    <Period id="21573" start="PT2378H3M34.8049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="10">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAFH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAEOV0w4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="11">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAE3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAJTX7KQ=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60666667" t="85610148049673"/>
                    <S d="48000000" r="9" t="85610208716340"/>
                    <S d="60333333" t="85610688716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60800000" t="85610148121950"/>
                    <S d="48000000" r="9" t="85610208921950"/>
                    <S d="60160000" t="85610688921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60800000" t="85610148121950"/>
                        <S d="48000000" r="9" t="85610208921950"/>
                        <S d="60160000" t="85610688921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60586667" t="85610148228616"/>
                    <S d="48000000" r="9" t="85610208815283"/>
                    <S d="60373333" t="85610688815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60586667" t="85610148228616"/>
                        <S d="48000000" r="9" t="85610208815283"/>
                        <S d="60373333" t="85610688815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21574" start="PT2378H4M34.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="300">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAFH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAD/c8cV</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="301">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAE3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACtsprZ</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749049673" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35666667" t="85610749049673"/>
                    <S d="48000000" r="172" t="85610784716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749081950" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35840000" t="85610749081950"/>
                    <S d="48000000" r="172" t="85610784921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749188616" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35626667" t="85610749188616"/>
                    <S d="48000000" r="172" t="85610784815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <UTCTiming schemeIdUri="urn:mpeg:dash:utc:http-iso:2014" value="https://time.akamai.com/?iso&amp;ms"/>
  </MPD>
)";

	static const char *manifest2 =
  R"(<?xml version="1.0" encoding="UTF-8"?>
    <MPD availabilityStartTime="2023-02-07T07:50:25Z" minBufferTime="PT9.6S" minimumUpdatePeriod="PT4.8S" profiles="urn:mpeg:dash:profile:isoff-live:2011" publishTime="2023-05-17T10:09:19.749Z" timeShiftBufferDepth="PT2H" type="dynamic" xmlns="urn:mpeg:dash:schema:mpd:2011" xmlns:cenc="urn:mpeg:cenc:2013" xmlns:dolby="http://www.dolby.com/ns/online/DASH" xmlns:mspr="urn:microsoft:playready" xmlns:scte35="urn:scte:scte35:2014:xml+bin" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd">
    <BaseURL>https://cfrt.stream.exampletv.com/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/</BaseURL>
    <Location>https://dc0c37fd4d014d2a92bb602c3ede9c88.mediatailor.us-east-1.amazonaws.com/v1/dash/6f3f45fea6332a47667932dede90d20a96f2690c/example-cmaf-dash-linear-4s-021821/Content/CMAF_OL2-CTR-4s-v2/Live/channel(lc127fxuqc)/master_2hr.mpd?c3.ri=3779723265365281424&amp;aws.sessionId=cc7de321-1e82-42a5-9a55-8b4f4c5a6f46</Location>
    <Period id="21573" start="PT2378H3M34.8049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event duration="601000000" id="20">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAFH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAEOV0w4=</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="19">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAE3//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhciIAAJTX7KQ=</scte35:Binary>
                </scte35:Signal>
            </Event>
                        <Event id="18">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/YVmtZQAmAiRDVUVJAAAAEn+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAAsAzeZ</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="17">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/YVmtZQAmAiRDVUVJAAAAEX+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACs+m0+</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60666667" t="85610148049673"/>
                    <S d="48000000" r="9" t="85610208716340"/>
                    <S d="60333333" t="85610688716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920">
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148049673" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60666667" t="85610148049673"/>
                        <S d="48000000" r="9" t="85610208716340"/>
                        <S d="60333333" t="85610688716340"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60800000" t="85610148121950"/>
                    <S d="48000000" r="9" t="85610208921950"/>
                    <S d="60160000" t="85610688921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148121950" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60800000" t="85610148121950"/>
                        <S d="48000000" r="9" t="85610208921950"/>
                        <S d="60160000" t="85610688921950"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                <SegmentTimeline>
                    <S d="60586667" t="85610148228616"/>
                    <S d="48000000" r="9" t="85610208815283"/>
                    <S d="60373333" t="85610688815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
                <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610148228616" startNumber="1790763" timescale="10000000">
                    <SegmentTimeline>
                        <S d="60586667" t="85610148228616"/>
                        <S d="48000000" r="9" t="85610208815283"/>
                        <S d="60373333" t="85610688815283"/>
                    </SegmentTimeline>
                </SegmentTemplate>
            </Representation>
        </AdaptationSet>
    </Period>
    <Period id="21574" start="PT2378H4M34.9049673S">
        <EventStream schemeIdUri="urn:scte:scte35:2014:xml+bin" timescale="10000000">
            <Event id="1">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAFH+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIxAAD/c8cV</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event id="2">
                <scte35:Signal>
                    <scte35:Binary>/DA8AAAAAAAAAADABQb/ZR2XHQAmAiRDVUVJAAAAE3+/DhV0ZWxlbXVuZG9hbGRpYV9saW5lYXIjAACtsprZ</scte35:Binary>
                </scte35:Signal>
            </Event>
            <Event duration="601000000" id="3">
                <scte35:Signal>
                    <scte35:Binary>/DBBAAAAAAAAAADABQb/ZMsONQArAilDVUVJAAAAFH//AABSiOgOFXRlbGVtdW5kb2FsZGlhX2xpbmVhcjAAAEOV0w4=</scte35:Binary>
                </scte35:Signal>
            </Event>
        </EventStream>
        <AdaptationSet mimeType="video/mp4" par="16:9" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <InbandEventStream schemeIdUri="urn:scte:scte35:2013:xml" value="1"/>
            <Accessibility schemeIdUri="urn:scte:dash:cc:cea-608:2015" value="CC1=spa"/>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749049673" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35666667" t="85610749049673"/>
                    <S d="48000000" r="180" t="85610784716340"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation bandwidth="350000" codecs="avc1.4d4015" frameRate="30" height="288" id="1675756212468item-01item" sar="1:1" scanType="progressive" width="512"/>
            <Representation bandwidth="860000" codecs="avc1.4d401e" frameRate="30" height="432" id="1675756212468item-02item" sar="1:1" scanType="progressive" width="768"/>
            <Representation bandwidth="1850000" codecs="avc1.4d401f" frameRate="30" height="540" id="1675756212468item-03item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="3000000" codecs="avc1.64001f" frameRate="30" height="540" id="1675756212468item-04item" sar="1:1" scanType="progressive" width="960"/>
            <Representation bandwidth="4830000" codecs="avc1.64001f" frameRate="30" height="720" id="1675756212468item-05item" sar="1:1" scanType="progressive" width="1280"/>
            <Representation bandwidth="7830000" codecs="avc1.640028" frameRate="30" height="1080" id="1675756212468item-06item" sar="1:1" scanType="progressive" width="1920"/>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749081950" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35840000" t="85610749081950"/>
                    <S d="48000000" r="180" t="85610784921950"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="192000" codecs="ec-3" id="1675756212468item-07item">
                <AudioChannelConfiguration schemeIdUri="tag:dolby.com,2014:dash:audio_channel_configuration:2011" value="F801"/>
            </Representation>
        </AdaptationSet>
        <AdaptationSet lang="spa" mimeType="audio/mp4" segmentAlignment="true" startWithSAP="1">
            <ContentProtection cenc:default_KID="00450e26-1a13-a414-b4aa-0a70bbf4b3a8" schemeIdUri="urn:mpeg:dash:mp4protection:2011" value="cenc"/>
            <ContentProtection schemeIdUri="urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95">
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAADYnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAA0JCAwAAAQABADgDPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwATABBAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAEEAXwBVAFIATAA+ADwATABVAEkAXwBVAFIATAA+AGgAdAB0AHAAOgAvAC8AbABpAGMALgBwAGUAYQBjAG8AYwBrAHQAdgAuAGMAbwBtAC8AUABsAGEAeQBSAGUAYQBkAHkATABpAGMAZQBuAHMAZQByAC8AcgBpAGcAaAB0AHMAbQBhAG4AYQBnAGUAcgAuAGEAcwBtAHgAPAAvAEwAVQBJAF8AVQBSAEwAPgA8AEsASQBEAD4ASgBnADUARgBBAEIATQBhAEYASwBTADAAcQBnAHAAdwB1AC8AUwB6AHEAQQA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgBCAG0AKwBXAFoAcwArAGwAdABsAG8APQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA=</cenc:pssh>
                <mspr:IsEncrypted xmlns:mspr="urn:microsoft:playready">1</mspr:IsEncrypted>
                <mspr:IV_Size xmlns:mspr="urn:microsoft:playready">8</mspr:IV_Size>
                <mspr:kid xmlns:mspr="urn:microsoft:playready">Jg5FABMaFKS0qgpwu/SzqA==</mspr:kid>
                <mspr:pro xmlns:mspr="urn:microsoft:playready">QgMAAAEAAQA4AzwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEwAQQBfAFUAUgBMAD4AaAB0AHQAcAA6AC8ALwBsAGkAYwAuAHAAZQBhAGMAbwBjAGsAdAB2AC4AYwBvAG0ALwBQAGwAYQB5AFIAZQBhAGQAeQBMAGkAYwBlAG4AcwBlAHIALwByAGkAZwBoAHQAcwBtAGEAbgBhAGcAZQByAC4AYQBzAG0AeAA8AC8ATABBAF8AVQBSAEwAPgA8AEwAVQBJAF8AVQBSAEwAPgBoAHQAdABwADoALwAvAGwAaQBjAC4AcABlAGEAYwBvAGMAawB0AHYALgBjAG8AbQAvAFAAbABhAHkAUgBlAGEAZAB5AEwAaQBjAGUAbgBzAGUAcgAvAHIAaQBnAGgAdABzAG0AYQBuAGEAZwBlAHIALgBhAHMAbQB4ADwALwBMAFUASQBfAFUAUgBMAD4APABLAEkARAA+AEoAZwA1AEYAQQBCAE0AYQBGAEsAUwAwAHEAZwBwAHcAdQAvAFMAegBxAEEAPQA9ADwALwBLAEkARAA+ADwAQwBIAEUAQwBLAFMAVQBNAD4AQgBtACsAVwBaAHMAKwBsAHQAbABvAD0APAAvAEMASABFAEMASwBTAFUATQA+ADwALwBEAEEAVABBAD4APAAvAFcAUgBNAEgARQBBAEQARQBSAD4A</mspr:pro>
            </ContentProtection>
            <ContentProtection schemeIdUri="urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed">c
                <cenc:pssh xmlns:cenc="urn:mpeg:cenc:2013">AAAAOHBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAABgSEABFDiYaE6QUtKoKcLv0s6hI49yVmwY=</cenc:pssh>
            </ContentProtection>
            <SegmentTemplate initialization="$RepresentationID$_init.m4i" media="$RepresentationID$_Segment-$Number$.mp4" presentationTimeOffset="85610749188616" startNumber="1790775" timescale="10000000">
                <SegmentTimeline>
                    <S d="35626667" t="85610749188616"/>
                    <S d="48000000" r="180" t="85610784815283"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation audioSamplingRate="48000" bandwidth="96000" codecs="mp4a.40.2" id="1675756212468item-08item">
                <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
            </Representation>
        </AdaptationSet>
    </Period>
    <UTCTiming schemeIdUri="urn:mpeg:dash:utc:http-iso:2014" value="https://time.akamai.com/?iso&amp;ms"/>
</MPD>
  )";

    // perform MPD merge and get mpdDocument
	ProcessMPDMerge (manifest1, manifest2);
    auto root = mpdDocument->getRoot();
    auto periods = root->getPeriods();
    long long mpdDuration = 0;
    int period_idx = 0;
    for (auto period : periods)
    {
        auto adaptationSet = period->getAdaptationSets().at(0);
        auto segTemplate = adaptationSet->getSegmentTemplate();
        if (segTemplate)
        {
            auto segTimeline = segTemplate->getSegmentTimeline();
            long timeScale = segTemplate->getTimeScale();
            if (segTimeline)
            {
                DomElement S = segTimeline->elem.firstChildElement("S");
                long long curTime = 0;
                long long startTime;
                // calcuate duration of 0th period, since manifest contains only one period entry
                while (!S.isNull())
                {
                    startTime = stoll(S.attribute("t", "0"));
                    auto dur = stoll(S.attribute("d", "0"));
                    curTime += dur;
                    auto rep = stoll(S.attribute("r", "0"));
                    if (rep > 0) {
                        for (int i = 0; i < rep; i++) {
                            curTime += dur;
                        }
                    }
                    S = S.nextSiblingElement("S");
                }
                mpdDuration += curTime/timeScale;
            }
            // Event Stream Verification
            vector<shared_ptr<DashMPDEvent>> scteEvents;
            auto eventStream = period->getEventStream();
            if (!(eventStream->isNull()))
            {
                auto events = eventStream->getEvents();
                if (0 == period_idx)
                {
                    EXPECT_EQ(events.size(), 4);
                }
                else if (1 == period_idx)
                {
                   EXPECT_EQ(events.size(), 3); 
                }    
            }
        }
        ++period_idx;
    }
    //dumpManifest(mpdDocument, "merged");  
    EXPECT_EQ(mpdDuration, 932);
}
