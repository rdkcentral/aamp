/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/**
 * @file CachedFragment.cpp
 * @brief Implementation of CachedFragment class
 */

#include "CachedFragment.h"

CachedFragment::CachedFragment() 
	: fragment(AampGrowableBuffer("cached-fragment"))
	, position(0.0)
	, duration(0.0)
	, initFragment(false)
	, discontinuity(false)
	, isDummy(false)
	, profileIndex(0)
	, timeScale(0)
	, uri()
	, cacheFragStreamInfo(StreamInfo())
	, type(eMEDIATYPE_DEFAULT)
	, downloadStartTime(0)
	, discontinuityIndex(0)
	, PTSOffsetSec(0)
	, absPosition(0.0)
{
}

void CachedFragment::Copy(CachedFragment* other, size_t len)
{
	this->position = other->position;
	this->duration = other->duration;
	this->initFragment = other->initFragment;
	this->discontinuity = other->discontinuity;
	this->profileIndex = other->profileIndex;
	this->cacheFragStreamInfo = other->cacheFragStreamInfo;
	this->type = other->type;
	this->fragment.AppendBytes(other->fragment.GetPtr(), len);
	this->downloadStartTime = other->downloadStartTime;
	this->uri = other->uri;
	this->timeScale = other->timeScale;
	this->PTSOffsetSec = other->PTSOffsetSec;
	this->absPosition = other->absPosition;
	this->isDummy = other->isDummy;
}

void CachedFragment::Clear()
{
	fragment.Free();
	position = 0.0;
	duration = 0.0;
	initFragment = false;
	discontinuity = false;
	isDummy = false;
	profileIndex = 0;
	timeScale = 0;
	uri = "";
	cacheFragStreamInfo = StreamInfo();
	type = eMEDIATYPE_DEFAULT;
	downloadStartTime = 0;
	discontinuityIndex = 0;
	PTSOffsetSec = 0;
	absPosition = 0.0;
}
