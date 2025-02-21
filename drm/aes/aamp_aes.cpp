/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file aamp_aes.cpp
 * @brief HLS AES drm decryptor
 */


#include "aamp_aes.h"
#include "AampUtils.h"

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <mutex>


#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define OPEN_SSL_CONTEXT mOpensslCtx
#else
#define OPEN_SSL_CONTEXT &mOpensslCtx
#endif
#define AES_128_KEY_LEN_BYTES 16

static std::mutex instanceLock;

/**
 * @brief key acquisition thread
 * @retval NULL
 */
void AesDec::acquire_key()
{
	AcquireKey();
	return;
}

/**
 * @brief Notify drm error
 */
void AesDec::NotifyDRMError(AAMPTuneFailure drmFailure)
{
	//If downloads are disabled, don't send error event upstream
	if (mpAamp->DownloadsAreEnabled())
	{
		mpAamp->DisableDownloads();
		if(AAMP_TUNE_UNTRACKED_DRM_ERROR == drmFailure)
		{
			mpAamp->SendErrorEvent(drmFailure, "AAMP: DRM Failure" );
		}
		else
		{
			mpAamp->SendErrorEvent(drmFailure);
		}
	}
	SignalDrmError();
	AAMPLOG_ERR("AesDec::NotifyDRMError: drmState:%d", mDrmState );
}


/**
 * @brief Signal drm error
 */
void AesDec::SignalDrmError()
{
	std::unique_lock<std::mutex> lock(mMutex);
	mDrmState = eDRM_KEY_FAILED;
	mCond.notify_all();
}

/**
 * @brief Signal key acquired event
 */
void AesDec::SignalKeyAcquired()
{
	AAMPLOG_WARN("aamp:AesDRMListener drmState:%d moving to KeyAcquired", mDrmState);
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mDrmState = eDRM_KEY_ACQUIRED;
		mCond.notify_all();
	}
	mpAamp->LogDrmInitComplete();
}

/**
 * @brief Acquire drm key from URI 
 */
void AesDec::AcquireKey()
{
	std::string tempEffectiveUrl;
	std::string keyURI;
	int http_error = 0;  //CID:88814 - Initialization
	double downloadTime = 0.0;
	bool keyAcquisitionStatus = false;
	AAMPTuneFailure failureReason = AAMP_TUNE_UNTRACKED_DRM_ERROR;

	if (aamp_pthread_setname(pthread_self(), "aampAesDRM"))
	{
		AAMPLOG_ERR("pthread_setname_np failed");
	}
	aamp_ResolveURL(keyURI, mDrmInfo.manifestURL, mDrmInfo.keyURI.c_str(), mDrmInfo.bPropagateUriParams);
	AAMPLOG_WARN("Key acquisition start uri = %s",  keyURI.c_str());
	bool fetched = mpAamp->GetFile(keyURI, eMEDIATYPE_LICENCE, &mAesKeyBuf, tempEffectiveUrl, &http_error, &downloadTime, NULL, mCurlInstance, true);
	if (fetched)
	{
		if (AES_128_KEY_LEN_BYTES == mAesKeyBuf.GetLen() )
		{
			AAMPLOG_WARN("Key fetch success len = %d",  (int)mAesKeyBuf.GetLen() );
			keyAcquisitionStatus = true;
		}
		else
		{
			AAMPLOG_ERR("Error Key fetch - size %d",  (int)mAesKeyBuf.GetLen() );
			failureReason = AAMP_TUNE_INVALID_DRM_KEY;
		}
	}
	else
	{
		AAMPLOG_ERR("Key fetch failed");
		if (http_error == CURLE_OPERATION_TIMEDOUT)
		{
			failureReason = AAMP_TUNE_LICENCE_TIMEOUT;
		}
		else
		{
			failureReason = AAMP_TUNE_LICENCE_REQUEST_FAILED;
		}
	}

	if(keyAcquisitionStatus)
	{
		SignalKeyAcquired();
	}
	else
	{
		mAesKeyBuf.Free(); //To cleanup previous successful key if any
		NotifyDRMError(failureReason);
	}
}

/**
 * @brief Set DRM meta-data. Stub implementation
 *
 */
DrmReturn AesDec::SetMetaData( PrivateInstanceAAMP *aamp, void* metadata,int trackType)
{
	return eDRM_SUCCESS;
}

/**
 * @brief AcquireKey Function to acquire key . Stub implementation
 */
void AesDec::AcquireKey( class PrivateInstanceAAMP *aamp, void *metadata,int trackType)
{

}

/**
 * @brief GetState Function to get current DRM State
 *
 */
DRMState AesDec::GetState()
{
	return mDrmState;
}

/**
 * @brief Set information required for decryption
 *
 */
