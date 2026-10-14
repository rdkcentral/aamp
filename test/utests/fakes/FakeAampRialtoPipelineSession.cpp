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
 * @file FakeAampRialtoPipelineSession.cpp
 * @brief Inert AampRialtoPipelineSession for suites that link
 *        FakeAampRialtoPlayer; the player tests use the real class.
 */

#include "AampRialtoPipelineSession.h"

AampRialtoPipelineSession::AampRialtoPipelineSession(
	std::unique_ptr<IRialtoControlBackend> controlBackend)
	: m_controlBackend(std::move(controlBackend))
{
}

AampRialtoPipelineSession::~AampRialtoPipelineSession() {}

std::shared_ptr<firebolt::rialto::IMediaPipeline>
AampRialtoPipelineSession::GetPipeline() const
{
	return nullptr;
}

void AampRialtoPipelineSession::SetPipeline(
	std::shared_ptr<firebolt::rialto::IMediaPipeline>) {}

void AampRialtoPipelineSession::EnsureClient() {}

AampRialtoPipelineSession::CreateResult
AampRialtoPipelineSession::CreateAndLoad(const Callbacks &)
{
	return CreateResult::FactoryUnavailable;
}

void AampRialtoPipelineSession::ClearClientCallbacks() {}

void AampRialtoPipelineSession::ReleaseControlBackend() {}
