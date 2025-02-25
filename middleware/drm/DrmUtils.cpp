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
 * @file DrmUtils.cpp
 * @brief DataStructures and methods for DRM license acquisition
 */

#include "DrmUtils.h"
#include "PlayerLogManager.h"

#include <uuid/uuid.h>
#include <regex>
#ifdef USE_SECMANAGER
#include "AampSecManager.h"



std::shared_ptr<AampSecManagerSession::SessionManager> AampSecManagerSession::SessionManager::getInstance(int64_t sessionID, std::size_t inputSummaryHash)
{
	std::shared_ptr<SessionManager> returnValue;

	static std::mutex instancesMutex;
	std::lock_guard<std::mutex> lock{instancesMutex};
	static std::map<int64_t, std::weak_ptr<SessionManager>> instances;

	//Remove pointers to expired instances
	{
		std::vector<int64_t> keysToRemove;
		for (auto i : instances)
		{
			if(i.second.expired())
			{
				keysToRemove.push_back(i.first);
			}
		}
		if(keysToRemove.size())
		{
			std::stringstream ss;
			ss<<"AampSecManagerSession: "<<keysToRemove.size()<<" expired (";
			for(auto key:keysToRemove)
			{
				ss<<key<<",";
				instances.erase(key);
			}

			ss<<"), instances remaining."<< instances.size();
			MW_LOG_MIL("%s",ss.str().c_str());
		}
	}

	/* Only create or retrieve instances for valid sessionIDs.
	 * <0 is used as an invalid value e.g. PLAYER_SECMGR_INVALID_SESSION_ID
	 * 0 removes all sessions which is not the intended behavior here*/
	if(sessionID>0)
	{
		if(instances.count(sessionID)>0)
		{
			//get an existing pointer which may be no longer valid
			returnValue = instances[sessionID].lock();

			if(!returnValue)
			{
				//unexpected
				MW_LOG_WARN("AampSecManagerSession: session ID %lld reused or session closed too early.",
				sessionID);
			}
		}

		if(returnValue)
		{
			if(returnValue->getInputSummaryHash()!=inputSummaryHash)
			{
				//this should only occur after a successful updatePlaybackSession
				MW_LOG_MIL("AampSecManagerSession: session ID %lld input data changed.", sessionID);
				returnValue->setInputSummaryHash(inputSummaryHash);
			}
		}
		else
		{
			/* where an existing, valid instance is not available for sessionID
			* create a new instance & save a pointer to it for possible future reuse*/
			returnValue.reset(new SessionManager{sessionID, inputSummaryHash});
			instances[sessionID] = returnValue;
			MW_LOG_WARN("AampSecManagerSession: new instance created for ID:%lld, %d instances total.",
			sessionID,
			instances.size());
		}
	}
	else
	{
		MW_LOG_WARN("AampSecManagerSession: invalid ID:%lld.", sessionID);
	}

	return returnValue;
}

AampSecManagerSession::SessionManager::~SessionManager()
{
	if(mID>0)
	{
		AampSecManager::GetInstance()->ReleaseSession(mID);
	}
}
void AampSecManagerSession::SessionManager::setInputSummaryHash(std::size_t inputSummaryHash)
{
	mInputSummaryHash=inputSummaryHash;
	std::stringstream ss;
	ss<<"Input summary hash updated to: "<<inputSummaryHash << "for ID "<<mID;
	MW_LOG_MIL("%s", ss.str().c_str());
}


AampSecManagerSession::SessionManager::SessionManager(int64_t sessionID, std::size_t inputSummaryHash):
mID(sessionID),
mInputSummaryHash(inputSummaryHash)
{};

AampSecManagerSession::AampSecManagerSession(int64_t sessionID, std::size_t inputSummaryHash):
mpSessionManager(AampSecManagerSession::SessionManager::getInstance(sessionID, inputSummaryHash)),
sessionIdMutex()
{};

int64_t AampSecManagerSession::getSessionID(void) const
{
	std::lock_guard<std::mutex>lock(sessionIdMutex);
	int64_t ID = PLAYER_SECMGR_INVALID_SESSION_ID;
	if(mpSessionManager)
	{
		ID = mpSessionManager->getID();
	}

	return ID;
}

