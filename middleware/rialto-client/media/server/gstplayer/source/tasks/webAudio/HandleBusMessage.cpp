/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#include "tasks/webAudio/HandleBusMessage.h"
#include "IGstWebAudioPlayerClient.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include "WebAudioPlayerContext.h"

namespace firebolt::rialto::server::tasks::webaudio
{
HandleBusMessage::HandleBusMessage(WebAudioPlayerContext &context, IGstWebAudioPlayerPrivate &player,
                                   IGstWebAudioPlayerClient *client,
                                   std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                   std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                   GstMessage *message)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}, m_gstWrapper{gstWrapper},
      m_glibWrapper{glibWrapper}, m_message{message}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing HandleBusMessage");
}

HandleBusMessage::~HandleBusMessage()
{
    RIALTO_SERVER_LOG_DEBUG("HandleBusMessage finished");
}

void HandleBusMessage::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing HandleBusMessage");
    switch (GST_MESSAGE_TYPE(m_message))
    {
    case GST_MESSAGE_STATE_CHANGED:
    {
        if (m_context.pipeline && GST_MESSAGE_SRC(m_message) == GST_OBJECT(m_context.pipeline))
        {
            GstState oldState, newState, pending;
            m_gstWrapper->gstMessageParseStateChanged(m_message, &oldState, &newState, &pending);
            RIALTO_SERVER_LOG_MIL("State changed (old: %s, new: %s, pending: %s)",
                                  m_gstWrapper->gstElementStateGetName(oldState),
                                  m_gstWrapper->gstElementStateGetName(newState),
                                  m_gstWrapper->gstElementStateGetName(pending));

            std::string filename = std::string(m_gstWrapper->gstElementStateGetName(oldState)) + "-" +
                                   std::string(m_gstWrapper->gstElementStateGetName(newState));
            m_gstWrapper->gstDebugBinToDotFileWithTs(GST_BIN(m_context.pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
                                                     filename.c_str());
            if (!m_gstPlayerClient)
            {
                break;
            }
            switch (newState)
            {
            case GST_STATE_PAUSED:
            {
                if (pending != GST_STATE_PAUSED)
                {
                    // newState==GST_STATE_PAUSED, pending==GST_STATE_PAUSED state transition is received as a result of
                    // waiting for preroll after seek.
                    // Subsequent newState==GST_STATE_PAUSED, pending!=GST_STATE_PAUSED transition will
                    // indicate that the pipeline is prerolled and it reached GST_STATE_PAUSED state after seek.
                    m_gstPlayerClient->notifyState(WebAudioPlayerState::PAUSED);
                }
                break;
            }
            case GST_STATE_PLAYING:
            {
                m_gstPlayerClient->notifyState(WebAudioPlayerState::PLAYING);
                break;
            }
            case GST_STATE_READY:
            {
                m_gstPlayerClient->notifyState(WebAudioPlayerState::IDLE);
                break;
            }
            case GST_STATE_NULL:
            case GST_STATE_VOID_PENDING:
            default:
                break;
            }
        }
        break;
    }
    case GST_MESSAGE_EOS:
    {
        if (m_context.pipeline && GST_MESSAGE_SRC(m_message) == GST_OBJECT(m_context.pipeline))
        {
            RIALTO_SERVER_LOG_MIL("End of stream reached.");
            if (m_gstPlayerClient)
            {
                m_gstPlayerClient->notifyState(WebAudioPlayerState::END_OF_STREAM);
            }

            // Flush the pipeline so that it can be reused
            if ((!m_gstWrapper->gstElementSendEvent(m_context.pipeline, m_gstWrapper->gstEventNewFlushStart())) ||
                (!m_gstWrapper->gstElementSendEvent(m_context.pipeline, m_gstWrapper->gstEventNewFlushStop(TRUE))))
            {
                RIALTO_SERVER_LOG_ERROR("Failed to flush the pipeline");
            }
        }
        break;
    }
    case GST_MESSAGE_ERROR:
    {
        GError *err = nullptr;
        gchar *debug = nullptr;
        m_gstWrapper->gstMessageParseError(m_message, &err, &debug);

        RIALTO_SERVER_LOG_ERROR("Error from %s - %d: %s (%s)", GST_OBJECT_NAME(GST_MESSAGE_SRC(m_message)), err->code,
                                err->message, debug);
        m_gstPlayerClient->notifyState(WebAudioPlayerState::FAILURE);

        m_glibWrapper->gFree(debug);
        m_glibWrapper->gErrorFree(err);
        break;
    }
    default:
        break;
    }

    m_gstWrapper->gstMessageUnref(m_message);
}
} // namespace firebolt::rialto::server::tasks::webaudio
