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
cmake_minimum_required (VERSION 3.13)
project (Metrics)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCJSON REQUIRED libcjson)

include_directories(${LIBCJSON_INCLUDE_DIRS})

set(LIB_SOURCES IPHTTPStatistics.cpp
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
	    ManifestCMCDHeaders.cpp)

add_library(metrics SHARED ${LIB_SOURCES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -std=c++11 -Wno-multichar")

if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
        execute_process (
                COMMAND bash -c "xcrun --show-sdk-path" OUTPUT_VARIABLE osxSdkPath OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(OS_CXX_FLAGS "${OS_CXX_FLAGS} -g -x objective-c++ -Wno-inconsistent-missing-override -F${osxSdkPath}/System/Library/Frameworks")
        set(OS_LD_FLAGS "${OS_LD_FLAGS} -F${osxSdkPath}/System/Library/Frameworks -L${osxSdkPath}/../MacOSX.sdk/usr/lib -L/usr/local/lib/")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -isysroot ${osxSdkPath}/../MacOSX.sdk -I/usr/local/include")
        string(STRIP ${OS_LD_FLAGS} OS_LD_FLAGS)
	target_link_libraries(metrics "${OS_LD_FLAGS}")
        link_directories(${LIBCJSON_LIBRARY_DIRS})
endif(CMAKE_SYSTEM_NAME STREQUAL Darwin)

target_link_libraries(metrics "-lcjson")

set_target_properties(metrics PROPERTIES PUBLIC_HEADER "IPVideoStat.h;CMCDHeaders.h;VideoCMCDHeaders.h;AudioCMCDHeaders.h;SubtitleCMCDHeaders.h;ManifestCMCDHeaders.h")
install(TARGETS metrics DESTINATION lib PUBLIC_HEADER DESTINATION include)
install(FILES IPHTTPStatistics.h IPFragmentStatistics.h IPLicnsStatistics.h IPProfileInfo.h IPSessionSummary.h IPLatencyReport.h StatsDefine.h ManifestGenericStats.h CMCDHeaders.h VideoCMCDHeaders.h AudioCMCDHeaders.h SubtitleCMCDHeaders.h ManifestCMCDHeaders.h DESTINATION include)
