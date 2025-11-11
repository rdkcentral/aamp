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

#include "PullModePlaybackDelegate.h"
#include "ControlBackend.h"
#include "GstreamerCatLog.h"
#include "RialtoGStreamerMSEBaseSink.h"
#include "RialtoGStreamerMSEBaseSinkPrivate.h"

#define GST_CAT_DEFAULT rialtoGStreamerCat

namespace
{
GstObject *getOldestGstBinParent(GstElement *element)
{
    GstObject *parent = gst_object_get_parent(GST_OBJECT_CAST(element));
    GstObject *result = GST_OBJECT_CAST(element);
    if (parent)
    {
        if (GST_IS_BIN(parent))
        {
            result = getOldestGstBinParent(GST_ELEMENT_CAST(parent));
        }
        gst_object_unref(parent);
    }

    return result;
}

unsigned getGstPlayFlag(const char *nick)
{
    GFlagsClass *flagsClass = static_cast<GFlagsClass *>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
    GFlagsValue *flag = g_flags_get_value_by_nick(flagsClass, nick);
    return flag ? flag->value : 0;
}

bool getNStreamsFromParent(GstObject *parentObject, gint &n_video, gint &n_audio, gint &n_text)
{
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "n-video") &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "n-audio") &&
        g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "n-text"))
    {
        g_object_get(parentObject, "n-video", &n_video, "n-audio", &n_audio, "n-text", &n_text, nullptr);

        if (g_object_class_find_property(G_OBJECT_GET_CLASS(parentObject), "flags"))
        {
            guint flags = 0;
            g_object_get(parentObject, "flags", &flags, nullptr);
            n_video = (flags & getGstPlayFlag("video")) ? n_video : 0;
            n_audio = (flags & getGstPlayFlag("audio")) ? n_audio : 0;
            n_text = (flags & getGstPlayFlag("text")) ? n_text : 0;
        }

        return true;
    }

    return false;
}
} // namespace

PullModePlaybackDelegate::PullModePlaybackDelegate(GstElement *sink) : m_sink{sink}
{
    RialtoMSEBaseSink *baseSink = RIALTO_MSE_BASE_SINK(sink);
    m_sinkPad = baseSink->priv->m_sinkPad;
    m_rialtoControlClient = std::make_unique<firebolt::rialto::client::ControlBackend>();
    gst_segment_init(&m_lastSegment, GST_FORMAT_TIME);
}

PullModePlaybackDelegate::~PullModePlaybackDelegate()
{
    if (m_caps)
        gst_caps_unref(m_caps);
    clearBuffersUnlocked();
}

void PullModePlaybackDelegate::clearBuffersUnlocked()
{
    m_isFlushOngoing = true;
    m_needDataCondVariable.notify_all();
    while (!m_samples.empty())
    {
        GstSample *sample = m_samples.front();
        m_samples.pop();
        gst_sample_unref(sample);
    }
}

void PullModePlaybackDelegate::setSourceId(int32_t sourceId)
{
    std::unique_lock lock{m_sinkMutex};
    m_sourceId = sourceId;
}

void PullModePlaybackDelegate::handleEos()
{
    GstState currentState = GST_STATE(m_sink);
    if ((currentState != GST_STATE_PAUSED) && (currentState != GST_STATE_PLAYING))
    {
        GST_ERROR_OBJECT(m_sink, "Sink cannot post a EOS message in state '%s', posting an error instead",
                         gst_element_state_get_name(currentState));

        const char *errMessage = "Rialto sinks received EOS in non-playing state";
        GError *gError{g_error_new_literal(GST_STREAM_ERROR, 0, errMessage)};
        gst_element_post_message(m_sink, gst_message_new_error(GST_OBJECT_CAST(m_sink), gError, errMessage));
        g_error_free(gError);
    }
    else if (!m_isFlushOngoing)
    {
        gst_element_post_message(m_sink, gst_message_new_eos(GST_OBJECT_CAST(m_sink)));
    }
    else
    {
        GST_WARNING_OBJECT(m_sink, "Skip sending eos message - flush is ongoing...");
    }
}

void PullModePlaybackDelegate::handleFlushCompleted()
{
    GST_INFO_OBJECT(m_sink, "Flush completed");
    std::unique_lock<std::mutex> lock(m_flushMutex);
    m_flushCondVariable.notify_all();
}

