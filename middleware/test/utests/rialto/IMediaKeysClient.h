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
 * @file IMediaKeysClient.h
 * @brief Stub Rialto IMediaKeysClient interface for unit tests.
 */

#ifndef FIREBOLT_RIALTO_I_MEDIA_KEYS_CLIENT_H_
#define FIREBOLT_RIALTO_I_MEDIA_KEYS_CLIENT_H_

#include "MediaCommon.h"
#include <string>
#include <vector>

namespace firebolt::rialto
{

class IMediaKeysClient
{
public:
	virtual ~IMediaKeysClient() = default;

	virtual void onLicenseRequest(int32_t keySessionId,
	                              const std::vector<unsigned char>& licenseRequestMessage,
	                              const std::string& url) = 0;

	virtual void onLicenseRenewal(int32_t keySessionId,
	                              const std::vector<unsigned char>& licenseRenewalMessage) = 0;

	virtual void onKeyStatusesChanged(int32_t keySessionId,
	                                  const KeyStatusVector& keyStatuses) = 0;
};

} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_I_MEDIA_KEYS_CLIENT_H_
