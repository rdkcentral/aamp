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

import base64
import json
import sys
import xml.etree.ElementTree as ET
from manifest import DASHManifest

def splice_info_section(section):
    """Encode or decode a splice_info_section

    section -- Encoder or decoder instance
    """
    section.Start()
    crc32 = section.StartCRC32()
    if 0xfc != section.Byte("table_id", 0xfc):
        raise Exception("Unsupported table_id")
    section.Bool("section_syntax_indicator")
    section.Bool("private_indicator")
    section.Bits("sap_type", 2, 0x03)
    section_length = section.Length("section_length", 12)
    section.Byte("protocol_version")
    if section.Bool("encrypted_packet"):
        raise Exception("Unsupported encrypted_packet")
    section.Bits("encryption_alogirithm", 6)
    section.Bits("pts_adjustment", 33)
    section.Byte("cw_index")
    section.Bits("tier", 12, 0xfff)

    # The splice_command_length is the number of bytes after
    # splice_command_type and before descriptor_loop_length.
    splice_command_length = section.Length("splice_command_length", 12)
    if section.Byte("splice_command_type", 0x6) != 0x6:
        raise Exception("Unsupporteded splice command type")
    splice_command_length.Start()

    splice_command = section.Subsection("splice_command")
    splice_command.Start()
    if splice_command.Bool("time_specified_flag"):
        splice_command.ReservedBits(6)
        splice_command.Bits("pts_time", 33)
    else:
        splice_command.ReservedBits(7)

    splice_command.End()
    splice_command_length.End()

    # Descriptor loop.
    descriptor_loop_length = section.Length("descriptor_loop_length", 16)
    descriptor_loop = section.DescriptorLoop("descriptors", descriptor_loop_length)
    while descriptor_loop.hasAnotherDescriptor():
        # segmentation_descriptor
        descriptor = descriptor_loop.Descriptor()
        if descriptor.Byte("splice_descriptor_tag", 0x02) != 0x02:
            raise Exception("Unsuppported splice_descriptor_tag")

        descriptor_length = descriptor.Length("descriptor_length", 8)
        descriptor.String("identifier", 4, "CUEI")
        descriptor.Bits("segmentation_event_id", 32)
        segmentation_event_cancel_indicator = descriptor.Bool("segmentation_event_cancel_id")
        descriptor.ReservedBits(7)
        if not segmentation_event_cancel_indicator:
            if not descriptor.Bool("program_segmentation_flag", True):
                raise Exception("Program segmentation not supported")

            segmentation_duration_flag = descriptor.Bool("segmentation_duration_flag")
            if not descriptor.Bool("delivery_not_restricted_flag"):
                descriptor.Bool("web_delivery_allowed_flag")
                descriptor.Bool("no_regional_blackout_flag")
                descriptor.Bool("archive_allowed_flag")
                descriptor.Bits("device_restrictions", 2)
            else:
                descriptor.reservedBits(5)

            # Program segmentation not supported

            if segmentation_duration_flag:
                descriptor.Bits("segmentation_duration", 40)

            segmentation_upid_type = descriptor.Byte("segmentation_upid_type")
            segmentation_upid_length = descriptor.Length("segmentation_upid_length", 8)

            # segmentation_upid()
            if segmentation_upid_type == 0x00:
                # Not Used.
                pass
            elif segmentation_upid_type == 0x0d:
                # MID
                upid_loop = descriptor.DescriptorLoop("MID", segmentation_upid_length)
                while upid_loop.hasAnotherDescriptor():
                    upid = upid_loop.Descriptor()
                    upid.Start()
                    upid_type = upid.Byte("segmentation_upid_type")
                    upid_length = upid.Length("segmentation_upid_length", 8)
                    if upid_type == 0x0e:
                        # ADS Information
                        upid.String("ADS", upid_length.length, "")
                    elif upid_type == 0x0f:
                        # ADS Information
                        upid.String("URI", upid_length.length, "")
                    else:
                        raise Exception("Unsupported UPID type")
                    upid_length.End()
                    upid.End()
            elif segmentation_upid_type == 0x0e:
                # ADS Information
                descriptor.String("ADS", segmentation_upid_length.length)
            elif segmentation_upid_type == 0x0f:
                # URI
                descriptor.String("URI", segmentation_upid_length.length)
            else:
                raise Exception("Unsupported UPID type")

            segmentation_upid_length.End()

            segmentation_type_id = descriptor.Byte("segmentation_type_id")
            descriptor.Byte("segment_num")
            descriptor.Byte("segments_expected")
            if segmentation_type_id in [0x34, 0x36, 0x38, 0x3a]:
                descriptor.Byte("subsegment_num")
                descriptor.Byte("subsegments_expected")
        descriptor.End()
        descriptor_length.End()
    descriptor_loop_length.End()

    # alignment_stuffing not supported.
    # E_CRC_32 not supported.

    section.CRC32("CRC32", crc32)
    section_length.End()
    section.End()

