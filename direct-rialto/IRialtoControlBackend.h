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
 * @file IRialtoControlBackend.h
 * @brief Abstraction for the Rialto application-state control backend.
 *
 * Separates the "wait for Rialto server to reach RUNNING" responsibility
 * from AampRialtoPlayer so it can be tested in isolation and mocked in
 * unit tests without a live Rialto server.
 */

#ifndef IRIALTO_CONTROL_BACKEND_H
#define IRIALTO_CONTROL_BACKEND_H

/**
 * @class IRialtoControlBackend
 * @brief Interface that wraps the IControl/IControlClient handshake needed
 *        to confirm the Rialto server has reached ApplicationState::RUNNING.
 */
class IRialtoControlBackend
{
public:
	virtual ~IRialtoControlBackend() = default;

	/**
	 * @brief Block until the Rialto server reports ApplicationState::RUNNING
	 *        or the timeout expires.
	 *
	 * @param[in] timeoutMs Maximum time to wait in milliseconds.
	 * @return true if RUNNING was observed within the timeout, false otherwise.
	 */
	virtual bool waitForRunning(int timeoutMs) = 0;
};

#endif // IRIALTO_CONTROL_BACKEND_H
