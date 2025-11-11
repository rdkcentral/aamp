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

#include "tasks/generic/SetupElement.h"
#include "GenericPlayerContext.h"
#include "IGlibWrapper.h"
#include "IGstGenericPlayerPrivate.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include "Utils.h"

namespace
{
/**
 * @brief Callback for audio underflow event from sink. Called by the Gstreamer thread.
 *
 * @param[in] object     : the object that emitted the signal
 * @param[in] fifoDepth  : the fifo depth (may be 0)
 * @param[in] queueDepth : the queue depth (may be NULL)
 * @param[in] self       : The pointer to IGstGenericPlayerPrivate
 *
 * @retval true if the handling of the message is successful, false otherwise.
 */
void audioUnderflowCallback(GstElement *object, guint fifoDepth, gpointer queueDepth, gpointer self)
{
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(self);
    player->scheduleAudioUnderflow();
}

/**
 * @brief Callback for video underflow event from sink. Called by the Gstreamer thread.
 *
 * @param[in] object     : the object that emitted the signal
 * @param[in] fifoDepth  : the fifo depth (may be 0)
 * @param[in] queueDepth : the queue depth (may be NULL)
 * @param[in] self       : The pointer to IGstGenericPlayerPrivate
 *
 * @retval true if the handling of the message is successful, false otherwise.
 */
void videoUnderflowCallback(GstElement *object, guint fifoDepth, gpointer queueDepth, gpointer self)
{
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(self);
    player->scheduleVideoUnderflow();
}

/**
 * @brief Callback for a autovideosink when a child has been added to the sink.
 *
 * @param[in] obj        : the parent element (autovideosink)
 * @param[in] object     : the child element
 * @param[in] name       : the name of the child element
 * @param[in] self       : The pointer to IGstGenericPlayerPrivate
 */
void autoVideoSinkChildAddedCallback(GstChildProxy *obj, GObject *object, gchar *name, gpointer self)
{
    RIALTO_SERVER_LOG_DEBUG("AutoVideoSink added element %s", name);
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(self);
    player->addAutoVideoSinkChild(object);
}

/**
 * @brief Callback for a autoaudiosink when a child has been added to the sink.
 *
 * @param[in] obj        : the parent element (autoaudiosink)
 * @param[in] object     : the child element
 * @param[in] name       : the name of the child element
 * @param[in] self       : The pointer to IGstGenericPlayerPrivate
 */
void autoAudioSinkChildAddedCallback(GstChildProxy *obj, GObject *object, gchar *name, gpointer self)
{
    RIALTO_SERVER_LOG_DEBUG("AutoAudioSink added element %s", name);
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(self);
    player->addAutoAudioSinkChild(object);
}

/**
 * @brief Callback for a autovideosink when a child has been removed from the sink.
 *
 * @param[in] obj        : the parent element (autovideosink)
 * @param[in] object     : the child element
 * @param[in] name       : the name of the child element
 * @param[in] self       : The pointer to IGstGenericPlayerPrivate
 */
void autoVideoSinkChildRemovedCallback(GstChildProxy *obj, GObject *object, gchar *name, gpointer self)
{
    RIALTO_SERVER_LOG_DEBUG("AutoVideoSink removed element %s", name);
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(self);
    player->removeAutoVideoSinkChild(object);
}

/**
 * @brief Callback for a autoaudiosink when a child has been removed from the sink.
 *
 * @param[in] obj        : the parent element (autoaudiosink)
 * @param[in] object     : the child element
 * @param[in] name       : the name of the child element
 * @param[in] self       : The pointer to IGstGenericPlayerPrivate
 */
void autoAudioSinkChildRemovedCallback(GstChildProxy *obj, GObject *object, gchar *name, gpointer self)
{
    RIALTO_SERVER_LOG_DEBUG("AutoAudioSink removed element %s", name);
    firebolt::rialto::server::IGstGenericPlayerPrivate *player =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(self);
    player->removeAutoAudioSinkChild(object);
}
} // namespace

