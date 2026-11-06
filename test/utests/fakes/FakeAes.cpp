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
	(void)metadata;
	(void)trackType;
	return eDRM_SUCCESS;
}

DRMState AesDec::GetState()
{
	return mDrmState;
}

void AesDec::AcquireKey(void* metadata, int trackType)
{
	(void)metadata;
	(void)trackType;
}

DrmReturn AesDec::SetDecryptInfo(const struct DrmInfo* drmInfo, int acquireKeyWaitTime)
{
	(void)drmInfo;
	mAcquireKeyWaitTime = acquireKeyWaitTime;
	return eDRM_SUCCESS;
}

DrmReturn AesDec::SetIV(unsigned char* iv)
{
	(void)iv;
	return eDRM_SUCCESS;
}

DrmReturn AesDec::Decrypt(int bucketType, void* encryptedDataPtr, size_t encryptedDataLen, int timeInMs)
{
	(void)bucketType;
	(void)encryptedDataPtr;
	(void)encryptedDataLen;
	(void)timeInMs;
	return eDRM_SUCCESS;
}

void AesDec::Release()
{
	mDrmState = eDRM_INITIALIZED;
}

void AesDec::CancelKeyWait()
{
}

void AesDec::RestoreKeyState()
{
}

void AesDec::acquire_key()
{
}

void AesDec::AcquireKey()
{
}

void AesDec::SignalKeyAcquired()
{
	mDrmState = eDRM_KEY_ACQUIRED;
}

void AesDec::NotifyDRMError(DrmTuneFailure drmFailure)
{
	mDrmState = eDRM_KEY_FAILED;
	if (NotifyDrmErrorCb)
	{
		NotifyDrmErrorCb(drmFailure);
	}
}

void AesDec::SignalDrmError()
{
	mDrmState = eDRM_KEY_FAILED;
}

void AesDec::WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn& err, std::unique_lock<std::mutex>& lock)
{
	(void)timeInMs;
	(void)lock;
	err = (mDrmState == eDRM_KEY_ACQUIRED) ? eDRM_SUCCESS : eDRM_ERROR;
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
