/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#include "tasks/generic/HandleBusMessage.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"

namespace firebolt::rialto::server::tasks::generic
{
HandleBusMessage::HandleBusMessage(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                   IGstGenericPlayerClient *client,
                                   const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                                   const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
                                   GstMessage *message, const IFlushWatcher &flushWatcher)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}, m_gstWrapper{gstWrapper},
      m_glibWrapper{glibWrapper}, m_message{message}, m_flushWatcher{flushWatcher},
      m_isFlushOngoingDuringCreation{flushWatcher.isFlushOngoing()},
      m_isAsyncFlushOngoingDuringCreation{flushWatcher.isAsyncFlushOngoing()}
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

            if (!m_gstPlayerClient)
            {
                break;
            }
            switch (newState)
            {
            case GST_STATE_NULL:
            {
                m_gstPlayerClient->notifyPlaybackState(PlaybackState::STOPPED);
                break;
            }
            case GST_STATE_PAUSED:
            {
                if (pending != GST_STATE_PAUSED)
                {
                    // If async flush was requested before HandleBusMessage task creation (but it was not executed yet)
                    // or if async flush was created after HandleBusMessage task creation (but before its execution)
                    // we can't report playback state, because async flush causes state loss - reported state is probably invalid.
                    if (m_isAsyncFlushOngoingDuringCreation || m_flushWatcher.isAsyncFlushOngoing())
                    {
                        RIALTO_SERVER_LOG_WARN("Skip PAUSED notification - flush is ongoing");
                        break;
                    }
                    // newState==GST_STATE_PAUSED, pending==GST_STATE_PAUSED state transition is received as a result of
                    // waiting for preroll after seek.
                    // Subsequent newState==GST_STATE_PAUSED, pending!=GST_STATE_PAUSED transition will
                    // indicate that the pipeline is prerolled and it reached GST_STATE_PAUSED state after seek.
                    m_gstPlayerClient->notifyPlaybackState(PlaybackState::PAUSED);
                }
                if (m_player.hasSourceType(MediaSourceType::SUBTITLE))
                {
                    m_player.stopSubtitleClockResyncTimer();
                }
                break;
            }
            case GST_STATE_PLAYING:
            {
                // If async flush was requested before HandleBusMessage task creation (but it was not executed yet)
                // or if async flush was created after HandleBusMessage task creation (but before its execution)
                // we can't report playback state, because async flush causes state loss - reported state is probably invalid.
                if (m_isAsyncFlushOngoingDuringCreation || m_flushWatcher.isAsyncFlushOngoing())
                {
                    RIALTO_SERVER_LOG_WARN("Skip PLAYING notification - flush is ongoing");
                    break;
                }

                m_context.isPlaying = true;
                m_gstPlayerClient->notifyPlaybackState(PlaybackState::PLAYING);

                if (m_context.pendingPlaybackRate != kNoPendingPlaybackRate)
                {
                    m_player.setPendingPlaybackRate();
                }
                m_player.startPositionReportingAndCheckAudioUnderflowTimer();
                if (m_player.hasSourceType(MediaSourceType::SUBTITLE))
                {
                    m_player.startSubtitleClockResyncTimer();
                }
                break;
            }
            case GST_STATE_VOID_PENDING:
            case GST_STATE_READY:
            {
                break;
            }
            }
            std::string filename = std::string(m_gstWrapper->gstElementStateGetName(oldState)) + "-" +
                                   std::string(m_gstWrapper->gstElementStateGetName(newState));
            m_gstWrapper->gstDebugBinToDotFileWithTs(GST_BIN(m_context.pipeline), GST_DEBUG_GRAPH_SHOW_ALL,
                                                     filename.c_str());
        }
        break;
    }
    case GST_MESSAGE_EOS:
    {
        // If flush was requested before HandleBusMessage task creation (but it was not executed yet)
        // or if flush was created after HandleBusMessage task creation (but before its execution)
        // we can't report EOS, because flush clears EOS.
        if (m_isFlushOngoingDuringCreation || m_flushWatcher.isFlushOngoing())
        {
            RIALTO_SERVER_LOG_WARN("Skip EOS notification - flush is ongoing");
            break;
        }
        if (m_context.pipeline && GST_MESSAGE_SRC(m_message) == GST_OBJECT(m_context.pipeline))
        {
            RIALTO_SERVER_LOG_MIL("End of stream reached.");
            if (!m_context.eosNotified && m_gstPlayerClient)
            {
                m_gstPlayerClient->notifyPlaybackState(PlaybackState::END_OF_STREAM);
                m_context.eosNotified = true;
            }
        }
        break;
    }
    case GST_MESSAGE_QOS:
    {
        GstFormat format;
        gboolean isLive = FALSE;
        guint64 runningTime = 0;
        guint64 streamTime = 0;
        guint64 timestamp = 0;
        guint64 duration = 0;
        guint64 dropped = 0;
        guint64 processed = 0;

        m_gstWrapper->gstMessageParseQos(m_message, &isLive, &runningTime, &streamTime, &timestamp, &duration);
        m_gstWrapper->gstMessageParseQosStats(m_message, &format, &processed, &dropped);

        if (GST_FORMAT_BUFFERS == format || GST_FORMAT_DEFAULT == format)
        {
            RIALTO_SERVER_LOG_INFO("QOS message: runningTime  %" G_GUINT64_FORMAT ", streamTime %" G_GUINT64_FORMAT
                                   ", timestamp %" G_GUINT64_FORMAT ", duration %" G_GUINT64_FORMAT
                                   ", format %u, processed %" G_GUINT64_FORMAT ", dropped %" G_GUINT64_FORMAT,
                                   runningTime, streamTime, timestamp, duration, format, processed, dropped);

            if (m_gstPlayerClient)
            {
                firebolt::rialto::QosInfo qosInfo = {processed, dropped};
                const gchar *klass;
                klass = m_gstWrapper->gstElementClassGetMetadata(GST_ELEMENT_GET_CLASS(GST_MESSAGE_SRC(m_message)),
                                                                 GST_ELEMENT_METADATA_KLASS);

                if (g_strrstr(klass, "Video"))
                {
                    m_gstPlayerClient->notifyQos(firebolt::rialto::MediaSourceType::VIDEO, qosInfo);
                }
                else if (g_strrstr(klass, "Audio"))
                {
                    m_gstPlayerClient->notifyQos(firebolt::rialto::MediaSourceType::AUDIO, qosInfo);
                }
                else
                {
                    RIALTO_SERVER_LOG_WARN("Unknown source type for class '%s', ignoring QOS Message", klass);
                }
            }
        }
        else
        {
            RIALTO_SERVER_LOG_WARN("Received a QOS_MESSAGE with unhandled format %s",
                                   m_gstWrapper->gstFormatGetName(format));
        }
        break;
    }
    case GST_MESSAGE_ERROR:
    {
        GError *err = nullptr;
        gchar *debug = nullptr;
        m_gstWrapper->gstMessageParseError(m_message, &err, &debug);

        if ((err->domain == GST_STREAM_ERROR) && (allSourcesEos()))
        {
            RIALTO_SERVER_LOG_WARN("Got stream error from %s. But all streams are ended, so reporting EOS. Error code "
                                   "%d: %s "
                                   "(%s).",
                                   GST_OBJECT_NAME(GST_MESSAGE_SRC(m_message)), err->code, err->message, debug);
            if (!m_context.eosNotified && m_gstPlayerClient)
            {
                m_gstPlayerClient->notifyPlaybackState(PlaybackState::END_OF_STREAM);
                m_context.eosNotified = true;
            }
        }
        else
        {
            RIALTO_SERVER_LOG_ERROR("Error from %s - %d: %s (%s)", GST_OBJECT_NAME(GST_MESSAGE_SRC(m_message)),
                                    err->code, err->message, debug);
            m_gstPlayerClient->notifyPlaybackState(PlaybackState::FAILURE);
        }

        m_glibWrapper->gFree(debug);
        m_glibWrapper->gErrorFree(err);
        break;
    }
    case GST_MESSAGE_WARNING:
    {
        PlaybackError rialtoError = PlaybackError::UNKNOWN;
        GError *err = nullptr;
        gchar *debug = nullptr;
        m_gstWrapper->gstMessageParseWarning(m_message, &err, &debug);

        if ((err->domain == GST_STREAM_ERROR) && (err->code == GST_STREAM_ERROR_DECRYPT))
        {
            RIALTO_SERVER_LOG_WARN("Decrypt error %s - %d: %s (%s)", GST_OBJECT_NAME(GST_MESSAGE_SRC(m_message)),
                                   err->code, err->message, debug);
            rialtoError = PlaybackError::DECRYPTION;
        }
        else
        {
            RIALTO_SERVER_LOG_WARN("Unknown warning, ignoring %s - %d: %s (%s)",
                                   GST_OBJECT_NAME(GST_MESSAGE_SRC(m_message)), err->code, err->message, debug);
        }

        if ((PlaybackError::UNKNOWN != rialtoError) && (m_gstPlayerClient))
        {
            const gchar *kName = GST_ELEMENT_NAME(GST_ELEMENT(GST_MESSAGE_SRC(m_message)));
            if (g_strrstr(kName, "video"))
            {
                m_gstPlayerClient->notifyPlaybackError(firebolt::rialto::MediaSourceType::VIDEO,
                                                       PlaybackError::DECRYPTION);
            }
            else if (g_strrstr(kName, "audio"))
            {
                m_gstPlayerClient->notifyPlaybackError(firebolt::rialto::MediaSourceType::AUDIO,
                                                       PlaybackError::DECRYPTION);
            }
            else
            {
                RIALTO_SERVER_LOG_WARN("Unknown source type for element '%s', not propagating error", kName);
            }
        }

        m_glibWrapper->gFree(debug);
        m_glibWrapper->gErrorFree(err);
        break;
    }
    default:
        break;
    }

    m_gstWrapper->gstMessageUnref(m_message);
}

bool HandleBusMessage::allSourcesEos() const
{
    for (const auto &streamInfo : m_context.streamInfo)
    {
        const auto eosInfoIt = m_context.endOfStreamInfo.find(streamInfo.first);
        if (eosInfoIt == m_context.endOfStreamInfo.end() || eosInfoIt->second != EosState::SET)
        {
            return false;
        }
    }
    return true;
}
} // namespace firebolt::rialto::server::tasks::generic
