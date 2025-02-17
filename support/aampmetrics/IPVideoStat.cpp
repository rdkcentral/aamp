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
 * @file VideoStat.cpp
 * @brief VideoStat data packing and storage 
 */
bool g_ForPartnerApps = false;

#include "IPVideoStat.h"

#include <iostream>
#include <algorithm>


std::map<VideoStatTrackType, std::string> Track::StatTrackTypeStr;

/**
 *   @brief Returns string of JSON object
 */
char * CVideoStat::ToJsonString(const char* additionalData, bool forPA) const
{
	//update the g_ForPartnerApps global variable
	g_ForPartnerApps = forPA;
	
	//Updated track names with respect to g_ForPartnerApps
	Track::setTrackNames();
	
	char * strRet = NULL;
	cJSON *monitor = cJSON_CreateObject();
	if(monitor)
	{

		cJSON * jsonObj = NULL;

		if(mTmeToTopProfile > 0 )
		{
			jsonObj =  cJSON_CreateNumber(mTmeToTopProfile);
			cJSON_AddItemToObject(monitor, TAG_TIME_TO_TOP, jsonObj);
		}

		if(mTimeAtTopProfile > 0)
		{
			jsonObj =  cJSON_CreateNumber(mTimeAtTopProfile);
			cJSON_AddItemToObject(monitor, TAG_TIME_AT_TOP, jsonObj);
		}

		if(mTotalVideoDuration > 0 )
		{
			jsonObj =  cJSON_CreateNumber(mTotalVideoDuration);
			cJSON_AddItemToObject(monitor, TAG_TIME_PLAYBACK_DURATION, jsonObj);
		}

		if(mTotalVideoDuration >0 )
		{
			jsonObj =  cJSON_CreateNumber(mNetworkDropCount);
			cJSON_AddItemToObject(monitor, TAG_NET_DROP, jsonObj);
		}

		if(mErrorDropCount > 0)
		{
			jsonObj =  cJSON_CreateNumber(mErrorDropCount);
			cJSON_AddItemToObject(monitor, TAG_ERR_DROP, jsonObj);
		}

		if(mRateCorrectionCount > 0)
		{
			jsonObj =  cJSON_CreateNumber(mRateCorrectionCount);
                        cJSON_AddItemToObject(monitor, TAG_RATE_CORRECTION, jsonObj);
		}

		if(mDisplayWidth != 0)
		{
			jsonObj =  cJSON_CreateNumber(mDisplayWidth);
			cJSON_AddItemToObject(monitor, TAG_DISPLAY_WIDTH, jsonObj);
		}
		if(mDisplayHeight != 0)
		{
			jsonObj =  cJSON_CreateNumber(mDisplayHeight);
			cJSON_AddItemToObject(monitor, TAG_DISPLAY_HEIGHT, jsonObj);
		}
		if(mDisplayWidth != 0 && mDisplayHeight != 0)
		{
			jsonObj =  cJSON_CreateNumber(mbProfileCapped);
			cJSON_AddItemToObject(monitor, TAG_PROFILE_CAPPING, jsonObj);
		}
		//Applicable only for playback stats
		if(g_ForPartnerApps)
		{
			cJSON * jsonObj = cJSON_CreateString(mMediaFormat.c_str());
			if(jsonObj) cJSON_AddItemToObject(monitor, TAG_MEDIA_TYPE, jsonObj);
			
			
			jsonObj = cJSON_CreateString(mPlaybackMode.c_str());
			if(jsonObj) cJSON_AddItemToObject(monitor, TAG_PLAY_MODE, jsonObj);
			
			
			if(mPlaybackMode == "LINEAR_TV" ||
				mPlaybackMode == "IVOD" ||
				mPlaybackMode == "SLE")
			{
				jsonObj = cJSON_CreateNumber(mLiveLatency);
				if(jsonObj) cJSON_AddItemToObject(monitor, TAG_LIVE_LATENCY, jsonObj);
			}
			
			jsonObj = cJSON_CreateNumber( CSessionSummary::totalErrorCount);
			if(jsonObj) cJSON_AddItemToObject(monitor, TAG_TOTAL_ERROR, jsonObj);

			if("DASH" == mMediaFormat)
			{
				jsonObj = cJSON_CreateNumber( ManifestGenericStats::totalGaps);
				if(jsonObj) cJSON_AddItemToObject(monitor, TAG_NUM_GAPS,jsonObj);
			}			
		}
		bool isDataAdded = false;

		cJSON *langList = cJSON_CreateObject();
		for (auto const& langItem : mMapLang)
		{
			std::string value =langItem.first.toString();
			if(!value.empty() && !langItem.second.empty())
			{
				isDataAdded = true;
				cJSON *strObj = cJSON_CreateString(langItem.second.c_str());
				cJSON_AddItemToObject(langList, value.c_str(), strObj);
			}
		}

		if(isDataAdded)
		{
			cJSON_AddItemToObject(monitor, TAG_SUPPORTED_LANG, langList);
		}
		else
		{
			cJSON_Delete(langList);
		}

		cJSON *monitor_json = cJSON_Parse(additionalData);

		for (auto const& mapProfileInfo : mMapStreamInfo)
		{
			cJSON *profiles = cJSON_CreateObject();
			jsonObj = NULL;

			isDataAdded = false;

			if(!additionalData)
			{
				for (auto const& profileInfo : mapProfileInfo.second)
				{

					jsonObj = profileInfo.second.ToJson();
					if(jsonObj)
					{
						std::string profileIndex = std::to_string(profileInfo.first);

						cJSON_AddItemToObject(profiles, profileIndex.c_str(), jsonObj);
						isDataAdded = true;
					}

				}
			}
			else
			{
				if (monitor_json)
				{
					cJSON *child = monitor_json->child;
					// iterate on stream info
					while(child)
					{
						if(!strcmp(child->string, mapProfileInfo.first.toString().c_str()))
						{
							// Found appropriate stream info
							cJSON *childProfile = child->child;
							// iterate on child profiles and add it to profile stat
							if(childProfile)
							{
								cJSON* childBitrates = childProfile->child;
								while(childBitrates)
								{
									// data collection
									// Add fog data one by one to aamp profiles.
									cJSON_AddItemReferenceToObject(profiles, childBitrates->string, childBitrates);
									isDataAdded = true;
									childBitrates = childBitrates->next;
								}
							}
						}
						child = child->next;
					}
				}
			}

			if(isDataAdded) // at least one profile added to profiles
			{

				cJSON * trackJson  = cJSON_CreateObject();

				cJSON_AddItemToObject(trackJson, TAG_PROFILES, profiles);

				cJSON * licenceJson = NULL;

				auto  it = mMapLicenseInfo.find(mapProfileInfo.first);
				if (it != mMapLicenseInfo.end())
				{
					licenceJson = (*it).second.ToJson();
				}

				if(licenceJson)
				{
					cJSON_AddItemToObject(trackJson, TAG_LICENSE_STAT, licenceJson);
				}

				cJSON_AddItemToObject(monitor, mapProfileInfo.first.toString().c_str(), trackJson);
			}
			else
			{
				cJSON_Delete(profiles);
			}
		}

		// code for ad values to JSON

		// monitor->child will be NULL if none of above data is added to monitor
		if(monitor->child)
		{
			jsonObj = cJSON_CreateString(VIDEO_END_DATA_VERSION);
			cJSON_AddItemToObject(monitor, TAG_VERSION, jsonObj);
			
			if(g_ForPartnerApps)
			{
				time_t     now = time(0);
				struct tm  tstruct;
				char       buf[80];
				tstruct = *gmtime(&now);
				strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
				
				jsonObj = cJSON_CreateString(buf);
				cJSON_AddItemToObject(monitor, TAG_REPORT_TIME, jsonObj);
			}
			
			if(mbTsb)
			{
				jsonObj =  cJSON_CreateNumber(1);
				cJSON_AddItemToObject(monitor, TAG_TSB_AVAILABILITY, jsonObj);
			}

			strRet = cJSON_PrintUnformatted(monitor);
		}

		cJSON_Delete(monitor);
		if (monitor_json)
		{
			cJSON_Delete(monitor_json);
		}
	}
	return strRet;
}


