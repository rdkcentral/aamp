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
 * @file FakeOpenCDMProviderFactory.cpp
 * @brief Fake implementation of OpenCDMProviderFactory for unit tests.
 *
 * Replaces the real factory (which pulls in RialtoMediaKeysProvider and
 * therefore the Rialto headers/library) with a test-only version that:
 *  - Always delegates to `m_creator` if one has been set via setCreator().
 *  - Otherwise returns nullptr (tests that need a live provider should
 *    inject one via setCreator()).
 *
 * This avoids any dependency on Rialto headers or libraries in test builds
 * that do not exercise the Rialto Direct path.
 */

#include "OpenCDMProviderFactory.h"

OpenCDMProviderFactory& OpenCDMProviderFactory::instance()
{
	static OpenCDMProviderFactory s_instance;
	return s_instance;
}

std::unique_ptr<IOpenCDM> OpenCDMProviderFactory::create(const std::string& keySystem)
{
	if (m_creator)
	{
		return m_creator(keySystem);
	}
	return nullptr;
}

void OpenCDMProviderFactory::setCreator(
	std::function<std::unique_ptr<IOpenCDM>(const std::string&)> creator)
{
	m_creator = std::move(creator);
}
