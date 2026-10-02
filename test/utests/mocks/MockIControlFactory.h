/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef MOCK_ICONTROL_FACTORY_H
#define MOCK_ICONTROL_FACTORY_H

#include "IControl.h"
#include <gmock/gmock.h>
#include <memory>

class MockIControlFactory : public firebolt::rialto::IControlFactory
{
public:
	MOCK_METHOD(std::shared_ptr<firebolt::rialto::IControl>, createControl,
		(), (const, override));
};

class MockIControl : public firebolt::rialto::IControl
{
public:
	MOCK_METHOD(bool, registerClient,
		(std::weak_ptr<firebolt::rialto::IControlClient> client,
		 firebolt::rialto::ApplicationState &appState),
		(override));
};

/// Global mock instance returned by IControlFactory::createFactory() in the
/// test-local factory stub.
extern std::shared_ptr<MockIControlFactory> g_mockControlFactory;

#endif // MOCK_ICONTROL_FACTORY_H
