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
 * @file FakeAampRialtoControlBackend.cpp
 * @brief Stub implementation of AampRialtoControlBackend for L1 tests.
 *
 * Provides linker symbols for the legacy AampRialtoPlayer constructor that
 * calls std::make_unique<AampRialtoControlBackend>().  Tests that exercise
 * AampRialtoPlayer use the test constructor which injects a
 * MockIRialtoControlBackend directly, bypassing this fake.
 */

#include "AampRialtoControlBackend.h"

AampRialtoControlBackend::AampRialtoControlBackend()
	: m_controlClient{nullptr}
{
}

bool AampRialtoControlBackend::waitForRunning(int /*timeoutMs*/)
{
	return true;
}

void AampRialtoControlBackend::onApplicationStateChanged(
	firebolt::rialto::ApplicationState /*state*/)
{
}

void AampRialtoControlBackend::ControlClient::notifyApplicationState(
	firebolt::rialto::ApplicationState /*state*/)
{
}
