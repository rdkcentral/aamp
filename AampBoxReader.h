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

/**************************************
 * @file AampBoxReader.h
 * @brief Lightweight cursor for reading big-endian ISOBMFF box fields.
 **************************************/

#ifndef __AAMP_BOX_READER_H__
#define __AAMP_BOX_READER_H__

#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @class BoxReader
 * @brief Lightweight cursor for reading big-endian ISOBMFF box fields.
 *
 * Provides typed Read<T>() and Skip<T>() operations over a raw byte
 * buffer. The caller is responsible for validating the box size field
 * before reading individual fields; no bounds checking is performed.
 *
 * Example usage:
 * @code
 *   BoxReader reader{buffer};
 *   auto size      = reader.Read<uint32_t>();
 *   auto type      = reader.Read<uint32_t>();
 *   auto timescale = reader.Read<uint32_t>();
 *   reader.Skip(12 * segmentIndex);
 * @endcode
 */
class BoxReader final
{
public:
	/**
	 * @brief Construct a BoxReader positioned at @p data.
	 * @param data  Pointer to the first byte to read. Must not be null.
	 */
	constexpr explicit BoxReader(const uint8_t *data) noexcept
		: mCursor{data}
	{
	}

	/// Non-copyable — cursor state should not be accidentally duplicated.
	BoxReader(const BoxReader &)            = delete;
	BoxReader &operator=(const BoxReader &) = delete;

	/// Movable so callers can transfer ownership when needed.
	BoxReader(BoxReader &&)            = default;
	BoxReader &operator=(BoxReader &&) = default;

	/**
	 * @brief Read a big-endian integer field and advance the cursor.
	 *
	 * @tparam T  Unsigned integral type. sizeof(T) determines the field
	 *            width. Supported widths: 1, 2, 4, 8 bytes.
	 * @return    The value read, converted to host byte order.
	 */
	template <typename T>
	[[nodiscard]] T Read() noexcept
	{
		static_assert(std::is_integral_v<T>,
					  "BoxReader::Read<T> requires an integral type");
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 ||
					  sizeof(T) == 4 || sizeof(T) == 8,
					  "BoxReader::Read<T> supports 1/2/4/8-byte types only");

		uint64_t val{0};
		for (size_t i = 0; i < sizeof(T); ++i)
		{
			val = (val << 8) | static_cast<uint8_t>(mCursor[i]);
		}
		mCursor += sizeof(T);
		return static_cast<T>(val);
	}

	/**
	 * @brief Skip a typed field without reading it.
	 *
	 * @tparam T  Integral type whose sizeof determines the skip width.
	 *            Supported widths: 1, 2, 4, 8 bytes.
	 */
	template <typename T>
	void Skip() noexcept
	{
		static_assert(std::is_integral_v<T>,
					  "BoxReader::Skip<T> requires an integral type");
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 ||
					  sizeof(T) == 4 || sizeof(T) == 8,
					  "BoxReader::Skip<T> supports 1/2/4/8-byte types only");

		mCursor += sizeof(T);
	}

	/**
	 * @brief Skip a runtime number of bytes.
	 * @param n  Number of bytes to advance the cursor.
	 */
	void Skip(size_t n) noexcept
	{
		mCursor += n;
	}

private:
	const uint8_t *mCursor; ///< Current read position within the buffer.
};

#endif /* __AAMP_BOX_READER_H__ */
