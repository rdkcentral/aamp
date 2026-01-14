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

/**
 * @file OcdmBasicSessionAdapter.cpp
 * @brief Handles operations on OCDM session
 */
#include "opencdmsessionadapter.h"
#include "DrmHelper.h"

/**
 * @class OCDMBasicSessionAdapter
 * @brief OCDM session Adapter
 */
class OCDMBasicSessionAdapter : public OCDMSessionAdapter
{
public:
	OCDMBasicSessionAdapter(DrmHelperPtr drmHelper, DrmCallbacks *drmCallbacks)
	: OCDMSessionAdapter(drmHelper, drmCallbacks)
	{};
	~OCDMBasicSessionAdapter() {};

	int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData);
};