def CRC32(ba):
    """Calculate an MPEG CRC32 value

    ba -- Bytearray

    return the CRC32 value
    """
    crc = 0xffffffff
    for val in ba:
        crc ^= val << 24
        for _ in range(8):
            crc = crc << 1 if (crc & 0x80000000) == 0 else (crc << 1) ^ 0x104c11db7
    return crc

def bitsToByteArray(bitString):
    """Convert a string of bits to a bytearray

    bitString -- String of zero and one characters

    return a bytearray
    """
    ba = bytearray()
    for i in range(0, len(bitString), 8):
        subString = bitString[i:i+8]
        ba.append(int(subString, 2))
    return ba

class SCTE35DecoderSectionLength:
    """Handles decoding section lengths
    """

    def __init__(self, decoder, tag, length):
        """Class constructor

        self -- Instance
        decoder -- SCTE35Decoder instance
        tag -- Length tag
        length -- Expected size of the section in bytes
        """
        self.decoder = decoder
        self.tag = tag
        self.length = length
        self.startBitOffset = decoder.bitOffset

    def Start(self):
        """Set the start of the section data covered by the length

        This is used if the section data included in the length does not start
        immediately after the length field.

        self -- Instance
        """
        self.startBitOffset = self.decoder.bitOffset

    def End(self):
        """Set the end of the section data covered by the length

        self -- Instance
        throw Exception if the length is not as expected
        """
        endBitOffset = self.decoder.bitOffset
        if (endBitOffset - self.startBitOffset) != (self.length*8):
            raise Exception("Length mismatch")

class SCTE35SectionDecoder():
    """Low level section decoder.

    An instance of this class is shared between the main decoder and
    subsection and descriptor decoders.
    """

    def Byte(self, tag):
        """Extract an 8 bit byte from the section data

        self -- Instance
        tag -- Field name
        return A byte value
        throw Exception if the data is not 8 bit aligned
        """
        if self.bitOffset & 7:
            raise Exception("Byte %s not aligned" % (tag))
        bits = self.bitString[self.bitOffset:self.bitOffset + 8]
        self.bitOffset += 8
        value = int(bits, 2)
        return value

    def String(self, tag, n):
        """Extract a string of 8 bit bytes from the section data

        self -- Instance
        tag -- Field name
        n -- Number of bytes
        return A byte string
        throw Exception if the data is not 8 bit aligned
        """
        if self.bitOffset & 7:
            raise Exception("String %s not aligned" % (tag))
        ba = bytearray()
        for i in range(n):
            bits = self.bitString[self.bitOffset:self.bitOffset + 8]
            self.bitOffset += 8
            ba.append(int(bits, 2))
        return ba.decode("utf-8")

    def Bool(self, tag):
        """Extract a flag bit from the section data

        self -- Instance
        tag -- Field name
        return A boolean value
        throw Exception if the data is not 8 bit aligned
        """
        if self.bitString[self.bitOffset] == "1":
            value = True
        else:
            value = False
        self.bitOffset += 1
        return value

    def Bits(self, tag, n):
        """Extract a multibit value from the section data

        self -- Instance
        tag -- Field name
        n -- Number of bits
        return A value
        throw Exception if the data is not 8 bit aligned
        """
        bits = self.bitString[self.bitOffset:self.bitOffset + n]
        self.bitOffset += n
        value = int(bits, 2)
        return value

    def ReservedBits(self, n):
        """Check reserved bits in the section data

        self -- Instance
        n -- Number of bits
        throw Exception if a reserved bit is not set
        """
        bits = self.bitString[self.bitOffset:self.bitOffset + n]
        self.bitOffset += n
        for bit in bits:
            if bit != "1":
                raise Exception("Reserved bit zero")

    def Length(self, tag, n):
        """Mark the position of a data length in the section data

        self -- Instance
        tag -- Field name
        n -- Number of bits
        return An instance representing the section data
        """
        value = self.Bits(tag, n)
        return SCTE35DecoderSectionLength(self, tag, value)

