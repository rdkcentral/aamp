#!/usr/bin/env python3
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2023 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

import sys
import unittest
import xml.etree.ElementTree as ET

# Import server from the parent directory
sys.path.append('.')
sys.path.append('..')
from server import DASHManifest, ServerParams, StreamType

def Canonicalize(element):
    """Canonical representation of an XML element

    Use this to generate a string representation of an XML element which can be
    used in tests to compare two XML elements.

    return A standardized string representation of an XML element
    """
    return ET.canonicalize(ET.tostring(element), strip_text=True)

class TestGetDuration(unittest.TestCase):
    """Test cases for DASHManifest.getDuration().
    """

    def test_GetDuration_Test1(self):
        manifest = DASHManifest()
        self.assertEqual(manifest.getDuration(""), 0.0)
        self.assertEqual(manifest.getDuration("PT0S"), 0.0)
        self.assertEqual(manifest.getDuration("PT0.000S"), 0.0)
        self.assertEqual(manifest.getDuration("PT30.000S"), 30.0)
        self.assertEqual(manifest.getDuration("PT0M30.000S"), 30.0)
        self.assertEqual(manifest.getDuration("PT1M30.5S"), 90.5)

class TestRewriteSegmentTemplate(unittest.TestCase):
    """Test cases for DASHManifest.rewriteSegmentTemplate().
    """

    def test_SegmentTimeline_Video1(self):
        """SegmentTimeline testcase.

        One segment is available.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        currentPlayTime = 2.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Video2(self):
        """SegmentTimeline testcase.

        Three segments are available.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="2" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        currentPlayTime = 6.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Video3(self):
        """SegmentTimeline testcase.

        Zero segments are available from a two second period starting at 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="16" presentationTimeOffset="384000">
    <SegmentTimeline>
        <S t="384000" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="16" presentationTimeOffset="384000">
    <SegmentTimeline />
</SegmentTemplate>
""")
        periodStartTime = 30.0
        currentPlayTime = 30.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Video4(self):
        """SegmentTimeline testcase.

        One segment is available from a two second period starting at 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="16" presentationTimeOffset="384000">
    <SegmentTimeline>
        <S t="384000" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="16" presentationTimeOffset="384000">
    <SegmentTimeline>
        <S t="384000" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 30.0
        currentPlayTime = 32.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Video5(self):
        """SegmentTimeline testcase.

        One segment is available from a two second period starting at 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="16" presentationTimeOffset="384000">
    <SegmentTimeline>
        <S t="384000" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="16" presentationTimeOffset="384000">
    <SegmentTimeline>
        <S t="384000" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 30.0
        currentPlayTime = 34.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Video6(self):
        """SegmentTimeline testcase.

        The stream duration is limited to 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="14" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        currentPlayTime = None
        mediaPresentationDuration = 30.0
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Audio1(self):
        """SegmentTimeline testcase.

        One segment is available after two seconds.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="95232" />
        <S d="96256" r="446" />
        <S d="78336" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="95232" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        currentPlayTime = 2.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Audio2(self):
        """SegmentTimeline testcase.

        Two segments are available after four seconds.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="95232" />
        <S d="96256" r="446" />
        <S d="78336" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="95232" />
        <S d="96256" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        currentPlayTime = 4.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Audio3(self):
        """SegmentTimeline testcase.

        Three segments are available after six seconds.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="95232" />
        <S d="96256" r="446" />
        <S d="78336" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="95232" />
        <S d="96256" r="1" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        currentPlayTime = 6.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Audio4(self):
        """SegmentTimeline testcase.

        No segments are available at the start of a two seond period.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="15" presentationTimeOffset="1440000">
    <SegmentTimeline>
        <S t="1346560" d="96256" r="1" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="15" presentationTimeOffset="1440000">
    <SegmentTimeline />
</SegmentTemplate>
""")
        periodStartTime = 30.0
        currentPlayTime = 30.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Audio5(self):
        """SegmentTimeline testcase.

        One segment is available at the end of a two seond period.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="15" presentationTimeOffset="1440000">
    <SegmentTimeline>
        <S t="1346560" d="96256" r="1" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="15" presentationTimeOffset="1440000">
    <SegmentTimeline>
        <S t="1346560" d="96256" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 30.0
        currentPlayTime = 32.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentTimeline_Audio6(self):
        """SegmentTimeline testcase.

        Two segments are available after the end of a two seond period.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="15" presentationTimeOffset="1440000">
    <SegmentTimeline>
        <S t="1346560" d="96256" r="1" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" startNumber="15" presentationTimeOffset="1440000">
    <SegmentTimeline>
        <S t="1346560" d="96256" r="1" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 30.0
        currentPlayTime = 34.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentTemplate(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

class TestRewriteSegmentList(unittest.TestCase):
    """Test cases for DASHManifest.rewriteSegmentList().
    """

    def test_SegmentList_Video1(self):
        """SegmentList testcase.

        No segments are available at the beginning of playback.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
</SegmentList>
""")
        periodStartTime = 0.0
        currentPlayTime = 6.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentList_Video2(self):
        """SegmentList testcase.

        One segment is available after 10s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
</SegmentList>
""")
        periodStartTime = 0.0
        currentPlayTime = 10.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentList_Video3(self):
        """SegmentList testcase.

        Two segments are available after 20s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
</SegmentList>
""")
        periodStartTime = 0.0
        currentPlayTime = 20.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentList_Video4(self):
        """SegmentList testcase.

        Three segments are available after 20s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
    <SegmentURL media="T_00005-399872.m4s" />
    <SegmentURL media="T_00006-495104.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
</SegmentList>
""")
        periodStartTime = 0.0
        currentPlayTime = 30.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentList_Video5(self):
        """SegmentTimeline testcase.

        One segment is available in a period which starts after 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="4" presentationTimeOffset="30000000">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
    <SegmentURL media="T_00005-399872.m4s" />
    <SegmentURL media="T_00006-495104.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="4" presentationTimeOffset="30000000">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
</SegmentList>
""")
        periodStartTime = 30.0
        currentPlayTime = 32.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentList_Video6(self):
        """SegmentTimeline testcase.

        Two segments are available in a period which starts after 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="4" presentationTimeOffset="30000000">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
    <SegmentURL media="T_00005-399872.m4s" />
    <SegmentURL media="T_00006-495104.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="4" presentationTimeOffset="30000000">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
    <SegmentURL media="T_00005-399872.m4s" />
</SegmentList>
""")
        periodStartTime = 30.0
        currentPlayTime = 40.0
        mediaPresentationDuration = None
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_SegmentList_Video7(self):
        """SegmentList testcase.

        The stream duration is limited to 30s.

        self -- Test class instance
        """
        segmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
    <SegmentURL media="T_00005-399872.m4s" />
    <SegmentURL media="T_00006-495104.m4s" />
