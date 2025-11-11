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

#include "TextTrackSession.h"
#include "ITextTrackAccessor.h"
#include "RialtoServerLogging.h"
#include <stdexcept>

namespace firebolt::rialto::server
{
ITextTrackSessionFactory &ITextTrackSessionFactory::getFactory()
{
    static TextTrackSessionFactory factory;
    return factory;
}

std::unique_ptr<ITextTrackSession> TextTrackSessionFactory::createTextTrackSession(const std::string &display) const
{
    return std::make_unique<TextTrackSession>(display, ITextTrackAccessorFactory::getFactory());
}

TextTrackSession::TextTrackSession(const std::string &displayName,
                                   const ITextTrackAccessorFactory &textTrackAccessorFactory)
{
    m_textTrackAccessor = textTrackAccessorFactory.getTextTrackAccessor();
    if (!m_textTrackAccessor)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to get TextTrackAccessor");
        throw std::runtime_error("Failed to get TextTrackAccessor");
    }

    std::optional<uint32_t> sessionId = m_textTrackAccessor->openSession(displayName);
    if (!sessionId)
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create TextTrack session");
        throw std::runtime_error("Failed to create TextTrack session");
    }

    m_sessionId = sessionId.value();
}

TextTrackSession::~TextTrackSession()
{
    m_textTrackAccessor->closeSession(m_sessionId);
}

bool TextTrackSession::resetSession(bool isMuted)
{
    // There is no direct way to clear TextTrack's data. The only option is to reset the session, but that also resets
    // the data type and mute values
    if (!m_textTrackAccessor->resetSession(m_sessionId))
    {
        return false;
    }

    bool wasDataTypeSelected = false;
    if (m_dataType == ITextTrackAccessor::DataType::WebVTT)
    {
        wasDataTypeSelected = setSessionWebVTTSelection();
    }
    else if (m_dataType == ITextTrackAccessor::DataType::TTML)
    {
        wasDataTypeSelected = setSessionTTMLSelection();
    }
    else if (m_dataType == ITextTrackAccessor::DataType::CC)
    {
        if (m_ccService.has_value())
        {
            wasDataTypeSelected = setSessionCCSelection(m_ccService.value());
            if (m_videoDecoderId.has_value())
            {
                wasDataTypeSelected = associateVideoDecoder(m_videoDecoderId.value());
            }
        }
        else
        {
            RIALTO_SERVER_LOG_ERROR("CC service not set");
            return false;
        }
    }

    if (!wasDataTypeSelected)
    {
        return false;
    }

    // changing the data type resets the mute value in TextTrack to its default (false), so we need to set mute
    // after selecting the data type
    return mute(isMuted);
}

bool TextTrackSession::pause()
{
    return m_textTrackAccessor->pause(m_sessionId);
}

bool TextTrackSession::play()
{
    return m_textTrackAccessor->play(m_sessionId);
}

bool TextTrackSession::mute(bool mute)
{
    return m_textTrackAccessor->mute(m_sessionId, mute);
}

bool TextTrackSession::setPosition(uint64_t mediaTimestampMs)
{
    return m_textTrackAccessor->setPosition(m_sessionId, mediaTimestampMs);
}

bool TextTrackSession::sendData(const std::string &data, int64_t displayOffsetMs)
{
    return m_textTrackAccessor->sendData(m_sessionId, data, m_dataType, displayOffsetMs);
}

bool TextTrackSession::setSessionWebVTTSelection()
{
    m_dataType = ITextTrackAccessor::DataType::WebVTT;
    m_ccService = std::optional<std::string>();
    return m_textTrackAccessor->setSessionWebVTTSelection(m_sessionId);
}

bool TextTrackSession::setSessionTTMLSelection()
{
    m_dataType = ITextTrackAccessor::DataType::TTML;
    m_ccService = std::optional<std::string>();
    return m_textTrackAccessor->setSessionTTMLSelection(m_sessionId);
}

bool TextTrackSession::setSessionCCSelection(const std::string &service)
{
    m_dataType = ITextTrackAccessor::DataType::CC;
    m_ccService = service;
    return m_textTrackAccessor->setSessionCCSelection(m_sessionId, service);
}

bool TextTrackSession::associateVideoDecoder(uint64_t decoderId)
{
    m_videoDecoderId = decoderId;
    std::string decoderIdStr = std::to_string(decoderId);
    return m_textTrackAccessor->associateVideoDecoder(m_sessionId, decoderIdStr);
}

bool TextTrackSession::isClosedCaptions() const
{
    return m_dataType == ITextTrackAccessor::DataType::CC;
}
} // namespace firebolt::rialto::server
