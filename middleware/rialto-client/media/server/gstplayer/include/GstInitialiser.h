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

#ifndef FIREBOLT_RIALTO_SERVER_GST_INITIALISER_H_
#define FIREBOLT_RIALTO_SERVER_GST_INITIALISER_H_

#include "IGstInitialiser.h"
#include "IGstWrapper.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace firebolt::rialto::server
{
class GstInitialiser : public IGstInitialiser
{
public:
    GstInitialiser() = default;
    ~GstInitialiser();

    void initialise(int *argc, char ***argv) override;
    void waitForInitialisation() const override;

private:
    bool m_isInitialised{false};
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    std::thread m_thread;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
};
}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_INITIALISER_H_
