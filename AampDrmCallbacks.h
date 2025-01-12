/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#ifndef AAMPDRMCALLBACKS_H
#define AAMPDRMCALLBACKS_H

/**
 * @file AampDrmCallbacks.h
 * @brief Call back handler for Aamp
 */

#include <string>

/**
 * @class AampDrmCallbacks
 * @brief DRM callback interface
 */
class AampDrmCallbacks
{
public:
	virtual void individualization(const std::string& payload) = 0;
	virtual void LicenseRenewal(std::shared_ptr<AampDrmHelper> drmHelper, void* userData) = 0;
	virtual int GetPlatformType() = 0;
	virtual ~AampDrmCallbacks() {};
};



#endif /* AAMPDRMCALLBACKS_H */

