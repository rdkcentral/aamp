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

#ifndef OpenCDMProviderFactory_h
#define OpenCDMProviderFactory_h

/**
 * @file OpenCDMProviderFactory.h
 * @brief Singleton factory that creates the correct IOpenCDM implementation
 *        based on runtime configuration.
 *
 * Production behaviour:
 *  - eAAMPConfig_useRialtoDirect == false → OpenCDMProvider (real OCDM C library)
 *  - eAAMPConfig_useRialtoDirect == true  → RialtoMediaKeysProvider
 *
 * The factory can be pre-configured with a custom creator function to allow
 * unit tests to inject mock IOpenCDM implementations without touching the
 * config system.
 *
 * TODO: [SHARED-INFRA] This file reads eAAMPConfig_useRialtoDirect which
 *       requires AampConfig.h.  Per direct-rialto.instructions.md, any
 *       dependency from middleware/ on direct-rialto-specific config must be
 *       reviewed as a separate, explicit shared-infrastructure change.
 */

#include "IOpenCDM.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @class OpenCDMProviderFactory
 * @brief Singleton factory for IOpenCDM instances.
 */
class OpenCDMProviderFactory
{
public:
	/**
	 * @brief Access the process-wide singleton instance.
	 */
	static OpenCDMProviderFactory& instance();

	/**
	 * @brief Create the appropriate IOpenCDM for the given key system.
	 *
	 * Checks eAAMPConfig_useRialtoDirect and returns either an
	 * OpenCDMProvider or a RialtoMediaKeysProvider.
	 *
	 * @param keySystem  DRM key system UUID string.
	 * @return Owning unique_ptr to the provider.
	 */
	std::unique_ptr<IOpenCDM> create(const std::string& keySystem);

	/**
	 * @brief Override the creator used by create() — intended for unit tests.
	 *
	 * Pass an empty function to restore the default production behaviour.
	 *
	 * @param creator  Factory function to use instead of the default.
	 */
	void setCreator(std::function<std::unique_ptr<IOpenCDM>(const std::string&)> creator);

	OpenCDMProviderFactory(const OpenCDMProviderFactory&) = delete;
	OpenCDMProviderFactory& operator=(const OpenCDMProviderFactory&) = delete;

private:
	OpenCDMProviderFactory() = default;

	std::function<std::unique_ptr<IOpenCDM>(const std::string&)> m_creator;
};

#endif // OpenCDMProviderFactory_h
