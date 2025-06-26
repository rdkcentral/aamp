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

#ifndef _HLS_OCDM_BRIDGE_INTERFACE_H_
#define _HLS_OCDM_BRIDGE_INTERFACE_H_

/**
 * @file HlsOcdmBridgeInterface.h
 * @brief Handles OCDM bridge interface to validate DRM License
 */

#include "HlsDrmBase.h"

/**
 * @class FakeHlsOcdmBridge
 * @brief Fake OCDM bridge to handle DRM key
 */

class FakeHlsOcdmBridge : public HlsDrmBase
{
public:
	FakeHlsOcdmBridge(DrmSession * DrmSession){}

	virtual ~FakeHlsOcdmBridge(){}

	FakeHlsOcdmBridge(const FakeHlsOcdmBridge&) = delete;

	FakeHlsOcdmBridge& operator=(const FakeHlsOcdmBridge&) = delete;

	/*HlsDrmBase Methods*/

	virtual DrmReturn SetMetaData(void* metadata,int trackType) override {return DrmReturn::eDRM_ERROR;}

	virtual DrmReturn SetDecryptInfo(const struct DrmInfo *drmInfo, int acquireKeyWaitTime) override {return DrmReturn::eDRM_ERROR;}

	virtual DrmReturn Decrypt(int bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs = DECRYPT_WAIT_TIME_MS) override {return DrmReturn::eDRM_ERROR;}

	virtual void Release() override {}

	virtual void CancelKeyWait() override {}

	virtual void RestoreKeyState() override {}

	virtual void AcquireKey(void *metadata,int trackType) override {}

	virtual DRMState GetState() override {return DRMState::eDRM_KEY_FAILED;}

};

class HlsOcdmBridgeInterface
{

public:

    static HlsDrmBase* GetBridge(DrmSession * drmSession);

};



#endif // _HLS_OCDM_BRIDGE_INTERFACE_H_
