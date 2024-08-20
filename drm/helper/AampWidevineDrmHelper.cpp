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
 * @file AampWidevineDrmHelper.cpp
 * @brief Handles the Widevine DRM helper functions
 */

#include <memory>
#include <iostream>

#include "AampWidevineDrmHelper.h"
#include "AampDRMutils.h"
#include "AampConfig.h"
#include "AampConstants.h"

static AampWidevineDrmHelperFactory widevine_helper_factory;

const std::string AampWidevineDrmHelper::WIDEVINE_OCDM_ID = "com.widevine.alpha";

#define READ_U32(buf) \
	((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3]; buf+=4;

static long ParseMultiInt( const unsigned char **ppData, const unsigned char *fin )
{
	const unsigned char *psshData = *ppData;
	long iVal = 0;
	int shift = 0;
	while( psshData<fin )
	{
		int code = *psshData++;
		iVal |= (code&0x7f)<<shift;
		if( !(code&0x80) )
		{
			break;
		}
		shift += 7;
	}
	*ppData = psshData;
	return iVal;
}
/**
 https://www.w3.org/TR/2014/WD-encrypted-media-20140828/cenc-format.html
 https://tools.axinom.com/generators/PsshBox
*/
bool AampWidevineDrmHelper::parsePssh( const uint8_t* psshData, uint32_t psshSize )
{
	bool rc = false;
	uint32_t kidCount = 0;
	mInitData.assign(psshData, psshData+psshSize);
	const uint8_t *fin = &psshData[psshSize];
	uint32_t boxSize = READ_U32(psshData);
	if( boxSize != psshSize  )
	{
		AAMPLOG_ERR( "unexpected boxSize %d expected %d", boxSize, psshSize );
	}
	else
	{
		uint32_t boxType = READ_U32(psshData);
		if( boxType!='pssh' )
		{
			AAMPLOG_ERR( "unexpected boxType %d", boxType );
		}
		else
		{
			uint32_t versionAndFlags = READ_U32(psshData);
			static const uint8_t systemID_WideVine[16] =
			{
				0xed,0xef,0x8b,0xa9,0x79,0xd6,0x4a,0xce,0xa3,0xc8,0x27,0xdc,0xd5,0x1d,0x21,0xed
			};
			if( memcmp(psshData,systemID_WideVine,sizeof(systemID_WideVine))!=0 )
			{
				AAMPLOG_ERR( "unexpected systemID" );
			}
			else
			{
				psshData += sizeof(systemID_WideVine);
				uint8_t psshDataVer = versionAndFlags>>24;
				if( psshDataVer == 0 )
				{
					uint32_t sz = READ_U32(psshData);
					if( fin - psshData != sz )
					{
						AAMPLOG_ERR( "unexpected size %d expected %d", sz, (int)(fin-psshData) );
					}
					long iVal;
					while( psshData<fin )
					{
						uint8_t fieldType = *psshData++;
						switch( fieldType )
						{
							case 0x38: // Crypto Period Index (deprecated)
							case 0x50: // Crypto Period Duration (deprecated)
							case 0x08: // Algorithm (deprecated)
								iVal = ParseMultiInt( &psshData, fin );
								AAMPLOG_WARN( "%02x: %ld", fieldType, iVal );
								break;

							case 0x48: // protection scheme
								protectionScheme = (uint32_t)ParseMultiInt( &psshData, fin );
								AAMPLOG_WARN( "%02x: %08x", fieldType, protectionScheme );
								break;
								
							case 0x22: // Content ID - important: some streams have contentid, but no keyid(s)
							case 0x12: // Key ID
							{
								int fieldSize = *psshData++;
								if( fieldSize>0 && &psshData[fieldSize] <= fin )
								{	std::vector<uint8_t> keyId;
									keyId.assign( psshData, &psshData[fieldSize] );
									mKeyIDs[kidCount++] = keyId;
									rc = true;
								}
								psshData += fieldSize;
							}
								break;
										
							case 0x32: // Policy (deprecated)
							case 0x2a: // Track Type (deprecated)
							case 0x1a: // Provider (deprecated)
							{
								int fieldSize = *psshData++;
								AAMPLOG_WARN( "0x%02x: '%.*s'\n", (int)fieldType, fieldSize, psshData );
								psshData += fieldSize;
							}
								break;
								
							default:
								// unknown
								break;
						}
					}
				}
				else if( psshDataVer == 1 )
				{
					kidCount = READ_U32(psshData);
					uint8_t fieldSize = 16;
					for( int i=0; i<kidCount; i++ )
					{
						std::vector<uint8_t> keyId;
						keyId.assign( psshData, &psshData[fieldSize] );
						mKeyIDs[i]=keyId;
						psshData += fieldSize;
						rc = true;
					}
				}
				else
				{
					AAMPLOG_ERR("unsupported PSSH version: %u", psshDataVer);
				}
			}
		}
	}
	return rc;
}

void AampWidevineDrmHelper::setDrmMetaData(const std::string& metaData)
{
	mContentMetadata = metaData;
}

void AampWidevineDrmHelper::setDefaultKeyID(const std::string& cencData)
{
	std::vector<uint8_t> defaultKeyID(cencData.begin(), cencData.end());
	if(!mKeyIDs.empty())
	{
		for(auto& it : mKeyIDs)
		{
			if(defaultKeyID == it.second)
			{
				mDefaultKeySlot = it.first;
				AAMPLOG_WARN("setDefaultKeyID : %s slot : %d", cencData.c_str(), mDefaultKeySlot);
			}
		}
	}
}


const std::string& AampWidevineDrmHelper::ocdmSystemId() const
{
	return WIDEVINE_OCDM_ID;
}

void AampWidevineDrmHelper::createInitData(std::vector<uint8_t>& initData) const
{
	initData = this->mInitData;
}

void AampWidevineDrmHelper::getKey(std::vector<uint8_t>& keyID) const
{
	AAMPLOG_WARN("AampWidevineDrmHelper::getKey defaultkey: %d mKeyIDs.size:%zu", mDefaultKeySlot, mKeyIDs.size());
	if ((mDefaultKeySlot >= 0) && (mDefaultKeySlot < mKeyIDs.size()))
	{
		keyID = this->mKeyIDs.at(mDefaultKeySlot);
	}
	else if (mKeyIDs.size() > 0)
	{
		keyID = this->mKeyIDs.at(0);
	}
	else
	{
		AAMPLOG_ERR("No key");
	}
}

void AampWidevineDrmHelper::getKeys(std::map<int, std::vector<uint8_t>>& keyIDs) const
{
	keyIDs = this->mKeyIDs;
}

void AampWidevineDrmHelper::generateLicenseRequest(const AampChallengeInfo& challengeInfo, AampLicenseRequest& licenseRequest) const
{
	licenseRequest.method = AampLicenseRequest::POST;

	if (licenseRequest.url.empty())
	{
		licenseRequest.url = challengeInfo.url;
	}

	licenseRequest.headers = {{"Content-Type:", {"application/octet-stream"}}};

	licenseRequest.payload= challengeInfo.data->getData();
}

bool AampWidevineDrmHelperFactory::isDRM(const struct DrmInfo& drmInfo) const
{
	return (((WIDEVINE_UUID == drmInfo.systemUUID) || (AampWidevineDrmHelper::WIDEVINE_OCDM_ID == drmInfo.keyFormat))
		&& ((drmInfo.mediaFormat == eMEDIAFORMAT_DASH) || (drmInfo.mediaFormat == eMEDIAFORMAT_HLS_MP4))
		);
}

std::shared_ptr<AampDrmHelper> AampWidevineDrmHelperFactory::createHelper(const struct DrmInfo& drmInfo) const
{
	if (isDRM(drmInfo))
	{
		return std::make_shared<AampWidevineDrmHelper>(drmInfo);
	}
	return NULL;
}

void AampWidevineDrmHelperFactory::appendSystemId(std::vector<std::string>& systemIds) const
{
	systemIds.push_back(WIDEVINE_UUID);
}
