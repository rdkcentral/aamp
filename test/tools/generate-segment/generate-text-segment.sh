#!/bin/sh -e
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
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

BASE_MEDIA_DECODE_TIME=$1
DURATION=$2
TIMESCALE=$3
SEGMENT_NUMBER=$4
MEDIA_FNAME=$5
INIT_FNAME=$6

DURATION_S=$(echo "scale=6; $DURATION / $TIMESCALE" | bc)
BASE_MEDIA_DECODE_TIME_S=$(echo "scale=6; $BASE_MEDIA_DECODE_TIME / $TIMESCALE" | bc)

python3 createTTMLSegment.py $MEDIA_FNAME $INIT_FNAME $BASE_MEDIA_DECODE_TIME_S $DURATION_S

# repair timescale in init header
python3 modifyContentMetaData.py $INIT_FNAME --timescale $TIMESCALE

# populate base media decode time and segment number
python3 modifyContentMetaData.py $MEDIA_FNAME --baseMediaDecodeTime $BASE_MEDIA_DECODE_TIME --segmentNumber $SEGMENT_NUMBER
