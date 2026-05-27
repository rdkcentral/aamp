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
#include "AampUtils.h"


/**
 * @brief Default constructor for CachedFragment
 *        Initializes all members to default values.
 */
CachedFragment::CachedFragment() 
	: fragment()
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
 * @brief Share payload + clone metadata from another CachedFragment.
 *
 * Replaces the previous deep-copy semantics: scalar/string fields are
 * cloned but the byte buffer is shared by refcount via `Payload::Share()`.
 * The previous code-path performed a full `std::vector<uint8_t>` copy at
 * the fan-out sites in `MediaStreamContext::OnFragmentDownloadSuccess`
 * and `CacheStagingFragmentForInjection`; this is no longer required.
 */
void CachedFragment::Copy(const CachedFragment& other)
{
	// Copy all member variables
	this->position = other.position;
	this->duration = other.duration;
	this->initFragment = other.initFragment;
	this->discontinuity = other.discontinuity;
	this->profileIndex = other.profileIndex;
	this->cacheFragStreamInfo = other.cacheFragStreamInfo;
	this->type = other.type;
	this->downloadStartTime = other.downloadStartTime;
	this->uri = other.uri;
	this->timeScale = other.timeScale;
	this->PTSOffsetSec = other.PTSOffsetSec;
	this->absPosition = other.absPosition;
	this->isDummy = other.isDummy;
	this->discontinuityIndex = other.discontinuityIndex;

	// Share the payload bytes by refcount — no byte-level memcpy.
	this->fragment = other.fragment.Share();
}


/**
 * @brief Clear all fragment data and reset to default values
 */
void CachedFragment::Clear()
{
	fragment.ClearAndRelease();
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

/* Copy constructor and copy assignment are deleted (see header).
 * CachedFragment is move-only; use Copy() to share payload + clone metadata. */

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
	
	swap(fragment, other.fragment);
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
