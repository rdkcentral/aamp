/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
 * @file lstring.hpp
 * @brief Lightweight, trivially-copyable string view defined by (pointer, length).
 *
 * lstring is a non-owning string view similar to std::basic_string_view. It
 * holds a pointer into an existing character buffer and a byte count; it never
 * allocates or frees memory. Copying an lstring copies only (ptr,len).
 *
 * Primary use-case is HLS manifest parsing inside AAMP:
 *   - ParseAttrList() walks a comma-separated attribute list and fires a
 *     callback for every (name, value) pair.
 *   - mystrpbrk() consumes the next LF-delimited line, stripping any trailing
 *     CR so callers see a clean token regardless of CRLF / LF line endings.
 *
 * @note This type is *trivially copyable* (trivial copy/move/dtor), but is not
 *       required to be trivially constructible.
 * @note All methods that do not mutate the view are marked @c const.
 * @note Invariant expectation: if @c len > 0 then @c ptr is non-null and points
 *       to at least @c len bytes. Debug builds assert this where practical.
 */

#ifndef LSTRING_HPP
#define LSTRING_HPP

#include <cstddef>
#include <cassert>
#include <cstring>
#include <climits>
#include <string>

/**
 * @def LSTRING_ENABLE_LEGACY_EQUAL
 * @brief Enables legacy wrapper overloads named equal(...).
 *
 * When enabled, lstring provides:
 *   - bool equal(const char*)  -> calls equalsCString(const char*)
 *   - bool equal(const lstring&) -> calls isSameView(const lstring&)
 *
 * These wrappers help preserve older call sites. Disable if you want to force
 * updated naming everywhere.
 */
#ifndef LSTRING_ENABLE_LEGACY_EQUAL
#define LSTRING_ENABLE_LEGACY_EQUAL 1
#endif

/**
 * @def LSTRING_EMPTY_PTR_NULL
 * @brief Forces ptr to become nullptr when the view becomes empty via removePrefix(n>=len).
 *
 * Default is 0 (string_view-like): empty means len==0, ptr may retain its prior value.
 * Enabling (1) makes empty consistently ptr==nullptr, which can be simpler, but may
 * subtly change behavior for code that uses pointer identity on empty views.
 */
#ifndef LSTRING_EMPTY_PTR_NULL
#define LSTRING_EMPTY_PTR_NULL 0
#endif

/**
 * @def LSTRING_SUBSTR_ALLOW_ENDPOS
 * @brief Controls substr(len) behavior.
 *
 * Default is 0 (backward-compatible with prior header): substr(offset>=len) returns empty().
 * If set to 1, substr(len) returns an empty view at end (ptr+len, 0), matching string_view.
 */
#ifndef LSTRING_SUBSTR_ALLOW_ENDPOS
#define LSTRING_SUBSTR_ALLOW_ENDPOS 0
#endif

/// Common ASCII characters used by HLS parsing helpers.
inline constexpr char CHAR_CR    = '\r'; // 0x0d
inline constexpr char CHAR_LF    = '\n'; // 0x0a
inline constexpr char CHAR_QUOTE = '"';  // 0x22

/**
 * @class lstring
 * @brief Non-owning, trivially-copyable string view (pointer + length).
 *
 * The pointed-to buffer is never owned and must remain valid for the lifetime
 * of the lstring object.
 */
class lstring
{
private:
	const char *ptr = nullptr; /**< Pointer into backing buffer; may be NULL when len == 0. */
	size_t      len = 0;       /**< Number of bytes in the view; excludes any null terminator. */

	inline void assertInvariant() const
	{
		assert(ptr != nullptr || len == 0);
	}

public:
	/**
	 * @brief Constructs an empty lstring (ptr == NULL, len == 0).
	 */
	constexpr lstring() noexcept = default;

	/**
	 * @brief Constructs an lstring view over an existing buffer.
	 * @param cstring Pointer to the first character of the backing buffer.
	 * @param sz      Number of bytes in the view (must not exceed buffer size).
	 */
	constexpr lstring(const char *cstring, size_t sz) noexcept
		: ptr(cstring), len(sz)
	{
		// Debug-time invariant: non-empty views must have a valid pointer.
		assertInvariant();
	}

