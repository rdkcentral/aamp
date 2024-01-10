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
#include <iostream>

struct SegmentInfo_t {
	
public:
	
	/** Default constructor */
	SegmentInfo_t()
	: pts_s{0.0},
	dts_s{0.},
	duration{0.},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	/** Constructor 
	 * @param[in] _pts_s PTS of current segment converted in s
	 * @param[in] _dts_s DTS of current segment converted in s
	 * @param[in] _duration Duration of current segment
	*/
	SegmentInfo_t(double _pts_s, double _dts_s, double _duration)
	: pts_s{_pts_s},
	dts_s{_dts_s},
	duration{_duration},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	
	/** Constructor 
	 * @param[in] _pts_s PTS of current segment converted in s
	 * @param[in] _dts_s DTS of current segment converted in s
	 * @param[in] _duration Duration of current segment
	 * @param[in] _init_fragment Flag to mark if a fragment is the init one
	 * @param[in] _discontinuity Flag to report if the fragment is on a discontinuity
	*/
	SegmentInfo_t(double _pts_s, double _dts_s, double _duration, bool _init_fragment, bool _discontinuity)
	: pts_s{_pts_s},
	dts_s{_dts_s},
	duration{_duration},
	isInitFragment{_init_fragment},
	hasDiscontinuity{_discontinuity}
	{}
	
	
	double pts_s;			/**< PTS of current segment converted in s */
	double dts_s;			/**< DTS of current segment converted in s */
	double duration;		/**< Duration of current segment */
	bool isInitFragment;	/**< Flag to mark if a fragment is the init one */
	bool hasDiscontinuity;	/**< Flag to report if the fragment is on a discontinuity */
	
};

inline std::string ToString(const SegmentInfo_t & info)
{
	std::string ret{};

	ret = "PTS: " + std::to_string(info.pts_s) + " - DTS: " + std::to_string(info.dts_s)
		+ " - Duration: " + std::to_string(info.duration) + " [" + (info.isInitFragment ? "true" : "false")
		+ " | " + (info.hasDiscontinuity ? "true" : "false") + "]";

	return ret;
}

#endif // AAMP_SEGMENT_INFO_HPP
