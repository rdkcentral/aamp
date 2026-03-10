/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
 * @file AampGrowableBuffer.h
 * @brief Header file of helper functions for Growable Buffer class
 */

#ifndef __AAMP_GROWABLE_BUFFER_H__
#define __AAMP_GROWABLE_BUFFER_H__

#include <stddef.h>
#include <cstring>
#include <utility>
#include <assert.h>
#include <vector>
#include <cstdint>

class AampGrowableBuffer
{
public:
	AampGrowableBuffer(const char *name = "?") : buffer(), name(name) {}
	~AampGrowableBuffer();

	// Copy constructor
	AampGrowableBuffer(const AampGrowableBuffer &other)
		: buffer(other.buffer),
		  name{other.name}
	{
	}

	// Copy assignment
	AampGrowableBuffer &operator=(AampGrowableBuffer other)
	{
		swap(*this, other);
		return *this;
	}

	// Move constructor
	AampGrowableBuffer(AampGrowableBuffer &&other) noexcept
		: buffer(std::move(other.buffer)),
		  name{other.name}
	{
	}

	friend void swap(AampGrowableBuffer &first, AampGrowableBuffer &second) noexcept
	{
		using std::swap;
		swap(first.buffer, second.buffer);
		swap(first.name, second.name);
	}

	void Free(void);
	bool ReserveBytes(size_t len); /**< Reserve buffer capacity; returns false if allocation fails */
	bool AppendBytes( const void *ptr, size_t len ); /**< Append binary data; returns false if allocation fails */
	void clear( void ) { buffer.clear(); } // sets logical buffer size back to zero, without releasing available pre-allocated memory; allows a growable buffer to be recycled
	void Replace( AampGrowableBuffer *src );

	/**
	 * @brief Extract the internal vector for ownership transfer
	 * @return vector object (moved, not copied) containing the buffer data
	 * @note The internal buffer is cleared after extraction. Uses move semantics for zero-copy transfer.
	 */
	std::vector<uint8_t> ExtractVector( void );

	/**
	 * @brief Access the internal storage vector by reference.
	 *	This returns a reference to the internal std::vector<uint8_t> so callers
	 * can pass it directly to APIs that accept a vector reference without
	 * performing an extra copy. Prefer using the const overload if mutation is
	 * not required.
	 */
	std::vector<uint8_t> &GetVector() { return buffer; }
	const std::vector<uint8_t> &GetVector() const { return buffer; }

	char *GetPtr(void) { return buffer.capacity() ? reinterpret_cast<char *>(buffer.data()) : nullptr; }
	const char *GetPtr(void) const { return buffer.capacity() ? reinterpret_cast<const char *>(buffer.data()) : nullptr; }
	size_t size() const { return buffer.size(); }
	size_t capacity(void) const { return buffer.capacity(); } // should be opaque, but used in logging
	void SetLen(size_t l)
	{
		assert(l <= buffer.capacity());
		buffer.resize(l);
	}

	// Vector-like convenience wrappers (lower-case names to match std::vector)
	bool empty() const { return buffer.empty(); }
	void shrink_to_fit() { buffer.shrink_to_fit(); }
	void reserve(size_t n) { buffer.reserve(n); }
	void resize(size_t n)
	{
		buffer.resize(n);
	}
	void insert(typename std::vector<uint8_t>::const_iterator pos, const void *first, const void *last)
	{
		const uint8_t* start = static_cast<const uint8_t*>(first);
		const uint8_t* end = static_cast<const uint8_t*>(last);

		buffer.insert(pos, start, end);
	}
	void assign(const void *first, const void *last)
	{
		const uint8_t* start = static_cast<const uint8_t*>(first);
		const uint8_t* end = static_cast<const uint8_t*>(last);

		buffer.assign(start, end);
	}
	uint8_t *data() { return buffer.data(); }
	const uint8_t *data() const { return buffer.data(); }

private:
	const char *name;
	std::vector<uint8_t> buffer;  /**< Vector holding buffer data */
};

#endif /* __AAMP_GROWABLE_BUFFER_H__ */
