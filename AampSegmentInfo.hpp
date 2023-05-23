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
#ifndef AAMP_STREAM_INFO_HPP
#define AAMP_STREAM_INFO_HPP

#include <stdlib.h>

struct SegmentInfo_t {
	
public:
	
	SegmentInfo_t()
	: pts{0.0},
	dts{0.},
	duration{0.},
	pts_offset{0.},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	SegmentInfo_t(double _pts, double _dts, double _duration)
	: pts{_pts},
	dts{_dts},
	duration{_duration},
	pts_offset{0.},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	SegmentInfo_t(double _pts, double _dts, double _duration, double _pts_offset)
	: pts{_pts},
	dts{_dts},
	duration{_duration},
	pts_offset{_pts_offset},
	isInitFragment{false},
	hasDiscontinuity{false}
	{}
	
	SegmentInfo_t(double _pts, double _dts, double _duration, bool _init_fragment, bool _discontinuity)
	: pts{_pts},
	dts{_dts},
	duration{_duration},
	pts_offset{0.},
	isInitFragment{_init_fragment},
	hasDiscontinuity{_discontinuity}
	{}
	
	
	double pts;
	double dts;
	double duration;
	double pts_offset;
	bool isInitFragment;
	bool hasDiscontinuity;
	
};

#endif // AAMP_STREAM_INFO_HPP