/**
 *   @brief Increment Normal Fragment stats
 *
 */
void CVideoStat::Increment_Fragment_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity)
{
	if(track.type != STAT_MAIN) // fragment stats are not applicable for main hls or dash manifest
	{
		//MapProfileInfo mapProfileInfo = mMapStreamInfo[eType];
		CProfileInfo * pinfo = &(mMapStreamInfo[track][bitrate]);
		pinfo->GetFragmentStat()->GetNormalFragmentStat()->IncrementCount(downloadTimeMs, response, connectivity);
	}
}

/**
 *   @brief Increment Init Fragment stats ( used for dash case only )
 *
 *   @param[in] Track track - Indicates track for which Increment required
 *   @param[in] bitrate : profile bitrate
 *   @param[in] download time - download time
 *   @param[in] response - HTTP/CURL response
 *   @param[in] bool - connection status flag
 *   @return None
 */
void CVideoStat::Increment_Init_Fragment_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity)
{
	if(track.type != STAT_MAIN) // fragment stats are not applicable for main hls or dash manifest
	{
		CProfileInfo * pinfo = &(mMapStreamInfo[track][bitrate]);
		pinfo->GetFragmentStat()->GetInitFragmentStat()->IncrementCount(downloadTimeMs, response, connectivity);
	}
}

