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

/**
 * @file HlsDrmBase.h
 * @brief Declaration common to various HLS DRM implementations
 */


#ifndef _DRM_HLSDRMBASE_H_
#define _DRM_HLSDRMBASE_H_
#include "PlayerUtils.h"
#include "DrmSession.h"


#define DECRYPT_WAIT_TIME_MS 3000

/**
 * @enum ProfilerBucketType
 * @brief Bucket types of  profiler
 */
typedef enum
{
	DRM_PROFILE_BUCKET_DECRYPT_VIDEO,       /**< Video decryption bucket*/
	DRM_PROFILE_BUCKET_DECRYPT_AUDIO,       /**< Audio decryption bucket*/
	DRM_PROFILE_BUCKET_DECRYPT_SUBTITLE,    /**< Subtitle decryption bucket*/
	DRM_PROFILE_BUCKET_DECRYPT_AUXILIARY,   /**< Auxiliary decryption bucket*/

	DRM_PROFILE_BUCKET_LA_TOTAL,            /**< License acquisition total bucket*/
	DRM_PROFILE_BUCKET_LA_PREPROC,          /**< License acquisition pre-processing bucket*/

} DrmProfilerBucketType;

/**
 * @enum DrmReturn
 * @brief Return values of various functions
 */
enum DrmReturn
{
	eDRM_SUCCESS,			/**< DRM is success */
	eDRM_ERROR,			/**< DRM Failed */
	eDRM_KEY_ACQUISITION_TIMEOUT	/**< DRM key acquisition timed out */
};

/**
 * @enum DRMState
 * @brief States of DRM object
 */
enum DRMState
{
	eDRM_INITIALIZED,	/**< DRM is initialized */
	eDRM_ACQUIRING_KEY,	/**< DRM Acquiring key in progress */
	eDRM_KEY_ACQUIRED,	/**< DRM key is Acquired */
	eDRM_KEY_FAILED,	/**< DRM Acquiring key is failed */
	eDRM_KEY_FLUSH		/**< DRM key is flushed */
};

/**
 * @class HlsDrmBase
 * @brief Base class of HLS DRM implementations
 */
class HlsDrmBase
{
public:

	/**
	 * @brief Set DRM specific meta-data
	 *
	 * @param metadata DRM specific metadata
	 * @retval 0 on success
	 */
	virtual DrmReturn SetMetaData(void* metadata,int trackType) = 0;

	/**
	 * @brief Set information required for decryption
	 *
	 * @param drmInfo Drm information
	 * @retval eDRM_SUCCESS on success
	 */
	virtual DrmReturn SetDecryptInfo(const struct DrmInfo *drmInfo,  int acquireKeyWaitTime) = 0;


	/**
	 * @brief Decrypts an encrypted buffer
	 * @param bucketType Type of bucket for profiling
	 * @param encryptedDataPtr pointer to encrypted payload
	 * @param encryptedDataLen length in bytes of data pointed to by encryptedDataPtr
	 * @param timeInMs wait time
	 * @retval eDRM_SUCCESS on success
	 */
	virtual DrmReturn Decrypt(int bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs = 3000) = 0;

	/**
	 * @brief Release drm session
	 */
	virtual void Release() = 0;

	/**
	 * @brief Cancel timed_wait operation drm_Decrypt
	 */
	virtual void CancelKeyWait() = 0;

	/**
	 * @brief Restore key state post cleanup of
	 * audio/video TrackState in case DRM data is persisted
	 */
	virtual void RestoreKeyState() = 0;
	/**
	* @brief AcquireKey Function to get DRM Key
	*
	*/
	virtual void AcquireKey(void *metadata,int trackType) = 0;
	/**
	* @brief GetState Function to get current DRM state
	*
	*/
	virtual DRMState GetState() = 0;
	/**
	 * @brief HlsDrmBase Destructor
	 */
	virtual ~HlsDrmBase(){};

};
#endif /* _DRM_HLSDRMBASE_H_ */
