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

#include "TextTrackAccessor.h"

#include <cinttypes>
#include <stdexcept>
#include <string>

namespace firebolt::rialto::server
{
ITextTrackAccessorFactory &ITextTrackAccessorFactory::getFactory()
{
    static TextTrackAccessorFactory factory;
    return factory;
}

std::shared_ptr<ITextTrackAccessor> TextTrackAccessorFactory::getTextTrackAccessor() const
try
{
    static std::shared_ptr<TextTrackAccessor> textTrackAccessor{
        std::make_shared<TextTrackAccessor>(firebolt::rialto::wrappers::ITextTrackPluginWrapperFactory::getFactory()
                                                ->getTextTrackPluginWrapper(),
                                            firebolt::rialto::wrappers::IThunderWrapperFactory::getFactory()
                                                ->getThunderWrapper())};

    return textTrackAccessor;
}
catch (const std::exception &e)
{
    return nullptr;
}

TextTrackAccessor::TextTrackAccessor(
    const std::shared_ptr<firebolt::rialto::wrappers::ITextTrackPluginWrapper> &textTrackPluginWrapper,
    const std::shared_ptr<firebolt::rialto::wrappers::IThunderWrapper> &thunderWrapper)
    : m_textTrackPluginWrapper{textTrackPluginWrapper}, m_thunderWrapper{thunderWrapper}
{
    if (!createTextTrackControlInterface())
    {
        RIALTO_SERVER_LOG_ERROR("Failed to create TextTrack interfaces");
        throw std::runtime_error("Failed to create TextTrack interfaces");
    }
}

TextTrackAccessor::~TextTrackAccessor() {}

std::optional<uint32_t> TextTrackAccessor::openSession(const std::string &displayName)
{
    uint32_t sessionId = {};
    uint32_t result = m_textTrackWrapper->openSession(displayName, sessionId);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_MIL("TextTrack session %u created with display '%s'", sessionId, displayName.c_str());
        return sessionId;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to create TextTrack session with display '%s'; error '%s'", displayName.c_str(),
                            m_thunderWrapper->errorToString(result));
    return std::nullopt;
}

