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
// MPDSegmentExtractor.h
//


/**
 * @file MPDSegmenter.h
 * @brief
 */


#ifndef FOG_CLI_MPDSEGMENTEXTRACTOR_H
#define FOG_CLI_MPDSEGMENTEXTRACTOR_H

#include "MPDModel.h"
#include "../utils/Utils.h"
#include "../utils/Url.h"
#include "AampConfig.h"

/**
 * This class is aimed at extracting MPDSegment's from a MPDDocument, regardless of the chosen
 * method to announce the segments (SegmentTemplate, SegmentList, etc...).
//  *
 * This segmenter assumes the <MDP fog:fetchTime> attribute and <MDP><Location> element are set.
 */

/**
 * @class   MPDSegmenter
 * @brief   This class is aimed at extracting MPDSegment's from a MPDDocument, regardless of the chosen
 * method to announce the segments (SegmentTemplate, SegmentList, etc...)
 */
class MPDSegmenter {
    DashMPDDocument &mpdDocument;


public:
    /**
     * @typedef SegmentVec
     * @brief   Vector of MPDSegment
     */
    typedef std::vector<std::shared_ptr<MPDSegment>> SegmentVec;
    /**
     * @typedef RepresentationVec
     * @brief   Vector of DashMPDRepresentation
     */
    typedef std::vector<std::shared_ptr<DashMPDRepresentation>> RepresentationVec;

    static const double SEGMENTS_ALL_AVAILABLE;
	static const double SEGMENTS_FROM_FETCH_TIME;

	static const int DIRECTION_FORWARD = 1;
	static const int DIRECTION_BACKWARD = -1;

    /**
     * The list of selected representations
     */
    RepresentationVec representations;

    explicit MPDSegmenter(DashMPDDocument& mpdDocument) : mpdDocument(mpdDocument),representations() {}

    /**
     * Extract all known MPDSegments.
     *
     * For live segments, if the end-time is unknown, we follow A.3.2 rules for the Period End Time.
     *
     * The "direction" param should be DIRECTION_FORWARD or DIRECTION_BACKWARD to return the first <limit>
     * or the last <limit> segments.
     *
     * The "init" segments are always returned (and count to the limit)
     */
    MPDSegmenter::SegmentVec getSegments(double startTime = -1, double endTime = -1);



};



#endif //FOG_CLI_MPDSEGMENTEXTRACTOR_H