class SCTE35DecoderDescriptorLoop():
    """Class representing a descriptor loop used by the decoder.
    """

    def __init__(self, parent, decoder, maxBitOffset, tag):
        """Constructor.

        self -- Instance
        parent -- Parent deocder instance
        decoder -- Low level section decoder instance
        maxBitOffset -- Maximum value of the descriptor bit offset
        tag --- Loop tag, typically "descriptors"
        """
        self.parent = parent
        self.decoder = decoder
        self.maxBitOffset = maxBitOffset
        self.tag = tag

    def hasAnotherDescriptor(self):
        """Return True if there is another descriptor to decode.

        self -- Instance
        return True if there is another descriptor
        """
        return self.decoder.bitOffset < self.maxBitOffset

    def Descriptor(self):
        """Create a Descriptor instance.

        self -- Instance
        return Descriptor instance
        """
        return self.parent.Descriptor(self)

    def DescriptorEnd(self, json):
        """Called at the end of a descriptor

        self -- Instance
        json -- JSON representation of the descriptor
        """
        self.parent.json[self.tag].append(json)

class SCTE35Decoder():
    """Main SCTE35 decoder class.
    """

    def __init__(self, decoder, parent = None, tag = None, loop = None):
        """Constructor.

        self -- Decoder instance
        decoder -- Shared low level section data decoder
        parent -- Optional parent decoder instance
        tag -- Optional tag for subsections
        loop -- Optional descriptor loop instance
        """
        self.json = dict()
        self.decoder = decoder
        self.parent = parent
        self.tag = tag
        self.loop = loop

    def Start(self):
        pass

    def End(self):
        """End of decoder section.

        self -- Instance
        """
        if self.loop is not None:
            # End of desciptor loop.
            self.loop.DescriptorEnd(self.json)
        elif self.parent is not None:
            # End of descriptor or subsection.
            self.parent.json[self.tag] = self.json
        else:
            pass

    def StartCRC32(self):
        """Start of section data for CRC32 calculation

        self -- Instance
        return Current bit offset
        """
        return self.decoder.bitOffset

    def CRC32(self, tag, startOffset):
        """Calculate and check a CRC32 value.

        self -- Instance
        tag -- Field name
        startOffset -- Starting bit offset
        throw Exception on an unexpected CRC32 value
        """
        ba = bytearray()
        bitString = self.decoder.bitString[startOffset:self.decoder.bitOffset]
        for i in range(0, len(bitString), 8):
            substring = bitString[i:i+8]
            ba.append(int(substring, 2))
        crc32 = CRC32(ba)
        if crc32 != self.decoder.Bits(tag, 32):
            raise Exception("CRC32 mismatch")
        self.json[tag] = crc32

    def Subsection(self, tag):
        """Start of a subsection.

        self -- Instance
        tag -- Subsection name
        return A subsection decoder instance
        """
        return SCTE35Decoder(self.decoder, self, tag, None)

    def Descriptor(self, loop):
        """Start of a descriptor in a descriptor loop.

        self -- Instance
        loop -- Descriptor loop instance
        return A descriptor decoder instance
        """
        return SCTE35Decoder(self.decoder, self, None, loop)

    def DescriptorLoop(self, tag, length):
        """Create a descriptor loop instance.

        self -- Instance
        tag -- Descriptor loop name, typically "descriptors"
        length -- Number of bytes
        return A descriptor loop instance
        """
        self.json[tag] = []
        return SCTE35DecoderDescriptorLoop(self, self.decoder, self.decoder.bitOffset + (length.length*8), tag)

    def Byte(self, tag, defaultValue = 0):
        """Decode an 8 bit byte.

        self -- Instance
        tag -- Field name
        defaultValue -- Ignored
        return A byte value
        throw Exception if not 8 bit aligned
        """
        value = self.decoder.Byte(tag)
        self.json[tag] = value
        return value

    def String(self, tag, n, defaultValue):
        """Decode a string of 8 bit bytes.

        self -- Instance
        tag -- Field name
        n -- Number of bytes
        defaultValue -- Ignored
        return A string
        throw Exception if not 8 bit aligned
        """
        value = self.decoder.String(tag, n)
        self.json[tag] = value
        return value

    def Bool(self, tag, defaultValue = False):
        """Decode a bit flag.

        self -- Instance
        tag -- Field name
        defaultValue -- Ignored
        return A boolean
        """
        value = self.decoder.Bool(tag)
        self.json[tag] = value
        return value

    def Bits(self, tag, n, defaultValue = 0):
        """Decode a multi-bit flag.

        self -- Instance
        tag -- Field name
        n -- Number of bits
        defaultValue -- Ignored
        return An integer value
        """
        value = self.decoder.Bits(tag, n)
        self.json[tag] = value
        return value

    def ReservedBits(self, n):
        """Check reserved bits.

        self -- Instance
        n -- Number of bits
        throw Exception if any reserved bit is zero
        """
        self.decoder.ReservedBits(n)

    def Length(self, tag, n):
        """Decode a section data length

        self -- Instance
        tag -- Field name
        n -- Number of bits
        return An instance representing the data length
        """
        value = self.decoder.Length(tag, n)
        self.json[tag] = value.length
        return value

    def decode(self, binary):
        """Decode an SCTE-35 splice info signal.

        self -- Instance
        binary -- Base64 encoded data string
        """
        # Convert to a bitstring
        ba = base64.b64decode(binary)
        self.decoder.bitString = ""
        self.decoder.bitOffset = 0
        for byte in ba:
            self.decoder.bitString += format(byte, '08b')

        # Decode the section data
        splice_info_section(self)