void PullModePlaybackDelegate::handleStateChanged(firebolt::rialto::PlaybackState state)
{
    GstState current = GST_STATE(m_sink);
    GstState next = GST_STATE_NEXT(m_sink);
    GstState pending = GST_STATE_PENDING(m_sink);
    GstState postNext = next == pending ? GST_STATE_VOID_PENDING : pending;

    GST_DEBUG_OBJECT(m_sink,
                     "Received server's state change to %u. Sink's states are: current state: %s next state: %s "
                     "pending state: %s, last return state %s",
                     static_cast<uint32_t>(state), gst_element_state_get_name(current),
                     gst_element_state_get_name(next), gst_element_state_get_name(pending),
                     gst_element_state_change_return_get_name(GST_STATE_RETURN(m_sink)));

    if (m_isStateCommitNeeded)
    {
        if ((state == firebolt::rialto::PlaybackState::PAUSED && next == GST_STATE_PAUSED) ||
            (state == firebolt::rialto::PlaybackState::PLAYING && next == GST_STATE_PLAYING))
        {
            GST_STATE(m_sink) = next;
            GST_STATE_NEXT(m_sink) = postNext;
            GST_STATE_PENDING(m_sink) = GST_STATE_VOID_PENDING;
            GST_STATE_RETURN(m_sink) = GST_STATE_CHANGE_SUCCESS;

            GST_INFO_OBJECT(m_sink, "Async state transition to state %s done", gst_element_state_get_name(next));

            gst_element_post_message(m_sink,
                                     gst_message_new_state_changed(GST_OBJECT_CAST(m_sink), current, next, pending));
            postAsyncDone();
        }
        /* Immediately transition to PLAYING when prerolled and PLAY is requested */
        else if (state == firebolt::rialto::PlaybackState::PAUSED && current == GST_STATE_PAUSED &&
                 next == GST_STATE_PLAYING)
        {
            GST_INFO_OBJECT(m_sink, "Async state transition to PAUSED done. Transitioning to PLAYING");
            changeState(GST_STATE_CHANGE_PAUSED_TO_PLAYING);
        }
    }
}

GstStateChangeReturn PullModePlaybackDelegate::changeState(GstStateChange transition)
{
    GstState current_state = GST_STATE_TRANSITION_CURRENT(transition);
    GstState next_state = GST_STATE_TRANSITION_NEXT(transition);
    GST_INFO_OBJECT(m_sink, "State change: (%s) -> (%s)", gst_element_state_get_name(current_state),
                    gst_element_state_get_name(next_state));

    GstStateChangeReturn status = GST_STATE_CHANGE_SUCCESS;
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();

    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (!m_sinkPad)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot start, because there's no sink pad");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (!m_rialtoControlClient->waitForRunning())
        {
            GST_ERROR_OBJECT(m_sink, "Control: Rialto client cannot reach running state");
            return GST_STATE_CHANGE_FAILURE;
        }
        GST_INFO_OBJECT(m_sink, "Control: Rialto client reached running state");
        break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        m_isFlushOngoing = false;

        StateChangeResult result = client->pause(m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC || result == StateChangeResult::NOT_ATTACHED)
        {
            // NOT_ATTACHED is not a problem here, because source will be attached later when GST_EVENT_CAPS is received
            if (result == StateChangeResult::NOT_ATTACHED)
            {
                postAsyncStart();
            }
            status = GST_STATE_CHANGE_ASYNC;
        }

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        StateChangeResult result = client->play(m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC)
        {
            status = GST_STATE_CHANGE_ASYNC;
        }
        else if (result == StateChangeResult::NOT_ATTACHED)
        {
            GST_ERROR_OBJECT(m_sink, "Failed to change state to playing");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        StateChangeResult result = client->pause(m_sourceId);
        if (result == StateChangeResult::SUCCESS_ASYNC)
        {
            status = GST_STATE_CHANGE_ASYNC;
        }
        else if (result == StateChangeResult::NOT_ATTACHED)
        {
            GST_ERROR_OBJECT(m_sink, "Failed to change state to paused");
            return GST_STATE_CHANGE_FAILURE;
        }

        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Cannot get the media player client object");
            return GST_STATE_CHANGE_FAILURE;
        }

        if (m_isStateCommitNeeded)
        {
            GST_DEBUG_OBJECT(m_sink, "Sending async_done in PAUSED->READY transition");
            postAsyncDone();
        }

        client->removeSource(m_sourceId);
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            clearBuffersUnlocked();
            m_sourceAttached = false;
        }
        break;
    case GST_STATE_CHANGE_READY_TO_NULL:
        // Playback will be stopped once all sources are finished and ref count
        // of the media pipeline object reaches 0
        m_mediaPlayerManager.releaseMediaPlayerClient();
        m_rialtoControlClient->removeControlBackend();
        break;
    default:
        break;
    }

    return status;
}

