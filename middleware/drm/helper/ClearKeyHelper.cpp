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
 * @file ClearKeyHelper.cpp
 * @brief Helper functions for Clear key
 */

#include "ClearKeyHelper.h"
#include "DrmJsonObject.h"

#include "DrmConstants.h"
#include "PlayerLogManager.h"
/**
 * @brief parse leading protocol from uri if present
 * @param[in] uri manifest/ fragment uri
 * @retval return pointer just past protocol (i.e. http://) if present (or) return NULL uri doesn't start with protcol
 */
static const char * ParseUriProtocol(const char *uri)
{
	if(NULL == uri)
	{
		MW_LOG_ERR("Empty URI");
		return NULL;
	}
	for(;;)
	{
		char ch = *uri++;
		if( ch ==':' )
		{
			if (uri[0] == '/' && uri[1] == '/')
			{
				return uri + 2;
			}
			break;
		}
		else if (isalnum (ch) || ch == '.' || ch == '-' || ch == '+') // other valid (if unlikely) characters for protocol
		{ // legal characters for uri protocol - continue
			continue;
		}
		else
		{
			break;
		}
	}
	return NULL;
}

/**
 * @brief Resolve file URL from the base and file path
 */
void resolveURL(std::string& dst, std::string base, const char *uri , bool bPropagateUriParams)
{
	if( ParseUriProtocol(uri) )
	{
		dst = uri;
	}
	else
	{
		if(base.empty())
		{
			MW_LOG_WARN("Empty base");
			return;
		}
		const char *baseStart = base.c_str();
		const char *basePtr = ParseUriProtocol(baseStart);
		const char *baseEnd;
		for(;;)
		{
			char c = *basePtr;
			if( c==0 || c=='/' || c=='?' )
			{
				baseEnd = basePtr;
				break;
			}
			basePtr++;
		}

		if( uri[0]!='/' && uri[0]!='\0' )
		{
			for(;;)
			{
				char c = *basePtr;
				if( c=='/' )
				{
					baseEnd = basePtr;
				}
				else if( c=='?' || c==0 )
				{
					break;
				}
				basePtr++;
			}
		}
		dst = base.substr(0,baseEnd-baseStart);
		if( uri[0]!='/' )
		{
			dst += "/";
		}
		dst += uri;
		if( bPropagateUriParams )
		{
			if (strchr(uri,'?') == 0)
			{ // uri doesn't have url parameters; copy from parents if present
				const char *baseParams = strchr(basePtr,'?');
				if( baseParams )
				{
					std::string params = base.substr(baseParams-baseStart);
					dst.append(params);
				}
			}
		}
	}
}
static ClearKeyHelperFactory clearkey_helper_factory;

const std::string ClearKeyHelper::CLEARKEY_OCDM_ID = "org.w3.clearkey";

const std::string& ClearKeyHelper::ocdmSystemId() const
{
	return CLEARKEY_OCDM_ID;
}

void ClearKeyHelper::createInitData(std::vector<uint8_t>& initData) const
{
	// For DASH the init data should have been extracted from the PSSH
	// For HLS, we need to construct it
	if (mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
	{
		initData = this->mInitData;
	}
	else
	{
		DrmJsonObject jsonInitDataObj;
		std::vector<std::string> keyIds = {CLEARKEY_KEY_ID};

		if (jsonInitDataObj.add("kids", keyIds))
		{
			jsonInitDataObj.print(initData);
		}
	}
}

bool ClearKeyHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	this->mInitData.assign(initData, initData + initDataLen);

	mKeyID.assign(initData + CLEARKEY_DASH_KEY_ID_OFFSET, initData + CLEARKEY_DASH_KEY_ID_OFFSET + CLEARKEY_DASH_KEY_ID_LEN);

	return true;
}

void ClearKeyHelper::getKey(std::vector<uint8_t>& keyID) const
{
	// For DASH the key should have been extracted from the PSSH
	// For HLS, we return a fixed key, which we also place in the init data
	if (mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
	{
		keyID = this->mKeyID;
	}
	else
	{
		keyID.clear();
		(void)keyID.insert(keyID.begin(), CLEARKEY_KEY_ID.begin(), CLEARKEY_KEY_ID.end());
	}
}

void ClearKeyHelper::generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const
{
	licenseRequest.method = LicenseRequest::POST;

	if(licenseRequest.url.empty())
        {
		if (!mDrmInfo.keyURI.empty())
		{
			std::string keyURI;
			resolveURL(keyURI, mDrmInfo.manifestURL, mDrmInfo.keyURI.c_str(), mDrmInfo.bPropagateUriParams);
			licenseRequest.url = keyURI;
		}
		else
		{
			licenseRequest.url = challengeInfo.url;
		}
        }

	if (NULL != challengeInfo.data)
	{
		licenseRequest.payload = challengeInfo.data->getData();
	}
}

void ClearKeyHelper::transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const
{
	// HLS requires the returned key to be transformed into a JWK.
	// For DASH it will already be in JWK format
	if ((mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS) ||
		(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
	{
		std::vector<uint8_t> licenseResponseData(
                                                 reinterpret_cast<const char*>(licenseResponse->getData().c_str()),
                                                 reinterpret_cast<const char*>(licenseResponse->getData().c_str()) + licenseResponse->getDataLength());

		std::vector<uint8_t> keyId(CLEARKEY_KEY_ID.begin(), CLEARKEY_KEY_ID.end());

		// Construct JSON Web Key (JWK)
		DrmJsonObject keyInstance;
		keyInstance.add("alg", "cbc"); // Hard coded to cbc for now
		keyInstance.add("k", licenseResponseData, DrmJsonObject::ENCODING_BASE64_URL);
		keyInstance.add("kid", keyId, DrmJsonObject::ENCODING_BASE64_URL);

		std::vector<DrmJsonObject*> values = {&keyInstance};

		DrmJsonObject keyObj;
		keyObj.add("keys", values);
		std::string printedJson = keyObj.print();

		licenseResponse->setData( printedJson.c_str(), printedJson.length());
	}
}

bool ClearKeyHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return ((drmInfo.method == eMETHOD_AES_128) &&
			((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) ||
			(drmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
		);
}

DrmHelperPtr ClearKeyHelperFactory::createHelper(const struct DrmInfo& drmInfo) const
{
	MW_LOG_ERR("creating helper");
	if (isDRM(drmInfo))
	{
		return std::make_shared<ClearKeyHelper>(drmInfo);
	}
	else{
		MW_LOG_ERR("creating helper failed");
	}
	return NULL;
}

void ClearKeyHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(CLEARKEY_UUID);
}

