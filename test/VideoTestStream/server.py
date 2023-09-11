##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2022 RDK Management
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

import time
import re
import os
import argparse
from datetime import datetime, timezone
from enum import Enum
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse
import copy
import xml.etree.ElementTree as ET

def getBoolean(key, string, value):
    """Get a bolean parameter.

    key -- Parameter name
    string -- Parameter value as a string
    value -- Default parameter value
    """
    if string in ("true", "True", "1"):
        value = True
    elif string in ("false", "False", "0"):
        value = False
    else:
        print("Unexpected %s value %s" % (key, string))

    return value

class StreamType(Enum):
    VOD = 1
    EVENT = 2
    LIVE = 3

class DASHManifest():
    """DASH manifest rewriting class.

    Methods in this class implement the rewriting of DASH manifests to emulate
    ongoing events or live content.
    """
    nsAndTagPattern = re.compile(r"{([^}]+)}(.+)")
    minutesAndSecondsPattern = re.compile(r"PT([\d]+)M([\d\.]+)S")
    secondsPattern = re.compile(r"PT([\d\.]+)S")
    indent = "  "

    def __init__(self):
        self.namespaces = dict()

    def getDuration(self, durationString):
        """Parse a time duration string.

        This is also used to parse the Period start time.

        durationString -- Input string
        return interval in seconds
        """
        # "PT([\d]+)M([\d\.]+)S"
        m = self.minutesAndSecondsPattern.match(durationString)
        if m:
            duration = (float(m.group(1))*60.0) + float(m.group(2))
        else:
            # "PT([\d\.]+)S"
            m = self.secondsPattern.match(durationString)
            if m:
                duration = float(m.group(1))
            else:
                print("Unsupported duration format %s" % (durationString))
                duration = 0.0
        return duration

    def rewriteSegmentTemplate(self, segmentTemplate, periodStartTime, currentPlayTime, mediaPresentationDuration):
        """Modify a DASH SegmentTemplate to emulate an ongoing event or live.

        Segments that have not completed are removed.

        self -- DASH manifest instance.
        segmentTemplate -- DASH SegmentTemplate to be modified
        periodStartTime -- Period start time in seconds
        currentPlayTime -- Stream time in seconds or None
        mediaPresentationDuration - Stream duration or None
        """
        if "timescale" in segmentTemplate.attrib:
            timescale = float(segmentTemplate.attrib.get("timescale"))
        else:
            timescale = 1.0

        if "presentationTimeOffset" in segmentTemplate.attrib:
            presentationTimeOffset = float(segmentTemplate.attrib.get("presentationTimeOffset"))
        else:
            presentationTimeOffset = 0.0

        for segmentTimeline in segmentTemplate:
            # Use xml.etree.ElementTree.findall() here to iterate over the
            # elements so that segments can be removed whilst iterating.
            segmentStartTime = periodStartTime
            for s in segmentTimeline.findall("./*"):
                # Use string.endswith() here to ignore any namespace.
                if s.tag.endswith("S"):
                    t = s.attrib.get("t")
                    d = s.attrib.get("d")
                    r = s.attrib.get("r")

                    if t is not None:
                        # t is in units of timescale.
                        segmentStartTime = periodStartTime + ((float(t) - presentationTimeOffset)/timescale)

                    if r is None:
                        segmentCount = 1
                    else:
                        segmentCount = int(r) + 1

                    if d:
                        # d is in units of timescale.
                        segmentDuration = float(d)/timescale
                    else:
                        # d should be specified, but default it to the timescale.
                        d = "%d" % (timescale)
                        segmentDuration = 1.0

                    firstSegmentIdx = None
                    lastSegmentIdx = None

                    for idx in range(segmentCount):
                        segmentEndTime = segmentStartTime + segmentDuration
                        if (mediaPresentationDuration is not None) and (segmentStartTime >= mediaPresentationDuration):
                            # This segment starts after the stream end.
                            # There are no more segments to check in this
                            # sequence, but do update the segment start time in
                            # case there are more sequences.
                            segmentStartTime = segmentEndTime
                            break
                        elif (currentPlayTime is not None) and (segmentEndTime > currentPlayTime):
                            # This segment is not yet available.
                            # There are no more segments to check in this
                            # sequence, but do update the segment start time in
                            # case there are more sequences with shorter
                            # durations.
                            segmentStartTime = segmentEndTime
                            break
                        else:
                            # This segment is available
                            if firstSegmentIdx is None:
                                firstSegmentIdx = idx
                            lastSegmentIdx = idx
                            segmentStartTime = segmentEndTime

                    if firstSegmentIdx is None:
                        # No segments from this element are available.
                        segmentTimeline.remove(s)
                    else:
                        # Modify the segments.
                        if t is not None:
                            s.set("t", t)
                        s.set("d", d)
                        if lastSegmentIdx > firstSegmentIdx:
                            # More than one segment.
                            s.set("r", "%d" % (lastSegmentIdx - firstSegmentIdx))
                        elif r is not None:
                            # Just one segment.
                            del s.attrib["r"]
                        else:
                            # No change.
                            pass

    def rewriteSegmentList(self, segmentList, periodStartTime, currentPlayTime, mediaPresentationDuration):
        """Modify a DASH SegmentList to emulate an ongoing event or live.

        Segments that have not completed are removed.

        self -- DASH manifest instance
        segmentList -- DASH SegmentList to be modified
        periodStartTime -- Period start time in seconds
        currentPlayTime -- Stream time in seconds or None
        mediaPresentationDuration - Stream duration or None
        """
        if "timescale" in segmentList.attrib:
            timescale = float(segmentList.attrib.get("timescale"))
        else:
            timescale = 1.0

        if "startNumber" in segmentList.attrib:
            firstSegmentIndex = int(segmentList.get("startNumber")) - 1
        else:
            firstSegmentIndex = 0

        if "presentationTimeOffset" in segmentList.attrib:
            presentationTimeOffset = float(segmentList.attrib.get("presentationTimeOffset"))
        else:
            presentationTimeOffset = 0.0

        if "duration" in segmentList.attrib:
            # The segment duration is in units of timescale.
            segmentDuration = float(segmentList.attrib.get("duration"))/timescale
        else:
            # Set a default duration here.
            segmentDuration = 1.0

        # Get the start time of the first segment in the list.
        segmentStartTime = periodStartTime + (float(firstSegmentIndex)*segmentDuration) - (presentationTimeOffset/timescale)

        # Use xml.etree.ElementTree.findall() here to iterate over the elements so
        # that segments can be removed from the list whilst iterating.
        for segmentURL in segmentList.findall("./*"):
            # Use string.endswith() here to ignore any namespace.
            if segmentURL.tag.endswith("SegmentURL"):
                segmentEndTime = segmentStartTime + segmentDuration
                if (mediaPresentationDuration is not None) and (segmentStartTime >= mediaPresentationDuration):
                    # This segment is after the stream end.
                    segmentList.remove(segmentURL)
                elif (currentPlayTime is not None) and (segmentEndTime > currentPlayTime):
                    # This segment is not yet available.
                    segmentList.remove(segmentURL)
                else:
                    # No change.
                    pass
                segmentStartTime = segmentEndTime

    def addNamespace(self, tag):
        """Rewrite an XML tag to handle any namespace prefix.

        self -- DASH manifest instance
        tag -- XML tag
        return Modified tag with namesapce
        """
        # "{([^}]+)}(.+)"
        m = self.nsAndTagPattern.match(tag)
        if m:
            ns = m.group(1)
            tag = m.group(2)
            if ns in self.namespaces:
                if len(self.namespaces[ns]):
                    tag = "%s:%s" % (self.namespaces[ns], tag)
            else:
                print("Namespace %s not recognized" % (ns))
        return tag

    def toString(self, element, depth = 0):
        """Convert a DASH element to a formatted string.

        xml.etree.ElementTree.indent() is available in newer versions of Python.

        self -- DASH manifest instance
        element -- XML element
        depth -- Recursive element depth
        """
        if depth == 0:
            value = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        else:
            value = self.indent * depth

        tag = self.addNamespace(element.tag)
        start = tag

        if depth == 0:
            # Add the namespaces to the root element.
            for ns in self.namespaces:
                prefix = self.namespaces[ns]
                if prefix == "":
                    start = start + " xmlns=\"%s\"" % (ns)
                else:
                    start = start + " xmlns:%s=\"%s\"" % (prefix, ns)

        for attribute in element.attrib:
            start = start + " %s=\"%s\"" % (self.addNamespace(attribute), element.get(attribute))

        # Strip element text
        text = element.text
        if text:
            text = text.strip()

        if len(element):
            # Element has sub-elements
            value += "<%s>\n" % (start)
            for subelement in element:
                value += self.toString(subelement, depth+1)
            value += "%s</%s>\n" % (self.indent*depth, tag)
        elif text:
            # Element has text
            value += "<%s>%s</%s>\n" % (start, text, tag)
        else:
            # Short element
            value += "<%s />\n" % (start)
        return value

    def rewriteManifest(self, params, currentTime, availabilityStartTime):
        """Modify a DASH manifest emulating an ongoing event or live or setting
        a maximum duration.

        self -- DASH manifest instance
        params -- Server parameters
        currentTime -- Current time
        availabilityStartTime -- Origin of stream time
        return Modified DASH manifest as a formatted string
        """
        if params.streamType == StreamType.VOD:
            currentPlayTime = None
        else:
            currentPlayTime = currentTime - availabilityStartTime

        # Modify the MPD attributes.
        if params.streamType != StreamType.VOD:
            self.root.set('type', "dynamic")
            self.root.set('availabilityStartTime', datetime.fromtimestamp(availabilityStartTime).astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
            self.root.set('minimumUpdatePeriod', "PT%dS" % (params.minimumUpdatePeriod))
            if params.streamType == StreamType.LIVE:
                self.root.set('timeShiftBufferDepth', "PT%dS" % (params.liveWindow))

        # Get the stream duration.
        if "mediaPresentationDuration" in self.root.attrib:
            mediaPresentationDuration = self.getDuration(self.root.get("mediaPresentationDuration"))
        else:
            mediaPresentationDuration = None

        # Update the duration if required.
        if (params.maximumDuration > 0.0) and (params.streamType == StreamType.VOD):
            if (mediaPresentationDuration is None) or (mediaPresentationDuration > params.maximumDuration):
                mediaPresentationDuration = params.maximumDuration
                if mediaPresentationDuration < 60.0:
                    self.root.set('mediaPresentationDuration', "PT%.3fS" % (mediaPresentationDuration))
                else:
                    minutes = int(mediaPresentationDuration/60.0)
                    seconds = mediaPresentationDuration - float(minutes*60)
                    self.root.set('mediaPresentationDuration', "PT%dM%.3fS" % (minutes, seconds))

        if ("mediaPresentationDuration" in self.root.attrib) and (params.streamType != StreamType.VOD):
            # Remove the mediaPresentationDuration attribute for non-VOD assets
            del(self.root.attrib["mediaPresentationDuration"])

        # Record the period start times.
        periodStartTimes = []
        for period in self.root:
            # Use string.endswith() here to ignore any namespace.
            if period.tag.endswith("Period"):
                if "start" in period.attrib:
                    periodStartTimes.append(self.getDuration(period.get("start")))
                else:
                    print("No start in period")
                    periodStartTimes.append(0.0)
        # Use None here to indicate an unknown end of the last period.
        periodStartTimes.append(mediaPresentationDuration)

        # Use xml.etree.ElementTree.findall() here to iterate over the
        # elements so that periods can be removed whilst iterating.
        periodIdx = 0
        for period in self.root.findall("./*"):
            # Use string.endswith() here to ignore any namespace.
            if period.tag.endswith("Period"):
                periodStartTime = periodStartTimes[periodIdx]
                periodEndTime = periodStartTimes[periodIdx + 1]

                # Remove unavailable periods.
                if (mediaPresentationDuration is not None) and (periodStartTime >= mediaPresentationDuration):
                    # Remove periods starting after the end of the stream.
                    self.root.remove(period)
                    periodIdx += 1
                    continue
                elif (params.streamType != StreamType.VOD) and (periodStartTime >= currentPlayTime):
                    # Remove periods starting after the current time.
                    self.root.remove(period)
                    periodIdx += 1
                    continue
                elif params.streamType == StreamType.LIVE:
                    if (periodEndTime is not None) and (periodEndTime <= (currentPlayTime - params.liveWindow)):
                        # In live mode, remove periods ending before the live
                        # window.
                        self.root.remove(period)
                        periodIdx += 1
                        continue
                else:
                    pass

                # Rewrite the segments in the period
                for adaptationSet in period:
                    for representation in adaptationSet:
                        for element in representation:
                            # Use string.endswith() here to ignore any
                            # namespace.
                            if element.tag.endswith("SegmentTemplate"):
                                self.rewriteSegmentTemplate(element, periodStartTime, currentPlayTime, mediaPresentationDuration)
                            elif element.tag.endswith("SegmentList"):
                                self.rewriteSegmentList(element, periodStartTime, currentPlayTime, mediaPresentationDuration)
                            else:
                                pass

                periodIdx += 1
        return self.toString(self.root)

class ServerParams():
    """Server parameter class.

    Default values are set here and can be modified on the server commandline.
    Parameters for individual requests can be set in the URI. For example:

    http://localhost:8080/main.mpd?live=true

    Use copy.copy() to take a copy of the parameters.
    """
    minTime = 10.0
    liveWindow = 30.0
    minimumUpdatePeriod = 6.0
    maximumDuration = 0.0
    streamType = StreamType.VOD
    addPDTTags = False
    addDiscontinuities = False
    getAll = False

class TestServer(BaseHTTPRequestHandler):
    """Test webserver class."""

    startTime = time.time()
    params = ServerParams()

    def getEventHLSPlaylist(self, path, params):
        """Get a video test stream HLS playlist modified to emulate an ongoing
        event or live.

        Event media playlists are truncated based on the time since the server
        was started. Live media playlists contain a window of segments based on
        the time since the server was started. Master playlists are unmodified.

        self -- Instance
        path -- Playlist file path
        params -- Server parameters
        """

        currentTime = time.time()
        currentPlayTime = (currentTime - self.startTime) + params.minTime
        segmentTime = 0.0
        extXTargetDurationPattern = re.compile(r"^#EXT-X-TARGETDURATION:([\d\.]+).*")
        extXPlaylistTypePattern = re.compile(r"^#EXT-X-PLAYLIST-TYPE:.*")
        extinfPattern = re.compile(r"^#EXTINF:([\d\.]+),.*")
        extXMediaSequencePattern = re.compile(r"^#EXT-X-MEDIA-SEQUENCE:.*")
        mediaUrlPattern = re.compile(r"^[^#\s]")
        skipSegment = False
        targetDuration = 0.0
        totalDuration = 0.0
        firstSegment = True
        sequenceNumber = 0

        # Get the target and total durations
        with open(path, "r") as f:
            for line in f:
                # "^#EXTINF:([\d\.]+),.*"
                m = extinfPattern.match(line)
                if m:
                    # Extract the segment duration.
                    totalDuration += float(m.group(1))
                    continue

                # "^#EXT-X-TARGETDURATION:([\d\.]+).*"
                m = extXTargetDurationPattern.match(line)
                if m:
                    # Extract the target duration.
                    targetDuration = float(m.group(1))
                    continue

        # Get a modified version of the playlist
        with open(path, "r") as f:
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

            for line in f:
                # "^#EXTINF:([\d\.]+),.*"
                m = extinfPattern.match(line)
                if m:
                    # Extract the segment duration.
                    segmentDuration = float(m.group(1))

                    # Truncate the playlist if the next segment ends after the
                    # current playlist time.
                    if currentPlayTime < (segmentTime + segmentDuration):
                        break

                    # In live emulation, skip segments outside the live window
                    # unless the total duration would be less than three times
                    # the target duration.
                    totalDuration -= segmentDuration
                    if ((params.streamType == StreamType.LIVE) and
                       (currentPlayTime >= (segmentTime + segmentDuration + params.liveWindow)) and
                       (totalDuration >= (targetDuration*3.0))):
                        skipSegment = True
                        segmentTime += segmentDuration
                        continue
                    else:
                        skipSegment = False

                    # If this is the first segment to be emitted, then emit the
                    # EXT-X-MEDIA-SEQUENCE and EXT-X-DISCONTINUITY-SEQUENCE
                    # tags.
                    if firstSegment:
                        firstSegment = False
                        self.wfile.write(bytes("#EXT-X-MEDIA-SEQUENCE:%d\n" % (sequenceNumber), "utf-8"))

                        if params.addDiscontinuities:
                            # Segment 0 has no EXT-X-DISCONTINUITY flag. If
                            # segment 0 is removed from the playlist, don't
                            # increment the EXT-X-DISCONTINUITY-SEQUENCE value.
                            if sequenceNumber == 0:
                                self.wfile.write(bytes("#EXT-X-DISCONTINUITY-SEQUENCE:0\n", "utf-8"))
                            else:
                                self.wfile.write(bytes("#EXT-X-DISCONTINUITY-SEQUENCE:%d\n" % (sequenceNumber - 1), "utf-8"))

                    if params.addDiscontinuities and sequenceNumber > 0:
                        # Segment 0 has no EXT-X-DISCONTINUITY flag.
                        self.wfile.write(bytes("#EXT-X-DISCONTINUITY\n", "utf-8"))

                    if params.addPDTTags:
                        # Add program date time tag
                        timestring = datetime.fromtimestamp(self.startTime + segmentTime).astimezone().isoformat(timespec='milliseconds')
                        self.wfile.write(bytes("#EXT-X-PROGRAM-DATE-TIME:" + timestring + "\n", "utf-8"))

                    segmentTime += segmentDuration
                # "^#EXT-X-PLAYLIST-TYPE:.*"
                elif extXPlaylistTypePattern.match(line):
                    # Skip or replace the playlist type tag
                    if params.streamType == StreamType.EVENT:
                        line = "#EXT-X-PLAYLIST-TYPE:EVENT\n"
                    else:
                        continue
                # "^#EXT-X-MEDIA-SEQUENCE:.*"
                elif extXMediaSequencePattern.match(line):
                    # Delay emitting the EXT-X-MEDIA-SEQUENCE tag until the
                    # first segment is about to be emitted when we know what the
                    # first sequence number is.
                    continue
                # "^[^#\s]"
                elif mediaUrlPattern.match(line):
                    # Media segment URI.
                    sequenceNumber += 1
                    if skipSegment:
                        skipSegment = False
                        continue

                self.wfile.write(bytes(line, "utf-8"))

    def getDASHManifest(self, path, params):
        """Get a test stream DASH manifest optionally modified to emulate live
        content, an ongoing event or limited in duration.

        In live and event mode, later segments are removed from the manifest
        based on the time since the server was started. In live mode, only
        periods within a live window are published.

        self -- Instance
        path -- Manifest file path
        params -- Server parameters
        """

        currentTime = time.time()
        availabilityStartTime = self.startTime - params.minTime

        manifest = DASHManifest()

        # Record namespaces here from the DASH manifest to ensure that these are
        # generated correctly when the modified XML is written.
        ns_dict = dict(
            [node for _, node in ET.iterparse(path, events=["start-ns"])]
        )

        for key in ns_dict:
            manifest.namespaces[ns_dict[key]] = key
            ET.register_namespace(key, ns_dict[key])

        # Parse the DASH manifest
        tree = ET.parse(path)

        # Rewrite the DASH manifest in live and event mode
        manifest.root = tree.getroot()
        manifest.rewriteManifest(params, currentTime, availabilityStartTime)

        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(bytes(manifest.toString(manifest.root), "utf-8"))

    def getFile(self, path):
        """Get a file.

        self -- Instance
        path -- File path
        """

        with open(path, "rb") as f:
            contents = f.read()
            self.send_response(200)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(contents)

    def do_GET(self):
        """Get request.

        self -- Instance
        """

        # Extract the relative path, file extension and parameters
        keyEqualsValuePattern = re.compile(r"(\w+)=(\w+)")
        params = copy.copy(self.params)
        up = urlparse(self.path)
        path = up.path[1:]
        filename, extension = os.path.splitext(path)
        if up.query != "":
            for query in up.query.split("&"):
                m = keyEqualsValuePattern.match(query)
                if m:
                    key = m.group(1)
                    value = m.group(2)
                    if key == "mintime":
                        params.minTime = float(value)
                    elif key == "livewindow":
                        params.liveWindow = float(value)
                    elif key == "minupdate":
                        params.minimumUpdatePeriod = float(value)
                    elif key == "maxduration":
                        params.maximumDuration = float(value)
                    elif key == "vod":
                        if getBoolean(key, value, False):
                            params.streamType = StreamType.VOD
                    elif key == "event":
                        if getBoolean(key, value, False):
                            params.streamType = StreamType.EVENT
                    elif key == "live":
                        if getBoolean(key, value, False):
                            params.streamType = StreamType.LIVE
                    elif key == "time":
                        params.addPDTTags = getBoolean(key, value, params.addPDTTags)
                    elif key == "discontinuity":
                        params.addDiscontinuities = getBoolean(key, value, params.addDiscontinuities)
                    elif key == "all":
                        params.getAll = getBoolean(key, value, params.getAll)
                    else:
                        print("Ignoring parameter %s" % (key))
                else:
                    print("Ignoring parameter %s" % (query))

        try:
            if extension == ".m3u8":
                # HLS playlist
                if params.streamType == StreamType.VOD:
                    self.getFile(path)
                else:
                    self.getEventHLSPlaylist(path, params)
            elif extension == ".mpd":
                # DASH manifest
                if (params.streamType == StreamType.VOD) and (params.maximumDuration <= 0.0):
                    self.getFile(path)
                else:
                    self.getDASHManifest(path, params)
            elif params.getAll:
                # Get all files
                self.getFile(path)
            elif extension == ".m4s":
                # fMP4 segment
                self.getFile(path)
            elif extension == ".mp4":
                # MP4 segment
                self.getFile(path)
            elif extension == ".ts":
                # MPEG TS segment
                self.getFile(path)
            elif extension == ".mp3":
                # MP3 audio
                self.getFile(path)
            elif extension == ".vtt":
                # WebVTT text
                self.getFile(path)
            else:
                self.send_response(404)
                self.end_headers()

        except FileNotFoundError:
            self.send_response(404)
            self.end_headers()

if __name__ == "__main__":
    hostName = "localhost"
    serverPort = 8080

    # Parse the command line arguments.
    parser = argparse.ArgumentParser(description="AAMP video test stream HTTP server")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--vod", action="store_true", help="VOD test stream (default)")
    group.add_argument("--event", action="store_true", help="emulate an event test stream")
    group.add_argument("--live", action="store_true", help="emulate a live test stream")
    parser.add_argument("--time", action="store_true", help="add EXT-X-PROGRAM-DATE-TIME tags to HLS (or live) event playlists (enabled for live)")
    parser.add_argument("--discontinuity", action="store_true", help="add EXT-X-DISCONTINUITY tags to HLS event playlists")
    parser.add_argument("--hostname", help="server socket host name (default %s)" % (hostName))
    parser.add_argument("--port", type=int, help="HTTP server port number")
    parser.add_argument("--mintime", type=float, help="starting event (or live) duration in seconds (default %d)" % (TestServer.params.minTime))
    parser.add_argument("--livewindow", type=float, help="live window in seconds (default %d)" % (TestServer.params.liveWindow))
    parser.add_argument("--minupdate", type=float, help="minimum update period in seconds for DASH live or event (default %d)" % (TestServer.params.minimumUpdatePeriod))
    parser.add_argument("--maxduration", type=float, help="maximum duration of the stream in seconds (DASH only, default %d for no maximum)" % (TestServer.params.maximumDuration))
    parser.add_argument("--all", action="store_true", help="enable GET of all files. By default, only files with expected extensions will be served")
    args = parser.parse_args()

    if args.event:
        TestServer.params.streamType = StreamType.EVENT

    if args.live:
        TestServer.params.streamType = StreamType.LIVE
        TestServer.params.addPDTTags = True

    if args.time:
        TestServer.params.addPDTTags = True

    if args.discontinuity:
        TestServer.params.addDiscontinuities = True

    if args.hostname:
        hostName = args.hostname

    if args.port:
        serverPort = args.port

    if args.mintime:
        TestServer.params.minTime = args.mintime

    if args.livewindow:
        TestServer.params.liveWindow = args.livewindow

    if args.minupdate:
        TestServer.params.minimumUpdatePeriod = args.minupdate

    if args.maxduration:
        TestServer.params.maximumDuration = args.maxduration

    if args.all:
        TestServer.params.getAll = True

    # Create and run the HTTP server.
    testServer = HTTPServer((hostName, serverPort), TestServer)
    print("Server started http://%s:%s" % (hostName, serverPort))

    try:
        testServer.serve_forever()
    except KeyboardInterrupt:
        pass

    testServer.server_close()
    print("Server stopped.")