void PullModePlaybackDelegate::handleError(const std::string &message, gint code)
{
    GError *gError{g_error_new_literal(GST_STREAM_ERROR, code, message.c_str())};
    gst_element_post_message(GST_ELEMENT_CAST(m_sink),
                             gst_message_new_error(GST_OBJECT_CAST(m_sink), gError, message.c_str()));
    g_error_free(gError);
}

void PullModePlaybackDelegate::postAsyncStart()
{
    m_isStateCommitNeeded = true;
    gst_element_post_message(GST_ELEMENT_CAST(m_sink), gst_message_new_async_start(GST_OBJECT(m_sink)));
}

void PullModePlaybackDelegate::postAsyncDone()
{
    m_isStateCommitNeeded = false;
    gst_element_post_message(m_sink, gst_message_new_async_done(GST_OBJECT_CAST(m_sink), GST_CLOCK_TIME_NONE));
}

void PullModePlaybackDelegate::setProperty(const Property &type, const GValue *value)
{
    switch (type)
    {
    case Property::IsSinglePathStream:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_isSinglePathStream = g_value_get_boolean(value) != FALSE;
        break;
    }
    case Property::NumberOfStreams:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_numOfStreams = g_value_get_int(value);
        break;
    }
    case Property::HasDrm:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_hasDrm = g_value_get_boolean(value) != FALSE;
        break;
    }
    default:
    {
        break;
    }
    }
}

void PullModePlaybackDelegate::getProperty(const Property &type, GValue *value)
{
    switch (type)
    {
    case Property::IsSinglePathStream:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_boolean(value, m_isSinglePathStream ? TRUE : FALSE);
        break;
    }
    case Property::NumberOfStreams:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_int(value, m_numOfStreams);
        break;
    }
    case Property::HasDrm:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        g_value_set_boolean(value, m_hasDrm ? TRUE : FALSE);
        break;
    }
    case Property::Stats:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            GST_ERROR_OBJECT(m_sink, "Could not get the media player client");
            break;
        }

        guint64 totalVideoFrames{0};
        guint64 droppedVideoFrames{0};
        if (client->getStats(m_sourceId, totalVideoFrames, droppedVideoFrames))
        {
            GstStructure *stats{gst_structure_new("stats", "rendered", G_TYPE_UINT64, totalVideoFrames, "dropped",
                                                  G_TYPE_UINT64, droppedVideoFrames, nullptr)};
            g_value_set_pointer(value, stats);
        }
        else
        {
            GST_ERROR_OBJECT(m_sink, "No stats returned from client");
        }
    }
    default:
    {
        break;
    }
    }
}

std::optional<gboolean> PullModePlaybackDelegate::handleQuery(GstQuery *query) const
{
    GST_DEBUG_OBJECT(m_sink, "handling query '%s'", GST_QUERY_TYPE_NAME(query));
    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_SEEKING:
    {
        GstFormat fmt;
        gst_query_parse_seeking(query, &fmt, NULL, NULL, NULL);
        gst_query_set_seeking(query, fmt, FALSE, 0, -1);
        return TRUE;
    }
    case GST_QUERY_POSITION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            return FALSE;
        }

        GstFormat fmt;
        gst_query_parse_position(query, &fmt, NULL);
        switch (fmt)
        {
        case GST_FORMAT_TIME:
        {
            gint64 position = client->getPosition(m_sourceId);
            GST_DEBUG_OBJECT(m_sink, "Queried position is %" GST_TIME_FORMAT, GST_TIME_ARGS(position));
            if (position < 0)
            {
                return FALSE;
            }

            gst_query_set_position(query, fmt, position);
            break;
        }
        default:
            break;
        }
        return TRUE;
    }
    case GST_QUERY_SEGMENT:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        GstFormat format{m_lastSegment.format};
        gint64 start{static_cast<gint64>(gst_segment_to_stream_time(&m_lastSegment, format, m_lastSegment.start))};
        gint64 stop{0};
        if (m_lastSegment.stop == GST_CLOCK_TIME_NONE)
        {
            stop = m_lastSegment.duration;
        }
        else
        {
            stop = gst_segment_to_stream_time(&m_lastSegment, format, m_lastSegment.stop);
        }
        gst_query_set_segment(query, m_lastSegment.rate, format, start, stop);
        return TRUE;
    }
    default:
        break;
    }
    return std::nullopt;
}

