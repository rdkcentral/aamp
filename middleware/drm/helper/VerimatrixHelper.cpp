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

#include <memory>
#include <iostream>

#include "VerimatrixHelper.h"
#include "DrmUtils.h"
#include "DrmConstants.h"

#define KEYURL_TAG_START "<KeyUrl><![CDATA["
#define KEYURL_TAG_END "]]></KeyUrl>"

static VerimatrixHelperFactory verimatrix_helper_factory;

const std::string VerimatrixHelper::VERIMATRIX_OCDM_ID = "com.verimatrix.ott";

bool VerimatrixHelper::parsePssh(const uint8_t* initData, uint32_t initDataLen)
{
	this->mInitData.assign(initData, initData+initDataLen);

	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
	{
		MW_LOG_WARN("mediaFormat is not DASH");
		return true;
	}

	if(initDataLen < VERIMATRIX_PSSH_DATA_POSITION)
	{
		MW_LOG_WARN("initDataLen less than %d", VERIMATRIX_PSSH_DATA_POSITION);
		return false;
	}

	initData += VERIMATRIX_PSSH_DATA_POSITION;
	initDataLen -= VERIMATRIX_PSSH_DATA_POSITION;
	char *init = new char[initDataLen + 1];
	memcpy(init, initData, initDataLen);
	init[initDataLen] = 0;

	std::string pssh(init);
	delete []init;

	MW_LOG_WARN("pssh %s", pssh.c_str());

	size_t sp = pssh.find(KEYURL_TAG_START);
	size_t ep = pssh.find(KEYURL_TAG_END);
	if((sp == std::string::npos) || (ep == std::string::npos))
	{
		MW_LOG_WARN("not found KeyUrl TAG");
		return false;
	}

	sp += strlen(KEYURL_TAG_START);

	std::string keyfile = pssh.substr(sp, ep - sp);
	MW_LOG_WARN("keyfile %s", keyfile.c_str());

	mKeyID.assign((uint8_t *)keyfile.c_str(), (uint8_t *)keyfile.c_str() + keyfile.length());

	return true;
}


void VerimatrixHelper::setDrmMetaData(const std::string& metaData)
{
	mContentMetadata = metaData;
}

const std::string& VerimatrixHelper::ocdmSystemId() const
{
	return VERIMATRIX_OCDM_ID;
}

void VerimatrixHelper::createInitData(std::vector<uint8_t>& initData) const
{
	const char *init;

	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
		init = "{\"contentType\" : \"HLS\"}";
	else if(mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
		init = "{\"contentType\" : \"DASH\"}";
	else
		MW_LOG_WARN("unknown mediaFormat %d", mDrmInfo.mediaFormat);

	initData.assign((uint8_t *)init, (uint8_t *)init + strlen(init));
}

void VerimatrixHelper::getKey(std::vector<uint8_t>& keyID) const
{
	keyID.clear();
	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
		keyID.insert(keyID.begin(), mDrmInfo.keyURI.begin(), mDrmInfo.keyURI.end());
	else if(mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
		keyID.insert(keyID.begin(), mKeyID.begin(), mKeyID.end());
	else
		MW_LOG_WARN("unknown mediaFormat %d", mDrmInfo.mediaFormat);
}

void VerimatrixHelper::generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const
{
	licenseRequest.method = LicenseRequest::DRM_RETRIEVE;
	licenseRequest.url = "";
	licenseRequest.payload = "";
}

void VerimatrixHelper::transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const
{
	if(mDrmInfo.mediaFormat == eMEDIAFORMAT_HLS)
		licenseResponse->setData((unsigned char*)mDrmInfo.keyURI.c_str(), mDrmInfo.keyURI.length());
	else if(mDrmInfo.mediaFormat == eMEDIAFORMAT_DASH)
		licenseResponse->setData((unsigned char*)mKeyID.data(), mKeyID.size());
	else
		MW_LOG_WARN("unknown mediaFormat %d", mDrmInfo.mediaFormat);
}

bool VerimatrixHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return (((VERIMATRIX_UUID == drmInfo.systemUUID) || (VerimatrixHelper::VERIMATRIX_OCDM_ID == drmInfo.keyFormat))
		&& ((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) || (drmInfo.mediaFormat == eMEDIAFORMAT_HLS))
		);
}

DrmHelperPtr VerimatrixHelperFactory::createHelper(const struct DrmInfo& drmInfo) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<VerimatrixHelper>(drmInfo);
	}
	return NULL;
}

void VerimatrixHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(VERIMATRIX_UUID);
}
