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

#include "isobmffbuffer.h"
#include "MockIsoBmffBuffer.h"

MockIsoBmffBuffer *g_mockIsoBmffBuffer = nullptr;

IsoBmffBuffer::~IsoBmffBuffer()
{
}

bool IsoBmffBuffer::isInitSegment()
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->isInitSegment();
    }
    else
    {
        return false;
    }
}

void IsoBmffBuffer::setBuffer(uint8_t *buf, size_t sz)
{
    if (g_mockIsoBmffBuffer)
    {
        g_mockIsoBmffBuffer->setBuffer(buf, sz);
    }
}

bool IsoBmffBuffer::parseBuffer(bool correctBoxSize, int newTrackId)
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->parseBuffer(correctBoxSize, newTrackId);
    }
    else
    {
        return false;
    }
}

bool IsoBmffBuffer::getTimeScale(uint32_t &timeScale)
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->getTimeScale(timeScale);
    }
    else
    {
        return false;
    }
}

void IsoBmffBuffer::destroyBoxes()
{
}

bool IsoBmffBuffer::getEMSGData(uint8_t* &message, uint32_t &messageLen, char * &schemeIdUri, uint8_t* &value, uint64_t &presTime, uint32_t &timeScale, uint32_t &eventDuration, uint32_t &id)
{
    return false;
}

Box* IsoBmffBuffer::getChunkedfBox() const
{
    return nullptr;
}

bool IsoBmffBuffer::getFirstPTSInternal(const std::vector<Box*> *boxes, uint64_t &pts)
{
    return true;
}

bool IsoBmffBuffer::getFirstPTS(uint64_t &pts)
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->getFirstPTS(pts);
    }
    else
    {
        return false;
    }
}

bool IsoBmffBuffer::getMdatBoxCount(size_t &count)
{
    return true;
}

std::vector<Box*> *IsoBmffBuffer::getParsedBoxes()
{
    return nullptr;
}

uint64_t IsoBmffBuffer::getSampleDurationInternal(const std::vector<Box*> *boxes)
{
    return 0;
}

void IsoBmffBuffer::getSampleDuration(Box *box, uint64_t &fduration)
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->getSampleDuration(box, fduration);
    }
}

bool IsoBmffBuffer::getTrack_id(uint32_t &track_id)
{
	return false;
}

void IsoBmffBuffer::restampPTS(uint64_t offset, uint64_t basePts, uint8_t *segment, uint32_t bufSz)
{
}

void IsoBmffBuffer::restampPts(int64_t offset)
{
    if (g_mockIsoBmffBuffer)
    {
        g_mockIsoBmffBuffer->restampPts(offset);
    }
}

uint64_t IsoBmffBuffer::getSegmentDuration()
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->getSegmentDuration();
    }
    else
    {
        return 0;
    }
}

Box* IsoBmffBuffer::getBox(const char *name, size_t &index)
{
    if (g_mockIsoBmffBuffer)
    {
        return g_mockIsoBmffBuffer->getBox(name, index);
    }
    else
    {
        return NULL;
    }
}

void IsoBmffBuffer::truncate(void)
{
}
