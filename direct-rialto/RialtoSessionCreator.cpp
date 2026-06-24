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
 * @file RialtoSessionCreator.cpp
 * @brief Implementation of makeRialtoSessionCreator().
 */

#include "RialtoSessionCreator.h"
#include "RialtoMediaKeySystem.h"
#include "RialtoMediaKeySessionAdapter.h"
#include "PlayerLogManager.h"

DrmSessionCreator makeRialtoSessionCreator()
{
	return [](DrmHelperPtr drmHelper, DrmCallbacks* callbacks) -> std::unique_ptr<IDrmSession>
	{
		const std::string systemId = drmHelper->ocdmSystemId();
		MW_LOG_INFO("makeRialtoSessionCreator: building RialtoMediaKeySystem for %s",
		            systemId.c_str());
		auto system = std::make_unique<RialtoMediaKeySystem>(systemId);
		if (!system->isValid())
		{
			MW_LOG_ERR("makeRialtoSessionCreator: RialtoMediaKeySystem creation failed for %s",
			           systemId.c_str());
			return nullptr;
		}
		return std::make_unique<RialtoMediaKeySessionAdapter>(
			drmHelper, std::move(system), callbacks);
	};
}
