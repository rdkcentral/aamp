/*
 *
 *
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
 * @file Aes.cpp
 * @brief HLS AES drm decryptor
 */


#include "Aes.h"
#include "cstring"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <mutex>
#include "PlayerLogManager.h"
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
void AesDec::NotifyDRMError(DrmTuneFailure drmFailure)
{
        this->NotifyDrmErrorCb((int)drmFailure);
	SignalDrmError();
	MW_LOG_ERR("AesDec::NotifyDRMError: drmState:%d", mDrmState );
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
	MW_LOG_WARN("AesDRMListener drmState:%d moving to KeyAcquired", mDrmState);
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mDrmState = eDRM_KEY_ACQUIRED;
		mCond.notify_all();
	}
	this->ProfileUpdateDrmDecrypt(1, DRM_PROFILE_BUCKET_LA_TOTAL); 
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
	DrmTuneFailure failureReasonIn = MW_UNTRACKED_DRM_ERROR;

        if (drm_pthread_setname(pthread_self(), "aesDRM"))
	{
		MW_LOG_ERR("pthread_setname_np failed");
	}
        
       	ResolveURL(keyURI, mDrmInfo.manifestURL, mDrmInfo.keyURI.c_str(), mDrmInfo.bPropagateUriParams);
	MW_LOG_WARN("Key acquisition start uri = %s",  keyURI.c_str());
	bool fetched =0;
	int failureReason = (int)failureReasonIn;
	 
         char* ptr = NULL;
	this->GetAccessKeyCb(keyURI,  tempEffectiveUrl, http_error, downloadTime, mCurlInstance, keyAcquisitionStatus, failureReason, &ptr);
        m_ptr = ( char*)ptr;
	if(keyAcquisitionStatus)
	{
		SignalKeyAcquired();
	}
	else
	{
		NotifyDRMError((DrmTuneFailure)failureReason);
	}
}

/**
 * @brief Set DRM meta-data. Stub implementation
 *
 */
DrmReturn AesDec::SetMetaData(void* metadata,int trackType)
{
	return eDRM_SUCCESS;
}

/**
 * @brief AcquireKey Function to acquire key . Stub implementation
 */
void AesDec::AcquireKey(void *metadata,int trackType)
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
DrmReturn AesDec::SetDecryptInfo(const struct DrmInfo *drmInfo, int acquireKeyWaitTime)
{
	DrmReturn err = eDRM_ERROR;
        std::unique_lock<std::mutex> lock(mMutex);

	mAcquireKeyWaitTime = acquireKeyWaitTime;
	if (mDrmState == eDRM_ACQUIRING_KEY)
	{
		MW_LOG_WARN("AesDec:: acquiring key in progress");
		WaitForKeyAcquireCompleteUnlocked(mAcquireKeyWaitTime, err, lock );
	}
	mDrmInfo = *drmInfo;

	if (!mDrmUrl.empty())
	{
		if ((eDRM_KEY_ACQUIRED == mDrmState) && (drmInfo->keyURI == mDrmUrl))
		{
			MW_LOG_TRACE("AesDec: same url:%s - not acquiring key", mDrmUrl.c_str());
			return eDRM_SUCCESS;
		}
	}
	mDrmUrl = drmInfo->keyURI;
	mDrmState = eDRM_ACQUIRING_KEY;
	mPrevDrmState = eDRM_INITIALIZED;
	this->GetCurlInitCb(mCurlInstance);

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
//TODO		MW_LOG_INFO("Thread created for acquire_key [%zx]", GetPrintableThreadID(licenseAcquisitionThreadId));
	}
	catch(const std::exception& e)
	{
		MW_LOG_ERR("AesDec:: thread create failed for acquire_key : %s", e.what());
		mDrmState = eDRM_KEY_FAILED;
		licenseAcquisitionThreadStarted = false;
	}
	MW_LOG_INFO("AesDec: drmState:%d ", mDrmState);
	return err;
}

