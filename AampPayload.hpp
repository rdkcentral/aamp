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
 * @file AampPayload.hpp
 * @brief Move-only, ref-counted fragment payload buffer.
 *
 * `Payload` is a thin, header-only wrapper around a
 * `std::shared_ptr<std::vector<uint8_t>>`. It exists to remove the deep-copy
 * fan-out paths in `MediaStreamContext` where the same fragment bytes had to
 * reach both the injector queue and the TSB writer queue (which previously
 * required `CachedFragment::Copy()` and `CachedFragment` copy construction).
 *
 *  - **Move-only at the type level**: copies fail to compile, so accidental
 *    deep copies of fragment bytes cannot creep back in.
 *  - **`Share()` is the only explicit duplication path**: it bumps the
 *    `shared_ptr` refcount; the bytes are *not* duplicated. After `Share()`
 *    both peers should treat the bytes as immutable.
 *  - **Vector-compatible API** (`data`, `size`, `empty`, `clear`, `resize`,
 *    `reserve`, `assign`, `insert`, iterators, indexing): existing call sites
 *    that previously used `std::vector<uint8_t>` continue to compile.
 *  - **Lazy allocation**: an empty `Payload` holds a null `shared_ptr` and
 *    allocates the underlying vector only on first mutation.
 */

#ifndef AAMP_PAYLOAD_HPP
#define AAMP_PAYLOAD_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

class Payload
{
public:
	using value_type      = std::uint8_t;
	using size_type       = std::size_t;
	using iterator        = std::vector<std::uint8_t>::iterator;
	using const_iterator  = std::vector<std::uint8_t>::const_iterator;

	Payload() noexcept = default;

	// Move-only: deep copies must fail to compile.
	Payload(const Payload&)            = delete;
	Payload& operator=(const Payload&) = delete;

	Payload(Payload&&) noexcept            = default;
	Payload& operator=(Payload&&) noexcept = default;

	/**
	 * @brief Implicit conversion to `std::vector<uint8_t>&` for legacy APIs.
	 *
	 * Many AAMP APIs (`GetFile`, `RetrieveFromInitFragmentCache`,
	 * `resetPTSOnAudioSwitch`, `setBuffer`, `IsoBmffHelper::*`, ...) take a
	 * `std::vector<uint8_t>&` (or `const&`). Providing these conversions
	 * keeps existing call sites compiling without manual `.GetVector()`
	 * sprinkles. The conversion is zero-cost: it returns a reference to the
	 * vector owned by the underlying `shared_ptr`.
	 *
	 * Detaches on write (copy-on-write): if the bytes are currently shared
	 * with another peer via `Share()`, allocate a private copy before
	 * returning a mutable reference. This preserves the historical
	 * deep-copy guarantee — legacy callers reach for `vector&` when they
	 * intend to mutate (resize, clear, std::move from, fill via curl) and
	 * must not corrupt any peer fragment.
	 */
	operator std::vector<std::uint8_t>&() &
	{
		detach_for_write();
		return *mBytes;
	}

	operator const std::vector<std::uint8_t>&() const & noexcept
	{
		return GetVector();
	}

	/**
	 * @brief Take ownership of an existing std::vector<uint8_t>.
	 *
	 * Used by the curl-callback / download-buffer handoff to move a
	 * fully-populated vector into a Payload without copying bytes.
	 * The source vector is left in a moved-from (empty) state.
	 */
	Payload& operator=(std::vector<std::uint8_t>&& v) noexcept
	{
		mBytes = std::make_shared<std::vector<std::uint8_t>>(std::move(v));
		return *this;
	}

	~Payload() = default;

	/**
	 * @brief Produce a peer Payload that shares the same bytes (refcount bump).
	 *
	 * Replaces the previous deep-copy fan-out (`CachedFragment::Copy`,
	 * `make_shared<CachedFragment>(*ptr)`). After calling `Share()` neither
	 * peer should mutate the underlying bytes; the vector is treated as
	 * effectively immutable.
	 */
	Payload Share() const noexcept
	{
		Payload p;
		p.mBytes = mBytes; // refcount++
		return p;
	}

	// ---- Vector-compatible read API -------------------------------------

	const std::uint8_t* data() const noexcept
	{
		return mBytes ? mBytes->data() : nullptr;
	}

	std::uint8_t* data() noexcept
	{
		return mBytes ? mBytes->data() : nullptr;
	}

	size_type size() const noexcept
	{
		return mBytes ? mBytes->size() : 0u;
	}

	bool empty() const noexcept
	{
		return !mBytes || mBytes->empty();
	}

	size_type capacity() const noexcept
	{
		return mBytes ? mBytes->capacity() : 0u;
	}

	const std::uint8_t& operator[](size_type i) const noexcept
	{
		return (*mBytes)[i];
	}

	std::uint8_t& operator[](size_type i) noexcept
	{
		return (*mBytes)[i];
	}

