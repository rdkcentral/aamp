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
 * @file PlayReadyHelper.cpp
 * @brief play Ready DRM helper Engine
 */

#include <uuid/uuid.h>
#include <algorithm>

#include "PlayReadyHelper.h"
#include "DrmJsonObject.h"
#include "DrmConstants.h"

#include "base64.h"

#define KEYID_TAG_START "<KID>"
#define KEYID_TAG_END "</KID>"
#define PLAYREADY_VERSION "4.0.0.0"

static PlayReadyHelperFactory playready_helper_factory;

const std::string PlayReadyHelper::PLAYREADY_OCDM_ID = "com.microsoft.playready";
const size_t PlayReadyHelper::PLAYREADY_DECODED_KEY_ID_LEN = 16U;
const size_t PlayReadyHelper::PLAYREADY_KEY_ID_LEN = 37U;

const std::string& PlayReadyHelper::ocdmSystemId() const
{
	return PLAYREADY_OCDM_ID;
}

void PlayReadyHelper::createInitData(std::vector<uint8_t>& initData) const
{
	initData = this->mInitData;
}


/**< API Move to util class**/
/**
 * @brief find sub string in between string
 * @return substring
 */
std::string PlayReadyHelper::findSubstr(std::string &data, std::string start, std::string end)
{
	std::string retVal = "";
	auto first = data.find(start);
	if ((first != std::string::npos))
	{
		std::string subStr = data.substr(first + start.size());
		auto last = subStr.find(end);
		if ((last != std::string::npos))
		{
			retVal =  subStr.substr (0,last);
		}
	}
	return retVal;
}

/**
 * @brief Extract keyID from given PSSH data.
 * 
 * @return std::string keyId - copy by value
 */
#define PLAYREADY_VERSION_4_0_KID_START "<KID>"     /**< KeyId represent as value for 4.0 version*/ 
#define PLAYREADY_VERSION_4_X_KID_START "<KID"      /**< KeyId represent as attribute for 4.x version where x > 0 */ 
#define PLAYREADY_KID_END       "</KID>"			/**< KeyId node end **/

#define PLAYREADY_VERSION_4_0           "4.0.0.0"   /**< Playready version 4.0 **/
#define PLAYREADY_VERSION_4_1           "4.1.0.0"   /**< Playready version 4.1 **/
#define PLAYREADY_VERSION_4_2           "4.2.0.0"   /**< Playready version 4.2 **/
#define PLAYREADY_VERSION_4_3           "4.3.0.0"   /**< Playready version 4.3 **/
std::string PlayReadyHelper::extractKeyID()
{
	std::string propValueEnd = "\"";
	std::string version = findSubstr(mStrInitDataFormated, "version=\"", propValueEnd);
	std::string keyId = "";

	MW_LOG_INFO ("PlayReady Version [%s]", version.c_str());
	
	if(version == PLAYREADY_VERSION_4_0)
	{
		keyId = findSubstr(mStrInitDataFormated, PLAYREADY_VERSION_4_0_KID_START, PLAYREADY_KID_END);
	}
	else if((version == PLAYREADY_VERSION_4_1))
	{
		/**< Key Id represent in protectinfo block without attribute; single keyId support 
		 * <PROTECTINFO>        
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * </PROTECTINFO>
		 */

		std::string protectInfo = findSubstr(mStrInitDataFormated, "<PROTECTINFO>", "</PROTECTINFO>");
		std::string keyTag = findSubstr(protectInfo, PLAYREADY_VERSION_4_X_KID_START, PLAYREADY_KID_END);
		keyId = findSubstr(keyTag, "VALUE=\"", propValueEnd);	
	}
	else if((version == PLAYREADY_VERSION_4_2))
	{
		/**< Key Id represent in protectinfo block without attribute; multiple keyIds support 
		 * <PROTECTINFO>
		 * <KIDS>
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * </KIDS>
		 * </PROTECTINFO>
		 */
		std::string protectInfo = findSubstr(mStrInitDataFormated, "<PROTECTINFO>", "</PROTECTINFO>");
		std::string kids = findSubstr(protectInfo, "<KIDS>", "</KIDS>");
		/**< TODO: multiple keyIds support in Player; now taking first value only **/
		std::string keyTag = findSubstr(kids, PLAYREADY_VERSION_4_X_KID_START, PLAYREADY_KID_END);
		keyId = findSubstr(keyTag, "VALUE=\"", propValueEnd);
	}
	else if((version == PLAYREADY_VERSION_4_3))
	{
		/**< Key Id represent in protectinfo block with attribute; multiple keyIds support 
		 * <PROTECTINFO LICENSEREQUESTED="true">
		 * <KIDS>
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * <KID ALGID="AESCTR" CHECKSUM="base64-encoded value" VALUE="base64-encoded guid"></KID>
		 * </KIDS>
		 * </PROTECTINFO>
		 */
		std::string protectInfo = findSubstr(mStrInitDataFormated, "<PROTECTINFO", "</PROTECTINFO>");
		std::string kids = findSubstr(protectInfo, "<KIDS>", "</KIDS>");
		/**< TODO: multiple keyIds support in PLAYER; now taking first value only **/
		std::string keyTag = findSubstr(kids, PLAYREADY_VERSION_4_X_KID_START, PLAYREADY_KID_END);
		keyId = findSubstr(keyTag, "VALUE=\"", propValueEnd);
	}
	else
	{
		MW_LOG_WARN ("Unsupported PSSH version MPD[%s]", version.c_str());
	}	

	MW_LOG_INFO("Extracted Key ID: %s", keyId.c_str());
	//DumpBlob((const unsigned char*)keyId.c_str(), keyId.size());
	return keyId;
			
}