	/**
	 * @brief Copy constructor — copies (pointer, length); no heap allocation.
	 */
	constexpr lstring(const lstring &) noexcept = default;

	/**
	 * @brief Copy assignment — copies (pointer, length); no heap allocation.
	 */
	constexpr lstring &operator=(const lstring &) noexcept = default;

	/**
	 * @brief Destructor — no-op; lstring does not own its backing buffer.
	 */
	~lstring() = default;

	/**
	 * @brief Returns the number of bytes in the view.
	 * @return Byte count (0 for empty).
	 */
	constexpr size_t length() const noexcept { return len; }

	/**
	 * @brief Returns the length as an int for use with printf "%.*s".
	 *
	 * Example: @code printf("%.*s", ls.getLen(), ls.getPtr()); @endcode
	 * @return Length clamped to INT_MAX to avoid truncation UB in printf width.
	 */
	int getLen() const noexcept
	{
		return (len > (size_t)INT_MAX) ? INT_MAX : (int)len;
	}

	/**
	 * @brief Returns a pointer to the first byte of the view.
	 * @return Pointer into the backing buffer; may be NULL if empty.
	 */
	constexpr const char *getPtr() const noexcept { return ptr; }

	/**
	 * @brief Convenience alias for getPtr().
	 */
	constexpr const char *data() const noexcept { return ptr; }

	/**
	 * @brief Resets to empty state (ptr = NULL, len = 0).
	 */
	constexpr void clear() noexcept
	{
		ptr = nullptr;
		len = 0;
	}

	/**
	 * @brief Parses the view as a floating-point number.
	 *
	 * Implemented in lstring.cpp to avoid pulling AampLogManager.h into this header.
	 * Parsing is HLS-tolerant; see implementation for details.
	 *
	 * @return Parsed double value, or 0.0 on failure.
	 */
	double atof() const;

	/**
	 * @brief Finds first occurrence of character @p c within the view.
	 * @param c Character to search for.
	 * @return Offset of first occurrence, or @c length() if not found.
	 */
	size_t find(char c) const noexcept
	{
		assertInvariant();
		for (size_t rc = 0; rc < len; rc++)
		{
			if (ptr[rc] == c)
			{
				return rc;
			}
		}
		return len;
	}

	/**
	 * @brief Extracts and returns the next LF-delimited line from the view.
	 *
	 * Advances this view past the line and its terminating LF (if present).
	 * Strips trailing CR characters from the returned token so callers see a
	 * clean line regardless of CRLF or LF line endings.
	 *
	 * If no LF is found, the entire remaining content is returned and this view
	 * is set to empty.
	 *
	 * @return lstring view of the next line, with trailing CR stripped.
	 */
	lstring mystrpbrk() noexcept
	{
		assertInvariant();
		size_t delim = find(CHAR_LF);
		lstring token(ptr, delim);

		while (token.peekLastChar() == CHAR_CR)
		{ // trim any trailing CR characters (handles CRLF line endings)
			token.len--;
		}

		// Consume the token plus LF if present.
		removePrefix(delim + 1);
		return token;
	}

	/**
	 * @brief Removes the first @p n bytes from the front of the view.
	 *
	 * If @p n >= len the view becomes empty.
	 *
	 * @param n Number of bytes to remove (default 1).
	 */
	void removePrefix(size_t n = 1) noexcept
	{
		assertInvariant();
		if (n < len)
		{
			ptr += n;
			len -= n;
		}
		else
		{
			len = 0;
#if LSTRING_EMPTY_PTR_NULL
			ptr = nullptr;
#endif
		}
	}

	/**
	 * @brief Strict content equality with a null-terminated C string.
	 *
	 * Returns true iff:
	 *   - the view has the same byte count as strlen(cstring), and
	 *   - all characters match.
	 *
	 * The comparison never reads past @p cstring's null terminator.
	 *
	 * @param cstring Null-terminated string to compare against (must be non-null).
	 * @return true if equal, false otherwise (including null input).
	 */
	bool equalsCString(const char *cstring) const noexcept
	{
		assertInvariant();
		if (!cstring) return false;

		for (size_t n = 0; n < len; n++)
		{
			if (cstring[n] == '\0')
			{ // cstring ended before view did
				return false;
			}
			if (ptr[n] != cstring[n])
			{
				return false;
			}
		}
		// All len chars matched; cstring must end here too
		return cstring[len] == '\0';
	}

