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
#include "DrmInterface.h"
#include "Aes.h"
#include "HlsOcdmBridgeInterface.h"
#include "AampGrowableBuffer.h"
#include "HlsDrmSessionManager.h"
#include "AampDRMLicManager.h"
#define AES_128_KEY_LEN_BYTES 16
/**
 * @file Drm_Interface.cpp
 * @brief HLS Drm Interface */
/*
 * @brief registerCallback for the interface file between player and DRM
 */
void registerCallback(DrmInterface *_this ,std::shared_ptr<AesDec> instance )
{
    auto Instance = static_cast<DrmInterface*>(_this);
    /** 
     * @brief Register the callback for license data */
    instance->RegisterTerminateCurlInstanceCb([_this](int mCurlInstance) {
        return _this->TerminateCurlInstance(mCurlInstance);
    });
    /**
     * @brief Register callback for notify any drm related failures to player
     */
     instance->RegisterNotifyDrmErrorCb([_this](int drmFailure ) {
        return _this->NotifyDrmError(drmFailure);
    });
     /** 
      * @brief Register callback to do profiling 
      */
      instance->RegisterProfileUpdateCb([_this](bool type , int bucketType ) {
        return _this->ProfileUpdateDrmDecrypt(type, (int)bucketType);
    });
      /**
       * @brief  Callback for Access key 
       */
      instance->RegisterGetAccessKeyCb([_this](std::string &keyURI, std::string& tempEffectiveUrl, int& http_error, double& downloadTime,unsigned int curlInstance
, bool &keyAcquisitionStatus, int &failureReason,  char** ptr) {
        return _this->GetAccessKey(keyURI, tempEffectiveUrl, http_error,  downloadTime,curlInstance
, keyAcquisitionStatus, failureReason, ptr);
    });
      /** 
       * @brief Callback for curl init
       */
      instance->RegisterGetCurlInitCb([_this](int& curlInstance ){
        return _this->GetCurlInit(curlInstance);
    });

}
/**
 *@brief updates the PrivateInstanceAAMP instance
 */
void DrmInterface::UpdateAamp(PrivateInstanceAAMP* aamp)
{
	mpAamp = aamp;
}

/**
 * @brief registerCallbackForHls - register callback only for HLS
 */
void registerCallbackForHls(DrmInterface* _this, PlayerHlsDrmSessionInterface* instance)
{

      instance->RegisterGetHlsDrmSessionCb([_this](std::shared_ptr <HlsDrmBase>&bridge, std::shared_ptr<DrmHelper> &drmHelper ,  DrmSession* &session , int streamType){
		      return _this->getHlsDrmSession(bridge, drmHelper, session ,  streamType);
		      });
}
/* 
 * @brief DrmInterface constructor 
 * */
DrmInterface::DrmInterface(PrivateInstanceAAMP* aamp):mAesKeyBuf("aesKeyBuf")
{

    mpAamp = aamp;	
}
/*
 * @brief DrmInterface destructor
 */
DrmInterface::~DrmInterface()
{
}
/*
 * @brief TerminateCurlInstance Terminating the curl instance if any 
 */
void DrmInterface::TerminateCurlInstance(int mCurlInstance)
{
   mpAamp->SyncBegin();
   mpAamp->CurlTerm((AampCurlInstance)mCurlInstance);
   mpAamp->SyncEnd();


}
/**
 * @brief Notify the error /failures to player
 */
void DrmInterface::NotifyDrmError(int drmFailure)
{
	//If downloads are disabled, don't send error event upstream
	if (mpAamp->DownloadsAreEnabled())
	{
		mpAamp->DisableDownloads();
		if(AAMP_TUNE_UNTRACKED_DRM_ERROR == drmFailure)
		{
			mpAamp->SendErrorEvent((AAMPTuneFailure)drmFailure, "AAMP: DRM Failure" );
		}
		else
		{
			mpAamp->SendErrorEvent((AAMPTuneFailure)drmFailure);
		}
	}

}
ProfilerBucketType DrmInterface::MapDrmToProfilerBucket(DrmProfilerBucketType drmType)
{
    switch (drmType)
    {
        case DRM_PROFILE_BUCKET_DECRYPT_VIDEO:    return PROFILE_BUCKET_DECRYPT_VIDEO;
        case DRM_PROFILE_BUCKET_DECRYPT_AUDIO:    return PROFILE_BUCKET_DECRYPT_AUDIO;
        case DRM_PROFILE_BUCKET_DECRYPT_SUBTITLE: return PROFILE_BUCKET_DECRYPT_SUBTITLE;
        case DRM_PROFILE_BUCKET_DECRYPT_AUXILIARY:return PROFILE_BUCKET_DECRYPT_AUXILIARY;

        case DRM_PROFILE_BUCKET_LA_TOTAL:         return PROFILE_BUCKET_LA_TOTAL;
        case DRM_PROFILE_BUCKET_LA_PREPROC:       return PROFILE_BUCKET_LA_PREPROC;

        default: return PROFILE_BUCKET_TYPE_COUNT; // or handle as error
    }
}
/**
 * @brief Update the profiling type to player
 */