/**
 *  @brief      Extract content meta data from given PSSH data.
 *              For example for content meta data,
 *              When strings are given as "ckm:policy xmlns:ckm="urn:ccp:ckm"" and "ckm:policy"
 *              <ckm:policy xmlns:ckm="urn:ccp:ckm">we need the contents from here</ckm:policy>
 */
std::string PlayReadyHelper::extractMetaData()
{
	return findSubstr(mStrInitDataFormated, DRM_METADATA_TAG_START, DRM_METADATA_TAG_END);
}

bool PlayReadyHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	int keyIdLen = 0;
	bool res = false;
	// Extract key
	if (initData != NULL &&  initDataLen > 0)
	{
		this->mInitData.assign(initData, initData + initDataLen);
		std::string keyData = "";
		char* cleanedPssh = (char*) malloc(initDataLen+1);
		if (cleanedPssh)
		{
			int cleanedPsshLen = 0;
			for(int itr = 0; itr < initDataLen; itr++)
			{
				if(initData[itr] != 0)
				{
					//cout<<psshData[itr];
					cleanedPssh[cleanedPsshLen++] = initData[itr];
				}
			}
			cleanedPssh[cleanedPsshLen] = 0;

			this->mStrInitDataFormated = std::string(cleanedPssh);
			free(cleanedPssh);
			cleanedPssh = NULL;
			//Clear unwanted spaces from pssh data - time being not neeed
			//std::remove(mStrInitDataFormated.begin(), mStrInitDataFormated.end(), ' ');
			
			keyData = extractKeyID();
		}
		//MW_LOG_INFO("pr keyid: %s keyIdlen: %d", keydata, keyIdLen);
		if (!keyData.empty())
		{
			size_t decodedDataLen = 0;
			unsigned char* decodedKeydata = Base64Utils::base64Decode(keyData.c_str(), &decodedDataLen, keyData.size());

			if (decodedDataLen != PLAYREADY_DECODED_KEY_ID_LEN)
			{
				MW_LOG_ERR("Invalid key size found while extracting PR Decoded-KeyID-Length: %zu (PR KeyID: %s  KeyID-Length: %d)", decodedDataLen, keyData.c_str(), keyIdLen);
			}
			else
			{
				unsigned char swappedKeydata[PLAYREADY_DECODED_KEY_ID_LEN] = {0};
				DrmUtils::convertEndianness(decodedKeydata, swappedKeydata);
				unsigned char keyId[PLAYREADY_KEY_ID_LEN] = {0};
				uuid_t *keyiduuid = (uuid_t*)swappedKeydata;
				uuid_unparse_lower(*keyiduuid, reinterpret_cast<char*>(keyId));
				MW_LOG_INFO("Extracted Key ID is %s", keyId);
				mKeyID.assign(keyId, keyId + (PLAYREADY_KEY_ID_LEN-1)); /**< No need end null character in vector **/
				res = true;
			}

			free(decodedKeydata);
		}
		else
		{
			MW_LOG_WARN("Bad DRM init data with Empty KeyID has received : %s!!", friendlyName().c_str());
		}

		/**< Extract the optional content metadata. 
		 * No specification available based on version*/
		mContentMetaData = extractMetaData();
	}
	else
	{
		MW_LOG_ERR("Invalid PSSH Data Received : NULL");
	}

	return res;
}

void PlayReadyHelper::setDrmMetaData(const std::string& metaData)
{
	if (mContentMetaData.empty())
	{
		mContentMetaData = metaData;
	}
}

void PlayReadyHelper::getKey(std::vector<uint8_t>& keyID) const
{
	keyID = this->mKeyID;
}

void PlayReadyHelper::generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const
{
	licenseRequest.method = LicenseRequest::POST;

	if (licenseRequest.url.empty())
	{
		licenseRequest.url = challengeInfo.url;
	}

	licenseRequest.headers = {{"Content-Type:", {"text/xml; charset=utf-8"}}};

	if (!mContentMetaData.empty())
	{
		std::vector<uint8_t> challengeData(reinterpret_cast<const char*>(challengeInfo.data->getData().c_str()),reinterpret_cast<const char*>(challengeInfo.data->getData().c_str()) + challengeInfo.data->getDataLength());

		DrmJsonObject comChallengeObj;
		comChallengeObj.add("keySystem", "playReady");
		comChallengeObj.add("mediaUsage", "stream");
		comChallengeObj.add("licenseRequest", challengeData, DrmJsonObject::ENCODING_BASE64);
		comChallengeObj.add("contentMetadata", mContentMetaData, DrmJsonObject::ENCODING_BASE64);

		if ((!challengeInfo.accessToken.empty()) && !licenseRequest.licenseAnonymousRequest)
		{
			comChallengeObj.add("accessToken", challengeInfo.accessToken);
		}

		licenseRequest.payload = comChallengeObj.print();
	}
	else if (challengeInfo.data)
	{
	   licenseRequest.payload = challengeInfo.data->getData();
	}
}

bool PlayReadyHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return (((drmInfo.systemUUID == PLAYREADY_UUID) || (drmInfo.keyFormat == PlayReadyHelper::PLAYREADY_OCDM_ID))
		&& ((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) || (drmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
		);
}

DrmHelperPtr PlayReadyHelperFactory::createHelper(const struct DrmInfo& drmInfo) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<PlayReadyHelper>(drmInfo);
	}
	return NULL;
}

void PlayReadyHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(PLAYREADY_UUID);
}
