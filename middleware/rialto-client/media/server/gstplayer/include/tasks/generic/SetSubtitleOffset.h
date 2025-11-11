/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 
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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_SET_SUBTITLE_OFFSET_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_SET_SUBTITLE_OFFSET_H_

#include "GenericPlayerContext.h"
#include "IGlibWrapper.h"
#include "IPlayerTask.h"
#include <cstdint>
#include <memory>

namespace firebolt::rialto::server::tasks::generic
{
class SetSubtitleOffset : public IPlayerTask
{
public:
    SetSubtitleOffset(GenericPlayerContext &context, const std::shared_ptr<wrappers::IGlibWrapper> &glibWrapper,
                      std::int64_t position);
    ~SetSubtitleOffset() override;
    void execute() const override;

private:
    GenericPlayerContext &m_context;
    std::shared_ptr<wrappers::IGlibWrapper> m_glibWrapper;
    std::int64_t m_position;
};

} // namespace firebolt::rialto::server::tasks::generic

#endif // FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_SET_SUBTITLE_OFFSET_H_
