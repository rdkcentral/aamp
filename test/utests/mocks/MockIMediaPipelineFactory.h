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

#ifndef MOCK_IMEDIA_PIPELINE_FACTORY_H
#define MOCK_IMEDIA_PIPELINE_FACTORY_H

#include "IMediaPipeline.h"
#include <gmock/gmock.h>
#include <memory>

/**
 * @class MockIMediaPipelineFactory
 * @brief Google Mock for firebolt::rialto::IMediaPipelineFactory.
 *
 * Inject this into AampRialtoPlayer::Configure() via the factory seam
 * parameter to control pipeline creation in L1 tests.
 */
class MockIMediaPipelineFactory : public firebolt::rialto::IMediaPipelineFactory
{
public:
	// clang-format off
	MOCK_METHOD(std::unique_ptr<firebolt::rialto::IMediaPipeline>, createMediaPipeline,
		(std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
		 const firebolt::rialto::VideoRequirements &videoRequirements),
		(const, override));
	// clang-format on
};

#endif // MOCK_IMEDIA_PIPELINE_FACTORY_H
