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

#ifndef MOCK_IMEDIA_PIPELINE_CAPABILITIES_H
#define MOCK_IMEDIA_PIPELINE_CAPABILITIES_H

#include "IMediaPipelineCapabilities.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @class MockIMediaPipelineCapabilitiesFactory
 * @brief Google Mock for firebolt::rialto::IMediaPipelineCapabilitiesFactory.
 */
class MockIMediaPipelineCapabilitiesFactory
	: public firebolt::rialto::IMediaPipelineCapabilitiesFactory
{
public:
	MOCK_METHOD(
		std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities>,
		createMediaPipelineCapabilities,
		(),
		(const, override));
};

/// Global used by the fake IMediaPipelineCapabilitiesFactory::createFactory().
/// Tests set this in SetUp() and reset it in TearDown().
extern std::shared_ptr<MockIMediaPipelineCapabilitiesFactory>
	g_mockCapabilitiesFactory;

/**
 * @class MockIMediaPipelineCapabilities
 * @brief Google Mock for firebolt::rialto::IMediaPipelineCapabilities.
 */
class MockIMediaPipelineCapabilities
	: public firebolt::rialto::IMediaPipelineCapabilities
{
public:
	// clang-format off
	MOCK_METHOD(std::vector<std::string>, getSupportedMimeTypes,
		(firebolt::rialto::MediaSourceType sourceType), (override));

	MOCK_METHOD(bool, isMimeTypeSupported,
		(const std::string &mimeType), (override));

	MOCK_METHOD(std::vector<std::string>, getSupportedProperties,
		(firebolt::rialto::MediaSourceType mediaType,
		 const std::vector<std::string> &propertyNames),
		(override));

	MOCK_METHOD(bool, isVideoMaster,
		(bool &isVideoMaster), (override));
	// clang-format on
};

#endif // MOCK_IMEDIA_PIPELINE_CAPABILITIES_H
