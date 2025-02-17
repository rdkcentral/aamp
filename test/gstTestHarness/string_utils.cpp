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
#include "string_utils.hpp"
#include <ctime>

uint64_t Number( const std::string &string )
{ // parseInt
	return std::stoull( string );
}

double parseFloat( const std::string &string )
{
	return atof( string.c_str() );
}

std::vector<std::string> splitString( const std::string &string, char c )
{
	size_t from = 0;
	std::vector<std::string> rc;
	for(;;)
	{
		auto delim = string.find(c,from);
		if( delim == std::string::npos )
		{
			rc.push_back(string.substr(from));
			//rc.push_back(string.substr(0,delim));
			return rc;
		}
		else
		{
			rc.push_back(string.substr(from,delim-from));
			from = delim+1;
		}
	}
}

bool starts_with( const std::string &string, const std::string &prefix )
{
	return string.rfind(prefix,0) == 0;
}

bool starts_with( const std::string &string, char prefix )
{
	return !string.empty() && string[0] == prefix;
}

bool ends_with( const std::string &string, char suffix )
{
	return !string.empty() && string.back() == suffix;
}

static double ISO8601DateTimeToUTCSeconds(const char *ptr)
{
	double timeSeconds = 0;
	if(ptr)
	{
		std::tm timeObj = { 0 };
		//Find out offset from utc by converting epoch
		std::tm baseTimeObj = { 0 };
		strptime("1970-01-01T00:00:00.", "%Y-%m-%dT%H:%M:%S.", &baseTimeObj);
		time_t offsetFromUTC = timegm(&baseTimeObj);
		//Convert input string to time
		const char *msString = strptime(ptr, "%Y-%m-%dT%H:%M:%S.", &timeObj);
		timeSeconds = timegm(&timeObj) - offsetFromUTC;

		if( msString && *msString )
		{ // at least one character following decimal point
			double ms = atof(msString-1); // back up and parse as float
			timeSeconds += ms; // include ms granularity
		}
	}
	return timeSeconds;
}

long long parseDate( const std::string &string )
{
	return (long long)(1000*ISO8601DateTimeToUTCSeconds(string.c_str()));
}

double parseDuration( const std::string &value )
{ // convert strings like "PT5M30.0S" to seconds
	double rc = 0;
	auto parts = splitString(value,'T');
	auto string = parts[0];
	if( starts_with(string,'P') )
	{
		unsigned long index = 1;
		auto delim = string.find('Y',index);
		if( delim!=std::string::npos )
		{
			rc += Number(string.substr(index,delim))*31104000; // 12 months
			index = delim+1;
		}
		delim = string.find('M',index);
		if( delim!=std::string::npos )
		{
			rc += Number(string.substr(index,delim))*2592000; // 30 days
			index = delim+1;
		}
		delim = string.find('D',index);
		if( delim!=std::string::npos)
		{
			rc += Number(string.substr(index,delim))*86400; // 24 hours
		}
	}
	if( parts.size()==2 )
	{
		string = parts[1];
		unsigned long index = 0;
		auto delim = string.find('H',index);
		if( delim!=std::string::npos )
		{
			rc += Number(string.substr(index,delim))*3600; // 60 minutes
			index = delim+1;
		}
		delim = string.find('M',index);
		if( delim!=std::string::npos )
		{
			rc += Number(string.substr(index,delim))*60; // 60 seconds
			index = delim+1;
		}
		delim = string.find('S',index);
		if( delim!=std::string::npos )
		{
			rc += parseFloat(string.substr(index,delim));
		}
	}
	return rc;
}

std::string PadDecimalWithLeadingZeros( int num, int places )
{ // zero-prefixed numbers
	auto name = std::to_string(num);
	//var name = num.toString();
	while( name.length()<places )
	{
		name = "0" + name;
	}
	return name;
}

std::string ExpandURL( std::string pat, std::map<std::string,std::string> param )
{ // replace patterns like $Number%03d$"
	auto pattern = splitString(pat,'$');
	std::string rc;
	int i = 0;
	while( i<pattern.size() )
	{
		rc += pattern[i++];
		if( i<pattern.size() )
		{
			std::string value;
			std::string key = pattern[i++];
			auto delim = key.find('%');//indexOf("%");
			if( delim==std::string::npos )
			{
				value = param[key];
			}
			else
			{
				std::string format = key.substr(delim+1);
				key = key.substr(0,delim);
				value = param[key];
				if( starts_with(format,'0') && ends_with(format,'d') )
				//if( format.startsWith("0") && format.endsWith("d") )
				{ // leading zeros, decimal
					int num = (int)Number(param[key]);
					int numDigits = (int)Number(format.substr(1,format.size()-2));
					value = PadDecimalWithLeadingZeros(num,numDigits);
				}
				else
				{
					//alert( "unsupported url format" );
				}
			}
			rc += value;
		}
	}
	return rc;
}