	/**
	 * @brief Tests whether @p cstring is a prefix of this view.
	 *
	 * @param cstring Null-terminated prefix to match (must be non-null).
	 * @return true if this view starts with cstring, false otherwise.
	 */
	bool SubStringMatch(const char *cstring) const noexcept
	{
		assertInvariant();
		if (!cstring) return false;

		for (size_t n = 0;; n++)
		{
			char c = cstring[n];
			if (c == '\0')
			{
				return true;
			}
			if (n >= len)
			{
				return false;
			}
			if (ptr[n] != c)
			{
				return false;
			}
		}
	}

	/**
	 * @brief Tests whether the first character of the view equals @p c.
	 * @param c Character to test.
	 * @return true if non-empty and begins with c.
	 */
	bool startswith(char c) const noexcept
	{
		assertInvariant();
		return (len > 0 && ptr[0] == c);
	}

	/**
	 * @brief If @p prefix matches start of view, removes it and returns true.
	 *
	 * @param prefix Null-terminated prefix to match and consume (must be non-null).
	 * @return true if prefix matched and was removed; false otherwise.
	 */
	bool removePrefix(const char *prefix) noexcept
	{
		assertInvariant();
		if (!prefix) return false;

		for (size_t n = 0;; n++)
		{
			char c = prefix[n];
			if (c == '\0')
			{
				removePrefix(n);
				return true;
			}
			if (n >= len)
			{
				return false;
			}
			if (ptr[n] != c)
			{
				return false;
			}
		}
	}

	/**
	 * @brief Returns a sub-view starting at byte offset @p offset.
	 *
	 * If @p offset is negative or out of range, returns an empty view.
	 *
	 * @param offset Start position within the view.
	 * @return Sub-view, or empty if out of range.
	 */
	lstring substr(int offset) const noexcept
	{
		assertInvariant();
		if (offset < 0)
		{
			return lstring();
		}
		size_t off = (size_t)offset;

#if LSTRING_SUBSTR_ALLOW_ENDPOS
		if (off > len)
		{
			return lstring();
		}
		return lstring(ptr + off, len - off);
#else
		// Backward-compatible: offset >= len -> empty()
		if (off >= len)
		{
			return lstring();
		}
		return lstring(ptr + off, len - off);
#endif
	}

	/**
	 * @brief Parses the view as a non-negative decimal integer (long long).
	 *
	 * Scans digits until a non-digit is found. Does not parse sign.
	 *
	 * @return Parsed value, or 0 if empty or begins with non-digit.
	 *
	 * @note This function does not attempt to detect overflow; extremely long
	 *       inputs may overflow long long and invoke undefined behavior.
	 *       If that is a concern for your inputs, ask and we can harden it.
	 */
	long long atoll() const noexcept
	{
		assertInvariant();
		long long rc = 0;
		for (size_t i = 0; i < len; i++)
		{
			char c = ptr[i];
			if (c >= '0' && c <= '9')
			{
				rc = rc * 10 + (c - '0');
			}
			else
			{
				break;
			}
		}
		return rc;
	}

	/**
	 * @brief Parses the view as a non-negative decimal integer (long).
	 */
	long atol() const noexcept { return (long)atoll(); }

	/**
	 * @brief Parses the view as a non-negative decimal integer (int).
	 */
	int atoi() const noexcept { return (int)atoll(); }

	/**
	 * @brief Tests whether the view has zero length.
	 */
	constexpr bool empty() const noexcept { return (len == 0); }

	/**
	 * @brief Converts the view to a std::string (allocates).
	 *
	 * @return std::string with same content as the view.
	 */
	std::string tostring() const
	{
		assertInvariant();
		if (len == 0)
		{
			return std::string();
		}
		return std::string(ptr, len);
	}