class SCTE35SectionEncoder():
    """Low level SCTE-35 section data encoder class.

    An instance of this class is shared between the main encoder and encoders
    for subsections and descriptors.
    """

    def Start(self):
        """Section start.

        self -- Instance
        """
        self.bitString = ""

    def End(self):
        """Section end.

        self -- Instance
        """
        pass

    def Bool(self, value, defaultValue):
        """Encode a bit flag.

        self -- Instance
        value -- Bit value or None
        defaultValue -- Default value used if value is None
        return the bit value
        """
        if value is None:
            value = defaultValue

        if value:
            self.bitString += "1"
        else:
            self.bitString += "0"
        return value

    def Byte(self, value, defaultValue = 0):
        """Encode an 8 bit byte.

        self -- Instance
        value -- Byte value or None
        defaultValue -- Default value used if value is None
        return the byte value
        """
        if value is None:
            value = defaultValue
        self.bitString += format(value, '08b')
        return value

    def String(self, value, defaultValue):
        """Encode a string of 8 bit bytes.

        self -- Instance
        value -- String value or None
        defaultValue -- Default value used if value is None
        return the string value
        """
        if value is None:
            value = defaultValue
        encoded = value.encode('utf-8')
        ba = bytearray(encoded)
        for i in range(len(ba)):
            self.Byte(ba[i])
        return value

    def Bits(self, value, n, defaultValue):
        """Encode a multi-bit value.

        self -- Instance
        value -- Value or None
        n -- Number of bits
        defaultValue -- Default value used if value is None
        return the value
        """
        if value is None:
            value = defaultValue
        self.bitString += format(value, '0%db' % (n))
        return value

    def ReservedBits(self, n):
        """Encode reserved bits.

        self -- Instance
        n -- Number of bits
        """
        self.bitString += "1" * n

    def setFieldValue(self, value, offset, n):
        """Set a value in existing section data

        self -- Instance
        value -- Value
        offset -- Bit offset where the value is stored
        n -- Number of bits
        """
        string = format(int(value), '0%db' % (n))
        self.bitString = self.bitString[:offset] + string + self.bitString[offset + n:]

    def Length(self, n):
        """Reserve space in the bitstring for a length.

        self -- Instance
        n -- Number of bits
        return The bit offset where the length is to be stored
        """
        # Add 'X' placeholders for length bits and return the bit offset
        offset = len(self.bitString)
        self.bitString += "X" * n
        return offset

    def toByteArray(self):
        """Convert the encoded bit string into a bytearray

        self -- Instance
        return A bytearray
        """
        ba = bytearray()
        for i in range(0, len(self.bitstring), 8):
            substring = self.bitstring[i:i+8]
            ba.append(int(substring, 2))
        return ba

