/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

/**
 * @file AampRialtoControlBackend.cpp
 * @brief Production implementation of IRialtoControlBackend.
 */

#include "AampRialtoControlBackend.h"
#include "AampLogManager.h"

#include <chrono>

// ---------------------------------------------------------------------------
// ControlClient
// ---------------------------------------------------------------------------

void AampRialtoControlBackend::ControlClient::notifyApplicationState(
	firebolt::rialto::ApplicationState state)
{
	m_backend.onApplicationStateChanged(state);
}

// ---------------------------------------------------------------------------
// AampRialtoControlBackend
// ---------------------------------------------------------------------------

AampRialtoControlBackend::AampRialtoControlBackend()
	: m_controlClient{std::make_shared<ControlClient>(*this)}
{
	auto factory = firebolt::rialto::IControlFactory::createFactory();
	if (!factory)
	{
		AAMPLOG_ERR("AampRialtoControlBackend: failed to create IControlFactory");
		return;
	}

	m_control = factory->createControl();
	if (!m_control)
	{
		AAMPLOG_ERR("AampRialtoControlBackend: failed to create IControl");
		return;
	}

	if (!m_control->registerClient(m_controlClient, m_rialtoClientState))
	{
		AAMPLOG_ERR("AampRialtoControlBackend: IControl::registerClient failed");
		m_control.reset();
		return;
	}

	AAMPLOG_INFO("AampRialtoControlBackend: registered, initial state=%d",
		static_cast<int>(m_rialtoClientState));
}

bool AampRialtoControlBackend::waitForRunning(int timeoutMs)
{
	std::unique_lock<std::mutex> lock{m_mutex};
	if (firebolt::rialto::ApplicationState::RUNNING == m_rialtoClientState)
	{
		return true;
	}
	m_stateCv.wait_for(
		lock,
		std::chrono::milliseconds{timeoutMs},
		[this]
		{
			return m_rialtoClientState ==
				firebolt::rialto::ApplicationState::RUNNING;
		});
	return firebolt::rialto::ApplicationState::RUNNING == m_rialtoClientState;
}

void AampRialtoControlBackend::onApplicationStateChanged(
	firebolt::rialto::ApplicationState state)
{
	AAMPLOG_INFO("AampRialtoControlBackend: application state changed to %d",
		static_cast<int>(state));
	{
		std::lock_guard<std::mutex> lock{m_mutex};
		m_rialtoClientState = state;
	}
	m_stateCv.notify_one();
}