gboolean PullModePlaybackDelegate::handleSendEvent(GstEvent *event)
{
    GST_DEBUG_OBJECT(m_sink, "handling event '%s'", GST_EVENT_TYPE_NAME(event));
    bool shouldForwardUpstream = GST_EVENT_IS_UPSTREAM(event);

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEEK:
    {
        gdouble rate{1.0};
        GstFormat seekFormat{GST_FORMAT_UNDEFINED};
        GstSeekFlags flags{GST_SEEK_FLAG_NONE};
        GstSeekType startType{GST_SEEK_TYPE_NONE}, stopType{GST_SEEK_TYPE_NONE};
        gint64 start{0}, stop{0};
        if (event)
        {
            gst_event_parse_seek(event, &rate, &seekFormat, &flags, &startType, &start, &stopType, &stop);

            if (flags & GST_SEEK_FLAG_FLUSH)
            {
                if (seekFormat == GST_FORMAT_TIME && startType == GST_SEEK_TYPE_END)
                {
                    GST_ERROR_OBJECT(m_sink, "GST_SEEK_TYPE_END seek is not supported");
                    gst_event_unref(event);
                    return FALSE;
                }
                // Update last segment
                if (seekFormat == GST_FORMAT_TIME)
                {
                    gboolean update{FALSE};
                    std::lock_guard<std::mutex> lock(m_sinkMutex);
                    gst_segment_do_seek(&m_lastSegment, rate, seekFormat, flags, startType, start, stopType, stop,
                                        &update);
                }
            }
#if GST_CHECK_VERSION(1, 18, 0)
            else if (flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE)
            {
                gdouble rateMultiplier = rate / m_lastSegment.rate;
                GstEvent *rateChangeEvent = gst_event_new_instant_rate_change(rateMultiplier, (GstSegmentFlags)flags);
                gst_event_set_seqnum(rateChangeEvent, gst_event_get_seqnum(event));
                gst_event_unref(event);
                if (gst_pad_send_event(m_sinkPad, rateChangeEvent) != TRUE)
                {
                    GST_ERROR_OBJECT(m_sink, "Sending instant rate change failed.");
                    return FALSE;
                }
                return TRUE;
            }
#endif
            else
            {
                GST_WARNING_OBJECT(m_sink, "Seek with flags 0x%X is not supported", flags);
                gst_event_unref(event);
                return FALSE;
            }
        }
        break;
    }
#if GST_CHECK_VERSION(1, 18, 0)
    case GST_EVENT_INSTANT_RATE_SYNC_TIME:
    {
        double rate{0.0};
        GstClockTime runningTime{GST_CLOCK_TIME_NONE}, upstreamRunningTime{GST_CLOCK_TIME_NONE};
        guint32 seqnum = gst_event_get_seqnum(event);
        gst_event_parse_instant_rate_sync_time(event, &rate, &runningTime, &upstreamRunningTime);

        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if ((client) && (m_mediaPlayerManager.hasControl()))
        {
            GST_DEBUG_OBJECT(m_sink, "Instant playback rate change: %.2f", rate);
            m_currentInstantRateChangeSeqnum = seqnum;
            client->setPlaybackRate(rate);
        }
        break;
    }
#endif
    default:
        break;
    }

    if (shouldForwardUpstream)
    {
        bool result = gst_pad_push_event(m_sinkPad, event);
        if (!result)
        {
            GST_DEBUG_OBJECT(m_sink, "forwarding upstream event '%s' failed", GST_EVENT_TYPE_NAME(event));
        }

        return result;
    }

    gst_event_unref(event);
    return TRUE;
}

