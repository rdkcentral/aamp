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
* @file lstring.h
* @brief abstract data type for a lightweight trivially-copiable string defined by (ptr,length)
* @note includes HLS-friendly parsing convenience methods, i.e. ParseAttrList
* @note similar to std::basic_string_view from <string_view>
*/
#ifndef __LSTRING_H__
#define __LSTRING_H__

#include <stddef.h>
#include <assert.h>
#include <string>
#include "AampLogManager.h"

const char CHAR_CR = '\r'; // 0x0d
const char CHAR_LF = '\n'; // 0x0a
const char CHAR_QUOTE = '\"'; // 0x22

class lstring
{
private:
	const char *ptr;
	size_t len;
	
public:
	lstring()
	{
		ptr = NULL;
		len = 0;
	}
	
	lstring( const char *cstring, size_t sz )
	{
		ptr = cstring;
		len = sz;
	}
	
	lstring( const lstring &other )
	{
		ptr = other.ptr;
		len = other.len;
	}
	
	~lstring()
	{
	}
	
	size_t length( void ) const
	{
		return len;
	}
	
	int getLen( void ) const
	{ // for use with printf( "%.*s", lstring.getLen(), lstring.getPtr ); format option
		return (int)len;
	}
	
	const char *getPtr( void ) const
	{
		return ptr;
	}
	
	void clear(void)
	{ // reset back to empty string
		ptr = NULL;
		len = 0;
	}
	
	size_t find( char c ) const
	{ // returns offset to first occurrence of character, or length if not found
		size_t rc;
		for( rc=0; rc<len; rc++ )
		{
			if( ptr[rc]==c )
			{
				break;
			}
		}
		return rc;
	}
	
	lstring mystrpbrk( void )
	{ // extract next LF-delimited substring
		lstring token;
		size_t delim = find(CHAR_LF);
		token = lstring(ptr,delim);
		while( token.peekLastChar() == CHAR_CR)
		{ // trim any final CR characters
			token.len--;
		}
		removePrefix(delim+1);
		return token;
	}
	
	void removePrefix( size_t n = 1 )
	{
		if( n<len )
		{
			ptr += n;
			len -= n;
		}
		else
		{
			len = 0;
		}
	}
	
	bool equal( const char *cstring )
	{
		size_t n = 0;
		while( n < len )
		{
			char c = *cstring++;
			if( ptr[n++] != c )
			{
				break;
			}
		}
		return *cstring<' ';// == 0x00;
	}
	
	bool SubStringMatch( const char *cstring ) const
	{
		size_t n = 0;
		for (;;)
		{
			char c = *cstring++;
			if (c == 0)
			{
				return true;
			}
			if( ptr[n++] != c)
			{
				return false;
			}
		}
	}
	
	bool startswith( char c ) const
	{
		return len>0 && *ptr == c;
	}
	
	bool removePrefix( const char *prefix )
	{
		size_t n = 0;
		for (;;)
		{
			char c = *prefix++;
			if (c == 0)
			{
				removePrefix(n);
				return true;
			}
			if( ptr[n++] != c)
			{
				return false;
			}
		}
	}
	
	lstring substr( int offset ) const
	{
		lstring rc;
		rc.ptr = ptr+offset;
		rc.len = len-offset;
		return rc;
	}
	
	long long atoll() const
	{
		long long rc = 0;
		for( int i=0; i<len; i++ )
		{
			char c = ptr[i];
			if( c>='0' && c<='9' )
			{
				rc *= 10;
				rc += (c-'0');
			}
			else
			{
				break;
			}
		}
		return rc;
	}
	
	long atol() const
	{
		return (long)atoll();
	}
	
