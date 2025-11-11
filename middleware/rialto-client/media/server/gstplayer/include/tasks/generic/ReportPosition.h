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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_REPORT_POSITION_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_REPORT_POSITION_H_

#include "GenericPlayerContext.h"
#include "IGstGenericPlayerClient.h"
#include "IGstGenericPlayerPrivate.h"
#include "IGstWrapper.h"
#include "IPlayerTask.h"
#include <memory>

namespace firebolt::rialto::server::tasks::generic
{
class ReportPosition : public IPlayerTask
{
public:
    ReportPosition(GenericPlayerContext &context, IGstGenericPlayerClient *client,
                   const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                   IGstGenericPlayerPrivate &player);
    ~ReportPosition() override = default;
    void execute() const override;

private:
    GenericPlayerContext &m_context;
    IGstGenericPlayerClient *m_gstPlayerClient;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    IGstGenericPlayerPrivate &m_player;
};
} // namespace firebolt::rialto::server::tasks::generic

#endif // FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_REPORT_POSITION_H_
