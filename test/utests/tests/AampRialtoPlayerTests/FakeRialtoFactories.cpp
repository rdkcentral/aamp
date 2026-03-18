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
 * @file FakeRialtoFactories.cpp
 * @brief Linker stubs for Rialto factory singletons used in L1 tests.
 *
 * In production, IMediaPipelineFactory::createFactory() and
 * IClientLogControlFactory::createFactory() connect to the Rialto server.
 * In unit tests these are never called (AampRialtoPlayer uses
 * SetPipelineFactoryForTesting() to inject mocks, and the log bridge is
 * suppressed when the factory returns nullptr), but the linker still
 * requires the symbols to resolve.
 */

#include "IMediaPipeline.h"
#include "IClientLogControl.h"
#include <memory>

namespace firebolt::rialto
{

std::shared_ptr<IMediaPipelineFactory> IMediaPipelineFactory::createFactory()
{
	return nullptr;
}

std::shared_ptr<IClientLogControlFactory> IClientLogControlFactory::createFactory()
{
	return nullptr;
}

} // namespace firebolt::rialto
