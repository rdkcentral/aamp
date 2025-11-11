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

#ifndef FIREBOLT_RIALTO_SERVER_TEXT_TRACK_SESSION_H_
#define FIREBOLT_RIALTO_SERVER_TEXT_TRACK_SESSION_H_

#include "ITextTrackAccessor.h"
#include "ITextTrackSession.h"
#include <memory>
#include <optional>
#include <string>

namespace firebolt::rialto::server
{
class TextTrackSessionFactory : public ITextTrackSessionFactory
{
public:
    std::unique_ptr<ITextTrackSession> createTextTrackSession(const std::string &display) const override;
};

class TextTrackSession : public ITextTrackSession
{
public:
    TextTrackSession(const std::string &displayName, const ITextTrackAccessorFactory &textTrackAccessorFactory);
    ~TextTrackSession() override;
    bool resetSession(bool isMuted) override;
    bool pause() override;
    bool play() override;
    bool mute(bool mute) override;
    bool setPosition(uint64_t mediaTimestampMs) override;
    bool sendData(const std::string &data, int64_t displayOffsetMs = 0) override;
    bool setSessionWebVTTSelection() override;
    bool setSessionTTMLSelection() override;
    bool setSessionCCSelection(const std::string &service) override;
    bool associateVideoDecoder(uint64_t decoderId) override;
    bool isClosedCaptions() const override;

private:
    std::shared_ptr<ITextTrackAccessor> m_textTrackAccessor;
    ITextTrackAccessor::DataType m_dataType{ITextTrackAccessor::DataType::UNKNOWN};
    uint32_t m_sessionId{0};
    std::optional<std::string> m_ccService;
    std::optional<uint64_t> m_videoDecoderId;
};
} // namespace firebolt::rialto::server
#endif // FIREBOLT_RIALTO_SERVER_TEXT_TRACK_SESSION_H_
