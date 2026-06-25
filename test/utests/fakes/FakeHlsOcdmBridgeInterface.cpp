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

#include "HlsOcdmBridgeInterface.h"

/**
 * @class FakeHlsOcdmBridge
 * @brief OCDM bridge to handle DRM key
 */

class AampFakeHlsOcdmBridge : public HlsDrmBase
{
public:
	AampFakeHlsOcdmBridge(DrmSession * DrmSession){}

	virtual ~AampFakeHlsOcdmBridge(){}

	AampFakeHlsOcdmBridge(const AampFakeHlsOcdmBridge&) = delete;

	AampFakeHlsOcdmBridge& operator=(const AampFakeHlsOcdmBridge&) = delete;

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

HlsDrmBase* HlsOcdmBridgeInterface::GetBridge(DrmSession * playerDrmSession)
{
   
    return new AampFakeHlsOcdmBridge(playerDrmSession);

}