std::size_t AampSecManagerSession::getInputSummaryHash()
{
	std::lock_guard<std::mutex>lock(sessionIdMutex);
	std::size_t hash=0;
	if(mpSessionManager)
	{
		hash = mpSessionManager->getInputSummaryHash();
	}

	return hash;
}
#endif

#define KEYID_TAG_START "<KID>"
#define KEYID_TAG_END "</KID>"

/* Regex to detect if a string starts with a protocol definition e.g. http:// */
static const std::string PROTOCOL_REGEX = "^[a-zA-Z0-9\\+\\.-]+://";
//#define KEY_ID_SIZE_INDICATOR 0x12

/**
 *  @brief  Default constructor for DrmData.
 *          NULL initialize data and dataLength.
 */
DrmData::DrmData() : data("")
{
}

/**
 *  @brief Constructor for DrmData
 *         allocate memory and initialize data and
 *         dataLength with given params.
 *
 */
DrmData::DrmData(const char *dataPtr, size_t dataLength) : data("")
{
		data.assign(dataPtr,dataLength);
}

/**
 *  @brief   Distructor for DrmData.
 *           Free memory (if any) allocated for data.
 */
DrmData::~DrmData()
{
	if(!data.empty())
	{
		data.clear();
	}
}

/**
 *  @brief Getter method for data.
 *
 */
const std::string &DrmData::getData()
{
	return data;
}

/**
 *  @brief  Getter method for dataLength.
 */
size_t DrmData::getDataLength()
{
	return data.length();
}

/**
 *  @brief Updates DrmData with given data.
 */
void DrmData::setData(const char *dataPtr, size_t dataLength)
{
	if(!data.empty())
	{
		data.clear();
	}
	data.assign(dataPtr,dataLength);
}

/**
 *  @brief  Appends DrmData with given data.
 */
void DrmData::addData(const char *dataPtr, size_t dataLength)
{
	if(data.empty())
	{
		setData(dataPtr,dataLength);
	}
	else
	{
		std::string key;
		key.assign(dataPtr,dataLength);
		data += key;
	}
}

/**
 *  @brief	Swap the bytes at given positions.
 *
 *  @param[out]	bytes - Pointer to byte block where swapping is done.
 *  @param[in]	pos1, pos2 - Swap positions.
 *  @return		void.
 */
static void swapBytes(unsigned char *bytes, int pos1, int pos2)
{
	unsigned char temp = bytes[pos1];
	bytes[pos1] = bytes[pos2];
	bytes[pos2] = temp;
}

/**
 * @brief Convert endianness of 16 byte block. 
 */
void DrmUtils::convertEndianness(unsigned char *original, unsigned char *guidBytes)
{
	memcpy(guidBytes, original, 16);
	swapBytes(guidBytes, 0, 3);
	swapBytes(guidBytes, 1, 2);
	swapBytes(guidBytes, 4, 5);
	swapBytes(guidBytes, 6, 7);
}

/**
 *  @brief  Extract WideVine content meta data from DRM
 *          Agnostic PSSH header. Might not work with WideVine PSSH header
 *
 */
std::string DrmUtils::extractWVContentMetadataFromPssh(const char* psshData, int dataLength)
{
	//WV PSSH format 4+4+4+16(system id)+4(data size)
	uint32_t header = 28;
	std::string metadata;
	uint32_t  content_id_size =
                    (uint32_t)((psshData[header] & 0x000000FFu) << 24 |
                               (psshData[header+1] & 0x000000FFu) << 16 |
                               (psshData[header+2] & 0x000000FFu) << 8 |
                               (psshData[header+3] & 0x000000FFu));

	MW_LOG_INFO("content meta data length  : %d",content_id_size);
	if ((header + 4 + content_id_size) <= dataLength)
	{
		metadata = std::string(psshData + header + 4, content_id_size);
	}
	else
	{
		MW_LOG_WARN("psshData : %d bytes in length, metadata would read past end of buffer", dataLength);
	}

	return metadata;
}
//End of special for Widevine
