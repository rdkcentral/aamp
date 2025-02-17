/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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

#ifndef __ACCESSIBILITY__
#define __ACCESSIBILITY__

#include <string>

/**
 * @class Accessibility
 * @brief Data type to store Accessibility Node data
 */
class Accessibility
{
private:
	std::string strSchemeId;
	std::string strValue;
	int intValue; // strValue parsed as non-negative integer, or -1 if invalid

	/**
	 * @brief string to non-negative decimal number conversion method (similar to std::stoi)
	 * @param str input string containing  number to parse
	 * @return -1 if parse failure, or non-negative number on success
	 */
	int parseNonNegativeDecimalInt(const std::string& str)
	{
		int rc = 0;
		const char * src = str.c_str();
		char ch = *src++;
		if( ch == 0x00 )
		{ // empty string
			rc = -1;
		}
		else
		{ // at least one character
			do
			{
				if( !std::isdigit(static_cast<unsigned char>(ch) ) )
				{ // found non-digit
					rc = -1;
					break;
				}
				rc = rc*10 + (ch-'0'); // incorporate this digit in integer result
				if( rc<0 )
				{ // overflow - huge number not fitting into an int
					rc = -1;
					break;
				}
				ch = *src++;
			} while( ch );
		}
		return rc;
	}
	
  public:
	Accessibility(std::string schemeId, std::string val): strSchemeId(schemeId), intValue(-1), strValue(val)
	{
		intValue = parseNonNegativeDecimalInt(val);
	};

	Accessibility():strSchemeId(""), intValue(-1), strValue("") {};

	void setAccessibilityData(std::string schemeId, const std::string &val)
	{
		strSchemeId = schemeId;
		intValue = parseNonNegativeDecimalInt(val);
		strValue = (intValue>=0)?"":val;
	};

	void setAccessibilityData(std::string schemeId, int val)
	{
		strSchemeId = schemeId;
		intValue = (val>=0)?val:-1;
		strValue = "";
	};

	const char * getTypeName() const {return intValue>=0?"int_value":"string_value";};
	const std::string& getSchemeId() const {return strSchemeId;};
	int getIntValue() const {return intValue;};
	const std::string& getStrValue() const {return strValue;};

	void clear()
	{
		strSchemeId = "";
		intValue = -1;
		strValue = "";
	};

	bool operator == (const Accessibility& track) const
	{
		return ((strSchemeId == track.strSchemeId) &&
			(intValue>=0?(intValue == track.intValue):(strValue == track.strValue)));
	};

	bool operator != (const Accessibility& track) const
	{
		return ((strSchemeId != track.strSchemeId) ||
			(intValue>=0?(intValue != track.intValue):(strValue != track.strValue)));
	};
	
	Accessibility& operator = (const Accessibility& track)
	{
		strSchemeId = track.strSchemeId;
		intValue = track.intValue;
		strValue = track.strValue;
		return *this;
	};

	/**
	 * @brief serialze to json-like syntax
	 */
	std::string print()
	{
		std::string retVal;
		if( strSchemeId.empty() )
		{
			retVal = "NULL";
		}
		else
		{
			retVal = "{ scheme:" + strSchemeId + ", " + getTypeName() + ":" + strValue + " }";
		}
		return retVal;
	};
};

#endif // __ACCESSIBILITY__

