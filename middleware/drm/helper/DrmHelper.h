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

#ifndef _DRM_HELPER_H
#define _DRM_HELPER_H

/**
 * @file DrmHelper.h
 * @brief Implemented DRM helper functionalities
 */

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <map>

#include "DrmData.h"
#include "DrmInfo.h"
#include "DrmUtils.h"
#include "DrmMemorySystem.h"

/**
 * @struct ChallengeInfo
 * @brief challenge info to get the License
 */

struct ChallengeInfo
{
	ChallengeInfo() : data(), url(), accessToken() {};
	std::shared_ptr<DrmData> data; /**< Challenge data returned from the DRM system */
	std::string url;               /**< Challenge URL returned from the DRM system */
	std::string accessToken;       /**< Access token required for the license request (if applicable) */
};

/**
 * @struct LicenseRequest
 * @brief Holds the data to get the License
 */

struct LicenseRequest
{
	LicenseRequest() : method(), url(), payload(), headers(),licenseAnonymousRequest(false)
	{}
	enum LicenseMethod
	{
		DRM_RETRIEVE,   /**< Don't fetch the license, it will be handled externally by the DRM */
		GET,		    /**< Fetch license via HTTP GET request */
		POST		    /**< Fetch license via HTTP POST request */
	};

	LicenseMethod method;
	bool licenseAnonymousRequest;
	std::string url;
	std::string payload;
	std::unordered_map<std::string, std::vector<std::string>> headers;
};

/**
 * @class DrmHelper
 * @brief DRM helper to handle DRM operations
 */
class DrmHelper;

using DrmHelperPtr = std::shared_ptr<DrmHelper>;

/**
 * @class DrmHelper
 * @brief DRM helper to handle DRM operations
 */
class DrmHelper
{
public:
	const uint32_t TIMEOUT_SECONDS;
	const std::string EMPTY_DRM_METADATA;
	
	const std::string EMPTY_STRING;
	DrmHelper(const struct DrmInfo drmInfo) : mDrmInfo(drmInfo), TIMEOUT_SECONDS(5000U), EMPTY_DRM_METADATA(), EMPTY_STRING() ,bOutputProtectionEnabled(false), protectionScheme() {}
	DrmHelper(const DrmHelper&) = delete;
	DrmHelper& operator=(const DrmHelper&) = delete;
	
	
	uint32_t getProtectionScheme()
	{
		return protectionScheme;
	}
	
	/**
	 * @brief Returns the OCDM system ID of the helper
	 * @return the OCDM system ID
	 */
	virtual const std::string& ocdmSystemId() const = 0;

	/**
	 *
	 * @param initData the Init Data to send to the CDM
	 */
	virtual void createInitData(std::vector<uint8_t>& initData) const = 0;

	/**
	 * @brief Parse the optional PSSH data
	 * @param initData The init data from the PSSH
	 * @param initDataLen the length of initData
	 * @return
	 */
	virtual bool parsePssh(const uint8_t* initData, uint32_t initDataLen) = 0;

	/**
	 * @brief Determine if the DRM system needs to be in the clear or encrypted
	 * @return true if the data is clear, false if it should remain in the TEE
	 */
	virtual bool isClearDecrypt() const = 0;

	/**
	 * @brief Determine whether HDCP 2.2 protection is required to be active
	 * @return true if HDCP 2.2 protection is required, false otherwise
	 */
	virtual bool isHdcp22Required() const { return bOutputProtectionEnabled; }

	/**
	 * @brief Returns the content specific DRM metadata
	 * @return the DRM metadata
	 */
	virtual const std::string& getDrmMetaData() const {return EMPTY_DRM_METADATA;}

	/**
	 * @brief Sets the content specific DRM metadata
	 * @param the DRM metadata
	 */
	virtual void setDrmMetaData(const std::string& metaData) { }

	/**
	 * @brief Sets the default keyID
	 * @param the DRM cencData data
	 */
	virtual void setDefaultKeyID(const std::string& cencData) { }

	/**
	 * @brief Returns the DRM codec type for the helper, used in trace
	 * @return the DRM codec type
	 */
	virtual int getDrmCodecType() const { return 0; }

	/**
	 * @brief Get the amount of time in milliseconds to wait before aborting the wait
	 * for the license_challenge message to be received
	 * Default is TWO Seconds - 2000
	 * @return the time to wait in milliseconds
	 */
	virtual uint32_t licenseGenerateTimeout() const { return TIMEOUT_SECONDS; }

	/**
	 * @brief Get the amount of time in milliseconds to wait before aborting the wait
	 * for the key_updated message to be received
	 * Default is TWO Seconds - 2000
	 * @return the time to wait in milliseconds
	 */
	virtual uint32_t keyProcessTimeout() const { return TIMEOUT_SECONDS; }

	/**
	 * @brief Get the key ID
	 * @param keyID The key ID as a vector of binary data
	 */
	virtual void getKey(std::vector<uint8_t>& keyID) const = 0;

	/**
         * @brief Get the key IDs
         * @param keyIDs The map containing Key ID vector of binary data
         */
	virtual void getKeys(std::map<int, std::vector<uint8_t>>& keyIDs) const {}

