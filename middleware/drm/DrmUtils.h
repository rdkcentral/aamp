/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
* @file DrmUtils.h
* @brief Data structures to help with DRM sessions.
*/

#ifndef DrmUtils_h
#define DrmUtils_h

#include <atomic>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <mutex>
#include <memory>

#include "DrmMediaFormat.h"
#include "DrmData.h"
#include "DrmInfo.h"
#include "DrmSystems.h"

/**
 * @brief Macros to track the value of API success or failure
 */
#define DRM_API_SUCCESS (0)
#define DRM_API_FAILED  (-1)

/**
 * @brief start and end tags for DRM policy
 */
#define DRM_METADATA_TAG_START "<ckm:policy xmlns:ckm=\"urn:ccp:ckm\">"
#define DRM_METADATA_TAG_END "</ckm:policy>"

#ifdef USE_SECMANAGER
#define PLAYER_SECMGR_INVALID_SESSION_ID (-1)

class AampSecManager;

/**
 * @brief Represents an player sec manager session, 
 * Sessions are automatically closed there are no AampSecManagerSession objects that reference it*/
class AampSecManagerSession
{	
	/* The coupling between AampSecManager & AampSecManagerSession is not ideal from an architecture standpoint but
	 * it minimises changes to existing AampSecManager code:
	 * ~SessionManager() calls AampSecManager::ReleaseSession()
	 * AampSecManager::acquireLicence() creates instances of AampSecManagerSession*/
	friend AampSecManager;
private:
	/**
	 * @brief Responsible for closing the corresponding sec manager sessions when it is no longer used
	 */
	class SessionManager
	{
		private:
		int64_t mID;	//set once undermutex in constructor
		std::atomic<std::size_t> mInputSummaryHash;	//can be changed by setInputSummaryHash
		SessionManager(int64_t sessionID, std::size_t inputSummaryHash);

		public:
		/**
		 * @fn getInstance
		 * @brief
		 * Get a shared pointer to an object corresponding to the sessionID, creating a new one if required
		*/
		static std::shared_ptr<AampSecManagerSession::SessionManager> getInstance(int64_t sessionID, std::size_t inputSummaryHash);

		int64_t getID(){return mID;}
		std::size_t getInputSummaryHash(){return mInputSummaryHash.load();}
		void setInputSummaryHash(std::size_t inputSummaryHash);

		//calls AampSecManager::ReleaseSession() on mID
		~SessionManager();
	};

	std::shared_ptr<AampSecManagerSession::SessionManager> mpSessionManager;
	mutable std::mutex sessionIdMutex;

	/**
 	* @brief constructor for valid objects
	* this will cause AampSecManager::ReleaseSession() to be called on sessionID
	* when the last AampSecManagerSession, referencing is destroyed
	* this is only intended to be used in AampSecManager::acquireLicence()
	* it is the responsibility of AampSecManager::acquireLicence() to ensure sessionID is valid
	*/
	AampSecManagerSession(int64_t sessionID, std::size_t inputSummaryHash);
public:
	/**
 	* @brief constructor for an invalid object*/
	AampSecManagerSession(): mpSessionManager(), sessionIdMutex() {};

	//allow copying, the secManager session will only be closed when all copies have gone out of scope
	AampSecManagerSession(const AampSecManagerSession& other): mpSessionManager(), sessionIdMutex()
	{
		std::lock(sessionIdMutex, other.sessionIdMutex);
		std::lock_guard<std::mutex> thisLock(sessionIdMutex, std::adopt_lock);
		std::lock_guard<std::mutex> otherLock(other.sessionIdMutex, std::adopt_lock);
		mpSessionManager=other.mpSessionManager;
	}
	AampSecManagerSession& operator=(const AampSecManagerSession& other)
	{
		std::lock(sessionIdMutex, other.sessionIdMutex);
		std::lock_guard<std::mutex> thisLock(sessionIdMutex, std::adopt_lock);
		std::lock_guard<std::mutex> otherLock(other.sessionIdMutex, std::adopt_lock);
		mpSessionManager=other.mpSessionManager;
		return *this;
	}

