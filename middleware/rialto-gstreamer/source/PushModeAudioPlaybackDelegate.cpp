/*
 * Copyright (C) 2025   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "PushModeAudioPlaybackDelegate.h"
#include "ControlBackend.h"
#include "GStreamerWebAudioPlayerClient.h"
#include "GstreamerCatLog.h"
#include "MessageQueue.h"
#include "WebAudioClientBackend.h"

#define GST_CAT_DEFAULT rialtoGStreamerCat

PushModeAudioPlaybackDelegate::PushModeAudioPlaybackDelegate(GstElement *sink)
    : m_sink{sink}, m_rialtoControlClient{std::make_unique<firebolt::rialto::client::ControlBackend>()},
      m_webAudioClient{
          std::make_shared<GStreamerWebAudioPlayerClient>(std::make_unique<firebolt::rialto::client::WebAudioClientBackend>(),
                                                          std::make_unique<rialto::MessageQueue>(), *this,
                                                          ITimerFactory::getFactory())}
{
}

PushModeAudioPlaybackDelegate::~PushModeAudioPlaybackDelegate()
{
    m_webAudioClient.reset();
}

void PushModeAudioPlaybackDelegate::handleEos()
{
    GstState currentState = GST_STATE(m_sink);
    if ((currentState != GST_STATE_PAUSED) && (currentState != GST_STATE_PLAYING))
    {
        GST_ERROR_OBJECT(m_sink, "Sink cannot post a EOS message in state '%s', posting an error instead",
                         gst_element_state_get_name(currentState));

        const char *errMessage = "Web audio sink received EOS in non-playing state";
        GError *gError{g_error_new_literal(GST_STREAM_ERROR, 0, errMessage)};
        gst_element_post_message(GST_ELEMENT_CAST(m_sink),
                                 gst_message_new_error(GST_OBJECT_CAST(m_sink), gError, errMessage));
        g_error_free(gError);
    }
    else
    {
        gst_element_post_message(GST_ELEMENT_CAST(m_sink), gst_message_new_eos(GST_OBJECT_CAST(m_sink)));
    }
}

void PushModeAudioPlaybackDelegate::handleStateChanged(firebolt::rialto::PlaybackState state)
{
    GstState current = GST_STATE(m_sink);
    GstState next = GST_STATE_NEXT(m_sink);
    GstState pending = GST_STATE_PENDING(m_sink);

    GST_DEBUG_OBJECT(m_sink,
                     "Received server's state change to %u. Sink's states are: current state: %s next state: %s "
                     "pending state: %s, last return state %s",
                     static_cast<uint32_t>(state), gst_element_state_get_name(current),
                     gst_element_state_get_name(next), gst_element_state_get_name(pending),
                     gst_element_state_change_return_get_name(GST_STATE_RETURN(m_sink)));

    if (m_isStateCommitNeeded && ((state == firebolt::rialto::PlaybackState::PAUSED && next == GST_STATE_PAUSED) ||
                                  (state == firebolt::rialto::PlaybackState::PLAYING && next == GST_STATE_PLAYING)))
    {
        GstState postNext = next == pending ? GST_STATE_VOID_PENDING : pending;
        GST_STATE(m_sink) = next;
        GST_STATE_NEXT(m_sink) = postNext;
        GST_STATE_PENDING(m_sink) = GST_STATE_VOID_PENDING;
        GST_STATE_RETURN(m_sink) = GST_STATE_CHANGE_SUCCESS;

        GST_INFO_OBJECT(m_sink, "Async state transition to state %s done", gst_element_state_get_name(next));

        gst_element_post_message(GST_ELEMENT_CAST(m_sink),
                                 gst_message_new_state_changed(GST_OBJECT_CAST(m_sink), current, next, pending));
        postAsyncDone();
    }
}

void PushModeAudioPlaybackDelegate::handleError(const std::string &message, gint code)
{
    GError *gError{g_error_new_literal(GST_STREAM_ERROR, code, message.c_str())};
    gst_element_post_message(GST_ELEMENT_CAST(m_sink),
                             gst_message_new_error(GST_OBJECT_CAST(m_sink), gError, message.c_str()));
    g_error_free(gError);
}

void PushModeAudioPlaybackDelegate::handleQos(uint64_t processed, uint64_t dropped) const {}

GstStateChangeReturn PushModeAudioPlaybackDelegate::changeState(GstStateChange transition)
{
    GstPad *sinkPad = gst_element_get_static_pad(GST_ELEMENT_CAST(m_sink), "sink");

    GstState current_state = GST_STATE_TRANSITION_CURRENT(transition);
    GstState next_state = GST_STATE_TRANSITION_NEXT(transition);
    GST_INFO_OBJECT(m_sink, "State change: (%s) -> (%s)", gst_element_state_get_name(current_state),
                    gst_element_state_get_name(next_state));

    GstStateChangeReturn result = GST_STATE_CHANGE_SUCCESS;
    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
    {
        GST_DEBUG_OBJECT(m_sink, "GST_STATE_CHANGE_NULL_TO_READY");

        if (!m_rialtoControlClient->waitForRunning())
        {
            GST_ERROR_OBJECT(m_sink, "Rialto client cannot reach running state");
            result = GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_PAUSED");
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        GST_DEBUG_OBJECT(m_sink, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
        if (!m_webAudioClient->isOpen())
        {
            GST_INFO_OBJECT(m_sink, "Delay playing until the caps are recieved and the player is opened");
            m_isPlayingDelayed = true;
            result = GST_STATE_CHANGE_ASYNC;
            postAsyncStart();
        }
        else
        {
            if (!m_webAudioClient->play())
            {
                GST_ERROR_OBJECT(m_sink, "Failed to play web audio");
                result = GST_STATE_CHANGE_FAILURE;
            }
            else
            {
                result = GST_STATE_CHANGE_ASYNC;
                postAsyncStart();
            }
        }
        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        GST_DEBUG_OBJECT(m_sink, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
        if (!m_webAudioClient->pause())
        {
            GST_ERROR_OBJECT(m_sink, "Failed to pause web audio");
            result = GST_STATE_CHANGE_FAILURE;
        }
        else
        {
            result = GST_STATE_CHANGE_ASYNC;
            postAsyncStart();
        }
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        GST_DEBUG_OBJECT(m_sink, "GST_STATE_CHANGE_PAUSED_TO_READY");
        if (!m_webAudioClient->close())
        {
            GST_ERROR_OBJECT(m_sink, "Failed to close web audio");
            result = GST_STATE_CHANGE_FAILURE;
        }
        break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
        GST_DEBUG("GST_STATE_CHANGE_READY_TO_NULL");

        m_rialtoControlClient->removeControlBackend();
    }
    default:
        break;
    }

    gst_object_unref(sinkPad);

    return result;
}

void PushModeAudioPlaybackDelegate::postAsyncStart()
{
    m_isStateCommitNeeded = true;
    gst_element_post_message(GST_ELEMENT_CAST(m_sink), gst_message_new_async_start(GST_OBJECT(m_sink)));
}

void PushModeAudioPlaybackDelegate::setProperty(const Property &type, const GValue *value)
{
    switch (type)
    {
    case Property::TsOffset:
    {
        GST_INFO_OBJECT(m_sink, "ts-offset property not supported, RialtoWebAudioSink does not require the "
                                "synchronisation of sources");
        break;
    }
    case Property::Volume:
    {
        m_volume = g_value_get_double(value);
        if (!m_webAudioClient || !m_webAudioClient->isOpen())
        {
            GST_DEBUG_OBJECT(m_sink, "Enqueue volume setting");
            m_isVolumeQueued = true;
            return;
        }
        if (!m_webAudioClient->setVolume(m_volume))
        {
            GST_ERROR_OBJECT(m_sink, "Failed to set volume");
        }
        break;
    }
    default:
    {
        break;
    }
    }
}

void PushModeAudioPlaybackDelegate::getProperty(const Property &type, GValue *value)
{
    switch (type)
    {
    case Property::TsOffset:
    {
        GST_INFO_OBJECT(m_sink, "ts-offset property not supported, RialtoWebAudioSink does not require the "
                                "synchronisation of sources");
        break;
    }
    case Property::Volume:
    {
        double volume{0.0};
        if (m_webAudioClient && m_webAudioClient->isOpen())
        {
            if (m_webAudioClient->getVolume(volume))
                m_volume = volume;
            else
                volume = m_volume; // Use last known volume
        }
        else
        {
            volume = m_volume;
        }
        g_value_set_double(value, volume);
        break;
    }
    default:
    {
        break;
    }
    }
}

std::optional<gboolean> PushModeAudioPlaybackDelegate::handleQuery(GstQuery *query) const
{
    return std::nullopt;
}

gboolean PushModeAudioPlaybackDelegate::handleSendEvent(GstEvent *event)
{
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        GST_INFO_OBJECT(m_sink, "Attaching AUDIO source with caps %" GST_PTR_FORMAT, caps);
    }
    default:
        break;
    }
    gst_event_unref(event);
    return TRUE;
}

gboolean PushModeAudioPlaybackDelegate::handleEvent(GstPad *pad, GstObject *parent, GstEvent *event)
{
    gboolean result = FALSE;
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_EOS:
    {
        GST_DEBUG_OBJECT(m_sink, "GST_EVENT_EOS");
        result = m_webAudioClient->setEos();
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        GST_INFO_OBJECT(m_sink, "Opening WebAudio with caps %" GST_PTR_FORMAT, caps);

        if (!m_webAudioClient->open(caps))
        {
            GST_ERROR_OBJECT(m_sink, "Failed to open web audio");
        }
        else
        {
            result = TRUE;
            if (m_isVolumeQueued)
            {
                if (!m_webAudioClient->setVolume(m_volume))
                {
                    GST_ERROR_OBJECT(m_sink, "Failed to set volume");
                    result = FALSE;
                }
                else
                {
                    m_isVolumeQueued = false;
                }
            }
            if (m_isPlayingDelayed)
            {
                if (!m_webAudioClient->play())
                {
                    GST_ERROR_OBJECT(m_sink, "Failed to play web audio");
                    result = FALSE;
                }
                else
                {
                    m_isPlayingDelayed = false;
                }
            }
        }
        break;
    }
    default:
        return gst_pad_event_default(pad, parent, event);
    }
    gst_event_unref(event);
    return result;
}

GstFlowReturn PushModeAudioPlaybackDelegate::handleBuffer(GstBuffer *buffer)
{
    if (m_webAudioClient->notifyNewSample(buffer))
    {
        return GST_FLOW_OK;
    }
    else
    {
        GST_ERROR_OBJECT(m_sink, "Failed to push sample");
        return GST_FLOW_ERROR;
    }
}

void PushModeAudioPlaybackDelegate::postAsyncDone()
{
    m_isStateCommitNeeded = false;
    gst_element_post_message(GST_ELEMENT_CAST(m_sink),
                             gst_message_new_async_done(GST_OBJECT_CAST(m_sink), GST_CLOCK_TIME_NONE));
}