class SCTE35EncoderSectionLength():
    """Class to represent a section data length.
    """

    def __init__(self, encoder, offset, n):
        """Constructor

        self -- Instance
        encoder -- Shared low level section data encoder
        offset -- Bit offset where the length is to be stored
        n -- Number of length bits
        """
        self.encoder = encoder
        self.lengthOffset = offset
        self.n = n
        self.startOffset = len(encoder.bitString)
        self.length = 0

    def Start(self):
        """Start of the data section.

        Called if the start of the data section does not immediately follow the
        length.

        self -- Instance
        """
        self.startOffset = len(self.encoder.bitString)

    def End(self):
        """End of the data section.

        self -- Instance
        """
        # Calculate and store the length in in bytes
        length = (len(self.encoder.bitString) - self.startOffset)/8
        self.encoder.setFieldValue(length, self.lengthOffset, self.n)

class SCTE35EncoderDescriptorLoop():
    """Descriptor loop encoder class
    """

    def __init__(self, parent, descriptors):
        """Constructor

        parent -- Parent encoder
        descriptors -- JSON array of descriptors, or None for an empty list
        """
        self.parent = parent
        if descriptors is None:
            self.count = 0
            self.descriptors = []
        else:
            self.count = len(descriptors)
            self.descriptors = descriptors
        self.idx = 0

    def hasAnotherDescriptor(self):
        """See if there is another descriptor.

        return True if there is another descriptor
        """
        return self.idx < self.count

    def Descriptor(self):
        """Start a descriptor.

        self -- Instance
        return a decriptor encoder instance
        """
        descriptor = self.descriptors[self.idx]
        self.idx += 1
        return self.parent.Descriptor(descriptor)