namespace firebolt::rialto::server::tasks::generic
{
SetupElement::SetupElement(GenericPlayerContext &context,
                           std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                           std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                           IGstGenericPlayerPrivate &player, GstElement *element)
    : m_context{context}, m_gstWrapper{gstWrapper}, m_glibWrapper{glibWrapper}, m_player{player}, m_element{element}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing SetupElement");
}

SetupElement::~SetupElement()
{
    RIALTO_SERVER_LOG_DEBUG("SetupElement finished");
}

void SetupElement::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing SetupElement");

    const std::string kElementTypeName = m_glibWrapper->gTypeName(G_OBJECT_TYPE(m_element));
    if (kElementTypeName == "GstAutoVideoSink")
    {
        // Check and store child sink so we can set underlying properties
        m_glibWrapper->gSignalConnect(m_element, "child-added", G_CALLBACK(autoVideoSinkChildAddedCallback), &m_player);
        m_glibWrapper->gSignalConnect(m_element, "child-removed", G_CALLBACK(autoVideoSinkChildRemovedCallback),
                                      &m_player);

        // AutoVideoSink sets child before it is setup on the pipeline, so check for children here
        GstIterator *sinks = m_gstWrapper->gstBinIterateSinks(GST_BIN(m_element));
        if (sinks && sinks->size > 1)
        {
            RIALTO_SERVER_LOG_WARN("More than one child sink attached");
        }

        GValue elem = G_VALUE_INIT;
        if (m_gstWrapper->gstIteratorNext(sinks, &elem) == GST_ITERATOR_OK)
        {
            m_player.addAutoVideoSinkChild(G_OBJECT(m_glibWrapper->gValueGetObject(&elem)));
        }
        m_glibWrapper->gValueUnset(&elem);

        if (sinks)
            m_gstWrapper->gstIteratorFree(sinks);
    }
    else if (kElementTypeName == "GstAutoAudioSink")
    {
        // Check and store child sink so we can set underlying properties
        m_glibWrapper->gSignalConnect(m_element, "child-added", G_CALLBACK(autoAudioSinkChildAddedCallback), &m_player);
        m_glibWrapper->gSignalConnect(m_element, "child-removed", G_CALLBACK(autoAudioSinkChildRemovedCallback),
                                      &m_player);

        // AutoAudioSink sets child before it is setup on the pipeline, so check for children here
        GstIterator *sinks = m_gstWrapper->gstBinIterateSinks(GST_BIN(m_element));
        if (sinks && sinks->size > 1)
        {
            RIALTO_SERVER_LOG_WARN("More than one child sink attached");
        }

        GValue elem = G_VALUE_INIT;
        if (m_gstWrapper->gstIteratorNext(sinks, &elem) == GST_ITERATOR_OK)
        {
            m_player.addAutoAudioSinkChild(G_OBJECT(m_glibWrapper->gValueGetObject(&elem)));
        }
        m_glibWrapper->gValueUnset(&elem);

        if (sinks)
            m_gstWrapper->gstIteratorFree(sinks);
    }

    if (m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(m_element), "amlhalasink"))
    {
        if (m_context.streamInfo.find(MediaSourceType::VIDEO) != m_context.streamInfo.end())
        {
            // Wait for video so that the audio aligns at the starting point with timeout of 4000ms.
            m_glibWrapper->gObjectSet(m_element, "wait-video", TRUE, "a-wait-timeout", 4000, nullptr);
        }

        // Xrun occasionally pauses the underlying sink due to unstable playback, but the rest of the pipeline
        // remains in the playing state. This causes problems with the synchronization of gst element and rialto
        // ultimately hangs waiting for pipeline termination.
        m_glibWrapper->gObjectSet(m_element, "disable-xrun", TRUE, nullptr);
    }
    else if (m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(m_element), "brcmaudiosink"))
    {
        m_glibWrapper->gObjectSet(m_element, "async", TRUE, nullptr);
    }
    else if (m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(m_element), "rialtotexttracksink"))
    {
        // in cannot be set during construction, because playsink overwrites "sync" value of text-sink during setup
        m_glibWrapper->gObjectSet(m_element, "sync", FALSE, nullptr);
    }

    if (m_context.subtitleSink)
    {
        if (!m_context.isVideoHandleSet && isVideoDecoder(*m_gstWrapper, m_element))
        {
            if (m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(m_element), "omx") ||
                m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(m_element), "westerossink") ||
                m_glibWrapper->gStrHasPrefix(GST_ELEMENT_NAME(m_element), "brcmvideodecoder"))
            {
                uintptr_t videoDecoderHandle = reinterpret_cast<uintptr_t>(m_element);
                m_glibWrapper->gObjectSet(m_context.subtitleSink, "video-decoder", videoDecoderHandle, nullptr);
                RIALTO_SERVER_LOG_INFO("Setting video decoder handle for subtitle sink: %zu", videoDecoderHandle);
                m_context.isVideoHandleSet = true;
            }
        }
    }

    if (isDecoder(*m_gstWrapper, m_element) || isSink(*m_gstWrapper, m_element))
    {
        std::optional<std::string> underflowSignalName = getUnderflowSignalName(*m_glibWrapper, m_element);
        if (underflowSignalName)
        {
            if (isAudio(*m_gstWrapper, m_element))
            {
                RIALTO_SERVER_LOG_INFO("Connecting audio underflow callback for signal: %s",
                                       underflowSignalName.value().c_str());
                m_glibWrapper->gSignalConnect(m_element, underflowSignalName.value().c_str(),
                                              G_CALLBACK(audioUnderflowCallback), &m_player);
            }
            else if (isVideo(*m_gstWrapper, m_element))
            {
                RIALTO_SERVER_LOG_INFO("Connecting video underflow callback for signal: %s",
                                       underflowSignalName.value().c_str());
                m_glibWrapper->gSignalConnect(m_element, underflowSignalName.value().c_str(),
                                              G_CALLBACK(videoUnderflowCallback), &m_player);
            }
        }
    }

    if (isVideoSink(*m_gstWrapper, m_element))
    {
        if (!m_context.videoSink)
        {
            m_gstWrapper->gstObjectRef(m_element);
            m_context.videoSink = m_element;
        }
        if (!m_context.pendingGeometry.empty())
        {
            m_player.setVideoSinkRectangle();
        }
        if (m_context.pendingImmediateOutputForVideo.has_value())
        {
            m_player.setImmediateOutput();
        }
        if (m_context.pendingRenderFrame)
        {
            m_player.setRenderFrame();
        }
        if (m_context.pendingShowVideoWindow.has_value())
        {
            m_player.setShowVideoWindow();
        }
    }
    else if (isAudioDecoder(*m_gstWrapper, m_element))
    {
        if (m_context.pendingSyncOff.has_value())
        {
            m_player.setSyncOff();
        }
        if (m_context.pendingStreamSyncMode.find(MediaSourceType::AUDIO) != m_context.pendingStreamSyncMode.end())
        {
            m_player.setStreamSyncMode(MediaSourceType::AUDIO);
        }
        if (m_context.pendingBufferingLimit.has_value())
        {
            m_player.setBufferingLimit();
        }
    }
    else if (isAudioSink(*m_gstWrapper, m_element))
    {
        if (m_context.pendingLowLatency.has_value())
        {
            m_player.setLowLatency();
        }
        if (m_context.pendingSync.has_value())
        {
            m_player.setSync();
        }
    }
    else if (isVideoParser(*m_gstWrapper, m_element))
    {
        if (m_context.pendingStreamSyncMode.find(MediaSourceType::VIDEO) != m_context.pendingStreamSyncMode.end())
        {
            m_player.setStreamSyncMode(MediaSourceType::VIDEO);
        }
    }

    if (m_gstWrapper->gstIsBaseParse(m_element))
    {
        m_gstWrapper->gstBaseParseSetPtsInterpolation(GST_BASE_PARSE(m_element), FALSE);
    }

    m_gstWrapper->gstObjectUnref(m_element);
}
} // namespace firebolt::rialto::server::tasks::generic
