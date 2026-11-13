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

#ifndef AAMP_MOCK_AES_H
#define AAMP_MOCK_AES_H

#include <gmock/gmock.h>
#include <memory>
#include <mutex>
#include "Aes.h"

class MockAesDec
{
public:
	MOCK_METHOD(DrmReturn, SetMetaData, (void* metadata, int trackType));
	MOCK_METHOD(DRMState, GetState, ());
	MOCK_METHOD(void, AcquireKey, (void* metadata, int trackType));
	MOCK_METHOD(DrmReturn, SetDecryptInfo, (const struct DrmInfo* drmInfo, int acquireKeyWaitTime));
	MOCK_METHOD(DrmReturn, SetIV, (unsigned char* iv));
	MOCK_METHOD(DrmReturn, Decrypt, (int bucketType, void* encryptedDataPtr, size_t encryptedDataLen, int timeInMs));
	MOCK_METHOD(void, Release, ());
	MOCK_METHOD(void, CancelKeyWait, ());
	MOCK_METHOD(void, RestoreKeyState, ());
	MOCK_METHOD(void, acquire_key, ());
	MOCK_METHOD(void, AcquireKeyInternal, ());
	MOCK_METHOD(void, SignalKeyAcquired, ());
	MOCK_METHOD(void, NotifyDRMError, (DrmTuneFailure drmFailure));
	MOCK_METHOD(void, SignalDrmError, ());
	MOCK_METHOD(void, WaitForKeyAcquireCompleteUnlocked, (int timeInMs, DrmReturn& err, std::unique_lock<std::mutex>& lock));
};

extern std::shared_ptr<MockAesDec> g_mockAesDec;

#endif /* AAMP_MOCK_AES_H */
