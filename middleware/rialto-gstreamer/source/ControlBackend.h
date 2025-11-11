/*
 * Copyright (C) 2022   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <condition_variable>
#include <gst/gst.h>
#include <mutex>

#include "ControlBackendInterface.h"
#include "IControl.h"

namespace firebolt::rialto::client
{
class ControlBackend final : public ControlBackendInterface
{
    class ControlClient : public IControlClient
    {
    public:
        explicit ControlClient(ControlBackend &backend) : mBackend{backend} {}
        ~ControlClient() override = default;
        void notifyApplicationState(ApplicationState state) override
        {
            GST_INFO("ApplicationStateChanged received by rialto sink");
            mBackend.onApplicationStateChanged(state);
        }

    private:
        ControlBackend &mBackend;
    };

public:
    ControlBackend()
        : m_rialtoClientState{ApplicationState::UNKNOWN}, m_controlClient{std::make_shared<ControlClient>(*this)},
          m_control{IControlFactory::createFactory()->createControl()}
    {
        if (!m_control)
        {
            GST_ERROR("Unable to create control");
            return;
        }
        if (!m_control->registerClient(m_controlClient, m_rialtoClientState))
        {
            GST_ERROR("Unable to register client");
            return;
        }
    }

    ~ControlBackend() final { m_control.reset(); }

    void removeControlBackend() override { m_control.reset(); }

    bool waitForRunning() override
    {
        std::unique_lock<std::mutex> lock{m_mutex};
        if (ApplicationState::RUNNING == m_rialtoClientState)
        {
            return true;
        }
        m_stateCv.wait_for(lock, std::chrono::seconds{1},
                           [&]() { return m_rialtoClientState == ApplicationState::RUNNING; });
        return ApplicationState::RUNNING == m_rialtoClientState;
    }

private:
    void onApplicationStateChanged(ApplicationState state)
    {
        GST_INFO("Rialto Client application state changed to: %s",
                 state == ApplicationState::RUNNING ? "Active" : "Inactive/Unknown");
        std::unique_lock<std::mutex> lock{m_mutex};
        m_rialtoClientState = state;
        m_stateCv.notify_one();
    }

private:
    ApplicationState m_rialtoClientState;
    std::shared_ptr<ControlClient> m_controlClient;
    std::shared_ptr<IControl> m_control;
    std::mutex m_mutex;
    std::condition_variable m_stateCv;
};
} // namespace firebolt::rialto::client
