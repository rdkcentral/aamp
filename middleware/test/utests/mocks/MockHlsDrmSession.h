/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#ifndef PLAYER_MOCK_HLS_DRM_SESSION
#define PLAYER_MOCK_HLS_DRM_SESSION

#include <gmock/gmock.h>
#include "HlsDrmBase.h"

class MockHlsDrmSession : public HlsDrmBase {
public:    
    // Implement all pure virtual functions with dummy behavior
    DrmReturn SetMetaData(void* metadata, int trackType) override { return eDRM_SUCCESS; }
    DrmReturn SetDecryptInfo(const struct DrmInfo* drmInfo, int acquireKeyWaitTime) override { return eDRM_SUCCESS; }
    DrmReturn Decrypt(int bucketType, void* encryptedDataPtr, size_t encryptedDataLen, int timeInMs = 3000) override { return eDRM_SUCCESS; }
    void Release() override {}
    void CancelKeyWait() override {}
    void RestoreKeyState() override {}
    void AcquireKey(void* metadata, int trackType) override {}
    DRMState GetState() override { return eDRM_INITIALIZED; }
};
#endif //PLAYER_MOCK_HLS_DRM_SESSION