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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_ENOUGH_DATA_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_ENOUGH_DATA_H_

#include "IPlayerTask.h"
#include <gst/app/gstappsrc.h>

namespace firebolt::rialto::server
{
struct GenericPlayerContext;
} // namespace firebolt::rialto::server

namespace firebolt::rialto::server::tasks::generic
{
class EnoughData : public IPlayerTask
{
public:
    EnoughData(GenericPlayerContext &context, GstAppSrc *src);
    ~EnoughData() override;
    void execute() const override;

private:
    GenericPlayerContext &m_context;
    GstAppSrc *m_src;
};
} // namespace firebolt::rialto::server::tasks::generic

#endif // FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_ENOUGH_DATA_H_