DrmReturn AesDec::SetDecryptInfo( PrivateInstanceAAMP *aamp, const struct DrmInfo *drmInfo)
{
	DrmReturn err = eDRM_ERROR;
	std::unique_lock<std::mutex> lock(mMutex);
	mpAamp = aamp;

	if (NULL!= mpAamp)
	{
		mAcquireKeyWaitTime = mpAamp->mConfig->GetConfigValue(eAAMPConfig_LicenseKeyAcquireWaitTime);
	}
	if (mDrmState == eDRM_ACQUIRING_KEY)
	{
		AAMPLOG_WARN("AesDec:: acquiring key in progress");
		WaitForKeyAcquireCompleteUnlocked(mAcquireKeyWaitTime, err, lock );
	}
	mDrmInfo = *drmInfo;

	if (!mDrmUrl.empty())
	{
		if ((eDRM_KEY_ACQUIRED == mDrmState) && (drmInfo->keyURI == mDrmUrl))
		{
			AAMPLOG_TRACE("AesDec: same url:%s - not acquiring key", mDrmUrl.c_str());
			return eDRM_SUCCESS;
		}
	}
	mDrmUrl = drmInfo->keyURI;
	mDrmState = eDRM_ACQUIRING_KEY;
	mPrevDrmState = eDRM_INITIALIZED;
	if ((-1 == mCurlInstance) && (aamp != NULL))
	{
		mCurlInstance = eCURLINSTANCE_AES;
		aamp->CurlInit((AampCurlInstance)mCurlInstance,1,aamp->GetLicenseReqProxy());
	}

	if (licenseAcquisitionThreadStarted)
	{
		licenseAcquisitionThreadId.join();
		licenseAcquisitionThreadStarted = false;
	}

	try
	{
		licenseAcquisitionThreadId = std::thread(&AesDec::acquire_key, this);
		err = eDRM_SUCCESS;
		licenseAcquisitionThreadStarted = true;
		AAMPLOG_INFO("Thread created for acquire_key [%zx]", GetPrintableThreadID(licenseAcquisitionThreadId));
	}
	catch(const std::exception& e)
	{
		AAMPLOG_ERR("AesDec:: thread create failed for acquire_key : %s", e.what());
		mDrmState = eDRM_KEY_FAILED;
		licenseAcquisitionThreadStarted = false;
	}
	AAMPLOG_INFO("AesDec: drmState:%d ", mDrmState);
	return err;
}

/**
 * @brief Wait for key acquisition completion
 */
void AesDec::WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn &err, std::unique_lock<std::mutex>& lock )
{
	AAMPLOG_INFO( "aamp:waiting for key acquisition to complete,wait time:%d",timeInMs );
	if( std::cv_status::timeout == mCond.wait_for(lock, std::chrono::milliseconds(timeInMs)) ) // block until drm ready
	{
		AAMPLOG_WARN("AesDec:: wait for key acquisition timed out");
		err = eDRM_KEY_ACQUISITION_TIMEOUT;
	}
}

/**
 * @brief Decrypts an encrypted buffer
 */
DrmReturn AesDec::Decrypt( ProfilerBucketType bucketType, void *encryptedDataPtr, size_t encryptedDataLen,int timeInMs)
{
	DrmReturn err = eDRM_ERROR;

	std::unique_lock<std::mutex> lock(mMutex);
	if (mDrmState == eDRM_ACQUIRING_KEY)
	{
		WaitForKeyAcquireCompleteUnlocked(timeInMs, err, lock);
	}
	if (mDrmState == eDRM_KEY_ACQUIRED)
	{
		AAMPLOG_INFO("AesDec: Starting decrypt");
		unsigned char *decryptedDataBuf = (unsigned char *)malloc(encryptedDataLen);
		int decryptedDataLen = 0;
		if (decryptedDataBuf)
		{
			int decLen = (int)encryptedDataLen;
			memset(decryptedDataBuf, 0, encryptedDataLen);
			mpAamp->LogDrmDecryptBegin(bucketType);
			if(!EVP_DecryptInit_ex(OPEN_SSL_CONTEXT, EVP_aes_128_cbc(), NULL, (unsigned char*)mAesKeyBuf.GetPtr(), mDrmInfo.iv))
			{
				AAMPLOG_ERR( "AesDec::EVP_DecryptInit_ex failed mDrmState = %d",(int)mDrmState);
			}
			else
			{
				if (!EVP_DecryptUpdate(OPEN_SSL_CONTEXT, decryptedDataBuf, &decLen, (const unsigned char*) encryptedDataPtr, (int)encryptedDataLen))
				{
					AAMPLOG_ERR("AesDec::EVP_DecryptUpdate failed mDrmState = %d",(int) mDrmState);
				}
				else
				{
					decryptedDataLen = decLen;
					decLen = 0;
					AAMPLOG_INFO("AesDec: EVP_DecryptUpdate success decryptedDataLen = %d encryptedDataLen %d", (int) decryptedDataLen, (int)encryptedDataLen);
					if (!EVP_DecryptFinal_ex(OPEN_SSL_CONTEXT, decryptedDataBuf + decryptedDataLen, &decLen))
					{
						AAMPLOG_ERR("AesDec::EVP_DecryptFinal_ex failed mDrmState = %d", 
						        (int) mDrmState);
					}
					else
					{
						decryptedDataLen += decLen;
						AAMPLOG_INFO("AesDec: decrypt success");
						err = eDRM_SUCCESS;
					}
				}
			}
			mpAamp->LogDrmDecryptEnd(bucketType);

			memcpy(encryptedDataPtr, decryptedDataBuf, encryptedDataLen);
			free(decryptedDataBuf);
			(void)decryptedDataLen; // Avoid a warning as this is only used in a log.
		}
	}
	else
	{
		AAMPLOG_ERR( "AesDec::key acquisition failure! mDrmState = %d",(int)mDrmState);
	}
	return err;
}


