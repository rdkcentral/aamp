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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_WRITE_BUFFER_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_WRITE_BUFFER_H_

#include "IGstWebAudioPlayerPrivate.h"
#include "IGstWrapper.h"
#include "IPlayerTask.h"
#include "MediaCommon.h"
#include "WebAudioPlayerContext.h"
#include <memory>

namespace firebolt::rialto::server::tasks::webaudio
{
class WriteBuffer : public IPlayerTask
{
public:
    WriteBuffer(WebAudioPlayerContext &context, std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                uint8_t *mainPtr, uint32_t mainLength, uint8_t *wrapPtr, uint32_t wrapLength);
    ~WriteBuffer() override;
    void execute() const override;

private:
    WebAudioPlayerContext &m_context;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    const uint8_t *m_mainPtr;
    const uint64_t m_mainLength;
    const uint8_t *m_wrapPtr;
    const uint64_t m_wrapLength;
};
} // namespace firebolt::rialto::server::tasks::webaudio

#endif // FIREBOLT_RIALTO_SERVER_TASKS_WEBAUDIO_WRITE_BUFFER_H_
