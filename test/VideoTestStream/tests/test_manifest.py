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

import unittest
import xml.etree.ElementTree as ET
import sys

# Import the manifest helper
sys.path.append("helper")
sys.path.append("../helper")

from manifest import DASHManifest

def Canonicalize(element):
    """Canonical representation of an XML element

    Use this to generate a string representation of an XML element which can be
    used in tests to compare two XML elements.

    element -- XML element
    return A standardized string representation of an XML element
    """
    return ET.canonicalize(ET.tostring(element), strip_text=True)

class TestMergeSegmentTemplate(unittest.TestCase):
    """SegmentTemplate merge tests.

    Perform tests with and without a SegmentTimeline.
    """
    def test_MergeSegmentTemplate_SegmentTimeline_Video1(self):
        """Merge six seconds of segments in a SegmentTimeline.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="2"/>
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        periodEndTime = 6.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Video2(self):
        """Merge two seconds of segments in a SegmentTimeline starting at six
        seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="4" presentationTimeOffset="76800">
    <SegmentTimeline>
        <S t="76800" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 6.0
        periodEndTime = 8.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Video3(self):
        """Merge segments in a SegmentTimeline starting at eight seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="12800" startNumber="5" presentationTimeOffset="102400">
    <SegmentTimeline>
        <S t="102400" d="25600" r="445" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 8.0
        periodEndTime = None
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Video4(self):
        """Merge four seconds of segments in a SegmentTimeline starting at three
        seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="12800" startNumber="1">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="12800" startNumber="2" presentationTimeOffset="38400">
    <SegmentTimeline>
        <S t="25600" d="25600" r="2" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 3.0
        periodEndTime = 7.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Video5(self):
        """Merge six seconds of segments in a SegmentTimeline.

        In this test, startNumber is zero.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="12800" startNumber="0">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="12800" startNumber="0">
    <SegmentTimeline>
        <S t="0" d="25600" r="2"/>
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        periodEndTime = 6.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Video6(self):
        """Merge two seconds of segments in a SegmentTimeline starting at six
        seconds.

        In this test, startNumber is zero.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="12800" startNumber="0">
    <SegmentTimeline>
        <S t="0" d="25600" r="449" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="12800" startNumber="3" presentationTimeOffset="76800">
    <SegmentTimeline>
        <S t="76800" d="25600" />
    </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 6.0
        periodEndTime = 8.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Audio1(self):
        """Merge six seconds of segments in a SegmentTimeline.

        In this test, segments are not always aligned to integer seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
  <SegmentTimeline>
    <S t="0" d="95232" />
    <S d="96256" r="446" />
    <S d="78336" />
  </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
  <SegmentTimeline>
    <S t="0" d="95232" />
    <S d="96256" r="2" />
  </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 0.0
        periodEndTime = 6.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Audio2(self):
        """Merge two seconds of segments in a SegmentTimeline starting at six
        seconds.

        In this test, segments are not always aligned to integer seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
  <SegmentTimeline>
    <S t="0" d="95232" />
    <S d="96256" r="446" />
    <S d="78336" />
  </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate xmlns="urn:mpeg:dash:schema:mpd:2011" timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="4" presentationTimeOffset="288000">
  <SegmentTimeline>
    <S t="287744" d="96256" />
  </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 6.0
        periodEndTime = 8.0
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_SegmentTimeline_Audio3(self):
        """Merge segments in a SegmentTimeline starting at eight seconds.

        In this test, segments are not always aligned to integer seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="1">
  <SegmentTimeline>
    <S t="0" d="95232" />
    <S d="96256" r="446" />
    <S d="78336" />
  </SegmentTimeline>
</SegmentTemplate>
""")
        expectedSegmentTemplate = ET.fromstring("""
<SegmentTemplate timescale="48000" initialization="dash/en_init.m4s" media="dash/en_$Number%03d$.mp3" startNumber="5" presentationTimeOffset="384000">
  <SegmentTimeline>
    <S t="384000" d="96256" r="443" />
    <S d="78336" />
  </SegmentTimeline>
</SegmentTemplate>
""")
        periodStartTime = 8.0
        periodEndTime = None
        targetSegmentTemplate = ET.Element(sourceSegmentTemplate.tag)
        manifest.mergeSegmentTemplate(targetSegmentTemplate, periodStartTime, periodEndTime, sourceSegmentTemplate, 0.0, None)
        self.assertEqual(Canonicalize(targetSegmentTemplate), Canonicalize(expectedSegmentTemplate))

    def test_MergeSegmentTemplate_NoSegmentTimeline_1(self):
        """Merge six seconds of segments in a SegmentTemplate with no
        SegmentTimeline.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentList = ET.fromstring("""
<SegmentTemplate timescale="1000000" duration="2000000" initialization="init.m4s" media="$Number%05d$.m4s" startNumber="1">
</SegmentTemplate>
""")
        expectedSegmentList = ET.fromstring("""
