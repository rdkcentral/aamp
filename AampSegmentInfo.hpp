/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
#ifndef AAMP_SEGMENT_INFO_HPP
#define AAMP_SEGMENT_INFO_HPP

#include <stdlib.h>

struct SegmentInfo_t {
	
public:
	
	/** Default constructor */
	SegmentInfo_t()
	: pts_ms{0.0},
	dts_ms{0.},
	duration{0.},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	/** Constructor 
	 * @param[in] _pts_ms PTS of current segment converted in ms
	 * @param[in] _dts_ms DTS of current segment converted in ms
	 * @param[in] _duration Duration of current segment
	*/
	SegmentInfo_t(double _pts_ms, double _dts_ms, double _duration)
	: pts_ms{_pts_ms},
	dts_ms{_dts_ms},
	duration{_duration},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	
	/** Constructor 
	 * @param[in] _pts_ms PTS of current segment converted in ms
	 * @param[in] _dts_ms DTS of current segment converted in ms
	 * @param[in] _duration Duration of current segment
	 * @param[in] _init_fragment Flag to mark if a fragment is the init one
	 * @param[in] _discontinuity Flag to report if the fragment is on a discontinuity
	*/
	SegmentInfo_t(double _pts_ms, double _dts_ms, double _duration, bool _init_fragment, bool _discontinuity)
	: pts_ms{_pts_ms},
	dts_ms{_dts_ms},
	duration{_duration},
	isInitFragment{_init_fragment},
	hasDiscontinuity{_discontinuity}
	{}
	
	
	double pts_ms;			/**< PTS of current segment converted in ms */
	double dts_ms;			/**< DTS of current segment converted in ms */
	double duration;		/**< Duration of current segment */
	bool isInitFragment;	/**< Flag to mark if a fragment is the init one */
	bool hasDiscontinuity;	/**< Flag to report if the fragment is on a discontinuity */
	
};

#endif // AAMP_SEGMENT_INFO_HPP
