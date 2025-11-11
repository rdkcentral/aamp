/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_SET_SOURCE_POSITION_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_SET_SOURCE_POSITION_H_

#include "GenericPlayerContext.h"
#include "IGlibWrapper.h"
#include "IGstGenericPlayerClient.h"
#include "IGstGenericPlayerPrivate.h"
#include "IGstWrapper.h"
#include "IPlayerTask.h"
#include <cstdint>
#include <memory>

namespace firebolt::rialto::server::tasks::generic
{
class SetSourcePosition : public IPlayerTask
{
public:
    SetSourcePosition(GenericPlayerContext &context, IGstGenericPlayerPrivate &player, IGstGenericPlayerClient *client,
                      const std::shared_ptr<wrappers::IGlibWrapper> &glibWrapper, const MediaSourceType &type,
                      std::int64_t position, bool resetTime, double appliedRate, uint64_t stopPosition);
    ~SetSourcePosition() override;
    void execute() const override;

private:
    void setSubtitlePosition(GstElement *source) const;
    GenericPlayerContext &m_context;
    IGstGenericPlayerPrivate &m_player;
    IGstGenericPlayerClient *m_gstPlayerClient;
    std::shared_ptr<wrappers::IGlibWrapper> m_glibWrapper;
    MediaSourceType m_type;
    std::int64_t m_position;
    bool m_resetTime;
    double m_appliedRate;
    uint64_t m_stopPosition;
};

} // namespace firebolt::rialto::server::tasks::generic

#endif // FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_SET_SOURCE_POSITION_H_