	/**
	 * @brief Get the UUID
	 * @return the UUID
	 */
	virtual const std::string& getUuid() const { return mDrmInfo.systemUUID; }

	/**
	 * @brief Determines if decrypt should be called on clear samples
	 * @return Flag to indicate if should decrypt
	 */
	virtual bool isDecryptClearSamplesRequired() const { return mDrmInfo.bDecryptClearSamplesRequired; }

	/**
	 * @brief Determines if the DRM itself fetches the license or if PLAYER should use
	 * its own internal HTTP client to fetch the license
	 * Returning 'true' removes PLAYER calling generateLicenseRequest() on the CDM
	 * Default is to return false
	 * @return true if the DRM acquires the license, false if PLAYER should do it
	 */
	virtual bool isExternalLicense() const { return false; }

	/**
	 * @brief Generate the request details for the DRM license
	 * @param challengeInfo challenge information from the DRM system necessary to construct the license request
	 * @param licenseRequest license request data to populate
	 */
	virtual void generateLicenseRequest(const ChallengeInfo& challengeInfo, LicenseRequest& licenseRequest) const = 0;

	/**
	 * @brief Transform the license response from the server into the necessary format for OCDM
	 * @param licenseResponse license response from the server to transform
	 */
	virtual void transformLicenseResponse(std::shared_ptr<DrmData> licenseResponse) const {}

	/**
	 * @brief Get the memory system used to transform data for transmission
	 * @return the memory system, or null if to send it as is to the ocdm wrapper
	 */
	virtual DRMMemorySystem* getMemorySystem() { return nullptr; }

	/**
	 * @fn compare
	 * @return true if the two helpers can be considered the same, false otherwise
	 */
	virtual bool compare(DrmHelperPtr other);

	/**
	 * @brief Cancels a DRM session
	 */
	virtual void cancelDrmSession() { }

	/**
	 * @brief Checks if the helper can cancel a session, or if the caller should do it
	 * @return true if the helper can cancel
	 */
	virtual bool canCancelDrmSession() { return false; }

	/**
	 * @brief Gets the friendly display name of the DRM
	 * @return friendly name
	 */
	virtual const std::string& friendlyName() const { return EMPTY_STRING; }

	/**
	 * @brief Set Output protection flag for the drmHelper
	 * @return None
	 */
	void setOutputProtectionFlag(bool bValue) { bOutputProtectionEnabled = bValue;}

	/**
	 * @brief Get propogateUri Parameter
	 * @return Flag to get propagate Manifest uri params in DRM value
	 */
	virtual bool getPropogateUriParam() const { return mDrmInfo.bPropagateUriParams; }

public:
	virtual ~DrmHelper() {}

protected:
	uint32_t protectionScheme;
	const DrmInfo mDrmInfo;
	bool bOutputProtectionEnabled;
};

/**
 * @class DrmHelperFactory
 * @brief Helper class to  Maintain DRM data
 */

class DrmHelperFactory
{
public:
	/**
         * @brief Default weighting of a helper factory.
	 * Nominal scale of 0 to DEFAULT_WEIGHTING * 2
	 * Larger weights have lower priority
	 */
	static const int DEFAULT_WEIGHTING = 50;

	/**
	 * @brief Determines if a helper class provides the identified DRM
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if this helper provides that DRM
	 */
	virtual bool isDRM(const struct DrmInfo& drmInfo) const = 0;

	/**
	 * @brief Build a helper class to support the identified DRM
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return the helper
	 */
	virtual DrmHelperPtr createHelper(const struct DrmInfo& drmInfo) const = 0;

	/**
	 * @brief Adds the system IDs supported by the DRM to a vector
	 * Used by the GStreamer plugins to advertise the DRM upstream to the pipeline
	 * @param systemIds the vector to use
	 */
	virtual void appendSystemId(std::vector<std::string>& systemIds) const = 0;

	/**
	 * @brief Get the weighting for this helper factory, which determines its priority
	 * @return weighting value
	 */
	int getWeighting() { return mWeighting; }

	virtual ~DrmHelperFactory() {}

protected:
	DrmHelperFactory(int weighting = DEFAULT_WEIGHTING);
	int mWeighting;
};


/**
 * @class DrmHelperEngine
 * @brief Helper Engine for Player DRM operations
 */
class DrmHelperEngine
{
private:
	std::vector<DrmHelperFactory* > factories;

public:
	/**
	 * @brief DrmHelperEngine constructor
	 */
	DrmHelperEngine() : factories() {}
	/**
	 * @fn hasDRM
	 * @param systemId the UUID from the PSSH or manifest
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM was found, false otherwise
	 */
	bool hasDRM(const struct DrmInfo& drmInfo) const;

	/**
	 * @fn createHelper
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return the helper
	 */
	DrmHelperPtr createHelper(const struct DrmInfo& drmInfo) const;

	/**
	 * @fn getSystemIds
	 * @param ids vector to populate with supported IDs
	 */
	void getSystemIds(std::vector<std::string>& ids) const;

	/**
	 * @fn getInstance
	 * @return DRM Helper Engine instance
	 */
	static DrmHelperEngine& getInstance();

	/**
	 * @fn registerFactory
	 * @param factory helper factory instance to register
	 */
	void registerFactory(DrmHelperFactory* factory);
};

#endif //_DRM_HELPER_H