<SegmentTemplate timescale="1000000" duration="2000000" initialization="init.m4s" media="$Number%05d$.m4s" startNumber="1">
</SegmentTemplate>
""")
        periodStartTime = 0.0
        periodEndTime = 6.0
        targetSegmentList = ET.Element(sourceSegmentList.tag)
        manifest.mergeSegmentTemplate(targetSegmentList, periodStartTime, periodEndTime, sourceSegmentList, 0.0, None)
        self.maxDiff = None
        self.assertEqual(Canonicalize(targetSegmentList), Canonicalize(expectedSegmentList))

    def test_MergeSegmentTemplate_NoSegmentTimeline_2(self):
        """Merge ten seconds of segments in a SegmentTemplate with no
        SegmentTimeline starting at ten seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentList = ET.fromstring("""
<SegmentTemplate timescale="1000000" duration="2000000" initialization="init.m4s" media="$Number%05d$.m4s" startNumber="1">
</SegmentTemplate>
""")
        expectedSegmentList = ET.fromstring("""
<SegmentTemplate timescale="1000000" duration="2000000" initialization="init.m4s" media="$Number%05d$.m4s" startNumber="6" presentationTimeOffset="10000000">
</SegmentTemplate>
""")
        periodStartTime = 10.0
        periodEndTime = 20.0
        targetSegmentList = ET.Element(sourceSegmentList.tag)
        manifest.mergeSegmentTemplate(targetSegmentList, periodStartTime, periodEndTime, sourceSegmentList, 0.0, None)
        self.maxDiff = None
        self.assertEqual(Canonicalize(targetSegmentList), Canonicalize(expectedSegmentList))

class TestMergeSegmentList(unittest.TestCase):
    """SegmentList merge tests.
    """

    def test_MergeSegmentList_test1(self):
        """Merge six seconds of segments from a SegmentList.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentList = ET.fromstring("""
<SegmentList timescale="1000000" duration="10000000" startNumber="1">
    <Initialization range="0-807" />
    <SegmentURL mediaRange="808-1240414" indexRange="808-859" />
    <SegmentURL mediaRange="1240415-2447211" indexRange="1240415-1240466" />
    <SegmentURL mediaRange="2447212-3670805" indexRange="2447212-2447263" />
    <SegmentURL mediaRange="3670806-4892154" indexRange="3670806-3670857" />
    <SegmentURL mediaRange="4892155-6118292" indexRange="4892155-4892206" />
    <SegmentURL mediaRange="6118293-7347061" indexRange="6118293-6118344" />
    <SegmentURL mediaRange="7347062-8579932" indexRange="7347062-7347113" />
    <SegmentURL mediaRange="8579933-9794867" indexRange="8579933-8579984" />
    <SegmentURL mediaRange="9794868-11018265" indexRange="9794868-9794919" />
    <SegmentURL mediaRange="11018266-12238683" indexRange="11018266-11018317" />
    <SegmentURL mediaRange="12238684-13458278" indexRange="12238684-12238735" />
    <SegmentURL mediaRange="13458279-14678677" indexRange="13458279-13458330" />
</SegmentList>
""")
        expectedSegmentList = ET.fromstring("""
<SegmentList timescale="1000000" duration="10000000" startNumber="1">
    <Initialization range="0-807" />
    <SegmentURL mediaRange="808-1240414" indexRange="808-859" />
</SegmentList>
""")
        periodStartTime = 0.0
        periodEndTime = 6.0
        targetSegmentList = ET.Element(sourceSegmentList.tag)
        manifest.mergeSegmentList(targetSegmentList, periodStartTime, periodEndTime, sourceSegmentList, 0.0, None)
        self.maxDiff = None
        self.assertEqual(Canonicalize(targetSegmentList), Canonicalize(expectedSegmentList))

    def test_MergeSegmentList_test2(self):
        """Merge ten seconds of segments from a SegmentList starting at ten
        seconds.

        self -- Test instance
        """
        manifest = DASHManifest()
        sourceSegmentList = ET.fromstring("""
<SegmentList timescale="1000000" duration="10000000" startNumber="1">
    <Initialization range="0-807" />
    <SegmentURL mediaRange="808-1240414" indexRange="808-859" />
    <SegmentURL mediaRange="1240415-2447211" indexRange="1240415-1240466" />
    <SegmentURL mediaRange="2447212-3670805" indexRange="2447212-2447263" />
    <SegmentURL mediaRange="3670806-4892154" indexRange="3670806-3670857" />
    <SegmentURL mediaRange="4892155-6118292" indexRange="4892155-4892206" />
    <SegmentURL mediaRange="6118293-7347061" indexRange="6118293-6118344" />
    <SegmentURL mediaRange="7347062-8579932" indexRange="7347062-7347113" />
    <SegmentURL mediaRange="8579933-9794867" indexRange="8579933-8579984" />
    <SegmentURL mediaRange="9794868-11018265" indexRange="9794868-9794919" />
    <SegmentURL mediaRange="11018266-12238683" indexRange="11018266-11018317" />
    <SegmentURL mediaRange="12238684-13458278" indexRange="12238684-12238735" />
    <SegmentURL mediaRange="13458279-14678677" indexRange="13458279-13458330" />
</SegmentList>
""")
        expectedSegmentList = ET.fromstring("""
<SegmentList timescale="1000000" duration="10000000" startNumber="2" presentationTimeOffset="10000000">
    <Initialization range="0-807" />
    <SegmentURL mediaRange="1240415-2447211" indexRange="1240415-1240466" />
</SegmentList>
""")
        periodStartTime = 10.0
        periodEndTime = 20.0
        targetSegmentList = ET.Element(sourceSegmentList.tag)
        manifest.mergeSegmentList(targetSegmentList, periodStartTime, periodEndTime, sourceSegmentList, 0.0, None)
        self.maxDiff = None
        self.assertEqual(Canonicalize(targetSegmentList), Canonicalize(expectedSegmentList))

if __name__ == '__main__':
    unittest.main()
