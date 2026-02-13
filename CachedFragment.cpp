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


/**
 * @brief Default constructor for CachedFragment
 *        Initializes all members to default values.
 */
CachedFragment::CachedFragment() 
	: fragment(AampGrowableBuffer("cached-fragment"))
	, position(0.0)
	, duration(0.0)
	, initFragment(false)
	, discontinuity(false)
	, profileIndex(0)
	, cacheFragStreamInfo(StreamInfo())
	, type(eMEDIATYPE_DEFAULT)
	, downloadStartTime(0)
	, timeScale(0)
	, PTSOffsetSec(0)
	, absPosition(0.0)
	, isDummy(false)
	, discontinuityIndex(0)
{
}


/**
 * @brief Copy content from another CachedFragment up to a specified length
 */
void CachedFragment::Copy(CachedFragment* other, size_t len)
{
	// Clear existing data first
	this->fragment.Free();
	
	// Copy all member variables
	this->position = other->position;
	this->duration = other->duration;
	this->initFragment = other->initFragment;
	this->discontinuity = other->discontinuity;
	this->profileIndex = other->profileIndex;
	this->cacheFragStreamInfo = other->cacheFragStreamInfo;
	this->type = other->type;
	this->downloadStartTime = other->downloadStartTime;
	this->uri = other->uri;
	this->timeScale = other->timeScale;
	this->PTSOffsetSec = other->PTSOffsetSec;
	this->absPosition = other->absPosition;
	this->isDummy = other->isDummy;
	this->discontinuityIndex = other->discontinuityIndex;
	
	// Copy fragment data up to specified length
	if (other && other->fragment.capacity() != 0 && len > 0) {
		this->fragment.AppendBytes(other->fragment.GetPtr(), len);
	}
}


/**
 * @brief Clear all fragment data and reset to default values
 */
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

/**
 * @brief Copy constructor
 */
CachedFragment::CachedFragment(const CachedFragment& other)
	: fragment(other.fragment)
	, position(other.position)
	, duration(other.duration)
	, initFragment(other.initFragment)
	, discontinuity(other.discontinuity)
	, isDummy(other.isDummy)
	, profileIndex(other.profileIndex)
	, timeScale(other.timeScale)
	, uri(other.uri)
	, cacheFragStreamInfo(other.cacheFragStreamInfo)
	, type(other.type)
	, downloadStartTime(other.downloadStartTime)
	, discontinuityIndex(other.discontinuityIndex)
	, PTSOffsetSec(other.PTSOffsetSec)
	, absPosition(other.absPosition)
{
}

/**
 * @brief Move constructor
 */
CachedFragment::CachedFragment(CachedFragment&& other) noexcept
	: fragment(std::move(other.fragment))
	, position(other.position)
	, duration(other.duration)
	, initFragment(other.initFragment)
	, discontinuity(other.discontinuity)
	, isDummy(other.isDummy)
	, profileIndex(other.profileIndex)
	, timeScale(other.timeScale)
	, uri(std::move(other.uri))
	, cacheFragStreamInfo(std::move(other.cacheFragStreamInfo))
	, type(other.type)
	, downloadStartTime(other.downloadStartTime)
	, discontinuityIndex(other.discontinuityIndex)
	, PTSOffsetSec(other.PTSOffsetSec)
	, absPosition(other.absPosition)
{
	// Reset moved-from object to default state
	other.position = 0.0;
	other.duration = 0.0;
	other.initFragment = false;
	other.discontinuity = false;
	other.isDummy = false;
	other.profileIndex = 0;
	other.timeScale = 0;
	other.type = eMEDIATYPE_DEFAULT;
	other.downloadStartTime = 0;
	other.discontinuityIndex = 0;
	other.PTSOffsetSec = 0;
	other.absPosition = 0.0;
}

/**
 * @brief Copy assignment operator
 */
CachedFragment& CachedFragment::operator=(const CachedFragment& other)
{
	if (this != &other) {
		fragment = other.fragment;
		position = other.position;
		duration = other.duration;
		initFragment = other.initFragment;
		discontinuity = other.discontinuity;
		isDummy = other.isDummy;
		profileIndex = other.profileIndex;
		timeScale = other.timeScale;
		uri = other.uri;
		cacheFragStreamInfo = other.cacheFragStreamInfo;
		type = other.type;
		downloadStartTime = other.downloadStartTime;
		discontinuityIndex = other.discontinuityIndex;
		PTSOffsetSec = other.PTSOffsetSec;
		absPosition = other.absPosition;
	}
	return *this;
}

/**
 * @brief Move assignment operator
 */
CachedFragment& CachedFragment::operator=(CachedFragment&& other) noexcept
{
	if (this != &other) {
		fragment = std::move(other.fragment);
		position = other.position;
		duration = other.duration;
		initFragment = other.initFragment;
		discontinuity = other.discontinuity;
		isDummy = other.isDummy;
		profileIndex = other.profileIndex;
		timeScale = other.timeScale;
		uri = std::move(other.uri);
		cacheFragStreamInfo = std::move(other.cacheFragStreamInfo);
		type = other.type;
		downloadStartTime = other.downloadStartTime;
		discontinuityIndex = other.discontinuityIndex;
		PTSOffsetSec = other.PTSOffsetSec;
		absPosition = other.absPosition;
		
		// Reset moved-from object to default state
		other.position = 0.0;
		other.duration = 0.0;
		other.initFragment = false;
		other.discontinuity = false;
		other.isDummy = false;
		other.profileIndex = 0;
		other.timeScale = 0;
		other.type = eMEDIATYPE_DEFAULT;
		other.downloadStartTime = 0;
		other.discontinuityIndex = 0;
		other.PTSOffsetSec = 0;
		other.absPosition = 0.0;
	}
	return *this;
}

/**
 * @brief Swap contents with another CachedFragment
 */
void CachedFragment::swap(CachedFragment& other) noexcept
{
	using std::swap;
	
	// For AampGrowableBuffer, we need to use assignment since it doesn't have swap
	AampGrowableBuffer tempFragment = std::move(fragment);
	fragment = std::move(other.fragment);
	other.fragment = std::move(tempFragment);
	
	swap(position, other.position);
	swap(duration, other.duration);
	swap(initFragment, other.initFragment);
	swap(discontinuity, other.discontinuity);
	swap(isDummy, other.isDummy);
	swap(profileIndex, other.profileIndex);
	swap(timeScale, other.timeScale);
	swap(uri, other.uri);
	swap(cacheFragStreamInfo, other.cacheFragStreamInfo);
	swap(type, other.type);
	swap(downloadStartTime, other.downloadStartTime);
	swap(discontinuityIndex, other.discontinuityIndex);
	swap(PTSOffsetSec, other.PTSOffsetSec);
	swap(absPosition, other.absPosition);
}

/**
 * @brief Free function swap for CachedFragment
 */
void swap(CachedFragment& lhs, CachedFragment& rhs) noexcept
{
	lhs.swap(rhs);
}
