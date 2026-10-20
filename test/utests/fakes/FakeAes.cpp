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

#include "Aes.h"
#include "MockAes.h"

std::shared_ptr<MockAesDec> g_mockAesDec{};
std::shared_ptr<AesDec> AesDec::mInstance = nullptr;

std::shared_ptr<AesDec> AesDec::GetInstance()
{
	if (!mInstance)
	{
		mInstance = std::make_shared<AesDec>();
	}
	return mInstance;
}

DrmReturn AesDec::SetMetaData(void* metadata, int trackType)
{
	DrmReturn result = eDRM_SUCCESS;
	if (g_mockAesDec)
	{
		result = g_mockAesDec->SetMetaData(metadata, trackType);
	}
	return result;
}

DRMState AesDec::GetState()
{
	DRMState result = eDRM_INITIALIZED;
	if (g_mockAesDec)
	{
		result = g_mockAesDec->GetState();
	}
	return result;
}

void AesDec::AcquireKey(void* metadata, int trackType)
{
	if (g_mockAesDec)
	{
		g_mockAesDec->AcquireKey(metadata, trackType);
	}
}

DrmReturn AesDec::SetDecryptInfo(const struct DrmInfo* drmInfo, int acquireKeyWaitTime)
{
	DrmReturn result = eDRM_SUCCESS;
	if (g_mockAesDec)
	{
		result = g_mockAesDec->SetDecryptInfo(drmInfo, acquireKeyWaitTime);
	}
	return result;
}

DrmReturn AesDec::SetIV(unsigned char* iv)
{
	DrmReturn result = eDRM_SUCCESS;
	if (g_mockAesDec)
	{
		result = g_mockAesDec->SetIV(iv);
	}
	return result;
}

DrmReturn AesDec::Decrypt(int bucketType, void* encryptedDataPtr, size_t encryptedDataLen, int timeInMs)
{
	DrmReturn result = eDRM_SUCCESS;
	if (g_mockAesDec)
	{
		result = g_mockAesDec->Decrypt(bucketType, encryptedDataPtr, encryptedDataLen, timeInMs);
	}
	return result;
}

void AesDec::Release()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->Release();
	}
}

void AesDec::CancelKeyWait()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->CancelKeyWait();
	}
}

void AesDec::RestoreKeyState()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->RestoreKeyState();
	}
}

void AesDec::acquire_key()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->acquire_key();
	}
}

void AesDec::AcquireKey()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->AcquireKeyInternal();
	}
}

void AesDec::SignalKeyAcquired()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->SignalKeyAcquired();
	}
}

void AesDec::NotifyDRMError(DrmTuneFailure drmFailure)
{
	if (g_mockAesDec)
	{
		g_mockAesDec->NotifyDRMError(drmFailure);
	}
}

void AesDec::SignalDrmError()
{
	if (g_mockAesDec)
	{
		g_mockAesDec->SignalDrmError();
	}
}

void AesDec::WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn& err, std::unique_lock<std::mutex>& lock)
{
	if (g_mockAesDec)
	{
		g_mockAesDec->WaitForKeyAcquireCompleteUnlocked(timeInMs, err, lock);
	}
}

AesDec::AesDec()
	: mCond(),
	  mMutex(),
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	  mOpensslCtx(nullptr),
#else
	  mOpensslCtx(),
#endif
	  mDrmInfo(),
	  m_ptr(nullptr),
	  mDrmState(eDRM_INITIALIZED),
	  mPrevDrmState(eDRM_INITIALIZED),
	  mDrmUrl(),
	  mCurlInstance(-1),
	  mAcquireKeyWaitTime(MAX_LICENSE_ACQ_WAIT_TIME),
	  licenseAcquisitionThreadId()
{
}

AesDec::~AesDec() = default;
