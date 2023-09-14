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

import xml.etree.ElementTree as ET
import re
import copy
import sys

class DASHManifest():
    """Helper class for merging DASH manifests.
    """

    nsAndTagPattern = re.compile(r"{([^}]+)}(.+)")
    durationPattern = re.compile(r"PT([\d\.]+)S")
    indent = "  "

    def __init__(self):
        """Constructor.

        self -- Instance
        """
        self.namespaces = dict()

    def addNamespace(self, tag):
        """Rewrite an XML tag to handle any namespace prefix.

        self -- DASH manifest instance
        tag -- XML tag
        return Modified tag with namespace
        """
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
        return a formatted string
        """
        if depth == 0:
            value = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        else:
            value = self.indent * depth

        tag = self.addNamespace(element.tag)
        start = tag
        for attribute in element.attrib:
            start = start + " %s=\"%s\"" % (self.addNamespace(attribute), element.get(attribute))

        if depth == 0:
            # Add the namespaces to the root element.
            for ns in self.namespaces:
                prefix = self.namespaces[ns]
                if prefix == "":
                    start = start + " xmlns=\"%s\"" % (ns)
                else:
                    start = start + " xmlns:%s=\"%s\"" % (prefix, ns)

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

    def parse(self, path):
        """Parse a DASH manifest file.

        self -- DASH manifest instance
        path -- File object
        """
        # Record namespaces here from the DASH manifest to ensure that these are
        # generated correctly when the modified XML is written.
        ns_dict = dict(
            [node for _, node in ET.iterparse(path, events=["start-ns"])]
        )

        for key in ns_dict:
            self.namespaces[ns_dict[key]] = key
            ET.register_namespace(key, ns_dict[key])

        # Parse the DASH manifest
        self.tree = ET.parse(path)
        self.root = self.tree.getroot()

    def getPeriodStartTimes(self):
        """Record the period start times.

        An extra element set to None is added to the end of the array.

        self -- DASH manifest instance
        return array of period start times
        """
        periodStartTimes = []
        for period in self.root:
            # Use string.endswith() here to ignore any namespace.
            if period.tag.endswith("Period"):
                if "start" in period.attrib:
                    # Format "PT%fs".
                    start = period.get("start")
                    m = self.durationPattern.match(start)
                    if m:
                        periodStartTimes.append(float(m.group(1)))
                    else:
                        print("Unsupported period time format %s" % start)
                        periodStartTimes.append(0.0)
                else:
                    periodStartTimes.append(0.0)

        # Append None here to indicate the end of the last period.
        periodStartTimes.append(None)
        return periodStartTimes

    def mergeSegmentTemplate(self, targetSegmentTemplate, targetPeriodStartTime, targetPeriodEndTime, sourceSegmentTemplate, sourcePeriodStartTime, sourcePeriodEndTime):
        """Merge a SegmentTemplate element from a second DASH manifest.

        self -- DASH manifest instance
        targetSegmentTemplate -- Output SegmentTemplate element
        targetPeriodStartTime -- Start time of target period in seconds
        targetPeriodEndTime -- End time of target period in seconds or None
        sourceSegmentTemplate -- Second SegmentTemplate element
        sourcePeriodStartTime -- Start time of second manifest period in seconds
        sourcePeriodEndTime -- End time of second manifest period in seconds or None
        """
        for a in sourceSegmentTemplate.attrib:
            targetSegmentTemplate.set(a, sourceSegmentTemplate.get(a))

        if "timescale" in sourceSegmentTemplate.attrib:
            timescale = int(sourceSegmentTemplate.get("timescale"))
        else:
            timescale = 1

        if "startNumber" in sourceSegmentTemplate.attrib:
            startNumber = int(sourceSegmentTemplate.get("startNumber"))
        else:
            startNumber = 1

        if "duration" in sourceSegmentTemplate.attrib:
            duration = int(sourceSegmentTemplate.get("duration"))
        else:
            # Set a default duration equal to the timescale.
            duration = timescale

        if "presentationTimeOffset" in sourceSegmentTemplate.attrib:
            presentationTimeOffset = int(sourceSegmentTemplate.get("presentationTimeOffset"))
        else:
            presentationTimeOffset = 0

        # The SegmentTemplate may or may not have a SegmentTimeline element.
        targetSegmentTimeline = None
        for element in sourceSegmentTemplate:
            if element.tag.endswith("SegmentTimeline"):
                # Copy the tag and attributes.
                targetSegmentTimeline = ET.Element(element.tag)
                for a in element.attrib:
                    targetSegmentTimeline.set(a, element.get(a))

                # Iterate through the subelements.
                segmentStartTimeTicks = 0
                segmentStartTime = sourcePeriodStartTime - (float(presentationTimeOffset)/float(timescale))
                tIsNeeded = True
                overallSegmentIdx = 0
                firstOverallSegmentIdx = None
                for s in element:
                    t = s.get("t")
                    d = s.get("d")
                    r = s.get("r")

                    if t is not None:
                        segmentStartTimeTicks = int(t)
                        segmentStartTime = sourcePeriodStartTime + (float(segmentStartTimeTicks - presentationTimeOffset)/float(timescale))

                    if r is not None:
                        segmentCount = int(r) + 1
                    else:
                        segmentCount = 1

                    if d is not None:
                        segmentDurationTicks = int(d)
                    else:
                        segmentDurationTicks = duration

                    # Iterate through the segments to see which are inside the
                    # target period.
                    firstSegmentIdx = None
                    lastSegmentIdx = None
                    for segmentIdx in range(segmentCount):
                        segmentEndTimeTicks = segmentStartTimeTicks + segmentDurationTicks
                        segmentEndTime = sourcePeriodStartTime + (float(segmentEndTimeTicks - presentationTimeOffset)/float(timescale))
                        if (targetPeriodEndTime is not None) and (segmentStartTime >= targetPeriodEndTime):
                            # Segment is after the target period.
                            # Update the segment start time here in case there
                            # are other segment sequences.
                            segmentStartTimeTicks = segmentEndTimeTicks
                            segmentStartTime = segmentEndTime
                            break
                        elif segmentEndTime <= targetPeriodStartTime:
                            # Segment is before the period.
                            pass
                        else:
                            # Segment is in the period.
                            if firstSegmentIdx is None:
                                firstSegmentIdx = segmentIdx
                                if tIsNeeded:
                                    t = str(segmentStartTimeTicks)
                            if firstOverallSegmentIdx is None:
                                firstOverallSegmentIdx = overallSegmentIdx
                            lastSegmentIdx = segmentIdx
                        segmentStartTimeTicks = segmentEndTimeTicks
                        segmentStartTime = segmentEndTime
                        overallSegmentIdx += 1

                    if firstSegmentIdx is not None:
                        # Add a modified subelement.
                        targetS = ET.Element(s.tag)
                        if tIsNeeded:
                            targetS.set("t", t)
                            tIsNeeded = False
                        targetS.set("d", d)
                        if lastSegmentIdx > firstSegmentIdx:
                            targetS.set("r", "%d" % (lastSegmentIdx - firstSegmentIdx))
                        targetSegmentTimeline.append(targetS)

                if firstOverallSegmentIdx is not None:
                    # Update the first segment number
                    startNumber += firstOverallSegmentIdx

                targetSegmentTemplate.append(targetSegmentTimeline)

        if targetSegmentTimeline is None:
            # No SegmentTimeline. Iterate throgh the segments to find the first
            # segment in the target period.
            segmentStartTimeTicks = (startNumber - 1)*duration
            segmentStartTime = sourcePeriodStartTime + (float(segmentStartTimeTicks - presentationTimeOffset)/float(timescale))
            segmentCount = 0
            while True:
                segmentEndTimeTicks = segmentStartTimeTicks + duration
                segmentEndTime = sourcePeriodStartTime + (float(segmentEndTimeTicks - presentationTimeOffset)/float(timescale))
                if (targetPeriodEndTime is not None) and (segmentStartTime >= targetPeriodEndTime):
                    # Segment is after the target period.
                    break
                elif segmentEndTime <= targetPeriodStartTime:
                    # Segment is before the period.
                    segmentCount += 1
                else:
                    # This segment is in the period.
                    break
                segmentStartTimeTicks = segmentEndTimeTicks
                segmentStartTime = segmentEndTime

            # Update the startNumber.
            if segmentCount > 0:
                startNumber += segmentCount

        # Set the target presentationTimeOffset
        targetPresentationTimeOffset = presentationTimeOffset + ((targetPeriodStartTime - sourcePeriodStartTime)*float(timescale))
        if targetPresentationTimeOffset > 0.0:
            targetSegmentTemplate.set("presentationTimeOffset", "%d" % (targetPresentationTimeOffset))

        # Set the startNumber.
        targetSegmentTemplate.set("startNumber", str(startNumber))

    def mergeSegmentList(self, targetSegmentList, targetPeriodStartTime, targetPeriodEndTime, sourceSegmentList, sourcePeriodStartTime, sourcePeriodEndTime):
        """Merge a SegmentList element from a second DASH manifest.

        self -- DASH manifest instance
        targetSegmentList -- Output SegmentList element
        targetPeriodStartTime -- Start time of target period in seconds
        targetPeriodEndTime -- End time of target period in seconds or None
        sourceSegmentList -- Second SegmentList element
        sourcePeriodStartTime -- Start time of second manifest period in seconds
        sourcePeriodEndTime -- End time of second manifest period in seconds or None
        """
        for a in sourceSegmentList.attrib:
            targetSegmentList.set(a, sourceSegmentList.get(a))

        if "timescale" in sourceSegmentList.attrib:
            timescale = int(sourceSegmentList.get("timescale"))
        else:
            timescale = 1

        if "duration" in sourceSegmentList.attrib:
            segmentDurationTicks = int(sourceSegmentList.get("duration"))
        else:
            # Duration should be present, but set a default
            segmentDurationTicks = timescale

        if "startNumber" in sourceSegmentList.attrib:
            startNumber = int(sourceSegmentList.get("startNumber"))
        else:
            startNumber = 1

        if "presentationTimeOffset" in sourceSegmentList.attrib:
            presentationTimeOffset = int(sourceSegmentList.get("presentationTimeOffset"))
        else:
            presentationTimeOffset = 0

        # Iterate though the SegmentURL elements to determine which segments are
        # in the target period.
        overallSegmentIdx = 0
        firstOverallSegmentIdx = None
        segmentStartTimeTicks = (startNumber - 1)*segmentDurationTicks
        segmentStartTime = sourcePeriodStartTime + (float(segmentStartTimeTicks - presentationTimeOffset)/float(timescale))
        for element in sourceSegmentList:
            if element.tag.endswith("SegmentURL"):
                segmentEndTimeTicks = segmentStartTimeTicks + segmentDurationTicks
                segmentEndTime = sourcePeriodStartTime + (float(segmentEndTimeTicks - presentationTimeOffset)/float(timescale))
                if (targetPeriodEndTime is not None) and (segmentStartTime >= targetPeriodEndTime):
                    # Segment is after the target period.
                    break
                elif segmentEndTime <= targetPeriodStartTime:
                    # Segment is before the period.
                    pass
                else:
                    # Segment is in the period.
                    targetSegmentList.append(copy.deepcopy(element))

                    if firstOverallSegmentIdx is None:
                        # Update the startNumber
                        firstOverallSegmentIdx = overallSegmentIdx
                        targetSegmentList.set("startNumber", str(startNumber + firstOverallSegmentIdx))
                segmentStartTimeTicks = segmentEndTimeTicks
                segmentStartTime = segmentEndTime
                overallSegmentIdx += 1
            else:
                # Copy this element.
                targetSegmentList.append(copy.deepcopy(element))

            # Set the target presentationTimeOffset.
            targetPresentationTimeOffset = presentationTimeOffset + ((targetPeriodStartTime - sourcePeriodStartTime)*float(timescale))
            if targetPresentationTimeOffset > 0.0:
                targetSegmentList.set("presentationTimeOffset", "%d" % (targetPresentationTimeOffset))

    def mergeRepresentation(self, targetRepresentation, targetPeriodStartTime, targetPeriodEndTime, sourceRepresentation, sourcePeriodStartTime, sourcePeriodEndTime):
        """Merge a Representation element from a second DASH manifest.

        self -- DASH manifest instance
        targetRepresentation -- Output Representation element
        targetPeriodStartTime -- Start time of target period in seconds
        targetPeriodEndTime -- End time of target period in seconds or None
        sourceRepresentation -- Second Representation element
        sourcePeriodStartTime -- Start time of second manifest period in seconds
        sourcePeriodEndTime -- End time of second manifest period in seconds or None
        """
        # Copy attributes.
        for a in sourceRepresentation.attrib:
            targetRepresentation.set(a, sourceRepresentation.get(a))

        # Iterate the subelements.
        for element in sourceRepresentation:
            if element.tag.endswith("SegmentTemplate"):
                # Merge the SegmentTemplate.
                targetSegmentTemplate = ET.Element(element.tag)
                self.mergeSegmentTemplate(targetSegmentTemplate, targetPeriodStartTime, targetPeriodEndTime, element, sourcePeriodStartTime, sourcePeriodEndTime)
                targetRepresentation.append(targetSegmentTemplate)
            elif element.tag.endswith("SegmentList"):
                # Merge the SegmentList.
                targetSegmentList = ET.Element(element.tag)
                self.mergeSegmentList(targetSegmentList, targetPeriodStartTime, targetPeriodEndTime, element, sourcePeriodStartTime, sourcePeriodEndTime)
                targetRepresentation.append(targetSegmentList)
            else:
                # Copy this element.
                targetRepresentation.append(copy.deepcopy(element))

    def mergeAdaptationSet(self, targetAdaptationSet, targetPeriodStartTime, targetPeriodEndTime, sourceAdaptationSet, sourcePeriodStartTime, sourcePeriodEndTime):
        """Merge an AdaptationSet element from a second DASH manifest.

        self -- DASH manifest instance
        targetAdaptationSet -- Output AdaptationSet element
        targetPeriodStartTime -- Start time of target period in seconds
        targetPeriodEndTime -- End time of target period in seconds or None
        sourceAdaptationSet -- Second AdaptationSet element
        sourcePeriodStartTime -- Start time of second manifest period in seconds
        sourcePeriodEndTime -- End time of second manifest period in seconds or None
        """
        # Copy attributes.
        for a in sourceAdaptationSet.attrib:
            targetAdaptationSet.set(a, sourceAdaptationSet.get(a))

        # Iterate the subelements.
        for element in sourceAdaptationSet:
            if element.tag.endswith("Representation"):
                # Merge the Representation.
                targetRepresentation = ET.Element(element.tag)
                self.mergeRepresentation(targetRepresentation, targetPeriodStartTime, targetPeriodEndTime, element, sourcePeriodStartTime, sourcePeriodEndTime)
                targetAdaptationSet.append(targetRepresentation)
            else:
                # Copy this element.
                targetAdaptationSet.append(copy.deepcopy(element))

    def mergeEventStream(self, targetEventStream, targetPeriodStartTime, targetPeriodEndTime, sourceEventStream, sourcePeriodStartTime, sourcePeriodEndTime):
        """Merge an EventStream element from a second DASH manifest.

        self -- DASH manifest instance
        targetEventStream -- Output EventStream element
        targetPeriodStartTime -- Start time of target period in seconds
        targetPeriodEndTime -- End time of target period in seconds or None
        sourceEventStream -- Second EventStream element
        sourcePeriodStartTime -- Start time of second manifest period in seconds
        sourcePeriodEndTime -- End time of second manifest period in seconds or None
        """
        # Copy attributes.
        for a in sourceEventStream.attrib:
            targetEventStream.set(a, sourceEventStream.get(a))

        if "timescale" in sourceEventStream.attrib:
            timescale = float(sourceEventStream.get("timescale"))
        else:
            timescale = 90000.0

        if "presentationTimeOffset" in sourceEventStream.attrib:
            presentationTimeOffset = int(sourceEventStream.get("presentationTimeOffset"))
        else:
            presentationTimeOffset = 0

        # Iterate through the events to see which are in the target period.
        for event in sourceEventStream:
            if "presentationTime" in event.attrib:
                presentationTime = sourcePeriodStartTime + (float(int(event.get("presentationTime")) - presentationTimeOffset)/timescale)
                if presentationTime < targetPeriodStartTime:
                    # The event is before the period.
                    pass
                elif (targetPeriodEndTime is not None) and (presentationTime >= targetPeriodEndTime):
                    # The event is after the period.
                    pass
                else:
                    # Copy this event
                    targetEventStream.append(copy.deepcopy(event))

        # Set the target presentationTimeOffset.
        targetPresentationTimeOffset = presentationTimeOffset + ((targetPeriodStartTime - sourcePeriodStartTime)*float(timescale))
        if targetPresentationTimeOffset > 0.0:
            targetEventStream.set("presentationTimeOffset", "%d" % (targetPresentationTimeOffset))

    def mergePeriod(self, targetPeriod, targetPeriodStartTime, targetPeriodEndTime, sourcePeriod, sourcePeriodStartTime, sourcePeriodEndTime):
        """Merge a Period element from a second DASH manifest.

        self -- DASH manifest instance
        targetPeriod -- Output Period element
        targetPeriodStartTime -- Start time of target period in seconds
        targetPeriodEndTime -- End time of target period in seconds or None
        sourcePeriod -- Second Period element
        sourcePeriodStartTime -- Start time of second manifest period in seconds
        sourcePeriodEndTime -- End time of second manifest period in seconds or None
        """
        for element in sourcePeriod:
            if element.tag.endswith("AdaptationSet"):
                # Merge the AdaptionSet.
                targetAdaptationSet = ET.Element(element.tag)
                self.mergeAdaptationSet(targetAdaptationSet, targetPeriodStartTime, targetPeriodEndTime, element, sourcePeriodStartTime, sourcePeriodEndTime)
                targetPeriod.append(targetAdaptationSet)
            elif element.tag.endswith("EventStream"):
                # Merge the EventStream.
                targetEventStream = ET.Element(element.tag)
                self.mergeEventStream(targetEventStream, targetPeriodStartTime, targetPeriodEndTime, element, sourcePeriodStartTime, sourcePeriodEndTime)
                targetPeriod.append(targetEventStream)
            else:
                # Ignore this element.
                print("Ignoring period element %s", self.addNamespace(element.tag))

    def merge(self, second):
        """Merge two DASH manifests into a third manifest.

        self -- DASH manifest instance
        second -- Second DASH manifest instance
        return merged DASH manifest instance
        """
        third = DASHManifest()

        # Merge namespaces.
        third.namespaces["urn:mpeg:dash:schema:mpd:2011"] = ""
        for ns in self.namespaces:
            third.namespaces[ns] = self.namespaces[ns]

        for ns in second.namespaces:
            third.namespaces[ns] = second.namespaces[ns]

        # Merge the root element.
        third.root = ET.Element("{urn:mpeg:dash:schema:mpd:2011}MPD")
        for a in self.root.attrib:
            third.root.set(a, self.root.get(a))

        # Merge elements other than periods.
        for element in self.root:
            if not element.tag.endswith("Period"):
                third.root.append(copy.deepcopy(element))

        for element in second.root:
            if not element.tag.endswith("Period"):
                third.root.append(copy.deepcopy(element))

        # Merge period start times.
        periodStartTime = -1
        selfPeriodStartTimes = self.getPeriodStartTimes()
        secondPeriodStartTimes = second.getPeriodStartTimes()
        thirdPeriodStartTimes = []

        while True:
            # Find the next period start time in this manifest.
            selfPeriodStartTime = None
            for startTime in selfPeriodStartTimes:
                if startTime is None:
                    break
                elif startTime > periodStartTime:
                    selfPeriodStartTime = startTime
                    break
                else:
                    pass

            # Find the next period start time in the second manifest.
            secondPeriodStartTime = None
            for startTime in secondPeriodStartTimes:
                if startTime is None:
                    break
                elif startTime > periodStartTime:
                    secondPeriodStartTime = startTime
                    break
                else:
                    pass

            # Choose the next period start time.
            if selfPeriodStartTime is None and secondPeriodStartTime is None:
                # End of manifests. Add None to signal the end of the period
                # start time list.
                thirdPeriodStartTimes.append(None)
                break
            elif selfPeriodStartTime is None:
                periodStartTime = secondPeriodStartTime
            elif secondPeriodStartTime is None:
                periodStartTime = selfPeriodStartTime
            elif selfPeriodStartTime <= secondPeriodStartTime:
                periodStartTime = selfPeriodStartTime
            else:
                periodStartTime = secondPeriodStartTime
            thirdPeriodStartTimes.append(periodStartTime)

        # Merge the manifest periods.
        for thirdPeriodIdx, thirdPeriodStartTime in enumerate(thirdPeriodStartTimes):
            if thirdPeriodStartTime is not None:
                thirdPeriodEndTime = thirdPeriodStartTimes[thirdPeriodIdx + 1]
                thirdPeriod = ET.Element("{urn:mpeg:dash:schema:mpd:2011}Period")
                thirdPeriod.set("id", "%d" % (thirdPeriodIdx))
                thirdPeriod.set("start", "PT%.3fS" % (thirdPeriodStartTime))

                selfPeriodIdx = 0
                for selfPeriod in self.root:
                    if selfPeriod.tag.endswith("Period"):
                        selfPeriodStartTime = selfPeriodStartTimes[selfPeriodIdx]
                        selfPeriodEndTime = selfPeriodStartTimes[selfPeriodIdx + 1]
                        if (selfPeriodEndTime is not None) and (selfPeriodEndTime <= thirdPeriodStartTime):
                            # This period has finished.
                            pass
                        elif (thirdPeriodEndTime is not None) and (selfPeriodStartTime >= thirdPeriodEndTime):
                            # This period has not started.
                            break
                        else:
                            # This period overlaps with the merged period.
                            third.mergePeriod(thirdPeriod, thirdPeriodStartTime, thirdPeriodEndTime, selfPeriod, selfPeriodStartTime, selfPeriodEndTime)
                        selfPeriodIdx += 1

                secondPeriodIdx = 0
                for secondPeriod in second.root:
                    if secondPeriod.tag.endswith("Period"):
                        secondPeriodStartTime = secondPeriodStartTimes[secondPeriodIdx]
                        secondPeriodEndTime = secondPeriodStartTimes[secondPeriodIdx + 1]
                        if (secondPeriodEndTime is not None) and (secondPeriodEndTime <= thirdPeriodStartTime):
                            # This period has finished.
                            pass
                        elif (thirdPeriodEndTime is not None) and (secondPeriodStartTime >= thirdPeriodEndTime):
                            # This period has not started.
                            break
                        else:
                            # This period overlaps with the merged period.
                            third.mergePeriod(thirdPeriod, thirdPeriodStartTime, thirdPeriodEndTime, secondPeriod, secondPeriodStartTime, secondPeriodEndTime)
                        secondPeriodIdx += 1

                third.root.append(thirdPeriod)
        return third

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python3 manifest.py merge <mpd1> <mpd2>")
        sys.exit()

    try:
        one = DASHManifest()
        one.parse(sys.argv[2])
        two = DASHManifest()
        two.parse(sys.argv[3])
        three = one.merge(two)
        print(three.toString(three.root))

    except FileNotFoundError as e:
        print("Failed to open file - %s" % (e))
