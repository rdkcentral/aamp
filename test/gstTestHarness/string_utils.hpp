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
#ifndef string_utils_hpp
#define string_utils_hpp

#include <string>
#include <vector>
#include <map>
#include <cstdint> // for std::uint64_t

uint64_t Number( const std::string &string );

double parseFloat( const std::string &string );

std::vector<std::string> splitString( const std::string &string, char c );

bool starts_with( const std::string &string, const std::string &prefix );
bool starts_with( const std::string &string, char prefix );
bool ends_with( const std::string &string, char suffix );

double parseDuration( const std::string &string );

long long parseDate( const std::string &string );

std::string PadDecimalWithLeadingZeros( int num, int places );

std::string ExpandURL( std::string pat, std::map<std::string,std::string> param );

#endif /* string_utils_hpp */
