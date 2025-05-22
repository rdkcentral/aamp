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
//
// MPDSegmentExtractor.cpp

/**
 * @file MPDSegmenter.cpp
 * @brief
 */

#include "MPDSegmenter.h"
#include "../utils/Utils.h"
#include "../utils/Url.h"
//#include "../../DownloadHelper.h"
#include <cmath>
#include <unordered_map>
#include <functional>

#define BUFSIZE 8192

using namespace std;

/**
 * @typedef TemplateContext
 * @brief
 */
typedef std::map<std::string, std::string> TemplateContext;

/**
 * @brief   finds and replace "toString" in "str" string
 * @param   string to replace in
 * @param   from string
 * @param   string to replace
 * @retval  Returns True or False
 */
bool replaceStr(std::string& str, const std::string& from, const std::string& toString)
{
    size_t tokenLength = from.length();
    size_t pos = 0;
    for (;;)
    {
        pos = str.find('$', pos);
        if (pos == std::string::npos)
        {
            break;
        }
        size_t next = str.find('$', pos + 1);
        if (str.substr(pos + 1, tokenLength) == from)
        {
            str.replace(pos, tokenLength + 2, toString);
            return true;
        }
        pos = next + 1;
    }
    return false;
}

/**
 * @brief   decode Template
 * @param   string
 * @retval  replaced string
 */
string decodeTemplate(string &tpl, TemplateContext &ctx) {
    string out(tpl);
	for (auto it = ctx.begin(); it != ctx.end(); it++) {
		replaceStr(out,it->first,it->second);
	}
    return out;
}

// Move the initialization of the double const variables here to be compatible with the C++ 03 standard
const double MPDSegmenter::SEGMENTS_ALL_AVAILABLE = -1;
const double MPDSegmenter::SEGMENTS_FROM_FETCH_TIME = -2;

/**
 * @brief   Get Dash MPD Segments
 * @param   startTime Start Time
 * @param   endTime End Time
 * @retval  MPD Segments
 */
