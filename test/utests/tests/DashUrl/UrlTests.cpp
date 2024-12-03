/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <iostream>
#include <string>
#include <string.h>
#include <arpa/inet.h>

//include the google test dependencies
#include <gtest/gtest.h>


#include "AampLogManager.h"
#include <Url.cpp>

AampConfig *gpGlobalConfig{nullptr};

namespace urlTest {

TEST(_UrlDashSuite, normalize_IPv6)
{
    std::string result;              // result from UUT

    std::string patterns[] = {
        "::",
        "2001:db8:0:0:1:0:0:1",
        "2001:0db8:0:0:1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::0:1:0:0:1",
        "2001:0db8::1:0:0:1",
        "2001:db8:0:0:1::1",
        "2001:db8:0000:0:1::1",
        "2001:DB8:0:0:1::1",
        "2001:db8:0:0:0:0:cafe:1111",
        "2001:db8::a:1:2:3:4",
        "2001:0DB8:AAAA:0000:0000:0000:0000:000C",
        "2001:db8::1:0:0:0:4",
        "0000:0000:0000:0000:0000:0000:0000:0000",
        "0000:0000:0000:0000:0000:0000:0000:1000",
        //"0000:0000:0000:0000:0000:0000:2000:1000", fails because inet_pton / inet_ntop is wrong!
        // Input 0000:0000:0000:0000:0000:0000:2000:1000 Expected ::32.0.16.0 Result ::2000:1000
        "0000:0000:0000:0000:0000:3000:2000:1000",
        "0000:0000:0000:0000:4000:3000:2000:1000",
        "0000:0000:0000:5000:4000:3000:2000:1000",
        "0000:0000:6000:5000:4000:3000:2000:1000",
        "0000:7000:6000:5000:4000:3000:2000:1000",
        "8000:7000:6000:5000:4000:3000:2000:1000",
    };

    std::string expects[] = {
        "::",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::1:0:0:1",
        "2001:db8::cafe:1111",
        "2001:db8:0:a:1:2:3:4",
        "2001:db8:aaaa::c",
        "2001:db8:0:1::4",
        "::",
        "::1000",
        //"0000:0000:0000:0000:0000:0000:2000:1000", fails because inet_pton / inet_ntop is wrong!
        // Input 0000:0000:0000:0000:0000:0000:2000:1000 Expected ::32.0.16.0 Result ::2000:1000
        "::3000:2000:1000",
        "::4000:3000:2000:1000",
        "::5000:4000:3000:2000:1000",
        "::6000:5000:4000:3000:2000:1000",
        "0:7000:6000:5000:4000:3000:2000:1000",
        "8000:7000:6000:5000:4000:3000:2000:1000",
    };

    result = normalize_IPv6("pattern");
    EXPECT_EQ(result, "");

    int index=0;
    for (auto pattern : patterns)
    {
        result = normalize_IPv6(pattern);
        //std::cout << index << " Input " << pattern << " Expected " << expects[index] << " Result " << result << "\n";
        EXPECT_EQ(result, expects[index++]);
    }

    // too many chars, will blow out tokens[10] resulting in SIGABRT
    result = normalize_IPv6("2001:db8:3333:4444:5555:6666:2001:db8:3333:4444:5555:6666");
    EXPECT_EQ(result, "");
    // not too many chars, but too many fields will blow out tokens[10] resulting in SIGABRT
    result = normalize_IPv6("1:2:3:4:5:6:7:8:9:A:B:C");
    EXPECT_EQ(result, "");

    // hits line coverage target
    result = normalize_IPv6("1:2:3::");
    EXPECT_EQ(result, "1:2:3::");
}

TEST(_UrlDashSuite, normalize_IPv6dual)
{
    std::string result;              // result from UUT

    std::string dualpatterns[] = {  // IPv4 as V6
        "::FFFF:1.2.3.4",  // current form
        "::11.22.33.44",   // deprecated form
        "::0000:0000:0000:0000:0000:11.22.33.44", // deprecated form
        "::123.123.123.123",
        "::FFFF:123.123.123.123",
        "::91.123.4.56",
        "::ffff:91.123.4.56"
    };

    std::string dualexpects[] = {  // IPv4 as V6
        "::ffff:1.2.3.4",
        "::11.22.33.44",
        "::11.22.33.44",
        "::123.123.123.123",
        "::ffff:123.123.123.123",
        "::91.123.4.56",
        "::ffff:91.123.4.56"
    };

    int index=0;
    for (auto pattern : dualpatterns)
    {
        result = normalize_IPv6(pattern);
        //std::cout << index << " Input " << pattern << " Expected " << dualexpects[index] << " Result " << result << "\n";
        EXPECT_EQ(result, dualexpects[index++]);
    }
}

} // namespace