/**
 *   @brief Increment Manifest stats
 *
 */
void CVideoStat::Increment_Manifest_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity, ManifestData * manifestData)
{
	CProfileInfo * pinfo = &(mMapStreamInfo[track][bitrate]);
	pinfo->GetManifestStat()->IncrementCount(downloadTimeMs, response, connectivity, manifestData);
}


/**
 *   @brief   Records License stat based on isEncrypted
 */
void CVideoStat::Record_License_EncryptionStat(VideoStatTrackType eType, bool isEncrypted, bool isKeyChanged, int audioIndex)
{
	mMapLicenseInfo[std::move(Track(eType, audioIndex))].Record_License_EncryptionStat(isEncrypted,isKeyChanged);
}

/**
 *   @brief Sets URL for failed download fragments
 */
void CVideoStat::SetFailedFragmentUrl(VideoStatTrackType eType, long bitrate, std::string url, int audioIndex)
{
	if(eType != STAT_MAIN) // fragment stats are not applicable for main hls or dash manifest
	{
		CProfileInfo * pinfo = &(mMapStreamInfo[std::move(Track(eType,audioIndex))][bitrate]);
		pinfo->GetFragmentStat()->SetUrl(url);
	}
}

/**
 *   @brief sets Lang associated with Audio Tracks
 *
 */
void CVideoStat::Setlanguage(VideoStatTrackType eType, std::string strLang, int audioIndex)
{
	if(eType == VideoStatTrackType::STAT_AUDIO)
	{
		this->mMapLang[std::move(Track(eType,audioIndex))] = strLang;
	}
}


/**
 *   @brief Increment stats ,
 *
 */
void CVideoStat::Increment_Data(VideoStatDataType dataType,VideoStatTrackType eType, long bitrate , long downloadTimeMs, int response, bool connectivity, int audioIndex, ManifestData * manifestData)
{
	switch(dataType)
	{
		case VE_DATA_MANIFEST:
			{
				Increment_Manifest_Count(std::move(Track(eType, audioIndex)),bitrate,downloadTimeMs,response,connectivity, manifestData);
			}
			break;
		case VE_DATA_FRAGMENT:
			{
				Increment_Fragment_Count(std::move(Track(eType, audioIndex)),bitrate,downloadTimeMs,response,connectivity);
			}
			break;
		case VE_DATA_INIT_FRAGMENT:
			{
				Increment_Init_Fragment_Count(std::move(Track(eType, audioIndex)),bitrate,downloadTimeMs,response,connectivity);
			}
			break;
		default:
			break;
	}
}

/**
 *   @brief Sets profile frame size
 */
void CVideoStat::SetProfileResolution(VideoStatTrackType eType, long bitrate, int width, int height, int audioIndex)
{
	if(eType != STAT_MAIN) // fragment stats are not applicable for main hls or dash manifest
	{
		CProfileInfo * pinfo = &(mMapStreamInfo[std::move(Track(eType,audioIndex))][bitrate]);
		pinfo->SetSize(width,height);
	}
}

/**
 *   @brief Sets Display frame size
 */
void CVideoStat::SetDisplayResolution(int width, int height)
{
	mDisplayWidth = width;
	mDisplayHeight = height;
}


/**
*   @brief increment gaps count between periods
*/
void CVideoStat::IncrementGaps()
{
	ManifestGenericStats::totalGaps++;
}
