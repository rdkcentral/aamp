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
#ifndef _PLAYREADY_HELPER_H
#define _PLAYREADY_HELPER_H

/**
 * @file PlayReadyHelper.h
 * @brief Handles the operation for Play ready DRM operations
 */

#include <memory>

#include "DrmHelper.h"
#include "PlayerLogManager.h"
/** 
 * @class PlayReadyHelper
 * @brief Handles the operation for Play ready DRM operations
 */

class PlayReadyHelper : public DrmHelper
{
public:
	friend class PlayReadyHelperFactory;

	const std::string& ocdmSystemId() const override;

	void createInitData(std::vector<uint8_t>& initData) const override;

	bool parsePssh(const uint8_t* initData, uint32_t initDataLen) override;

	bool isClearDecrypt() const override { return false; }

	bool isHdcp22Required() const override { return bOutputProtectionEnabled; }

	void setDrmMetaData( const std::string& metaData ) override;

	void getKey(std::vector<uint8_t>& keyID) const override;

	virtual int getDrmCodecType() const override { return CODEC_TYPE; }

	void generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const override;

	const std::string& getDrmMetaData() const override {return mContentMetaData;}

	virtual const std::string& friendlyName() const override { return FRIENDLY_NAME; };

	PlayReadyHelper(const struct DrmInfo& drmInfo) : DrmHelper(drmInfo), FRIENDLY_NAME("PlayReady"), CODEC_TYPE(2),
		mPsshStr(),
		mInitData(), mKeyID(), mContentMetaData(), mStrInitDataFormated()
	{}

	~PlayReadyHelper() {}

private:
	std::string extractMetaData();
	std::string extractKeyID();
	std::string findSubstr(std::string &data, std::string start, std::string end); /**< move to utils */
	
	static const std::string PLAYREADY_OCDM_ID;
	static const size_t PLAYREADY_DECODED_KEY_ID_LEN; // Expected size of base64 decoded key ID from the PSSH
	static const size_t PLAYREADY_KEY_ID_LEN; // PlayReady ID key length. A NULL character is included at the end
	const std::string FRIENDLY_NAME;
	const int CODEC_TYPE;

	std::string mPsshStr;
	std::vector<uint8_t> mInitData;
	std::vector<uint8_t> mKeyID;
	std::string mContentMetaData;
	std::string mStrInitDataFormated;           /**< String init data after clean up*/
};

/**
 * @class PlayReadyHelperFactory
 * @brief Handles operations to support play ready DRM
 */
class PlayReadyHelperFactory : public DrmHelperFactory
{
public:
	DrmHelperPtr createHelper(const struct DrmInfo& drmInfo) const;

	void appendSystemId(std::vector<std::string>& systemIds) const;

	bool isDRM(const struct DrmInfo& drmInfo) const;
};

#endif //_PLAYREADY_HELPER_H
