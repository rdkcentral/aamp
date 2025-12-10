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
#ifndef _WIDEVINE_DRM_HELPER_H
#define _WIDEVINE_DRM_HELPER_H

/**
 * @file WidevineDrmHelper.h
 * @brief Handles the operation for Wide vine DRM operation
 */

#include <memory>

#include "DrmHelper.h"

/**
 * @class WidevineDrmHelper
 * @brief Handles the operation for Wide vine DRM operation
 */

class WidevineDrmHelper: public DrmHelper
{
public:
	friend class WidevineDrmHelperFactory;

	virtual const std::string& ocdmSystemId() const override;

	void createInitData(std::vector<uint8_t>& initData) const override;

	bool parsePssh(const uint8_t* initData, uint32_t initDataLen) override;

	bool isClearDecrypt() const override { return false; }

	bool isExternalLicense() const override { return false; };

	void getKey(std::vector<uint8_t>& keyID) const override;

	void getKeys(std::map<int, std::vector<uint8_t>>& keyIDs) const override;

	const std::string& getDrmMetaData() const override {return mContentMetadata;}

	void setDrmMetaData(const std::string& metaData) override;

	void setDefaultKeyID(const std::string& cencData) override;

	virtual int getDrmCodecType() const override { return CODEC_TYPE; }

	void generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const override;

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; };

	WidevineDrmHelper(const struct DrmInfo& drmInfo) : DrmHelper(drmInfo), FRIENDLY_NAME("Widevine"),
		CODEC_TYPE(1), mInitData(), mKeyID(), mKeyIDs(), mContentMetadata(), mDefaultKeySlot(-1)
	{}

	~WidevineDrmHelper() { }

private:
	static const std::string WIDEVINE_OCDM_ID;
	const std::string FRIENDLY_NAME;
	const int CODEC_TYPE;
	std::vector<uint8_t> mInitData;
	std::vector<uint8_t> mKeyID;
	std::map<int,std::vector<uint8_t>> mKeyIDs;
	std::string mContentMetadata;
	int mDefaultKeySlot;
};

/**
 * @class WidevineDrmHelperFactory
 * @brief Helps to handle widevine DRM
 */
class WidevineDrmHelperFactory : public DrmHelperFactory
{
	DrmHelperPtr createHelper(const struct DrmInfo& drmInfo) const;

	void appendSystemId(std::vector<std::string>& systemIds) const;

	bool isDRM(const struct DrmInfo& drmInfo) const;
};


#endif //_WIDEVINE_DRM_HELPER_H
