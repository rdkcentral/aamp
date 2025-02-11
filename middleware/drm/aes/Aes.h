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

#ifndef _AES_H_
#define _AES_H_

/**
 * @file Aes.h
 * @brief HLS AES drm decryptor
 */

#include <stddef.h>
#include "HlsDrmBase.h"
#include <openssl/evp.h>
#include <memory>
#include <condition_variable>
#include <functional>
#include <DrmInfo.h>
#include <thread>
#include "middleware/PlayerUtils.h"

#define MAX_LICENSE_ACQ_WAIT_TIME 12000                         /**< 12 secs Increase from 10 to 12 sec */

#ifdef __APPLE__
#define drm_pthread_setname(tid,name) pthread_setname_np(name)
#else
#define drm_pthread_setname(tid,name) pthread_setname_np(tid,name)
#endif
/**
 * @class AesDec
 * @brief Vanilla AES based DRM management
 */
class AesDec : public HlsDrmBase
{
public:
	/**
	 * @fn GetInstance
	 */
	static std::shared_ptr<AesDec> GetInstance();
	/**
	 * @fn SetMetaData
	 *
	 * @param metadata - Ignored
	 *
	 * @retval eDRM_SUCCESS
	 */
	DrmReturn SetMetaData(  void* metadata,int trackType);
	/**
	 * @fn GetState
	 * @retval DRMState
	 */
	DRMState GetState();
	/**
	 * @fn AcquireKey
	 *
	 * @param[in] metadata   Ignored
	 *
	 * @retval None
	 */
	void AcquireKey(void *metadata,int trackType);
	/**
	 * @fn SetDecryptInfo
	 *
	 * @param drmInfo Drm information
	 * @retval eDRM_SUCCESS on success
	 */
	DrmReturn SetDecryptInfo(  const struct DrmInfo *drmInfo , int acquireKeyWaitTime);
	/**
	 * @fn Decrypt
	 * @param bucketType Type of bucket for profiling
	 * @param encryptedDataPtr pointer to encrypted payload
	 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
	 * @param timeInMs wait time
	 */
	/**
	 * @fn SetIV
	 *
	 * @param iv New address of initialization vector to use in Decrypt (because received new #EXT-X-KEY with IV)
	 * @retval eDRM_SUCCESS on success
	 */
	DrmReturn SetIV(unsigned char* iv);
	DrmReturn Decrypt(int bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs);
	/**
	 * @fn Release
	 */
	void Release();
	/**
	 * @fn CancelKeyWait
	 *
	 */
	void CancelKeyWait();
	/**
	 * @fn RestoreKeyState
	 */
	void RestoreKeyState();

	/*Functions to support internal operations*/
	/**
	 * @brief key acquisition thread
	 * @retval NULL
	 */
	void acquire_key();
	/**
	 * @brief Acquire drm key from URI
	 */
	void AcquireKey();
	/**
	 * @fn SignalKeyAcquired
	 */
	void SignalKeyAcquired();
	/**
	 * @fn NotifyDRMError
	 * @param drmFailure drm error type
	 */
	void NotifyDRMError(DrmTuneFailure drmFailure);
	/**
	 * @fn SignalDrmError 
        */
	void SignalDrmError();
	/**
	 * @fn WaitForKeyAcquireCompleteUnlocked
	 * @param[in] timeInMs timeout
	 * @param[out] err error on failure
	 */
	void WaitForKeyAcquireCompleteUnlocked(int timeInMs, DrmReturn &err, std::unique_lock<std::mutex>& lock );
	/**
	 * @fn AesDec
	 * 
	 */
	AesDec();
	/**
	 * @fn ~AesDec
	 */
	~AesDec();
	AesDec(const AesDec&) = delete;
	AesDec& operator=(const AesDec&) = delete;

        /*
         *@brief Type definition for acquireLicense callback from application 
         */
        using NotifyCallback = std::function<void(int drmFailure)>;
        NotifyCallback NotifyDrmErrorCb;
        /*
         *@brief Registers acquireLicense callback from application 
         */
        void RegisterNotifyDrmErrorCb(const NotifyCallback Callback)      {
               NotifyDrmErrorCb = Callback;
        };
        using TerminateCurlInstanceCallback = std::function<void(int mCurlInstance)>;
        TerminateCurlInstanceCallback TerminateCurlInstanceCb;
        /*
         *@brief Registers acquireLicense callback from application 
         */
        void RegisterTerminateCurlInstanceCb(const TerminateCurlInstanceCallback Callback)      {
               TerminateCurlInstanceCb = Callback;
        };
	  /** ProfileUpdate callback for updating the profile bucket type to application */
                using ProfileUpdateCallback = std::function<void(int bucketType)>;
        ProfileUpdateCallback ProfileUpdateInitCb;
        ProfileUpdateCallback ProfileUpdateEndCb;

        /*
         *@brief Registers ProfileUpdate  callback from application
         */
        void RegisterProfileUpdateInitCb(const ProfileUpdateCallback Callback){
               ProfileUpdateInitCb = Callback;
        };

	/*
         *@brief Registers ProfileUpdate  callback from application
         */
        void RegisterProfileUpdateEndCb(const ProfileUpdateCallback Callback){
               ProfileUpdateEndCb = Callback;
        };
	  /** ProfileUpdate callback for getting the access key from application  */
        using GetAccessKeyCallback = std::function<void( std::string &keyURI, std::string& tempEffectiveUrl, int& http_error, double& downloadTime, unsigned int curlInstance, bool &KeyAcquisitionStatus, int &failureReason, char** ptr)>;
        GetAccessKeyCallback GetAccessKeyCb;

        /*
         *@brief Registers GetAccessKey callback from application
         */
        void RegisterGetAccessKeyCb(const GetAccessKeyCallback Callback){
               GetAccessKeyCb = Callback;
        };
	  /** ProfileUpdate callback for initiating the curl init  from application  */
        using GetCurlInitCallback = std::function<void(int& curlInstance )>;
        GetCurlInitCallback GetCurlInitCb;

        /*
         *@brief Registers GetAccessKey callback from application
         */
        void RegisterGetCurlInitCb(const GetCurlInitCallback Callback){
               GetCurlInitCb = Callback;
        };
private:

	static std::shared_ptr<AesDec> mInstance;
	std::condition_variable mCond;
	std::mutex mMutex;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX *mOpensslCtx;
#else
	EVP_CIPHER_CTX mOpensslCtx;
#endif
	DrmInfo mDrmInfo ;
        char *m_ptr;
	DRMState mDrmState;
	DRMState mPrevDrmState;
	std::string mDrmUrl;
	int mCurlInstance;
	int mAcquireKeyWaitTime;
	std::thread licenseAcquisitionThreadId;
	bool licenseAcquisitionThreadStarted;
};

#endif // _AES_H_