/**
 * @brief Wait for key acquisition completion
 */
void AesDec::WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn &err, std::unique_lock<std::mutex>& lock )
{
	MW_LOG_INFO( "waiting for key acquisition to complete,wait time:%d",timeInMs );
	if( std::cv_status::timeout == mCond.wait_for(lock, std::chrono::milliseconds(timeInMs)) ) // block until drm ready
	{
		MW_LOG_WARN("AesDec:: wait for key acquisition timed out");
		err = eDRM_KEY_ACQUISITION_TIMEOUT;
	}
}

/**
 * @brief Decrypts an encrypted buffer
 */
DrmReturn AesDec::Decrypt( int bucketTypeIn, void *encryptedDataPtr, size_t encryptedDataLen,int timeInMs)
{
	DrmProfilerBucketType bucketType = (DrmProfilerBucketType)bucketTypeIn;
	DrmReturn err = eDRM_ERROR;

	std::unique_lock<std::mutex> lock(mMutex);
	if (mDrmState == eDRM_ACQUIRING_KEY)
	{
		WaitForKeyAcquireCompleteUnlocked(timeInMs, err, lock);
	}
	if (mDrmState == eDRM_KEY_ACQUIRED)
	{
		MW_LOG_INFO("AesDec: Starting decrypt");
		unsigned char *decryptedDataBuf = (unsigned char *)malloc(encryptedDataLen);
		int decryptedDataLen = 0;
		if (decryptedDataBuf)
		{
			int decLen = (int)encryptedDataLen;
			memset(decryptedDataBuf, 0, encryptedDataLen);
	               
		       this->ProfileUpdateDrmDecrypt(0, bucketType);
		       if(!EVP_DecryptInit_ex(OPEN_SSL_CONTEXT, EVP_aes_128_cbc(), NULL, (unsigned char*)m_ptr, mDrmInfo.iv))
			{
				MW_LOG_ERR( "AesDec::EVP_DecryptInit_ex failed mDrmState = %d",(int)mDrmState);
			}
			else
			{
				if (!EVP_DecryptUpdate(OPEN_SSL_CONTEXT, decryptedDataBuf, &decLen, (const unsigned char*)encryptedDataPtr, (int)encryptedDataLen))
				{
					MW_LOG_ERR("AesDec::EVP_DecryptUpdate failed mDrmState = %d",(int) mDrmState);
				}
				else
				{
					decryptedDataLen = decLen;
					decLen = 0;
					MW_LOG_INFO("AesDec: EVP_DecryptUpdate success decryptedDataLen = %d encryptedDataLen %d", (int) decryptedDataLen, (int)encryptedDataLen);
					if (!EVP_DecryptFinal_ex(OPEN_SSL_CONTEXT, decryptedDataBuf + decryptedDataLen, &decLen))
					{
						MW_LOG_ERR("AesDec::EVP_DecryptFinal_ex failed mDrmState = %d", 
						        (int) mDrmState);
					}
					else
					{
						decryptedDataLen += decLen;
						MW_LOG_INFO("AesDec: decrypt success");
						err = eDRM_SUCCESS;
					}
				}
			}
                        this->ProfileUpdateDrmDecrypt(1, bucketType);
			memcpy(encryptedDataPtr, decryptedDataBuf, encryptedDataLen);
			free(decryptedDataBuf);
			(void)decryptedDataLen; // Avoid a warning as this is only used in a log.
		}
	}
	else
	{
		MW_LOG_ERR( "AesDec::key acquisition failure! mDrmState = %d",(int)mDrmState);
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
		this->TerminateCurlInstanceCb(mCurlInstance);

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
AesDec::AesDec() :  mDrmState(eDRM_INITIALIZED),
		mPrevDrmState(eDRM_INITIALIZED), mDrmUrl(""),
		mCond(), mMutex(), mOpensslCtx(),
         	mDrmInfo(), mCurlInstance(-1),
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