gboolean PullModePlaybackDelegate::handleEvent(GstPad *pad, GstObject *parent, GstEvent *event)
{
    GST_DEBUG_OBJECT(m_sink, "handling event %" GST_PTR_FORMAT, event);
    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_SEGMENT:
    {
        copySegment(event);
        setSegment();
        break;
    }
    case GST_EVENT_EOS:
    {
        std::lock_guard<std::mutex> lock(m_sinkMutex);
        m_isEos = true;
        break;
    }
    case GST_EVENT_CAPS:
    {
        GstCaps *caps;
        gst_event_parse_caps(event, &caps);
        {
            std::lock_guard<std::mutex> lock(m_sinkMutex);
            if (m_caps)
            {
                if (!gst_caps_is_equal(caps, m_caps))
                {
                    gst_caps_unref(m_caps);
                    m_caps = gst_caps_copy(caps);
                }
            }
            else
            {
                m_caps = gst_caps_copy(caps);
            }
        }
        break;
    }
    case GST_EVENT_SINK_MESSAGE:
    {
        GstMessage *message = nullptr;
        gst_event_parse_sink_message(event, &message);

        if (message)
        {
            gst_element_post_message(m_sink, message);
        }

        break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
    {
        if (gst_event_has_name(event, "custom-instant-rate-change"))
        {
            GST_DEBUG_OBJECT(m_sink, "Change rate event received");
            changePlaybackRate(event);
        }
        break;
    }
    case GST_EVENT_FLUSH_START:
    {
        startFlushing();
        break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
        gboolean resetTime{FALSE};
        gst_event_parse_flush_stop(event, &resetTime);

        stopFlushing(resetTime);
        break;
    }
    case GST_EVENT_STREAM_COLLECTION:
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (!client)
        {
            gst_event_unref(event);
            return FALSE;
        }
        int32_t videoStreams{0}, audioStreams{0}, textStreams{0};
        GstStreamCollection *streamCollection{nullptr};
        gst_event_parse_stream_collection(event, &streamCollection);
        guint streamsSize = gst_stream_collection_get_size(streamCollection);
        for (guint i = 0; i < streamsSize; ++i)
        {
            auto *stream = gst_stream_collection_get_stream(streamCollection, i);
            auto type = gst_stream_get_stream_type(stream);
            if (type & GST_STREAM_TYPE_AUDIO)
            {
                ++audioStreams;
            }
            else if (type & GST_STREAM_TYPE_VIDEO)
            {
                ++videoStreams;
            }
            else if (type & GST_STREAM_TYPE_TEXT)
            {
                ++textStreams;
            }
        }
        gst_object_unref(streamCollection);
        client->handleStreamCollection(audioStreams, videoStreams, textStreams);
        client->sendAllSourcesAttachedIfPossible();
        break;
    }
#if GST_CHECK_VERSION(1, 18, 0)
    case GST_EVENT_INSTANT_RATE_CHANGE:
    {
        guint32 seqnum = gst_event_get_seqnum(event);
        if (m_lastInstantRateChangeSeqnum == seqnum || m_currentInstantRateChangeSeqnum.load() == seqnum)
        {
            /* Ignore if we already received the instant-rate-sync-time event from the pipeline */
            GST_DEBUG_OBJECT(m_sink, "Instant rate change event with seqnum %u already handled. Ignoring...", seqnum);
            break;
        }

        m_lastInstantRateChangeSeqnum = seqnum;
        gdouble rate{0.0};
        GstSegmentFlags flags{GST_SEGMENT_FLAG_NONE};
        gst_event_parse_instant_rate_change(event, &rate, &flags);
        GstMessage *msg = gst_message_new_instant_rate_request(GST_OBJECT_CAST(m_sink), rate);
        gst_message_set_seqnum(msg, seqnum);
        gst_element_post_message(m_sink, msg);
        break;
    }
#endif
    default:
        break;
    }

    gst_event_unref(event);

    return TRUE;
}

void PullModePlaybackDelegate::copySegment(GstEvent *event)
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    gst_event_copy_segment(event, &m_lastSegment);
}