/**
 * @brief Release drm session
 */
void AesDec::Release()
{
	DrmReturn err = eDRM_ERROR;
	std::unique_lock<std::mutex> lock(mMutex);
	//We wait for license acquisition to complete. Once license acquisition is complete
	//the appropriate state will be set to mDrmState and hence RestoreKeyState will be a no-op.
	if ( ( mDrmState == eDRM_ACQUIRING_KEY || mPrevDrmState == eDRM_ACQUIRING_KEY ) && mDrmState != eDRM_KEY_FAILED )
	{
		WaitForKeyAcquireCompleteUnlocked(mAcquireKeyWaitTime, err, lock );
	}
	if (licenseAcquisitionThreadStarted)
	{
		licenseAcquisitionThreadId.join();
		licenseAcquisitionThreadStarted = false;
	}
	mCond.notify_all();
	if (-1 != mCurlInstance)
	{
		if (mpAamp)
		{
			mpAamp->SyncBegin();
			mpAamp->CurlTerm((AampCurlInstance)mCurlInstance);
			mpAamp->SyncEnd();
		}
		mCurlInstance = -1;
	}
}

/**
 * @brief Cancel timed_wait operation drm_Decrypt
 *
 */
void AesDec::CancelKeyWait()
{
	std::lock_guard<std::mutex> guard(mMutex);
	//save the current state in case required to restore later.
	if (mDrmState != eDRM_KEY_FLUSH)
	{
		mPrevDrmState = mDrmState;
	}
	//required for demuxed assets where the other track might be waiting on mMutex lock.
	mDrmState = eDRM_KEY_FLUSH;
	mCond.notify_all();
}

/**
 * @brief Restore key state post cleanup of
 * audio/video TrackState in case DRM data is persisted
 */
void AesDec::RestoreKeyState()
{
	std::lock_guard<std::mutex> guard(mMutex);
	//In case somebody overwritten mDrmState before restore operation, keep that state
	if (mDrmState == eDRM_KEY_FLUSH)
	{
		mDrmState = mPrevDrmState;
	}
}

std::shared_ptr<AesDec> AesDec::mInstance = nullptr;

/**
 * @brief Get singleton instance
 */
std::shared_ptr<AesDec> AesDec::GetInstance()
{
	std::lock_guard<std::mutex> guard(instanceLock);
	if (nullptr == mInstance)
	{
		mInstance = std::make_shared<AesDec>();
	}
	return mInstance;
}

/**
 * @brief AesDec Constructor
 * 
 */
AesDec::AesDec() : mpAamp(nullptr), mDrmState(eDRM_INITIALIZED),
		mPrevDrmState(eDRM_INITIALIZED), mDrmUrl(""),
		mCond(), mMutex(), mOpensslCtx(),
		mDrmInfo(), mAesKeyBuf("aesKeyBuf"), mCurlInstance(-1),
		licenseAcquisitionThreadId(),
		licenseAcquisitionThreadStarted(false),
		mAcquireKeyWaitTime(MAX_LICENSE_ACQ_WAIT_TIME)
{
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	OPEN_SSL_CONTEXT = EVP_CIPHER_CTX_new();
#else
	EVP_CIPHER_CTX_init(OPEN_SSL_CONTEXT);
#endif
}


/**
 * @brief AesDec Destructor
 */
AesDec::~AesDec()
{
	CancelKeyWait();
	Release();
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX_free(OPEN_SSL_CONTEXT);
#else
	EVP_CIPHER_CTX_cleanup(OPEN_SSL_CONTEXT);
#endif
}
