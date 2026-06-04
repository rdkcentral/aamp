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
 * @file OpenCDMProviderFactory.cpp
 * @brief Singleton factory implementation.
 */

#include "OpenCDMProviderFactory.h"
#include "OpenCDMProvider.h"
#include "RialtoMediaKeysProvider.h"
#include "AampConfig.h"
#include "PlayerLogManager.h"

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

	// TODO: [SHARED-INFRA] gpGlobalConfig is AAMP's global config pointer.
	//       A cleaner approach would be to pass the config flag as a parameter,
	//       but this matches the existing pattern used throughout AAMP middleware.
	if (gpGlobalConfig && gpGlobalConfig->IsConfigSet(eAAMPConfig_useRialtoDirect))
	{
		MW_LOG_INFO("OpenCDMProviderFactory: creating RialtoMediaKeysProvider for %s", keySystem.c_str());
		return std::make_unique<RialtoMediaKeysProvider>(keySystem);
	}

	MW_LOG_INFO("OpenCDMProviderFactory: creating OpenCDMProvider for %s", keySystem.c_str());
	return std::make_unique<OpenCDMProvider>(keySystem);
}

void OpenCDMProviderFactory::setCreator(
	std::function<std::unique_ptr<IOpenCDM>(const std::string&)> creator)
{
	m_creator = std::move(creator);
}