</SegmentList>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentList timescale="1000000" duration="7640000" startNumber="1">
    <Initialization sourceURL="init.m4s" />
    <SegmentURL media="T_00001-0.m4s" />
    <SegmentURL media="T_00002-97792.m4s" />
    <SegmentURL media="T_00003-201216.m4s" />
    <SegmentURL media="T_00004-303616.m4s" />
</SegmentList>
""")
        periodStartTime = 0.0
        currentPlayTime = None
        mediaPresentationDuration = 30.0
        DASHManifest().rewriteSegmentList(segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration)
        self.assertEqual(Canonicalize(segmentTemplate), Canonicalize(expectedSegmentTemplate))

class TestRewriteManifest(unittest.TestCase):
    """Test cases for DASHManifest.rewriteManifest().
    """

    def test_RewriteManifest_Live1(self):
        """DASH manifest live testcase.

        Segments available after 10s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
	  <Representation id="1080p" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			<S t="0" d="25600" r="449"/>
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
    <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="eng">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="English" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="95232" />
			  <S d="96256" r="446" />
			  <S d="78336" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S" timeShiftBufferDepth="PT30S">
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
	  <Representation id="1080p" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			<S t="0" d="25600" r="4" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
    <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="eng">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="English" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="95232" />
			  <S d="96256" r="2" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 10.0
        params = ServerParams()
        params.streamType = StreamType.LIVE
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Live2(self):
        """DASH manifest live testcase.

        One period is available after 30s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S" timeShiftBufferDepth="PT30S">
  <Period id="0" start="PT0.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 30.0
        params = ServerParams()
        params.streamType = StreamType.LIVE
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Live3(self):
        """DASH manifest live testcase.

        Two periods are available after 32s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S" timeShiftBufferDepth="PT30S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 32.0
        params = ServerParams()
        params.streamType = StreamType.LIVE
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Live4(self):
        """DASH manifest live testcase.

        The last two periods are available after 60s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S" timeShiftBufferDepth="PT30S">
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 60.0
        params = ServerParams()
        params.streamType = StreamType.LIVE
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Event1(self):
        """DASH manifest event testcase.

        One period is available after 30s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S">
  <Period id="0" start="PT0.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 30.0
        params = ServerParams()
        params.streamType = StreamType.EVENT
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Event2(self):
        """DASH manifest event testcase.

        Two periods are available after 30s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 32.0
        params = ServerParams()
        params.streamType = StreamType.EVENT
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Event3(self):
        """DASH manifest event testcase.

        All three periods are available after 60s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="dynamic" minBufferTime="PT4.0S" availabilityStartTime="2023-01-01T00:00:00Z" minimumUpdatePeriod="PT6S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 60.0
        params = ServerParams()
        params.streamType = StreamType.EVENT
        params.liveWindow = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Vod1(self):
        """DASH manifest VOD testcase.

        Limit the stream duration to 30s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
	  <Representation id="1080p" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			<S t="0" d="25600" r="449"/>
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
    <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="eng">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="English" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="95232" />
			  <S d="96256" r="446" />
			  <S d="78336" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT30.000S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
	  <Representation id="1080p" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			<S t="0" d="25600" r="14" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
    <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="eng">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="English" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="95232" />
			  <S d="96256" r="13" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 10.0
        params = ServerParams()
        params.streamType = StreamType.VOD
        params.maximumDuration = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Vod2(self):
        """DASH manifest VOD testcase.

        Limit the stream duration to 90s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
	  <Representation id="1080p" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			<S t="0" d="25600" r="449"/>
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
    <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="eng">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="English" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="95232" />
			  <S d="96256" r="446" />
			  <S d="78336" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT1M30.000S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
	<AdaptationSet id="0" contentType="video" segmentAlignment="true" bitstreamSwitching="true" lang="und">
	  <Representation id="1080p" mimeType="video/mp4" codecs="avc1.4d4028" bandwidth="5000000" width="1920" height="1080" frameRate="25/1">
		<SegmentTemplate timescale="12800" initialization="dash/1080p_init.m4s" media="dash/1080p_$Number%03d$.m4s" startNumber="1">
		  <SegmentTimeline>
			<S t="0" d="25600" r="44" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
    <AdaptationSet id="1" contentType="audio" segmentAlignment="true" bitstreamSwitching="true" lang="eng">
	  <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
	  <Representation id="English" mimeType="audio/mp4" codecs="mp4a.40.2" bandwidth="288000" audioSamplingRate="48000">
		<AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="1"/>
		<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
		  <SegmentTimeline>
			  <S t="0" d="95232" />
			  <S d="96256" r="43" />
		  </SegmentTimeline>
		</SegmentTemplate>
	  </Representation>
	</AdaptationSet>
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 10.0
        params = ServerParams()
        params.streamType = StreamType.VOD
        params.maximumDuration = 90.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Vod3(self):
        """DASH manifest VOD testcase.

        The duration is limited to 30s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT30.000S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 60.0
        params = ServerParams()
        params.streamType = StreamType.VOD
        params.maximumDuration = 30.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

    def test_RewriteManifest_Vod4(self):
        """DASH manifest VOD testcase.

        The duration is limited to 90s.

        self -- Test class instance
        """
        manifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT15M0.0S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        expectedManifest = ET.fromstring("""
<MPD type="static" mediaPresentationDuration="PT1M30.000S" minBufferTime="PT4.0S">
  <Period id="0" start="PT0.0S">
  </Period>
  <Period id="1" start="PT30.0S">
  </Period>
  <Period id="2" start="PT32.0S">
  </Period>
</MPD>
""")
        availabilityStartTime = 1672531200.0
        currentTime = availabilityStartTime + 60.0
        params = ServerParams()
        params.streamType = StreamType.VOD
        params.maximumDuration = 90.0
        instance = DASHManifest()
        instance.root = manifest
        instance.rewriteManifest(params, currentTime, availabilityStartTime)
        self.assertEqual(Canonicalize(instance.root), Canonicalize(expectedManifest))

if __name__ == '__main__':
    unittest.main()
