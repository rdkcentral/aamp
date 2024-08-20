/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#ifndef AAMP_MOCK_ISOBMFF_BUFFER_H
#define AAMP_MOCK_ISOBMFF_BUFFER_H

#include <gmock/gmock.h>
#include "isobmff/isobmffbuffer.h"

class MockIsoBmffBuffer : public IsoBmffBuffer
{
public:

    MOCK_METHOD(bool, getFirstPTS, (uint64_t&));
    MOCK_METHOD(bool, getTimeScale, (uint32_t&));
    MOCK_METHOD(bool, isInitSegment, ());
    MOCK_METHOD(Box*, getBox, (const char *, size_t &));
    MOCK_METHOD(void, getSampleDuration, (Box *, uint64_t &));
    MOCK_METHOD(void, setBuffer, (uint8_t *, size_t));
    MOCK_METHOD(bool, parseBuffer, (bool, int));
    MOCK_METHOD(void, restampPts, (int64_t));
    MOCK_METHOD(void, setPtsAndDuration, (uint64_t, uint64_t));
    MOCK_METHOD(uint64_t, getSegmentDuration, ());
    MOCK_METHOD(bool, getMdatBoxCount, (size_t&));
    MOCK_METHOD(size_t, getParsedBoxesSize, ());
    MOCK_METHOD(bool, getChunkedfBoxMetaData, (uint32_t &, std::string &, uint32_t &));
    MOCK_METHOD(int, UpdateBufferData, (size_t , char* &, size_t &, size_t& ));
    MOCK_METHOD(double, getTotalChunkDuration, (int));
    MOCK_METHOD(bool, ParseChunkData, (const char* , char* &, uint32_t,	size_t & , size_t &, double& , double &));
	MOCK_METHOD(bool, setTrickmodeTimescale, (uint32_t));
};

extern MockIsoBmffBuffer *g_mockIsoBmffBuffer;

#endif /* AAMP_MOCK_ISOBMFF_BUFFER_H */
