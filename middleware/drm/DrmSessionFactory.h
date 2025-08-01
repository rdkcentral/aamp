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
* @file DrmSessionFactory.h
* @brief Header file for DrmSessionFactory
*/

#ifndef DrmSessionFactory_h
#define DrmSessionFactory_h

#include "DrmSession.h"
#include "DrmHelper.h"
#include "DrmCallbacks.h"
/**
 * @class DrmSessionFactory
 * @brief Factory class to create DRM sessions based on
 *        requested system ID
 */
class DrmSessionFactory
{
public:
	
	/**
	 *  @fn  	GetDrmSession
	 *
	 *  @param[in]	drmHelper - DrmHelper instance
	 *  @return		Pointer to DrmSession.
	 */
	static DrmSession* GetDrmSession(DrmHelperPtr drmHelper, DrmCallbacks *drmCallbacks);
};
#endif
