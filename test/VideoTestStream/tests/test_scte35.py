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
import json
import base64
import io
import re

sys.path.append("helper")
sys.path.append("../helper")

from scte35 import encode, decode, mpd, CRC32

class TestEvents(unittest.TestCase):
    """Encode event test suite.
    """

    def test_event1(self):
        """Break Start test.
        """
        jsonString = '{"type":"Break Start", "time":30.0, "duration":34.0, "event_id":12345678}'
        binary = encode(jsonString)
        event = decode(binary)
        self.assertEqual(event["descriptors"][0]["segmentation_type_id"], 0x22)
        self.assertEqual(event["splice_command"]["pts_time"], 30*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_duration_flag"], True)
        self.assertEqual(event["descriptors"][0]["segmentation_duration"], 34*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_event_id"], 12345678)

    def test_event2(self):
        """Provider Advertisement Start test.
        """
        jsonString = '{"type":"Provider Advertisement Start", "time":32.0, "duration":30.0, "event_id":2}'
        binary = encode(jsonString)
        event = decode(binary)
        self.assertEqual(event["descriptors"][0]["segmentation_type_id"], 0x30)
        self.assertEqual(event["splice_command"]["pts_time"], 32*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_duration_flag"], True)
        self.assertEqual(event["descriptors"][0]["segmentation_duration"], 30*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_event_id"], 2)

    def test_event3(self):
        """Provider Advertisement End test.
        """
        jsonString = '{"type":"Provider Advertisement End", "time":62.0, "event_id":2}'
        binary = encode(jsonString)
        event = decode(binary)
        self.assertEqual(event["descriptors"][0]["segmentation_type_id"], 0x31)
        self.assertEqual(event["splice_command"]["pts_time"], 62*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_duration_flag"], False)
        self.assertEqual(event["descriptors"][0]["segmentation_event_id"], 2)

    def test_event4(self):
        """Provider Placement Opportunity Start test.
        """
        jsonString = '{"type":"Provider Placement Opportunity Start", "time":32.0, "duration":30.0, "event_id":3}'
        binary = encode(jsonString)
        event = decode(binary)
        self.assertEqual(event["descriptors"][0]["segmentation_type_id"], 0x34)
        self.assertEqual(event["splice_command"]["pts_time"], 32*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_duration_flag"], True)
        self.assertEqual(event["descriptors"][0]["segmentation_duration"], 30*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_event_id"], 3)

    def test_event5(self):
        """Provider Placement Opportunity End test.
        """
        jsonString = '{"type":"Provider Placement Opportunity End", "time":62.0, "event_id":3}'
        binary = encode(jsonString)
        event = decode(binary)
        self.assertEqual(event["descriptors"][0]["segmentation_type_id"], 0x35)
        self.assertEqual(event["splice_command"]["pts_time"], 62*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_duration_flag"], False)
        self.assertEqual(event["descriptors"][0]["segmentation_event_id"], 3)

    def test_event6(self):
        """Provider Break End test.
        """
        jsonString = '{"type":"Break End", "time":64.0, "event_id":12345678}'
        binary = encode(jsonString)
        event = decode(binary)
        self.assertEqual(event["descriptors"][0]["segmentation_type_id"], 0x23)
        self.assertEqual(event["splice_command"]["pts_time"], 64*90000)
        self.assertEqual(event["descriptors"][0]["segmentation_duration_flag"], False)
        self.assertEqual(event["descriptors"][0]["segmentation_event_id"], 12345678)

class TestDecode(unittest.TestCase):
    """Decode event test suite.
    """

    def setCRC32(self, section):
        """Set the MPEG CRC32 value in a test data section.

        self -- Test instance
        section -- Section data bytearray
        """
        sectionLength = len(section)
        crc = CRC32(section[:sectionLength - 4])
        section[sectionLength - 4] = (crc >> 24) & 0xff
        section[sectionLength - 3] = (crc >> 16) & 0xff
        section[sectionLength - 2] = (crc >> 8) & 0xff
        section[sectionLength - 1] = crc & 0xff

        # Verify
        crc = CRC32(section)
        self.assertEqual(crc, 0)

    def test_BreakStart(self):
        """Break start test.
        """
        section = bytearray([
            0xfc,   # "table_id":0xfc
            0x30,   # "section_syntax_indicator":0
                    # "private_indicator":0
                    # "sap_type":3
                    # "section_length":44
            0x2c,
            0x00,   # "protocol_version":0
            0x01,   # "encrypted_packet":0
                    # "encryption_algorithm":0
                    # "pts_adjustment":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "cw_index":0
            0xff,   # "tier":0xfff
            0xf0,   # "splice_command_length":5
            0x05,
            0x06,   # "splice_command_type":6 (time_signal)
            0xff,   # "time_specified_flag":1
                    # "reserved":0x3f
                    # "pts_time":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "descriptor_loop_length":22
            0x16,
            0x02,   # "splice_descriptor_tag":2 (segmentation_descriptor)
            0x14,   # "descriptor_length":20
            0x43,   # "identifier":'CUEI'
            0x55,
            0x45,
            0x49,
            0x01,   # "segmentation_event_id":0x01234567
            0x23,
            0x45,
            0x67,
            0x7f,   # "segmentation_event_cancel_indicator":0
                    # "reserved":0x7f
            0xc0,   # "program_segmentation_flag":1
                    # "segmentation_duration_flag":1
                    # "delivery_not_restricted_flag":0
                    # "web_delivery_allowed_flag":0
                    # "no_regional_blackout_flag":0
                    # "archive_allowed_flag": 0
                    # "device_restrictions":0
            0x00,   # "segmentation_duration":360000
            0x00,
            0x05,
            0x7e,
            0x40,
            0x00,   # "segmentation_upid_type":0
            0x00,   # "segmentation_upid_length":0
            0x22,   # "segmentation_type_id":0x22 (Break Start)
            0x00,   # "segment_num":0
            0x00,   # "segments_expected":0
            0x00,   # "CRC32":0 (set by the test)
            0x00,
            0x00,
            0x00
        ])
        self.setCRC32(section)
        binary = base64.b64encode(section).decode("utf-8")
        event = decode(binary)

        # Verify some data.
        self.assertEqual(event["table_id"], 0xfc)
        self.assertEqual(event["sap_type"], 3)
        self.assertEqual(event["pts_adjustment"], 0x123456789)
        self.assertEqual(event["splice_command_type"], 6)
        self.assertEqual(event["splice_command"]["pts_time"], 0x123456789)
        self.assertEqual(len(event["descriptors"]), 1)
        descriptor = event["descriptors"][0]
        self.assertEqual(descriptor["splice_descriptor_tag"], 0x02)
        self.assertEqual(descriptor["identifier"], "CUEI")
        self.assertEqual(descriptor["segmentation_event_id"], 0x01234567)
        self.assertEqual(descriptor["segmentation_duration"], 360000)
        self.assertEqual(descriptor["segmentation_type_id"], 0x22)

        # Verify that the event can be re-encoded
        self.assertEqual(binary, encode(json.dumps(event)))

    def test_AdStart(self):
        """Ad start test.
        """
        section = bytearray([
            0xfc,   # "table_id":0xfc
            0x30,   # "section_syntax_indicator":0
                    # "private_indicator":0
                    # "sap_type":3
                    # "section_length":54
            0x36,
            0x00,   # "protocol_version":0
            0x01,   # "encrypted_packet":0
                    # "encryption_algorithm":0
                    # "pts_adjustment":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "cw_index":0
            0xff,   # "tier":0xfff
            0xf0,   # "splice_command_length":5
            0x05,
            0x06,   # "splice_command_type":6 (time_signal)
            0xff,   # "time_specified_flag":1
                    # "reserved":0x3f
                    # "pts_time":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "descriptor_loop_length":32
            0x20,
            0x02,   # "splice_descriptor_tag":2 (segmentation_descriptor)
            0x1e,   # "descriptor_length":30
            0x43,   # "identifier":'CUEI'
            0x55,
            0x45,
            0x49,
            0x01,   # "segmentation_event_id":0x01234567
            0x23,
            0x45,
            0x67,
            0x7f,   # "segmentation_event_cancel_indicator":0
                    # "reserved":0x7f
            0xc0,   # "program_segmentation_flag":1
                    # "segmentation_duration_flag":1
                    # "delivery_not_restricted_flag":0
                    # "web_delivery_allowed_flag":0
                    # "no_regional_blackout_flag":0
                    # "archive_allowed_flag": 0
                    # "device_restrictions":0
            0x00,   # "segmentation_duration":2700000
            0x00,
            0x29,
            0x32,
            0xe0,
            0x0d,   # "segmentation_upid_type":0x0d (MID)
            0x0a,   # "segmentation_upid_length":10
            0x0e,   # "segmentation_upid_type":0x0e (ADS Information)
            0x03,   # "segmentation_upid_length":3
            0x41,   # "ADS"
            0x44,
            0x53,
            0x0f,   # "segmentation_upid_type":0x0f (URI)
            0x03,   # "segmentation_upid_length":3
            0x55,   # "URI"
            0x52,
            0x49,
            0x30,   # "segmentation_type_id":0x30 (Provider Advertisement Start)
            0x00,   # "segment_num":0
            0x00,   # "segments_expected":0
            0x00,   # "CRC32":0 (set by the test)
            0x00,
            0x00,
            0x00
        ])

        self.setCRC32(section)
        binary = base64.b64encode(section).decode("utf-8")
        event = decode(binary)

        # Verify some data.
        self.assertEqual(event["table_id"], 0xfc)
        self.assertEqual(event["pts_adjustment"], 0x123456789)
        self.assertEqual(event["splice_command_type"], 6)
        self.assertEqual(event["splice_command"]["pts_time"], 0x123456789)
        self.assertEqual(len(event["descriptors"]), 1)
        descriptor = event["descriptors"][0]
        self.assertEqual(descriptor["splice_descriptor_tag"], 0x02)
        self.assertEqual(descriptor["segmentation_event_id"], 0x01234567)
        self.assertEqual(descriptor["segmentation_duration"], 2700000)
        self.assertEqual(descriptor["segmentation_type_id"], 0x30)
        self.assertEqual(descriptor["segmentation_upid_type"], 0x0d)
        self.assertEqual(len(descriptor["MID"]), 2)
        mid = descriptor["MID"]
        self.assertEqual(mid[0]["segmentation_upid_type"], 0x0e)
        self.assertEqual(mid[0]["ADS"], "ADS")
        self.assertEqual(mid[1]["segmentation_upid_type"], 0x0f)
        self.assertEqual(mid[1]["URI"], "URI")

        # Verify that the event can be re-encoded
        self.assertEqual(binary, encode(json.dumps(event)))

    def test_AdEnd(self):
        """Ad end test.
        """
        section = bytearray([
            0xfc,   # "table_id":0xfc
            0x30,   # "section_syntax_indicator":0
                    # "private_indicator":0
                    # "sap_type":3
                    # "section_length":49
            0x31,
            0x00,   # "protocol_version":0
            0x01,   # "encrypted_packet":0
                    # "encryption_algorithm":0
                    # "pts_adjustment":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "cw_index":0
            0xff,   # "tier":0xfff
            0xf0,   # "splice_command_length":5
            0x05,
            0x06,   # "splice_command_type":6 (time_signal)
            0xff,   # "time_specified_flag":1
                    # "reserved":0x3f
                    # "pts_time":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "descriptor_loop_length":27
            0x1b,
            0x02,   # "splice_descriptor_tag":2 (segmentation_descriptor)
            0x19,   # "descriptor_length":25
            0x43,   # "identifier":'CUEI'
            0x55,
            0x45,
            0x49,
            0x01,   # "segmentation_event_id":0x01234567
            0x23,
            0x45,
            0x67,
            0x7f,   # "segmentation_event_cancel_indicator":0
                    # "reserved":0x7f
            0x80,   # "program_segmentation_flag":1
                    # "segmentation_duration_flag":0
                    # "delivery_not_restricted_flag":0
                    # "web_delivery_allowed_flag":0
                    # "no_regional_blackout_flag":0
                    # "archive_allowed_flag": 0
                    # "device_restrictions":0
            0x0d,   # "segmentation_upid_type":0x0d (MID)
            0x0a,   # "segmentation_upid_length":10
            0x0e,   # "segmentation_upid_type":0x0e (ADS Information)
            0x03,   # "segmentation_upid_length":3
            0x41,   # "ADS"
            0x44,
            0x53,
            0x0f,   # "segmentation_upid_type":0x0f (URI)
            0x03,   # "segmentation_upid_length":3
            0x55,   # "URI"
            0x52,
            0x49,
            0x31,   # "segmentation_type_id":0x31 (Provider Advertisement End)
            0x00,   # "segment_num":0
            0x00,   # "segments_expected":0
            0x00,   # "CRC32":0 (set by the test)
            0x00,
            0x00,
            0x00
        ])

        self.setCRC32(section)
        binary = base64.b64encode(section).decode("utf-8")
        event = decode(binary)

        # Verify some data.
        self.assertEqual(event["table_id"], 0xfc)
        self.assertEqual(event["pts_adjustment"], 0x123456789)
        self.assertEqual(event["splice_command_type"], 6)
        self.assertEqual(event["splice_command"]["pts_time"], 0x123456789)
        self.assertEqual(len(event["descriptors"]), 1)
        descriptor = event["descriptors"][0]
        self.assertEqual(descriptor["splice_descriptor_tag"], 0x02)
        self.assertEqual(descriptor["segmentation_event_id"], 0x01234567)
        self.assertEqual(descriptor["segmentation_type_id"], 0x31)
        self.assertEqual(descriptor["segmentation_upid_type"], 0x0d);
        self.assertEqual(len(descriptor["MID"]), 2)
        mid = descriptor["MID"]
        self.assertEqual(mid[0]["segmentation_upid_type"], 0x0e)
        self.assertEqual(mid[0]["ADS"], "ADS")
        self.assertEqual(mid[1]["segmentation_upid_type"], 0x0f)
        self.assertEqual(mid[1]["URI"], "URI")

        # Verify that the event can be re-encoded
        self.assertEqual(binary, encode(json.dumps(event)))

    def test_BreakEnd(self):
        """Break end test.
        """
        section = bytearray([
            0xfc,   # "table_id":0xfc
            0x30,   # "section_syntax_indicator":0
                    # "private_indicator":0
                    # "sap_type":3
                    # "section_length":39
            0x27,
            0x00,   # "protocol_version":0
            0x01,   # "encrypted_packet":0
                    # "encryption_algorithm":0
                    # "pts_adjustment":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "cw_index":0
            0xff,   # "tier":0xfff
            0xf0,   # "splice_command_length":5
            0x05,
            0x06,   # "splice_command_type":6 (time_signal)
            0xff,   # "time_specified_flag":1
                    # "reserved":0x3f
                    # "pts_time":0x123456789
            0x23,
            0x45,
            0x67,
            0x89,
            0x00,   # "descriptor_loop_length":17
            0x11,
            0x02,   # "splice_descriptor_tag":2 (segmentation_descriptor)
            0x0f,   # "descriptor_length":15
            0x43,   # "identifier":"CUEI"
            0x55,
            0x45,
            0x49,
            0x01,   # "segmentation_event_id":0x01234567
            0x23,
            0x45,
            0x67,
            0x7f,   # "segmentation_event_cancel_indicator":0
                    # "reserved":0x7f
            0x80,   # "program_segmentation_flag":1
                    # "segmentation_duration_flag":0
                    # "delivery_not_restricted_flag":0
                    # "web_delivery_allowed_flag":0
                    # "no_regional_blackout_flag":0
                    # "archive_allowed_flag": 0
                    # "device_restrictions":0
            0x00,   # "segmentation_upid_type":0
            0x00,   # "segmentation_upid_length":0
            0x23,   # "segmentation_type_id":0x23 (Break End)
            0x00,   # "segment_num":0
            0x00,   # "segments_expected":0
            0x00,   # "CRC32":0 (set by the test)
            0x00,
            0x00,
            0x00
        ])

        self.setCRC32(section)
        binary = base64.b64encode(section).decode("utf-8")
        event = decode(binary)

        # Verify some data.
        self.assertEqual(event["table_id"], 0xfc)
        self.assertEqual(event["pts_adjustment"], 0x123456789)
        self.assertEqual(event["splice_command_type"], 6)
        self.assertEqual(event["splice_command"]["pts_time"], 0x123456789)
        self.assertEqual(len(event["descriptors"]), 1)
        descriptor = event["descriptors"][0]
        self.assertEqual(descriptor["splice_descriptor_tag"], 0x02)
        self.assertEqual(descriptor["segmentation_event_id"], 0x01234567)
        self.assertEqual(descriptor["segmentation_type_id"], 0x23)

        # Verify that the event can be re-encoded
        self.assertEqual(binary, encode(json.dumps(event)))

class TestDASHManifestGeneration(unittest.TestCase):
    """SCTE-35 event stream DASH manifest generation test suite.
    """

    def test_manifest1(self):
        """Manifest generation test.

        self -- Test instance
        """
        jsonString = """
[
    {"type":"Break Start", "time":30.0, "duration":34.0, "event_id":1},
    {"type":"Provider Advertisement Start", "time":32.0, "duration":30.0, "event_id":2},
    {"type":"Provider Advertisement End", "time":62.0, "event_id":2},
    {"type":"Break End", "time":64.0, "event_id":1}
]
"""
        jsonArray = json.loads(jsonString)
        manifestString = mpd(io.StringIO(jsonString))
        root = ET.fromstring(manifestString)
        periodIdx = 0
        binaryIdx = 0
        for element in root.findall(".//*"):
            # Use string.endswith() here to ignore any namespace prefix.
            if element.tag.endswith("Period"):
                # Verify the period start time.
                m = re.match(r"PT([\d\.]+)S", element.get("start"))
                if m:
                    self.assertEqual(float(m.group(1)), jsonArray[periodIdx]["time"])
                else:
                    self.fail("No start attribute")
                periodIdx += 1
            elif element.tag.endswith("Binary"):
                # Verify the SCTE-35 event.
                binary = element.text.strip()
                self.assertEqual(binary, encode(json.dumps(jsonArray[binaryIdx])))
                binaryIdx += 1
            else:
                pass
        self.assertEqual(periodIdx, len(jsonArray))
        self.assertEqual(binaryIdx, len(jsonArray))

if __name__ == '__main__':
    unittest.main()