MPDSegmenter::SegmentVec MPDSegmenter::getSegments(double startTime, double endTime) {

    SegmentVec totalSegments;

    auto root = mpdDocument.getRoot();
    auto fetchTime = root->getFetchTime(); // Mandatory to be set
    auto dynamic = root->isDynamic();
    auto minUpdatePeriod = root->getMinimumUpdatePeriod();
    auto mediaPresentationDuration = root->getMediaPresentationDuration();
    if (mediaPresentationDuration == MPD_UNSET_DOUBLE) {
        mediaPresentationDuration = 1e10;   // when not set, should be "inf+" for the math to work
    }

    if (fetchTime == MPD_UNSET_DOUBLE) {
        throw std::runtime_error("FetchTime not set in MPD.");
    }

    // from A.3.4
    auto availabilityStart = root->getAvailabilityStartTime();  // Mandatory for dynamic streams
    if (availabilityStart == MPD_UNSET_DOUBLE) availabilityStart = 0; // Available since always (for static streams)
    auto timeShiftBufferDepth = root->getTimeShiftBufferDepth();
    if (timeShiftBufferDepth == MPD_UNSET_DOUBLE) timeShiftBufferDepth = 1e10;  // Infinite duration if not set

    for (const auto &repr: representations) {

        SegmentVec segments;
        // This method implements basically the algorithm presented in A.3

        MPDSegment baseSeg;
        baseSeg.representationKey = repr->getRepresentationKey();
        string periodKey;

        // from A.3.2 try to get period bounds
        double periodStart = 0;
        double periodEnd = 0;

        auto adaptation = repr->parent.lock();
        if (adaptation) {
            auto period = adaptation->parent.lock();
            baseSeg.adaptationKey = adaptation->getAdaptationSetKey();
            baseSeg.periodId = period->getId();
            if (period ) {
                periodStart = period->getStart();
                periodEnd = period->getEnd();
                periodKey = period->getPeriodKey();
            }
        }

        auto dashPeriods = root->getPeriods();
        int lastNonEmptyPeriodIndex = (int)dashPeriods.size() - 1;
        while(lastNonEmptyPeriodIndex > 0){
            auto currPeriod = dashPeriods.at(lastNonEmptyPeriodIndex);
            if(currPeriod->isVssEarlyAvailablePeriod() || currPeriod->getAdaptationSets().size() == 0){
                lastNonEmptyPeriodIndex--;
            }
            else{
                break;
            }
        }

        auto lastPeriod = dashPeriods.at(lastNonEmptyPeriodIndex)->getPeriodKey() == periodKey;

        if (endTime > 0 && ((availabilityStart + periodStart) > endTime)) {
            // this period starts after endTime so we're ignoring
            continue;
        }

        if ((periodEnd == MPD_UNSET_DOUBLE || periodEnd <= 0) && lastPeriod) {
            // try based on fetch time.
            auto case_a = fetchTime + minUpdatePeriod;
            auto case_b = availabilityStart + mediaPresentationDuration;
            periodEnd = min(case_a, case_b);
        }

        if (periodEnd < periodStart) {
            // this could happen when we're past due update period (case_a)
            continue;
        }

        if (startTime > -1 && ((periodEnd + availabilityStart) < startTime)) {
            AAMPLOG_ERR("periodEnd < startTime; PeriodID %s  periodEnd (%f),  startTime (%f)", baseSeg.periodId.c_str(), periodEnd, startTime);
            // period ends before start time, ignoring
            continue;
        }

        // Timeline building method from A.3.3. We're using a functions to better match the algorithm
        // description in ISO documents (except we're 0-based and they're 1-based).

        vector<shared_ptr<DashMPDMultipleSegmentBase>> multipleSegmentBases;
        vector<shared_ptr<DashMPDSegmentBase>> singleSegmentBases;

        auto tpl = repr->getSegmentTemplate();
        if (!tpl->isNull()) {
            multipleSegmentBases.push_back(tpl);
        }

        auto slists = repr->getSegmentLists();
        if (!slists.empty()) {
            for (auto &slist: slists) multipleSegmentBases.push_back(slist);
        }

        // The SegmentBase element is sufficient to describe the Segment Information if and only if a single Media
        // Segment is provided per Representation and the Media Segment URL is included in the BaseURL element
        // So we should handle this case.
        auto singleSegBase = repr->getSingleSegmentBase();
        if (singleSegBase) {
            singleSegmentBases.push_back(singleSegBase);
        }

        Url baseUrl(repr->getBaseUrl());
        
        for (auto &segBase : singleSegmentBases) {
            // It's a single segment
            if (segBase->getTag() == "SegmentBase") {
                auto init = segBase->getInitialization();
                
                if (!init->isNull()) {
                    auto seg = make_shared<MPDSegment>(baseSeg); // copy
                    seg->isInit = true;
                    if (init->getRange() != MPD_UNSET_STRING) {
                        seg->rangeSpec = init->getRange();
                    }
                    string strBaseUrl = baseUrl.str();
                    size_t lastCharPos = strBaseUrl.length() - 1;
                    if (strBaseUrl[lastCharPos] == '/') {
                        strBaseUrl = strBaseUrl.substr(0, lastCharPos);
                    }
                    seg->url = strBaseUrl.c_str();
                    seg->number = -1;
                    seg->scaledStart = -1;
                    seg->startTime = -1;
                    segments.push_back(seg);
                }
            }
        }
        
        
        for (auto &base : multipleSegmentBases) {
            // Multiple Segment
            auto duration = base->getDuration();
            auto timeline = base->getSegmentTimeline();
            auto timescale = base->getTimeScale();
            auto presentationTimeOffset = double(base->getPresentationTimeOffset()) / double(timescale);

            // startTime and duration in scaled values relative to periodStart
            std::function<long long(size_t)> segmentStartTime;
            std::function<long long(size_t)> segmentDuration;

            auto segAvailStart = [&](const MPDSegment &seg) {
                // Note: per ISO spec, we add duration, but DASH IF does not and may 404...
                return availabilityStart + periodStart + seg.startTime - presentationTimeOffset + seg.duration;
            };
            auto segAvailEnd = [&](const MPDSegment &seg) {
                return availabilityStart + periodStart + seg.startTime - presentationTimeOffset + (2*seg.duration) + timeShiftBufferDepth;
            };

            size_t k_begin = 0, k_end = 0;  // index of first and last segments


            // Should we sync the last segment duration to period end?
            // Only if we have a declared duration and we're not cutting of with endTime
            bool syncLastSegmentToPeriodEnd = false;
            auto startNumber = base->getStartNumber();


            if (duration != MPD_UNSET_LONG) {
                // use a regular duration for every segment
                segmentStartTime = [=](size_t k) { return (k * duration); };
                segmentDuration = [=](size_t k) { return duration; };
                auto realDuration = double(duration) / double(timescale);
                if (!dynamic) {
                    k_begin = 0;
                    k_end = (size_t) ceil(((periodEnd * timescale) - (periodStart * timescale)) / duration);
                    syncLastSegmentToPeriodEnd = true;
                } else {
                    k_begin = (size_t) floor((startTime - periodStart - availabilityStart) / realDuration);
                    auto limit = min(periodEnd, endTime);
                    k_end = (size_t) max(0., ceil((limit - availabilityStart - periodStart) /
                                                  realDuration));
                    syncLastSegmentToPeriodEnd = false;
                }
            } else if (!timeline->isNull()) {
                // use a timeline specification
                vector<TimelineItem> timelineItems;
                extractTimeline(*timeline, timelineItems);

                segmentStartTime = [=](size_t i) {
                    return timelineItems[i - startNumber + 1].startTime;
                };
                segmentDuration = [=](size_t i) {
                    return timelineItems[i - startNumber + 1].duration;
                };
                k_begin = 0;
                k_end = (size_t) timelineItems.size();
            } else {
                k_begin = 0;
                k_end = 1;
                segmentStartTime = [=](size_t i) { return 0; };
                segmentDuration = [=](size_t i) { return periodEnd * timescale - periodStart * timescale; };
            }

            if (base->getTag() == "SegmentList") {
                auto slist = dynamic_pointer_cast<DashMPDSegmentList>(base);
                auto segUrls = slist->getSegmentURLs();
                auto init = slist->getInitialization();

                if (!init->isNull()) {
                    auto seg = make_shared<MPDSegment>(baseSeg); // copy
                    seg->isInit = true;
                    if (init->getRange() != MPD_UNSET_STRING) {
                        seg->rangeSpec = init->getRange();
                    }
                    seg->url = baseUrl.resolveOrReplace(init->getSourceURL()).str();
                    seg->number = -1;
                    seg->scaledStart = -1;
                    seg->startTime = -1;
                    segments.push_back(seg);
                }
                for (size_t i = k_begin; i < k_end; i++) {
                    auto segUrl = segUrls[i];
                    auto seg = make_shared<MPDSegment>(baseSeg);; // copy
                    auto scaledDuration = segmentDuration(i);
                    seg->scaledStart = segmentStartTime(i);
                    seg->number = (long long) i;
                    seg->scaledDuration = scaledDuration;
                    seg->duration = double(scaledDuration) / double(timescale);
                    seg->startTime = seg->scaledStart / double(timescale);
                    if (segUrl->getMediaRange() != MPD_UNSET_STRING) {
                        seg->rangeSpec = segUrl->getMediaRange();
                    }
                    seg->url = baseUrl.resolveOrReplace(segUrl->getMedia()).str();
                    if (dynamic) {
                        seg->availabilityStartTime = segAvailStart(*seg);
                        seg->availabilityEndTime = segAvailEnd(*seg);
                    }
                    segments.push_back(seg);
                }

            } else if (base->getTag() == "SegmentTemplate") {
                tpl = dynamic_pointer_cast<DashMPDSegmentTemplate>(base);
                auto initTemplate = tpl->getInitializationAttr();
                auto media = tpl->getMedia();
                TemplateContext ctx;
                ctx["RepresentationID"] = repr->getId();  // Mandatory
                ctx["Bandwidth"] = to_string(repr->getBandwidth());  // Mandatory
                if (initTemplate != MPD_UNSET_STRING) {
                    auto seg = make_shared<MPDSegment>(baseSeg); // copy
                    seg->url = baseUrl.resolveOrReplace(decodeTemplate(initTemplate, ctx)).str();
                    seg->isInit = true;
                    segments.push_back(seg);
                }

                for (size_t k = k_begin; k < k_end; k++) {
                    auto i = startNumber + ((long long)k - 1);
                    auto scaledDuration = segmentDuration(i);
                    double segDuration = (double)scaledDuration / (double)timescale;
                    ctx["Number"] = to_string(i + 1);
                    ctx["Time"] = to_string(segmentStartTime(i));
                    auto seg = make_shared<MPDSegment>(baseSeg); // copy
                    seg->number = (long long) i + 1;
                    seg->scaledStart = segmentStartTime(i);
                    seg->url = baseUrl.resolveOrReplace(decodeTemplate(media, ctx)).str();
                    seg->duration = segDuration;
                    seg->scaledDuration = scaledDuration;
                    seg->startTime = double(seg->scaledStart) / double(timescale);
                    if (syncLastSegmentToPeriodEnd && k == (k_end - 1)) {
                        // handle duration for last segment
                        seg->duration = (periodEnd - periodStart) + seg->startTime;
                    }
                    if (dynamic) {
                        seg->availabilityStartTime = segAvailStart(*seg);
                        seg->availabilityEndTime = segAvailEnd(*seg);
                    }
                    segments.push_back(seg);
                }
            }
        }

        for (auto &seg: segments) {
            totalSegments.push_back(seg);
        }

//        std::sort(totalSegments.begin(), totalSegments.end(), [](shared_ptr<MPDSegment> &a, shared_ptr<MPDSegment> &b) {
//            return a->startTime < b->startTime;
//        });
    }

    return totalSegments;
}
