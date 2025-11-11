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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_HANDLE_BUS_MESSAGE_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_HANDLE_BUS_MESSAGE_H_

#include "IGlibWrapper.h"
#include "IGstWebAudioPlayerClient.h"
#include "IGstWebAudioPlayerPrivate.h"
#include "IGstWrapper.h"
#include "IPlayerTask.h"
#include "WebAudioPlayerContext.h"
#include <gst/gst.h>
#include <memory>

namespace firebolt::rialto::server::tasks::webaudio
{
class HandleBusMessage : public IPlayerTask
{
public:
    HandleBusMessage(WebAudioPlayerContext &context, IGstWebAudioPlayerPrivate &player, IGstWebAudioPlayerClient *client,
                     std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                     std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper, GstMessage *message);
    ~HandleBusMessage() override;
    void execute() const override;

private:
    WebAudioPlayerContext &m_context;
    IGstWebAudioPlayerPrivate &m_player;
    IGstWebAudioPlayerClient *m_gstPlayerClient;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;
    GstMessage *m_message;
};
} // namespace firebolt::rialto::server::tasks::webaudio

#endif // FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_HANDLE_BUS_MESSAGE_H_
