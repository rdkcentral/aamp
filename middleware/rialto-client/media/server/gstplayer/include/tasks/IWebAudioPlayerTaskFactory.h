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

#ifndef FIREBOLT_RIALTO_SERVER_I_WEB_AUDIO_PLAYER_TASK_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_I_WEB_AUDIO_PLAYER_TASK_FACTORY_H_

#include "IGstWebAudioPlayerPrivate.h"
#include "IHeartbeatHandler.h"
#include "IPlayerTask.h"
#include "WebAudioPlayerContext.h"
#include <memory>
#include <string>

namespace firebolt::rialto::server
{
/**
 * @brief IWebAudioPlayerTaskFactory factory class, returns a concrete implementation of IPlayerTask
 */
class IWebAudioPlayerTaskFactory
{
public:
    IWebAudioPlayerTaskFactory() = default;
    virtual ~IWebAudioPlayerTaskFactory() = default;

    /**
     * @brief Creates a Shutdown task.
     *
     * @param[in] player     : The GstWebAudioPlayer instance
     *
     * @retval the new Shutdown task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createShutdown(IGstWebAudioPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a Stop task.
     *
     * @param[in] player     : The GstWebAudioPlayer instance
     *
     * @retval the new Stop task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createStop(IGstWebAudioPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a Play task.
     *
     * @param[in] player     : The GstWebAudioPlayer instance
     *
     * @retval the new Play task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createPlay(IGstWebAudioPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a Pause task.
     *
     * @param[in] player     : The GstWebAudioPlayer instance
     *
     * @retval the new Pause task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createPause(IGstWebAudioPlayerPrivate &player) const = 0;

    /**
     * @brief Creates a SetCaps task.
     *
     * @param[in] context       : The GstWebAudioPlayer context
     * @param[in] audioMimeType : The audio encoding format, currently only "audio/x-raw" (PCM)
     * @param[in] config        : Additional type dependent configuration data or nullptr
     *
     * @retval the new SetCaps task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetCaps(WebAudioPlayerContext &context, const std::string &audioMimeType,
                                                       std::weak_ptr<const WebAudioConfig> config) const = 0;

    /**
     * @brief Creates a Eos task.
     *
     * @param[in] context       : The GstWebAudioPlayer context
     *
     * @retval the new Eos task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createEos(WebAudioPlayerContext &context) const = 0;

    /**
     * @brief Creates a SetVolume task.
     *
     * @param[in] context       : The GstWebAudioPlayer context
     * @param[in] volume        : The volume to set
     *
     * @retval the new SetVolume task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createSetVolume(WebAudioPlayerContext &context, double volume) const = 0;

    /**
     * @brief Creates a WriteBuffer task.
     *
     * @param[in] context       : The GstWebAudioPlayer context
     * @param[in] mainPtr       : Pointer to the start of the data
     * @param[in] mainLength    : Amount of bytes to write from the mainPtr
     * @param[in] wrapPtr       : Pointer to the start of the wrapped data
     * @param[in] wrapLength    : Amount of bytes to write from the wrapPtr
     *
     * @retval the new SetVolume task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createWriteBuffer(WebAudioPlayerContext &context, uint8_t *mainPtr,
                                                           uint32_t mainLength, uint8_t *wrapPtr,
                                                           uint32_t wrapLength) const = 0;

    /**
     * @brief Creates a HandleBusMessage task.
     *
     * @param[in] context    : The GstWebAudioPlayer context
     * @param[in] player     : The GstWebAudioPlayer instance
     * @param[in] message    : The message to be handled
     *
     * @retval the new HandleBusMessage task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createHandleBusMessage(WebAudioPlayerContext &context,
                                                                IGstWebAudioPlayerPrivate &player,
                                                                GstMessage *message) const = 0;

    /**
     * @brief Creates a Ping task.
     *
     * @param[in] heartbeatHandler       : The HeartbeatHandler instance
     *
     * @retval the new Ping task instance.
     */
    virtual std::unique_ptr<IPlayerTask> createPing(std::unique_ptr<IHeartbeatHandler> &&heartbeatHandler) const = 0;
};

} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_WEB_AUDIO_PLAYER_TASK_FACTORY_H_