void PullModePlaybackDelegate::setSegment()
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(m_sink, "Could not get the media player client");
        return;
    }
    const bool kResetTime{m_lastSegment.flags == GST_SEGMENT_FLAG_RESET};
    int64_t position = static_cast<int64_t>(m_lastSegment.start);
    client->setSourcePosition(m_sourceId, position, kResetTime, m_lastSegment.applied_rate, m_lastSegment.stop);
    m_segmentSet = true;
}

void PullModePlaybackDelegate::changePlaybackRate(GstEvent *event)
{
    const GstStructure *structure{gst_event_get_structure(event)};
    gdouble playbackRate{1.0};
    if (gst_structure_get_double(structure, "rate", &playbackRate) == TRUE)
    {
        std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
        if (client && m_mediaPlayerManager.hasControl())
        {
            GST_DEBUG_OBJECT(m_sink, "Instant playback rate change: %.2f", playbackRate);
            client->setPlaybackRate(playbackRate);
        }
    }
}

void PullModePlaybackDelegate::startFlushing()
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (!m_isFlushOngoing)
    {
        GST_INFO_OBJECT(m_sink, "Starting flushing");
        if (m_isEos)
        {
            GST_DEBUG_OBJECT(m_sink, "Flush will clear EOS state.");
            m_isEos = false;
        }
        m_isFlushOngoing = true;
        m_segmentSet = false;
        clearBuffersUnlocked();
    }
}

void PullModePlaybackDelegate::stopFlushing(bool resetTime)
{
    GST_INFO_OBJECT(m_sink, "Stopping flushing");
    flushServer(resetTime);
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    m_isFlushOngoing = false;

    if (resetTime)
    {
        GST_DEBUG_OBJECT(m_sink, "sending reset_time message");
        gst_element_post_message(m_sink, gst_message_new_reset_time(GST_OBJECT_CAST(m_sink), 0));
    }
}

void PullModePlaybackDelegate::flushServer(bool resetTime)
{
    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(m_sink, "Could not get the media player client");
        return;
    }

    std::unique_lock<std::mutex> lock(m_flushMutex);
    GST_INFO_OBJECT(m_sink, "Flushing sink with sourceId %d", m_sourceId.load());
    client->flush(m_sourceId, resetTime);
    if (m_sourceAttached)
    {
        m_flushCondVariable.wait(lock);
    }
    else
    {
        GST_DEBUG_OBJECT(m_sink, "Skip waiting for flush finish - source not attached yet.");
    }
}

GstFlowReturn PullModePlaybackDelegate::handleBuffer(GstBuffer *buffer)
{
    constexpr size_t kMaxInternalBuffersQueueSize = 24;
    GST_LOG_OBJECT(m_sink, "Handling buffer %p with PTS %" GST_TIME_FORMAT, buffer,
                   GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

    std::unique_lock<std::mutex> lock(m_sinkMutex);

    if (m_samples.size() >= kMaxInternalBuffersQueueSize)
    {
        GST_DEBUG_OBJECT(m_sink, "Waiting for more space in buffers queue\n");
        m_needDataCondVariable.wait(lock);
    }

    if (m_isFlushOngoing)
    {
        GST_DEBUG_OBJECT(m_sink, "Discarding buffer which was received during flushing");
        gst_buffer_unref(buffer);
        return GST_FLOW_FLUSHING;
    }

    GstSample *sample = gst_sample_new(buffer, m_caps, &m_lastSegment, nullptr);
    if (sample)
        m_samples.push(sample);
    else
        GST_ERROR_OBJECT(m_sink, "Failed to create a sample");

    gst_buffer_unref(buffer);

    return GST_FLOW_OK;
}

GstRefSample PullModePlaybackDelegate::getFrontSample() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (!m_samples.empty())
    {
        GstSample *sample = m_samples.front();
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GST_LOG_OBJECT(m_sink, "Pulling buffer %p with PTS %" GST_TIME_FORMAT, buffer,
                       GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));

        return GstRefSample{sample};
    }

    return GstRefSample{};
}

void PullModePlaybackDelegate::popSample()
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    if (!m_samples.empty())
    {
        gst_sample_unref(m_samples.front());
        m_samples.pop();
    }
    m_needDataCondVariable.notify_all();
}

