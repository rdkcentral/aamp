/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file HlsOcdmBridge.cpp
 * @brief Handles OCDM bridge to validate DRM License
 */

#include "HlsOcdmBridge.h"

#include "PlayerLogManager.h"
#define DRM_IV_LEN 16


HlsOcdmBridge::HlsOcdmBridge(DrmSession * drmSession) :
	m_drmInfo(nullptr),
	m_drmSession(drmSession),
	m_drmState(eDRM_INITIALIZED),
	m_Mutex()
{
}


HlsOcdmBridge::~HlsOcdmBridge()
{
}


DrmReturn HlsOcdmBridge::SetDecryptInfo( const struct DrmInfo *drmInfo,  int acquireKeyWaitTime)
{
	DrmReturn result  = eDRM_ERROR;

	std::lock_guard<std::mutex> guard(m_Mutex);
	m_drmInfo = drmInfo;
	KeyState eKeyState = m_drmSession->getState();
	if (eKeyState == KEY_READY)
	{
		m_drmState = eDRM_KEY_ACQUIRED;
		result = eDRM_SUCCESS; //frag_collector ignores the return
	}
	MW_LOG_TRACE("DecryptInfo Set");

	return result;
}


DrmReturn HlsOcdmBridge::Decrypt( int bucketTypeIn, void *encryptedDataPtr, size_t encryptedDataLen,int timeInMs)
{
        DrmProfilerBucketType bucketType = (DrmProfilerBucketType)bucketTypeIn;

	DrmReturn result = eDRM_ERROR;

	std::lock_guard<std::mutex> guard(m_Mutex);
	if (m_drmState == eDRM_KEY_ACQUIRED)
	{
		 MW_LOG_TRACE("Starting decrypt");
		 int retVal = m_drmSession->decrypt(m_drmInfo->iv, DRM_IV_LEN, (const uint8_t *)encryptedDataPtr , (uint32_t)encryptedDataLen, NULL);
		 if (retVal)
		 {
			MW_LOG_WARN("Decrypt failed err = %d", retVal);
		 }
		 else
		 {
			MW_LOG_TRACE("Decrypt success");
			result = eDRM_SUCCESS;
		 }
	}
	else
	{
		MW_LOG_WARN("Decrypt Called in Incorrect State! DrmState = %d", (int)m_drmState);
	}
	return result;
}


void HlsOcdmBridge::Release(void)
{
	MW_LOG_WARN("Releasing the Opencdm Session");
	m_drmSession->clearDecryptContext();
}


void HlsOcdmBridge::CancelKeyWait(void)
{
	//TBD:Unimplemented
}

