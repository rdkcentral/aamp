/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef _HLS_OCDM_BRIDGE_H_
#define _HLS_OCDM_BRIDGE_H_

/**
 * @file HlsOcdmBridge.h
 * @brief Handles OCDM bridge to validate DRM License
 */

#include "HlsDrmBase.h"

/**
 * @class HlsOcdmBridge
 * @brief OCDM bridge to handle DRM key
 */

class HlsOcdmBridge : public HlsDrmBase
{
	DRMState m_drmState;

	const DrmInfo* m_drmInfo;
	DrmSession* m_drmSession;
	std::mutex m_Mutex;
public:
	HlsOcdmBridge(DrmSession * DrmSession);

	~HlsOcdmBridge();

	HlsOcdmBridge(const HlsOcdmBridge&) = delete;

	HlsOcdmBridge& operator=(const HlsOcdmBridge&) = delete;

	/*HlsDrmBase Methods*/

	virtual DrmReturn SetMetaData(void* metadata,int trackType) override {return eDRM_SUCCESS;};

	virtual DrmReturn SetDecryptInfo(const struct DrmInfo *drmInfo, int acquireKeyWaitTime) override;

	virtual DrmReturn Decrypt(int bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs = DECRYPT_WAIT_TIME_MS) override;

	virtual void Release() override;

	virtual void CancelKeyWait() override;

	virtual void RestoreKeyState() override {};

	virtual void AcquireKey(void *metadata,int trackType) override {};

	virtual DRMState GetState() override {return m_drmState;}

};

#endif // _HLS_OCDM_BRIDGE_H_
