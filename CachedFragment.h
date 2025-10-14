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
 * @file CachedFragment.h
 * @brief CachedFragment class for holding cached fragment data
 */

#ifndef CACHED_FRAGMENT_H
#define CACHED_FRAGMENT_H

#include "AampGrowableBuffer.h"
#include "AampMediaType.h"
#include "priv_aamp.h"  // For BitsPerSecond and BitrateChangeReason definitions
#include <string>
#include <utility>  // For std::swap and std::move
#include <mutex>     // For thread safety

/**
 * @brief Structure holding the resolution of stream
 */
struct StreamResolution
{
	int width;			/**< Width in pixels*/
	int height;			/**< Height in pixels*/
	double framerate;	/**< Frame Rate in frames per second */

	StreamResolution(): width(0), height(0), framerate(0.0)
	{
	}
};

/**
 * @brief Structure holding the information of a stream.
 */
struct StreamInfo
{
	bool enabled;							/**< Indicates if the streamInfo profile is enabled */
	bool isIframeTrack;						/**< Indicates if the stream is iframe stream */
	bool validity;							/**< Indicates profile validity against user configured profile range */
	std::string codecs;						/**< Codec String */
	BitsPerSecond bandwidthBitsPerSecond;	/**< Bandwidth of the stream bps */
	StreamResolution resolution;			/**< Resolution of the stream */
	BitrateChangeReason reason;				/**< Reason for bitrate change */
	std::string baseUrl;					/**< Base URL of the stream */
	StreamInfo():enabled(false),isIframeTrack(false),validity(false),codecs(),bandwidthBitsPerSecond(0),resolution(),reason(eAAMP_BITRATE_CHANGE_BY_ABR),baseUrl(){};
};

/**
 * @enum FragmentType
 * @brief Type of fragment for migration to AampFragment
 */
enum class FragmentType
{
	COMPLETE_FRAGMENT,  /**< Complete fragment supplied as single unit (replaces mCachedFragment usage) */
	FRAGMENT_CHUNK      /**< Fragment chunk supplied incrementally (replaces mCachedFragmentChunks usage) */
};

/**
 * @brief Structure of cached fragment data
 *        Holds information about a cached fragment
 */
class CachedFragment
{
public:
	AampGrowableBuffer fragment;		/**< Buffer to keep fragment content */
	double position;					/**< Position in the playlist, in seconds */
	double duration;					/**< Duration of the fragment, in seconds; as specified in the manifest */
	bool initFragment;					/**< Flag indicating whether this fragment is an initialization fragment */
	bool discontinuity;					/**< Flag indicating that a PTS discontinuity occurs just before this fragment */
	bool isDummy;						/**< Flag indicating that this is a dummy fragment (e.g. for gap filling) */
	int profileIndex;					/**< Profile index; Updated internally */
	uint32_t timeScale;					/**< timescale of this fragment as read from manifest */
	std::string uri;					/**< for debug */
	StreamInfo cacheFragStreamInfo;		/**< Bitrate information associated with this fragment */
	AampMediaType type;					/**< AampMediaType info of the fragment */
	long long downloadStartTime;		/**< The start time of file download */
	long long discontinuityIndex;		/**< Discontinuity index */
	double PTSOffsetSec; 				/**< PTS offset to apply for this segment */
	double absPosition;					/**< Absolute position in seconds */
	FragmentType fragmentType;			/**< Type of fragment for migration strategy */

private:
	mutable std::mutex mMutex;			/**< Mutex for thread safety */

public:

	/**
	 * @brief Default constructor
	 */
	CachedFragment();

	/**
	 * @brief Copy constructor
	 * @param other Source CachedFragment to copy from
	 */
	CachedFragment(const CachedFragment& other);

	/**
	 * @brief Move constructor
	 * @param other Source CachedFragment to move from
	 */
	CachedFragment(CachedFragment&& other) noexcept;

	/**
	 * @brief Copy assignment operator
	 * @param other Source CachedFragment to copy from
	 * @return Reference to this object
	 */
	CachedFragment& operator=(const CachedFragment& other);

	/**
	 * @brief Move assignment operator
	 * @param other Source CachedFragment to move from
	 * @return Reference to this object
	 */
	CachedFragment& operator=(CachedFragment&& other) noexcept;

	/**
	 * @brief Swap contents with another CachedFragment
	 * @param other CachedFragment to swap with
	 */
	void swap(CachedFragment& other) noexcept;

	/**
	 * @brief Copy content from another CachedFragment
	 * @param other Source CachedFragment to copy from
	 * @param len Length of data to copy from the fragment buffer
	 */
	void Copy(CachedFragment* other, size_t len);

	/**
	 * @brief Clear all fragment data and reset to default values
	 */
	void Clear();

	/**
	 * @brief Get the fragment type
	 * @return The fragment type
	 */
	FragmentType GetFragmentType() const;

	/**
	 * @brief Set the fragment type
	 * @param type The fragment type to set
	 */
	void SetFragmentType(FragmentType type);

	/**
	 * @brief Thread-safe getter for position
	 * @return The position value
	 */
	double GetPosition() const;

	/**
	 * @brief Thread-safe setter for position
	 * @param pos The position value to set
	 */
	void SetPosition(double pos);

	/**
	 * @brief Thread-safe getter for duration
	 * @return The duration value
	 */
	double GetDuration() const;

	/**
	 * @brief Thread-safe setter for duration
	 * @param dur The duration value to set
	 */
	void SetDuration(double dur);

	/**
	 * @brief Thread-safe getter for URI
	 * @return Copy of the URI string
	 */
	std::string GetUri() const;

	/**
	 * @brief Thread-safe setter for URI
	 * @param newUri The URI string to set
	 */
	void SetUri(const std::string& newUri);

	/**
	 * @brief Thread-safe method to get fragment buffer length
	 * @return Length of the fragment buffer
	 */
	size_t GetFragmentLength() const;

	/**
	 * @brief Thread-safe method to check if fragment is empty
	 * @return True if fragment has no data
	 */
	bool IsEmpty() const;
};

/**
 * @brief Free function swap for CachedFragment
 * @param lhs First CachedFragment to swap
 * @param rhs Second CachedFragment to swap
 */
void swap(CachedFragment& lhs, CachedFragment& rhs) noexcept;

#endif // CACHED_FRAGMENT_H