void DrmInterface::ProfileUpdateDrmDecrypt(bool type, int bucketType)
{
	if(type == 0)
	{
		mpAamp->LogDrmInitComplete();
	}
	else
	{
	       ProfilerBucketType val = MapDrmToProfilerBucket((DrmProfilerBucketType)bucketType);
	       mpAamp->LogDrmDecryptEnd(val);
	}
}
/**
 * @brief GetAccessKey To get access of the key 
 */
void DrmInterface::GetAccessKey(std::string &keyURI,  std::string& tempEffectiveUrl, int& http_error, double& downloadTime,unsigned int curlInstance, bool &keyAcquisitionStatus, int &failureReason,  char** ptr)
{
        bool fetched = mpAamp->GetFile(keyURI, (AampMediaType)eMEDIATYPE_LICENCE, &mAesKeyBuf, tempEffectiveUrl, &http_error, &downloadTime, NULL, curlInstance, true);
	*ptr =mAesKeyBuf.GetPtr();

        if (fetched)
        {
                if (AES_128_KEY_LEN_BYTES == mAesKeyBuf.GetLen() )
                {
                        AAMPLOG_WARN("Key fetch success len = %d",  (int)mAesKeyBuf.GetLen());
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
}
/*
 * initialise the static member variable 
 */
std::shared_ptr<DrmInterface> DrmInterface::mInstance = nullptr;
/**
 * @brief Get singleton instance
 */
std::shared_ptr<DrmInterface> DrmInterface::GetInstance(PrivateInstanceAAMP* aamp)
{
	if (nullptr == mInstance)
        {
                mInstance = std::make_shared<DrmInterface>(aamp);
        } 
        return mInstance;
}
void DrmInterface::RegisterAesInterfaceCb( std::shared_ptr<HlsDrmBase> instance)
{
      std::shared_ptr<AesDec> aesDecPtr = std::dynamic_pointer_cast<AesDec>(instance);

     if(instance)
     {
         registerCallback(this , aesDecPtr);
     }
}
/** 
 * @brief RegisterHlsInterfaceCb callback to register 
 */
void DrmInterface::RegisterHlsInterfaceCb( PlayerHlsDrmSessionInterface* instance)
{
      PlayerHlsDrmSessionInterface* hlsDecPtr = (PlayerHlsDrmSessionInterface*)instance;

      registerCallbackForHls(this , hlsDecPtr);
}
/**
 * @brief Curl Init
 */
void DrmInterface::GetCurlInit(int &curlInstance)
{
	if ((-1 == curlInstance) && (mpAamp != NULL))
        {
              curlInstance = eCURLINSTANCE_AES;
              mpAamp->CurlInit((AampCurlInstance)curlInstance,1,mpAamp->GetLicenseReqProxy());
	
	}
}

/**
 * @brief - Get Drm Session for HLS
 */
void DrmInterface::getHlsDrmSession(std::shared_ptr <HlsDrmBase>&bridge, std::shared_ptr<DrmHelper> &drmHelper ,  DrmSession* &session , int streamType)
{
        mpAamp->mDRMLicenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);

        mpAamp->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
        DrmMetaDataEventPtr event = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false, mpAamp->GetSessionId());
        session = mpAamp->mDRMLicenseManager->createDrmSession( drmHelper, mpAamp, event , (int)streamType);
        if (!session)
        {
                AAMPLOG_WARN("Failed to create Drm Session ");

                if (mpAamp->DownloadsAreEnabled())
                {
                        AAMPTuneFailure failure = event->getFailure();

                        mpAamp->DisableDownloads();
                        mpAamp->SendErrorEvent(failure);

                        mpAamp->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int) failure);
                }
        }
        else
        {
                AAMPLOG_WARN("created Drm Session ");
                HlsDrmBase* tempBridge = HlsOcdmBridgeInterface::GetBridge(session);
                bridge = std::shared_ptr<HlsDrmBase>(tempBridge);
        }
        mpAamp->profiler.ProfileEnd(PROFILE_BUCKET_LA_TOTAL);
}