class SCTE35Encoder():
    """ Main SCTE-35 splice info encoder class.
    """

    def __init__(self, parent = None, event = None):
        """Constructor.

        self -- Instance
        parent -- Optional parent encoder
        event -- JSON representation or None if empty
        """
        self.parent = parent
        if parent is not None:
            self.encoder = parent.encoder
        if event is None:
            self.event = dict()
        else:
            self.event = event
        self.CRC32EndOffset = None

    def Start(self):
        """Section or subsection start.

        self -- Instance
        """
        if self.parent is None:
            self.encoder.Start()

    def End(self):
        """Section or subsection end.

        CRC32 calculation is deferred until here so that all length fields can
        be set before the calculation is performed.

        self -- Instance
        """
        self.encoder.End()
        if self.CRC32EndOffset:
            bitString = self.encoder.bitString[self.CRC32StartOffset:self.CRC32EndOffset]
            ba = bytearray()
            for i in range(0, len(bitString), 8):
                substring = bitString[i:i+8]
                ba.append(int(substring, 2))
            crc32 = CRC32(ba)
            self.encoder.setFieldValue(crc32, self.CRC32EndOffset, 32)

    def StartCRC32(self):
        """Start a region of section data for which a CRC32 value will be valid.

        self -- Instance
        """
        self.CRC32StartOffset = len(self.encoder.bitString)
        return self.CRC32StartOffset

    def CRC32(self, tag, startOffset):
        """CRC32 value.

        Setting of CRC32 values is deferred until the end of section data is
        reached. This is to allow length fields to be set first.

        self -- Instance
        tag -- Field name
        startOffset - Starting bit offset
        """
        self.CRC32EndOffset = len(self.encoder.bitString)
        self.encoder.bitString += "X" * 32

    def Subsection(self, tag):
        """Define a subsection of the data.

        self -- Instance
        tag -- Field name
        return -- Subsection encoder instance
        """
        return SCTE35Encoder(self, self.event.get(tag))

    def Bool(self, tag, defaultValue = False):
        """Encode a bit flag.

        self -- Instance
        tag -- Field name
        defaultValue -- Default value
        return -- Flag value
        """
        value = self.event.get(tag)
        return self.encoder.Bool(value, defaultValue)

    def Byte(self, tag, defaultValue = 0):
        """Encode an 8 bit byte.

        self -- Instance
        tag -- Field name
        defaultValue -- Default value
        return -- Byte value
        """
        value = self.event.get(tag)
        return self.encoder.Byte(value, defaultValue)

    def Bits(self, tag, n, defaultValue = 0):
        """Encode a multi-bit byte.

        self -- Instance
        tag -- Field name
        n -- Number of bits
        defaultValue -- Default value
        return -- Integer value
        """
        value = self.event.get(tag)
        return self.encoder.Bits(value, n, defaultValue)

    def ReservedBits(self, n):
        """Encode reserved bits.

        self -- Instance
        n -- Number of bits
        """
        self.encoder.ReservedBits(n)

    def String(self, tag, n, defaultValue):
        """Encode a string of 8 bit bytes.

        self -- Instance
        tag -- Field name
        n -- Ignored
        defaultValue -- Default value
        return -- String value
        """
        value = self.event.get(tag)
        return self.encoder.String(value, defaultValue)

    def Length(self, tag, n):
        """Create a section length instance.

        self -- Instance
        tag -- Field name
        n -- Number of bits
        return -- Section length instance
        """
        offset = self.encoder.Length(n)
        return SCTE35EncoderSectionLength(self.encoder, offset, n)

    def DescriptorLoop(self, tag, length):
        """Create a descriptor loop instance.

        self -- Instance
        tag -- Field name
        length -- Ignored
        return -- Descriptor loop instance
        """
        return SCTE35EncoderDescriptorLoop(self, self.event.get(tag))

    def Descriptor(self, descriptor):
        """Create a descriptor instance.

        self -- Instance
        descriptor -- Descriptor JSON object
        return -- Descriptor decoder instance
        """
        return SCTE35Encoder(self, descriptor)

    def encode(self, event):
        """Encode an SCTE-35 splice info signal.

        self -- Instance
        event -- JSON representation of the signal
        return A base64 encoded string
        """
        # Ensure that the event has a descriptor loop.
        if "descriptors" not in event:
            event["descriptors"] = json.loads("[]")

        if len(event["descriptors"]) == 0:
            if ("event_id" in event) or ("duration" in event) or ("type" in event):
                # Ensure that the event has at least one descriptor.
                event["descriptors"].append(json.loads("{}"))

        # Translate some generic parameters.
        if "time" in event:
            pts_time = int(float(event["time"])*90000.0)
            jsonString = '{"time_specified_flag":true, "pts_time":%d}' % (pts_time)
            event["splice_command"] = json.loads(jsonString)

        if "event_id" in event:
            event["descriptors"][0]["segmentation_event_id"] = event["event_id"]

        if "duration" in event:
            pts_duration = int(float(event["duration"])*90000.0)
            event["descriptors"][0]["segmentation_duration_flag"] = True
            event["descriptors"][0]["segmentation_duration"] = pts_duration

        if "type" in event:
            if event["type"] == "Break Start":
                event["descriptors"][0]["segmentation_type_id"] = 0x22
            elif event["type"] == "Break End":
                event["descriptors"][0]["segmentation_type_id"] = 0x23
            elif event["type"] == "Provider Advertisement Start":
                event["descriptors"][0]["segmentation_type_id"] = 0x30
            elif event["type"] == "Provider Advertisement End":
                event["descriptors"][0]["segmentation_type_id"] = 0x31
            elif event["type"] == "Provider Placement Opportunity Start":
                event["descriptors"][0]["segmentation_type_id"] = 0x34
            elif event["type"] == "Provider Placement Opportunity End":
                event["descriptors"][0]["segmentation_type_id"] = 0x35
            else:
                print("Unsupported type %s" % (event["type"]))

        self.event = event
        self.encoder = SCTE35SectionEncoder()
        splice_info_section(self)
        ba = bitsToByteArray(self.encoder.bitString)
        return base64.b64encode(ba).decode("utf-8")

