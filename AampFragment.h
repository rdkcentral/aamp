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
 * @file AampFragment.h
 * @brief Simple fragment class for AAMP - built using TDD
 */

#ifndef AAMP_FRAGMENT_H
#define AAMP_FRAGMENT_H

#include <string>
#include <mutex>
#include <vector>
#include <memory>
#include <cstdint>
#include "AampTime.h"

/**
 * @class AampFragment
 * @brief Unified fragment class for AAMP supporting both complete fragments and chunk-based fragments
 * 
 * This class replaces the existing CachedFragment functionality and provides
 * thread-safe operations for fragment caching.
 */
class AampFragment
{
public:
    /**
     * @brief Default constructor
     */
    AampFragment();

    /**
     * @brief Constructor with URL
     * @param url Fragment URL
     */
    AampFragment(const std::string& url);

    /**
     * @brief Destructor
     */
    ~AampFragment() = default;

    /**
     * @brief Get the fragment URL
     * @return Fragment URL
     */
    std::string GetUrl() const;

    /**
     * @brief Set the fragment URL
     * @param url New fragment URL
     */
    void SetUrl(const std::string& url);

    // === Fragment Caching Support ===
    
    /**
     * @brief Fragment types
     */
    enum FragmentType
    {
        COMPLETE_FRAGMENT,  ///< Complete fragment (replaces mCachedFragment)
        FRAGMENT_CHUNK      ///< Fragment chunk (replaces mCachedFragmentChunks)
    };

    /**
     * @brief Set fragment data buffer
     * @param data Pointer to fragment data
     * @param size Size of fragment data
     * @param type Fragment type (complete or chunk)
     */
    void SetFragmentData(const uint8_t* data, size_t size, FragmentType type = COMPLETE_FRAGMENT);

    /**
     * @brief Get fragment data buffer
     * @return Pointer to fragment data (const access)
     */
    const uint8_t* GetFragmentData() const;

    /**
     * @brief Get fragment data size
     * @return Size of fragment data in bytes
     */
    size_t GetFragmentSize() const;

    /**
     * @brief Set fragment position in playlist
     * @param pos Position as AampTime
     */
    void SetPosition(const AampTime& pos);

    /**
     * @brief Get fragment position in playlist
     * @return Position as AampTime
     */
    AampTime GetPosition() const;

    /**
     * @brief Set fragment duration
     * @param dur Duration as AampTime
     */
    void SetDuration(const AampTime& dur);

    /**
     * @brief Get fragment duration
     * @return Duration as AampTime
     */
    AampTime GetDuration() const;

    /**
     * @brief Set discontinuity flag
     * @param discontinuity True if this fragment has a discontinuity
     */
    void SetDiscontinuity(bool discontinuity);

    /**
     * @brief Check if fragment has discontinuity
     * @return True if fragment has discontinuity
     */
    bool HasDiscontinuity() const;

    /**
     * @brief Set whether this is an init fragment
     * @param isInit True if this is an init fragment
     */
    void SetInitFragment(bool isInit);

    /**
     * @brief Check if this is an init fragment
     * @return True if this is an init fragment
     */
    bool IsInitFragment() const;

    /**
     * @brief Add a chunk to this fragment (for chunk-based fragments)
     * @param chunkData Chunk data to add
     * @param chunkSize Size of chunk data
     */
    void AddChunk(const uint8_t* chunkData, size_t chunkSize);

    /**
     * @brief Check if fragment is complete (all chunks received)
     * @return True if fragment is complete
     */
    bool IsComplete() const;

    /**
     * @brief Get number of chunks added to this fragment
     * @return Number of chunks added
     */
    size_t GetChunkCount() const;

    /**
     * @brief Clear fragment data and reset state
     */
    void Clear();

    /**
     * @brief Copy fragment data from another fragment
     * @param other Source fragment to copy from
     * @param length Optional length limit for partial copy
     */
    void CopyFrom(const AampFragment& other, size_t length = 0);

private:
    mutable std::mutex mMutex;           ///< Mutex for thread safety
    std::string mUrl;                    ///< Fragment URL
    
    // === Fragment Data ===
    std::vector<uint8_t> mFragmentData;  ///< Fragment data buffer (for both complete fragments and accumulated chunks)
    FragmentType mType;                  ///< Fragment type (complete or chunk-based)
    size_t mChunkCount;                  ///< Number of chunks added (for chunk-based fragments)
    
    // === Fragment Metadata ===
    AampTime mPosition;                  ///< Position in playlist
    AampTime mDuration;                  ///< Fragment duration
    bool mIsInitFragment;                ///< True if this is an init fragment
    bool mHasDiscontinuity;              ///< True if fragment has discontinuity
    bool mIsComplete;                    ///< True if all chunks have been received (for chunk-based fragments)
};

#endif // AAMP_FRAGMENT_H
