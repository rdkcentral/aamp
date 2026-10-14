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
 * @file AampRialtoControlBackend.h
 * @brief Production implementation of IRialtoControlBackend.
 *
 * Follows the pattern of firebolt::rialto::client::ControlBackend from
 * rialto-gstreamer: a nested IControlClient receives application-state
 * notifications from IControl and unblocks waitForRunning() once the server
 * reports ApplicationState::RUNNING.
 */

#ifndef AAMP_RIALTO_CONTROL_BACKEND_H
#define AAMP_RIALTO_CONTROL_BACKEND_H

#include "IRialtoControlBackend.h"
#include "IControl.h"
#include "IControlClient.h"

#include <condition_variable>
#include <memory>
#include <mutex>

/**
 * @class AampRialtoControlBackend
 * @brief Registers with Rialto's IControl on construction and exposes
 *        waitForRunning() to block until ApplicationState::RUNNING is seen.
 *
 * The IControl object is created eagerly in the constructor so that the
 * client is registered as early as possible — before any IMediaPipeline is
 * created — minimising the window in which the RUNNING broadcast can be
 * missed.
 */
class AampRialtoControlBackend final : public IRialtoControlBackend
{
	/**
	 * @brief Application state shared between the backend and its client.
	 *
	 * The client co-owns it, so a notification that is still running when the
	 * backend is destroyed (Rialto's event thread holds the client for the
	 * duration of the call) never touches destroyed members, whatever order
	 * they are destroyed in.
	 */
	struct SharedState
	{
		std::mutex                         mutex;
		std::condition_variable            cv;
		firebolt::rialto::ApplicationState state{
			firebolt::rialto::ApplicationState::UNKNOWN};
		/// True once a notification has set @c state, so the initial state
		/// returned by registerClient() cannot overwrite a newer one.
		bool                               notified{false};
	};

	/**
	 * @brief Nested IControlClient that records state notifications in the
	 *        shared state and wakes waitForRunning().
	 */
	class ControlClient final : public firebolt::rialto::IControlClient
	{
	public:
		explicit ControlClient(std::shared_ptr<SharedState> state)
			: m_state{std::move(state)} {}
		~ControlClient() override = default;

		void notifyApplicationState(
			firebolt::rialto::ApplicationState state) override;

	private:
		std::shared_ptr<SharedState> m_state;
	};

public:
	AampRialtoControlBackend();
	~AampRialtoControlBackend() override = default;

	/// @copydoc IRialtoControlBackend::waitForRunning
	bool waitForRunning(int timeoutMs) override;

private:
	std::shared_ptr<SharedState>             m_state;
	std::shared_ptr<ControlClient>           m_controlClient;
	std::shared_ptr<firebolt::rialto::IControl> m_control;
};

#endif // AAMP_RIALTO_CONTROL_BACKEND_H