	/**
	 * @fn getSessionID
	 * @brief
	 * returns the session ID value for use with JSON API
	 * The returned value should not be used outside the lifetime of
	 * the AampSecManagerSession on which this method is called
	 * otherwise the session may be closed before the ID can be used
	 */
	int64_t getSessionID(void) const;
	std::size_t getInputSummaryHash();

	bool isSessionValid(void) const
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		return (mpSessionManager.use_count()!=0);
	}
	void setSessionInvalid(void)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		mpSessionManager.reset();
	}

	std::string ToString()
	{
		std::stringstream ss;
		ss<<"Session ";
		auto id = getSessionID();	//ID retrieved under mutex
		if(id != PLAYER_SECMGR_INVALID_SESSION_ID)
		{
			ss<<id<<" valid";
		}
		else
		{
			ss<<"invalid";
		}
		return ss.str();
	}
};
#endif

typedef enum
{
	MW_DRM_INIT_FAILED,			   /**< DRM initialization failure */
	MW_DRM_DATA_BIND_FAILED,			   /**< InitData binding with DRM failed */
	MW_DRM_SESSIONID_EMPTY,			   /**< DRM session ID empty */
	MW_DRM_CHALLENGE_FAILED,			   /**< DRM key request challenge generation failed */
	MW_INVALID_DRM_KEY,			   /**< DRM reporting invalid license key */
	MW_CORRUPT_DRM_DATA,			   /**< DRM failure due to corrupt drm data, self heal might clear further errors*/
	MW_CORRUPT_DRM_METADATA,			   /**< DRM failure due to corrupt drm metadata in the stream*/
	MW_DRM_DECRYPT_FAILED,			   /**< DRM Decryption Failed for Fragments */
	MW_DRM_UNSUPPORTED,			   /**< DRM Format Unsupported */
	MW_DRM_SELF_ABORT,			   /**< Download activity is aborted by player */
	MW_DRM_KEY_UPDATE_FAILED,		   /**< Failed to process DRM key, see the error code returned from Update() for more info */
	MW_UNTRACKED_DRM_ERROR,
	MW_FAILED_TO_GET_KEYID			   /**< Failed to parse key id from init data*/
} DrmTuneFailure;
namespace DrmUtils
{
	/**
	 *  @brief	Convert endianness of 16 byte block.
	 *
	 *  @param[in]	original - Pointer to source byte block.
	 *  @param[out]	guidBytes - Pointer to destination byte block.
	 *  @return	void.
	 */
	void convertEndianness(unsigned char *original, unsigned char *guidBytes);
	/**
	 *  @fn 	extractDataFromPssh
	 *  @param[in]	psshData - Pointer to PSSH data.
	 *  @param[in]	dataLength - Length of PSSH data.
	 *  @param[in]	startStr, endStr - Pointer to delimiter strings.
	 *  @param[in]  verStr - Pointer to version delimiter string.
	 *  @param[out]	len - Gets updated with length of content meta data.
	 *  @return	Extracted data between delimiters; NULL if not found.
	 */
	unsigned char *extractDataFromPssh(const char* psshData, int dataLength, const char* startStr, const char* endStr, int *len, const char* verStr);
	/**
	 *  @fn 	extractWVContentMetadataFromPssh
	 *  @param[in]	psshData - Pointer to PSSH data.
	 *  @param[in]  dataLength - pssh data length
	 *  @return	Extracted ContentMetaData.
	 */
	std::string extractWVContentMetadataFromPssh(const char* psshData, int dataLength);

	unsigned char * extractKeyIdFromPssh(const char* psshData, int dataLength, int *len, DRMSystems drmSystem);
}
#endif
