/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file DrmSessionFactory.cpp
 * @brief Source file for DrmSessionFactory
 */

#include "DrmSessionFactory.h"
#if defined(USE_OPENCDM_ADAPTER)
#include "OcdmBasicSessionAdapter.h"
#include "OcdmGstSessionAdapter.h"
#endif
#include "ClearKeyDrmSession.h"

/**
 *  @brief Creates an appropriate DRM session based on the given DrmHelper
 */
DrmSession* DrmSessionFactory::GetDrmSession(DrmHelperPtr drmHelper, DrmCallbacks *drmCallbacks)
{
	const std::string systemId = drmHelper->ocdmSystemId();

#if defined (USE_OPENCDM_ADAPTER)
	if (drmHelper->isClearDecrypt())
	{
#if defined(USE_CLEARKEY)
		if (systemId == CLEAR_KEY_SYSTEM_STRING)
		{
			return new ClearKeySession();
		}
		else
#endif
		{
			return new OCDMBasicSessionAdapter(drmHelper, drmCallbacks);
		}
	}
	else
	{
		return new OCDMGSTSessionAdapter(drmHelper, drmCallbacks);
	}
#else // No form of OCDM support. Attempt to fallback to hardcoded session classes
    if (systemId == CLEAR_KEY_SYSTEM_STRING)
	{
#if defined(USE_CLEARKEY)
		return new ClearKeySession();
#endif // USE_CLEARKEY
	}
#endif // Not USE_OPENCDM_ADAPTER
	return NULL;
}
