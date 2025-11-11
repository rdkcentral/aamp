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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_SET_VOLUME_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_SET_VOLUME_H_

#include "IGstWrapper.h"
#include "IPlayerTask.h"
#include "WebAudioPlayerContext.h"
#include <memory>

namespace firebolt::rialto::server::tasks::webaudio
{
class SetVolume : public IPlayerTask
{
public:
    SetVolume(WebAudioPlayerContext &context, std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
              double volume);
    ~SetVolume() override;
    void execute() const override;

private:
    WebAudioPlayerContext &m_context;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    double m_volume;
};
} // namespace firebolt::rialto::server::tasks::webaudio

#endif // FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_SET_VOLUME_H_