bool PullModePlaybackDelegate::isEos() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    return m_samples.empty() && m_isEos;
}

bool PullModePlaybackDelegate::isReadyToSendData() const
{
    std::lock_guard<std::mutex> lock(m_sinkMutex);
    return m_isEos || m_segmentSet;
}

void PullModePlaybackDelegate::lostState()
{
    m_isStateCommitNeeded = true;
    gst_element_lost_state(m_sink);
}

bool PullModePlaybackDelegate::attachToMediaClientAndSetStreamsNumber(const uint32_t maxVideoWidth,
                                                                      const uint32_t maxVideoHeight)
{
    GstObject *parentObject = getOldestGstBinParent(m_sink);
    if (!m_mediaPlayerManager.attachMediaPlayerClient(parentObject, maxVideoWidth, maxVideoHeight))
    {
        GST_ERROR_OBJECT(m_sink, "Cannot attach the MediaPlayerClient");
        return false;
    }

    gchar *parentObjectName = gst_object_get_name(parentObject);
    GST_INFO_OBJECT(m_sink, "Attached media player client with parent %s(%p)", parentObjectName, parentObject);
    g_free(parentObjectName);

    return setStreamsNumber(parentObject);
}

bool PullModePlaybackDelegate::setStreamsNumber(GstObject *parentObject)
{
    int32_t videoStreams{-1}, audioStreams{-1}, subtitleStreams{-1};

    GstContext *context = gst_element_get_context(m_sink, "streams-info");
    if (context)
    {
        GST_DEBUG_OBJECT(m_sink, "Getting number of streams from \"streams-info\" context");

        guint n_video{0}, n_audio{0}, n_text{0};

        const GstStructure *streamsInfoStructure = gst_context_get_structure(context);
        gst_structure_get_uint(streamsInfoStructure, "video-streams", &n_video);
        gst_structure_get_uint(streamsInfoStructure, "audio-streams", &n_audio);
        gst_structure_get_uint(streamsInfoStructure, "text-streams", &n_text);

        if (n_video > std::numeric_limits<int32_t>::max() || n_audio > std::numeric_limits<int32_t>::max() ||
            n_text > std::numeric_limits<int32_t>::max())
        {
            GST_ERROR_OBJECT(m_sink, "Number of streams is too big, video=%u, audio=%u, text=%u", n_video, n_audio,
                             n_text);
            gst_context_unref(context);
            return false;
        }

        videoStreams = n_video;
        audioStreams = n_audio;
        subtitleStreams = n_text;

        gst_context_unref(context);
    }
    else if (getNStreamsFromParent(parentObject, videoStreams, audioStreams, subtitleStreams))
    {
        GST_DEBUG_OBJECT(m_sink, "Got number of streams from playbin2 properties");
    }
    else
    {
        // The default value of streams is V:1, A:1, S:0
        // Changing the default setting via properties is considered as DEPRECATED
        subtitleStreams = 0;
        std::lock_guard<std::mutex> lock{m_sinkMutex};
        if (m_mediaSourceType == firebolt::rialto::MediaSourceType::VIDEO)
        {
            videoStreams = m_numOfStreams;
            if (m_isSinglePathStream)
            {
                audioStreams = 0;
                subtitleStreams = 0;
            }
        }
        else if (m_mediaSourceType == firebolt::rialto::MediaSourceType::AUDIO)
        {
            audioStreams = m_numOfStreams;
            if (m_isSinglePathStream)
            {
                videoStreams = 0;
                subtitleStreams = 0;
            }
        }
        else if (m_mediaSourceType == firebolt::rialto::MediaSourceType::SUBTITLE)
        {
            subtitleStreams = m_numOfStreams;
            if (m_isSinglePathStream)
            {
                videoStreams = 0;
                audioStreams = 0;
            }
        }
    }

    std::shared_ptr<GStreamerMSEMediaPlayerClient> client = m_mediaPlayerManager.getMediaPlayerClient();
    if (!client)
    {
        GST_ERROR_OBJECT(m_sink, "MediaPlayerClient is nullptr");
        return false;
    }

    client->handleStreamCollection(audioStreams, videoStreams, subtitleStreams);

    return true;
}