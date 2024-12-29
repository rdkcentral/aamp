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
//
//  StringEx.h

/**
 * @file StringEx.h
 * @brief
 */

#ifndef FOG_CLI_STRING_H
#define FOG_CLI_STRING_H

#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
 #include <functional>

using std::string;

/**
 * Augmented string class
 */

/**
 * @class   String
 * @brief   Augmented string class
 */
class String : public string {

public:

    String (): string() {

    }

    String(string s): string(s) { }

    bool endsWith(string ending) {
        return std::equal(begin() + size() - ending.size(), end(), ending.begin());
    }

    /**
     * In-place lowercase
     */
	void toLower() {
		std::transform( string::begin(), string::end(), string::begin(),
					   []( int ch ) { return std::tolower(ch); } );
	}

    bool startsWith(const string &o) {
		if (size() < o.size()) return false;

        return std::equal(begin(), begin()+o.size(), o.begin());
    }

    /**
     * In-place trim
     */
    void trim() {
        char *c, *begin, *end;

        c = (char *) c_str();

        while(*c == ' ' || *c == '\n' || *c == '\t' || *c == '\r') c++;
        begin = c;

        c = ((char *) c_str()) + size() - 1;
        while(*c == ' ' || *c == '\n' || *c == '\t' || *c == '\r') c--;
        end = c + 1;

        string s(begin, end);
        swap(s);
    }
};


#endif //FOG_CLI_STRING_H
