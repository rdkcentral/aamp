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
 * @file AampFragment.cpp
 * @brief Simple fragment class for AAMP - implementation
 */

#include "AampFragment.h"
#include <cstring>

/**
 * @file AampFragment.cpp
 * @brief Implementation of unified fragment class for AAMP
 */

#include "AampFragment.h"

/**
 * @brief Default constructor
 */
AampFragment::AampFragment()
    : mUrl("")
    , mFragmentData()
    , mType(COMPLETE_FRAGMENT)
    , mChunkCount(0)
    , mPosition(0.0)
    , mDuration(0.0)
    , mIsInitFragment(false)
    , mHasDiscontinuity(false)
    , mIsComplete(false)
{
}

/**
 * @brief Constructor with URL
 * @param url Fragment URL
 */
AampFragment::AampFragment(const std::string& url)
    : mUrl(url)
    , mFragmentData()
    , mType(COMPLETE_FRAGMENT)
    , mChunkCount(0)
    , mPosition(0.0)
    , mDuration(0.0)
    , mIsInitFragment(false)
    , mHasDiscontinuity(false)
    , mIsComplete(false)
{
}

/**
 * @brief Get the fragment URL
 * @return Fragment URL
 */
std::string AampFragment::GetUrl() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mUrl;
}

/**
 * @brief Set the fragment URL
 * @param url New fragment URL
 */
void AampFragment::SetUrl(const std::string& url)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mUrl = url;
}

// === Fragment Caching Support Implementation ===

/**
 * @brief Set fragment data buffer
 * @param data Pointer to fragment data
 * @param size Size of fragment data
 * @param type Fragment type (complete or chunk)
 */
void AampFragment::SetFragmentData(const uint8_t* data, size_t size, FragmentType type)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    
    // Reject modification if fragment is already complete
    if (mIsComplete)
    {
        return;
    }
    
    mType = type;
    mFragmentData.clear(); // Clear existing data first
    
    if (data && size > 0)
    {
        mFragmentData.assign(data, data + size);
        if (type == COMPLETE_FRAGMENT)
        {
            mIsComplete = true;
        }
    }
}

/**
 * @brief Get fragment data buffer
 * @return Pointer to fragment data (const access)
 */
const uint8_t* AampFragment::GetFragmentData() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mFragmentData.empty() ? nullptr : mFragmentData.data();
}

/**
 * @brief Get fragment data size
 * @return Size of fragment data in bytes
 */
size_t AampFragment::GetFragmentSize() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mFragmentData.size();
}

/**
 * @brief Set fragment position in playlist
 * @param pos Position as AampTime
 */
void AampFragment::SetPosition(const AampTime& pos)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mPosition = pos;
}

/**
 * @brief Get fragment position in playlist
 * @return Position as AampTime
 */
AampTime AampFragment::GetPosition() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mPosition;
}

/**
 * @brief Set fragment duration
 * @param dur Duration as AampTime
 */
void AampFragment::SetDuration(const AampTime& dur)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mDuration = dur;
}

/**
 * @brief Get fragment duration
 * @return Duration as AampTime
 */
AampTime AampFragment::GetDuration() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mDuration;
}

/**
 * @brief Set discontinuity flag
 * @param discontinuity True if this fragment has a discontinuity
 */
void AampFragment::SetDiscontinuity(bool discontinuity)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mHasDiscontinuity = discontinuity;
}

/**
 * @brief Check if fragment has discontinuity
 * @return True if fragment has discontinuity
 */
bool AampFragment::HasDiscontinuity() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mHasDiscontinuity;
}

/**
 * @brief Set whether this is an init fragment
 * @param isInit True if this is an init fragment
 */
void AampFragment::SetInitFragment(bool isInit)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mIsInitFragment = isInit;
}

/**
 * @brief Check if this is an init fragment
 * @return True if this is an init fragment
 */
bool AampFragment::IsInitFragment() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mIsInitFragment;
}

/**
 * @brief Add a chunk to this fragment (for chunk-based fragments)
 * @param chunkData Chunk data to add
 * @param chunkSize Size of chunk data
 * @return True if chunk was successfully added, false if rejected (e.g., fragment already complete)
 */
bool AampFragment::AddChunk(const uint8_t* chunkData, size_t chunkSize)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    
    // Reject chunk addition if fragment is already complete
    if (mIsComplete)
    {
        return false;
    }
    
    if (chunkData && chunkSize > 0)
    {
        // Add chunk to the main fragment buffer
        mFragmentData.insert(mFragmentData.end(), chunkData, chunkData + chunkSize);
        
        // Increment chunk counter
        mChunkCount++;
        
        mType = FRAGMENT_CHUNK;
        return true;
    }
    
    // Invalid input parameters
    return false;
}

/**
 * @brief Check if fragment is complete (all chunks received)
 * @return True if fragment is complete
 */
bool AampFragment::IsComplete() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mIsComplete;
}

/**
 * @brief Set fragment completion status
 * @param complete True to mark fragment as complete, false otherwise
 */
void AampFragment::SetComplete(bool complete)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mIsComplete = complete;
}

/**
 * @brief Get number of chunks in this fragment
 * @return Number of chunks
 */
size_t AampFragment::GetChunkCount() const
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    return mChunkCount;
}

/**
 * @brief Clear fragment data and reset state
 */
void AampFragment::Clear()
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    mUrl.clear();
    mFragmentData.clear(); // Clear the vector
    mChunkCount = 0;
    mType = COMPLETE_FRAGMENT;
    mPosition = AampTime(0.0);
    mDuration = AampTime(0.0);
    mIsInitFragment = false;
    mHasDiscontinuity = false;
    mIsComplete = false;
}

/**
 * @brief Copy fragment data from another fragment
 * @param other Source fragment to copy from
 * @param length Optional length limit for partial copy
 */
void AampFragment::CopyFrom(const AampFragment& other, size_t length)
{
    std::lock_guard<std::mutex> lock(mFragmentStateMutex);
    std::lock_guard<std::mutex> otherLock(other.mFragmentStateMutex);
    
    mUrl = other.mUrl;
    mType = other.mType;
    mChunkCount = other.mChunkCount;
    mPosition = other.mPosition;
    mDuration = other.mDuration;
    mIsInitFragment = other.mIsInitFragment;
    mHasDiscontinuity = other.mHasDiscontinuity;
    mIsComplete = other.mIsComplete;
    
    // Copy fragment data
    mFragmentData.clear();
    if (length > 0 && length < other.mFragmentData.size())
    {
        mFragmentData.assign(other.mFragmentData.begin(), other.mFragmentData.begin() + length);
    }
    else
    {
        mFragmentData = other.mFragmentData;
    }
}