class SCTE35Manifest(DASHManifest):
    """Class to handle the creation of a DASH manifest from an array of SCTE-35 events.
    """
    def __init__(self):
        """Class constructor.

        self -- Instance
        """
        super().__init__()
        self.root = None
        self.namespaces["urn:scte:scte35:2014:xml+bin"] = "scte35"
        self.namespaces["urn:mpeg:dash:schema:mpd:2011"] = ""

    def mpd(self, events):
        """Create DASH manifest.

        self -- Instance
        events -- SCTE-35 events
        return DASH manifest in a string
        """
        self.root = ET.Element("{urn:mpeg:dash:schema:mpd:2011}MPD")

        timescale = 90000.0
        for event in events:
            startTime = event.get("time")
            period = ET.Element("{urn:mpeg:dash:schema:mpd:2011}Period")
            period.set("start", "PT%.3fS" % (startTime))
            eventStream = ET.Element("{urn:mpeg:dash:schema:mpd:2011}EventStream")
            eventStream.set("schemeIdUri", "urn:scte:scte35:2014:xml+bin")
            eventStream.set("timescale", "%d" % (int(timescale)))
            eventStream.set("presentationTimeOffset", "%d" % (int(timescale*startTime)))
            eventElement = ET.Element("{urn:mpeg:dash:schema:mpd:2011}Event")
            eventElement.set("presentationTime", "%d" % (int(timescale*startTime)))
            if "duration" in event:
                eventElement.set("duration", "%d" % (int(timescale*event.get("duration"))))
            signal = ET.Element("{urn:scte:scte35:2014:xml+bin}Signal")
            binary = ET.Element("{urn:scte:scte35:2014:xml+bin}Binary")
            encoder = SCTE35Encoder()
            binary.text = encoder.encode(event)
            signal.append(binary)
            eventElement.append(signal)
            eventStream.append(eventElement)
            period.append(eventStream)
            self.root.append(period)

        return self.toString(self.root)

def decode(binary):
    """Decode an SCTE-35 splice info signal.

    binary -- Base64 encode signal data
    return A JSON representation of the signal
    """
    decoder = SCTE35Decoder(SCTE35SectionDecoder())
    decoder.decode(binary)
    return decoder.json

def encode(event):
    """Encode SCTE-35 splice info signals.

    evemt -- Signal JSON string
    return Base64 encoded string
    """
    try:
        encoder = SCTE35Encoder()
        return encoder.encode(json.loads(event))

    except FileNotFoundError as e:
        print("Failed to open file - %s" % (e))

    except json.decoder.JSONDecodeError as e:
        print("Failed to decode file - %s" % (e))

    except KeyError as e:
        print("Failed to decode %s - %s" % (filename, e))

def mpd(file):
    """Create DASH manifest file.

    file -- Signal JSON file
    return DASH manifest string
    """
    string = ""
    try:
        events = json.load(file)
        manifest = SCTE35Manifest()
        string = manifest.mpd(events)

    except json.decoder.JSONDecodeError as e:
        print("Failed to decode file - %s" % (e))

    except KeyError as e:
        print("Failed to decode %s" % (e))

    return string

if __name__ == "__main__":
    # Parse the command line arguments.
    filename = None
    idx = 1
    while idx < len(sys.argv):
        arg = sys.argv[idx]
        if arg == "encode":
            idx += 1
            if idx >= len(sys.argv):
                print("Missing encode string")
            else:
                print(encode(sys.argv[idx]))
                idx += 1
        elif arg == "decode":
            idx += 1
            if idx >= len(sys.argv):
                print("Missing decode string")
            else:
                obj = decode(sys.argv[idx])
                print(json.dumps(obj, indent="  "))
                idx += 1
        elif arg == "mpd":
            idx += 1
            if idx >= len(sys.argv):
                print("Missing manifest filename")
            else:
                filename = sys.argv[idx]
                try:
                    with open(filename, "r") as file:
                        print(mpd(file))

                except FileNotFoundError as e:
                    print("Failed to open file - %s" % (e))

                idx += 1
        else:
            print("""
Usage: python3 scte35.py encode <json>
       python3 scte35.py decode <base64>
       python3 scte35.py mpd <filename>

SCTE-35 splice info signal helper.
    encode <json>       Encode signal data from JSON string <json>.
    decode <base64>     Decode base64 signal binary data.
    mpd <filename>      Create DASH manifest from JSON file <filename>.
""")
            sys.exit()
