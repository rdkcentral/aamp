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

#ifndef FIREBOLT_RIALTO_SERVER_I_NEED_MEDIA_DATA_THREAD_H_
#define FIREBOLT_RIALTO_SERVER_I_NEED_MEDIA_DATA_THREAD_H_

namespace firebolt::rialto::server
{
class INeedMediaDataThread
{
public:
    INeedMediaDataThread() = default;
    virtual ~INeedMediaDataThread() = default;

    INeedMediaDataThread(const INeedMediaDataThread &) = delete;
    INeedMediaDataThread(INeedMediaDataThread &&) = delete;
    INeedMediaDataThread &operator=(const INeedMediaDataThread &) = delete;
    INeedMediaDataThread &operator=(INeedMediaDataThread &&) = delete;

    virtual void queueEvent() = 0;
    virtual void stop() = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_NEED_MEDIA_DATA_THREAD_H_