	/**
	 * @brief Extracts the unquoted content of a quoted attribute value.
	 *
	 * If the view begins and ends with a double quote, returns inner content.
	 * Otherwise returns "NONE".
	 */
	std::string GetAttributeValueString() const
	{
		assertInvariant();
		if (len >= 2 && startswith(CHAR_QUOTE) && ptr[len - 1] == CHAR_QUOTE)
		{
			return std::string(ptr + 1, len - 2);
		}
		return "NONE";
	}

	/**
	 * @brief Parses a comma-separated HLS attribute list and fires @p cb for each pair.
	 *
	 * Format: name=value[,name=value]*
	 * Values may be optionally double-quoted; commas inside quotes do not split.
	 * Parsing stops on empty view, CR, or embedded 0x00.
	 *
	 * @param cb Callback invoked for each (attr, value) pair.
	 * @param context Opaque pointer passed to each callback invocation.
	 */
	void ParseAttrList(void(*cb)(lstring attr, lstring value, void *context), void *context) const
	{
		assertInvariant();
		lstring iter = *this;

		for (;;)
		{
			iter.stripLeadingSpaces();
			if (iter.startswith(CHAR_CR))
			{
				return;
			}

			// Parse attribute name up to '='
			lstring attr = iter;
			attr.len = 0;
			for (;;)
			{
				if (iter.empty()) return;
				char c = iter.popFirstChar();
				if (c == '=') break;
				attr.len++;
			}

			// Parse value up to comma (outside quotes) or end
			lstring value = iter;
			value.len = 0;
			bool inQuote = false;

			for (;;)
			{
				if (iter.empty()) break;
				char c = iter.popFirstChar();

				if (c == 0x00)
				{
					break;
				}
				if (c == ',' && !inQuote)
				{
					break;
				}

				value.len++;
				if (c == CHAR_QUOTE)
				{
					inQuote = !inQuote;
				}
			}

			cb(attr, value, context);
		}
	}

	/**
	 * @brief Removes and returns the first character of the view.
	 *
	 * If empty, returns '\0'.
	 */
	char popFirstChar() noexcept
	{
		assertInvariant();
		if (len == 0)
		{
			return '\0';
		}
		char c = *ptr;
		ptr++;
		len--;
		return c;
	}

	/**
	 * @brief Returns the last character without removing it.
	 *
	 * @return last char or '\0' if empty.
	 */
	char peekLastChar() const noexcept
	{
		assertInvariant();
		return (len == 0) ? '\0' : ptr[len - 1];
	}

	/**
	 * @brief Removes leading ASCII space characters (0x20) from the view.
	 *
	 * @note Deliberately trims only ' ' for backward compatibility with existing parsing.
	 */
	void stripLeadingSpaces() noexcept
	{
		assertInvariant();
		while (startswith(' '))
		{
			removePrefix(1);
		}
	}

	/**
	 * @brief Pointer-identity check: true if both views refer to same buffer region.
	 *
	 * This is NOT a content-equality check. Use equalsCString(), tostring(), or
	 * equalsContent() below.
	 */
	bool isSameView(const lstring &other) const noexcept
	{
		// No invariant asserts needed: identity check doesn't dereference pointers.
		return (len == other.len && ptr == other.ptr);
	}

	/**
	 * @brief Content equality check between two lstring objects.
	 *
	 * @return true iff same length and same bytes.
	 */
	bool equalsContent(const lstring &other) const noexcept
	{
		assertInvariant();
		other.assertInvariant();

		if (len != other.len)
		{
			return false;
		}
		if (len == 0)
		{
			return true;
		}
		return (std::memcmp(ptr, other.ptr, len) == 0);
	}

#if LSTRING_ENABLE_LEGACY_EQUAL
	/**
	 * @brief Legacy wrapper for equalsCString().
	 * @deprecated Prefer equalsCString().
	 */
	bool equal(const char *cstring) const noexcept { return equalsCString(cstring); }

	/**
	 * @brief Legacy wrapper for isSameView().
	 * @deprecated Prefer isSameView().
	 */
	bool equal(const lstring &other) const noexcept { return isSameView(other); }
#endif
};

#endif // LSTRING_HPP
