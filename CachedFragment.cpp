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
	, fragmentType(FragmentType::COMPLETE_FRAGMENT)
	, mMutex()  // Default construct the mutex
{
	// RAII: All members are initialized, no manual setup needed
}

/**
 * @brief Copy content from another CachedFragment up to a specified length
 *        Tolerant to external Free() calls on the fragment buffer.
 */
void CachedFragment::Copy(CachedFragment* other, size_t len)
{
	if (!other) 
	{
		return;
	}
	if (this == other)
	{
		// Self-copy: lock only this->mMutex to ensure consistent locking semantics
		std::lock_guard<std::mutex> lockThis(mMutex);
		// No copy performed, but lock is held for duration
		return;
	}
	
	// Lock both objects to prevent data races
	// Use std::lock to avoid deadlock via its deadlock-avoidance algorithm
	std::lock(mMutex, other->mMutex);
	std::lock_guard<std::mutex> lockThis(mMutex, std::adopt_lock);
	std::lock_guard<std::mutex> lockOther(other->mMutex, std::adopt_lock);
	
	// RAII: Instead of calling Free() explicitly, assign a new empty buffer
	// This is tolerant to external Free() calls and follows RAII principles
	fragment = AampGrowableBuffer("cached-fragment");
	
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
	
	this->fragmentType = other->fragmentType;
	
	// Copy fragment data up to specified length
	// Defensive: Check if buffer is valid before accessing
	if (other->fragment.GetPtr() && len > 0)
	{
		this->fragment.AppendBytes(other->fragment.GetPtr(), len);
	}
}


/**
 * @brief Clear all fragment data and reset to default values
 *        Tolerant to external Free() calls on the fragment buffer.
 */
void CachedFragment::Clear()
{
	std::lock_guard<std::mutex> lock(mMutex);
	// RAII: Assign a new empty buffer instead of calling Free()
	fragment = AampGrowableBuffer("cached-fragment");
	
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
	fragmentType = FragmentType::COMPLETE_FRAGMENT;
}

/**
 * @brief Copy constructor
 */
CachedFragment::CachedFragment(const CachedFragment& other)
	: fragment(AampGrowableBuffer("cached-fragment"))  // RAII: Initialize with empty buffer
	, position(other.position)  // Direct initialization where possible
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
	, fragmentType(other.fragmentType)
	, mMutex()  // Each object gets its own mutex
{
	// Lock the source object for thread-safe reading
	std::lock_guard<std::mutex> lock(other.mMutex);
	
	// Copy fragment data - defensive check for valid buffer
	if (other.fragment.GetPtr() && other.fragment.GetLen() > 0) 
	{
		fragment.AppendBytes(other.fragment.GetPtr(), other.fragment.GetLen());
	}
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
	, fragmentType(other.fragmentType)
	, mMutex()  // Each object gets its own mutex
{
	// Lock the source object during move
	std::lock_guard<std::mutex> lock(other.mMutex);
	
	// RAII: Reset moved-from object to valid default state
	// Assign new empty buffer to ensure valid state
	other.fragment = AampGrowableBuffer("cached-fragment-moved");
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
	other.fragmentType = FragmentType::COMPLETE_FRAGMENT;
}

/**
 * @brief Copy assignment operator
 */
CachedFragment& CachedFragment::operator=(const CachedFragment& other)
{
	if (this != &other) 
	{
		// Use std::lock to avoid deadlock by acquiring locks in consistent order
		std::lock(mMutex, other.mMutex);
		std::lock_guard<std::mutex> lockThis(mMutex, std::adopt_lock);
		std::lock_guard<std::mutex> lockOther(other.mMutex, std::adopt_lock);
		
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
		fragmentType = other.fragmentType;
	}
	return *this;
}

/**
 * @brief Move assignment operator
 */
CachedFragment& CachedFragment::operator=(CachedFragment&& other) noexcept
{
	if (this != &other) 
	{
		// Use std::lock to avoid deadlock by acquiring locks in consistent order
		std::lock(mMutex, other.mMutex);
		std::lock_guard<std::mutex> lockThis(mMutex, std::adopt_lock);
		std::lock_guard<std::mutex> lockOther(other.mMutex, std::adopt_lock);
		
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
		fragmentType = other.fragmentType;
		
		// RAII: Reset moved-from object to valid default state
		other.fragment = AampGrowableBuffer("cached-fragment-moved");
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
		other.fragmentType = FragmentType::COMPLETE_FRAGMENT;
	}
	return *this;
}

/**
 * @brief Swap contents with another CachedFragment
 */
void CachedFragment::swap(CachedFragment& other) noexcept
{
	if (this != &other) 
	{
		// Use std::lock to avoid deadlock by acquiring locks in consistent order
		std::lock(mMutex, other.mMutex);
		std::lock_guard<std::mutex> lockThis(mMutex, std::adopt_lock);
		std::lock_guard<std::mutex> lockOther(other.mMutex, std::adopt_lock);
		
		using std::swap;
		
		// RAII: Use move assignment for buffer swap
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
		swap(fragmentType, other.fragmentType);
	}
}

/**
 * @brief Free function swap for CachedFragment
 */
void swap(CachedFragment& lhs, CachedFragment& rhs) noexcept
{
	lhs.swap(rhs);
}

/**
 * @brief Get the fragment type
 */
FragmentType CachedFragment::GetFragmentType() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return fragmentType;
}

/**
 * @brief Set the fragment type
 */
void CachedFragment::SetFragmentType(FragmentType type)
{
	std::lock_guard<std::mutex> lock(mMutex);
	fragmentType = type;
}

/**
 * @brief Thread-safe getter for position
 */
double CachedFragment::GetPosition() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return position;
}

/**
 * @brief Thread-safe setter for position
 */
void CachedFragment::SetPosition(double pos)
{
	std::lock_guard<std::mutex> lock(mMutex);
	position = pos;
}

/**
 * @brief Thread-safe getter for duration
 */
double CachedFragment::GetDuration() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return duration;
}

/**
 * @brief Thread-safe setter for duration
 */
void CachedFragment::SetDuration(double dur)
{
	std::lock_guard<std::mutex> lock(mMutex);
	duration = dur;
}

/**
 * @brief Thread-safe getter for URI
 */
std::string CachedFragment::GetUri() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return uri;
}

/**
 * @brief Thread-safe setter for URI
 */
void CachedFragment::SetUri(const std::string& newUri)
{
	std::lock_guard<std::mutex> lock(mMutex);
	uri = newUri;
}

/**
 * @brief Thread-safe getter for fragment length
 *        Defensive against external Free() calls
 */
size_t CachedFragment::GetFragmentLength() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	// Defensive: Check if buffer pointer is valid before getting length
	return fragment.GetPtr() ? fragment.GetLen() : 0;
}

/**
 * @brief Thread-safe check if fragment is empty
 *        Defensive against external Free() calls
 */
bool CachedFragment::IsEmpty() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	// Defensive: Treat null pointer as empty
	return (!fragment.GetPtr() || fragment.GetLen() == 0);
}
