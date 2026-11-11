/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file AampTrackSet.h
 * @brief Set of track media types and its atomic holder
 */

#ifndef AAMP_TRACK_SET_H
#define AAMP_TRACK_SET_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include "AampDefine.h"
#include "AampMediaType.h"

/**
 * @class AampTrackSet
 * @brief Set of track media types (video, audio, subtitle)
 *
 * Purpose: records which track slots (video, audio, subtitle) are active,
 * independent of their position.
 *
 * Only the first AAMP_TRACK_COUNT media types are track types; any other
 * value is never contained and is ignored by With().
 *
 * Iteration is in ascending AampMediaType order, which is primary-first
 * (video, audio, subtitle). Use Reversed() to iterate in descending order.
 *
 * A value type, trivially copyable, so a copy is a consistent snapshot.
 */
class AampTrackSet
{
public:
	/** Number of track types a set can hold. */
	static constexpr size_t MAX_TRACK_TYPES = AAMP_TRACK_COUNT;
	static_assert(MAX_TRACK_TYPES <= 8, "AampTrackSet stores one bit per track type in a uint8_t");

	/** Forward or reverse iterator over the media types in a set. */
	template <bool REVERSE>
	class BasicIterator
	{
	public:
		constexpr explicit BasicIterator(uint8_t bits) : mBits{bits}
		{
		}

		constexpr AampMediaType operator*() const
		{
			return static_cast<AampMediaType>(NextIndex());
		}

		constexpr BasicIterator &operator++()
		{
			mBits = static_cast<uint8_t>(mBits & ~(1u << NextIndex()));
			return *this;
		}

		constexpr bool operator!=(const BasicIterator &other) const
		{
			return mBits != other.mBits;
		}

	private:
		/** Index of the next type to yield; mBits must not be empty. */
		constexpr unsigned NextIndex() const
		{
			unsigned index{REVERSE ? static_cast<unsigned>(MAX_TRACK_TYPES) - 1u : 0u};
			while (0u == (mBits & (1u << index)))
			{
				index = REVERSE ? index - 1u : index + 1u;
			}
			return index;
		}

		uint8_t mBits; ///< Types not yet visited
	};

	using Iterator = BasicIterator<false>;

	/** Range adaptor for descending iteration. */
	class ReverseView
	{
	public:
		constexpr explicit ReverseView(uint8_t bits) : mBits{bits}
		{
		}

		constexpr BasicIterator<true> begin() const
		{
			return BasicIterator<true>{mBits};
		}

		constexpr BasicIterator<true> end() const
		{
			return BasicIterator<true>{0u};
		}

	private:
		uint8_t mBits; ///< Types to visit
	};

	constexpr AampTrackSet() = default;

	/** Construct a set holding the given types. */
	constexpr AampTrackSet(std::initializer_list<AampMediaType> types)
	{
		for (const AampMediaType type : types)
		{
			mBits = static_cast<uint8_t>(mBits | Bit(type));
		}
	}

	/** @return true if the set holds the track type */
	constexpr bool Contains(AampMediaType type) const
	{
		return 0u != (mBits & Bit(type));
	}

	/** @return number of track types in the set */
	constexpr size_t Size() const
	{
		size_t count{0};
		for (uint8_t bits{mBits}; 0u != bits; bits = static_cast<uint8_t>(bits & (bits - 1u)))
		{
			++count;
		}
		return count;
	}

	/** @return true if the set holds no track type */
	constexpr bool Empty() const
	{
		return 0u == mBits;
	}

	/** @return a copy of the set that also holds the track type */
	constexpr AampTrackSet With(AampMediaType type) const
	{
		AampTrackSet result = *this;
		result.mBits = static_cast<uint8_t>(mBits | Bit(type));
		return result;
	}

	/** @return a copy of the set without the track type */
	constexpr AampTrackSet Without(AampMediaType type) const
	{
		AampTrackSet result = *this;
		result.mBits = static_cast<uint8_t>(mBits & ~Bit(type));
		return result;
	}

	/** @return a view that iterates the set in descending media type order */
	constexpr ReverseView Reversed() const
	{
		return ReverseView{mBits};
	}

	constexpr Iterator begin() const
	{
		return Iterator{mBits};
	}

	constexpr Iterator end() const
	{
		return Iterator{0u};
	}

	constexpr bool operator==(const AampTrackSet &other) const
	{
		return mBits == other.mBits;
	}

	constexpr bool operator!=(const AampTrackSet &other) const
	{
		return mBits != other.mBits;
	}

private:
	/** @return the bit for a track type, or 0 if it is not a track type */
	static constexpr unsigned Bit(AampMediaType type)
	{
		return (static_cast<size_t>(type) < MAX_TRACK_TYPES) ? (1u << static_cast<unsigned>(type)) : 0u;
	}

	uint8_t mBits{0}; ///< One bit per track type, indexed by AampMediaType
};

static_assert(std::is_trivially_copyable<AampTrackSet>::value, "AampTrackSet must be trivially copyable");

/**
 * @class AampAtomicTrackSet
 * @brief Lock-free holder of an AampTrackSet shared between threads
 *
 * Load() returns a snapshot; a loop that iterates the snapshot sees one
 * consistent set even if another thread changes it meanwhile.
 */
class AampAtomicTrackSet
{
public:
	/** @return a snapshot of the current set */
	AampTrackSet Load() const
	{
		return mSet.load();
	}

	/** Replace the set. */
	void Store(AampTrackSet set)
	{
		mSet.store(set);
	}

	/** Remove every track type. */
	void Clear()
	{
		mSet.store(AampTrackSet{});
	}

	/** Add a track type without losing a concurrent change to another type. */
	void Add(AampMediaType type)
	{
		auto expected = mSet.load();
		while (!mSet.compare_exchange_weak(expected, expected.With(type)))
		{
		}
	}

private:
	std::atomic<AampTrackSet> mSet{AampTrackSet{}}; ///< Current set
};

#endif // AAMP_TRACK_SET_H