	int atoi() const
	{
		return (int)atoll();
	}
	
// -----------------------------------------------------------------------------
// Safe atof() implementation for lstring
//
// This function provides a robust, fault‑tolerant conversion of a character
// buffer into a floating‑point value. It is designed specifically for parsing
// HLS playlist fields (e.g., EXTINF durations) where malformed or unexpected
// input must never cause a thread crash.
//
// Key Features:
//   • Supports leading‑decimal formats such as ".25" and "-.75", treating them
//     as valid numbers (0.25 and -0.75 respectively).
//
//   • Handles leading whitespace, optional sign characters, and stops cleanly
//     at commas or trailing whitespace — matching common HLS formatting.
//
//   • Detects malformed numeric sequences such as multiple decimal points,
//     invalid characters, or empty input.
//
//   • Logs the exact character index where parsing failed, along with the full
//     input buffer, making debugging of malformed playlist values far easier.
//
//   • Never allows exceptions to escape the function. All parsing errors are
//     caught internally, logged, and a safe fallback value (0.0) is returned.
//     This prevents std::terminate() from being triggered inside worker threads.
//
// This function is intentionally strict about what constitutes a valid numeric
// character, but forgiving enough to handle real‑world HLS formatting quirks.
// -----------------------------------------------------------------------------
    
double atof() const
    {
        try
        {
            if (len == 0)
                throw std::runtime_error("empty string at index 0");

            int i = 0;

            // Skip leading whitespace
            while (i < len && std::isspace(ptr[i]))
                i++;

            // If we reached the end, the string was only whitespace
            if (i >= len)
                throw std::runtime_error("empty string at index 0");

            // Optional sign
            int sign = 1;
            if (ptr[i] == '-' || ptr[i] == '+')
            {
                if (ptr[i] == '-')
                    sign = -1;
                i++;
            }

            // After optional sign, the first character MUST be digit or '.'
            // Otherwise it's an invalid number like "abc" or "+x5"
            if (i >= len || !(std::isdigit(ptr[i]) || ptr[i] == '.'))
            {
                throw std::runtime_error(
                    std::string("unexpected character '") + ptr[i] +
                    "' at index " + std::to_string(i)
                );
            }

            long long ival = 0;     // integer part
            long long frac = 0;     // fractional part
            long long fracDiv = 1;  // divisor for fractional digits
            bool afterDecimal = false;

            // Main parsing loop
            for (; i < len; i++)
            {
                char c = ptr[i];

                if (c >= '0' && c <= '9')
                {
                    // Accumulate integer or fractional digits
                    if (!afterDecimal)
                        ival = ival * 10 + (c - '0');
                    else
                    {
                        frac = frac * 10 + (c - '0');
                        fracDiv *= 10;
                    }
                }
                else if (c == '.')
                {
                    // Only one decimal point allowed
                    if (afterDecimal)
                    {
                        throw std::runtime_error(
                            "multiple decimal points at index " + std::to_string(i)
                        );
                    }
                    afterDecimal = true;
                }
                else
                {
                    // STOP PARSING on any non-numeric character
                    // This allows "3.25e", "3.25xyz", "3.1x5", etc.
                    break;
                }
            }

            // Combine integer + fractional parts
            double result = (double)ival + (double)frac / (double)fracDiv;
            return sign * result;
        }
        catch (const std::exception& e)
        {
            // Log detailed error message
            AAMPLOG_ERR("ERROR in lstring::atof(): unknown exception | input=\"%.*s\" (len=%lu)", (int)len, ptr, len );
            return 0.0;
        }
        catch (...)
        {
            AAMPLOG_ERR("ERROR in lstring::atof(): unknown exception | input=\"%.*s\" (len=%lu)", (int)len, ptr, len );
            return 0.0;
        }
    }

	bool empty( void ) const
	{
		return len==0;
	}
	
	std::string tostring( void ) const
	{
		return std::string(ptr,len);
	}
	
	std::string GetAttributeValueString( void ) const
	{
		std::string rc;
		if( len>=2 && startswith(CHAR_QUOTE) )
		{ // strip quotes
			rc = std::string( ptr+1, len-2 );
		}
		else
		{
			rc = "NONE";
		}
		return rc;
	}
	
	void ParseAttrList( void(*cb)(lstring attr, lstring value, void *context), void *context) const
	{
		lstring iter = *this;
		for(;;)
		{
			iter.stripLeadingSpaces();
			if( iter.startswith('\r') )
			{ // break out on carriage return
				return;
			}
			
			lstring attr = iter;
			attr.len = 0;
			for(;;)
			{
				if( iter.empty() ) return;
				char c = iter.popFirstChar();
				if( c=='=' ) break;
				attr.len++;
			}
			lstring value = iter;
			value.len = 0;
			bool inQuote = false;
			for(;;)
			{
				if( iter.empty() ) break;
				char c = iter.popFirstChar();
				if( c==0x00 )
				{ // avoid parsing into trailing 0x00
					break;
				}
				if (c == ',' && !inQuote)
				{ // next attribute/value pair
					break;
				}
				value.len++;
				if( c == CHAR_QUOTE )
				{
					inQuote = !inQuote;
				}
			}
			cb(attr, value, context);
		} // read value
	} // next attr
	
	// below only used internally (for now), but keeping public
	char popFirstChar( void )
	{
		char c = 0;
		if( len>0 )
		{
			len--;
			c = *ptr++;
		}
		return c;
	}
	
	char peekLastChar() const
	{
		char c = 0;
		if( len )
		{
			c = ptr[len-1];
		}
		return c;
	}
	
	void stripLeadingSpaces( void )
	{
		while( startswith(' ') )
		{
			removePrefix();
		}
	}
	
	bool equal( const lstring &c )
	{
		return ( len == c.len && ptr == c.ptr );
	}
};

#endif // __LSTRING_H__