bool TextTrackAccessor::closeSession(uint32_t sessionId)
{
    uint32_t result = m_textTrackWrapper->closeSession(sessionId);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_MIL("TextTrack session %u closed", sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to close TextTrack session %u; error %s", sessionId,
                            m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::pause(uint32_t sessionId)
{
    uint32_t result = m_textTrackWrapper->pauseSession(sessionId);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_INFO("TextTrack session %u paused", sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to pause TextTrack session %u; error %s", sessionId,
                            m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::play(uint32_t sessionId)
{
    uint32_t result = m_textTrackWrapper->resumeSession(sessionId);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_INFO("TextTrack session %u resumed", sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to resume TextTrack session %u; error %s", sessionId,
                            m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::mute(uint32_t sessionId, bool mute)
{
    if (mute)
    {
        uint32_t result = m_textTrackWrapper->muteSession(sessionId);
        if (m_thunderWrapper->isSuccessful(result))
        {
            RIALTO_SERVER_LOG_INFO("TextTrack session %u muted", sessionId);
            return true;
        }

        RIALTO_SERVER_LOG_ERROR("Failed to mute TextTrack session %u; error %s", sessionId,
                                m_thunderWrapper->errorToString(result));
    }
    else
    {
        uint32_t result = m_textTrackWrapper->unmuteSession(sessionId);
        if (m_thunderWrapper->isSuccessful(result))
        {
            RIALTO_SERVER_LOG_INFO("TextTrack session %u unmuted", sessionId);
            return true;
        }

        RIALTO_SERVER_LOG_ERROR("Failed to unmute TextTrack session %u; error %s", sessionId,
                                m_thunderWrapper->errorToString(result));
    }

    return false;
}

bool TextTrackAccessor::setPosition(uint32_t sessionId, uint64_t mediaTimestampMs)
{
    uint32_t result = m_textTrackWrapper->sendSessionTimestamp(sessionId, mediaTimestampMs);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_INFO("TextTrack session %u set position to %" PRIu64, sessionId, mediaTimestampMs);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to set position of TextTrack session %u to  %" PRIu64 "; error %s", sessionId,
                            mediaTimestampMs, m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::sendData(uint32_t sessionId, const std::string &data, DataType datatype, int64_t displayOffsetMs)
{
    firebolt::rialto::wrappers::ITextTrackWrapper::DataType wrapperDataType{};
    if (datatype == DataType::WebVTT)
    {
        wrapperDataType = firebolt::rialto::wrappers::ITextTrackWrapper::DataType::WEBVTT;
    }
    else if (datatype == DataType::TTML)
    {
        wrapperDataType = firebolt::rialto::wrappers::ITextTrackWrapper::DataType::TTML;
    }
    else if (datatype == DataType::CC)
    {
        RIALTO_SERVER_LOG_WARN("Data received for ClosedCaptions. It should not happen.");
        return false;
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Unknown data type");
        return false;
    }

    const uint32_t result = m_textTrackWrapper->sendSessionData(sessionId, wrapperDataType, displayOffsetMs, data);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_DEBUG("Sending data to TextTrack session %u was successful; size %zu", sessionId, data.size());
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to send data to TextTrack session %u; error %s", sessionId,
                            m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::createTextTrackControlInterface()
{
    if (!m_textTrackPluginWrapper)
    {
        RIALTO_SERVER_LOG_ERROR("TextTrackPlugin is null!");
        return false;
    }

    uint32_t openResult = m_textTrackPluginWrapper->open();

    if (m_thunderWrapper->isSuccessful(openResult))
    {
        if (m_textTrackPluginWrapper->isOperational())
        {
            m_textTrackWrapper = m_textTrackPluginWrapper->interface();
            if (m_textTrackWrapper)
            {
                RIALTO_SERVER_LOG_INFO("Created TextTrack interface");
                return true;
            }
            else
            {
                RIALTO_SERVER_LOG_ERROR("Failed to create TextTrack interface");
            }
        }
        else
        {
            RIALTO_SERVER_LOG_ERROR("TextTrack plugin is NOT operational");
        }
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Failed to open TextTrack plugin; error '%s'",
                                m_thunderWrapper->errorToString(openResult));
    }

    return false;
}

bool TextTrackAccessor::setSessionWebVTTSelection(uint32_t sessionId)
{
    uint32_t result = m_textTrackWrapper->setSessionWebVTTSelection(sessionId);

    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_INFO("Setting WebVTT selection for session %u was successful", sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to set WebVTT selection for session %u; error %s", sessionId,
                            m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::setSessionTTMLSelection(uint32_t sessionId)
{
    uint32_t result = m_textTrackWrapper->setSessionTTMLSelection(sessionId);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_INFO("Setting TTML selection for session %u was successful", sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to set TTML selection for session %u; error %s", sessionId,
                            m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::setSessionCCSelection(uint32_t sessionId, const std::string &service)
{
    uint32_t result = m_textTrackWrapper->setSessionClosedCaptionsService(sessionId, service);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_INFO("Setting CC selection service '%s' for session %u was successful", service.c_str(),
                               sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to set CC selection service '%s' for session %u; error %s", service.c_str(),
                            sessionId, m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::resetSession(uint32_t sessionId)
{
    uint32_t result = m_textTrackWrapper->resetSession(sessionId);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_MIL("Resseting session %u was successful", sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to reset session %u; error %s", sessionId, m_thunderWrapper->errorToString(result));
    return false;
}

bool TextTrackAccessor::associateVideoDecoder(uint32_t sessionId, const std::string &videoDecoder)
{
    uint32_t result = m_textTrackWrapper->associateVideoDecoder(sessionId, videoDecoder);
    if (m_thunderWrapper->isSuccessful(result))
    {
        RIALTO_SERVER_LOG_MIL("Associating video decoder '%s' with session %u was successful", videoDecoder.c_str(),
                              sessionId);
        return true;
    }

    RIALTO_SERVER_LOG_ERROR("Failed to associate video decoder '%s' with session %u; error %s", videoDecoder.c_str(),
                            sessionId, m_thunderWrapper->errorToString(result));
    return false;
}

} // namespace firebolt::rialto::server