	const_iterator begin() const noexcept
	{
		return mBytes ? mBytes->begin() : kEmpty().begin();
	}

	const_iterator end() const noexcept
	{
		return mBytes ? mBytes->end() : kEmpty().end();
	}

	iterator begin() noexcept
	{
		ensure();
		return mBytes->begin();
	}

	iterator end() noexcept
	{
		ensure();
		return mBytes->end();
	}

	// ---- Vector-compatible mutation API ---------------------------------
	// Each mutator first detaches from any shared peer (copy-on-write):
	// after `Share()` neither peer should mutate the *common* bytes, so any
	// write here transparently allocates a private buffer. This preserves
	// the historical deep-copy guarantee that mutating one CachedFragment
	// did not corrupt another.

	void clear() noexcept
	{
		if (!mBytes)
		{
			return;
		}
		if (mBytes.use_count() > 1)
		{
			mBytes = std::make_shared<std::vector<std::uint8_t>>();
		}
		else
		{
			mBytes->clear();
		}
	}

	void reserve(size_type n)
	{
		detach_for_write();
		mBytes->reserve(n);
	}

	void resize(size_type n)
	{
		detach_for_write();
		mBytes->resize(n);
	}

	void push_back(std::uint8_t b)
	{
		detach_for_write();
		mBytes->push_back(b);
	}

	template<typename InputIt>
	void assign(InputIt first, InputIt last)
	{
		detach_for_write();
		mBytes->assign(first, last);
	}

	template<typename InputIt>
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		detach_for_write();
		return mBytes->insert(pos, first, last);
	}

	/**
	 * @brief Release the underlying storage and reset to empty.
	 *
	 * Equivalent to `aamp_utils::ClearAndRelease` for vectors: drops the
	 * shared buffer (or decrements its refcount) and leaves the Payload in
	 * the default-constructed state.
	 */
	void ClearAndRelease() noexcept
	{
		mBytes.reset();
	}

	/**
	 * @brief Read-only access to the underlying byte vector.
	 *
	 * Used at the few legacy interop sites (e.g. `AampCacheHandler::
	 * InsertToInitFragCache`) that still take a `const std::vector<uint8_t>&`.
	 * Returns a reference to a stable empty vector when the Payload is empty,
	 * so callers never see a null reference.
	 */
	const std::vector<std::uint8_t>& GetVector() const noexcept
	{
		return mBytes ? *mBytes : kEmpty();
	}

	/**
	 * @brief Yield a `std::vector<uint8_t>` for legacy consumer APIs that
	 * still take ownership by `std::vector<uint8_t>&&`.
	 *
	 * Rvalue-qualified so it cannot be called on a long-lived Payload. After
	 * this returns the source Payload is empty (mBytes reset).
	 *
	 *  - **Sole owner** (use_count == 1): the underlying vector is moved out
	 *    of the shared buffer with zero byte-copying.
	 *  - **Shared** (use_count > 1, i.e. after `Share()` for fan-out): a copy
	 *    is unavoidable because a peer still references the bytes. This is
	 *    the same cost the previous deep-copy fan-out always paid; we just
	 *    pay it once at injection time instead of once at fan-out time.
	 */
	std::vector<std::uint8_t> ExtractVector() &&
	{
		if (!mBytes)
		{
			return {};
		}
		std::vector<std::uint8_t> out;
		if (mBytes.use_count() == 1)
		{
			out = std::move(*mBytes);
		}
		else
		{
			out = *mBytes; // copy, peer still references the bytes
		}
		mBytes.reset();
		return out;
	}

	/**
	 * @brief How many Payload peers currently share these bytes.
	 *
	 * Returns 0 for an empty Payload, 1 for a sole owner, >1 after Share().
	 * Intended for assertions and diagnostics, not control flow.
	 */
	long use_count() const noexcept
	{
		return mBytes ? mBytes.use_count() : 0;
	}

private:
	void ensure()
	{
		if (!mBytes)
		{
			mBytes = std::make_shared<std::vector<std::uint8_t>>();
		}
	}

	// Detach for write: ensure mBytes is non-null AND that this Payload is
	// the sole owner of the byte buffer. If the buffer is currently shared
	// (use_count > 1), allocate a private copy before mutating. This
	// preserves the deep-copy guarantee for callers that previously held
	// their own vector copy.
	void detach_for_write()
	{
		if (!mBytes)
		{
			mBytes = std::make_shared<std::vector<std::uint8_t>>();
		}
		else if (mBytes.use_count() > 1)
		{
			mBytes = std::make_shared<std::vector<std::uint8_t>>(*mBytes);
		}
	}

	// Stable empty vector for begin()/end() on a null Payload.
	static const std::vector<std::uint8_t>& kEmpty() noexcept
	{
		static const std::vector<std::uint8_t> sEmpty;
		return sEmpty;
	}

	std::shared_ptr<std::vector<std::uint8_t>> mBytes;
};

#endif // AAMP_PAYLOAD_HPP
