/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#ifndef FIREBOLT_RIALTO_SERVER_I_TEXT_TRACK_SESSION_H_
#define FIREBOLT_RIALTO_SERVER_I_TEXT_TRACK_SESSION_H_

#include <memory>
#include <string>

namespace firebolt::rialto::server
{
class ITextTrackSession;

class ITextTrackSessionFactory
{
public:
    virtual ~ITextTrackSessionFactory() = default;
    static ITextTrackSessionFactory &getFactory();
    virtual std::unique_ptr<ITextTrackSession> createTextTrackSession(const std::string &display) const = 0;
};

class ITextTrackSession
{
public:
    ITextTrackSession() = default;
    virtual ~ITextTrackSession() = default;

    virtual bool resetSession(bool isMuted) = 0;
    virtual bool pause() = 0;
    virtual bool play() = 0;
    virtual bool mute(bool mute) = 0;
    virtual bool setPosition(uint64_t mediaTimestampMs) = 0;
    virtual bool sendData(const std::string &data, int64_t displayOffsetMs = 0) = 0;
    virtual bool setSessionWebVTTSelection() = 0;
    virtual bool setSessionTTMLSelection() = 0;
    virtual bool setSessionCCSelection(const std::string &service) = 0;
    virtual bool associateVideoDecoder(uint64_t decoderId) = 0;
    virtual bool isClosedCaptions() const = 0;
};
} // namespace firebolt::rialto::server
#endif // FIREBOLT_RIALTO_SERVER_I_TEXT_TRACK_SESSION_H_
