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

#include "tasks/generic/FinishSetupSource.h"
#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "IGstGenericPlayerPrivate.h"
#include "RialtoServerLogging.h"
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

namespace
{
/**
 * @brief Callback for need-data event from gstreamer. Called by the Gstreamer thread.
 *
 * @param[in] src       : the appsrc element that emitted the signal
 * @param[in] length    : the amount of bytes needed.
 * @param[in] user_data : The data to be passed with the message.
 *
 */
void appSrcNeedData(GstAppSrc *src, guint length, gpointer user_data)
{
    firebolt::rialto::server::IGstGenericPlayerPrivate *self =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(user_data);
    self->scheduleNeedMediaData(src);
}

/**
 * @brief Callback for enough-data event from gstreamer. Called by the Gstreamer thread.
 *
 * @param[in] src       : the appsrc element that emitted the signal
 * @param[in] user_data : The data to be passed with the message.
 *
 */
void appSrcEnoughData(GstAppSrc *src, gpointer user_data)
{
    firebolt::rialto::server::IGstGenericPlayerPrivate *self =
        static_cast<firebolt::rialto::server::IGstGenericPlayerPrivate *>(user_data);
    self->scheduleEnoughData(src);
}

/**
 * @brief Callback for seek-data event from gstreamer. Called by the Gstreamer thread.
 *
 * @param[in] src       : the appsrc element that emitted the signal
 * @param[in] offset    : the offset to seek to
 * @param[in] user_data : The data to be passed with the message.
 *
 * @retval true if the handling of the message is successful, false otherwise.
 */
gboolean appSrcSeekData(GstAppSrc *src, guint64 offset, gpointer user_data)
{
    appSrcEnoughData(src, user_data);
    return TRUE;
}
} // namespace

namespace firebolt::rialto::server::tasks::generic
{
FinishSetupSource::FinishSetupSource(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                                     IGstGenericPlayerClient *client)
    : m_context{context}, m_player{player}, m_gstPlayerClient{client}
{
    RIALTO_SERVER_LOG_DEBUG("Constructing FinishSetupSource");
}

FinishSetupSource::~FinishSetupSource()
{
    RIALTO_SERVER_LOG_DEBUG("FinishSetupSource finished");
}

void FinishSetupSource::execute() const
{
    RIALTO_SERVER_LOG_DEBUG("Executing FinishSetupSource");
    m_context.wereAllSourcesAttached = true;

    if (!m_context.source)
    {
        RIALTO_SERVER_LOG_DEBUG("Source is not ready");
        return;
    }

    GstAppSrcCallbacks callbacks = {appSrcNeedData, appSrcEnoughData, appSrcSeekData, {nullptr}};

    for (auto &elem : m_context.streamInfo)
    {
        firebolt::rialto::MediaSourceType sourceType = elem.first;
        if (sourceType == firebolt::rialto::MediaSourceType::UNKNOWN)
        {
            RIALTO_SERVER_LOG_WARN("Unknown media segment type");
            continue;
        }

        StreamInfo &streamInfo = elem.second;
        m_context.gstSrc->setupAndAddAppSrc(m_context.decryptionService, m_context.source, streamInfo, &callbacks,
                                            &m_player, sourceType);
        m_player.notifyNeedMediaData(sourceType);
    }

    m_context.gstSrc->allAppSrcsAdded(m_context.source);

    // Notify GstPlayerClient of Idle state once setup has finished
    if (m_gstPlayerClient)
        m_gstPlayerClient->notifyPlaybackState(PlaybackState::IDLE);

    m_context.setupSourceFinished = true;

    RIALTO_SERVER_LOG_MIL("All sources attached.");
}
} // namespace firebolt::rialto::server::tasks::generic
