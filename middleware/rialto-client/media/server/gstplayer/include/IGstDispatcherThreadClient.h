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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_DISPATCHER_THREAD_CLIENT_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_DISPATCHER_THREAD_CLIENT_H_

#include <gst/gst.h>

namespace firebolt::rialto::server
{
class IGstDispatcherThreadClient
{
public:
    IGstDispatcherThreadClient() = default;
    virtual ~IGstDispatcherThreadClient() = default;

    IGstDispatcherThreadClient(const IGstDispatcherThreadClient &) = delete;
    IGstDispatcherThreadClient(IGstDispatcherThreadClient &&) = delete;
    IGstDispatcherThreadClient &operator=(const IGstDispatcherThreadClient &) = delete;
    IGstDispatcherThreadClient &operator=(IGstDispatcherThreadClient &&) = delete;

    /**
     * @brief Callback on a new bus message.
     *
     * @param[in] message : The new Gst message.
     */
    virtual void handleBusMessage(GstMessage *message, bool priority) = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_DISPATCHER_THREAD_CLIENT_H_
