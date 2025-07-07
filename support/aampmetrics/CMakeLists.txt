# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2018 RDK Management
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
cmake_minimum_required (VERSION 3.5)
project (Metrics)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCJSON REQUIRED libcjson)

include_directories(${LIBCJSON_INCLUDE_DIRS})

set(LIBMETRICS_SOURCES
	IPHTTPStatistics.cpp
	IPLicnsStatistics.cpp
	IPFragmentStatistics.cpp
	IPVideoStat.cpp
	IPProfileInfo.cpp
	IPSessionSummary.cpp
	IPLatencyReport.cpp
	ManifestGenericStats.cpp
	CMCDHeaders.cpp
	VideoCMCDHeaders.cpp
	AudioCMCDHeaders.cpp
	SubtitleCMCDHeaders.cpp
	ManifestCMCDHeaders.cpp
)
set(LIBMETRICS_PUBLIC_HEADERS
	IPHTTPStatistics.h
	IPLicnsStatistics.h
	IPFragmentStatistics.h
	IPVideoStat.h
	IPProfileInfo.h
	IPSessionSummary.h
	IPLatencyReport.h
	StatsDefine.h
	ManifestGenericStats.h
	CMCDHeaders.h
	VideoCMCDHeaders.h
	AudioCMCDHeaders.h
	SubtitleCMCDHeaders.h
	ManifestCMCDHeaders.h
)

add_library(metrics SHARED ${LIBMETRICS_SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wno-multichar")

target_link_libraries(metrics ${LIBCJSON_LINK_LIBRARIES})
set_target_properties(metrics PROPERTIES PUBLIC_HEADER "${LIBMETRICS_PUBLIC_HEADERS}")
install(TARGETS metrics
		DESTINATION lib
		PUBLIC_HEADER DESTINATION include
)