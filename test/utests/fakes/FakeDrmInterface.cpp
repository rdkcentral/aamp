
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

/**
 * @file Drm_Interface.cpp
 * @brief HLS Drm Interface */
/* Constructor for dmrInterfce()
 * */
DrmInterface::DrmInterface(PrivateInstanceAAMP* aamp):mAesKeyBuf("aesKeyBuf")
{

}
/*Destructor for DrmInterfcae()
 */
DrmInterface::~DrmInterface()
{
}
void DrmInterface::TerminateCurlInstance(int mCurlInstance)
{
}
void DrmInterface::NotifyDrmError(int drmFailure)
{

}
void DrmInterface::ProfileUpdateDrmDecrypt(bool type, int bucketType)
{


}
void DrmInterface::GetAccessKey(std::string &keyURI,  std::string& tempEffectiveUrl, int& http_error, double& downloadTime,unsigned int curlInstance, bool &keyAcquisitionStatus, int &failureReason,  char** ptr)
{

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
    return nullptr;
}
#ifdef AAMP_VANILLA_AES_SUPPORT
void DrmInterface::RegisterAesInterfaceCb( std::shared_ptr<HlsDrmBase> instance)
{
}
#endif
void DrmInterface::RegisterHlsInterfaceCb( PlayerHlsDrmSessionInterface* instance)
{
}
void DrmInterface::GetCurlInit(int &curlInstance)
{
}
void DrmInterface::getHlsDrmSession(std::shared_ptr <HlsDrmBase>&bridge, std::shared_ptr<DrmHelper> &drmHelper ,  DrmSession* &session , int streamType)
{

}
void DrmInterface::UpdateAamp(PrivateInstanceAAMP* aamp)
{
    
}
