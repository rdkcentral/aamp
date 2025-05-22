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
// Utils.c
//

#include <sstream>
#include "Utils.h"
#include "AampUtils.h"

using namespace std;

// @formatter:off
const static regex isoDurationToSecondsRegex(
                "^(-)?"
                "P"
                "(?:([0-9,.]*)Y)?"
                "(?:([0-9,.]*)M)?"
                "(?:([0-9,.]*)W)?"
                "(?:([0-9,.]*)D)?"
                "(?:T?"
                "(?:([0-9,.]*)H)?"
                "(?:([0-9,.]*)M)?"
                "(?:([0-9,.]*)S)?"
                ")$");


/**
 * @brief   isoDuration to Seconds
 * @param   duration Duration
 * @param   defaultValue default seconds
 * @retval  seconds
 */
double isoDurationToSeconds(const string &duration, double defaultValue) {
    if (duration.empty()) return defaultValue;

    smatch match;
    regex_match(duration, match, isoDurationToSecondsRegex);

    string n, Y, M, W, D, h, m, s;

    n = match[1]; // n
    Y = match[2]; // Y;
    M = match[3]; // M;
    W = match[4]; // W;
    D = match[5]; // D;
    h = match[6]; // h;
    m = match[7]; // m;
    s = match[8]; // s;


    double d =
            (Y.empty() ? 0 : stod(Y) * (365 * 24 * 60 * 60)) +
            (M.empty() ? 0 : stod(M) * ( 30 * 24 * 60 * 60)) +
            (W.empty() ? 0 : stod(W) * (  7 * 24 * 60 * 60)) +
            (D.empty() ? 0 : stod(D) * (  1 * 24 * 60 * 60)) +
            (h.empty() ? 0 : stod(h) * (  1 *  1 * 60 * 60)) +
            (m.empty() ? 0 : stod(m) * (  1 *  1 *  1 * 60)) +
            (s.empty() ? 0 : stod(s) * (  1 *  1 *  1 *  1));

    return d * ((n.empty() || n == "+") ? 1 : -1);
}
// @formatter:on


/**
 * @brief   iso DateTime to Epoch Seconds
 * @param   isotime iso time
 * @param   defaultValue default seconds
 * @retval  seconds
 */
double isoDateTimeToEpochSeconds(string isotime, double defaultValue) {

    double ret_val = defaultValue;

    if (!isotime.empty())
    {
        ret_val = ISO8601DateTimeToUTCSeconds(isotime.c_str());
    }
    return ret_val;
}

/**
 * @brief   epoch Seconds to Iso DateTime
 * @param   seconds
 * @param   isoDateTime
 * @retval  true or false
 */
bool epochSecondsToIsoDateTime(double seconds, std::string& isoDateTime) {
    std::ostringstream ss;
    auto utc = (time_t) seconds;
    tm *time = std::gmtime(&utc);
    char buffer[80];
    strftime(buffer,80,"%Y-%m-%dT%H:%M:%SZ", time);
    ss << buffer;
    isoDateTime = ss.str();
    return true;
}

/**
 * @brief   epoch Seconds to Iso Duration
 * @param   seconds
 * @retval  PTsecondS
 */
std::string epochSecondsToIsoDuration(double seconds) {
    string s;
    s.append("PT").append(to_string(seconds)).append("S");
    return s;
}
