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
 * @file fragmentcollector_mpd.cpp
 * @brief Fragment collector implementation of MPEG DASH
 */

#include "iso639map.h"
#include "fragmentcollector_mpd.h"
#include "AampStreamSinkManager.h"
#include "MediaStreamContext.h"
#include "priv_aamp.h"
#include "AampDRMLicManager.h"
#include "AampConstants.h"
#include "SubtecFactory.hpp"
#include "isobmffprocessor.h"
#include <stdlib.h>
#include <string.h>
#include "_base64.h"
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <set>
#include <iomanip>
#include <ctime>
#include <inttypes.h>
#include <libxml/xmlreader.h>
#include <math.h>
#include <cmath> // For double abs(double)
#include <algorithm>
#include <cctype>
#include <regex>
#include "AampCacheHandler.h"
#include "AampUtils.h"
#include "AampMPDUtils.h"
#include <chrono>
#include "AampTSBSessionManager.h"
//#define DEBUG_TIMELINE
#include "PlayerCCManager.h"

/**
 * @addtogroup AAMP_COMMON_TYPES
 */
#define SEGMENT_COUNT_FOR_ABR_CHECK 5
#define TIMELINE_START_RESET_DIFF 4000000000
#define MIN_TSB_BUFFER_DEPTH 6 //6 seconds from 4.3.3.2.2 in https://dashif.org/docs/DASH-IF-IOP-v4.2-clean.htm
#define VSS_DASH_EARLY_AVAILABLE_PERIOD_PREFIX "vss-"
#define FOG_INSERTED_PERIOD_ID_PREFIX "FogPeriod"
#define INVALID_VOD_DURATION  (0)

/**
 * Macros for extended audio codec check as per ETSI-TS-103-420-V1.2.1
 */
#define SUPPLEMENTAL_PROPERTY_TAG "SupplementalProperty"
#define SCHEME_ID_URI_EC3_EXT_CODEC "tag:dolby.com,2018:dash:EC3_ExtensionType:2018"
#define EC3_EXT_VALUE_AUDIO_ATMOS "JOC"

/**
 * @class HeaderFetchParams
 * @brief Holds information regarding initialization fragment
 */
class HeaderFetchParams
{
public:
	HeaderFetchParams() : context(NULL), pMediaStreamContext(NULL), initialization(""), fragmentduration(0),
		isInitialization(false), discontinuity(false)
	{
	}
	HeaderFetchParams(const HeaderFetchParams&) = delete;
	HeaderFetchParams& operator=(const HeaderFetchParams&) = delete;
	class StreamAbstractionAAMP_MPD *context;
	class MediaStreamContext *pMediaStreamContext;
	string initialization;
	double fragmentduration;
	bool isInitialization;
	bool discontinuity;
};

/**
 * @class FragmentDownloadParams
 * @brief Holds data of fragment to be downloaded
 */
class FragmentDownloadParams
{
public:
	class StreamAbstractionAAMP_MPD *context;
	class MediaStreamContext *pMediaStreamContext;
	bool playingLastPeriod;
	long long lastPlaylistUpdateMS;
};

static bool IsIframeTrack(IAdaptationSet *adaptationSet);


/**
 * @brief StreamAbstractionAAMP_MPD Constructor
 */
StreamAbstractionAAMP_MPD::StreamAbstractionAAMP_MPD(class PrivateInstanceAAMP *aamp, double seek_pos, float rate, id3_callback_t id3Handler)
	: StreamAbstractionAAMP(aamp, id3Handler),
	mLangList(), seekPosition(seek_pos), rate(rate), fragmentCollectorThreadID(),tsbReaderThreadID(),
	mpd(NULL), mNumberOfTracks(0), mCurrentPeriodIdx(0), mEndPosition(0), mIsLiveStream(true), mIsLiveManifest(true),mManifestDnldRespPtr(nullptr),mManifestUpdateHandleFlag(false), mUpdateManifestState(false),
	mStreamInfo(NULL), mPrevStartTimeSeconds(0), mPrevLastSegurlMedia(""), mPrevLastSegurlOffset(0),
	mPeriodEndTime(0), mPeriodStartTime(0), mPeriodDuration(0), mMinUpdateDurationMs(DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS),
	mLastPlaylistDownloadTimeMs(0), mFirstPTS(0), mStartTimeOfFirstPTS(0), mAudioType(eAUDIO_UNKNOWN),
	mPrevAdaptationSetCount(0), mBitrateIndexVector(), mProfileMaps(), mIsFogTSB(false),
	mCurrentPeriod(NULL), mBasePeriodId(""), mBasePeriodOffset(0), mCdaiObject(NULL), mLiveEndPosition(0), mCulledSeconds(0)
	,mAdPlayingFromCDN(false)
	,mMaxTSBBandwidth(0), mTSBDepth(0)
	,mVideoPosRemainder(0)
	,mPresentationOffsetDelay(0)
	,mUpdateStreamInfo(false)
	,mAvailabilityStartTime(0)
	,mFirstPeriodStartTime(0)
	,mDrmPrefs({{CLEARKEY_UUID, 1}, {WIDEVINE_UUID, 2}, {PLAYREADY_UUID, 3}})// Default values, may get changed due to config file
	,mCommonKeyDuration(0), mEarlyAvailablePeriodIds(), thumbnailtrack(), indexedTileInfo()
	,mMaxTracks(0)
	,mDeltaTime(0)
	,mHasServerUtcTime(false)
	,latencyMonitorThreadStarted(false),prevLatencyStatus(LATENCY_STATUS_UNKNOWN),latencyStatus(LATENCY_STATUS_UNKNOWN),latencyMonitorThreadID()
	,mStreamLock()
	,mProfileCount(0)
	,mIterPeriodIndex(0), mNumberOfPeriods(0)
	,playlistDownloaderThreadStarted(false)
	,mSubtitleParser()
	,mMultiVideoAdaptationPresent(false)
	,mLocalUtcTime(0)
	,prevTimeScale(0)
	,mMPDParseHelper(NULL)
	,mLowLatencyMode(false)
	,mABRMode(ABRMode::UNDEF)
	,mLastManifestFileSize(0)
	,mFirstVideoFragPTS(0)
	,mIsFcsRepresentation(false)
	,mFcsRepresentationId(-1)
	,mFcsSegments()
	,isVidDiscInitFragFail(false)
	,abortTsbReader(false)
	,mShortAdOffsetCalc(false)
	,mNextPts(0.0)
	,mPrevFirstPeriodStart(0.0f)
	,mTrackWorkers()
	,mAudioSurplus(0)
	,mVideoSurplus(0)
	,mIsSegmentTimelineEnabled(false)
	,mSeekedInPeriod(false)
	,mIsFinalFirstPTS(false)
{
	this->aamp = aamp;
	if (aamp->mDRMLicenseManager)
	{
		AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
		licenseManager->SetLicenseFetcher(this);
	}
	memset(&mMediaStreamContext, 0, sizeof(mMediaStreamContext));
	GetABRManager().clearProfiles();
	mLastPlaylistDownloadTimeMs = aamp_GetCurrentTimeMS();

	// setup DRM prefs from config
	int highestPref = 0;
	#if 0
	std::vector<std::string> values;
	if (gpGlobalConfig->getMatchingUnknownKeys("drm-preference.", values))
	{
		for(auto&& item : values)
		{
			int i = atoi(item.substr(item.find(".") + 1).c_str());
			mDrmPrefs[gpGlobalConfig->getUnknownValue(item)] = i;
			if (i > highestPref)
			{
				highestPref = i;
			}
		}
	}
	#endif

	// Get the highest number
	for (auto const& pair: mDrmPrefs)
	{
		if(pair.second > highestPref)
		{
			highestPref = pair.second;
		}
	}

	// Give preference based on GetPreferredDRM.
	switch (aamp->GetPreferredDRM())
	{
		case eDRM_WideVine:
		{
			AAMPLOG_INFO("DRM Selected: WideVine");
			mDrmPrefs[WIDEVINE_UUID] = highestPref+1;
		}
			break;

		case eDRM_ClearKey:
		{
			AAMPLOG_INFO("DRM Selected: ClearKey");
			mDrmPrefs[CLEARKEY_UUID] = highestPref+1;
		}
			break;

		case eDRM_PlayReady:
		default:
		{
			AAMPLOG_INFO("DRM Selected: PlayReady");
			mDrmPrefs[PLAYREADY_UUID] = highestPref+1;
		}
			break;
	}

	AAMPLOG_INFO("DRM prefs");
	for (auto const& pair: mDrmPrefs) {
		AAMPLOG_INFO("{ %s, %d }", pair.first.c_str(), pair.second);
	}

	trickplayMode = (rate != AAMP_NORMAL_PLAY_RATE);
}

/**
 * @brief Get Additional tag property value from any child node of MPD
 * @param nodePtr Pointer to MPD child node, Tag Name , Property Name,
 * 	  SchemeIdUri (if the property mapped against scheme Id , default value is empty)
 * @retval return the property name if found, if not found return empty string
 */
static bool IsAtmosAudio(const IMPDElement *nodePtr)
{
	bool isAtmos = false;

	if (!nodePtr){
		AAMPLOG_ERR("API Failed due to Invalid Arguments");
	}else{
		std::vector<INode*> childNodeList = nodePtr->GetAdditionalSubNodes();
		for (size_t j=0; j < childNodeList.size(); j++) {
			INode* childNode = childNodeList.at(j);
			const std::string& name = childNode->GetName();
			if (name == SUPPLEMENTAL_PROPERTY_TAG ) {
				if (childNode->HasAttribute("schemeIdUri")){
					const std::string& schemeIdUri = childNode->GetAttributeValue("schemeIdUri");
					if (schemeIdUri == SCHEME_ID_URI_EC3_EXT_CODEC ){
						if (childNode->HasAttribute("value")) {
							std::string value = childNode->GetAttributeValue("value");
							AAMPLOG_INFO("Received %s tag property value as %s ",
							SUPPLEMENTAL_PROPERTY_TAG, value.c_str());
							if (value == EC3_EXT_VALUE_AUDIO_ATMOS){
								isAtmos = true;
								break;
							}

						}
					}
					else
					{
						AAMPLOG_WARN("schemeIdUri is not equals to  SCHEME_ID_URI_EC3_EXT_CODEC ");  //CID:84346 - Null Returns
					}
				}
			}
		}
	}

	return isAtmos;
}

/**
 * @brief Get codec value from representation level
 * @param[out] codecValue - string value of codec as per manifest
 * @param[in] rep - representation node for atmos audio check
 * @retval audio type as per aamp code from string value
 */
static AudioType getCodecType(string & codecValue, const IMPDElement *rep)
{
	AudioType audioType = eAUDIO_UNSUPPORTED;
	std::string ac4 = "ac-4";
	if (codecValue == "ec+3")
	{
#ifndef __APPLE__
		audioType = eAUDIO_ATMOS;
#endif
	}
	else if (!codecValue.compare(0, ac4.size(), ac4))
	{
		audioType = eAUDIO_DOLBYAC4;
	}
	else if ((codecValue == "ac-3"))
	{
		audioType = eAUDIO_DOLBYAC3;
	}
	else if ((codecValue == "ec-3"))
	{
		audioType = eAUDIO_DDPLUS;
		/*
		* check whether ATMOS Flag is set as per ETSI TS 103 420
		*/
		if (IsAtmosAudio(rep))
		{
			AAMPLOG_INFO("Setting audio codec as eAUDIO_ATMOS as per ETSI TS 103 420");
			audioType = eAUDIO_ATMOS;
		}
	}
	else if( codecValue.find("vorbis") != std::string::npos )
	{
		audioType = eAUDIO_VORBIS;
	}
	else if( codecValue == "opus" )
	{
		audioType = eAUDIO_OPUS;
	}
	else if( codecValue == "aac" || codecValue.find("mp4") != std::string::npos )
	{
		audioType = eAUDIO_AAC;
	}

	return audioType;
}

/**
 * @brief Get representation index from preferred codec list
 * @retval whether track selected or not
 */
bool StreamAbstractionAAMP_MPD::GetPreferredCodecIndex(IAdaptationSet *adaptationSet, int &selectedRepIdx, AudioType &selectedCodecType,
	uint32_t &selectedRepBandwidth, long &bestScore, bool disableEC3, bool disableATMOS, bool disableAC4, bool disableAC3, bool& disabled)
{
	bool isTrackSelected = false;
	if( aamp->preferredCodecList.size() > 0 )
	{
		selectedRepIdx = -1;
		if(adaptationSet != NULL)
		{
			long score = 0;
			const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
			/* check for codec defined in Adaptation Set */
			const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
			for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
			{
				score = 0;
				const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
				uint32_t bandwidth = rep->GetBandwidth();
				const std::vector<string> codecs = rep->GetCodecs();
				string codecValue="";

				/* check if Representation includec codec */
				if(codecs.size())
				{
					codecValue=codecs.at(0);
				}
				else if(adapCodecs.size()) /* else check if Adaptation has codec defined */
				{
					codecValue = adapCodecs.at(0);
				}
				auto iter = std::find(aamp->preferredCodecList.begin(), aamp->preferredCodecList.end(), codecValue);
				if(iter != aamp->preferredCodecList.end())
				{  /* track is in preferred codec list */
					int distance = (int)std::distance(aamp->preferredCodecList.begin(),iter);
					score = ((aamp->preferredCodecList.size()-distance)) * AAMP_CODEC_SCORE; /* bonus for codec match */
				}
				AudioType codecType = getCodecType(codecValue, rep);
				score += (uint32_t)codecType;
				if (((codecType == eAUDIO_ATMOS) && (disableATMOS || disableEC3)) || /*ATMOS audio disable by config */
					((codecType == eAUDIO_DDPLUS) && disableEC3) || /* EC3 disable neglact it that case */
					((codecType == eAUDIO_DOLBYAC4) && disableAC4) || /** Disable AC4 **/
					((codecType == eAUDIO_DOLBYAC3) && disableAC3) ) /**< Disable AC3 **/
				{
					//Reduce score to 0 since ATMOS and/or DDPLUS is disabled;
					score = 0;
				}

				if(( score > bestScore ) || /* better matching codec */
				((score == bestScore ) && (bandwidth > selectedRepBandwidth) && isTrackSelected )) /* Same codec as selected track but better quality */
				{
					bestScore = score;
					selectedRepIdx = representationIndex;
					selectedRepBandwidth = bandwidth;
					selectedCodecType = codecType;
					isTrackSelected = true;
				}
			} //representation Loop
			if (score == 0)
			{
				/**< No valid representation found here */
				disabled = true;
			}
		} //If valid adaptation
	} // If preferred Codec Set
	return isTrackSelected;
}

/**
 * @brief Get representation index from preferred codec list
 * @retval whether track selected or not
 */
void StreamAbstractionAAMP_MPD::GetPreferredTextRepresentation(IAdaptationSet *adaptationSet, int &selectedRepIdx, uint32_t &selectedRepBandwidth, unsigned long long &score, std::string &name, std::string &codec)
{
	if(adaptationSet != NULL)
	{
		selectedRepBandwidth = 0;
		selectedRepIdx = 0;
		/* check for codec defined in Adaptation Set */
		const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
		const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
		for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
		{
			const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
			uint32_t bandwidth = rep->GetBandwidth();
			if (bandwidth > selectedRepBandwidth) /**< Get Best Rep based on bandwidth **/
			{
				selectedRepIdx  = representationIndex;
				selectedRepBandwidth = bandwidth;
				score += 2; /**< Increase score by 2 if multiple track present*/
			}
			name = rep->GetId();
			const std::vector<std::string> repCodecs = rep->GetCodecs();
			// check if Representation includes codec
			if (repCodecs.size())
			{
				codec = repCodecs.at(0);
			}
			else if (adapCodecs.size()) // else check if Adaptation has codec
			{
				codec = adapCodecs.at(0);
			}
			else
			{
				// For subtitle, it might be vtt/ttml format
				PeriodElement periodElement(adaptationSet, rep);
				codec = periodElement.GetMimeType();
			}
		}
	}
	AAMPLOG_INFO("StreamAbstractionAAMP_MPD: SelectedRepIndex : %d selectedRepBandwidth: %d", selectedRepIdx, selectedRepBandwidth);
}

static int GetDesiredCodecIndex(IAdaptationSet *adaptationSet, AudioType &selectedCodecType, uint32_t &selectedRepBandwidth,
				bool disableEC3,bool disableATMOS, bool disableAC4,bool disableAC3,  bool &disabled)
{
	int selectedRepIdx = -1;
	if(adaptationSet != NULL)
	{
		const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
		// check for codec defined in Adaptation Set
		const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
		for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
		{
			const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
			uint32_t bandwidth = rep->GetBandwidth();
			const std::vector<string> codecs = rep->GetCodecs();
			AudioType audioType = eAUDIO_UNKNOWN;
			string codecValue="";
			// check if Representation includec codec
			if(codecs.size())
				codecValue=codecs.at(0);
			else if(adapCodecs.size()) // else check if Adaptation has codec defn
				codecValue = adapCodecs.at(0);
			// else no codec defined , go with unknown
			audioType = getCodecType(codecValue, rep);

			/*
			* By default the audio profile selection priority is set as ATMOS then DD+ then AAC
			* Note that this check comes after the check of selected language.
			* disableATMOS: avoid use of ATMOS track
			* disableEC3: avoid use of DDPLUS and ATMOS tracks
			* disableAC4: avoid use if ATMOS AC4 tracks
			*/
			if ((selectedCodecType == eAUDIO_UNKNOWN && (audioType != eAUDIO_UNSUPPORTED || selectedRepBandwidth == 0)) || // Select any profile for the first time, reject unsupported streams then
				(selectedCodecType == audioType && bandwidth>selectedRepBandwidth) || // same type but better quality
				(selectedCodecType < eAUDIO_DOLBYAC4 && audioType == eAUDIO_DOLBYAC4 && !disableAC4 ) || // promote to AC4
				(selectedCodecType < eAUDIO_ATMOS && audioType == eAUDIO_ATMOS && !disableATMOS && !disableEC3) || // promote to atmos
				(selectedCodecType < eAUDIO_DDPLUS && audioType == eAUDIO_DDPLUS && !disableEC3) || // promote to ddplus
				(selectedCodecType != eAUDIO_AAC && audioType == eAUDIO_AAC && disableEC3) || // force AAC
				(selectedCodecType == eAUDIO_UNSUPPORTED) // anything better than nothing
				)
			{
				selectedRepIdx = representationIndex;
				selectedCodecType = audioType;
				selectedRepBandwidth = bandwidth;
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: SelectedRepIndex : %d ,selectedCodecType : %d, selectedRepBandwidth: %d", selectedRepIdx, selectedCodecType, selectedRepBandwidth);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("adaptationSet  is null");  //CID:85233 - Null Returns
	}
	return selectedRepIdx;
}

/**
 * @brief Get representation index of desired video codec
 * @param adaptationSet Adaptation set object
 * @param[out] selectedRepIdx index of desired representation
 * @retval index of desired representation
 */
static int GetDesiredVideoCodecIndex(IAdaptationSet *adaptationSet)
{
	const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
	int selectedRepIdx = -1;
	for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
	{
		const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
		const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
		const std::vector<string> codecs = rep->GetCodecs();
		string codecValue="";
		if(codecs.size())
			codecValue=codecs.at(0);
		else if(adapCodecs.size())
			codecValue = adapCodecs.at(0);
		selectedRepIdx = representationIndex;
	}
	return selectedRepIdx;
}


/**
 * @brief map canonical media type to corresponding playlist type
 */
static AampMediaType MediaTypeToPlaylist( AampMediaType mediaType )
{
	switch( mediaType )
	{
		case eMEDIATYPE_VIDEO:
			return eMEDIATYPE_PLAYLIST_VIDEO;
		case eMEDIATYPE_AUDIO:
			return eMEDIATYPE_PLAYLIST_AUDIO;
		case eMEDIATYPE_SUBTITLE:
			return eMEDIATYPE_PLAYLIST_SUBTITLE;
		case eMEDIATYPE_AUX_AUDIO:
			return eMEDIATYPE_PLAYLIST_AUX_AUDIO;
		case eMEDIATYPE_IFRAME:
			return eMEDIATYPE_PLAYLIST_IFRAME;
		default:
			assert(0);
			break;
	}
}


/**
 * @brief read unsigned multi-byte value and update buffer pointer
 * @param[in] pptr buffer
 * @param[in] n word size in bytes
 * @retval 32 bit value
 */
static uint64_t ReadWordHelper( const char **pptr, int n )
{
	const char *ptr = *pptr;
	uint64_t rc = 0;
	while( n-- )
	{
		rc <<= 8;
		rc |= (unsigned char)*ptr++;
	}
	*pptr = ptr;
	return rc;
}

static unsigned int Read16( const char **pptr)
{
	return (unsigned int)ReadWordHelper(pptr,2);
}
static unsigned int Read32( const char **pptr)
{
	return (unsigned int)ReadWordHelper(pptr,4);
}
static uint64_t Read64( const char **pptr)
{
	return ReadWordHelper(pptr,8);
}

/**
 * @brief Parse segment index box
 * @note The SegmentBase indexRange attribute points to Segment Index Box location with segments and random access points.
 * @param start start of box
 * @param size size of box
 * @param segmentIndex segment index
 * @param[out] referenced_size referenced size
 * @param[out] referenced_duration referenced duration
 * @retval true on success
 */
static bool ParseSegmentIndexBox( const char *start, size_t size, int segmentIndex, unsigned int *referenced_size, float *referenced_duration, unsigned int *firstOffset)
{
	if (!start)
	{
		// If the fragment pointer is NULL then return from here, no need to process it further.
		return false;
	}

	const char **f = &start;

	unsigned int len = Read32(f);
	if (len != size)
	{
		AAMPLOG_WARN("Wrong size in ParseSegmentIndexBox %d found, %zu expected", len, size);
		if (firstOffset) *firstOffset = 0;
		return false;
	}

	unsigned int type = Read32(f);
	if (type != 'sidx')
	{
		AAMPLOG_WARN("Wrong type in ParseSegmentIndexBox %c%c%c%c found, %zu expected",
					 (type >> 24) % 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, size);
		if (firstOffset) *firstOffset = 0;
		return false;
	}

	unsigned int version = Read32(f); (void) version;
	unsigned int reference_ID = Read32(f); (void)reference_ID;
	unsigned int timescale = Read32(f);
	uint64_t earliest_presentation_time;
	uint64_t first_offset;
	if( version==0 )
	{
		earliest_presentation_time = Read32(f);
		(void)earliest_presentation_time; // unused
		first_offset = Read32(f);
	}
	else
	{
		earliest_presentation_time = Read64(f);
		(void)earliest_presentation_time; // unused
		first_offset = Read64(f);
	}
	unsigned int reserved = Read16(f); (void)reserved;
	unsigned int reference_count = Read16(f);
	if (firstOffset)
	{
		*firstOffset = (unsigned int)first_offset;
		return true;
	}
	if( segmentIndex<reference_count )
	{
		start += 12*segmentIndex;
		*referenced_size = Read32(f)&0x7fffffff;
		// top bit is "reference_type"

		*referenced_duration = Read32(f)/(float)timescale;

		unsigned int flags = Read32(f);
		(void)flags;
		// starts_with_SAP (1 bit)
		// SAP_type (3 bits)
		// SAP_delta_time (28 bits)

		return true;
	}
	return false;
}

/**
 * @brief Replace matching token with given number
 * @param str String in which operation to be performed
 * @param from token
 * @param toNumber number to replace token
 * @retval position
 */
static int replace(std::string& str, const std::string& from, uint64_t toNumber )
{
	int rc = 0;
	size_t tokenLength = from.length();

	for (;;)
	{
		bool done = true;
		size_t pos = 0;
		for (;;)
		{
			pos = str.find('$', pos);
			if (pos == std::string::npos)
			{
				break;
			}
			size_t next = str.find('$', pos + 1);
			if(next != 0)
			{
				if (str.substr(pos + 1, tokenLength) == from)
				{
					size_t formatLen = next - pos - tokenLength - 1;
					char buf[256];
					if (formatLen > 0)
					{
						std::string format = str.substr(pos + tokenLength + 1, formatLen-1);
						char type = str[pos+tokenLength+formatLen];
						switch( type )
						{ // don't use the number-formatting string from dash manifest as-is; map to uint64_t equivalent
							case 'd':
								format += PRIu64;
								break;
							case 'x':
								format += PRIx64;
								break;
							case 'X':
								format += PRIX64;
								break;
							default:
								AAMPLOG_WARN( "unsupported template format: %s%c", format.c_str(), type );
								format += type;
								break;
						}

						snprintf(buf, sizeof(buf), format.c_str(), toNumber);
						tokenLength += formatLen;
					}
					else
					{
						snprintf(buf, sizeof(buf), "%" PRIu64 "", toNumber);
					}
					str.replace(pos, tokenLength + 2, buf);
					done = false;
					rc++;
					break;
				}
				pos = next + 1;
			}
			else
			{
				AAMPLOG_WARN("next is not found ");  //CID:81252 - checked return
				break;
			}
		}
		if (done) break;
	}

	return rc;
}


/**
 * @brief Replace matching token with given string
 * @param str String in which operation to be performed
 * @param from token
 * @param toString string to replace token
 * @retval position
 */
static int replace(std::string& str, const std::string& from, const std::string& toString )
{
	int rc = 0;
	size_t tokenLength = from.length();

	for (;;)
	{
		bool done = true;
		size_t pos = 0;
		for (;;)
		{
			pos = str.find('$', pos);
			if (pos == std::string::npos)
			{
				break;
			}
			size_t next = str.find('$', pos + 1);
			if(next != 0)
			{
				if (str.substr(pos + 1, tokenLength) == from)
				{
					str.replace(pos, tokenLength + 2, toString);
					done = false;
					rc++;
					break;
				}
				pos = next + 1;
			}
			else
			{
				AAMPLOG_ERR("Error at  next");  //CID:81346 - checked return
				break;
			}
		}

		if (done) break;
	}

	return rc;
}


/**
 * @brief Generates fragment url from media information
 */
void StreamAbstractionAAMP_MPD::ConstructFragmentURL( std::string& fragmentUrl, const FragmentDescriptor *fragmentDescriptor, std::string media)
{
	std::string constructedUri = fragmentDescriptor->GetMatchingBaseUrl();
	if( media.empty() )
	{
	}
	else if( aamp_IsAbsoluteURL(media) )
	{ // don't pre-pend baseurl if media starts with http:// or https://
		constructedUri.clear();
	}
	else if (!constructedUri.empty())
	{
		if(ISCONFIGSET(eAAMPConfig_DASHIgnoreBaseURLIfSlash))
		{
			if (constructedUri == "/")
			{
				AAMPLOG_WARN("ignoring baseurl /");
				constructedUri.clear();
			}
		}
		// append '/' suffix to BaseURL if not already present
		if( aamp_IsAbsoluteURL(constructedUri) )
		{
			if( constructedUri.back() != '/' )
			{
				constructedUri += '/';
			}
		}
	}
	else
	{
		AAMPLOG_TRACE("BaseURL not available");
	}
	constructedUri += media;
	replace(constructedUri, "Bandwidth", fragmentDescriptor->Bandwidth);
	replace(constructedUri, "RepresentationID", fragmentDescriptor->RepresentationID);
	replace(constructedUri, "Number", fragmentDescriptor->Number);
	replace(constructedUri, "Time", (uint64_t)fragmentDescriptor->Time );
	aamp_ResolveURL(fragmentUrl, fragmentDescriptor->manifestUrl, constructedUri.c_str(),ISCONFIGSET(eAAMPConfig_PropagateURIParam));
}

/**
 * @brief Gets a curlInstance index for a given AampMediaType
 * @param type the stream AampMediaType
 * @retval AampCurlInstance index to curl_easy_perform session
 */
static AampCurlInstance getCurlInstanceByMediaType(AampMediaType type)
{
	AampCurlInstance instance;

	switch (type)
	{
	case eMEDIATYPE_VIDEO:
		instance = eCURLINSTANCE_VIDEO;
		break;
	case eMEDIATYPE_AUDIO:
		instance = eCURLINSTANCE_AUDIO;
		break;
	case eMEDIATYPE_SUBTITLE:
		instance = eCURLINSTANCE_SUBTITLE;
		break;
	default:
		instance = eCURLINSTANCE_VIDEO;
		break;
	// what about eMEDIATYPE_AUX_AUDIO?
	}

	return instance;
}

static void deIndexTileInfo(std::vector<TileInfo> &indexedTileInfo)
{
	if( !indexedTileInfo.empty() )
	{
		AAMPLOG_WARN("indexedTileInfo size=%zu",indexedTileInfo.size());
		indexedTileInfo.clear();
	}
}

/**
 * @brief Fetch and cache a fragment
 *
 * @retval true on fetch success
 */
bool StreamAbstractionAAMP_MPD::FetchFragment(MediaStreamContext *pMediaStreamContext, std::string media, double fragmentDuration, bool isInitializationSegment, unsigned int curlInstance, bool discontinuity, double pto , uint32_t timeScale )
{ // given url, synchronously download and transmit associated fragment
	bool retval = true;
	std::string fragmentUrl;
	ConstructFragmentURL(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, media);
	double position = ((double) pMediaStreamContext->fragmentDescriptor.Time) / ((double) pMediaStreamContext->fragmentDescriptor.TimeScale);
	if(isInitializationSegment)
	{
		if(!(pMediaStreamContext->initialization.empty()) && (0 == pMediaStreamContext->initialization.compare(fragmentUrl))&& !discontinuity)
		{
			AAMPLOG_TRACE("We have pushed the same initialization segment for %s skipping", GetMediaTypeName(AampMediaType(pMediaStreamContext->type)));
			return retval;
		}
		else
		{
			pMediaStreamContext->initialization = std::string(fragmentUrl);
		}
	}
	bool fragmentCached = pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, position, fragmentDuration, NULL, isInitializationSegment, discontinuity
		,(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING), pto, timeScale);
	// Check if we have downloaded the fragment and waiting for init fragment download on
	// bitrate switching before caching it.
	bool fragmentSaved = (NULL != pMediaStreamContext->mDownloadedFragment.GetPtr() );

	if (!fragmentCached)
	{
		if(!fragmentSaved)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: failed. fragmentUrl %s fragmentTime %f %d %d", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime,isInitializationSegment, pMediaStreamContext->type);
			//Added new check to avoid marking ad as failed if the http code is not worthy.
			if (isInitializationSegment && mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING &&
				(pMediaStreamContext->httpErrorCode!=CURLE_WRITE_ERROR && pMediaStreamContext->httpErrorCode!= CURLE_ABORTED_BY_CALLBACK))
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: [CDAI] Ad init fragment not available. Playback failed.");
				mCdaiObject->mAdBreaks[mBasePeriodId].mAdFailed = true;
			}
		}
		if (discontinuity && isInitializationSegment)
		{
			if(eTRACK_VIDEO == pMediaStreamContext->type)
			{
				isVidDiscInitFragFail = true;
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: failed. isInit: %d IsTrackVideo: %s isDisc: %d vidInitFail: %d",
								isInitializationSegment, GetMediaTypeName(AampMediaType(pMediaStreamContext->type) ), isInitializationSegment, isVidDiscInitFragFail);
			}
			if(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING)
			{
				// Insert a dummy fragment with discontinuity, since we didn't get an init fragments so it wouldn't get flagged
				CachedFragment* cachedFragment = nullptr;
				if(pMediaStreamContext->IsInjectionFromCachedFragmentChunks())
				{
					if(pMediaStreamContext->WaitForCachedFragmentChunkInjected())
					{
						cachedFragment = pMediaStreamContext->GetFetchChunkBuffer(true);
					}
				}
				else
				{
					if(pMediaStreamContext->WaitForFreeFragmentAvailable())
					{
						cachedFragment = pMediaStreamContext->GetFetchBuffer(true);
					}
				}
				if(cachedFragment && !(aamp->GetTSBSessionManager() && pMediaStreamContext->IsLocalTSBInjection()))
				{
					// The pointer is loaded to bypass null check in InjectFragment thread
					cachedFragment->fragment.AppendBytes("0x0a", 2);
					cachedFragment->position=0;
					cachedFragment->duration=0;
					cachedFragment->initFragment=true;
					cachedFragment->discontinuity=true;
					cachedFragment->profileIndex=0;
					cachedFragment->isDummy=true;
					cachedFragment->type=pMediaStreamContext->mediaType;
					if(pMediaStreamContext->IsInjectionFromCachedFragmentChunks())
					{
						pMediaStreamContext->UpdateTSAfterChunkFetch();
					}
					else
					{
						pMediaStreamContext->UpdateTSAfterFetch(true);
					}
				}
			}
		}
		retval = false;
	}

	if (discontinuity && (isInitializationSegment && eTRACK_VIDEO == pMediaStreamContext->type ) && (retval && isVidDiscInitFragFail))
{
		isVidDiscInitFragFail = false;
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: rampdown init download success. isInit: %d IsTrackVideo: %s isDisc: %d vidInitFail: %d",
						isInitializationSegment, GetMediaTypeName(AampMediaType(pMediaStreamContext->type) ), isInitializationSegment, isVidDiscInitFragFail);
	}

	/**In the case of ramp down same fragment will be retried
	 *Avoid fragmentTime update in such scenarios.
	 *In other cases if it's success or failure, AAMP will be going
	 *For next fragment so update fragmentTime with fragment duration
	 */
	if (!pMediaStreamContext->mCheckForRampdown && !fragmentSaved)
	{
		if(rate > 0)
		{
			pMediaStreamContext->fragmentTime += fragmentDuration;
			if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
			{
				mBasePeriodOffset += fragmentDuration;
			}
		}
		else
		{
			pMediaStreamContext->fragmentTime -= fragmentDuration;
			if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
			{
				mBasePeriodOffset -= fragmentDuration;
			}
			if(pMediaStreamContext->fragmentTime < 0)
			{
				pMediaStreamContext->fragmentTime = 0;
			}
		}
	}
	return retval;
}
/*
* @brief Use lastSegmentTime to find position in segment timeline after manifest update
*
* @param[in] pMediaStreamContext->fragmentDescriptor.Time == 0
* @param[in] pMediaStreamContext->lastSegmentTime               Time of the last segment that was injected
* @param[in] pMediaStreamContext->fragmentDescriptor.Number     Set to the SegmentTemplate startNumber
* @param[in] pMediaStreamContext->fragmentRepeatCount == 0
* @param[in] pMediaStreamContext->lastSegmentDuration           Duration of the last segment injected

* @param[out] pMediaStreamContext->fragmentDescriptor.Number    Number of the last segment that was injected
* @param[out] pMediaStreamContext->fragmentDescriptor.nextfragmentNum
* @param[out] pMediaStreamContext->fragmentRepeatCount          0 base index into row of the SegmentTimeline
* @param[out] pMediaStreamContext->timeLineIndex                0 base index of row
* @param[out] pMediaStreamContext->fragmentDescriptor.Time      Start time of the last segment that was injected
*/
uint64_t StreamAbstractionAAMP_MPD::FindPositionInTimeline(class MediaStreamContext *pMediaStreamContext, const std::vector<ITimeline *>&timelines)
{
	uint32_t duration = 0;
	uint32_t repeatCount = 0;
	uint64_t nextStartTime = 0;
	uint64_t startTime = 0;
	int index = pMediaStreamContext->timeLineIndex;
	ITimeline *timeline = NULL;

#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
	AAMPLOG_INFO("Type[%d] timeLineIndex %d timelines.size %zu fragmentRepeatCount %d Number %" PRIu64 "lastSegmentTime %" PRIu64 " lastSegmentDuration %" PRIu64, pMediaStreamContext->type,
	pMediaStreamContext->timeLineIndex, timelines.size(), pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentDescriptor.Number,
	pMediaStreamContext->lastSegmentTime, pMediaStreamContext->lastSegmentDuration);
#endif

	for(;index<timelines.size();index++)
	{
		timeline = timelines.at(index);
		map<string, string> attributeMap = timeline->GetRawAttributes();
		if(attributeMap.find("t") != attributeMap.end())
		{
			startTime = timeline->GetStartTime();
		}
		else
		{
			startTime = pMediaStreamContext->fragmentDescriptor.Time;
		}

		duration = timeline->GetDuration();
		// For Dynamic segment timeline content
		if (0 == startTime && 0 != duration)
		{
			startTime = nextStartTime;
		}
		repeatCount = timeline->GetRepeatCount();

		nextStartTime = startTime+((uint64_t)(repeatCount+1)*duration);  //CID:98056 - Resolve overflow Before Widen

		/* For a timeline
		* <SegmentTimeline>
		*  <S d="109568" t="0"/>
		*  <S d="107520" r="4" t="109568"/>
		* and a manifest update after segment 1 has been sent. Ensure one cycle of the for loop so
		* timeLineIndex gets incremented.
		* Without this we get a segment dropped and another repeated in server side ads
		*/

		bool isFirstSegment = pMediaStreamContext->lastSegmentTime == 0 && startTime == 0
									&& pMediaStreamContext->lastSegmentDuration != 0
									&& repeatCount == 0 && pMediaStreamContext->timeLineIndex == 0;

#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
		AAMPLOG_INFO("Type[%d] nextStartTime=%" PRIu64 " startTime=%" PRIu64 " repeatCount=%u", pMediaStreamContext->type,
			nextStartTime,startTime,repeatCount);
#endif
		if(pMediaStreamContext->lastSegmentTime < nextStartTime && !isFirstSegment)
		{
			break;
		}
		pMediaStreamContext->fragmentDescriptor.Number += (repeatCount+1);
		pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+(repeatCount+1);
	}// end of for

	/*
	*  Boundary check added to handle the edge case leading to crash,
	*/
	if(index == timelines.size())
	{
		AAMPLOG_WARN("Type[%d] Boundary Condition !!! Index(%d) reached Max.Start=%" PRIu64 " Last=%" PRIu64 " ",
			pMediaStreamContext->type,index,startTime,pMediaStreamContext->lastSegmentTime);
		index--;
		startTime = pMediaStreamContext->lastSegmentTime;
		pMediaStreamContext->fragmentRepeatCount = repeatCount+1;
	}

	pMediaStreamContext->timeLineIndex = index;
	// Now we reached the right row , need to traverse the repeat index to reach right node
	// Whenever new fragments arrive inside the same timeline update fragment number,repeat count and startNumber.
	// If first fragment start Number is zero, check lastSegmentDuration of period timeline for update.
	while((pMediaStreamContext->fragmentRepeatCount < repeatCount && startTime < pMediaStreamContext->lastSegmentTime) ||
		(startTime == 0 && pMediaStreamContext->lastSegmentTime == 0 && pMediaStreamContext->lastSegmentDuration != 0))
	{
		startTime += duration;
		pMediaStreamContext->fragmentDescriptor.Number++;
		pMediaStreamContext->fragmentRepeatCount++;
		pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+1;
	}

#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
	AAMPLOG_INFO("Type[%d] timeLineIndex %d timelines.size %zu fragmentRepeatCount %d Number %" PRIu64 "lastSegmentTime %" PRIu64 " lastSegmentDuration %" PRIu64, pMediaStreamContext->type,
	pMediaStreamContext->timeLineIndex, timelines.size(), pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentDescriptor.Number,
	pMediaStreamContext->lastSegmentTime, pMediaStreamContext->lastSegmentDuration);
#endif
	return startTime;
}
/**
 * @brief Fetch and push next fragment
 * @param pMediaStreamContext Track object
 * @param curlInstance instance of curl to be used to fetch
 * @param skipFetch true if fragment fetch is to be skipped for seamlessaudio
 * @retval true if push is done successfully
 */
bool StreamAbstractionAAMP_MPD::PushNextFragment( class MediaStreamContext *pMediaStreamContext, unsigned int curlInstance, bool skipFetch)
{
	bool retval=false;
	bool fcsContent=false;
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
	AAMPLOG_INFO("Type[%d] timeLineIndex %d fragmentRepeatCount %u PeriodDuration:%f mCurrentPeriodIdx:%d mPeriodStartTime %f",pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount,mPeriodDuration,mCurrentPeriodIdx,mPeriodStartTime );
#endif
//To fill the failover Content Vector for only once on every representation ,if it presents
	pMediaStreamContext->failAdjacentSegment = false;
	if(mFcsRepresentationId != pMediaStreamContext->representationIndex)
	{
		mIsFcsRepresentation=false;
		mFcsSegments.clear();
		fcsContent = false;
		ISegmentTemplate *segmentTemplate = pMediaStreamContext->representation->GetSegmentTemplate();
		if (segmentTemplate)
		{
			const IFailoverContent *failoverContent = segmentTemplate->GetFailoverContent();
			if(failoverContent)
			{
				mFcsSegments = failoverContent->GetFCS();
				bool valid = failoverContent->IsValid();
				//to indicate this representation contain failover tag or not
				mIsFcsRepresentation = valid ? false : true;
			}
		}
	}
	if( segmentTemplates.HasSegmentTemplate() )
	{
		std::string media = segmentTemplates.Getmedia();
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		uint32_t timeScale = segmentTemplates.GetTimescale();

		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(!timelines.empty())
			{
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
				AAMPLOG_INFO("Type[%d] timelineCnt=%zu timeLineIndex:%d FDTime=%f L=%" PRIu64 " [fragmentTime = %f,  mLiveEndPosition=%f]",
					pMediaStreamContext->type, timelines.size(), pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentTime, mLiveEndPosition);
#endif
				if ((pMediaStreamContext->timeLineIndex >= timelines.size()) || (pMediaStreamContext->timeLineIndex < 0)
						||(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState &&
							((rate > AAMP_NORMAL_PLAY_RATE && pMediaStreamContext->fragmentTime >= aamp->mAbsoluteEndPosition)
							 ||(rate < 0 && pMediaStreamContext->fragmentTime <= mPeriodStartTime))))
				{
					AAMPLOG_INFO("Type[%d] EOS. timeLineIndex[%d] size [%zu]",pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
					pMediaStreamContext->eos = true;
				}
				else
				{
					// Presentation Offset handling - When period is splitted for Ads, additional segments
					// for last period will be added which are not required to play.Need to play based on
					// presentationTimeOffset attribute

					// When new period starts , need to check if PresentationOffset exists and its
					// different from startTime of first timeline
					// ->During refresh of manifest, fragmentDescriptor.Time != 0.Not to update PTSOffset
					// ->During period change or start of playback , fragmentDescriptor.Time=0. Need to
					//      update with PTSOffset
					uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
					uint32_t tScale = segmentTemplates.GetTimescale();
					uint64_t periodStart = 0;
					string startTimeStr = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetStart();

					pMediaStreamContext->timeStampOffset = 0;

					if ((!startTimeStr.empty()) && (tScale != 0))
					{
						periodStart = (uint64_t)(ParseISO8601Duration(startTimeStr.c_str()) / 1000);
						int64_t timeStampOffset = (int64_t)(periodStart - (uint64_t)(presentationTimeOffset/tScale));

						if (timeStampOffset > 0)
						{
							pMediaStreamContext->timeStampOffset = timeStampOffset;
						}
					}

					if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO && prevTimeScale != 0 && prevTimeScale != timeScale)
					{
						AAMPLOG_WARN("[!!! WARNING !!!] Inconsistent timescale detected within same adaptation sets - prevTimeScale %d currentTimeScale %d, which contradicts stream compliance, Applying workaround",prevTimeScale, timeScale);

						double timelineOffset  = (double)timeScale / (double)prevTimeScale;
						if(pMediaStreamContext->lastSegmentTime != 0)
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->lastSegmentTime * timelineOffset;
						}
						if(pMediaStreamContext->lastSegmentDuration != 0)
						{
							pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->lastSegmentDuration * timelineOffset;
						}

						//When manifest refresh, FD.Time will be 0 and updated based on start time later.
						if(pMediaStreamContext->fragmentDescriptor.Time != 0 )
						{
							pMediaStreamContext->fragmentDescriptor.Time = pMediaStreamContext->fragmentDescriptor.Time * timelineOffset;
						}

					}

					if (presentationTimeOffset > 0 && pMediaStreamContext->lastSegmentDuration ==  0
						&& pMediaStreamContext->fragmentDescriptor.Time == 0)
					{
						// Check the first timeline starttime.
						int index = 0;
						uint64_t segmentStartTime = 0;
						uint64_t startTime = 0;
						ITimeline *timeline = timelines.at(index);
						// Some timeline may not have attribute for timeline , check it .
						map<string, string> attributeMap = timeline->GetRawAttributes();
						if(attributeMap.find("t") != attributeMap.end())
						{
							segmentStartTime = timeline->GetStartTime();
							startTime = segmentStartTime;
						}
						else
						{
							segmentStartTime = 0;
							startTime = presentationTimeOffset;
						}

						// This logic comes into picture if startTime is different from
						// presentation Offset
						if(startTime != presentationTimeOffset)
						{
							// if startTime is 0 or if no timeline attribute "t"
							if(startTime == 0)
							{
								AAMPLOG_INFO("Type[%d] Setting start time with PTSOffset:%" PRIu64 "",pMediaStreamContext->type,presentationTimeOffset);
							}
							else
							{
								// Non zero startTime , need to traverse and find the right
								// line index and repeat number
								uint32_t duration =0;
								uint32_t repeatCount =0;
								uint64_t nextStartTime = 0;
								int offsetNumber = 0;
								// This loop is to go to the right index and segment number
								//based on presentationTimeOffset
								pMediaStreamContext->fragmentRepeatCount = 0;
								while(index<timelines.size())
								{
									timeline = timelines.at(index);
									map<string, string> attributeMap = timeline->GetRawAttributes();
									if(attributeMap.find("t") != attributeMap.end())
									{
										segmentStartTime = timeline->GetStartTime();
										startTime = segmentStartTime;
									}
									else
									{
										startTime = nextStartTime;
									}
									duration = timeline->GetDuration();
									// For Dynamic segment timeline content
									if (0 == startTime && 0 != duration)
									{
										startTime = nextStartTime;
									}
									repeatCount = timeline->GetRepeatCount();
									nextStartTime = startTime+((uint64_t)(repeatCount+1)*duration);
									// found the right index
									if(nextStartTime > (presentationTimeOffset+1))
									{
										// if timeline has repeat value ,go to correct offset
										if (repeatCount != 0)
										{
											for(int i=0; i<repeatCount; i++)
											{
												if((startTime + duration) > (presentationTimeOffset+1))
												{       // found the right offset
													break;
												}
												segmentStartTime += duration;
												startTime += duration;
												offsetNumber++;
												pMediaStreamContext->fragmentRepeatCount++;
											}
										}
										break; // break the while loop
									}
									// Add all the repeat count before going to next timeline
									segmentStartTime += ((uint64_t)(repeatCount+1))*((uint64_t)duration);
									offsetNumber += (repeatCount+1);
									index++;
								}
								// After exit of loop , update the fields
								if(index != timelines.size())
								{
									pMediaStreamContext->fragmentDescriptor.Number += offsetNumber;
									pMediaStreamContext->timeLineIndex = index;
									AAMPLOG_INFO("Type[%d] skipping fragments[%d] to Index:%d FNum=%" PRIu64 " Repeat:%d", pMediaStreamContext->type,offsetNumber,index,pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentRepeatCount);
								}
							}
						}
						// Modify the descriptor time to start download
						pMediaStreamContext->fragmentDescriptor.Time = segmentStartTime;
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
						AAMPLOG_INFO("Type[%d] timelineCnt %zu timeLineIndex %d FDTime %f fragmentTime %f mLiveEndPosition %f",
						pMediaStreamContext->type ,timelines.size(), pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentDescriptor.Time,
						pMediaStreamContext->fragmentTime, mLiveEndPosition);

						AAMPLOG_INFO("Type[%d] lastSegmentTime %" PRIu64 " lastSegmentDuration %" PRIu64 " lastSegmentNumber %" PRIu64,pMediaStreamContext->type,
						pMediaStreamContext->lastSegmentTime, pMediaStreamContext->lastSegmentDuration, pMediaStreamContext->lastSegmentNumber);
#endif
					}
					else if (pMediaStreamContext->fragmentRepeatCount == 0 && !pMediaStreamContext->mReachedFirstFragOnRewind)
					{
						ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
						uint64_t startTime = 0;
						map<string, string> attributeMap = timeline->GetRawAttributes();
						if(attributeMap.find("t") != attributeMap.end())
						{
							// If there is a presentation offset time, update start time to that value.
							startTime = timeline->GetStartTime();
						}
						else
						{
							startTime = pMediaStreamContext->fragmentDescriptor.Time;
						}
						if(mIsLiveStream)
						{
							// After mpd refresh , Time will be 0. Need to traverse to the right fragment for playback
							if((0 == pMediaStreamContext->fragmentDescriptor.Time) || rate > AAMP_NORMAL_PLAY_RATE)
							{
								startTime = FindPositionInTimeline(pMediaStreamContext, timelines);
							}
						}// if starttime
						if(0 == pMediaStreamContext->timeLineIndex)
						{
							AAMPLOG_INFO("Type[%d] update startTime to %" PRIu64 ,pMediaStreamContext->type, startTime);
						}
						pMediaStreamContext->fragmentDescriptor.Time = startTime;
						//Some partner streams have timeline start variation(~1) under diff representation but on same adaptation with same startNumber. Reset lastSegmentTime, as FDT>lastSegmentTime, it leads to Fetch duplicate fragment, as fragment number remains same.
						if (pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO &&
							(prevTimeScale != 0 && prevTimeScale != timeScale) && (pMediaStreamContext->lastSegmentTime != 0 && pMediaStreamContext->lastSegmentTime < pMediaStreamContext->fragmentDescriptor.Time) &&
							(pMediaStreamContext->lastSegmentNumber != 0 && pMediaStreamContext->lastSegmentNumber == pMediaStreamContext->fragmentDescriptor.Number))
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
						}

#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
						AAMPLOG_INFO("Type[%d] Setting startTime to %" PRIu64, pMediaStreamContext->type, startTime);
#endif
					} // if fragRepeat == 0

					ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
					uint32_t repeatCount = timeline->GetRepeatCount();
					uint32_t duration = timeline->GetDuration();
					if (pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
					{
						prevTimeScale = timeScale;
					}

					// Flag used to identify manifest refresh after fragment download (FetchFragment)
					pMediaStreamContext->freshManifest = false;
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
					AAMPLOG_INFO("Type[%d] FDt=%f L=%" PRIu64 " d=%d r=%d fragrep=%d x=%d num=%" PRIu64,
					pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time,
					pMediaStreamContext->lastSegmentTime, duration, repeatCount,pMediaStreamContext->fragmentRepeatCount,
					pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Number);
#endif
				   /* While calling from SwitchAudioTrack(),no need to perform fragment download,requires only the parameters
					* updated, so avoiding FetchFragment() by using during Manifest refreshed case while AudioTrack Switching
					* Particularly during the MPD refresh scenario, wrong fragment position injection is happening due to not properly updating the
					* pMediaStreamContext params for audio during performing SeamlessAudioSwitch
					*/
					if (!skipFetch && ((pMediaStreamContext->fragmentDescriptor.Time > pMediaStreamContext->lastSegmentTime) || (0 == pMediaStreamContext->lastSegmentTime)))
					{
						double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
						double endTime  = (mPeriodStartTime+(mPeriodDuration/1000));
						ITimeline *firstTimeline = timelines.at(0);
						double positionInPeriod = 0;
						uint64_t ret = pMediaStreamContext->lastSegmentDuration;
						double firstSegStartTime = mPeriodStartTime;
						uint64_t firstStartTime = firstTimeline->GetStartTime();
						// CID:186808 - Invalid iterator comparison
						map<string, string> attributeMap = firstTimeline->GetRawAttributes();
						if((attributeMap.find("t") != attributeMap.end()) && (ret > 0))
						{
							// 't' in first timeline is expected.
							positionInPeriod = (pMediaStreamContext->lastSegmentDuration - firstTimeline->GetStartTime()) / timeScale;
						}
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
						AAMPLOG_INFO("Type[%d] presenting FDt%f Number(%" PRIu64 ") Last=%" PRIu64 " Duration(%d) FTime(%f) endTime:%f",
							pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time,pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->lastSegmentTime,duration,pMediaStreamContext->fragmentTime,endTime);
#endif
						retval = true;
						if (mIsFcsRepresentation)
						{
							fcsContent = false;
							for (int i = 0; i < mFcsSegments.size(); i++)
							{
								uint64_t starttime = mFcsSegments.at(i)->GetStartTime();
								uint64_t duration = mFcsSegments.at(i)->GetDuration();
								// Logic  to handle the duration option missing case
								if (!duration)
								{
									// If not present, the alternative content section lasts until the start of the next FCS _element_
									if (i + 1 < mFcsSegments.size())
									{
										duration =  mFcsSegments.at(i+1)->GetStartTime();
									}
									// until the end of the Period
									else if(mPeriodEndTime)
									{
										duration = mPeriodEndTime;
									}
									// or until the end of MPD duration
									else
									{
										duration	=	mMPDParseHelper->GetMediaPresentationDuration();
									}
								}
								// the value of this attribute minus the value of the @presentationTimeOffset specifies the MPD start time,
								uint64_t fcscontent_range = (starttime  + duration);
								if((starttime <= pMediaStreamContext->fragmentDescriptor.Time)&&(fcscontent_range > pMediaStreamContext->fragmentDescriptor.Time))
								{
									fcsContent = true;
								}
							}
						}

						uint64_t fragmentTimeBackUp = pMediaStreamContext->fragmentDescriptor.Time;
						pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time+duration;
						if(pMediaStreamContext->fragmentDescriptor.nextfragmentNum == -1)
						{
							pMediaStreamContext->fragmentDescriptor.nextfragmentNum  = pMediaStreamContext->fragmentDescriptor.Number+1;
						}
						bool liveEdgePeriodPlayback = mIsLiveManifest && (mCurrentPeriodIdx == mMPDParseHelper->mUpperBoundaryPeriod);
						uint64_t fragmentNumberBackUp = pMediaStreamContext->fragmentDescriptor.Number;

						if (firstStartTime < presentationTimeOffset)
						{
							firstSegStartTime = (double)(firstStartTime / tScale);
							// Period end time also needs to be updated based on the PTO.
							// Otherwise, there is a chance to skip the last few fragments from the period if there is a large delta between the  start time available in the manifest and the PTO.
							endTime = (firstSegStartTime + (mPeriodDuration / 1000));

							AAMPLOG_INFO(" PTO ::(startTime < PTO) firstStartTime %" PRIu64 " tScale : %d presentationTimeOffset[%" PRIu64 "] positionInPeriod = %f  startTime = %f  endTime : %f mPeriodStartTime = %f mPeriodDuration = %f ",
										 firstStartTime, tScale, presentationTimeOffset, positionInPeriod, firstSegStartTime, endTime, mPeriodStartTime, mPeriodDuration);
						}

						if(!fcsContent &&
							(mIsFogTSB ||
								((0 != mPeriodDuration) &&
									(((firstSegStartTime + positionInPeriod) < endTime) || liveEdgePeriodPlayback || mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING)))) //For split period ads, the position in the period doesn't need to be between the period's start and end
						{
							/*
							 * Avoid FetchFragment for following cases
							 *
							 *  1. Fail over content
							 *  2. Non-Fog(Linear OR VOD) with fragment position outside period end, except live final period.
							 */
							if(!mIsFogTSB)
							{
								setNextobjectrequestUrl(media,&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
							}
							retval = FetchFragment( pMediaStreamContext, media, fragmentDuration, false, curlInstance);
						}
						else
						{
							AAMPLOG_WARN("Type[%d] Skipping Fetchfragment, Number(%" PRIu64 ") fragment beyond duration. fragmentPosition: %lf  starttime:%lf periodEndTime : %lf ", pMediaStreamContext->type
								, pMediaStreamContext->fragmentDescriptor.Number, positionInPeriod ,  firstSegStartTime, endTime);
						}
						if(fcsContent)
						{
							int http_code = 404;
							retval = false;
							if (pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
							{
								// Attempt rampdown
								if (CheckForRampDownProfile(http_code))
								{
									AAMPLOG_WARN("RampDownProfile Due to failover Content %" PRIu64 " Number %lf FDT",pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentDescriptor.Time);
									pMediaStreamContext->mCheckForRampdown = true;
									// Rampdown attempt success, download same segment from lower profile.
									pMediaStreamContext->mSkipSegmentOnError = false;
									return retval;
								}
								else
								{
									AAMPLOG_WARN("Already at the lowest profile, skipping segment due to failover");
									mRampDownCount = 0;
								}
							}
						}

						if (retval)
						{
							if(pMediaStreamContext->freshManifest)
							{
								// if manifest refreshed in between, take backup values
								pMediaStreamContext->lastSegmentTime = fragmentTimeBackUp;
								pMediaStreamContext->lastSegmentDuration = fragmentTimeBackUp + duration;
								pMediaStreamContext->lastSegmentNumber = fragmentNumberBackUp;
							}
							else
							{
								pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
								pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
								pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
							}

							// pMediaStreamContext->lastDownloadedPosition is introduced to calculate the buffered duration value.
							// Update position in period after fragment download
							pMediaStreamContext->lastDownloadedPosition = pMediaStreamContext->fragmentTime;
							AAMPLOG_INFO("[%s] lastDownloadedPosition %lfs fragmentTime %lfs fragmentDuration %fs",
								GetMediaTypeName(pMediaStreamContext->mediaType),
								pMediaStreamContext->lastDownloadedPosition.load(),
								pMediaStreamContext->fragmentTime,
								fragmentDuration);
						}
						else if((mIsFogTSB && !mAdPlayingFromCDN) && pMediaStreamContext->mDownloadedFragment.GetPtr() )
						{
							pMediaStreamContext->profileChanged = true;
							profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							FetchAndInjectInitialization(eMEDIATYPE_VIDEO);
							UpdateRampUpOrDownProfileReason();
							pMediaStreamContext->SetCurrentBandWidth(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							return false;
						}
						else if( pMediaStreamContext->mCheckForRampdown && pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO)
						{
							//  On audio fragment download failure (http500), rampdown was attempted .
							// rampdown is only needed for video fragments not for audio.
							// second issue : after rampdown lastSegmentTime was going into "0" . When this combined with mpd refresh immediately after rampdown ,
							// startTime is set to start of Period . This caused audio fragment download from "0" resulting in PTS mismatch and mute
							// Fix : Only do lastSegmentTime correction for video not for audio
							//	 lastSegmentTime to be corrected with duration of last segment attempted .
							return retval; /* Incase of fragment download fail, no need to increase the fragment number to download next fragment,
									 * instead check the same fragment in lower profile. */
						}
						else if((mIsFogTSB && (ISCONFIGSET(eAAMPConfig_InterruptHandling))) || (!pMediaStreamContext->mCheckForRampdown && pMediaStreamContext->mDownloadedFragment.GetPtr() == NULL))
						{
							// Mark fragment fetched and save last segment time to avoid reattempt.
							if(pMediaStreamContext->freshManifest)
							{
								// if manifest refreshed in between, take backup values
								pMediaStreamContext->lastSegmentTime = fragmentTimeBackUp;
								pMediaStreamContext->lastSegmentDuration = fragmentTimeBackUp + duration;
							}
							else
							{
								pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
								pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
							}
						}
					}
					else if (!skipFetch && rate < 0)
					{
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
						AAMPLOG_INFO("Type[%d] presenting %f" ,pMediaStreamContext->type,pMediaStreamContext->fragmentDescriptor.Time);
#endif
						pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
						pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
						pMediaStreamContext->mReachedFirstFragOnRewind = false;
						double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
						if(!mIsFogTSB)
						{
							setNextobjectrequestUrl(media,&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
						}
						retval = FetchFragment( pMediaStreamContext, media, fragmentDuration, false, curlInstance);
						if (!retval && ((mIsFogTSB && !mAdPlayingFromCDN) && pMediaStreamContext->mDownloadedFragment.GetPtr() ))
						{
							pMediaStreamContext->profileChanged = true;
							profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							FetchAndInjectInitialization(eMEDIATYPE_VIDEO);
							UpdateRampUpOrDownProfileReason();
							pMediaStreamContext->SetCurrentBandWidth(pMediaStreamContext->fragmentDescriptor.Bandwidth);
							return false;
						}
					}
					else if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO &&
							((pMediaStreamContext->lastSegmentTime - pMediaStreamContext->fragmentDescriptor.Time) > TIMELINE_START_RESET_DIFF))
					{
						if(!mIsLiveStream || !aamp->IsLiveAdjustRequired())
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time - 1;
							// CID:328774 - Data race condition
							return false;
						}
						AAMPLOG_WARN("Calling ScheduleRetune to handle start-time reset lastSegmentTime=%" PRIu64 " start-time=%f" , pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentDescriptor.Time);
						aamp->ScheduleRetune(eDASH_ERROR_STARTTIME_RESET, pMediaStreamContext->mediaType);
					}
					else
					{
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
						AAMPLOG_INFO("Type[%d] Before skipping. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d fragRep=%d,repMax=%d Number=%" PRIu64, pMediaStreamContext->type,
							pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime,pMediaStreamContext->timeLineIndex,
							pMediaStreamContext->fragmentRepeatCount , repeatCount,pMediaStreamContext->fragmentDescriptor.Number);
#endif
						if(!pMediaStreamContext->freshManifest)
						{
							while(pMediaStreamContext->fragmentDescriptor.Time < pMediaStreamContext->lastSegmentTime &&
									pMediaStreamContext->fragmentRepeatCount < repeatCount )
								{
									if(rate > 0)
									{
										pMediaStreamContext->fragmentDescriptor.Time += duration;
										pMediaStreamContext->fragmentDescriptor.Number++;
										pMediaStreamContext->fragmentRepeatCount++;
										pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time+duration;
										pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+1;
									}
								}
						}
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
						AAMPLOG_INFO("Type[%d] After skipping. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d Number=%" PRIu64, pMediaStreamContext->type,
								pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime,pMediaStreamContext->timeLineIndex,pMediaStreamContext->fragmentDescriptor.Number);
#endif
					}
					if(!pMediaStreamContext->freshManifest)
					{
						if(rate > 0)
						{
							pMediaStreamContext->fragmentDescriptor.Time += duration;
							pMediaStreamContext->fragmentDescriptor.Number++;
							pMediaStreamContext->fragmentRepeatCount++;
							if( pMediaStreamContext->fragmentRepeatCount > repeatCount)
							{
								pMediaStreamContext->fragmentRepeatCount = 0;
								pMediaStreamContext->timeLineIndex++;
							}
							pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time+duration;
							pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+1;
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
							AAMPLOG_INFO("Type[%d] After Incr. fragmentDescriptor.Time %f lastSegmentTime %" PRIu64 " Index=%d fragRep=%d,repMax=%d Number=%" PRIu64, pMediaStreamContext->type,
							pMediaStreamContext->fragmentDescriptor.Time, pMediaStreamContext->lastSegmentTime,pMediaStreamContext->timeLineIndex,
							pMediaStreamContext->fragmentRepeatCount , repeatCount,pMediaStreamContext->fragmentDescriptor.Number);
#endif
						}
						else
						{
							if (pMediaStreamContext->fragmentRepeatCount == 0 && pMediaStreamContext->timeLineIndex > 0)
							{
								//Going backwards and at the beginning of a timeline idx, get duration from preceding entry
								ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex - 1);
								duration = timeline->GetDuration();
							}
							pMediaStreamContext->fragmentDescriptor.Time -= duration;
							pMediaStreamContext->fragmentDescriptor.Number--;
							pMediaStreamContext->fragmentRepeatCount--;
							if( pMediaStreamContext->fragmentRepeatCount < 0)
							{
								pMediaStreamContext->timeLineIndex--;
								if(pMediaStreamContext->timeLineIndex >= 0)
								{
									// CID:306172 - Value not atomically updated
									pMediaStreamContext->fragmentRepeatCount = GetSegmentRepeatCount(pMediaStreamContext, pMediaStreamContext->timeLineIndex);
								}
							}
							pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number-1;
							pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time-duration;
						}
					}
					else
					{
						AAMPLOG_TRACE("Manifest refreshed during fragment download, type[%d]", pMediaStreamContext->type);
					}
				}
				 //< FailoverContent> can span more than one adjacent segment on a profile - in this case, client shall remain ramped down for the duration of the marked fragments (without re-injecting extra init header in-between)
				if((mFcsRepresentationId != pMediaStreamContext->representationIndex) && ( pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO))
				{
					std::vector<IFCS *>mFcsSegments;
					if(mFcsRepresentationId != -1)
					{
						if(pMediaStreamContext->adaptationSet!=NULL){
							const std::vector<IRepresentation *> representation = pMediaStreamContext->adaptationSet ->GetRepresentation();
							if(mFcsRepresentationId < (representation.size()-1)){
								const dash::mpd::IRepresentation *rep = representation.at(mFcsRepresentationId);
								if(rep){
									ISegmentTemplate *segmentTemplate = rep->GetSegmentTemplate();
									if (segmentTemplate)
									{
										const IFailoverContent *failoverContent = segmentTemplate->GetFailoverContent();
										if(failoverContent)
										{
											mFcsSegments = failoverContent->GetFCS();
											bool valid = failoverContent->IsValid();
											for(int i =0;i< mFcsSegments.size() && !valid;i++)
											{
												uint64_t starttime = mFcsSegments.at(i)->GetStartTime();
												uint64_t duration  =  mFcsSegments.at(i)->GetDuration();
												uint64_t fcscontent_range = starttime + duration ;
												if((starttime <= pMediaStreamContext->fragmentDescriptor.Time)&&(fcscontent_range > pMediaStreamContext->fragmentDescriptor.Time))
													pMediaStreamContext->failAdjacentSegment = true;
											}
										}
									}
								}}
						}}
					if(!pMediaStreamContext->failAdjacentSegment)
					{
						mFcsRepresentationId = pMediaStreamContext->representationIndex;
					}
				}
			}
			else
			{
				AAMPLOG_WARN("timelines is null");  //CID:81702 ,82851 - Null Returns
			}
		}
		else
		{
#if defined(DEBUG_TIMELINE) || defined(AAMP_SIMULATOR_BUILD)
			AAMPLOG_INFO("segmentTimeline not available");
#endif

			double currentTimeSeconds = (double)aamp_GetCurrentTimeMS() / 1000;

			uint32_t duration = segmentTemplates.GetDuration();
			double fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
			long startNumber = segmentTemplates.GetStartNumber();

			if (mMPDParseHelper->GetLiveTimeFragmentSync())
			{
				startNumber += (long)((mPeriodStartTime - mAvailabilityStartTime) / fragmentDuration);
			}

			uint32_t scale = segmentTemplates.GetTimescale();
			double pto =  (double) segmentTemplates.GetPresentationTimeOffset();
			AAMPLOG_TRACE("Type[%d] currentTimeSeconds:%f duration:%d fragmentDuration:%f startNumber:%ld", pMediaStreamContext->type, currentTimeSeconds,duration,fragmentDuration,startNumber);
			if (0 == pMediaStreamContext->lastSegmentNumber)
			{
				if (mIsLiveStream)
				{
					if(mHasServerUtcTime)
					{
						currentTimeSeconds+=mDeltaTime;
					}
					double liveTime = currentTimeSeconds - aamp->mLiveOffset;
					if(liveTime < mPeriodStartTime)
					{
						// Not to go beyond the period , as that is checked in skipfragments
						liveTime = mPeriodStartTime;
					}

					pMediaStreamContext->lastSegmentNumber = (long long)((liveTime - mPeriodStartTime) / fragmentDuration) + startNumber;
					pMediaStreamContext->fragmentDescriptor.Time = liveTime;
					AAMPLOG_INFO("Type[%d] Printing fragmentDescriptor.Number %" PRIu64 " Time=%f  ", pMediaStreamContext->type, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time);
				}
				else
				{
					if (rate < 0)
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodEndTime;
					}
					else
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
					}
					if(!aamp->IsLive()) pMediaStreamContext->lastSegmentNumber =  pMediaStreamContext->fragmentDescriptor.Number;
				}
			}

			// Recalculate the fragmentDescriptor.Time after periodic manifest updates
			if (mIsLiveStream && 0 == pMediaStreamContext->fragmentDescriptor.Time)
			{
				pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;

				if(pMediaStreamContext->lastSegmentNumber > startNumber )
				{
					pMediaStreamContext->fragmentDescriptor.Time += ((pMediaStreamContext->lastSegmentNumber - startNumber) * fragmentDuration);
				}
			}
			/**
			 *Find out if we reached end/beginning of period.
			 *First block in this 'if' is for VOD, where boundaries are 0 and PeriodEndTime
			 *Second block is for LIVE, where boundaries are
						 *  mPeriodStartTime and currentTime
			 */
			double fragmentRequestTime = 0.0f;
			double availabilityTimeOffset = 0.0f;
			if(mLowLatencyMode)
			{
				// Low Latency Mode will be pointing to edge of the fragment based on availablitystarttimeoffset,
				// and fragmentDescriptor time itself will be pointing to edge time when live offset is 0.
				// So adding the duration will cause the latency of fragment duration and sometime repeat the same content.
				availabilityTimeOffset =  aamp->GetLLDashServiceData()->availabilityTimeOffset;
				fragmentRequestTime = pMediaStreamContext->fragmentDescriptor.Time+(fragmentDuration-availabilityTimeOffset);
			}
			else
			{
				fragmentRequestTime = pMediaStreamContext->fragmentDescriptor.Time + fragmentDuration;
			}
			pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time + fragmentDuration;

			AAMPLOG_TRACE("fDesc.Time= %lf utcTime=%lf delta=%lf CTSeconds=%lf,FreqTime=%lf  nextfragTime : %lf",pMediaStreamContext->fragmentDescriptor.Time,
					mLocalUtcTime,mDeltaTime,currentTimeSeconds,fragmentRequestTime,pMediaStreamContext->fragmentDescriptor.nextfragmentTime);

			bool bProcessFragment = true;
			if(!mIsLiveStream)
			{
				if(ISCONFIGSET(eAAMPConfig_EnableIgnoreEosSmallFragment))
				{
					if(fragmentRequestTime >= mPeriodEndTime)
					{
						double fractionDuration = (mPeriodEndTime-pMediaStreamContext->fragmentDescriptor.Time)/fragmentDuration;
						bProcessFragment = (fractionDuration < MIN_SEG_DURATION_THRESHOLD)? false:true;
						AAMPLOG_TRACE("Type[%d] DIFF=%f Process Fragment=%d", pMediaStreamContext->type, fractionDuration, bProcessFragment);
					}
				}
				else
				{
					//Directly comparing two double values is unreliable because minor precision errors
					// here we introduce a 1ms epsilon to compensate and avoid injecting one segment too many at period boundary
					bProcessFragment = (pMediaStreamContext->fragmentDescriptor.Time+0.001 < mPeriodEndTime);
				}
			}
			if ((!mIsLiveStream && ((!bProcessFragment) || (rate < 0 )))
			|| (mIsLiveStream && (
				(mLowLatencyMode? pMediaStreamContext->fragmentDescriptor.Time>mPeriodEndTime+availabilityTimeOffset:pMediaStreamContext->fragmentDescriptor.Time >= mPeriodEndTime)
			|| (pMediaStreamContext->fragmentDescriptor.Time < mPeriodStartTime))))  //CID:93022 - No effect
			{
				AAMPLOG_INFO("Type[%d] EOS. pMediaStreamContext->lastSegmentNumber %" PRIu64 " fragmentDescriptor.Time=%f mPeriodEndTime=%f mPeriodStartTime %f  currentTimeSeconds %f FTime=%f", pMediaStreamContext->type, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time, mPeriodEndTime, mPeriodStartTime, currentTimeSeconds, pMediaStreamContext->fragmentTime);
				pMediaStreamContext->eos = true;
			}
			else if( mIsLiveStream &&  mHasServerUtcTime &&
					( mLowLatencyMode? fragmentRequestTime >= mLocalUtcTime+mDeltaTime : fragmentRequestTime >= mLocalUtcTime))
			{
				int sleepTime = MIN_DELAY_BETWEEN_MPD_UPDATE_MS;

				AAMPLOG_TRACE("With ServerUTCTime. Next fragment Not Available yet: fragmentDescriptor.Time %f fragmentDuration:%f currentTimeSeconds %f Local UTCTime %f sleepTime %d ", pMediaStreamContext->fragmentDescriptor.Time, fragmentDuration, currentTimeSeconds, mLocalUtcTime, sleepTime);
				aamp->interruptibleMsSleep(sleepTime);
				retval = false;
			}
			else if(mIsLiveStream && !mHasServerUtcTime &&
					(mLowLatencyMode?(fragmentRequestTime>=currentTimeSeconds):(fragmentRequestTime >= (currentTimeSeconds-mPresentationOffsetDelay))))
			{
				int sleepTime = MIN_DELAY_BETWEEN_MPD_UPDATE_MS;

				AAMPLOG_TRACE("Without ServerUTCTime. Next fragment Not Available yet: fragmentDescriptor.Time %f fragmentDuration:%f currentTimeSeconds %f Local UTCTime %f sleepTime %d ", pMediaStreamContext->fragmentDescriptor.Time, fragmentDuration, currentTimeSeconds, mLocalUtcTime, sleepTime);
				aamp->interruptibleMsSleep(sleepTime);
				retval = false;
			}
			else
			{
				if (mIsLiveStream)
				{
					pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->lastSegmentNumber;
				}
				uint64_t lastSegmentNumberBackup = pMediaStreamContext->fragmentDescriptor.Number;
				pMediaStreamContext->freshManifest = false;
				if(pMediaStreamContext->fragmentDescriptor.nextfragmentNum == -1)
				{
					pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+1;
				}
				if(!mIsFogTSB)
				{
					setNextobjectrequestUrl(media,&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
				}
				retval = FetchFragment(pMediaStreamContext, media, fragmentDuration, false, curlInstance, false, pto, scale);
				string startTimeStringValue = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetStart();
				pMediaStreamContext->lastDownloadedPosition = pMediaStreamContext->fragmentTime;
				AAMPLOG_INFO("[%s] lastDownloadedPosition %lfs fragmentTime %lfs",
					GetMediaTypeName(pMediaStreamContext->mediaType),
					pMediaStreamContext->lastDownloadedPosition.load(),
					pMediaStreamContext->fragmentTime);
				if( pMediaStreamContext->mCheckForRampdown )
				{
					/* NOTE : This case needs to be validated with the segmentTimeline not available stream */
					return retval;

				}

				if(!pMediaStreamContext->freshManifest)
				{
					if (rate > 0)
					{
						pMediaStreamContext->fragmentDescriptor.Number++;
						pMediaStreamContext->fragmentDescriptor.Time += fragmentDuration;
						pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+1;
					}
					else
					{
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentDescriptor.Time -= fragmentDuration;
						pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number-1;
					}
					pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
				}
				else if(retval == true)
				{
					if (rate > 0)
					{
						lastSegmentNumberBackup++;
					}
					else
					{
						lastSegmentNumberBackup--;
					}
					// Manifest changed while successful fragment download, save te backup number as lastSegmentNumber
					pMediaStreamContext->lastSegmentNumber = lastSegmentNumberBackup;
				}
				AAMPLOG_TRACE("Type[%d] Printing fragmentDescriptor.Number %" PRIu64 " Time=%f  ", pMediaStreamContext->type, pMediaStreamContext->lastSegmentNumber, pMediaStreamContext->fragmentDescriptor.Time);
			}
			//if pipeline is currently clear and if encrypted period is found in the last updated mpd, then do an internal retune
			//to configure the pipeline the for encrypted content
			if(mIsLiveStream && aamp->mEncryptedPeriodFound && aamp->mPipelineIsClear)
			{
				AAMPLOG_WARN("Retuning as encrypted pipeline found when pipeline is configured as clear");
				aamp->ScheduleRetune(eDASH_RECONFIGURE_FOR_ENC_PERIOD,(AampMediaType) pMediaStreamContext->type);
				AAMPLOG_INFO("Retune (due to enc pipeline) done");
				aamp->mEncryptedPeriodFound = false;
				aamp->mPipelineIsClear = false;
			}
		}
	}
	else
	{
		ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
		if (segmentBase)
		{ // single-segment
			std::string fragmentUrl;
			ConstructFragmentURL(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");
			if (!pMediaStreamContext->IDX.GetPtr() )
			{ // lazily load index
				std::string range = segmentBase->GetIndexRange();
				uint64_t start;
				sscanf(range.c_str(), "%" PRIu64 "-%" PRIu64 "", &start, &pMediaStreamContext->fragmentOffset);

				ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(pMediaStreamContext->mediaType, true);
				AampMediaType actualType = MediaTypeToPlaylist(pMediaStreamContext->mediaType);
				std::string effectiveUrl;
				int http_code;
				double downloadTime;
				int iFogError = -1;
				int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
				aamp->LoadIDX(bucketType, fragmentUrl, effectiveUrl,&pMediaStreamContext->IDX, curlInstance, range.c_str(),&http_code, &downloadTime, actualType,&iFogError);


				if (iCurrentRate != AAMP_NORMAL_PLAY_RATE)
				{
					if(actualType == eMEDIATYPE_VIDEO)
					{
						actualType = eMEDIATYPE_IFRAME;
					}
					//CID:101284 - To resolve  deadcode
					else if(actualType == eMEDIATYPE_INIT_VIDEO)
					{
						actualType = eMEDIATYPE_INIT_IFRAME;
					}
				}

				//update videoend info
				aamp->UpdateVideoEndMetrics( actualType,
										pMediaStreamContext->fragmentDescriptor.Bandwidth,
										(iFogError > 0 ? iFogError : http_code),effectiveUrl,pMediaStreamContext->fragmentDescriptor.Time, downloadTime);

				pMediaStreamContext->fragmentOffset++; // first byte following packed index
				if (pMediaStreamContext->IDX.GetPtr() )
				{
					unsigned int firstOffset;
					ParseSegmentIndexBox(
										 pMediaStreamContext->IDX.GetPtr(),
										 pMediaStreamContext->IDX.GetLen(),
										 0,
										 NULL,
										 NULL,
										 &firstOffset);
					pMediaStreamContext->fragmentOffset += firstOffset;
				}
				if (pMediaStreamContext->fragmentIndex != 0 && pMediaStreamContext->IDX.GetPtr() )
				{
					unsigned int referenced_size;
					float fragmentDuration;
					AAMPLOG_INFO("current fragmentIndex = %d", pMediaStreamContext->fragmentIndex);
					//Find the offset of previous fragment in new representation
					for (int i = 0; i < pMediaStreamContext->fragmentIndex; i++)
					{
						if (ParseSegmentIndexBox(
												 pMediaStreamContext->IDX.GetPtr(),
												 pMediaStreamContext->IDX.GetLen(),
												 i,
												 &referenced_size,
												 &fragmentDuration,
												 NULL))
						{
							pMediaStreamContext->fragmentOffset += referenced_size;
						}
					}
				}
			}
			if (pMediaStreamContext->IDX.GetPtr() )
			{
				unsigned int referenced_size;
				float fragmentDuration;
				if (ParseSegmentIndexBox(
										 pMediaStreamContext->IDX.GetPtr(),
										 pMediaStreamContext->IDX.GetLen(),
										 pMediaStreamContext->fragmentIndex++,
										 &referenced_size,
										 &fragmentDuration,
										 NULL) )
				{
					char range[MAX_RANGE_STRING_CHARS];
					snprintf(range, sizeof(range), "%" PRIu64 "-%" PRIu64 "", pMediaStreamContext->fragmentOffset, pMediaStreamContext->fragmentOffset + referenced_size - 1);
					AAMPLOG_INFO("%s [%s]",GetMediaTypeName(pMediaStreamContext->mediaType), range);
					unsigned int nextReferencedSize;
					float nextfragmentDuration;
					uint64_t nextfragmentOffset;
					if (ParseSegmentIndexBox(
							pMediaStreamContext->IDX.GetPtr(),
							pMediaStreamContext->IDX.GetLen(),
							pMediaStreamContext->fragmentIndex,
							&nextReferencedSize,
							&nextfragmentDuration,
							NULL))
					{
						char nextrange[MAX_RANGE_STRING_CHARS];
						nextfragmentOffset = pMediaStreamContext->fragmentOffset+referenced_size;
						snprintf(nextrange, sizeof(nextrange), "%" PRIu64 "-%" PRIu64 "",nextfragmentOffset, nextfragmentOffset+nextReferencedSize - 1);
						setNextRangeRequest(fragmentUrl,nextrange,(&pMediaStreamContext->fragmentDescriptor)->Bandwidth,AampMediaType(pMediaStreamContext->type));
					}
					if(pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, pMediaStreamContext->fragmentTime, fragmentDuration, range ))
					{
						pMediaStreamContext->fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentOffset += referenced_size;
						retval = true;
					}
					// pMediaStreamContext->lastDownloadedPosition is introduced to calculate the buffered duration value for SegmentBase contents.
					//Absolute position reporting
					pMediaStreamContext->lastDownloadedPosition = pMediaStreamContext->fragmentTime;
					AAMPLOG_INFO("[%s] lastDownloadedPosition %lfs fragmentTime %lfs",
						GetMediaTypeName(pMediaStreamContext->mediaType),
						pMediaStreamContext->lastDownloadedPosition.load(),
						pMediaStreamContext->fragmentTime);
				}
				else
				{ // done with index
					pMediaStreamContext->IDX.Free();
					pMediaStreamContext->eos = true;
				}
			}
			else
			{
				pMediaStreamContext->eos = true;
			}
		}
		else
		{
			ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
			if(pMediaStreamContext->nextfragmentIndex == -1)
			{
				pMediaStreamContext->nextfragmentIndex = pMediaStreamContext->fragmentIndex +1;
			}
			if (segmentList)
			{
				const std::vector<ISegmentURL*>segmentURLs = segmentList->GetSegmentURLs();
				if (pMediaStreamContext->fragmentIndex >= segmentURLs.size() ||  pMediaStreamContext->fragmentIndex < 0)
				{
					pMediaStreamContext->eos = true;
				}
				else if(!segmentURLs.empty())
				{
					ISegmentURL *segmentURL = segmentURLs.at(pMediaStreamContext->fragmentIndex);
					ISegmentURL *nextsegmentURL = NULL;
					// Avoid setting the nextfragmentIndex url when it reaches the last fragmentIndex
					if(pMediaStreamContext->nextfragmentIndex < segmentURLs.size())
					{
						nextsegmentURL = segmentURLs.at(pMediaStreamContext->nextfragmentIndex);
					}
					if(segmentURL != NULL)
					{

						std::map<string,string> rawAttributes = segmentList->GetRawAttributes();
						if(rawAttributes.find("customlist") == rawAttributes.end()) //"CheckForFogSegmentList")
						{
							std::string fragmentUrl;
							ConstructFragmentURL(fragmentUrl, &pMediaStreamContext->fragmentDescriptor,  segmentURL->GetMediaURI());
							AAMPLOG_INFO("%s [%s]", GetMediaTypeName(pMediaStreamContext->mediaType), segmentURL->GetMediaRange().c_str());
							if(nextsegmentURL != NULL && (mIsFogTSB != true))
							{
								setNextobjectrequestUrl(nextsegmentURL->GetMediaURI(),&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
							}
							double fragmentDurationS = ComputeFragmentDuration(segmentList->GetDuration(), segmentList->GetTimescale());
							if( pMediaStreamContext->CacheFragment(fragmentUrl, curlInstance, pMediaStreamContext->fragmentTime, fragmentDurationS, segmentURL->GetMediaRange().c_str() ) )
							{
								pMediaStreamContext->fragmentTime += fragmentDurationS;
							}
							else
							{
								AAMPLOG_WARN("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
							}
						}
						else //We are processing the custom segment list provided by Fog for DASH TSB
						{
							uint32_t timeScale = segmentList->GetTimescale();
							string durationStr = segmentURL->GetRawAttributes().at("d");
							string startTimestr = segmentURL->GetRawAttributes().at("s");
							uint32_t duration = (uint32_t)stoull(durationStr);
							long long startTime = stoll(startTimestr);
							// CID:337057 - Division or modulo by zero
							if (duration == 0)
							{
								AAMPLOG_WARN("Zero duration in TSB");
								return false;
							}
							else if(startTime > pMediaStreamContext->lastSegmentTime || 0 == pMediaStreamContext->lastSegmentTime || rate < 0 )
							{
								/*
									Added to inject appropriate initialization header in
									the case of fog custom mpd
								*/
								if(eMEDIATYPE_VIDEO == pMediaStreamContext->mediaType)
								{
									uint32_t bitrate = 0;
									std::map<string,string> rawAttributes =  segmentURL->GetRawAttributes();
									if(rawAttributes.find("bitrate") == rawAttributes.end()){
										bitrate = pMediaStreamContext->fragmentDescriptor.Bandwidth;
									}else{
										string bitrateStr = rawAttributes["bitrate"];
										bitrate = stoi(bitrateStr);
									}
									if(pMediaStreamContext->fragmentDescriptor.Bandwidth != bitrate || pMediaStreamContext->profileChanged)
									{
										pMediaStreamContext->fragmentDescriptor.Bandwidth = bitrate;
										pMediaStreamContext->profileChanged = true;
										profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(bitrate);
										// CID:306172 - Value not atomically updated
										FetchAndInjectInitialization(eMEDIATYPE_VIDEO);
										UpdateRampUpOrDownProfileReason();
										pMediaStreamContext->SetCurrentBandWidth(pMediaStreamContext->fragmentDescriptor.Bandwidth);
										return false; //Since we need to check WaitForFreeFragmentCache
									}
								}
								double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
								pMediaStreamContext->lastSegmentTime = startTime;
								if(nextsegmentURL != NULL && (mIsFogTSB != true))
								{
									setNextobjectrequestUrl(nextsegmentURL->GetMediaURI(),&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
								}
								retval = FetchFragment(pMediaStreamContext, segmentURL->GetMediaURI(), fragmentDuration, false, curlInstance);
								if( pMediaStreamContext->mCheckForRampdown )
								{
									/* This case needs to be validated with the segmentList available stream */
									return retval;
								}
							}
							else if(pMediaStreamContext->mediaType == eMEDIATYPE_VIDEO && duration > 0 && ((pMediaStreamContext->lastSegmentTime - startTime) > TIMELINE_START_RESET_DIFF))
							{
								AAMPLOG_WARN("START-TIME RESET in TSB period, lastSegmentTime=%" PRIu64 " start-time=%lld duration=%u", pMediaStreamContext->lastSegmentTime, startTime, duration);
								pMediaStreamContext->lastSegmentTime = startTime - 1;
								return retval;
							}
							else
							{
								int index = pMediaStreamContext->fragmentIndex + 1;
								int listSize = (int)segmentURLs.size();

								/*Added this block to reduce the skip overhead for custom mpd after
								 *MPD refresh
								*/
								int nextIndex;
								if( duration==0 )
								{
									AAMPLOG_WARN( "zero duration" );
									nextIndex = -1;
								}
								else
								{
									nextIndex = (int)((pMediaStreamContext->lastSegmentTime - startTime) / duration) - 5;
								}
								while(nextIndex > 0 && nextIndex < listSize)
								{
									segmentURL = segmentURLs.at(nextIndex);
									string startTimestr = segmentURL->GetRawAttributes().at("s");
									startTime = stoll(startTimestr);
									if(startTime > pMediaStreamContext->lastSegmentTime)
									{
										nextIndex -= 5;
										continue;
									}
									else
									{
										index = nextIndex;
										break;
									}
								}

								while(startTime < pMediaStreamContext->lastSegmentTime && index < listSize)
								{
									segmentURL = segmentURLs.at(index);
									string startTimestr = segmentURL->GetRawAttributes().at("s");
									startTime = stoll(startTimestr);
									index++;
								}
								pMediaStreamContext->fragmentIndex = index - 1;
								 AAMPLOG_TRACE("PushNextFragment Exit : startTime %lld lastSegmentTime %" PRIu64 " index = %d", startTime, pMediaStreamContext->lastSegmentTime, pMediaStreamContext->fragmentIndex);
							}
						}
						if(rate > 0)
						{
							pMediaStreamContext->fragmentIndex++;
							pMediaStreamContext->nextfragmentIndex = pMediaStreamContext->fragmentIndex+1;
						}
						else
						{
							pMediaStreamContext->fragmentIndex--;
							pMediaStreamContext->nextfragmentIndex = pMediaStreamContext->fragmentIndex-1;
						}

					}
					else
					{
						AAMPLOG_WARN("segmentURL    is null");  //CID:82493 ,86180 - Null Returns
					}
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: SegmentUrl is empty");
				}
			}
			else
			{
				IBaseUrl *baseURL = pMediaStreamContext->representation->GetBaseURLs().at(0);
				if(baseURL && (pMediaStreamContext->mediaType == eMEDIATYPE_SUBTITLE))
				{
					pMediaStreamContext->eos = true;
				}
				else
				{
					AAMPLOG_ERR(" not-yet-supported mpd format");
				}
			}
		}
	}
	return retval;
}


/**
 * @brief Seek current period by a given time
 */
void StreamAbstractionAAMP_MPD::SeekInPeriod( double seekPositionSeconds, bool skipToEnd)
{
	for (int i = 0; i < mNumberOfTracks; i++)
	{
		if (eMEDIATYPE_SUBTITLE == i)
		{
			double skipTime = seekPositionSeconds;
			SkipFragments(mMediaStreamContext[i], skipTime, true);
		}
		else
		{
			SkipFragments(mMediaStreamContext[i], seekPositionSeconds, true, skipToEnd);
		}
	}
}

/**
 * @brief Find the fragment based on the system clock after the SAP.
 */
void StreamAbstractionAAMP_MPD::ApplyLiveOffsetWorkaroundForSAP( double seekPositionSeconds)
{
	for(int i = 0; i < mNumberOfTracks; i++)
	{
		SegmentTemplates segmentTemplates(mMediaStreamContext[i]->representation->GetSegmentTemplate(),
		mMediaStreamContext[i]->adaptationSet->GetSegmentTemplate() );
		if( segmentTemplates.HasSegmentTemplate() )
		{
			const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if(segmentTimeline)
			{
				SeekInPeriod(seekPositionSeconds);
				break;
			}
			else
			{
				double currentplaybacktime = aamp->mOffsetFromTunetimeForSAPWorkaround;
				long startNumber = segmentTemplates.GetStartNumber();
				uint32_t duration = segmentTemplates.GetDuration();
				uint32_t timeScale = segmentTemplates.GetTimescale();
				double fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
				if(currentplaybacktime < mPeriodStartTime)
				{
					currentplaybacktime = mPeriodStartTime;
				}
				mMediaStreamContext[i]->fragmentDescriptor.Number = (long long)((currentplaybacktime - mPeriodStartTime) / fragmentDuration) + startNumber - 1;
				mMediaStreamContext[i]->fragmentDescriptor.Time = currentplaybacktime - fragmentDuration;
				// This is incomplete workaround, need full API rework
				mMediaStreamContext[i]->fragmentTime = mMediaStreamContext[i]->fragmentDescriptor.Time = currentplaybacktime - fragmentDuration;
				mMediaStreamContext[i]->lastSegmentNumber= mMediaStreamContext[i]->fragmentDescriptor.Number;
				AAMPLOG_INFO("moffsetFromStart:%f startNumber:%ld mPeriodStartTime:%f fragmentDescriptor.Number:%" PRIu64 " >fragmentDescriptor.Time:%f  mLiveOffset:%f seekPositionSeconds:%f"
				,aamp->mOffsetFromTunetimeForSAPWorkaround,startNumber,mPeriodStartTime, mMediaStreamContext[i]->fragmentDescriptor.Number,mMediaStreamContext[i]->fragmentDescriptor.Time,aamp->mLiveOffset,seekPositionSeconds);
			}
		}
		else
		{
			SeekInPeriod(seekPositionSeconds);
			break;
		}
	}
}

/**
 * @brief Skip to end of track
 */
void StreamAbstractionAAMP_MPD::SkipToEnd( MediaStreamContext *pMediaStreamContext)
{
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
	if( segmentTemplates.HasSegmentTemplate() )
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if(!timelines.empty())
			{
				uint32_t repeatCount = 0;
				// Store the first 't' value in fragmentDescriptor.Time
				ITimeline *firstTimeline = timelines.at(0);
				map<string, string> attributeMap = firstTimeline->GetRawAttributes();
				if(attributeMap.find("t") != attributeMap.end())
				{
					pMediaStreamContext->fragmentDescriptor.Time = static_cast<double>(firstTimeline->GetStartTime());
				}
				else
				{
					pMediaStreamContext->fragmentDescriptor.Time = 0;
				}

				for(int i = 0; i < timelines.size(); i++)
				{
					ITimeline *timeline = timelines.at(i);
					repeatCount += (timeline->GetRepeatCount() + 1);
					pMediaStreamContext->fragmentDescriptor.Time += ((timeline->GetRepeatCount() + 1) * timeline->GetDuration());
				}
				pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->fragmentDescriptor.Number + repeatCount - 1;
				pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
				pMediaStreamContext->timeLineIndex = (int)timelines.size() - 1;
				pMediaStreamContext->fragmentRepeatCount = timelines.at(pMediaStreamContext->timeLineIndex)->GetRepeatCount();
			}
			else
			{
				AAMPLOG_WARN("timelines is null");  //CID:82016,84031 - Null Returns
			}
		}
		else
		{
			double segmentDuration = ComputeFragmentDuration(segmentTemplates.GetDuration(), segmentTemplates.GetTimescale() );
			double startTime = mPeriodStartTime;
			int number = 0;
			while(startTime < mPeriodEndTime)
			{
				startTime += segmentDuration;
				number++;
			}
			pMediaStreamContext->fragmentDescriptor.Number = pMediaStreamContext->fragmentDescriptor.Number + number - 1;
		}
	}
	else
	{
		ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
		if (segmentList)
		{
			const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
			pMediaStreamContext->fragmentIndex = (int)segmentURLs.size() - 1;
		}
		else
		{
			AAMPLOG_ERR("not-yet-supported mpd format");
		}
	}
}


/**
 * @brief Skip fragments by given time
 */
double StreamAbstractionAAMP_MPD::SkipFragments( MediaStreamContext *pMediaStreamContext, double skipTime, bool updateFirstPTS, bool skipToEnd)
{
	if( !pMediaStreamContext->representation )
	{ // avoid crash with video-only content
		return 0.0;
	}
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
	if( segmentTemplates.HasSegmentTemplate() )
	{
		 AAMPLOG_INFO("Enter : Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentTime %f skipTime %f segNumber %" PRIu64" Ftime:%f" ,pMediaStreamContext->type,
								pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentTime, skipTime, pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentDescriptor.Time);

		gboolean firstFrag = true;

		std::string media = segmentTemplates.Getmedia();
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		uint64_t pto = segmentTemplates.GetPresentationTimeOffset();
		// Align to PTO if present for segmentTimeline
		// For segment template, its handled in its own way
		// Only for updateFirstPTS, we need to align to PTO, for trickplay its not required
		if (segmentTimeline && (pto > 0) && updateFirstPTS)
		{
			uint64_t startTime = 0;
			const auto& timelines = segmentTimeline->GetTimelines();
			ITimeline *timeline = timelines.at(0);
			map<string, string> attributeMap = timeline->GetRawAttributes();
			if(attributeMap.find("t") != attributeMap.end())
			{
				startTime = timeline->GetStartTime();
			}
			// TODO: For cases where PTO is less than startTime, unclear what needs to be done.
			if (pto > startTime)
			{
				double offset = (double)(pto - startTime) / (double)segmentTemplates.GetTimescale();
				AAMPLOG_INFO("Adding PTO offset:%lf to skipTime: %lf", offset, skipTime);
				skipTime += offset;
			}
		}
		do
		{
			if (segmentTimeline)
			{
				uint32_t timeScale = segmentTemplates.GetTimescale();
				std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
				if (pMediaStreamContext->timeLineIndex >= timelines.size() || pMediaStreamContext->timeLineIndex < 0)
				{
					AAMPLOG_INFO("Type[%d] EOS. timeLineIndex[%d] size [%zu]",pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
					pMediaStreamContext->eos = true;
					break;
				}
				else
				{
					ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
					uint32_t repeatCount = timeline->GetRepeatCount();
					if (pMediaStreamContext->fragmentRepeatCount == 0)
					{
						map<string, string> attributeMap = timeline->GetRawAttributes();
						if(attributeMap.find("t") != attributeMap.end())
						{
							uint64_t startTime = timeline->GetStartTime();
							pMediaStreamContext->fragmentDescriptor.Time = startTime;
						}
					}

					/*
					The following variables point to the segment that will be the next to play/skip over when
					playing in a fwd direction
						pMediaStreamContext->fragmentRepeatCount
						pMediaStreamContext->timeLineIndex
					When going in reverse then the 'next' segment will be before these variables and if we are already
					pointing to the first entry in the timeLineIndex then the duration will come from the previous
					timeLineIndex
					<SegmentTimeline>
						  <S t="927972765613" d="336000" r="0" />     <-- the duration we want for rew
						  <S t="927973101613" d="326400" r="0" />     <------timeLineIndex
						  <S t="927973428013" d="460800" r="43" />
					</SegmentTimeline>
					*/
					uint32_t duration = timeline->GetDuration();
					if (skipTime < 0)
					{
						// rewind
						if (pMediaStreamContext->fragmentRepeatCount == 0 && pMediaStreamContext->timeLineIndex > 0)
						{
							ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex - 1);
							duration = timeline->GetDuration();
						}
					}

					double fragmentDuration = ComputeFragmentDuration(duration,timeScale);
					double nextPTS = (double)(pMediaStreamContext->fragmentDescriptor.Time + duration)/timeScale;
					double firstPTS = (double)pMediaStreamContext->fragmentDescriptor.Time/timeScale;
					if (firstFrag && updateFirstPTS)
					{
						if (skipToEnd)
						{
							AAMPLOG_INFO("Processing skipToEnd for track type %d", pMediaStreamContext->type);
						}
						else
						{
							//When player started with rewind mode during trickplay, make sure that the skip time calculation would not spill over the duration of the content
							if(aamp->playerStartedWithTrickPlay && aamp->rate < 0 && skipTime > fragmentDuration )
							{
								AAMPLOG_INFO("Player switched in rewind mode, adjusted skptime from %f to %f ", skipTime, skipTime - fragmentDuration);
								skipTime -= fragmentDuration;
							}
							if (pMediaStreamContext->type == eTRACK_AUDIO && (mFirstVideoFragPTS || mFirstPTS || mVideoPosRemainder) && ( !pMediaStreamContext->refreshAudio ))
							{
								/* In case of the Seamless Audio Switch enabled scenario, no need to adjust the skiptime, as the video fragment PTS alignment is not performed in this case
								* so if the skip time is adjusted wrong audio fragments downloaded, so Audio mute issue is observed if we switch the track, so we handled that the below case won't
								* execute during the seamless audio switch scenario
								*/
								/* need to adjust audio skipTime/seekPosition so 1st audio fragment sent matches start of 1st video fragment being sent */
								double newSkipTime = skipTime + (mFirstVideoFragPTS - firstPTS); /* adjust for audio/video frag start PTS differences */
								newSkipTime -= mVideoPosRemainder;   /* adjust for mVideoPosRemainder, which is (video seekposition/skipTime - mFirstPTS) */
								newSkipTime += fragmentDuration/4.0; /* adjust for case where video start is near end of current audio fragment by adding to the audio skipTime, pushing it into the next fragment, if close(within this adjustment) */
								skipTime = newSkipTime;
							}
						}
						firstFrag = false;
						// Save the first video fragment PTS for checking alignment with audio first PTS
						if (pMediaStreamContext->type == eTRACK_VIDEO)
						{
							mFirstVideoFragPTS = firstPTS;
						}
					}

					if (skipToEnd)
					{
						if ((pMediaStreamContext->fragmentRepeatCount == repeatCount) &&
							(pMediaStreamContext->timeLineIndex + 1 == timelines.size()))
						{
							skipTime = 0; // We have reached the last fragment
						}
						else
						{
							if (updateFirstPTS)
							{
								pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
								pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
							}
							pMediaStreamContext->fragmentTime += fragmentDuration;
							pMediaStreamContext->fragmentTime = ceil(pMediaStreamContext->fragmentTime * 1000.0) / 1000.0;
							pMediaStreamContext->fragmentDescriptor.Time += duration;
							pMediaStreamContext->fragmentDescriptor.Number++;
							pMediaStreamContext->fragmentRepeatCount++;
							if( pMediaStreamContext->fragmentRepeatCount > repeatCount)
							{
								pMediaStreamContext->fragmentRepeatCount= 0;
								pMediaStreamContext->timeLineIndex++;
							}

							continue;  /* continue to next fragment */
						}
					}
					//Content Audio and Video Played for 1-2 seconds when we seek after Ad break.
					//Even if skiptime is equal to fragmentduration(eg: skipTime = 1.190600 & fragmentDuration=1.190600 this is based on logs)
					//it is not entering the loop which is leading to go back to 2 seconds of previous period content and play,
					//then jump to next period. The issue here is complier is optimizing the value to 1.18999 for skiptime where as
					//fragment duration is optimized to 1.190600. so adding floating point precision.
					else if (skipTime >= fragmentDuration - FLOATING_POINT_EPSILON)
					{
						if (updateFirstPTS)
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
							pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
						}
						skipTime -= fragmentDuration;
						pMediaStreamContext->fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentDescriptor.Time += duration;
						pMediaStreamContext->fragmentDescriptor.Number++;
						pMediaStreamContext->fragmentRepeatCount++;
						if( pMediaStreamContext->fragmentRepeatCount > repeatCount)
						{
							pMediaStreamContext->fragmentRepeatCount= 0;
							pMediaStreamContext->timeLineIndex++;
						}
						continue;  /* continue to next fragment */
					}
					else if (-(skipTime) >= fragmentDuration)
					{
						if (updateFirstPTS)
						{
							pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time;
							pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time + duration;
						}
						skipTime += fragmentDuration;
						pMediaStreamContext->fragmentTime -= fragmentDuration;
						pMediaStreamContext->fragmentDescriptor.Time -= duration;
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentRepeatCount--;
						if( pMediaStreamContext->fragmentRepeatCount < 0)
						{
							pMediaStreamContext->timeLineIndex--;
							if(pMediaStreamContext->timeLineIndex >= 0)
							{
								pMediaStreamContext->fragmentRepeatCount = timelines.at(pMediaStreamContext->timeLineIndex)->GetRepeatCount();
							}
						}
						continue;  /* continue to next fragment */
					}
					if (abs(skipTime) < fragmentDuration)
					{ // last iteration
						AAMPLOG_INFO("[%s] firstPTS %f, nextPTS %f  skipTime %f  fragmentDuration %f ", pMediaStreamContext->name, firstPTS, nextPTS, skipTime, fragmentDuration);
						if (updateFirstPTS)
						{
							/*Keep the lower PTS */
							if ( ((mFirstPTS == 0) || (firstPTS < mFirstPTS)) && (pMediaStreamContext->type == eTRACK_VIDEO))
							{
								AAMPLOG_INFO("[%s] mFirstPTS %f -> %f ", pMediaStreamContext->name, mFirstPTS, firstPTS);
								mFirstPTS = firstPTS;
								// Here, the firstPTS is known from timeline, so set it as final PTS
								mIsFinalFirstPTS = true;
								mVideoPosRemainder = skipTime;
								if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
								{
									mFirstPTS += mVideoPosRemainder;
									if(mVideoPosRemainder > fragmentDuration/2)
									{
										if(aamp->GetInitialBufferDuration() == 0)
										{
											AAMPPlayerState state = aamp->GetState();
											if(state == eSTATE_SEEKING)
											{
												// To prevent underflow when seeked to end of fragment.
												// Added +1 to ensure next fragment is fetched.
												SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,(int)fragmentDuration + 1);
												aamp->midFragmentSeekCache = true;
											}
										}
									}
									else if(aamp->midFragmentSeekCache)
									{
										// Resetting fragment cache when seeked to first half of the fragment duration.
										SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,0);
										aamp->midFragmentSeekCache = false;
									}

								}
								AAMPLOG_INFO("[%s] mFirstPTS %f  mVideoPosRemainder %f", pMediaStreamContext->name, mFirstPTS, mVideoPosRemainder);
							}
						}
						skipTime = 0;
						// This is a special case, when we rewind to the first fragment in period, its hitting fragmentRepeatCount==0 login PushNextFragment
						// For now, avoid this
						if (rate < 0 && pMediaStreamContext->timeLineIndex == 0 && pMediaStreamContext->fragmentRepeatCount == 0 &&
								pMediaStreamContext->fragmentDescriptor.Time == 0)
						{
							AAMPLOG_INFO("Reached last fragment in the period during rewind");
							pMediaStreamContext->mReachedFirstFragOnRewind = true;
						}
						if (pMediaStreamContext->type == eTRACK_AUDIO){
//							AAMPLOG_TRACE("audio/video PTS offset %f  audio %f video %f", firstPTS-mFirstPTS, firstPTS, mFirstPTS);
							if (abs(firstPTS - mFirstPTS)> 1.00){
								AAMPLOG_WARN("audio/video PTS offset Large %f  audio %f video %f",  firstPTS-mFirstPTS, firstPTS, mFirstPTS);
							}
						}
						break;
					}
				}
			}
			else
			{
				if(0 == pMediaStreamContext->fragmentDescriptor.Time)
				{
					if (rate < 0)
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodEndTime;
					}
					else
					{
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
					}
				}

				if(pMediaStreamContext->fragmentDescriptor.Time > mPeriodEndTime || (rate < 0 && pMediaStreamContext->fragmentDescriptor.Time <= 0))
				{
					AAMPLOG_INFO("Type[%d] EOS. fragmentDescriptor.Time=%f",pMediaStreamContext->type, pMediaStreamContext->fragmentDescriptor.Time);
					pMediaStreamContext->eos = true;
					break;
				}
				else
				{
					uint32_t timeScale = segmentTemplates.GetTimescale();
					double segmentDuration = ComputeFragmentDuration( segmentTemplates.GetDuration(), timeScale );

					if (skipToEnd)
					{
						skipTime = mPeriodEndTime - pMediaStreamContext->fragmentDescriptor.Time;
						if ( skipTime > segmentDuration )
						{
							skipTime -= segmentDuration;
						}
						else
						{
							skipTime = 0;
						}
					}

					if(!aamp->IsLive())
					{
						if( timeScale )
						{
							mFirstPTS = (double)segmentTemplates.GetPresentationTimeOffset() / (double)timeScale;
						}
						if( updateFirstPTS )
						{
							mFirstPTS += skipTime;
							// Here, PTO is known from manifest, so set it as final PTS
							mIsFinalFirstPTS = true;
							AAMPLOG_DEBUG("Type[%d] updateFirstPTS: %f SkipTime: %f",pMediaStreamContext->type, mFirstPTS, skipTime);
						}
					}
					if (skipTime >= segmentDuration)
					{ // seeking past more than one segment
						uint64_t number = skipTime / segmentDuration;
						double fragmentTimeFromNumber = segmentDuration * number;

						pMediaStreamContext->fragmentDescriptor.Number += number;
						pMediaStreamContext->fragmentDescriptor.Time += fragmentTimeFromNumber;
						pMediaStreamContext->fragmentTime = mPeriodStartTime + fragmentTimeFromNumber;

						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						skipTime -= fragmentTimeFromNumber;
						if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
						{
							mVideoPosRemainder = skipTime;
							AAMPLOG_INFO("[%s] mFirstPTS %f  mVideoPosRemainder %f", pMediaStreamContext->name, mFirstPTS, mVideoPosRemainder);
						}
						break;
					}
					else if (-(skipTime) >= segmentDuration)
					{
						pMediaStreamContext->fragmentDescriptor.Number--;
						pMediaStreamContext->fragmentTime -= segmentDuration;
						pMediaStreamContext->fragmentDescriptor.Time -= segmentDuration;
						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						skipTime += segmentDuration;
					}
					else if(skipTime == 0)
					{
						// Linear or VOD in both the cases if offset is set to 0 then this code will execute.
						// if no offset set then there is back up code in PushNextFragment function
						// which will take care of setting fragmentDescriptor.Time
						// based on live offset(linear) or period start ( vod ) on (pMediaStreamContext->lastSegmentNumber ==0 ) condition
						pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number;
						pMediaStreamContext->fragmentDescriptor.Time = mPeriodStartTime;
						break;
					}
					else if(abs(skipTime) < segmentDuration)
					{
						if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
						{
							mVideoPosRemainder = skipTime;
							AAMPLOG_INFO("[%s] mFirstPTS %f  mVideoPosRemainder %f", pMediaStreamContext->name, mFirstPTS, mVideoPosRemainder);
						}
						break;
					}
				}
			}
			if( skipTime==0 ) AAMPLOG_WARN( "skipTime is 0" );
		} while(true);

		AAMPLOG_INFO("Exit :Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentDescriptor.Number %" PRIu64 " fragmentTime %f FTime:%f",pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentDescriptor.Number, pMediaStreamContext->fragmentTime,pMediaStreamContext->fragmentDescriptor.Time);
	}
	else
	{
		ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
		if (segmentBase)
		{ // single-segment
			std::string range = segmentBase->GetIndexRange();
			if (!pMediaStreamContext->IDX.GetPtr() )
			{   // lazily load index
				std::string fragmentUrl;
				ConstructFragmentURL(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");

				//update the next segment for download

				ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(pMediaStreamContext->mediaType, true);
				AampMediaType actualType = MediaTypeToPlaylist(pMediaStreamContext->mediaType);
				std::string effectiveUrl;
				int http_code;
				double downloadTime;
				int iFogError = -1;
				aamp->LoadIDX(bucketType, fragmentUrl, effectiveUrl, &pMediaStreamContext->IDX, pMediaStreamContext->mediaType, range.c_str(),&http_code, &downloadTime, actualType,&iFogError);
			}
			if (pMediaStreamContext->IDX.GetPtr() )
			{
				unsigned int referenced_size = 0;
				float fragmentDuration = 0.00;
				float fragmentTime = 0.00;
				int fragmentIndex =0;

				unsigned int lastReferencedSize = 0;
				float lastFragmentDuration = 0.00;
				double presentationTimeOffsetSec = segmentBase->GetPresentationTimeOffset();

				if(segmentBase->GetTimescale())
				{
					presentationTimeOffsetSec /= ((double)segmentBase->GetTimescale());
				}

				AAMPLOG_INFO("SegmentBase: Presentation time offset present:%lf", presentationTimeOffsetSec);


				while ((fragmentTime < skipTime + presentationTimeOffsetSec ) || skipToEnd)
				{
					if (ParseSegmentIndexBox(
											 pMediaStreamContext->IDX.GetPtr(),
											 pMediaStreamContext->IDX.GetLen(),
											 fragmentIndex++,
											 &referenced_size,
											 &fragmentDuration,
											 NULL))
					{
						lastFragmentDuration = fragmentDuration;
						lastReferencedSize = referenced_size;

						fragmentTime += fragmentDuration;
						pMediaStreamContext->fragmentOffset += referenced_size;
					}
					else if (skipToEnd)
					{
						fragmentTime -= lastFragmentDuration;
						pMediaStreamContext->fragmentOffset -= lastReferencedSize;
						fragmentIndex--;
						break;
					}
					else
					{
						// done with index
						pMediaStreamContext->IDX.Free();
						pMediaStreamContext->eos = true;
						break;
					}
				}

				mFirstPTS = fragmentTime;
				//updated seeked position
				pMediaStreamContext->fragmentIndex = fragmentIndex;
				pMediaStreamContext->fragmentTime = mPeriodStartTime + (fragmentTime -  presentationTimeOffsetSec);
			}
			else
			{
				pMediaStreamContext->eos = true;
			}
		}
		else
		{
		ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
		if (segmentList)
		{
			AAMPLOG_INFO("Enter : fragmentIndex %d skipTime %f",
					pMediaStreamContext->fragmentIndex, skipTime);
			const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
			double segmentDuration = 0;
			if(!segmentURLs.empty())
			{
				std::map<string,string> rawAttributes = segmentList->GetRawAttributes();
				uint32_t timescale = segmentList->GetTimescale();
				bool isFogTsb = !(rawAttributes.find("customlist") == rawAttributes.end());
				if(!isFogTsb)
				{
					segmentDuration = ComputeFragmentDuration( segmentList->GetDuration() , timescale);
				}
				else if(pMediaStreamContext->type == eTRACK_AUDIO)
				{
					MediaStreamContext *videoContext = mMediaStreamContext[eMEDIATYPE_VIDEO];
					if(videoContext != NULL)
					{
						const std::vector<ISegmentURL*> vidSegmentURLs = videoContext->representation->GetSegmentList()->GetSegmentURLs();
						if(!vidSegmentURLs.empty())
						{
							string videoStartStr = vidSegmentURLs.at(0)->GetRawAttributes().at("s");
							string audioStartStr = segmentURLs.at(0)->GetRawAttributes().at("s");
							long long videoStart = stoll(videoStartStr);
							long long audioStart = stoll(audioStartStr);
							long long diff = audioStart - videoStart;
							AAMPLOG_WARN("Printing diff value for adjusting %lld",diff);
							if(diff > 0)
							{
								double diffSeconds = double(diff) / timescale;
								skipTime -= diffSeconds;
							}
						}
						else
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Video SegmentUrl is empty");
						}
					}
					else
					{
						AAMPLOG_WARN("videoContext  is null");  //CID:82361 - Null Returns
					}
				}

				while ((skipTime != 0) || skipToEnd)
				{
					if ((pMediaStreamContext->fragmentIndex >= segmentURLs.size()) || (pMediaStreamContext->fragmentIndex < 0))
					{
						pMediaStreamContext->eos = true;
						break;
					}
					else
					{
						//Calculate the individual segment duration for fog tsb
						if(isFogTsb)
						{
							ISegmentURL* segmentURL = segmentURLs.at(pMediaStreamContext->fragmentIndex);
							string durationStr = segmentURL->GetRawAttributes().at("d");
							uint32_t duration = (uint32_t)stoull(durationStr);
							segmentDuration = ComputeFragmentDuration(duration,timescale);
						}
						if (skipToEnd)
						{
							if ((pMediaStreamContext->fragmentIndex + 1) >= segmentURLs.size())
							{
								break;
							}

							pMediaStreamContext->fragmentIndex++;
							pMediaStreamContext->fragmentTime += segmentDuration;
						}
						else if (skipTime >= segmentDuration)
						{
							pMediaStreamContext->fragmentIndex++;
							skipTime -= segmentDuration;
							pMediaStreamContext->fragmentTime += segmentDuration;
						}
						else if (-(skipTime) >= segmentDuration)
						{
							pMediaStreamContext->fragmentIndex--;
							skipTime += segmentDuration;
							pMediaStreamContext->fragmentTime -= segmentDuration;
						}
						else
						{
							skipTime = 0;
							break;
						}
					}
				}
			}
			else
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: SegmentUrl is empty");
			}

			AAMPLOG_INFO("Exit : fragmentIndex %d segmentDuration %f",
					pMediaStreamContext->fragmentIndex, segmentDuration);
		}
		else
		{
			if(pMediaStreamContext->mediaType == eMEDIATYPE_SUBTITLE)
			{
				//Updating fragmentTime and fragmentDescriptor.Time with fisrtPTS
				//CacheFragment is called with both fragmentTime and fragmentDescriptor.Time
				pMediaStreamContext->fragmentTime = GetFirstPTS();
				pMediaStreamContext->fragmentDescriptor.Time = GetFirstPTS();

			}
			else
				AAMPLOG_ERR("not-yet-supported mpd format");
		}
		}
	}
	return skipTime;
}


void StreamAbstractionAAMP_MPD::ProcessManifestHeaderResponse(ManifestDownloadResponsePtr mpdDnldResp,
			bool init)
{
	if(aamp->IsEventListenerAvailable(AAMP_EVENT_HTTP_RESPONSE_HEADER) && (aamp->manifestHeadersNeeded.size()))
	{
		std::vector<std::string> manifestResponseHeader	=	mpdDnldResp->GetManifestDownloadHeaders();
		if(manifestResponseHeader.size())
		{
			// clear the existing response header, to pack new response header
			aamp->httpHeaderResponses.clear();
			std::vector<std::string> headersNeeded 			=	aamp->manifestHeadersNeeded;

			for ( std::string header : headersNeeded )
			{
				header.erase(std::remove_if(header.begin(), header.end(), ::isspace),header.end()); // normalize - strip whitespace
				for ( std::string availHeader : manifestResponseHeader )
				{
					auto delim = availHeader.find(':');
					if (delim != std::string::npos)
					{
						std::string headerString = availHeader.substr(0, delim); // normalize - strip whitespace
						headerString.erase(std::remove_if(headerString.begin(), headerString.end(), ::isspace),headerString.end());
						if(headerString == header)
						{
							aamp->httpHeaderResponses[header] = availHeader.substr(header.length()+2);
							break;
						}
					}
				}
			}
			// HTTP Response header needs to be sent to app when:
			// 1. HTTP header response event listener is available
			// 2. HTTP header response values are present
			// 3. Manifest refresh has happened during Live
			// Sending response after ProcessPlaylist is done
			// send the http response header values if available
			// For Init , message need to be sent after changing the state to PREPARING
			if(!init && !aamp->httpHeaderResponses.empty()) {
				aamp->SendHTTPHeaderResponse();
			}
		}
	}
}

/**
 * @brief Process Metadata from the manifest
 * @retval None
*/
void StreamAbstractionAAMP_MPD::ProcessMetadataFromManifest( ManifestDownloadResponsePtr mpdDnldResp, bool init)
{
	vector<std::string> locationUrl;
	// Store the mpd pointer which is already parsed in the MPDDownloader
	dash::mpd::IMPD *tmpMPD			=	mpdDnldResp->mMPDInstance.get();

	if (tmpMPD)
	{
		Node *root			=	mpdDnldResp->mRootNode;
		//If bulk metadata is enabled for live, all metadata should be reported as bulkmetadata event
		//and player should not send the same  events again.
		bool bMetadata		=	ISCONFIGSET(eAAMPConfig_BulkTimedMetaReport) || ISCONFIGSET(eAAMPConfig_BulkTimedMetaReportLive);
		FindTimedMetadata((dash::mpd::MPD *)tmpMPD, root, init, bMetadata);
		if(!init)
		{
			aamp->ReportTimedMetadata(bMetadata);
		}
		// get Network time
		mHasServerUtcTime = FindServerUTCTime(root);
		mMPDParseHelper->SetHasServerUtcTime(mHasServerUtcTime);
		// Find the gaps in the Period
		if(mIsFogTSB && ISCONFIGSET(eAAMPConfig_InterruptHandling))
		{
			FindPeriodGapsAndReport();
		}
		// Process VSS stream
		if(aamp->mIsVSS)
		{
			CheckForVssTags();
			if(!init)
			{
				ProcessVssLicenseRequest();
			}
		}
		// Process and send manifest http headers
		ProcessManifestHeaderResponse(mpdDnldResp,init);
	}
}


/**
 * @brief Get mpd object of manifest
 * @retval AAMPStatusType indicates if success or fail
*/
AAMPStatusType StreamAbstractionAAMP_MPD::GetMPDFromManifest( ManifestDownloadResponsePtr mpdDnldResp, bool init)
{
	AAMPStatusType ret = eAAMPSTATUS_MANIFEST_PARSE_ERROR;
	vector<std::string> locationUrl;
	// Store the mpd pointer which is already parsed in the MPDDownloader
	dash::mpd::IMPD *tmpMPD			=	mpdDnldResp->mMPDInstance.get();

	if (tmpMPD)
	{
		this->mpd	=	tmpMPD;
		// Parse for generic parameters
		mMPDParseHelper	=	mpdDnldResp->GetMPDParseHelper();

		//Node *root			=	mpdDnldResp->mRootNode;
		// this flag for current state of manifest ( Linear to VOD can happen)
		if((mMPDParseHelper->IsLiveManifest() != mIsLiveManifest) && !init )
		{
			AAMPLOG_INFO("Linear to Vod transition , update the manifest state");
			mUpdateManifestState = true;
		}
		else
		{
			mUpdateManifestState = false;
		}
		mIsLiveManifest		=	mMPDParseHelper->IsLiveManifest();
		aamp->SetIsLive(mIsLiveManifest);

		if(init)
		{
			// to be updated only for Init , state of the stream when playback started
			mIsLiveStream	=	mIsLiveManifest;
		}

		/* All manifest requests after the first should
		* reference the url from the Location _element_. This is per MPEG
		* specification */
		locationUrl = this->mpd->GetLocations();
		if( !locationUrl.empty() )
		{
			aamp->SetManifestUrl(locationUrl[0].c_str());
		}

		mLastPlaylistDownloadTimeMs = aamp_GetCurrentTimeMS();
		if(mIsLiveStream && ISCONFIGSET(eAAMPConfig_EnableClientDai))
		{
			mCdaiObject->PlaceAds(mMPDParseHelper);
		}

		ret = AAMPStatusType::eAAMPSTATUS_OK;
	}
	else
	{
		ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
	}
	return ret;
}


/**
 * @brief Parse XML NS
 * @param fullName full name of node
 * @param[out] ns namespace
 * @param[out] name name after :
 */
static void ParseXmlNS(const std::string& fullName, std::string& ns, std::string& name)
{
	size_t found = fullName.find(':');
	if (found != std::string::npos)
	{
		ns = fullName.substr(0, found);
		name = fullName.substr(found+1);
	}
	else
	{
		ns = "";
		name = fullName;
	}
}

/**
 * @brief Get the DRM preference value.
 * @return The preference level for the DRM type.
 */
int StreamAbstractionAAMP_MPD::GetDrmPrefs(const std::string& uuid)
{
	auto iter = mDrmPrefs.find(uuid);

	if (iter != mDrmPrefs.end())
	{
		return iter->second;
	}

	return 0; // Unknown DRM
}

/**
 * @brief Get the UUID of preferred DRM.
 * @return The UUID of preferred DRM
 */
std::string StreamAbstractionAAMP_MPD::GetPreferredDrmUUID()
{
	int selectedPref = 0;
	std::string selectedUuid = "";
	for (const auto& iter : mDrmPrefs)
	{
		if( iter.second > selectedPref){
			selectedPref = iter.second;
			selectedUuid = iter.first;
		}
	}
	return selectedUuid; // return uuid of preferred DRM
}

/**
 * @brief Create DRM helper from ContentProtection
 * @retval shared_ptr of DrmHelper
 */
DrmHelperPtr StreamAbstractionAAMP_MPD::CreateDrmHelper(const IAdaptationSet * adaptationSet,AampMediaType mediaType)
{
	const vector<IDescriptor*> contentProt = mMPDParseHelper->GetContentProtection(adaptationSet);
	unsigned char* data = NULL;
	unsigned char *outData = NULL;
	size_t outDataLen  = 0;
	size_t dataLength = 0;
	DrmHelperPtr tmpDrmHelper;
	DrmHelperPtr drmHelper = nullptr;
	DrmInfo drmInfo;
	std::string contentMetadata;
	std::string cencDefaultData;
	bool forceSelectDRM = false;
	const char *pMp4Protection = "mpeg:dash:mp4protection";

	AAMPLOG_TRACE("[HHH] contentProt.size=%zu", contentProt.size());
	for (unsigned iContentProt = 0; iContentProt < contentProt.size(); iContentProt++)
	{
		// extract the UUID
		std::string schemeIdUri = contentProt.at(iContentProt)->GetSchemeIdUri();
		if(schemeIdUri.find(pMp4Protection) != std::string::npos )
		{
			std::string Value = contentProt.at(iContentProt)->GetValue();
			//ToDo check the value key and use the same along with custom attribute such as default_KID
			auto attributesMap = contentProt.at(iContentProt)->GetRawAttributes();
			if(attributesMap.find("cenc:default_KID") != attributesMap.end())
			{
				cencDefaultData=attributesMap["cenc:default_KID"];
				AAMPLOG_INFO("cencDefaultData= %s", cencDefaultData.c_str());
			}
		}

		// Look for UUID in schemeIdUri by matching any UUID to maintain backwards compatibility
		std::regex rgx(".*([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}).*");
		std::smatch uuid;
		if (!std::regex_search(schemeIdUri, uuid, rgx))
		{
			AAMPLOG_WARN("(%s) got schemeID empty at ContentProtection node-%d", GetMediaTypeName(mediaType), iContentProt);
			continue;
		}

		drmInfo.method = eMETHOD_AES_128;
		drmInfo.mediaFormat = eMEDIAFORMAT_DASH;
		drmInfo.systemUUID = uuid[1];
		drmInfo.bPropagateUriParams = ISCONFIGSET(eAAMPConfig_PropagateURIParam);
		drmInfo.bDecryptClearSamplesRequired = aamp->isDecryptClearSamplesRequired();

		//Convert UUID to all lowercase
		std::transform(drmInfo.systemUUID.begin(), drmInfo.systemUUID.end(), drmInfo.systemUUID.begin(), [](unsigned char ch){ return std::tolower(ch); });

		// Extract the PSSH data
		const vector<INode*> node = contentProt.at(iContentProt)->GetAdditionalSubNodes();
		if (!node.empty())
		{
			for(int i=0; i < node.size(); i++)
			{
				std::string tagName = node.at(i)->GetName();
				/**< PSSH Data can be represented in <mspr:pro> tag also in PR
				 * reference : https://docs.microsoft.com/en-us/playready/specifications/mpeg-dash-playready#2-playready-dash-content-protection-scheme
				*/
				/*** TODO: Enable the condition after OCDM support added  */
				/** if ((tagName.find("pssh") != std::string::npos) || (tagName.find("mspr:pro") != std::string::npos))*/
				if (tagName.find("pssh") != std::string::npos)
				{
					string psshData = node.at(i)->GetText();
					data = base64_Decode(psshData.c_str(), &dataLength);
					if (0 == dataLength)
					{
						AAMPLOG_WARN("base64_Decode of pssh resulted in 0 length");
						if (data)
						{
							free(data);
							data = NULL;
						}
					}
					else
					{
						/**< Time being giving priority to cenc:ppsh if it is present */
						break;
					}
				}
				else if (tagName.find("mspr:pro") != std::string::npos)
				{
					AAMPLOG_WARN("Unsupported  PSSH data format - MSPR found in manifest");
				}
			}
		}

		// A special PSSH is used to signal data to append to the widevine challenge request
		if (drmInfo.systemUUID == CONSEC_AGNOSTIC_UUID)
		{
			if (data)
			{
				contentMetadata = DrmUtils::extractWVContentMetadataFromPssh((const char*)data, (int)dataLength);
				free(data);
								data = NULL;
			}
			else
			{
				AAMPLOG_WARN("data is NULL");
			}
			continue;
		}

		if (aamp->mIsWVKIDWorkaround && drmInfo.systemUUID == CLEARKEY_UUID ){
			/* WideVine KeyID workaround present , UUID change from clear key to widevine **/
			AAMPLOG_INFO("WideVine KeyID workaround present, processing KeyID from clear key to WV Data");
			drmInfo.systemUUID = WIDEVINE_UUID;
			forceSelectDRM = true; /** Need this variable to understand widevine has chosen **/
			outData = aamp->ReplaceKeyIDPsshData(data, dataLength, outDataLen );
			if (outData){
				if(data) free(data);
				data = 	outData;
				dataLength = outDataLen;
			}
		}

		// Try and create a DRM helper
		if (!DrmHelperEngine::getInstance().hasDRM(drmInfo))
		{
			AAMPLOG_WARN("(%s) Failed to locate DRM helper for UUID %s", GetMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
			/** Preferred DRM configured and it is failed hhen exit here */
			if(aamp->isPreferredDRMConfigured && (GetPreferredDrmUUID() == drmInfo.systemUUID) && !aamp->mIsWVKIDWorkaround){
				AAMPLOG_ERR("(%s) Preferred DRM Failed to locate with UUID %s", GetMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
				if (data)
				{
					free(data);
					data = NULL;
				}
				break;
			}
		}
		else if (data && dataLength)
		{
			tmpDrmHelper = DrmHelperEngine::getInstance().createHelper(drmInfo);

			if (!tmpDrmHelper->parsePssh(data, (uint32_t)dataLength))
			{
				AAMPLOG_WARN("(%s) Failed to Parse PSSH from the DRM InitData", GetMediaTypeName(mediaType));
			}
			else
			{
				if (forceSelectDRM){
					AAMPLOG_INFO("(%s) If Widevine DRM Selected due to Widevine KeyID workaround",
						GetMediaTypeName(mediaType));
					drmHelper = tmpDrmHelper;
					/** No need to progress further**/
					free(data);
					data = NULL;
					break;
				}

				// Track the best DRM available to use
				else if ((!drmHelper) || (GetDrmPrefs(drmInfo.systemUUID) > GetDrmPrefs(drmHelper->getUuid())))
				{
					AAMPLOG_WARN("(%s) Created DRM helper for UUID %s and best to use", GetMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
					drmHelper = tmpDrmHelper;
				}
			}
		}
		else
		{
			AAMPLOG_WARN("(%s) No PSSH data available from the stream for UUID %s", GetMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
			/** Preferred DRM configured and it is failed then exit here */
			if(aamp->isPreferredDRMConfigured && (GetPreferredDrmUUID() == drmInfo.systemUUID)&& !aamp->mIsWVKIDWorkaround){
				AAMPLOG_ERR("(%s) No PSSH data available for Preferred DRM with UUID  %s", GetMediaTypeName(mediaType), drmInfo.systemUUID.c_str());
				if (data)
				{
					free(data);
					data = NULL;
				}
				break;
			}
		}

		if (data)
		{
			free(data);
			data = NULL;
		}
	}

	if(drmHelper)
	{
		drmHelper->setDrmMetaData(contentMetadata);
		drmHelper->setDefaultKeyID(cencDefaultData);
	}

	return drmHelper;
}

/**
* @brief Function to Process deferred VSS license requests
*/
void StreamAbstractionAAMP_MPD::ProcessVssLicenseRequest()
{
	std::vector<IPeriod*> vssPeriods;
	// Collect only new vss periods from manifest
	GetAvailableVSSPeriods(vssPeriods);
	for (auto tempPeriod : vssPeriods)
	{
		if (NULL != tempPeriod)
		{
			// Save new period ID and create DRM helper for that
			mEarlyAvailablePeriodIds.push_back(tempPeriod->GetId());
			DrmHelperPtr drmHelper = CreateDrmHelper(tempPeriod->GetAdaptationSets().at(0), eMEDIATYPE_VIDEO);
			// Identify key ID from parsed PSSH data
			std::vector<uint8_t> keyIdArray;
			drmHelper->getKey(keyIdArray);

			if (!keyIdArray.empty())
			{
				std::string keyIdDebugStr = AampLogManager::getHexDebugStr(keyIdArray);
				AAMPLOG_INFO("New VSS Period : %s Key ID: %s", tempPeriod->GetId().c_str(), keyIdDebugStr.c_str());
				QueueContentProtection(tempPeriod, 0, eMEDIATYPE_VIDEO, true, true);
			}
			else
			{
				AAMPLOG_WARN("Failed to get keyID for vss common key EAP");
			}
		}
	}
}

/**
 * @fn QueueContentProtection
 * @param[in] period - period
 * @param[in] adaptationSetIdx - adaptation set index
 * @param[in] mediaType - media type
 * @param[in] qGstProtectEvent - Flag denotes if GST protection event should be queued in sink
 * @brief queue content protection for the given adaptation set
 * @retval true on success
 */
void StreamAbstractionAAMP_MPD::QueueContentProtection(IPeriod* period, uint32_t adaptationSetIdx, AampMediaType mediaType, bool qGstProtectEvent, bool isVssPeriod)
{
	if (period)
	{
		if( adaptationSetIdx < period->GetAdaptationSets().size() )
		{
			IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(adaptationSetIdx);
			if (adaptationSet)
			{
				DrmHelperPtr drmHelper = CreateDrmHelper(adaptationSet, mediaType);
				if (drmHelper)
				{
					if (aamp->mDRMLicenseManager)
					{
						AampDRMLicenseManager *licenseMgr = aamp->mDRMLicenseManager;
						if (qGstProtectEvent)
						{
							/** Queue protection event to the pipeline **/
							licenseMgr->QueueProtectionEvent(drmHelper, period->GetId(), adaptationSetIdx, mediaType);
						}
						/** Queue content protection in DRM license fetcher **/
						licenseMgr->QueueContentProtection(drmHelper, period->GetId(), adaptationSetIdx, mediaType, isVssPeriod);
					}
					hasDrm = true;
					aamp->licenceFromManifest = true;
				}
			}
		}
	}
}

/**
 * @brief Parse CC streamID and language from value
 *
 * @param[in] input - input value
 * @param[out] id - stream ID value
 * @param[out] lang - language value
 * @return void
 */
void ParseCCStreamIDAndLang(std::string input, std::string &id, std::string &lang)
{
	// Expected formats : 	eng;deu
	// 			CC1=eng;CC2=deu
	//			1=lang:eng;2=lang:deu
	//			1=lang:eng;2=lang:eng,war:1,er:1
		size_t delim = input.find('=');
		if (delim != std::string::npos)
		{
				id = input.substr(0, delim);
				lang = input.substr(delim + 1);

		//Parse for additional fields
				delim = lang.find(':');
				if (delim != std::string::npos)
				{
						size_t count = lang.find(',');
						if (count != std::string::npos)
						{
								count = (count - delim - 1);
						}
						lang = lang.substr(delim + 1, count);
				}
		}
		else
		{
				lang = input;
		}
}

/**
 *   @brief  Initialize TSB reader
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_MPD::InitTsbReader(TuneType tuneType)
{
	AAMPStatusType retVal = eAAMPSTATUS_OK;
	mTuneType = tuneType;
	double livePlayPosition = aamp->GetLivePlayPosition();
	AAMPLOG_INFO("TuneType:%d seek position: %lfs, livePlayPos: %lfs, Offset: %lfs, culledSeconds from actual manifest: %lf", tuneType, seekPosition, livePlayPosition, aamp->mLiveOffset, mCulledSeconds);
	AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
	if(NULL != tsbSessionManager)
	{
		double position = seekPosition;
		if((eTUNETYPE_SEEKTOLIVE == tuneType) || (seekPosition >= livePlayPosition))
		{
			position = livePlayPosition;
			AAMPLOG_INFO("Adjusting to live play position: %lfs, totalDuration: %lfs", position, aamp->durationSeconds);
			if(AAMP_NORMAL_PLAY_RATE == aamp->rate && !aamp->GetPauseOnFirstVideoFrameDisp())
			{
				if (aamp->GetLLDashChunkMode())
				{
					AAMPLOG_INFO("Re-enabling LLD DASH speed correction");
					aamp->SetLLDashAdjustSpeed(true);
				}
				mTuneType = eTUNETYPE_SEEKTOLIVE;
			}
			mIsAtLivePoint = true;
			aamp->NotifyOnEnteringLive();
		}

		retVal = tsbSessionManager->InvokeTsbReaders(position, aamp->rate, mTuneType);

		if(eAAMPSTATUS_OK == retVal)
		{
			seekPosition = position;
			mFirstPTS = tsbSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->GetFirstPTS();
			AAMPLOG_MIL("Updated position: %lfs, pts:%lfs", seekPosition, mFirstPTS);
			if (aamp->IsLocalAAMPTsbInjection())
			{
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					if (mMediaStreamContext[i] != NULL)
					{
						mMediaStreamContext[i]->SetLocalTSBInjection(true);
					}
				}
			}
		}
		else
		{
			retVal = eAAMPSTATUS_SEEK_RANGE_ERROR;
		}
	}
	else
	{
		AAMPLOG_ERR("TSBSessionManager not found for seek/trickplay");
		retVal = eAAMPSTATUS_INVALID_PLAYLIST_ERROR;
	}
	return retVal;
}

/**
 *   @brief  Initialize a newly created object.
 *   @note   To be implemented by sub classes
 *   @retval true on success
 *   @retval false on failure
 */
AAMPStatusType StreamAbstractionAAMP_MPD::Init(TuneType tuneType)
{
	bool forceSpeedsChangedEvent = false;
	AAMPStatusType retval = eAAMPSTATUS_OK;
	aamp->CurlInit(eCURLINSTANCE_VIDEO, DEFAULT_CURL_INSTANCE_COUNT, aamp->GetNetworkProxy());
	mCdaiObject->ResetState();
	aamp->SetLLDashChunkMode(false); //Reset ChunkMode
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);
	if (sink)
	{
		sink->ClearProtectionEvent();
	}
	AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
	bool forceClearSession = (!ISCONFIGSET(eAAMPConfig_SetLicenseCaching) && (tuneType == eTUNETYPE_NEW_NORMAL));
	licenseManager->clearDrmSession(forceClearSession);
	licenseManager->clearFailedKeyIds();
	licenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
	licenseManager->setLicenseRequestAbort(false);
	aamp->licenceFromManifest = false;
	bool newTune = aamp->IsNewTune();

	if(newTune)
	{
		//Clear previously stored vss early period ids
		mEarlyAvailablePeriodIds.clear();
	}

	aamp->IsTuneTypeNew = newTune;

	bool pushEncInitFragment = newTune || (eTUNETYPE_RETUNE == tuneType) || aamp->mbDetached;
	if(aamp->mbDetached){
			/* No more needed reset it **/
			aamp->mbDetached = false;
	}

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs, (AampCurlInstance)i);
	}

	AAMPStatusType ret = FetchDashManifest();
	if (ret == eAAMPSTATUS_OK)
	{
		std::string manifestUrl = aamp->GetManifestUrl();
		mMaxTracks = (rate == AAMP_NORMAL_PLAY_RATE) ? AAMP_TRACK_COUNT : 1;
		double offsetFromStart = seekPosition;
		uint64_t durationMs = 0;
		mNumberOfTracks = 0;
		bool mpdDurationAvailable = false;
		std::string tempString;
		// Start the worker threads for each track
		InitializeWorkers();
		if(mpd == NULL)
		{
			AAMPLOG_WARN("mpd is null");  //CID:81139 , 81645 ,82315.83556- Null Returns
			return ret;
		}

		durationMs = mMPDParseHelper->GetMediaPresentationDuration();
		mpdDurationAvailable = true;
		AAMPLOG_MIL("StreamAbstractionAAMP_MPD: MPD duration val %" PRIu64 " seconds", durationMs/1000);

		mIsLiveStream = mMPDParseHelper->IsLiveManifest();
		aamp->SetIsLive(mIsLiveStream);
		if(newTune)
		{
			aamp->SetIsLiveStream(mIsLiveStream);
		}
		if(mMPDParseHelper->IsFogMPD())
		{
			mCdaiObject->mIsFogTSB = mIsFogTSB	=  true;
		}

		if(ContentType_UNKNOWN == aamp->GetContentType())
		{
			if(mIsLiveStream)
				aamp->SetContentType("LINEAR_TV");
			else
				aamp->SetContentType("VOD");
		}

		// Validate tune type
		// (need to find a better way to do this)
		if (tuneType == eTUNETYPE_NEW_NORMAL)
		{
			if(!mIsLiveStream && !aamp->mIsDefaultOffset)
			{
				tuneType = eTUNETYPE_NEW_END;
			}
		}

		if(mIsLiveStream)
		{
			/** Set preferred live Offset*/
			aamp->mIsStream4K = GetPreferredLiveOffsetFromConfig();
			/*LL DASH VERIFICATION START*/
			ret = EnableAndSetLiveOffsetForLLDashPlayback((MPD*)this->mpd);
			if(eAAMPSTATUS_OK != ret && ret == eAAMPSTATUS_MANIFEST_PARSE_ERROR)
			{
				aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
				return ret;
			}
			if (aamp->mIsVSS)
			{
				std::string vssVirtualStreamId = GetVssVirtualStreamID();

				if (!vssVirtualStreamId.empty())
				{
					AAMPLOG_INFO("Virtual stream ID :%s", vssVirtualStreamId.c_str());
					aamp->SetVssVirtualStreamID(vssVirtualStreamId);
				}
			}


			mMinUpdateDurationMs		=	mMPDParseHelper->GetMinUpdateDurationMs();
			mAvailabilityStartTime		=	mMPDParseHelper->GetAvailabilityStartTime();
			mTSBDepth					=	mMPDParseHelper->GetTSBDepth();
			mPresentationOffsetDelay	=	mMPDParseHelper->GetPresentationOffsetDelay();
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: AvailabilityStartTime=%f", mAvailabilityStartTime);

			mFirstPeriodStartTime = mMPDParseHelper->GetPeriodStartTime(0,mLastPlaylistDownloadTimeMs);
			if(aamp->mProgressReportOffset < 0)
			{
				// Progress offset for relative position, default value will be -1, since 0 is a possible offset.
				aamp->mProgressReportOffset = mFirstPeriodStartTime;
				aamp->mProgressReportAvailabilityOffset = mAvailabilityStartTime;
			}

			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: MPD minupdateduration val %" PRIu64 " seconds mTSBDepth %f mPresentationOffsetDelay :%f StartTimeFirstPeriod: %lf offsetStartTime: %lf",  mMinUpdateDurationMs/1000, mTSBDepth, mPresentationOffsetDelay, mFirstPeriodStartTime, aamp->mProgressReportOffset);

			aamp->mTsbDepthMs = CONVERT_SEC_TO_MS(mTSBDepth);

		}

		for (int i = 0; i < mMaxTracks; i++)
		{
			mMediaStreamContext[i] = new MediaStreamContext((TrackType)i, this, aamp, GetMediaTypeName(AampMediaType(i)));
			mMediaStreamContext[i]->fragmentDescriptor.manifestUrl = manifestUrl;
			mMediaStreamContext[i]->mediaType = (AampMediaType)i;
			mMediaStreamContext[i]->representationIndex = -1;
		}

		std::vector<PeriodInfo> currMPDPeriodDetails;
		uint64_t durMs = 0;
		UpdateMPDPeriodDetails(currMPDPeriodDetails,durMs);
		mMPDParseHelper->SetMPDPeriodDetails(currMPDPeriodDetails);

		uint64_t nextPeriodStart = 0;
		double currentPeriodStart = 0;
		double prevPeriodEndMs = 0; // used to find gaps between periods
		int numPeriods = (int)mMPDParseHelper->GetNumberOfPeriods();
		bool seekPeriods = true;

		if (!aamp->IsUninterruptedTSB())
		{
			double culled = aamp->culledSeconds;
			UpdateCulledAndDurationFromPeriodInfo(currMPDPeriodDetails);
			culled = aamp->culledSeconds - culled;
			if (culled > 0 && !newTune)
			{
				// Adjust the additional culled seconds from offset
				offsetFromStart -= culled;
				if(offsetFromStart < 0)
				{
					offsetFromStart = 0;
					AAMPLOG_WARN("Resetting offset from start to 0");
				}
			}
		}
		if (eTUNETYPE_NEW_NORMAL == tuneType)
		{
			double currentTime = NOW_STEADY_TS_SECS_FP;
			aamp->mLiveEdgeDeltaFromCurrentTime = currentTime - aamp->mAbsoluteEndPosition;
			AAMPLOG_INFO("currentTime %lfs mAbsoluteEndPosition %lfs mLiveEdgeDeltaFromCurrentTime %lfs", currentTime, aamp->mAbsoluteEndPosition, aamp->mLiveEdgeDeltaFromCurrentTime );
		}
		for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
		{//TODO -  test with streams having multiple periods.
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(mMPDParseHelper->IsEmptyPeriod(iPeriod, (rate != AAMP_NORMAL_PLAY_RATE)))
			{
				// Empty Period . Ignore processing, continue to next.
				continue;
			}
			std::string tempString = period->GetDuration();
			double  periodStartMs = 0;
			double periodDurationMs = 0;
			periodDurationMs =mMPDParseHelper->aamp_GetPeriodDuration(iPeriod, mLastPlaylistDownloadTimeMs);
			if (!mpdDurationAvailable)
			{
				durationMs += periodDurationMs;
				 AAMPLOG_INFO("Updated duration %lf seconds", ((double) durationMs/1000));
			 }

			if(offsetFromStart >= 0 && seekPeriods)
			{
				tempString = period->GetStart();
				if(!tempString.empty() && !aamp->IsUninterruptedTSB())
				{
					periodStartMs = ParseISO8601Duration( tempString.c_str() );
				}
				else if (periodDurationMs)
				{
					periodStartMs = nextPeriodStart;
				}

				if(aamp->IsLiveStream() && !aamp->IsUninterruptedTSB() && iPeriod == 0)
				{
					// Adjust start time wrt presentation time offset.
					if(!aamp->IsLive() && mAvailabilityStartTime > 0 )
					{
						periodStartMs += aamp->culledSeconds;
					}
					else
					{
						periodStartMs += (mMPDParseHelper->aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(period) * 1000);
					}
				}

				double periodStartSeconds = periodStartMs/1000;
				double periodDurationSeconds = (double)periodDurationMs / 1000;
				if (periodDurationMs != 0)
				{
					double periodEnd = periodStartMs + periodDurationMs;
					nextPeriodStart += periodDurationMs; // set the value here, nextPeriodStart is used below to identify "Multi period assets with no period duration" if it is set to ZERO.

					// check for gaps between periods
					if(prevPeriodEndMs > 0)
					{
						double periodGap = (periodStartMs - prevPeriodEndMs)/ 1000; // current period start - prev period end will give us GAP between period
						if(std::abs(periodGap) > 0 ) // ohh we have GAP between last and current period
						{
							offsetFromStart -= periodGap; // adjust offset to accommodate gap
							if(offsetFromStart < 0 ) // this means offset is between gap, set to start of currentPeriod
							{
								offsetFromStart = 0;
							}
							AAMPLOG_WARN("GAP between period found :GAP:%f  mCurrentPeriodIdx %d currentPeriodStart %f offsetFromStart %f",
								periodGap, mCurrentPeriodIdx, periodStartSeconds, offsetFromStart);
						}
						if(!mIsLiveStream && periodGap > 0 )
						{
							//increment period gaps to notify partner apps during manifest parsing for VOD assets
							aamp->IncrementGaps();
						}
					}
					prevPeriodEndMs = periodEnd; // store for future use
					currentPeriodStart = periodStartSeconds;
					mCurrentPeriodIdx = iPeriod;
					if (periodDurationSeconds <= offsetFromStart && iPeriod < (numPeriods - 1))
					{
						offsetFromStart -= periodDurationSeconds;
						AAMPLOG_WARN("Skipping period %d seekPosition %f periodEnd %f offsetFromStart %f", iPeriod, seekPosition, periodEnd, offsetFromStart);
						continue;
					}
					else
					{
						seekPeriods = false;
					}
				}
				else if(periodStartSeconds <= offsetFromStart)
				{
					mCurrentPeriodIdx = iPeriod;
					currentPeriodStart = periodStartSeconds;
				}
			}
		}

		//Check added to update offsetFromStart for
		//Multi period assets with no period duration
		if(0 == nextPeriodStart)
		{
			offsetFromStart -= currentPeriodStart;
		}
		bool segmentTagsPresent = true;
		//The OR condition is added to see if segment info is available in live MPD
		//else we need to calculate fragments based on time
		if (0 == durationMs || (mpdDurationAvailable && mIsLiveStream && !mIsFogTSB))
		{
			durationMs = mMPDParseHelper->GetDurationFromRepresentation();
			AAMPLOG_WARN("Duration after GetDurationFromRepresentation %" PRIu64 " seconds", durationMs/1000);
		}

		if(0 == durationMs)
		{
			segmentTagsPresent = false;
			for(int iPeriod = 0; iPeriod < numPeriods; iPeriod++)
			{
				if(mMPDParseHelper->IsEmptyPeriod(iPeriod, (rate != AAMP_NORMAL_PLAY_RATE)))
				{
					continue;
				}
				durationMs += mMPDParseHelper->GetPeriodDuration(iPeriod,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
			}
			AAMPLOG_WARN("Duration after adding up Period Duration %" PRIu64 " seconds", durationMs/1000);
		}
		/*Do live adjust on live streams on 1. eTUNETYPE_NEW_NORMAL, 2. eTUNETYPE_SEEKTOLIVE,
		 * 3. Seek to a point beyond duration*/
		bool notifyEnteringLive = false;
		if (mIsLiveStream)
		{
			double duration = (double) durationMs / 1000;
			mLiveEndPosition = aamp->mAbsoluteEndPosition;
			bool liveAdjust = (eTUNETYPE_NEW_NORMAL == tuneType) && aamp->IsLiveAdjustRequired();
			if (eTUNETYPE_SEEKTOLIVE == tuneType)
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: eTUNETYPE_SEEKTOLIVE");
				liveAdjust = true;
				notifyEnteringLive = true;
			}
			else if (((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_RETUNE == tuneType || eTUNETYPE_NEW_SEEK == tuneType)) && (rate > 0))
			{
				double seekWindowEnd = duration - aamp->mLiveOffset;
				// check if seek beyond live point
				if (seekPosition > seekWindowEnd)
				{
					AAMPLOG_WARN( "StreamAbstractionAAMP_MPD: offSetFromStart[%f] seekWindowEnd[%f] ",
							seekPosition, seekWindowEnd);
					liveAdjust = true;
					if (eTUNETYPE_SEEK == tuneType)
					{
						notifyEnteringLive = true;
					}
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Live latency correction is enabled due to the seek (rate=%f) to live window!!", rate);
					aamp->mDisableRateCorrection = false;
				}
				else
				{
					if((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_NEW_SEEK == tuneType))
					{
						if (mLowLatencyMode)
						{
							aamp->SetLLDashAdjustSpeed(false);
						}
					}
				}

				if (mLowLatencyMode && !liveAdjust)
				{
					int maxLatency = GETCONFIGVALUE(eAAMPConfig_LLMaxLatency);
					//Chunk mode is applied when the seek position is between the live edge and the maximum allowed latency from it
					if (seekPosition > (duration - maxLatency))
					{
						aamp->SetLLDashChunkMode(true);
						AAMPLOG_MIL("Chunk mode set: seekPosition (%f) not exceeded maxLatency (%d) threshold, enabling LLDashChunkMode", seekPosition, maxLatency);
					}
				}
			}

			if (liveAdjust)
			{
				if(mLowLatencyMode)
				{
					aamp->SetLLDashChunkMode(true);
					AAMPLOG_MIL("Chunk mode set: Enabling LLDashChunkMode due to liveadjust");
				}
				// After live adjust ( for Live or CDVR) , possibility of picking an empty last period exists.
				// Though its ignored in Period selection earlier , live adjust will end up picking last added empty period
				// Instead of picking blindly last period, pick the period the last period which contains some stream data
				mCurrentPeriodIdx = numPeriods;
				while( mCurrentPeriodIdx>0 )
				{
					mCurrentPeriodIdx--;
					if( !mMPDParseHelper->IsEmptyPeriod(mCurrentPeriodIdx, (rate != AAMP_NORMAL_PLAY_RATE)))
					{ // found last non-empty period
						break;
					}
				}

				if(aamp->IsLiveAdjustRequired())
				{
					if(segmentTagsPresent)
					{
						duration = (mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB())) / 1000;
						currentPeriodStart = ((double)durationMs / 1000) - duration;
						offsetFromStart = duration - aamp->mLiveOffset;
						while(offsetFromStart < 0 && mCurrentPeriodIdx > 0)
						{
							AAMPLOG_INFO("Adjusting to live offset offsetFromStart %f, mCurrentPeriodIdx %d", offsetFromStart, mCurrentPeriodIdx);
							mCurrentPeriodIdx--;
							while((mCurrentPeriodIdx > 0) && (mMPDParseHelper->IsEmptyPeriod(mCurrentPeriodIdx, (rate != AAMP_NORMAL_PLAY_RATE))))
							{
								mCurrentPeriodIdx--;
							}
							duration = (mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB())) / 1000;
							currentPeriodStart = currentPeriodStart - duration;
							offsetFromStart = offsetFromStart + duration;
						}
					}
					else
					{
						//Calculate live offset based on time elements in the mpd
						double currTime = (double)aamp_GetCurrentTimeMS() / 1000;
						double liveoffset = aamp->mLiveOffset;
						if(mTSBDepth && mTSBDepth < liveoffset)
						{
							liveoffset = mTSBDepth;
						}

						double startTime = currTime - liveoffset;
						if(startTime < 0)
							startTime = 0;
						currentPeriodStart = ((double)durationMs / 1000);
						while(mCurrentPeriodIdx >= 0)
						{
							while((mCurrentPeriodIdx > 0) && (mMPDParseHelper->IsEmptyPeriod(mCurrentPeriodIdx, (rate != AAMP_NORMAL_PLAY_RATE))))
							{
								mCurrentPeriodIdx--;
							}
							mPeriodStartTime =  mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs);
							duration = (mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB())) / 1000;
							currentPeriodStart -= duration;
							if(mPeriodStartTime < startTime)
							{
								offsetFromStart = startTime - mPeriodStartTime;
								break;
							}
							mCurrentPeriodIdx--;
						}
					}
					AAMPLOG_WARN("duration %f durationMs %f mCurrentPeriodIdx %d currentPeriodStart %f offsetFromStart %f",
				 duration, (double)durationMs / 1000, mCurrentPeriodIdx, currentPeriodStart, offsetFromStart);
				}
				else
				{
					IPeriod *period = mpd->GetPeriods().at(mCurrentPeriodIdx);
					uint64_t  periodStartMs = ParseISO8601Duration( period->GetStart().c_str() );
					currentPeriodStart = periodStartMs/(double)1000.0;
					offsetFromStart = duration - aamp->mLiveOffset - currentPeriodStart;
				}

				if (offsetFromStart < 0)
				{
					offsetFromStart = duration - aamp->mLiveOffset;
					if( offsetFromStart<0 )
					{ // clamp if negative
						offsetFromStart = 0;
					}
				}
				if(mCurrentPeriodIdx < 0)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Invalid currentPeriodIdx[%d], resetting to 0",mCurrentPeriodIdx);
					mCurrentPeriodIdx = 0;
				}
				mIsAtLivePoint = true;
				AAMPLOG_WARN( "StreamAbstractionAAMP_MPD: liveAdjust - Updated offSetFromStart[%f] duration [%f] currentPeriodStart[%f] MaxPeriodIdx[%d]",
						offsetFromStart, duration, currentPeriodStart,mCurrentPeriodIdx);
			}

		}
		else
		{
			// Non-live - VOD/CDVR(Completed)
			if(durationMs == INVALID_VOD_DURATION)
			{
				AAMPLOG_WARN("Duration of VOD content is 0");
				return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			}

			double seekWindowEnd = (double) durationMs / 1000;
			if(seekPosition > seekWindowEnd)
			{
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					mMediaStreamContext[i]->eosReached=true;
				}
				AAMPLOG_WARN("seek target out of range, mark EOS. playTarget:%f End:%f. ",
					seekPosition, seekWindowEnd);
				return eAAMPSTATUS_SEEK_RANGE_ERROR;
			}
		}
		mPeriodStartTime =  mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs);
		mPeriodDuration =  mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
		mPeriodEndTime = mMPDParseHelper->GetPeriodEndTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
		int periodCnt = numPeriods;
		if(mCurrentPeriodIdx < periodCnt)
		{
			mCurrentPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);
		}

		if(!aamp->IsUninterruptedTSB())
		{
			mStartTimeOfFirstPTS = mPeriodStartTime * 1000;
		}

		if(mCurrentPeriod != NULL)
		{
			mBasePeriodId = mCurrentPeriod->GetId();
			mIsSegmentTimelineEnabled = mMPDParseHelper->aamp_HasSegmentTimeline(mCurrentPeriod);
		}
		else
		{
			AAMPLOG_WARN("mCurrentPeriod  is null");  //CID:84770 - Null Return
		}
		mBasePeriodOffset = offsetFromStart;
		onAdEvent(AdEvent::INIT, offsetFromStart);

		UpdateLanguageList();

		if ((eTUNETYPE_SEEK == tuneType) ||
			(eTUNETYPE_SEEKTOEND == tuneType))
		{
			forceSpeedsChangedEvent = true; // Send speed change event if seek done from non-iframe period to iframe available period to inform XRE to allow trick operations.
		}

		StreamSelection(true, forceSpeedsChangedEvent);

		if(aamp->mIsFakeTune)
		{
			// Aborting init here after stream and DRM initialization.
			return eAAMPSTATUS_FAKE_TUNE_COMPLETE;
		}
		//calling ReportTimedMetadata function after drm creation in order
		//to reduce the delay caused
		aamp->ReportTimedMetadata(true);
		if(mAudioType == eAUDIO_UNSUPPORTED)
		{
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			aamp->SendErrorEvent(AAMP_TUNE_UNSUPPORTED_AUDIO_TYPE);
		}
		else if(mNumberOfTracks)
		{
			aamp->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_PLAYLIST_INDEXED, aamp->GetSessionId()),AAMP_EVENT_ASYNC_MODE);
			if (eTUNED_EVENT_ON_PLAYLIST_INDEXED == aamp->GetTuneEventConfig(mIsLiveStream))
			{
				if (aamp->SendTunedEvent())
				{
					AAMPLOG_WARN("aamp: mpd - sent tune event after indexing playlist");
				}
			}
			// Update track with update in stream info
			mUpdateStreamInfo = true;
			ret = UpdateTrackInfo(!newTune, true);

			if(eAAMPSTATUS_OK != ret)
			{
				if (ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
				{
					AAMPLOG_ERR("ERROR: No playable profiles found");
				}
				return ret;
			}

			if(!newTune && mIsFogTSB)
			{
				double culled = 0;
				if(mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled)
				{
					culled = GetCulledSeconds(currMPDPeriodDetails);
				}
				if(culled > 0)
				{
					AAMPLOG_INFO("Culled seconds = %f, Adjusting seekPos after considering new culled value", culled);
					aamp->UpdateCullingState(culled);
				}

				double durMs = 0;
				for(int periodIter = 0; periodIter < numPeriods; periodIter++)
				{
					if(!mMPDParseHelper->IsEmptyPeriod(periodIter, (rate != AAMP_NORMAL_PLAY_RATE)))
					{
						durMs += mMPDParseHelper->GetPeriodDuration(periodIter,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
					}
				}

				double duration = (double)durMs / 1000;
				aamp->UpdateDuration(duration);
			}

			if(notifyEnteringLive)
			{
				aamp->NotifyOnEnteringLive();
			}
			if(mIsLiveStream && aamp->mLanguageChangeInProgress)
			{
				aamp->mLanguageChangeInProgress = false;
				ApplyLiveOffsetWorkaroundForSAP(offsetFromStart);
			}
			else if ((tuneType == eTUNETYPE_SEEKTOEND) ||
				 (tuneType == eTUNETYPE_NEW_END))
			{
				SeekInPeriod( 0, true );
				seekPosition = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime;
			}
			else
			{
				if( (mLowLatencyMode ) &&
					(!aamp->GetLLDashServiceData()->isSegTimeLineBased) && \
					( ( eTUNETYPE_SEEK != tuneType   ) &&
					( eTUNETYPE_NEW_SEEK != tuneType ) &&
					( eTUNETYPE_NEW_END != tuneType ) &&
					( eTUNETYPE_SEEKTOEND != tuneType ) &&
					( eTUNETYPE_RETUNE != tuneType ) ) )
				{
					offsetFromStart = offsetFromStart+(aamp->GetLLDashServiceData()->fragmentDuration - aamp->GetLLDashServiceData()->availabilityTimeOffset);
				}
				else if( (mLowLatencyMode ) &&
					(aamp->GetLLDashServiceData()->isSegTimeLineBased) && \
					( (eTUNETYPE_SEEK != tuneType   ) &&
					( eTUNETYPE_NEW_SEEK != tuneType ) &&
					( eTUNETYPE_NEW_END != tuneType ) &&
					( eTUNETYPE_SEEKTOEND != tuneType ) &&
					( eTUNETYPE_RETUNE != tuneType ) ) )
				{
					double manifestEndDelta = ((double)mLastPlaylistDownloadTimeMs/1000.0) - mPeriodEndTime;
					if((manifestEndDelta > 0) && (manifestEndDelta <=  DEFAULT_ALLOWED_DELAY_LOW_LATENCY))
					{
						offsetFromStart = offsetFromStart + manifestEndDelta;
					}
					else if ( manifestEndDelta >  DEFAULT_ALLOWED_DELAY_LOW_LATENCY)
					{
						/** Only allowed shift (server delay) can be adjusted in AAMP*/
						offsetFromStart = offsetFromStart + DEFAULT_ALLOWED_DELAY_LOW_LATENCY;
					}

					AAMPLOG_INFO("manifestEndDelta = %lf mLastPlaylistDownloadTimeMs %lf mPeriodEndTime = %lf offsetFromStart = %lf",
					manifestEndDelta, (double)mLastPlaylistDownloadTimeMs/1000.0, mPeriodEndTime, offsetFromStart);
				}
				SeekInPeriod( offsetFromStart);
			}

			if(!ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
			{
				seekPosition = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime;
			}
			else
			{
				seekPosition = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime + mVideoPosRemainder;
			}

			for (int i = 0; i < mNumberOfTracks; i++)
			{
				//The default fragment time has been updated to an absolute time format. Therefore,
				//the periodStartOffset should now be relative to the Availability Start Time.
				mMediaStreamContext[i]->periodStartOffset = mPeriodStartTime;
				if (mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING && mCdaiObject->mCurAdIdx > 0)
				{
					//Ensuring basePeriodOffset has the proper value
					if (mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).basePeriodOffset != -1)
					{
						/*When multiple DAI ads are mapped to a single source period, the periodStartOffset
						needs to be updated with the basePeriodOffset for each ad. Otherwise,
						the CheckForAdTerminate() API will produce incorrect results. In this API,
						the offset is calculated as the difference between the fragment time and
						the basePeriodOffset.
						If not updated correctly, the offset will accumulate for each ad,
						leading to inaccuracies.*/
						mMediaStreamContext[i]->periodStartOffset += ((double)mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).basePeriodOffset / 1000.0);
					}
				}
			}
			if(mLowLatencyMode)
			{
				aamp->mLLActualOffset = seekPosition;
				AAMPLOG_INFO("LL-Dash speed correction %s", aamp->GetLLDashAdjustSpeed()? "enabled":"disabled");
			}
			AAMPLOG_INFO("offsetFromStart(%f) seekPosition(%lf) currentPeriodStart(%lf)", offsetFromStart,seekPosition, currentPeriodStart);

			if (newTune )
			{
				aamp->SetState(eSTATE_PREPARING);

				// send the http response header values if available
				if(!aamp->httpHeaderResponses.empty()) {
					aamp->SendHTTPHeaderResponse();
				}

				double durationSecond = (((double)durationMs)/1000);
				if (mMPDParseHelper->GetLiveTimeFragmentSync())
				{
					durationSecond = mPeriodDuration / 1000;
				}

				aamp->UpdateDuration(durationSecond);
				GetCulledSeconds(currMPDPeriodDetails);
				aamp->UpdateRefreshPlaylistInterval((float)mMinUpdateDurationMs / 1000);
				mProgramStartTime = mAvailabilityStartTime;
			}
			if(ISCONFIGSET(eAAMPConfig_EnableMediaProcessor))
			{
				// For segment timeline based streams, media processor is initialized in passthrough mode
				InitializeMediaProcessor(mIsSegmentTimelineEnabled);
			}
		}
		else
		{
			AAMPLOG_WARN("No adaptation sets could be selected");
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}
	}
	else if(ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
	{
		retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
	}
	else
	{
		AAMPLOG_ERR("StreamAbstractionAAMP_MPD: corrupt/invalid manifest");
		retval = eAAMPSTATUS_MANIFEST_PARSE_ERROR;
	}
	if (ret == eAAMPSTATUS_OK)
	{
		//CheckForInitalClearPeriod() check if the current period is clear or encrypted
		if (pushEncInitFragment && CheckForInitalClearPeriod())
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Pushing EncryptedHeaders");
			std::map<int, std::string> headers;

			// Getting the headers alone will not cache the protectionEvent in sink which fails to select drmSession in decryptor
			// Hence reorder to get the encrypted init fragment from manifest and if not check for header urls in StreamSinkManager
			// Check if a period in a multi-period asset has an encrypted content
			if(GetEncryptedHeaders(headers))
			{
				PushEncryptedHeaders(headers);
				aamp->mPipelineIsClear = false;
				aamp->mEncryptedPeriodFound = false;
			}
			else
			{
				// Check if single pipeline has a main asset that has
				// encrypted content whose init header urls have been saved
				AampStreamSinkManager::GetInstance().GetEncryptedHeaders(headers);
				if (!headers.empty())
				{
					PushEncryptedHeaders(headers);
					aamp->mPipelineIsClear = false;
					aamp->mEncryptedPeriodFound = false;
				}
				else if (mIsLiveStream)
				{
					AAMPLOG_INFO("Pipeline set as clear since no enc perid found");
					//If no encrypted period is found, then update the pipeline status
					aamp->mPipelineIsClear = true;
				}
			}
		}
		else
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Getting EncryptedHeaders");
			std::map<int, std::string> headers;

			if(GetEncryptedHeaders(headers))
			{
				AampStreamSinkManager::GetInstance().SetEncryptedHeaders(aamp, headers);
			}
		}

		// Rialto does not support dynamic streams, so we need to extract and save the 
		// subtitle init fragment from the main vod asset, so that it can be injected
		// later if a pre-roll advert is played that does not contain subtitles.
		if (ISCONFIGSET(eAAMPConfig_useRialtoSink) && 
		   !mIsLiveStream &&
		   (!(AampStreamSinkManager::GetInstance().GetMediaHeader(eMEDIATYPE_SUBTITLE))))
		{
			AAMPLOG_MIL("StreamAbstractionAAMP_MPD: extract and add subtitleMedia header");
			ExtractAndAddSubtitleMediaHeader();
		}

		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: fetch initialization fragments");
		// We have decided on the first period, calculate the PTSoffset to be applied to
		// all segments including the init segments for the GST buffer that goes with the init
		mPTSOffset = 0.0;
		mNextPts = 0.0;
		UpdatePtsOffset(true);
		FetchAndInjectInitFragments();
	}

	return retval;
}




/**
 * @fn Is4Kstream
 * @brief check if current stream have 4K content
 * @retval true on success
 */
bool StreamAbstractionAAMP_MPD::Is4KStream(int &height, BitsPerSecond &bandwidth)
{
	bool Stream4k = false;
	//2. Is this 4K stream? if so get height width information
	for (auto period : mpd->GetPeriods())
	{
		for (auto adaptationSet : period->GetAdaptationSets())
		{
			//Check for video adaptation
			if (!mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_VIDEO))
			{
				continue;
			}

			if (mIsFogTSB)
			{
				vector<Representation *> representations = mMPDParseHelper->GetBitrateInfoFromCustomMpd(adaptationSet);
				for (auto representation : representations)
				{
					height =  representation->GetHeight();
					if ( height > AAMP_FHD_HEIGHT)
					{
						bandwidth = representation->GetBandwidth();
						Stream4k = true;
						break;
					}
				}
			}
			else
			{
				//vector<IRepresentation *> representations = a
				for (auto representation : adaptationSet->GetRepresentation())
				{
					height = representation->GetHeight();
					if (height > AAMP_FHD_HEIGHT)
					{
						bandwidth = representation->GetBandwidth();
						Stream4k = true;
						break;
					}
				}
			}
			/**< If 4K stream found*/
			if (Stream4k)
			{
				AAMPLOG_INFO("4K profile found with resolution : height %d bandwidth %" BITSPERSECOND_FORMAT, height, bandwidth);
				break;
			}
		}
		/**< If 4K stream found*/
		if (Stream4k)
			break;
	}
	return Stream4k;
}

/**
* @brief Function to Parse/Index mpd document after being downloaded.
*/
AAMPStatusType StreamAbstractionAAMP_MPD::IndexNewMPDDocument(bool updateTrackInfo)
{
	AAMPStatusType ret = eAAMPSTATUS_OK;
	if( mpd )
	{
		int deltaInPeriodIndex = mCurrentPeriodIdx;
		mNumberOfPeriods = 	mMPDParseHelper->GetNumberOfPeriods();
		if(mIsLiveStream && updateTrackInfo)
		{
			//Periods could be added or removed, So select period based on periodID
			//If period ID not found in MPD that means it got culled, in that case select
			// first period
			AAMPLOG_INFO("Updating period index after mpd refresh");
			vector<IPeriod *> periods = mpd->GetPeriods();
			int iter = (int)periods.size() - 1;
			mCurrentPeriodIdx = 0;
			while(iter > 0)
			{
				if(aamp->mPipelineIsClear &&
				   mMPDParseHelper->IsPeriodEncrypted(iter))
				{
					AAMPLOG_WARN("Encrypted period found while pipeline is currently configured for clear streams");
					aamp->mEncryptedPeriodFound = true;
				}
				if(mBasePeriodId == periods.at(iter)->GetId())
				{
					mCurrentPeriodIdx = iter;
					break;
				}
				iter--;
			}
		}
		else
		{
			// looping of cdvr video - Issue happens with multiperiod content only
			// When playback is near live position (last period) or after eos in period
			// mCurrentPeriodIdx was reset to 0 . This caused fetch loop to continue from Period 0/fragment 1
			// Reset of mCurrentPeriodIdx to be done to max period if Period count changes after mpd refresh
			if(mCurrentPeriodIdx > (mNumberOfPeriods - 1))
			{
				mCurrentPeriodIdx = mNumberOfPeriods - 1;
			}
		}
		deltaInPeriodIndex -= mCurrentPeriodIdx;
		//Adjusting currently iterating period index based on delta
		mIterPeriodIndex -= deltaInPeriodIndex;
		if(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState)
		{
			mCurrentPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);
		}
		std::vector<IPeriod*> availablePeriods = mpd->GetPeriods();
		mMPDParseHelper->UpdateBoundaryPeriod(rate != AAMP_NORMAL_PLAY_RATE);
		// Update Track Information based on flag
		if (updateTrackInfo)
		{
			AAMPLOG_INFO("MPD has %d periods current period index %u", mNumberOfPeriods, mCurrentPeriodIdx);
			if(mIsLiveStream)
			{
				// IsLive = 1 , resetTimeLineIndex = 1
				// InProgressCdvr (IsLive=1) , resetTimeLineIndex = 1
				// Vod/CDVR for PeriodChange(mUpdateStreamInfo will be true) , resetTimeLineIndex = 1
				if(((AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState) && (AdState::IN_ADBREAK_WAIT2CATCHUP != mCdaiObject->mAdState))
				   || (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState && mUpdateStreamInfo))
				{
					AAMPLOG_TRACE("Indexing new mpd doc");
					ret = UpdateTrackInfo(true, true);
					if(ret != eAAMPSTATUS_OK)
					{
						AAMPLOG_WARN("manifest : %d error", ret);
						return ret;
					}
					for (int i = 0; i < mNumberOfTracks; i++)
					{
						mMediaStreamContext[i]->freshManifest = true;
					}
				}
				// To store period duration in local reference to avoid duplicate mpd parsing to reduce processing delay
				std::vector<PeriodInfo> currMPDPeriodDetails;
				uint64_t durMs = 0;
				UpdateMPDPeriodDetails(currMPDPeriodDetails,durMs);
				mMPDParseHelper->SetMPDPeriodDetails(currMPDPeriodDetails);
				double culled = 0;

				if(mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled)
				{
					culled = GetCulledSeconds(currMPDPeriodDetails);
				}
				if(culled > 0)
				{
					AAMPLOG_INFO("Culled seconds = %f", culled);
					if(!aamp->IsLocalAAMPTsb())
					{
						aamp->UpdateCullingState(culled);
					}
					mCulledSeconds += culled;
				}
				if(!aamp->IsUninterruptedTSB())
				{
					UpdateCulledAndDurationFromPeriodInfo(currMPDPeriodDetails);
				}

				mPeriodEndTime   = mMPDParseHelper->GetPeriodEndTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
				mPeriodStartTime = mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs);
				mPeriodDuration = mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());

				double duration = durMs /(double)1000;
				mLiveEndPosition = duration + mCulledSeconds;
				if(!aamp->IsLocalAAMPTsb())
				{
					aamp->UpdateDuration(duration);
				}
				else
				{
					AampTSBSessionManager *tsbSessionManager = aamp->GetTSBSessionManager();
					if(tsbSessionManager)
					{
						tsbSessionManager->UpdateProgress(duration, mCulledSeconds);
					}
				}
			}
		}
	}
	return ret;
}

/**
 * @brief Fetch MPD manifest
 */
AAMPStatusType StreamAbstractionAAMP_MPD::FetchDashManifest()
{
	AAMPStatusType ret = AAMPStatusType::eAAMPSTATUS_OK;
	std::string manifestUrl = aamp->GetManifestUrl();

	// take the original url before it gets changed in GetFile
	std::string origManifestUrl = manifestUrl;
	bool gotManifest = false;
	double downloadTime;
	bool updateVideoEndMetrics = false;
	int http_error = 0;

	{
		mManifestDnldRespPtr = MakeSharedManifestDownloadResponsePtr();
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_MANIFEST);

		AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
		// Get the Manifest with a wait of Manifest Timeout time
		mManifestDnldRespPtr = dnldInstance->GetManifest(true, aamp->mManifestTimeoutMs);
		gotManifest		=	(mManifestDnldRespPtr->mMPDStatus == AAMPStatusType::eAAMPSTATUS_OK);
		http_error		=	mManifestDnldRespPtr->mMPDDownloadResponse->iHttpRetValue;
		downloadTime	=	mManifestDnldRespPtr->mMPDDownloadResponse->downloadCompleteMetrics.total;
		//update videoend info
		updateVideoEndMetrics = true;
		if (gotManifest)
		{
			manifestUrl = aamp->mManifestUrl = mManifestDnldRespPtr->mMPDDownloadResponse->sEffectiveUrl;
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
			mNetworkDownDetected = false;
		}
		else if (aamp->DownloadsAreEnabled())
		{
			aamp->profiler.ProfileError(PROFILE_BUCKET_MANIFEST, http_error);
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
			if (this->mpd != NULL && (CURLE_OPERATION_TIMEDOUT == http_error || CURLE_COULDNT_CONNECT == http_error))
			{
				//Skip this for first ever update mpd request
				mNetworkDownDetected = true;
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Ignore curl timeout");
				ret = AAMPStatusType::eAAMPSTATUS_OK;
			}
			else if (http_error == 512 )
			{
				// check if any response available to search
				if(mManifestDnldRespPtr->mMPDDownloadResponse->mResponseHeader.size() && aamp->mFogTSBEnabled)
				{
					for ( std::string header : mManifestDnldRespPtr->mMPDDownloadResponse->mResponseHeader )
					{
						if(STARTS_WITH_IGNORE_CASE(header.c_str(),FOG_REASON_STRING))
						{
							aamp->mFogDownloadFailReason.clear();
							aamp->mFogDownloadFailReason  =         header.substr(std::string(FOG_REASON_STRING).length());
							AAMPLOG_WARN("Received FOG-Reason header: %s",aamp->mFogDownloadFailReason.c_str());
							aamp->SendAnomalyEvent(ANOMALY_WARNING, "FOG-Reason:%s", aamp->mFogDownloadFailReason.c_str());
							break;
						}
					}
				}
				if(aamp->mFogDownloadFailReason.find("PROFILE_NONE") != std::string::npos)
				{

					aamp->mFogDownloadFailReason.clear();
					AAMPLOG_ERR("StreamAbstractionAAMP_MPD: No playable profiles found");
					ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
				}
			}
			//When Fog is having tsb write error , then it will respond back with 302 with direct CDN url,In this case alone TSB should be disabled
			else if (aamp->mFogTSBEnabled && http_error == 302)
			{
					aamp->mFogTSBEnabled = false;
			}

			else
			{
				aamp->UpdateDuration(0);
				aamp->SetFlushFdsNeededInCurlStore(true);
				aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
				AAMPLOG_ERR("StreamAbstractionAAMP_MPD: manifest download failed");
				ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
			}
		}
		else // if downloads disabled
		{
			aamp->UpdateDuration(0);
			AAMPLOG_ERR("StreamAbstractionAAMP_MPD: manifest download failed");
			aamp->SetFlushFdsNeededInCurlStore(true);
			ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		}
	}

	long parseTimeMs = 0;
	if (gotManifest)
	{
		vector<std::string> locationUrl;
		long long tStartTime = NOW_STEADY_TS_MS;
		ret = GetMPDFromManifest(mManifestDnldRespPtr , true);
		if (AAMPStatusType::eAAMPSTATUS_OK == ret)
		{
			ProcessMetadataFromManifest(mManifestDnldRespPtr , true);
			if(mIsLiveManifest)
			{
				// Register for manifest update from downloader . No need for any poll
				AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
				dnldInstance->RegisterCallback(MPDUpdateCallback,(void *)this);
				mManifestUpdateHandleFlag       =       true;
			}
		}
		// get parse time for playback stats, if required, parse error can be considered for a later point for statistics
		parseTimeMs = NOW_STEADY_TS_MS - tStartTime;
	}
	else if (AAMPStatusType::eAAMPSTATUS_OK != ret)
	{
		AAMPLOG_ERR("aamp: error on manifest fetch");
	}

	if(updateVideoEndMetrics)
	{
		ManifestData manifestData(downloadTime * 1000, mManifestDnldRespPtr->mMPDDownloadResponse->size(),
				parseTimeMs, mpd ? mpd->GetPeriods().size() : 0);
		aamp->UpdateVideoEndMetrics(eMEDIATYPE_MANIFEST,0,http_error,manifestUrl,downloadTime, &manifestData);
	}
	if( ret == eAAMPSTATUS_MANIFEST_PARSE_ERROR || ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
	{
		std::string downloadData =	std::string( mManifestDnldRespPtr->mMPDDownloadResponse->mDownloadData.begin(),
									 mManifestDnldRespPtr->mMPDDownloadResponse->mDownloadData.end());
		if(!downloadData.empty() && !manifestUrl.empty())
		{
			AAMPLOG_ERR("ERROR: Invalid Playlist URL: %s ret:%d", manifestUrl.c_str(),ret);
			AAMPLOG_ERR("ERROR: Invalid Playlist DATA: %s ", downloadData.c_str());
		}
		if(ret == eAAMPSTATUS_MANIFEST_CONTENT_ERROR && http_error == 512)
		{
			aamp->SendErrorEvent(AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR);
		}
		else
		{
			aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
		}
	}

	return ret;
}


void StreamAbstractionAAMP_MPD::MPDUpdateCallback(void *cbArg)
{
	// handle the response for the manifest download
	if(cbArg != NULL)
	{
		StreamAbstractionAAMP_MPD *myObj = (StreamAbstractionAAMP_MPD *)cbArg;
		if(myObj->mManifestUpdateHandleFlag)
		{
			myObj->MPDUpdateCallbackExec();
		}
	}
}

void StreamAbstractionAAMP_MPD::MPDUpdateCallbackExec()
{
	// Got callback from MPD Downloader for a new MPD File .
	// Inform Fetcher loop to proceed from the wait loop
	// Also parse and send events
	// Get the Manifest with a wait of Manifest Timeout time
	AAMPLOG_INFO("MPDUpdateCallbackExec .............");
	AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
	ManifestDownloadResponsePtr tmpManifestDnldRespPtr;
	// No wait needed , pick the manifest from top of the Q
	tmpManifestDnldRespPtr	= dnldInstance->GetManifest(false, 0);
	// Need to check if same last Manifest is given or new refresh happened
	if(tmpManifestDnldRespPtr->mMPDStatus == AAMPStatusType::eAAMPSTATUS_OK)
	{
		mNetworkDownDetected = false;
		ProcessMetadataFromManifest(tmpManifestDnldRespPtr , false);
		AampMPDParseHelperPtr	mMPDParser = tmpManifestDnldRespPtr->GetMPDParseHelper();

		if(mMPDParser != NULL)
		{
			uint64_t manifestDuration = 0;
			if(mIsLiveManifest)
			{
				manifestDuration = mMPDParser->GetTSBDepth();
			}
			else
			{
				manifestDuration = mMPDParser->GetMediaPresentationDuration();
			}

			std::string manifestType = mMPDParser->IsLiveManifest() ? "dynamic" : "static";
			AAMPLOG_INFO( "Send RefreshNotify Dur[%" PRIu64 "] NoOfPeriods[%d] PubTime[%u] ManifestType[%s]", manifestDuration, mMPDParser->GetNumberOfPeriods(), tmpManifestDnldRespPtr->mMPDInstance->GetFetchTime(), manifestType.c_str());
			aamp->SendEvent(std::make_shared<ManifestRefreshEvent>(manifestDuration, mMPDParser->GetNumberOfPeriods(), tmpManifestDnldRespPtr->mMPDInstance->GetFetchTime(), manifestType, aamp->GetSessionId()), AAMP_EVENT_ASYNC_MODE);
		}
	}
	else
	{
		// Failure from the manifest download during refresh --- fire , what to do ??
		// Check if the App only insisted to stop the download resulting in partial failure ?
		int http_error	=	tmpManifestDnldRespPtr->mMPDDownloadResponse->iHttpRetValue;

		if (aamp->DownloadsAreEnabled())
		{
			// if already mpd is available
			if (this->mpd != NULL
				&& (CURLE_OPERATION_TIMEDOUT == http_error || CURLE_COULDNT_CONNECT == http_error))
			{
				//Skip this for first ever update mpd request
				mNetworkDownDetected = true;
				AAMPLOG_WARN("Ignore curl timeout");
			}
			else
			{
				if (http_error == 512 )
				{
					if(tmpManifestDnldRespPtr->mMPDDownloadResponse->mResponseHeader.size() && aamp->mFogTSBEnabled)
					{
						for ( std::string header : tmpManifestDnldRespPtr->mMPDDownloadResponse->mResponseHeader )
						{
							if(STARTS_WITH_IGNORE_CASE(header.c_str(),FOG_REASON_STRING))
							{
								aamp->mFogDownloadFailReason.clear();
								aamp->mFogDownloadFailReason  =         header.substr(std::string(FOG_REASON_STRING).length());
								AAMPLOG_WARN("Received FOG-Reason header: %s",aamp->mFogDownloadFailReason.c_str());
								aamp->SendAnomalyEvent(ANOMALY_WARNING, "FOG-Reason:%s", aamp->mFogDownloadFailReason.c_str());
								break;
							}
						}
					}
				}
				else if(tmpManifestDnldRespPtr->mMPDStatus == eAAMPSTATUS_MANIFEST_PARSE_ERROR)
				{
					aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE); //corrupt or invalid manifest
					AAMPLOG_ERR("Invalid manifest, parse failed");
				}
				else if(tmpManifestDnldRespPtr->mMPDStatus == eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
				{
					//Unknown Manifest content
					aamp->SendErrorEvent(AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR);
					AAMPLOG_ERR("Unknown manifest content");
				}
				else
				{
					aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
					AAMPLOG_ERR("manifest download failed");
				}
			}
		}
		else // if downloads disabled
		{
			AAMPLOG_ERR("manifest download failed");
		}
	}
}

/**
 * @brief Check if Period is empty or not
 * @retval Return true on empty Period
 */
void StreamAbstractionAAMP_MPD::FindPeriodGapsAndReport()
{
	double prevPeriodEndMs = aamp->culledSeconds * 1000;
	double curPeriodStartMs = 0;
	int numPeriods =	mMPDParseHelper->GetNumberOfPeriods();
	for(int i = 0; i< numPeriods; i++)
	{
		auto tempPeriod = mpd->GetPeriods().at(i);
		std::string curPeriodStartStr = tempPeriod->GetStart();
		if(!curPeriodStartStr.empty())
		{
			curPeriodStartMs = ParseISO8601Duration(curPeriodStartStr.c_str());
		}
		if (STARTS_WITH_IGNORE_CASE(tempPeriod->GetId().c_str(), FOG_INSERTED_PERIOD_ID_PREFIX))
		{
			if(mMPDParseHelper->IsEmptyPeriod(i, (rate != AAMP_NORMAL_PLAY_RATE)))
			{
				// Empty period indicates that the gap is growing, report event without duration
				aamp->ReportContentGap((long long)(prevPeriodEndMs - (aamp->mProgressReportOffset*1000)), tempPeriod->GetId());
			}
			else
			{
				// Non empty  event indicates that the gap is complete.
				if(curPeriodStartMs > 0 && prevPeriodEndMs > 0)
				{
					double periodGapMS = (curPeriodStartMs - prevPeriodEndMs);
					aamp->ReportContentGap((long long)(prevPeriodEndMs - (aamp->mProgressReportOffset*1000)), tempPeriod->GetId(), periodGapMS);
				}
			}
		}
		else if (prevPeriodEndMs > 0 && (std::round((curPeriodStartMs - prevPeriodEndMs) / 1000) > 0))
		{
			// Gap in between periods, but period ID changed after interrupt
			// Fog skips duplicate period and inserts fragments in new period.
			double periodGapMS = (curPeriodStartMs - prevPeriodEndMs);
			aamp->ReportContentGap((long long)(prevPeriodEndMs - (aamp->mProgressReportOffset*1000)), tempPeriod->GetId(), periodGapMS);
		}
		if(mMPDParseHelper->IsEmptyPeriod(i, (rate != AAMP_NORMAL_PLAY_RATE))) continue;
		double periodDuration = mMPDParseHelper->aamp_GetPeriodDuration(i, mLastPlaylistDownloadTimeMs);
		prevPeriodEndMs = curPeriodStartMs + periodDuration;
	}
}

/**
 * @brief Read UTCTiming _element_
 * @retval Return true if UTCTiming _element_ is available in the manifest
 */
bool  StreamAbstractionAAMP_MPD::FindServerUTCTime(Node* root)
{
	bool hasServerUtcTime = false;
	if( root )
	{
		mLocalUtcTime = 0;
		for ( auto node :  root->GetSubNodes() )
		{
			if(node)
			{
				if( "UTCTiming" == node->GetName() && node->HasAttribute("schemeIdUri"))
				{
					std::string schemeIdUri = node->GetAttributeValue("schemeIdUri");
					if ( SERVER_UTCTIME_DIRECT == schemeIdUri && node->HasAttribute("value"))
					{
						const std::string &value = node->GetAttributeValue("value");
						mLocalUtcTime = ISO8601DateTimeToUTCSeconds(value.c_str() );
						double currentTime = (double)aamp_GetCurrentTimeMS() / 1000;
						mDeltaTime =  mLocalUtcTime - currentTime;
						hasServerUtcTime = true;
						break;
					}
					else if((SERVER_UTCTIME_HTTP == schemeIdUri || (URN_UTC_HTTP_ISO == schemeIdUri) || (URN_UTC_HTTP_HEAD == schemeIdUri)) && node->HasAttribute("value"))
					{
						int http_error = -1;
						std::string ServerUrl = node->GetAttributeValue("value");
						if(!(ServerUrl.find("http") == 0))
						{
							std::string valueCopy = ServerUrl;
							// Do not add parameters to the Time Server URL, even if eAAMPConfig_PropagateURIParam is set
							aamp_ResolveURL(ServerUrl, aamp->GetManifestUrl(), valueCopy.c_str(), false);
						}

						mLocalUtcTime = GetNetworkTime(ServerUrl, &http_error, aamp->GetNetworkProxy());
						if(mLocalUtcTime > 0 )
						{
							double currentTime = (double)aamp_GetCurrentTimeMS() / 1000;
							mDeltaTime =  mLocalUtcTime - currentTime;
							hasServerUtcTime = true;
						}
						else
						{
							AAMPLOG_ERR("Failed to read timeServer [%s] RetCode[%d]",ServerUrl.c_str(),http_error);
						}
						break;
					}
				}
			}
		}
	}
	mMPDParseHelper->SetLocalTimeDelta(mDeltaTime);
	return hasServerUtcTime;
}

/**
 * @brief Find timed metadata from mainifest
 */
void StreamAbstractionAAMP_MPD::FindTimedMetadata(MPD* mpd, Node* root, bool init, bool reportBulkMeta)
{
	std::vector<Node*> subNodes = root->GetSubNodes();


	if(!subNodes.empty())
	{
		uint64_t periodStartMS = 0;
		uint64_t periodDurationMS = 0;
		std::vector<std::string> newPeriods;
		int64_t firstSegmentStartTime = -1;

		// If we intend to use the PTS presentation time from the event to determine the event start time then
		// before parsing the events we will get the first segment time. If we do not have a valid first
		// segment time then we will get the event start time from the period start/duration
		if (ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime))
		{
			if (mpd != NULL)
			{
				int iPeriod = 0;
				int numPeriods	=	(int)mpd->GetPeriods().size();
				while (iPeriod < numPeriods)
				{
					IPeriod *period = mpd->GetPeriods().at(iPeriod);
					if (period == NULL)
					{
						break;
					}

					uint64_t segmentStartPTS = mMPDParseHelper->GetFirstSegmentStartTime(period);
					if (segmentStartPTS)
					{
						// Got a segment start time so convert it to ms and quit
						uint64_t timescale = mMPDParseHelper->GetPeriodSegmentTimeScale(period);
						if (timescale > 1)
						{
							// We have a first segment start time so we will use that
							firstSegmentStartTime = segmentStartPTS;

							firstSegmentStartTime *= 1000;
							firstSegmentStartTime /= timescale;
						}
						break;
					}
					iPeriod++;
				}
			}
			if (firstSegmentStartTime == -1)
			{
				AAMPLOG_ERR("SCTEDBG - failed to get firstSegmentStartTime");
			}
			else
			{
				AAMPLOG_INFO("SCTEDBG - firstSegmentStartTime %" PRId64 , firstSegmentStartTime);
			}
		}

		// Iterate through each of the MPD's Period nodes, and ProgrameInformation.
		int periodCnt = 0;
		for (size_t i=0; i < subNodes.size(); i++) {
			Node* node = subNodes.at(i);
			if(node == NULL)  //CID:163928 - forward null
			{
				AAMPLOG_WARN("node is null");  //CID:80723 - Null Returns
				return;
			}
			const std::string& name = node->GetName();
			if (name == "Period") {
				std::string AdID;
				std::string AssetID;
				std::string ProviderID;
				periodCnt++;

				// Calculate period start time and duration
				periodStartMS += periodDurationMS;
				if (node->HasAttribute("start")) {
					const std::string& value = node->GetAttributeValue("start");
					uint64_t valueMS = 0;
					if (!value.empty())
						valueMS = ParseISO8601Duration(value.c_str() );
					if (periodStartMS < valueMS)
						periodStartMS = valueMS;
				}
				periodDurationMS = 0;
				if (node->HasAttribute("duration")) {
					const std::string& value = node->GetAttributeValue("duration");
					uint64_t valueMS = 0;
					if (!value.empty())
						valueMS = ParseISO8601Duration(value.c_str() );
					periodDurationMS = valueMS;
				}
				IPeriod * period = NULL;
				if (mpd != NULL)
				{
						period = mpd->GetPeriods().at(periodCnt-1);
				}
				else
				{
					AAMPLOG_WARN("mpd is null");  //CID:80941 - Null Returns
				}
				std::string adBreakId("");
				if(period != NULL)
				{
					const std::string &prdId = period->GetId();
					// Iterate through children looking for SupplementProperty nodes
					std::vector<Node*> children = node->GetSubNodes();
					for (size_t j=0; j < children.size(); j++) {
						Node* child = children.at(j);
						const std::string& name = child->GetName();
						if(!name.empty())
						{
							if (name == "SupplementalProperty") {
								ProcessPeriodSupplementalProperty(child, AdID, periodStartMS, periodDurationMS, init, reportBulkMeta);
								continue;
							}
							if (name == "AssetIdentifier") {
								ProcessPeriodAssetIdentifier(child, periodStartMS, periodDurationMS, AssetID, ProviderID, init, reportBulkMeta);
								continue;
							}
							if((name == "EventStream") && ("" != prdId) && !mCdaiObject->isPeriodExist(prdId))
							{
								bool processEventsInPeriod = ((!init || (1 < periodCnt && 0 == period->GetAdaptationSets().size())) //Take last & empty period at the MPD init AND all new periods in the MPD refresh. (No empty periods will come the middle)
												  || (!mIsLiveManifest && init) || (mIsLiveManifest && ISCONFIGSET(eAAMPConfig_BulkTimedMetaReportLive) ));

								bool modifySCTEProcessing = ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime);
								if (modifySCTEProcessing)
								{
									// cdvr events that are currently recording are tagged as live - we still need to process
									// all the SCTE events for these manifests so we'll just rely on isPeriodExist() to prevent
									// repeated notifications and process all events in the manifest
									processEventsInPeriod = true;
								}

								if (processEventsInPeriod)
								{
									mCdaiObject->InsertToPeriodMap(period);	//Need to do it. Because the FulFill may finish quickly
									ProcessEventStream(periodStartMS, firstSegmentStartTime, period, reportBulkMeta);
									continue;
								}
							}
						}
						else
						{
							AAMPLOG_WARN("name is empty");  //CID:80526 - Null Returns
						}
					}
					if("" != prdId)
					{
						mCdaiObject->InsertToPeriodMap(period);
						newPeriods.emplace_back(prdId);
					}
					continue;
				}
			}
			if (name == "ProgramInformation") {
				std::vector<Node*> infoNodes = node->GetSubNodes();
				for (size_t i=0; i < infoNodes.size(); i++) {
					Node* infoNode = infoNodes.at(i);
					std::string name;
					std::string ns;
					ParseXmlNS(infoNode->GetName(), ns, name);
					const std::string& infoNodeType = infoNode->GetAttributeValue("type");
					if (name == "ContentIdentifier" && (infoNodeType == "URI" || infoNodeType == "URN")) {
						if (infoNode->HasAttribute("value")) {
							const std::string& content = infoNode->GetAttributeValue("value");

							AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-CONTENT-IDENTIFIER:%s", 0.0f, content.c_str());

							for (int i = 0; i < aamp->subscribedTags.size(); i++)
							{
								const std::string& tag = aamp->subscribedTags.at(i);
								if (tag == "#EXT-X-CONTENT-IDENTIFIER") {

									if(reportBulkMeta && init)
									{
										aamp->SaveTimedMetadata(0, tag.c_str(), content.c_str(), (int)content.size());
									}
									else
									{
										aamp->SaveNewTimedMetadata(0, tag.c_str(), content.c_str(), (int)content.size());
									}
									break;
								}
							}
						}
						continue;
					}
				}
				continue;
			}
			if (name == "SupplementalProperty" && node->HasAttribute("schemeIdUri")) {
				const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");
				if (schemeIdUri == aamp->mSchemeIdUriDai && node->HasAttribute("id")) {
					const std::string& ID = node->GetAttributeValue("id");
					if (ID == "identityADS" && node->HasAttribute("value")) {
						const std::string& content = node->GetAttributeValue("value");

						AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-IDENTITY-ADS:%s", 0.0f, content.c_str());

						for (int i = 0; i < aamp->subscribedTags.size(); i++)
						{
							const std::string& tag = aamp->subscribedTags.at(i);
							if (tag == "#EXT-X-IDENTITY-ADS") {
								if(reportBulkMeta && init)
								{
									aamp->SaveTimedMetadata(0, tag.c_str(), content.c_str(), (int)content.size());
								}
								else
								{
									aamp->SaveNewTimedMetadata(0, tag.c_str(), content.c_str(), (int)content.size());
								}

								break;
							}
						}
						continue;
					}
					if (ID == "messageRef" && node->HasAttribute("value")) {
						const std::string& content = node->GetAttributeValue("value");

						AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-MESSAGE-REF:%s", 0.0f, content.c_str());

						for (int i = 0; i < aamp->subscribedTags.size(); i++)
						{
							const std::string& tag = aamp->subscribedTags.at(i);
							if (tag == "#EXT-X-MESSAGE-REF") {
								if(reportBulkMeta && init)
								{
									aamp->SaveTimedMetadata(0, tag.c_str(), content.c_str(), (int)content.size());
								}
								else
								{
									aamp->SaveNewTimedMetadata(0, tag.c_str(), content.c_str(), (int)content.size());
								}
								break;
							}
						}
						continue;
					}
				}
				continue;
			}
		}
		mCdaiObject->PrunePeriodMaps(newPeriods);
	}
	else
	{
		AAMPLOG_WARN("SubNodes  is empty");  //CID:85449 - Null Returns
	}
}


/**
 * @brief Process supplemental property of a period
 */
void StreamAbstractionAAMP_MPD::ProcessPeriodSupplementalProperty(Node* node, std::string& AdID, uint64_t startMS, uint64_t durationMS, bool isInit, bool reportBulkMeta)
{
	if (node->HasAttribute("schemeIdUri")) {
		const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");

		if(!schemeIdUri.empty())
		{
			if ((schemeIdUri == aamp->mSchemeIdUriDai) && node->HasAttribute("id")) {
				const std::string& ID = node->GetAttributeValue("id");
				if ((ID == "Tracking") && node->HasAttribute("value")) {
					const std::string& value = node->GetAttributeValue("value");
					if (!value.empty()) {
						AdID = value;

						// Check if we need to make AdID a quoted-string
						if (AdID.find(',') != std::string::npos) {
							AdID="\"" + AdID + "\"";
						}

						double duration = durationMS / 1000.0f;
						double start = startMS / 1000.0f;

						std::ostringstream s;
						s << "ID=" << AdID;
						s << ",DURATION=" << std::fixed << std::setprecision(3) << duration;
						s << ",PSN=true";

						std::string content = s.str();
						AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-CUE:%s", start, content.c_str());

						for (int i = 0; i < aamp->subscribedTags.size(); i++)
						{
							const std::string& tag = aamp->subscribedTags.at(i);
							if (tag == "#EXT-X-CUE") {
								if(reportBulkMeta && isInit)
								{
									aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
								}
								else
								{
									aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
								}
								break;
							}
						}
					}
				}
			}
			if (!AdID.empty() && (schemeIdUri == "urn:scte:scte130-10:2014")) {
				std::vector<Node*> children = node->GetSubNodes();
				for (size_t i=0; i < children.size(); i++) {
					Node* child = children.at(i);
					std::string name;
					std::string ns;
					ParseXmlNS(child->GetName(), ns, name);
					if (name == "StreamRestrictionListType") {
						ProcessStreamRestrictionList(child, AdID, startMS, isInit, reportBulkMeta);
						continue;
					}
					if (name == "StreamRestrictionList") {
						ProcessStreamRestrictionList(child, AdID, startMS, isInit, reportBulkMeta);
						continue;
					}
					if (name == "StreamRestriction") {
						ProcessStreamRestriction(child, AdID, startMS, isInit, reportBulkMeta);
						continue;
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("schemeIdUri  is empty");  //CID:83796 - Null Returns
		}
	}
}


/**
 * @brief Process Period AssetIdentifier
 */
void StreamAbstractionAAMP_MPD::ProcessPeriodAssetIdentifier(Node* node, uint64_t startMS, uint64_t durationMS, std::string& AssetID, std::string& ProviderID, bool isInit, bool reportBulkMeta)
{
	if (node->HasAttribute("schemeIdUri")) {
		const std::string& schemeIdUri = node->GetAttributeValue("schemeIdUri");
		if ((schemeIdUri == "urn:cablelabs:md:xsd:content:3.0") && node->HasAttribute("value")) {
			const std::string& value = node->GetAttributeValue("value");
			if (!value.empty()) {
				size_t pos = value.find("/Asset/");
				if (pos != std::string::npos) {
					std::string assetID = value.substr(pos+7);
					std::string providerID = value.substr(0, pos);
					double duration = durationMS / 1000.0f;
					double start = startMS / 1000.0f;

					AssetID = assetID;
					ProviderID = providerID;

					// Check if we need to make assetID a quoted-string
					if (assetID.find(',') != std::string::npos) {
						assetID="\"" + assetID + "\"";
					}

					// Check if we need to make providerID a quoted-string
					if (providerID.find(',') != std::string::npos) {
						providerID="\"" + providerID + "\"";
					}

					std::ostringstream s;
					s << "ID=" << assetID;
					s << ",PROVIDER=" << providerID;
					s << ",DURATION=" << std::fixed << std::setprecision(3) << duration;

					std::string content = s.str();
					AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-ASSET-ID:%s", start, content.c_str());

					for (int i = 0; i < aamp->subscribedTags.size(); i++)
					{
						const std::string& tag = aamp->subscribedTags.at(i);
						if (tag == "#EXT-X-ASSET-ID") {
							if(reportBulkMeta && isInit)
							{
								aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
							}
							else
							{
								aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
							}
							break;
						}
					}
				}
			}
		}
		else if ((schemeIdUri == "urn:scte:dash:asset-id:upid:2015"))
		{
			double start = startMS / 1000.0f;
			std::ostringstream s;
			for(auto childNode : node->GetSubNodes())
			{
				if(childNode->GetAttributeValue("type") == "URI")
				{
					s << "ID=" << "\"" << childNode->GetAttributeValue("value") << "\"";
				}
				else if(childNode->GetAttributeValue("type") == "ADI")
				{
					s << ",SIGNAL=" << "\"" << childNode->GetAttributeValue("value") << "\"";
				}
			}
			std::string content = s.str();
			AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-SOURCE-STREAM:%s", start, content.c_str());

			for (int i = 0; i < aamp->subscribedTags.size(); i++)
			{
				const std::string& tag = aamp->subscribedTags.at(i);
				if (tag == "#EXT-X-SOURCE-STREAM") {
					if(reportBulkMeta && isInit)
					{
						aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
					}
					else
					{
						aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
					}
					break;
				}
			}
		}

	}
}

/**
 *   @brief Process event stream.
 */
bool StreamAbstractionAAMP_MPD::ProcessEventStream(uint64_t startMS, int64_t startOffsetMS, IPeriod * period, bool reportBulkMeta)
{
	bool ret = false;

	const std::string &prdId = period->GetId();
	if(!prdId.empty())
	{
		uint64_t startMS1 = 0;
		//Vector of pair of scte35 binary data and corresponding duration
		std::vector<EventBreakInfo> eventBreakVec;
		if(isAdbreakStart(period, startMS1, eventBreakVec))
		{
			#define MAX_EVENT_STARTIME (24*60*60*1000)
			uint64_t maxPTSTime = ((uint64_t)0x1FFFFFFFF / (uint64_t)90); // 33 bit pts max converted to ms

			AAMPLOG_WARN("Found CDAI events for period %s ", prdId.c_str());
			for(EventBreakInfo &eventInfo : eventBreakVec)
			{
				uint64_t eventStartTime = startMS; // by default, use the time derived from the period start/duration

				// If we have a presentation time and a valid start time for the stream, then we will use the presentation
				// time to set / adjust the event start and duration relative to the start time of the stream
				if (eventInfo.presentationTime && (startOffsetMS > -1))
				{
					// Adjust for stream start offset and check for pts wrap
					eventStartTime = eventInfo.presentationTime;
					if (eventStartTime < startOffsetMS)
					{
						eventStartTime += (maxPTSTime - startOffsetMS);

						// If the difference is too large (>24hrs), assume this is not a pts wrap and the event is timed
						// to occur before the start - set it to start immediately and adjust the duration accordingly
						if (eventStartTime > MAX_EVENT_STARTIME)
						{
							uint64_t diff = startOffsetMS - eventInfo.presentationTime;
							if (eventInfo.duration > diff)
							{
								eventInfo.duration -= diff;
							}
							else
							{
								eventInfo.duration = 0;
							}
							eventStartTime = 0;

						}
					}
					else
					{
						eventStartTime -= startOffsetMS;
					}
					AAMPLOG_INFO("SCTEDBG adjust start time %" PRIu64 " -> %" PRIu64 " (duration %d)", eventInfo.presentationTime, eventStartTime, eventInfo.duration);
				}

				//for livestream send the timedMetadata only., because at init, control does not come here
				if(mIsLiveManifest && ! ISCONFIGSET(eAAMPConfig_BulkTimedMetaReportLive))
				{
					// The current process relies on enabling eAAMPConfig_EnableClientDai and that may not be desirable
					// for our requirements. We'll just skip this and use the VOD process to send events
					bool modifySCTEProcessing = ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime);
					if (modifySCTEProcessing)
					{
						aamp->SaveNewTimedMetadata(eventStartTime, eventInfo.name.c_str(), eventInfo.payload.c_str(), (int)eventInfo.payload.size(), prdId.c_str(), eventInfo.duration);
					}
					else
					{
						aamp->FoundEventBreak(prdId, eventStartTime, eventInfo);
					}
				}
				else
				{
					//for vod, send TimedMetadata only when bulkmetadata is not enabled
					if(reportBulkMeta)
					{
						AAMPLOG_INFO("Saving timedMetadata for VOD %s event for the period, %s", eventInfo.name.c_str(), prdId.c_str());
						aamp->SaveTimedMetadata(eventStartTime, eventInfo.name.c_str() , eventInfo.payload.c_str(), (int)eventInfo.payload.size(), prdId.c_str(), eventInfo.duration);
					}
					else
					{
						aamp->SaveNewTimedMetadata(eventStartTime, eventInfo.name.c_str(), eventInfo.payload.c_str(), (int)eventInfo.payload.size(), prdId.c_str(), eventInfo.duration);
					}
				}
			}
			ret = true;
		}
	}
	return ret;
}

/**
 * @brief Process Stream restriction list
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestrictionList(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	std::vector<Node*> children = node->GetSubNodes();
	if(!children.empty())
		{
		for (size_t i=0; i < children.size(); i++) {
			Node* child = children.at(i);
			std::string name;
			std::string ns;
			ParseXmlNS(child->GetName(), ns, name);
			if (name == "StreamRestriction") {
				ProcessStreamRestriction(child, AdID, startMS, isInit, reportBulkMeta);
				continue;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("node is null");  //CID:84081 - Null Returns
	}
}


/**
 * @brief Process stream restriction
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	std::vector<Node*> children = node->GetSubNodes();
	for (size_t i=0; i < children.size(); i++) {
		Node* child = children.at(i);
		if(child != NULL)
			{
			std::string name;
			std::string ns;
			ParseXmlNS(child->GetName(), ns, name);
			if (name == "Ext") {
				ProcessStreamRestrictionExt(child, AdID, startMS, isInit, reportBulkMeta);
				continue;
			}
		}
		else
		{
			AAMPLOG_WARN("child is null");  //CID:84810 - Null Returns
		}
	}
}


/**
 * @brief Process stream restriction extension
 */
void StreamAbstractionAAMP_MPD::ProcessStreamRestrictionExt(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	std::vector<Node*> children = node->GetSubNodes();
	for (size_t i=0; i < children.size(); i++) {
		Node* child = children.at(i);
		std::string name;
		std::string ns;
		ParseXmlNS(child->GetName(), ns, name);
		if (name == "TrickModeRestriction") {
			ProcessTrickModeRestriction(child, AdID, startMS, isInit, reportBulkMeta);
			continue;
		}
	}
}


/**
 * @brief Process trick mode restriction
 */
void StreamAbstractionAAMP_MPD::ProcessTrickModeRestriction(Node* node, const std::string& AdID, uint64_t startMS, bool isInit, bool reportBulkMeta)
{
	double start = startMS / 1000.0f;

	std::string trickMode;
	if (node->HasAttribute("trickMode")) {
		trickMode = node->GetAttributeValue("trickMode");
		if(!trickMode.length())
		{
			AAMPLOG_WARN("trickMode is null");  //CID:86122 - Null Returns
		}
	}

	std::string scale;
	if (node->HasAttribute("scale")) {
		scale = node->GetAttributeValue("scale");
	}

	std::string text = node->GetText();
	if (!trickMode.empty() && !text.empty()) {
		std::ostringstream s;
		s << "ADID=" << AdID
		  << ",MODE=" << trickMode
		  << ",LIMIT=" << text;

		if (!scale.empty()) {
			s << ",SCALE=" << scale;
		}

		std::string content = s.str();
		AAMPLOG_TRACE("TimedMetadata: @%1.3f #EXT-X-TRICKMODE-RESTRICTION:%s", start, content.c_str());

		for (int i = 0; i < aamp->subscribedTags.size(); i++)
		{
			const std::string& tag = aamp->subscribedTags.at(i);
			if (tag == "#EXT-X-TRICKMODE-RESTRICTION") {
				if(reportBulkMeta && isInit)
				{
					aamp->SaveTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
				}
				else
				{
					aamp->SaveNewTimedMetadata((long long)startMS, tag.c_str(), content.c_str(), (int)content.size());
				}
				break;
			}
		}
	}
}


/**
 * @brief Fragment downloader thread
 * @param arg HeaderFetchParams pointer
 */
void StreamAbstractionAAMP_MPD::TrackDownloader(int trackIdx, std::string initialization)
{
	UsingPlayerId playerId(aamp->mPlayerId);
	double fragmentDuration = 0.0;
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[trackIdx];

	//Calling WaitForFreeFragmentAvailable timeout as 0 since waiting for one tracks
	//init header fetch can slow down fragment downloads for other track
	if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
	{
		pMediaStreamContext->profileChanged = false;
		FetchFragment(pMediaStreamContext, initialization, fragmentDuration, true, getCurlInstanceByMediaType(pMediaStreamContext->mediaType), //CurlContext 0=Video, 1=Audio)
			pMediaStreamContext->discontinuity);
		pMediaStreamContext->discontinuity = false;
	}
}

/**
 * @brief Check if adaptation set is iframe track
 * @param adaptationSet Pointer to adaptationSet
 * @retval true if iframe track
 */
static bool IsIframeTrack(IAdaptationSet *adaptationSet)
{
	const std::vector<INode *> subnodes = adaptationSet->GetAdditionalSubNodes();
	for (unsigned i = 0; i < subnodes.size(); i++)
	{
		INode *xml = subnodes[i];
		if(xml != NULL)
		{
			if (xml->GetName() == "EssentialProperty")
			{
				if (xml->HasAttribute("schemeIdUri"))
				{
					const std::string& schemeUri = xml->GetAttributeValue("schemeIdUri");
					if (schemeUri == "http://dashif.org/guidelines/trickmode")
					{
						AAMPLOG_TRACE("AdaptationSet:%d is IframeTrack",adaptationSet->GetId());
						return true;
					}
				}
			}
		}
		else
		{
			AAMPLOG_WARN("xml is null");  //CID:81118 - Null Returns
		}
	}
	return false;
}


/**
 * @brief Get the language for an adaptation set
 */
std::string StreamAbstractionAAMP_MPD::GetLanguageForAdaptationSet(IAdaptationSet *adaptationSet)
{
	std::string lang = adaptationSet->GetLang();
	// If language from adaptation is undefined , retain the current player language
	// Not to change the language .

//	if(lang == "und")
//	{
//		lang = aamp->language;
//	}
	if(!lang.empty())
	{
		lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
	}
	else
	{
		// set und+id as lang, this is required because sometimes lang is not present and stream has multiple audio track.
		// this unique lang string will help app to SetAudioTrack by index.
		// Attempt is made to make lang unique by picking ID of first representation under current adaptation
		IRepresentation* representation = adaptationSet->GetRepresentation().at(0);
		if(NULL != representation)
		{
			lang = "und_" + representation->GetId();
			if( lang.size() > (MAX_LANGUAGE_TAG_LENGTH-1))
			{
				// Lang string len  should not be more than "MAX_LANGUAGE_TAG_LENGTH" so trim from end
				// lang is sent to metadata event where len of char array  is limited to MAX_LANGUAGE_TAG_LENGTH
				lang = lang.substr(0,(MAX_LANGUAGE_TAG_LENGTH-1));
			}
		}
	}
	return lang;
}

/**
 * @brief Get Adaptation Set at given index for the current period
 *
 * @retval Adaptation Set at given Index
 */
const IAdaptationSet* StreamAbstractionAAMP_MPD::GetAdaptationSetAtIndex(int idx)
{
	assert(idx < mCurrentPeriod->GetAdaptationSets().size());
	return mCurrentPeriod->GetAdaptationSets().at(idx);
}

/**
 * @brief Get Adaptation Set and Representation Index for given profile
 *
 * @retval Adaptation Set and Representation Index pair for given profile
 */
struct ProfileInfo StreamAbstractionAAMP_MPD::GetAdaptationSetAndRepresentationIndicesForProfile(int profileIndex)
{
	assert(profileIndex < GetProfileCount());
	return mProfileMaps.at(profileIndex);
}

/**
 * @brief Update language list state variables
 */
void StreamAbstractionAAMP_MPD::UpdateLanguageList()
{
	size_t numPeriods = mMPDParseHelper->GetNumberOfPeriods();
	for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
	{
		IPeriod *period = mpd->GetPeriods().at(iPeriod);
		if(period != NULL)
		{
			size_t numAdaptationSets = period->GetAdaptationSets().size();
			for (int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
			{
				IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
				if(adaptationSet != NULL)
				{
					if (mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_AUDIO ))
					{
						mLangList.insert( GetLanguageForAdaptationSet(adaptationSet) );
					}
				}
				else
				{
					AAMPLOG_WARN("adaptationSet is null");  //CID:86317 - Null Returns
				}
			}
		}
		else
		{
			AAMPLOG_WARN("period is null");  //CID:83754 - Null Returns
		}
	}
	aamp->StoreLanguageList(mLangList);
}

/**
 * @brief Get the best Audio track by Language, role, and/or codec type
 * @return int selected representation index
 */
int StreamAbstractionAAMP_MPD::GetBestAudioTrackByLanguage( int &desiredRepIdx, AudioType &CodecType,
std::vector<AudioTrackInfo> &ac4Tracks, std::string &audioTrackIndex)
{
	int bestTrack = -1;
	unsigned long long bestScore = 0;
	AudioTrackInfo selectedAudioTrack; /**< Selected Audio track information */
	IPeriod *period = mCurrentPeriod;
	bool isMuxedAudio = false; /** Flag to indicate muxed audio track , used by AC4 */
	if(!period)
	{
		AAMPLOG_WARN("period is null");
		return bestTrack;
	}

	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for( int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		if( mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_AUDIO) )
		{
			unsigned long long score = 0;
			std::string trackLabel = adaptationSet->GetLabel();
			std::string trackLanguage = GetLanguageForAdaptationSet(adaptationSet);
			if(trackLanguage.empty())
			{
				isMuxedAudio = true;
				AAMPLOG_INFO("Found Audio Track with no language, look like muxed stream");
			}
			else if( aamp->preferredLanguagesList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), trackLanguage);
				if(iter != aamp->preferredLanguagesList.end())
				{ // track is in preferred language list
					int distance = (int)std::distance(aamp->preferredLanguagesList.begin(),iter);
					score += ((aamp->preferredLanguagesList.size()-distance)) * AAMP_LANGUAGE_SCORE; // big bonus for language match
				}
			}

			if( aamp->preferredLabelList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredLabelList.begin(), aamp->preferredLabelList.end(), trackLabel);
				if(iter != aamp->preferredLabelList.end())
				{ // track is in preferred language list
					int distance = (int)std::distance(aamp->preferredLabelList.begin(),iter);
					score += ((aamp->preferredLabelList.size()-distance)) * AAMP_LABEL_SCORE; // big bonus for language match
				}
			}

			auto role = adaptationSet->GetRole();
			for (unsigned iRole = 0; iRole < role.size(); iRole++)
			{
				auto rendition = role.at(iRole);
				if (rendition->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != std::string::npos)
				{
					const auto& trackRendition = rendition->GetValue();
					if (!aamp->preferredRenditionString.empty())
					{
						if (aamp->preferredRenditionString.compare(trackRendition) == 0)
						{
							score += AAMP_ROLE_SCORE;
						}
					}
					else if (trackRendition == "main")
					{
						AAMPLOG_INFO("prioritizing main track");
						score++; // tiebreaker - favor "main"
					}
				}
			}
			if( !aamp->preferredTypeString.empty() )
			{
				for( auto iter : adaptationSet->GetAccessibility() )
				{
					if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						std::string trackType = iter->GetValue();
						if (aamp->preferredTypeString.compare(trackType)==0 )
						{
							score += AAMP_TYPE_SCORE;
						}
					}
				}
			}

			Accessibility accessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode((void* )adaptationSet);
			if (accessibilityNode == aamp->preferredAudioAccessibilityNode)
			{
				if (!accessibilityNode.getSchemeId().empty())
				{
					score += AAMP_SCHEME_ID_SCORE;
				}
			}

			AudioType selectedCodecType = eAUDIO_UNKNOWN;
			unsigned int selRepBandwidth = 0;
			bool disableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
			// if EC3 disabled, implicitly disable ATMOS
			bool disableATMOS = (disableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);
			bool disableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3);
			bool disableAC4 = ISCONFIGSET(eAAMPConfig_DisableAC4);

			int audioRepresentationIndex = -1;
			long codecScore = 0;
			bool disabled = false;
			if(!GetPreferredCodecIndex(adaptationSet, audioRepresentationIndex, selectedCodecType, selRepBandwidth, codecScore, disableEC3 , disableATMOS, disableAC4, disableAC3, disabled))
			{
				audioRepresentationIndex = GetDesiredCodecIndex(adaptationSet, selectedCodecType, selRepBandwidth, disableEC3 , disableATMOS, disableAC4, disableAC3, disabled);
				switch( selectedCodecType )
				{
					case eAUDIO_DOLBYAC4:
						if( !disableAC4 )
						{
							score += 10;
						}
						break;

					case eAUDIO_ATMOS:
						if( !disableATMOS )
						{
							score += 8;
						}
						break;

					case eAUDIO_DDPLUS:
						if( !disableEC3 )
						{
							score += 6;
						}
						break;

					case eAUDIO_DOLBYAC3:
						if( !disableAC3 )
						{
							score += 4;
						}
						break;

					case eAUDIO_AAC:
						score += 2;
						break;

					case eAUDIO_VORBIS:
					case eAUDIO_OPUS:
					case eAUDIO_UNKNOWN:
						score += 1;
						break;

					default:
						break;
				}
			}
			else
			{
				score += codecScore;
			}

			/**Add score for language , role and type matching from preselection node for AC4 tracks */
			if ((selectedCodecType == eAUDIO_DOLBYAC4) && isMuxedAudio)
			{
				AAMPLOG_INFO("AC4 Track Selected, get the priority based on language and role" );
				unsigned long long ac4CurrentScore = 0;
				unsigned long long ac4SelectedScore = 0;


				for(const auto& ac4Track:ac4Tracks)
				{
					if (ac4Track.codec.find("ac-4") != std::string::npos)
					{
						//TODO: Non AC4 codec in preselection node as per Dash spec
						//https://dashif.org/docs/DASH-IF-IOP-for-ATSC3-0-v1.0.pdf#page=63&zoom=100,92,113
						continue;
					}
					if( aamp->preferredLanguagesList.size() > 0 )
					{
						auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), ac4Track.language);
						if(iter != aamp->preferredLanguagesList.end())
						{ // track is in preferred language list
							int distance = (int)std::distance(aamp->preferredLanguagesList.begin(),iter);
							ac4CurrentScore += ((aamp->preferredLanguagesList.size()-distance)) * AAMP_LANGUAGE_SCORE; // big bonus for language match
						}
					}
					if( !aamp->preferredRenditionString.empty() )
					{
						if( aamp->preferredRenditionString.compare(ac4Track.rendition)==0 )
						{
							ac4CurrentScore += AAMP_ROLE_SCORE;
						}
					}
					if( !aamp->preferredTypeString.empty() )
					{
						if (aamp->preferredTypeString.compare(ac4Track.contentType)==0 )
						{
							ac4CurrentScore += AAMP_TYPE_SCORE;
						}
					}
					if ((ac4CurrentScore > ac4SelectedScore) ||
					((ac4SelectedScore == ac4CurrentScore ) && (ac4Track.bandwidth > selectedAudioTrack.bandwidth)) /** Same track with best quality */
					)
					{
						selectedAudioTrack = ac4Track;
						audioTrackIndex = ac4Track.index; /**< for future reference **/
						ac4SelectedScore = ac4CurrentScore;

					}
				}
				score += ac4SelectedScore;
				//KC
				//PrasePreselection and found language match and add points for language role match type match
			}
			if (disabled || (audioRepresentationIndex < 0))
			{
				/**< No valid representation found in this Adaptation, discard this adaptation */
				score = 0;
			}
			AAMPLOG_INFO( "track#%d::%d -  score = %llu", iAdaptationSet, audioRepresentationIndex,  score );
			if( score > bestScore )
			{
				bestScore = score;
				bestTrack = iAdaptationSet;
				desiredRepIdx = audioRepresentationIndex;
				CodecType = selectedCodecType;
			}
		} // IsContentType(adaptationSet, eMEDIATYPE_AUDIO)
	} // next iAdaptationSet
	if (CodecType == eAUDIO_DOLBYAC4)
	{
		/* TODO: Currently current Audio track is updating only for AC4 need mechanism to update for other tracks also */
		AAMPLOG_INFO( "Audio Track selected index - %s - language : %s rendition : %s - codec - %s  score = %llu", selectedAudioTrack.index.c_str(),
		selectedAudioTrack.language.c_str(), selectedAudioTrack.rendition.c_str(), selectedAudioTrack.codec.c_str(),  bestScore );
	}
	return bestTrack;
}


/**
 * @fn GetBestTextTrackByLanguage
 *
 * @brief Get the best Text track by Language, role, and schemeId
 * @return int selected representation index
 */
bool StreamAbstractionAAMP_MPD::GetBestTextTrackByLanguage( TextTrackInfo &selectedTextTrack)
{
	bool bestTrack = false;
	unsigned long long bestScore = 0;
	IPeriod *period = mCurrentPeriod;
	if(!period)
	{
		AAMPLOG_WARN("period is null");
		return bestTrack;
	}
	std::string trackLanguage = "";
	std::string trackRendition = "";
	std::string trackLabel = "";
	std::string trackType = "";
	Accessibility accessibilityNode;

	size_t numAdaptationSets = period->GetAdaptationSets().size();
	for( int iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		bool labelFound = false;
		bool accessibilityFound = false;
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		if(mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_SUBTITLE) )
		{
			unsigned long long score = 1; /** Select first track by default **/
			trackLabel = adaptationSet->GetLabel();
			if (!trackLabel.empty())
			{
				if (!aamp->preferredTextLabelString.empty() && aamp->preferredTextLabelString.compare(trackLabel) == 0)
				{
					/**< Label matched **/
					score += AAMP_LABEL_SCORE;
				}
				labelFound = true;
			}
			trackLanguage = GetLanguageForAdaptationSet(adaptationSet);
			if( aamp->preferredTextLanguagesList.size() > 0 )
			{
				auto iter = std::find(aamp->preferredTextLanguagesList.begin(), aamp->preferredTextLanguagesList.end(), trackLanguage);
				if(iter != aamp->preferredTextLanguagesList.end())
				{ // track is in preferred language list
					int dist = (int)std::distance(aamp->preferredTextLanguagesList.begin(),iter);
					score += (aamp->preferredTextLanguagesList.size()-dist) * AAMP_LANGUAGE_SCORE; // big bonus for language match
				}
			}

			if( !aamp->preferredTextRenditionString.empty() )
			{
				std::vector<IDescriptor *> rendition = adaptationSet->GetRole();
				for (unsigned iRendition = 0; iRendition < rendition.size(); iRendition++)
				{
					if (rendition.at(iRendition)->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						if (!trackRendition.empty())
						{
							trackRendition =+ ",";
						}
						/**< Save for future use **/
						trackRendition += rendition.at(iRendition)->GetValue();
						/**< If any rendition matched add the score **/
						std::string rend = rendition.at(iRendition)->GetValue();
						if( aamp->preferredTextRenditionString.compare(rend)==0 )
						{
							score += AAMP_ROLE_SCORE;
						}
					}
				}
			}

			if( !aamp->preferredTextTypeString.empty() )
			{
				for( auto iter : adaptationSet->GetAccessibility() )
				{
					if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
					{
						if (!trackType.empty())
						{
							trackType += ",";
						}
						trackType += iter->GetValue();
						/**< If any type matched add the score **/
						std::string type = iter->GetValue();
						if (aamp->preferredTextTypeString.compare(type)==0 )
						{
							score += AAMP_TYPE_SCORE;
						}
					}
				}
			}

			accessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode((void *)adaptationSet);
			if (accessibilityNode == aamp->preferredTextAccessibilityNode)
			{
				if (!accessibilityNode.getSchemeId().empty())
				{
					accessibilityFound = true;
				}
				score += AAMP_SCHEME_ID_SCORE;
			}

			uint32_t selRepBandwidth = 0;
			int textRepresentationIndex = -1;
			std::string name;
			std::string codec;
			std::string empty;
			std::string type;
			if (accessibilityFound)
			{
				type = "captions";
			}
			else if (labelFound)
			{
				type = "subtitle_" + trackLabel;
			}
			else
			{
				type = "subtitle";
			}

			GetPreferredTextRepresentation(adaptationSet, textRepresentationIndex, selRepBandwidth, score, name, codec);
			AAMPLOG_INFO( "track#%d::%d - trackLanguage = %s bandwidth = %d score = %llu currentBestScore = %llu ", iAdaptationSet, textRepresentationIndex, trackLanguage.c_str(),
			selRepBandwidth, score, bestScore);
			if( score > bestScore )
			{
				bestTrack = true;
				bestScore = score;
				std::string index =  std::to_string(iAdaptationSet) + "-" + std::to_string(textRepresentationIndex); //KC
				selectedTextTrack.set(index, trackLanguage, false, trackRendition, name, codec, empty, trackType, trackLabel, type, accessibilityNode);
			}
		} // IsContentType(adaptationSet, eMEDIATYPE_SUBTITLE)
	} // next iAdaptationSet
	return bestTrack;
}

void StreamAbstractionAAMP_MPD::StartSubtitleParser()
{
	class MediaStreamContext *subtitle = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		auto seekPoint = aamp->seek_pos_seconds;
		if(aamp->IsLive())
		{ // adjust subtitle presentation start to align with AV live offset
			seekPoint -= aamp->mLiveOffset;
		}
		else
		{ // absolute to relative correction for subtec synchronization within period
			seekPoint -= aamp->mNextPeriodStartTime;
		}

		SegmentTemplates segmentTemplates(subtitle->representation->GetSegmentTemplate(),
										  subtitle->adaptationSet->GetSegmentTemplate() );
		uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
		uint32_t tScale = segmentTemplates.GetTimescale();
		seekPoint += presentationTimeOffset/(double)tScale;

		// seekPoint can end up -0.088400000000000006 here
		// 0.1 seems too big an epsilon
		// need to check the math leading up to above value to see if precision can be improved

		if (seekPoint < -FLOATING_POINT_EPSILON )
		{
			AAMPLOG_INFO("Not reached start point yet - returning");
			return;
		}

		AAMPLOG_INFO("sending init seekPoint=%.3f", seekPoint);
		subtitle->mSubtitleParser->init(seekPoint, 0);
		subtitle->mSubtitleParser->mute(aamp->subtitles_muted);
		subtitle->mSubtitleParser->isLinear(mIsLiveStream);
	}
}

void StreamAbstractionAAMP_MPD::PauseSubtitleParser(bool pause)
{
	class MediaStreamContext *subtitle = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("setting subtitles pause state = %d", pause);
		subtitle->mSubtitleParser->pause(pause);
	}
}

/** Static function Do not use aamp log function **/
Accessibility StreamAbstractionAAMP_MPD::getAccessibilityNode(AampJsonObject &accessNode)
{
	std::string strSchemeId;
	std::string strValue;
	int intValue = -1;
	Accessibility accessibilityNode;
	if (accessNode.get(std::string("scheme"), strSchemeId))
	{
		if (accessNode.get(std::string("string_value"), strValue))
		{
			accessibilityNode.setAccessibilityData(strSchemeId, strValue);
		}
		else if(accessNode.get(std::string("int_value"), intValue))
		{
			accessibilityNode.setAccessibilityData(strSchemeId, intValue);
		}
	}
	return accessibilityNode;
}

/** Static function Do not use aamp log function **/
Accessibility StreamAbstractionAAMP_MPD::getAccessibilityNode(void* adaptationSetShadow)
{
	IAdaptationSet *adaptationSet = (IAdaptationSet*) adaptationSetShadow;
	Accessibility accessibilityNode;
	if (adaptationSet)
	{
		for( auto iter : adaptationSet->GetAccessibility() )
		{
			std::string schemeId  = iter->GetSchemeIdUri();
			std::string strValue = iter->GetValue();
			accessibilityNode.setAccessibilityData(schemeId, strValue);
			if (!accessibilityNode.getSchemeId().empty())
			{
				break; /**< Only one valid accessibility node is processing, so break here */
			}
		}
	}
	return accessibilityNode;
}

/**
 * @fn ParseTrackInformation
 *
 * @brief get the Label value from adaptation in Dash
 * @return void
 */
void StreamAbstractionAAMP_MPD::ParseTrackInformation(IAdaptationSet *adaptationSet, uint32_t iAdaptationIndex, AampMediaType media, std::vector<AudioTrackInfo> &aTracks, std::vector<TextTrackInfo> &tTracks)
{
	if(adaptationSet)
	{
		bool labelFound = false;
		bool schemeIdFound = false;
		std::string type = "";
		/** Populate Common for all tracks */

		Accessibility accessibilityNode = StreamAbstractionAAMP_MPD::getAccessibilityNode((void*)adaptationSet);
		if (!accessibilityNode.getSchemeId().empty())
		{
			schemeIdFound = true;
		}

		/**< Audio or Subtitle representations **/
		if (eMEDIATYPE_AUDIO == media || eMEDIATYPE_SUBTITLE == media)
		{
			std::string lang = GetLanguageForAdaptationSet(adaptationSet);
			const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
			std::string codec;
			std::string group; // value of <Role>, empty if <Role> not present
			std::string accessibilityType; // value of <Accessibility> //KC
			std::string empty;
			std::string label = adaptationSet->GetLabel();
			if(!label.empty())
			{
				labelFound = true;
			}
			if (eMEDIATYPE_AUDIO == media)
			{
				if (schemeIdFound)
				{
					type = "audio_" + accessibilityNode.getStrValue();
				}
				else if (labelFound)
				{
					type = "audio_" + label;
				}
				else
				{
					type = "audio";
				}
			}
			else if (eMEDIATYPE_SUBTITLE == media)
			{
				if (schemeIdFound)
				{
					type = "captions";
				}
				else if (labelFound)
				{
					type = "subtitle_" + label;
				}
				else
				{
					type = "subtitle";
				}
			}

			std::vector<IDescriptor *> role = adaptationSet->GetRole();
			for (unsigned iRole = 0; iRole < role.size(); iRole++)
			{
				if (role.at(iRole)->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
				{
					if (!group.empty())
					{
						group += ",";
					}
					group += role.at(iRole)->GetValue();
				}
			}
			for( auto iter : adaptationSet->GetAccessibility() )
			{
				if (iter->GetSchemeIdUri().find("urn:mpeg:dash:role:2011") != string::npos)
				{
					if (!accessibilityType.empty())
					{
						accessibilityType += ",";
					}
					accessibilityType += iter->GetValue();
				}
			}
			// check for codec defined in Adaptation Set
			const std::vector<string> adapCodecs = adaptationSet->GetCodecs();
			for (int representationIndex = 0; representationIndex < representation.size(); representationIndex++)
			{
				std::string index = std::to_string(iAdaptationIndex) + "-" + std::to_string(representationIndex);
				const dash::mpd::IRepresentation *rep = representation.at(representationIndex);
				std::string name = rep->GetId();
				long bandwidth = rep->GetBandwidth();
				const std::vector<std::string> repCodecs = rep->GetCodecs();
				bool isAvailable = !mMPDParseHelper->IsEmptyAdaptation(adaptationSet);
				// check if Representation includes codec
				if (repCodecs.size())
				{
					codec = repCodecs.at(0);
				}
				else if (adapCodecs.size()) // else check if Adaptation has codec
				{
					codec = adapCodecs.at(0);
				}
				else
				{
					// For subtitle, it might be vtt/ttml format
					PeriodElement periodElement(adaptationSet, rep);
					codec = periodElement.GetMimeType();
				}

				if (eMEDIATYPE_AUDIO == media)
				{
					if ((codec.find("ac-4") != std::string::npos) && lang.empty())
					{
						/* Incase of AC4 muxed audio track, track information is already provided by preselection node*/
						AAMPLOG_WARN("AC4 muxed audio track detected.. Skipping..");
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Audio Track - lang:%s, group:%s, name:%s, codec:%s, bandwidth:%ld, AccessibilityType:%s label:%s type:%s availability:%d",
						lang.c_str(), group.c_str(), name.c_str(), codec.c_str(), bandwidth, accessibilityType.c_str(), label.c_str(), type.c_str(), isAvailable);
						aTracks.push_back(AudioTrackInfo(index, lang, group, name, codec, bandwidth, accessibilityType, false, label, type, accessibilityNode, isAvailable));
					}
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Text Track - lang:%s, isCC:0, group:%s, name:%s, codec:%s, accessibilityType:%s label:%s type:%s availability:%d",
						lang.c_str(), group.c_str(), name.c_str(), codec.c_str(), accessibilityType.c_str(), label.c_str(), type.c_str(), isAvailable);
					tTracks.push_back(TextTrackInfo(index, lang, false, group, name, codec, empty, accessibilityType, label, type, accessibilityNode, isAvailable));
				}
			}
		}
		// Look in VIDEO adaptation for inband CC track related info
		else if ((eMEDIATYPE_VIDEO == media) && (!IsIframeTrack(adaptationSet)))
		{
			std::vector<IDescriptor *> adapAccessibility = adaptationSet->GetAccessibility();
			for (int index = 0 ; index < adapAccessibility.size(); index++)
			{
				std::string schemeId = adapAccessibility.at(index)->GetSchemeIdUri();
				if (schemeId.find("urn:scte:dash:cc:cea") != string::npos)
				{
					std::string lang, id;
					std::string value = adapAccessibility.at(index)->GetValue();
					if (!value.empty())
					{
						// Expected formats : 	eng;deu
						// 			CC1=eng;CC2=deu
						//			1=lang:eng;2=lang:deu
						//			1=lang:eng;2=lang:eng,war:1,er:1
						size_t delim = value.find(';');
						while (delim != std::string::npos)
						{
							ParseCCStreamIDAndLang(value.substr(0, delim), id, lang);
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: CC Track - lang:%s, isCC:1, group:%s, id:%s",
								lang.c_str(), schemeId.c_str(), id.c_str());
							TextTrackInfo textTrack = TextTrackInfo(true, schemeId);
							textTrack.setInstreamId(id);
							textTrack.setLanguage(lang);
							textTrack.setType("captions");
							tTracks.push_back(textTrack);
							value = value.substr(delim + 1);
							delim = value.find(';');
						}
						ParseCCStreamIDAndLang(value, id, lang);
						lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: CC Track - lang:%s, isCC:1, group:%s, id:%s",
							lang.c_str(), schemeId.c_str(), id.c_str());
						TextTrackInfo textTrack = TextTrackInfo(true, schemeId);
						textTrack.setInstreamId(id);
						textTrack.setLanguage(lang);
						textTrack.setType("captions");
						tTracks.push_back(textTrack);
					}
					else
					{
						// value = empty is highly discouraged as per spec, just added as fallback
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: CC Track - group:%s, isCC:1", schemeId.c_str());
						TextTrackInfo textTrack = TextTrackInfo(true, schemeId);
						textTrack.setAccessibilityItem(accessibilityNode);
						textTrack.setAvailable(true);
						tTracks.push_back(textTrack);
					}
				}
			}
		}
	}//NULL Check
}
/**
 * @brief Switch to the corresponding subtitle track, Load new subtitle, flush the old subtitle fragments,
 * select the new subtitle track, set the new subtitle info, update the new subtitle track, then skip to the appropriate
 * path to download.
 */
void StreamAbstractionAAMP_MPD::SwitchSubtitleTrack(bool newTune)
{
	AAMPStatusType ret = eAAMPSTATUS_OK;
	uint32_t fragmentDuration = 0;
	uint64_t oldMediaSequenceNumber = 0;
	int diffFragmentsDownloaded = 0;
	double offsetFromStart;
	std::string tTrackIdx;
	double   oldPlaylistPosition,diffInFetchedDuration,diffInInjectedDuration,newInjectedPosition;

	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	if ((!pMediaStreamContext) || (!pMediaStreamContext->enabled))
	{
		AAMPLOG_WARN("pMediaStreamContext  is null");
		pMediaStreamContext->NotifyCachedSubtitleFragmentAvailable();
		return;
	}
	AbortWaitForAudioTrackCatchup(true);

	pMediaStreamContext->LoadNewSubtitle(true);
	/* Flush Subtitle Fragments */
	pMediaStreamContext->FlushFragments();
	if( pMediaStreamContext->freshManifest )
	{
		/*In Live scenario, the manifest refresh got happened frequently,so in the UpdateTrackinfo(), all the params
		* get reset lets say.., pMediaStreamContext->fragmentDescriptor.Number, fragmentTime.., so during that case, we are getting
		* totalfetchDuration, totalInjectedDuration as negative values once we subtract the OldPlaylistpositions with the Newplaylistpositions, for avoiding
		* this in the case if the manifest got updated, we have called the PushNextFragment(), for the proper update of the params like
		* pMediaStreamContext->fragmentTime, pMediaStreamContext->fragmentDescriptor.Number and pMediaStreamContext->fragmentDescriptor.Time
		*/
		AAMPLOG_INFO("Manifest got updated[%u]",pMediaStreamContext->freshManifest);
		PushNextFragment(pMediaStreamContext, getCurlInstanceByMediaType(static_cast<AampMediaType>(eMEDIATYPE_SUBTITLE)),true);
	}
	/* Switching to selected Subtitle Track */
	SelectSubtitleTrack(newTune,mTextTracks,tTrackIdx);
	SetTextTrackInfo(mTextTracks, tTrackIdx);
	printSelectedTrack(tTrackIdx, eMEDIATYPE_SUBTITLE);
	/* Caching the oldPlaylistPosition and oldMediaSequenceNumber */
	oldPlaylistPosition = pMediaStreamContext->fragmentTime;
	oldMediaSequenceNumber = pMediaStreamContext->fragmentDescriptor.Number;

	/* Getting Gstreamer Play position */
	offsetFromStart = aamp->GetPositionSeconds() - aamp->culledSeconds;
	AAMPLOG_INFO( "Playlist pos offsetFromStart[%lf] culledSeconds[%lf]",offsetFromStart,aamp->culledSeconds );

	UpdateSeekPeriodOffset(offsetFromStart);
	AAMPLOG_INFO( "Updated pos offsetFromStart[%lf] culledSeconds[%lf]",offsetFromStart,aamp->culledSeconds );

	/*Updating subtitle Tracks info*/
	ret = UpdateMediaTrackInfo(eMEDIATYPE_SUBTITLE);
	if (ret != eAAMPSTATUS_OK)
	{
		pMediaStreamContext->NotifyCachedSubtitleFragmentAvailable();
		return;
	}
	/* Skipping the subtitle fragments to reach the current position for fetching */
	SkipFragments(pMediaStreamContext, offsetFromStart, true);

	/* Getting current fragment duration, for calculating the injected duration timestamp, because the current injected time
	 * calculation requires the start time of the fragment which is downloaded, so subtracting the fragment duration for identification
	 * the starttime of the downloaded fragment.
	 */
	fragmentDuration = GetCurrentFragmentDuration( pMediaStreamContext );

	/* In the case, If the Skiptime is 0, In the Skipfragment the lastsegment related params were not updated.,
	* due to the cond (pMediaStreamContext->fragmentDescriptor.Time<pMediaStreamContext->lastSegmentTime),in the PushNextFragment()
	* loops untill the Fdt time reaches the last segment time and the wrong Audio fragment gets Pushed, so reseting all the
	* lastsegment related params here...
	*/
	pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time - fragmentDuration;
	pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time;
	pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number - 1;

	/* Calculating the start time of the downloaded fragment */
	newInjectedPosition = static_cast<double>(pMediaStreamContext->fragmentDescriptor.Time - fragmentDuration) / pMediaStreamContext->fragmentDescriptor.TimeScale;

	/*Calculating the difference in Fetched duration, injected duration and diff in Media Sequence number */
	diffInFetchedDuration = oldPlaylistPosition - pMediaStreamContext->fragmentTime;
	diffInInjectedDuration = ( pMediaStreamContext->GetLastInjectedPosition() - newInjectedPosition );
	diffFragmentsDownloaded = static_cast<int>(oldMediaSequenceNumber) - static_cast<int>(pMediaStreamContext->fragmentDescriptor.Number);

	AAMPLOG_INFO("Calculated diffInFetchedDuration[%lf] diffFragmentsDownloaded[%u] diffInInjectedDuration[%lf] LastInjectedDuration[%lf] Duration[%u], newInjectedPosition[%lf]",
		diffInFetchedDuration,diffFragmentsDownloaded, diffInInjectedDuration, pMediaStreamContext->GetLastInjectedPosition(), fragmentDuration, newInjectedPosition );

	pMediaStreamContext->resetAbort(false);
	pMediaStreamContext->OffsetTrackParams(diffInFetchedDuration, diffInInjectedDuration, diffFragmentsDownloaded);

	/*Fetch and injecting initialization params*/
	FetchAndInjectInitialization(eMEDIATYPE_SUBTITLE, false);
	if(!aamp->IsGstreamerSubsEnabled())
	{
		AAMPLOG_INFO("gstreamer not enabled");
		pMediaStreamContext->mSubtitleParser->init(aamp->GetPositionSeconds(), aamp->GetBasePTS());
	}
}
/**
 * @brief Selects the subtitle track based on the available audio tracks and updates the desired representation index.
 *
 */
void StreamAbstractionAAMP_MPD::SelectSubtitleTrack(bool newTune, std::vector<TextTrackInfo> &tTracks ,std::string &tTrackIdx)
{
	IPeriod *period = mCurrentPeriod;
	int  selAdaptationSetIndex = -1;
	int selRepresentationIndex = -1;
	TextTrackInfo selectedTextTrack;
	TextTrackInfo preferredTextTrack;
	std::vector<AudioTrackInfo> aTracks;//aTracks is only used for calling ParseTrackInformation
	bool isFrstAvailableTxtTrackSelected = false;

	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_SUBTITLE];
	mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled = false;

	if(!period)
	{
		AAMPLOG_WARN("period is null");  //CID:84742 - Null Returns
		return;
	}

	size_t numAdaptationSets = period->GetAdaptationSets().size();
	if (AAMP_NORMAL_PLAY_RATE == rate)
	{
		GetBestTextTrackByLanguage(preferredTextTrack);
	}
	for (unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
	{
		IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
		AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD: Content type [%s] AdapSet [%d] ",
					  adaptationSet->GetContentType().c_str(), iAdaptationSet);
		if (mMPDParseHelper->IsContentType(adaptationSet,eMEDIATYPE_SUBTITLE))
		{
			ParseTrackInformation(adaptationSet, iAdaptationSet,eMEDIATYPE_SUBTITLE , aTracks, tTracks);
			if (AAMP_NORMAL_PLAY_RATE == rate)
			{
				if (selAdaptationSetIndex == -1 || isFrstAvailableTxtTrackSelected)
				{
					AAMPLOG_INFO("Checking subs - mime %s lang %s selAdaptationSetIndex %d",
								 adaptationSet->GetMimeType().c_str(), GetLanguageForAdaptationSet(adaptationSet).c_str(), selAdaptationSetIndex);

					TextTrackInfo *firstAvailTextTrack = nullptr;
					if (aamp->GetPreferredTextTrack().index.empty() && !isFrstAvailableTxtTrackSelected)
					{
						// If no subtitles are selected, opt for the first subtitle as default, and
						for (int j = 0; j < tTracks.size(); j++)
						{
							if (!tTracks[j].isCC)
							{
								if (nullptr == firstAvailTextTrack)
								{
									firstAvailTextTrack = &tTracks[j];
								}
							}
						}
					}
					if (nullptr != firstAvailTextTrack)
					{
						isFrstAvailableTxtTrackSelected = true;
						AAMPLOG_INFO("Selected first subtitle track, lang:%s, index:%s",
									 firstAvailTextTrack->language.c_str(), firstAvailTextTrack->index.c_str());
					}
					if (!preferredTextTrack.index.empty())
					{
						/**< Available preferred**/
						selectedTextTrack = preferredTextTrack;
					}
					else
					{
						// TTML selection as follows:
						// 1. Text track as set from SetTextTrack API (this is confusingly named preferredTextTrack, even though it's explicitly set)
						// 2. The *actual* preferred text track, as set through the SetPreferredSubtitleLanguage API
						// 3. First text track and keep it
						// 3. Not set
						selectedTextTrack = (nullptr != firstAvailTextTrack) ? *firstAvailTextTrack : aamp->GetPreferredTextTrack();
					}

					if (!selectedTextTrack.index.empty())
					{
						if (IsMatchingLanguageAndMimeType(eMEDIATYPE_SUBTITLE, selectedTextTrack.language, adaptationSet, selRepresentationIndex))
						{
							const auto& adapSetName = (adaptationSet->GetRepresentation().at(selRepresentationIndex))->GetId();
							AAMPLOG_INFO("adapSet Id %s selName %s", adapSetName.c_str(), selectedTextTrack.name.c_str());
							if (adapSetName.empty() || adapSetName == selectedTextTrack.name)
							{
								selAdaptationSetIndex = iAdaptationSet;
							}
						}
					}
					else
					{
						for (const auto& LangStr : aamp->preferredSubtitleLanguageVctr)
						{
							if(IsMatchingLanguageAndMimeType(eMEDIATYPE_SUBTITLE, LangStr, adaptationSet, selRepresentationIndex))
							{
								AAMPLOG_INFO("matched default sub language %s [%d]", LangStr.c_str(), iAdaptationSet);
								selAdaptationSetIndex = iAdaptationSet;
								break;
							}
						}
					}
				}
			}
		}
	}

	if (selAdaptationSetIndex != -1)
	{
		aamp->mIsInbandCC = false;
		AAMPLOG_WARN("SDW config set %d", ISCONFIGSET(eAAMPConfig_GstSubtecEnabled));
		if (!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
		{
			const IAdaptationSet *pAdaptationSet = period->GetAdaptationSets().at(selAdaptationSetIndex);
			PeriodElement periodElement(pAdaptationSet, pAdaptationSet->GetRepresentation().at(selRepresentationIndex));
			std::string mimeType = periodElement.GetMimeType();
			if (mimeType.empty())
			{
				if (!pMediaStreamContext->mSubtitleParser)
				{
					AAMPLOG_WARN("mSubtitleParser is NULL");
				}
				else if (pMediaStreamContext->mSubtitleParser->init(0.0, 0))
				{
					pMediaStreamContext->mSubtitleParser->mute(aamp->subtitles_muted);
				}
				else
				{
					pMediaStreamContext->mSubtitleParser.reset(nullptr);
					pMediaStreamContext->mSubtitleParser = NULL;
				}
			}
			bool isExpectedMimeType = !mimeType.compare("text/vtt");
			pMediaStreamContext->mSubtitleParser = this->RegisterSubtitleParser_CB( mimeType, isExpectedMimeType);
			if (!pMediaStreamContext->mSubtitleParser)
			{
				pMediaStreamContext->enabled = false;
				selAdaptationSetIndex = -1;
			}
		}
		if (-1 != selAdaptationSetIndex)
			tTrackIdx = std::to_string(selAdaptationSetIndex) + "-" + std::to_string(selRepresentationIndex);

		aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
	}
	if ((AAMP_NORMAL_PLAY_RATE == rate) && (pMediaStreamContext->enabled == false) && selAdaptationSetIndex >= 0)
	{
		pMediaStreamContext->enabled = true;
		pMediaStreamContext->adaptationSetIdx = selAdaptationSetIndex;
		pMediaStreamContext->representationIndex = selRepresentationIndex;
		pMediaStreamContext->profileChanged = true;

		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] Adaptation set[%d] RepIdx[%d] TrackCnt[%d]",
		GetMediaTypeName(eMEDIATYPE_SUBTITLE),selAdaptationSetIndex,selRepresentationIndex,(mNumberOfTracks+1) );

		AAMPLOG_WARN("Queueing content protection from StreamSelection for type:%d", eMEDIATYPE_SUBTITLE);
		QueueContentProtection(period, selAdaptationSetIndex,eMEDIATYPE_SUBTITLE );
		// Check if the track was enabled mid-playback, eg - subtitles. Then we might need to start injection loop
		// For now, do this only for subtitles
		if (!newTune &&
			!pMediaStreamContext->isFragmentInjectorThreadStarted())
		{
			AAMPLOG_WARN("Subtitle track enabled, but fragmentInjection loop not yet started! Starting now..");
			aamp->ResumeTrackInjection(eMEDIATYPE_SUBTITLE);
			// TODO: This could be moved to StartInjectLoop, but due to lack of testing will keep it here for now
			if(pMediaStreamContext->playContext)
			{
				pMediaStreamContext->playContext->reset();
			}
			pMediaStreamContext->StartInjectLoop();
		}
	}
	if(selAdaptationSetIndex < 0 && rate == 1)
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No valid adaptation set found for Media[%s]",GetMediaTypeName(eMEDIATYPE_SUBTITLE));
	}

	AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] %s",
		GetMediaTypeName(eMEDIATYPE_SUBTITLE), pMediaStreamContext->enabled?"enabled":"disabled");

	// Enable/ disable subtitle mediastream context based on stream selection status
	if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && pMediaStreamContext->playContext)
	{
		pMediaStreamContext->playContext->enable(pMediaStreamContext->enabled);
	}
}
/**
 * @brief Selects the audio track based on the available audio tracks and updates the desired representation index.
 *
 * This function selects the audio track from the given vector of AC4 audio tracks based on audio track selection logic
 * It also updates the audioAdaptationSetIndex and audioRepresentationIndex variables accordingly.
 *
 * @param[in] aTracks The vector of available audio tracks. These are the parsed ac4 tracks
 * @param[out] aTrackIdx The selected audio track index.
 * @param[out] audioAdaptationSetIndex The index of the selected audio adaptation set.
 * @param[out] audioRepresentationIndex The index of the selected audio representation.
 */
void StreamAbstractionAAMP_MPD::SelectAudioTrack(std::vector<AudioTrackInfo> &aTracks, std::string &aTrackIdx, int &audioAdaptationSetIndex, int &audioRepresentationIndex)
{
	IPeriod *period = mCurrentPeriod;
	int desiredRepIdx = -1;
	AudioType selectedCodecType = eAUDIO_UNKNOWN;
	bool disableAC4 = ISCONFIGSET(eAAMPConfig_DisableAC4);

	if (!period)
	{
		AAMPLOG_WARN("period is null");
		return;
	}
	if (!disableAC4)
	{
		std::vector<AudioTrackInfo> ac4Tracks;
		ParseAvailablePreselections(period, ac4Tracks);
		aTracks.insert(aTracks.end(), ac4Tracks.begin(), ac4Tracks.end());
	}
	audioAdaptationSetIndex = GetBestAudioTrackByLanguage(desiredRepIdx, selectedCodecType, aTracks, aTrackIdx);
	IAdaptationSet *audioAdaptationSet = NULL;

	if (audioAdaptationSetIndex >= 0)
	{
		audioAdaptationSet = period->GetAdaptationSets().at(audioAdaptationSetIndex);
	}
	if (audioAdaptationSet)
	{
		std::string lang = GetLanguageForAdaptationSet(audioAdaptationSet);
		if (desiredRepIdx != -1)
		{
			audioRepresentationIndex = desiredRepIdx;
			mAudioType = selectedCodecType;
		}
		AAMPLOG_MIL("StreamAbstractionAAMP_MPD: lang[%s] AudioType[%d]", lang.c_str(), selectedCodecType);
	}
	else
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Unable to get audioAdaptationSet.");
	}
	/* To solve a no audio issue - Force configure gst audio pipeline/playbin in the case of multi period
	* multi audio codec available for current decoding language on stream. For example, first period has AAC: no EC3,
	* so the player will choose AAC then start decoding, but in the forthcoming periods,
	* if the stream has AAC and EC3 for the current decoding language then as per the EC3(default priority)
	* the player will choose EC3 but the audio pipeline actually not configured in this case to affect this change.
	*/
	if (aamp->previousAudioType != selectedCodecType)
	{
		AAMPLOG_MIL("StreamAbstractionAAMP_MPD: AudioType Changed %d -> %d", aamp->previousAudioType, selectedCodecType);
		aamp->previousAudioType = selectedCodecType;
		SetESChangeStatus();
	}
}

/**
 * @brief Stops the Track Injection,Restarts once the track has been changed
 */
void StreamAbstractionAAMP_MPD::RefreshTrack(AampMediaType type)
{
	MediaStreamContext *track = mMediaStreamContext[type];

	if (track && track->Enabled())
	{
		if(type == eMEDIATYPE_AUDIO)
		{
			track->refreshAudio = true;
		}
		else
		{
			track->refreshSubtitles = true;
		}
		track->AbortWaitForCachedAndFreeFragment(true);
		aamp->StopTrackInjection(type);
		aamp->mDisableRateCorrection = true;
	}
}

/**
 * @brief Updates the corresponding media track params like fragment time, fragment duration, fragment.descriptor number
 * fragment.descriptor time..etc, and also updates the corresponding urls.
 */
AAMPStatusType StreamAbstractionAAMP_MPD::UpdateMediaTrackInfo(AampMediaType type)
{
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[type];

	IPeriod *period = mCurrentPeriod;

	if ((!pMediaStreamContext) || (!pMediaStreamContext->enabled))
	{
		AAMPLOG_WARN("pMediaStreamContext  is null");  //CID:82464,84186 - Null Returns
		return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
	}
	AAMPLOG_INFO("Enter : Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentTime %f segNumber %" PRIu64 " Ftime:%f" ,pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentTime, pMediaStreamContext->fragmentDescriptor.Number, pMediaStreamContext->fragmentDescriptor.Time);

	pMediaStreamContext->adaptationSet = period->GetAdaptationSets().at(pMediaStreamContext->adaptationSetIdx);
	pMediaStreamContext->adaptationSetId = pMediaStreamContext->adaptationSet->GetId();

	if (pMediaStreamContext->representationIndex < pMediaStreamContext->adaptationSet->GetRepresentation().size())
	{
		pMediaStreamContext->representation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);
	}
	else
	{
		AAMPLOG_WARN("Not able to find representation from manifest, sending error event");
		aamp->SendErrorEvent(AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR);
		return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
	}

	pMediaStreamContext->fragmentDescriptor.ClearMatchingBaseUrl();
	pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &mpd->GetBaseUrls() );
	pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &period->GetBaseURLs() );
	pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &pMediaStreamContext->adaptationSet->GetBaseURLs() );
	pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &pMediaStreamContext->representation->GetBaseURLs() );
	pMediaStreamContext->fragmentIndex = 0;
	pMediaStreamContext->fragmentRepeatCount = 0;
	pMediaStreamContext->fragmentOffset = 0;
	pMediaStreamContext->periodStartOffset = pMediaStreamContext->fragmentTime;
	if (0 == pMediaStreamContext->fragmentDescriptor.Bandwidth || !aamp->IsFogTSBSupported())
	{
		pMediaStreamContext->fragmentDescriptor.Bandwidth = pMediaStreamContext->representation->GetBandwidth();
	}
	pMediaStreamContext->fragmentDescriptor.RepresentationID.assign(pMediaStreamContext->representation->GetId());
	pMediaStreamContext->fragmentDescriptor.Time = 0;
	pMediaStreamContext->eos = false;
	pMediaStreamContext->mReachedFirstFragOnRewind = false;
	/* Resetting the timeline Index values, otherwise it will reach the max timelines.size, so undetermined issue might cause*/
	pMediaStreamContext->timeLineIndex = 0;

	pMediaStreamContext->fragmentTime = mPeriodStartTime;
	AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Track %d changed, updating fragmentTime to %lf fragmentDescriptor.Number %" PRIu64,AampMediaType(type), pMediaStreamContext->fragmentTime, pMediaStreamContext->fragmentDescriptor.Number);


	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),pMediaStreamContext->adaptationSet->GetSegmentTemplate());
	if( segmentTemplates.HasSegmentTemplate())
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		long int startNumber = segmentTemplates.GetStartNumber();
		double fragmentDuration = 0;
		bool timelineAvailable = true;
		pMediaStreamContext->fragmentDescriptor.TimeScale = segmentTemplates.GetTimescale();
		if(NULL == segmentTimeline)
		{
			uint32_t timeScale = segmentTemplates.GetTimescale();
			uint32_t duration = segmentTemplates.GetDuration();
			fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
			timelineAvailable = false;
			if( timeScale )
			{
				pMediaStreamContext->scaledPTO = (double)segmentTemplates.GetPresentationTimeOffset() / (double)timeScale;
			}
			if(!aamp->IsLive())
			{
				if(segmentTemplates.GetPresentationTimeOffset())
				{
					uint64_t ptoFragmentNumber = 0;
					ptoFragmentNumber = (pMediaStreamContext->scaledPTO / fragmentDuration) + 1;
					AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD: Track %d startnumber:%ld PTO specific fragment Number : %" PRIu64 ,AampMediaType(type),startNumber, ptoFragmentNumber);
					if(ptoFragmentNumber > startNumber)
					{
						startNumber = ptoFragmentNumber;
					}
				}
			}
		}
		pMediaStreamContext->fragmentDescriptor.Number = startNumber;
		if (mMPDParseHelper->GetLiveTimeFragmentSync() && !timelineAvailable)
		{
			pMediaStreamContext->fragmentDescriptor.Number += (long)((mMPDParseHelper->GetPeriodStartTime(0,mLastPlaylistDownloadTimeMs) - mAvailabilityStartTime) / fragmentDuration);
		}
		AAMPLOG_INFO("Exit : Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentTime %f segNumber %" PRIu64 " Ftime:%f" ,pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentTime, pMediaStreamContext->fragmentDescriptor.Number, pMediaStreamContext->fragmentDescriptor.Time);
		/*As the Init fragments download get happened only when the profile changed is true */
		pMediaStreamContext->profileChanged = true;
		return eAAMPSTATUS_OK;
	}
	else
	{
		AAMPLOG_TRACE("StreamAbstractionAAMP_MPD:: Segment template not available");
		return eAAMPSTATUS_MANIFEST_INVALID_TYPE;
	}

}
/**
 * @brief If Multiperiods exists then, this UpdateSeekPeriodOffset(), returns appropriate seek offset for skipping.
 * If mCurrentPeriodIdx moves ahead than the offsetFromStart index, then no fragments need to be skipped so the
 * offsetFromStart is updated as 0, otherwise the appropriate offsetFromStart returned for skipping in case if both exists
 * in same period.
 * @param[out] offsetFromStart offset in seconds to skip from beginning of the manifest
 */
void StreamAbstractionAAMP_MPD::UpdateSeekPeriodOffset( double &offsetFromStart )
{
	int currentSeekPeriodIdx = 0;
	std::string tempString;
	if (mpd == NULL || mMPDParseHelper == NULL)
	{
		AAMPLOG_WARN("Manifest is null mpd[%p] mMPDParseHelper[%p]", (void*)mpd, static_cast<AampMPDParseHelper*>(mMPDParseHelper.get()));
		return;
	}
	uint64_t nextPeriodStart = 0;
	double prevPeriodEndMs = 0; // used to find gaps between periods
	int numPeriods = (int)mMPDParseHelper->GetNumberOfPeriods();
	bool seekPeriods = true;

	for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
	{
		IPeriod *period = mpd->GetPeriods().at(iPeriod);
		if(mMPDParseHelper->IsEmptyPeriod(iPeriod, (rate != AAMP_NORMAL_PLAY_RATE)))
		{
			// Empty Period . Ignore processing, continue to next.
			continue;
		}
		std::string tempString = period->GetDuration();
		double  periodStartMs = 0;
		double periodDurationMs = 0;
		periodDurationMs =mMPDParseHelper->aamp_GetPeriodDuration(iPeriod, mLastPlaylistDownloadTimeMs);

		if(offsetFromStart >= 0 && seekPeriods)
		{
			tempString = period->GetStart();
			if(!tempString.empty() && !aamp->IsUninterruptedTSB())
			{
				periodStartMs = ParseISO8601Duration( tempString.c_str() );
			}
			else if (periodDurationMs)
			{
				periodStartMs = nextPeriodStart;
			}

			if(aamp->IsLiveStream() && !aamp->IsUninterruptedTSB() && iPeriod == 0)
			{
				// Adjust start time wrt presentation time offset.
				if(!aamp->IsLive())
				{
					periodStartMs += aamp->culledSeconds;
				}
				else
				{
					periodStartMs += (mMPDParseHelper->aamp_GetPeriodStartTimeDeltaRelativeToPTSOffset(period) * 1000);
				}
			}

			double periodStartSeconds = (double)periodStartMs/1000.0;
			double periodDurationSeconds = (double)periodDurationMs / 1000.0;
			if (periodDurationMs != 0)
			{
				double periodEnd = periodStartMs + periodDurationMs;
				nextPeriodStart += periodDurationMs; // set the value here, nextPeriodStart is used below to identify "Multi period assets with no period duration" if it is set to ZERO.

				// check for gaps between periods
				if(prevPeriodEndMs > 0)
				{
					double periodGap = (periodStartMs - prevPeriodEndMs)/ 1000; // current period start - prev period end will give us GAP between period
					if(std::abs(periodGap) > 0 ) // ohh we have GAP between last and current period
					{
						offsetFromStart -= periodGap; // adjust offset to accommodate gap
						if(offsetFromStart < 0 ) // this means offset is between gap, set to start of currentPeriod
						{
							offsetFromStart = 0;
						}
						AAMPLOG_WARN("GAP between period found :GAP:%f currentSeekPeriodIdx %d currentPeriodStart %f offsetFromStart %f",
								periodGap, currentSeekPeriodIdx, periodStartSeconds, offsetFromStart);
					}
				}
				prevPeriodEndMs = periodEnd; // store for future use
				currentSeekPeriodIdx = iPeriod;
				if (periodDurationSeconds <= offsetFromStart && iPeriod < (numPeriods - 1))
				{
					offsetFromStart -= periodDurationSeconds;
					AAMPLOG_WARN("Skipping period %d seekPosition %f periodEnd %f offsetFromStart %f", iPeriod, seekPosition, periodEnd, offsetFromStart);
					continue;
				}
				else
				{
					seekPeriods = false;
				}
			}
			else if(periodStartSeconds <= offsetFromStart)
			{
				currentSeekPeriodIdx = iPeriod;
			}
		}
	}
	if( offsetFromStart < 0 )
	{
		offsetFromStart = 0;
	}
	if( currentSeekPeriodIdx < mCurrentPeriodIdx )
	{
		//No need to do any skip, just updating the period details alone, and make the offsetFromStart(seekposition) to 0
		AAMPLOG_INFO("Skip not required, as mCurrentPeriodIdx[%u] moved ahead of currentSeekPeriodIdx[%u]", mCurrentPeriodIdx, currentSeekPeriodIdx);
		offsetFromStart = 0;
	}
}

/**
 * @brief  For computing the current fragment duration.
 * @param  MediaStreamContext
 * @return current duration value
 */
uint32_t StreamAbstractionAAMP_MPD::GetCurrentFragmentDuration( MediaStreamContext *pMediaStreamContext )
{
	uint32_t duration = 0;
	if (!pMediaStreamContext)
	{
		AAMPLOG_WARN("pMediaStreamContext is null");
		return duration;
	}
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
			pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
	if (segmentTemplates.HasSegmentTemplate())
	{
		AAMPLOG_INFO("Enter : Type[%d] timeLineIndex %d fragmentRepeatCount %d fragmentTime %f segNumber %" PRIu64 " Ftime:%f" ,pMediaStreamContext->type,
				pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentRepeatCount, pMediaStreamContext->fragmentTime, pMediaStreamContext->fragmentDescriptor.Number,pMediaStreamContext->fragmentDescriptor.Time);

		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();

		if (NULL != segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
			if (pMediaStreamContext->timeLineIndex >= timelines.size() || pMediaStreamContext->timeLineIndex < 0)
			{
				AAMPLOG_INFO("Type[%d] EOS. timeLineIndex[%d] size [%zu]",pMediaStreamContext->type, pMediaStreamContext->timeLineIndex, timelines.size());
				pMediaStreamContext->eos = true;
				return 0;
			}
			else
			{
				ITimeline *timeline = timelines.at(pMediaStreamContext->timeLineIndex);
				duration = timeline->GetDuration();
			}
		}
		else
		{
			AAMPLOG_TRACE("StreamAbstractionAAMP_MPD:: Segment template not available");
		}
	}
	return duration;
}

/**
 * @brief Switch to the corresponding audio track, Load new audio, flush the old audio fragments,
 * select the new audio track, set the new audio info, update the new audio track, then skip to the appropriate
 * path to download.
 */
void StreamAbstractionAAMP_MPD::SwitchAudioTrack()
{
	std::vector<AudioTrackInfo> aTracks;
	std::string aTrackIdx;
	int audioRepresentationIndex = -1;
	int audioAdaptationSetIndex = -1;
	IPeriod *period = mCurrentPeriod;
	int  selAdaptationSetIndex = -1;
	int selRepresentationIndex = -1;
	AAMPStatusType ret = eAAMPSTATUS_OK;
	uint64_t oldMediaSequenceNumber = 0;
	uint32_t fragmentDuration = 0;
	double   oldPlaylistPosition,diffInFetchedDuration,diffInInjectedDuration,newInjectedPosition;
	int diffFragmentsDownloaded = 0;
	double offsetFromStart;

	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_AUDIO];
	if ((!pMediaStreamContext) || (!pMediaStreamContext->enabled))
	{
		AAMPLOG_WARN("pMediaStreamContext  is null");
		pMediaStreamContext->NotifyCachedAudioFragmentAvailable();
		return;
	}
	pMediaStreamContext->LoadNewAudio(true);
	/* Flush Audio Fragments */
	pMediaStreamContext->FlushFragments();
	if( pMediaStreamContext->freshManifest )
	{
		/*In Live scenario, the manifest refresh got happened frequently,so in the UpdateTrackinfo(), all the params
		* get reset lets say.., pMediaStreamContext->fragmentDescriptor.Number, fragmentTime.., so during that case, we are getting
		* totalfetchDuration, totalInjectedDuration as negative values once we subtract the OldPlaylistpositions with the Newplaylistpositions, for avoiding
		* this in the case if the manifest got updated, we have called the PushNextFragment(), for the proper update of the params like
		* pMediaStreamContext->fragmentTime, pMediaStreamContext->fragmentDescriptor.Number and pMediaStreamContext->fragmentDescriptor.Time
		*/
		AAMPLOG_INFO("Manifest got updated[%u]",pMediaStreamContext->freshManifest);
		PushNextFragment(pMediaStreamContext, getCurlInstanceByMediaType(static_cast<AampMediaType>(eMEDIATYPE_AUDIO)),true);
	}
	/* Switching to selected Audio Track */
	SelectAudioTrack(aTracks,aTrackIdx,audioAdaptationSetIndex,audioRepresentationIndex);

	selAdaptationSetIndex = audioAdaptationSetIndex;
	selRepresentationIndex = audioRepresentationIndex;
	aTrackIdx = std::to_string(selAdaptationSetIndex) + "-" + std::to_string(selRepresentationIndex);
	if (selAdaptationSetIndex >= 0)
	{
		pMediaStreamContext->adaptationSetIdx = selAdaptationSetIndex;
	}
	else
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No valid adaptation set found for Media[%s]",
				GetMediaTypeName(AampMediaType(eMEDIATYPE_AUDIO)));
	}
	if (selRepresentationIndex >= 0)
	{
		pMediaStreamContext->representationIndex = selRepresentationIndex;
	}
	else
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No valid RepresentationIndex found for Media[%s]",
				GetMediaTypeName(AampMediaType(eMEDIATYPE_AUDIO)));
	}
	// Skip processing content protection for multi video, since the right adaptation will be selected only after ABR
	// This might cause an unwanted content prot to be queued and delay playback start
	if ( !mMultiVideoAdaptationPresent)
	{
		AAMPLOG_WARN("Queueing content protection from StreamSelection for type:%d", eMEDIATYPE_AUDIO);
		QueueContentProtection(period, selAdaptationSetIndex, (AampMediaType)eMEDIATYPE_AUDIO);
	}
	AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] Adaptation set[%d] RepIdx[%d] TrackCnt[%d]",
			GetMediaTypeName(AampMediaType(eMEDIATYPE_AUDIO)),selAdaptationSetIndex,selRepresentationIndex,(mNumberOfTracks+1) );

	AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Media[%s] %s",
			GetMediaTypeName(AampMediaType(eMEDIATYPE_AUDIO)), pMediaStreamContext->enabled?"enabled":"disabled");

	// Set audio/text track related structures
	SetAudioTrackInfo(mAudioTracks, aTrackIdx);
	/**< log selected track information to support triage **/
	printSelectedTrack(aTrackIdx, eMEDIATYPE_AUDIO);
	std::vector<BitsPerSecond> bitratelist;

	for (auto &audioTrack : mAudioTracks)
	{
		if (audioTrack.index == aTrackIdx)
		{
			pMediaStreamContext->SetCurrentBandWidth((int)audioTrack.bandwidth);
		}
	}
	/* Caching the oldPlaylistPosition and oldMediaSequenceNumber */
	oldPlaylistPosition = pMediaStreamContext->fragmentTime;
	oldMediaSequenceNumber = pMediaStreamContext->fragmentDescriptor.Number;

	/* Getting Gstreamer Play position */
	offsetFromStart = aamp->GetPositionSeconds() - aamp->culledSeconds;
	AAMPLOG_INFO( "Playlist pos offsetFromStart[%lf] culledSeconds[%lf]",offsetFromStart,aamp->culledSeconds );

	UpdateSeekPeriodOffset(offsetFromStart);
	AAMPLOG_INFO( "Updated pos offsetFromStart[%lf] culledSeconds[%lf]",offsetFromStart,aamp->culledSeconds );

	/*Updating audio Tracks info*/
	ret = UpdateMediaTrackInfo(eMEDIATYPE_AUDIO);
	if (ret != eAAMPSTATUS_OK)
	{
		pMediaStreamContext->NotifyCachedAudioFragmentAvailable();
		return;
	}
	/* Skipping the old fragments and moving to the corresponding Audio playlist position
	* for fetching and injecting during seamless audio switch.
	* Skipping the audio fragments to reach the current position for fetching
	* */
	SkipFragments(pMediaStreamContext, offsetFromStart, true);
	/* Getting current fragment duration, for calculating the injected duration timestamp, because the current injected time
	 * calculation requires the start time of the fragment which is downloaded, so subtracting the fragment duration for identification
	 * the starttime of the downloaded fragment.
	 */
	fragmentDuration = GetCurrentFragmentDuration( pMediaStreamContext );

	/* In the case, If the Skiptime is 0, In the Skipfragment the lastsegment related params were not updated.,
	* due to the cond (pMediaStreamContext->fragmentDescriptor.Time<pMediaStreamContext->lastSegmentTime),in the PushNextFragment()
	* loops untill the Fdt time reaches the last segment time and the wrong Audio fragment gets Pushed, so reseting all the
	* lastsegment related params here...
	*/
	pMediaStreamContext->lastSegmentTime = pMediaStreamContext->fragmentDescriptor.Time - fragmentDuration;
	pMediaStreamContext->lastSegmentDuration = pMediaStreamContext->fragmentDescriptor.Time;
	pMediaStreamContext->lastSegmentNumber = pMediaStreamContext->fragmentDescriptor.Number - 1;



	/* Calculating the start time of the downloaded fragment */
	newInjectedPosition = ( pMediaStreamContext->fragmentDescriptor.Time - fragmentDuration )/pMediaStreamContext->fragmentDescriptor.TimeScale;

	/*Calculating the difference in Fetched duration, injected duration and diff in Media Sequence number */
	diffInFetchedDuration = oldPlaylistPosition - pMediaStreamContext->fragmentTime;
	diffInInjectedDuration = ( pMediaStreamContext->GetLastInjectedPosition() - newInjectedPosition );
	diffFragmentsDownloaded = static_cast<int>(oldMediaSequenceNumber - pMediaStreamContext->fragmentDescriptor.Number);

	AAMPLOG_INFO("Calculated oldPlaylistPosition[%lf] newPlaylistPosition[%lf] diffInFetchedDuration[%lf] LastInjectedDuration[%lf] Duration[%u], newInjectedPosition[%lf] diffInInjectedDuration[%lf] oldMediaSequenceNumber[%" PRIu64 "] newMediaSequenceNumber[%" PRIu64 "] diffFragmentsDownloaded[%d]",
			oldPlaylistPosition,pMediaStreamContext->fragmentTime,diffInFetchedDuration, pMediaStreamContext->GetLastInjectedPosition(),
			fragmentDuration, newInjectedPosition, diffInInjectedDuration,oldMediaSequenceNumber, pMediaStreamContext->fragmentDescriptor.Number,diffFragmentsDownloaded);

	pMediaStreamContext->resetAbort(false);
	pMediaStreamContext->OffsetTrackParams(diffInFetchedDuration, diffInInjectedDuration, diffFragmentsDownloaded);
	/*Fetch and injecting initialization params*/
	FetchAndInjectInitialization(eMEDIATYPE_AUDIO, false);
}

/**
 * @brief Does stream selection
 */
void StreamAbstractionAAMP_MPD::StreamSelection( bool newTune, bool forceSpeedsChangedEvent)
{
	std::vector<AudioTrackInfo> aTracks;
	std::vector<TextTrackInfo> tTracks;
	std::string aTrackIdx;
	std::string tTrackIdx;
	mNumberOfTracks = 0;
	IPeriod *period = mCurrentPeriod;
	int audioRepresentationIndex = -1;
	int audioAdaptationSetIndex = -1;

	if(!period)
	{
		AAMPLOG_WARN("period is null");  //CID:84742 - Null Returns
		return;
	}
	AAMPLOG_INFO("Selected Period index %d, id %s", mCurrentPeriodIdx, period->GetId().c_str());
	for( int i = 0; i < mMaxTracks; i++ )
	{
		mMediaStreamContext[i]->enabled = false;
	}

	mMultiVideoAdaptationPresent = false;

	if (rate == AAMP_NORMAL_PLAY_RATE)
	{
		SelectAudioTrack(aTracks,aTrackIdx,audioAdaptationSetIndex,audioRepresentationIndex);
	}

	for (int i = 0; i < mMaxTracks; i++)
	{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[i];
		size_t numAdaptationSets = period->GetAdaptationSets().size();
		int  selAdaptationSetIndex = -1;
		int selRepresentationIndex = -1;
		bool isIframeAdaptationAvailable = false;
		bool encryptedIframeTrackPresent = false;
		int videoRepresentationIdx;   //CID:118900 - selRepBandwidth variable locally declared but not reflected

		if (eMEDIATYPE_SUBTITLE == i)
		{
			SelectSubtitleTrack(newTune,tTracks,tTrackIdx);
			if(mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled)
			{
				mNumberOfTracks++;
			}
		}
		if(eMEDIATYPE_SUBTITLE !=i)
		{
			for (unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets; iAdaptationSet++)
			{
				IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
				AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD: Content type [%s] AdapSet [%d] ",
						adaptationSet->GetContentType().c_str(), iAdaptationSet);
				if (mMPDParseHelper->IsContentType(adaptationSet, (AampMediaType)i ))
				{
					ParseTrackInformation(adaptationSet, iAdaptationSet, (AampMediaType)i,  aTracks, tTracks);
									//if isFrstAvailableTxtTrackSelected is true, we should look for the best option (aamp->mSubLanguage) among all the tracks
					if (AAMP_NORMAL_PLAY_RATE == rate)
					{
						if (eMEDIATYPE_AUX_AUDIO == i && aamp->IsAuxiliaryAudioEnabled())
						{
							if (aamp->GetAuxiliaryAudioLanguage() == aamp->mAudioTuple.language)
							{
								AAMPLOG_WARN("PrivateStreamAbstractionMPD: auxiliary audio same as primary audio, set forward audio flag");
								SetAudioFwdToAuxStatus(true);
								break;
							}
							else if (IsMatchingLanguageAndMimeType((AampMediaType)i, aamp->GetAuxiliaryAudioLanguage(), adaptationSet, selRepresentationIndex) == true)
							{
								selAdaptationSetIndex = iAdaptationSet;
							}
						}
						else if (eMEDIATYPE_AUDIO == i)
						{
							selAdaptationSetIndex = audioAdaptationSetIndex;
							selRepresentationIndex = audioRepresentationIndex;
							aTrackIdx = std::to_string(selAdaptationSetIndex) + "-" + std::to_string(selRepresentationIndex);
						}
						else if (eMEDIATYPE_VIDEO == i && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
						{
							// Parse all the video adaptation sets except the iframe tracks
							// If the content contains multiple video adaptation, we should employ ABR to select the adaptation set
							if (!IsIframeTrack(adaptationSet))
							{
								// Got Video , confirmed its not iframe adaptation
								videoRepresentationIdx = GetDesiredVideoCodecIndex(adaptationSet);
								if (videoRepresentationIdx != -1)
								{
									if (selAdaptationSetIndex == -1)
									{
										selAdaptationSetIndex = iAdaptationSet;
										if(!newTune)
										{
											if((GetProfileCount() == adaptationSet->GetRepresentation().size()) &&
												(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState))
											{
												selRepresentationIndex = pMediaStreamContext->representationIndex;
											}
											else
											{
												selRepresentationIndex = -1; // this will be set based on profile selection
											}
										}
									}
									else
									{
										// Multiple video adaptation identified
										mMultiVideoAdaptationPresent = true;
										if (isIframeAdaptationAvailable)
										{
											// We have confirmed that iframe track and multiple video adaptation are present
											// Already video adaptation index is set. Break from the loop
											break;
										}
									}
								}
							}
							else
							{
								isIframeAdaptationAvailable = true;
							}
						}
					}
					else if ((!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback)) && (eMEDIATYPE_VIDEO == i))
					{
						//iframe track
						if ( IsIframeTrack(adaptationSet) )
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Got TrickMode track");
							pMediaStreamContext->enabled = true;
							pMediaStreamContext->profileChanged = true;
							pMediaStreamContext->adaptationSetIdx = iAdaptationSet;
							mNumberOfTracks = 1;
							isIframeAdaptationAvailable = true;
							if(!mMPDParseHelper->GetContentProtection(adaptationSet).empty())
							{
								encryptedIframeTrackPresent = true;
								AAMPLOG_WARN("PrivateStreamAbstractionMPD: Detected encrypted iframe track");
							}
							break;
						}
					}
				}
			}// next iAdaptationSet
			if ((eAUDIO_UNKNOWN == mAudioType) && (AAMP_NORMAL_PLAY_RATE == rate) && (eMEDIATYPE_VIDEO != i) && selAdaptationSetIndex >= 0)
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Selected Audio Track codec is unknown");
				mAudioType = eAUDIO_AAC; // assuming content is still playable
			}
			if ((AAMP_NORMAL_PLAY_RATE == rate) && (pMediaStreamContext->enabled == false) && selAdaptationSetIndex >= 0)
			{
				pMediaStreamContext->enabled = true;
				pMediaStreamContext->adaptationSetIdx = selAdaptationSetIndex;
				pMediaStreamContext->representationIndex = selRepresentationIndex;
				pMediaStreamContext->profileChanged = true;

				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] Adaptation set[%d] RepIdx[%d] TrackCnt[%d]",
					GetMediaTypeName(AampMediaType(i)),selAdaptationSetIndex,selRepresentationIndex,(mNumberOfTracks+1) );

				// Skip processing content protection for multi video, since the right adaptation will be selected only after ABR
				// This might cause an unwanted content prot to be queued and delay playback start
				if (eMEDIATYPE_VIDEO != i || !mMultiVideoAdaptationPresent)
				{
					AAMPLOG_WARN("Queueing content protection from StreamSelection for type:%d", i);
					QueueContentProtection(period, selAdaptationSetIndex, (AampMediaType)i);
				}

				mNumberOfTracks++;
			}
			else if (encryptedIframeTrackPresent) //Process content protection for encrypted Iframe
			{
				QueueContentProtection(period, pMediaStreamContext->adaptationSetIdx, (AampMediaType)i);
			}

			if(selAdaptationSetIndex < 0 && rate == 1)
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No valid adaptation set found for Media[%s]",
					GetMediaTypeName(AampMediaType(i)));
			}

			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Media[%s] %s",
				GetMediaTypeName(AampMediaType(i)), pMediaStreamContext->enabled?"enabled":"disabled");
			//This is for cases where subtitle is not enabled, but auxiliary audio track is enabled
			if (eMEDIATYPE_AUX_AUDIO == i && pMediaStreamContext->enabled && !mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled)
			{
				AAMPLOG_WARN("PrivateStreamAbstractionMPD: Auxiliary enabled, but subtitle disabled, swap MediaStreamContext of both");
				mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled = mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->enabled;
				mMediaStreamContext[eMEDIATYPE_SUBTITLE]->adaptationSetIdx = mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->adaptationSetIdx;
				mMediaStreamContext[eMEDIATYPE_SUBTITLE]->representationIndex = mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->representationIndex;
				mMediaStreamContext[eMEDIATYPE_SUBTITLE]->mediaType = eMEDIATYPE_AUX_AUDIO;
				mMediaStreamContext[eMEDIATYPE_SUBTITLE]->type = eTRACK_AUX_AUDIO;
				mMediaStreamContext[eMEDIATYPE_SUBTITLE]->profileChanged = true;
				mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->enabled = false;
			}

			if( aamp->IsLocalAAMPTsbFromConfig() && aamp->IsIframeExtractionEnabled())
			{
				/** Fake the iframe track if AAMP TSB and i-frame extraction are enabled */
				isIframeAdaptationAvailable = true;
			}

			//Store the iframe track status in current period if there is any change
			if (!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback) && (i == eMEDIATYPE_VIDEO) && (aamp->mIsIframeTrackPresent != isIframeAdaptationAvailable))
			{
				aamp->mIsIframeTrackPresent = isIframeAdaptationAvailable;
				//Iframe tracks changed mid-stream, sent a playbackspeed changed event
				if (!newTune || forceSpeedsChangedEvent)
				{
					aamp->SendSupportedSpeedsChangedEvent(aamp->mIsIframeTrackPresent);
				}
			}
		}

	} // next track
	if (aamp->mDRMLicenseManager)
	{
		AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
		if (mMultiVideoAdaptationPresent)
		{
			// We have multiple video adaptations in the same period and
			// if one of them fails in license acquisition, we can skip error event
			licenseManager->SetSendErrorOnFailure(false);
		}
		else
		{
			licenseManager->SetSendErrorOnFailure(true);
		}
	}

	if(1 == mNumberOfTracks && !mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled)
	{ // what about audio+subtitles?
		if(newTune)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Audio only period");
			// set audio only playback flag to true
			aamp->mAudioOnlyPb = true;
		}
		mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled = mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->adaptationSetIdx = mMediaStreamContext[eMEDIATYPE_AUDIO]->adaptationSetIdx;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->representationIndex = mMediaStreamContext[eMEDIATYPE_AUDIO]->representationIndex;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->mediaType = eMEDIATYPE_VIDEO;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->type = eTRACK_VIDEO;
		mMediaStreamContext[eMEDIATYPE_VIDEO]->profileChanged = true;
		mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled = false;
	}
	// Set audio/text track related structures
	SetAudioTrackInfo(aTracks, aTrackIdx);

	/**< log selected track information to support triage **/
	printSelectedTrack(aTrackIdx, eMEDIATYPE_AUDIO);

	SetTextTrackInfo(tTracks, tTrackIdx);
	printSelectedTrack(tTrackIdx, eMEDIATYPE_SUBTITLE);
	std::vector<BitsPerSecond> bitratelist;
	for (auto &audioTrack : aTracks)
	{
		if(audioTrack.index == aTrackIdx)
		{
			mMediaStreamContext[eMEDIATYPE_AUDIO]->SetCurrentBandWidth((int)audioTrack.bandwidth);
		}
		bitratelist.push_back(audioTrack.bandwidth);
	}
	aamp->mCMCDCollector->SetBitrates(eMEDIATYPE_AUDIO, bitratelist);
}


/**
 * @brief Get profile index for bandwidth notification
 * @retval profile index of the current bandwidth
 */
int StreamAbstractionAAMP_MPD::GetProfileIdxForBandwidthNotification(uint32_t bandwidth)
{
	int profileIndex = 0; // Keep default index as 0

	std::vector<BitsPerSecond>::iterator it = std::find(mBitrateIndexVector.begin(), mBitrateIndexVector.end(), (long)bandwidth);

	if (it != mBitrateIndexVector.end())
	{
		// Get index of _element_ from iterator
		profileIndex = (int)std::distance(mBitrateIndexVector.begin(), it);
	}

	return profileIndex;
}

/**
 * @brief GetCurrentMimeType
 * @param AampMediaType type of media
 * @retval mimeType
 */
std::string StreamAbstractionAAMP_MPD::GetCurrentMimeType(AampMediaType mediaType)
{
	std::string mimeType;
	if( mediaType < mNumberOfTracks )
	{
		auto pMediaStreamContext = mMediaStreamContext[mediaType];
		if( pMediaStreamContext )
		{
			if( pMediaStreamContext->representation )
			{
				mimeType = pMediaStreamContext->representation->GetMimeType();
			}
			if( mimeType.empty() )
			{
				if( pMediaStreamContext->adaptationSet )
				{
					mimeType = pMediaStreamContext->adaptationSet->GetMimeType();
				}
			}
		}
	}
	if( mimeType.empty() )
	{
		AAMPLOG_WARN( "unknown" );
	}
	else
	{
		AAMPLOG_DEBUG( "%s", mimeType.c_str() );
	}
	return mimeType;
}

/**
 * @brief Is this a Webvm video codec
 *
 * @param codec Name of codec
 * @return true if this a VPx codec, false otherwise
 */
static bool IsWebmVideoCodec(const std::string &codec )
{
	/* For example, "vp09.00.10.08". */
	return codec.rfind("vp", 0) == 0;
}

/**
 * @brief Updates track information based on current state
 */
AAMPStatusType StreamAbstractionAAMP_MPD::UpdateTrackInfo(bool modifyDefaultBW, bool resetTimeLineIndex)
{
	AAMPStatusType ret = eAAMPSTATUS_OK;
	long defaultBitrate = aamp->GetDefaultBitrate();
	long iframeBitrate = aamp->GetIframeBitrate();
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;	/*Conveys whether the current playback from FOG or not.*/
	long minBitrate = aamp->GetMinimumBitrate();
	long maxBitrate = aamp->GetMaximumBitrate();
	bool periodChanged = false;
	std::set<uint32_t> chosenAdaptationIdxs;

	if(mUpdateStreamInfo)
	{
		periodChanged = true;
		mABRMode = ABRMode::UNDEF;
	}

	for (int i = 0; i < mNumberOfTracks; i++)
	{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[i];
		if(!pMediaStreamContext)
		{
			AAMPLOG_WARN("pMediaStreamContext  is null");  //CID:82464,84186 - Null Returns
			return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}
		if(mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING)
		{
			AdNode &adNode = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
			if(adNode.mpd != NULL)
			{
				pMediaStreamContext->fragmentDescriptor.manifestUrl = adNode.url.c_str();
			}
		}
		if(pMediaStreamContext->enabled)
		{
			IPeriod *period = mCurrentPeriod;
			/**< Safe check to avoid wrong access on adaptation due to update track
			info called before stream selection in fragment downloader thread on period switching */
			auto numAdaptationSets = period->GetAdaptationSets().size();
			if( numAdaptationSets==0 )
			{
				AAMPLOG_WARN("empty period");
				pMediaStreamContext->adaptationSet = NULL;
				continue;
			}
			if (pMediaStreamContext->adaptationSetIdx >= numAdaptationSets )
			{
				pMediaStreamContext->adaptationSetIdx = 0;
			}
			pMediaStreamContext->adaptationSet = period->GetAdaptationSets().at(pMediaStreamContext->adaptationSetIdx);
			pMediaStreamContext->adaptationSetId = pMediaStreamContext->adaptationSet->GetId();
			std::string adapFrameRate = pMediaStreamContext->adaptationSet->GetFrameRate();
			/*Populate StreamInfo for ABR Processing*/
			if (i == eMEDIATYPE_VIDEO)
			{
				if(isFogTsb && mUpdateStreamInfo)
				{
					mUpdateStreamInfo = false;
					vector<Representation *> representations = mMPDParseHelper->GetBitrateInfoFromCustomMpd(pMediaStreamContext->adaptationSet);
					int representationCount = (int)representations.size();
					if ((representationCount != mBitrateIndexVector.size()) && mStreamInfo)
					{
						SAFE_DELETE_ARRAY(mStreamInfo);
					}
					if (!mStreamInfo)
					{
						mStreamInfo = new StreamInfo[representationCount];
					}
					GetABRManager().clearProfiles();
					mBitrateIndexVector.clear();
					mMaxTSBBandwidth = 0;
					mABRMode = ABRMode::FOG_TSB;
					int idx = 0;
					for (idx = 0; idx < representationCount; idx++)
					{
						Representation* representation = representations.at(idx);
						if(representation != NULL)
						{
							mStreamInfo[idx].bandwidthBitsPerSecond = representation->GetBandwidth();
							mStreamInfo[idx].isIframeTrack = !(AAMP_NORMAL_PLAY_RATE == rate);
							mStreamInfo[idx].resolution.height = representation->GetHeight();
							mStreamInfo[idx].resolution.width = representation->GetWidth();
							mStreamInfo[idx].resolution.framerate = 0;
							mStreamInfo[idx].enabled = false;
							mStreamInfo[idx].validity = false;
							//Get video codec details
							if (representation->GetCodecs().size())
							{
								mStreamInfo[idx].codecs = representation->GetCodecs().at(0);
							}
							else if (pMediaStreamContext->adaptationSet->GetCodecs().size())
							{
								mStreamInfo[idx].codecs = pMediaStreamContext->adaptationSet->GetCodecs().at(0);
							}
							else
							{
								mStreamInfo[idx].codecs.clear();
							}
							//Update profile resolution with VideoEnd Metrics object.
							aamp->UpdateVideoEndProfileResolution((mStreamInfo[idx].isIframeTrack ? eMEDIATYPE_IFRAME : eMEDIATYPE_VIDEO ),
													mStreamInfo[idx].bandwidthBitsPerSecond,
													mStreamInfo[idx].resolution.width,
													mStreamInfo[idx].resolution.height);

							std::string repFrameRate = representation->GetFrameRate();
							if(repFrameRate.empty())
								repFrameRate = adapFrameRate;
							if(!repFrameRate.empty())
							{
								int val1, val2;
								sscanf(repFrameRate.c_str(),"%d/%d",&val1,&val2);
								double frate = val2? ((double)val1/val2):val1;
								mStreamInfo[idx].resolution.framerate = frate;
							}

							SAFE_DELETE(representation);
							// Skip profile by resolution, if profile capping already applied in Fog
							if(ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mProfileCappedStatus &&  aamp->mDisplayWidth > 0 && mStreamInfo[idx].resolution.width > aamp->mDisplayWidth)
							{
								AAMPLOG_INFO ("Video Profile Ignoring resolution=%d:%d display=%d:%d Bw=%" BITSPERSECOND_FORMAT, mStreamInfo[idx].resolution.width, mStreamInfo[idx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[idx].bandwidthBitsPerSecond);
								continue;
							}
							AAMPLOG_INFO("Added Video Profile to ABR BW=%" BITSPERSECOND_FORMAT" to bitrate vector index:%d", mStreamInfo[idx].bandwidthBitsPerSecond, idx);
							mStreamInfo[idx].enabled = true;
							mBitrateIndexVector.push_back(mStreamInfo[idx].bandwidthBitsPerSecond);
							if(mStreamInfo[idx].bandwidthBitsPerSecond > mMaxTSBBandwidth)
							{
								mMaxTSBBandwidth = mStreamInfo[idx].bandwidthBitsPerSecond;
								mTsbMaxBitrateProfileIndex = idx;
							}
						}
						else
						{
							AAMPLOG_WARN("representation  is null");  //CID:82489 - Null Returns
						}
					}
					mProfileCount = idx;
					pMediaStreamContext->representationIndex = 0; //Fog custom mpd has a single representation
					IRepresentation* representation = pMediaStreamContext->adaptationSet->GetRepresentation().at(0);
					pMediaStreamContext->fragmentDescriptor.Bandwidth = representation->GetBandwidth();
					aamp->profiler.SetBandwidthBitsPerSecondVideo(pMediaStreamContext->fragmentDescriptor.Bandwidth);
					profileIdxForBandwidthNotification = GetProfileIdxForBandwidthNotification(pMediaStreamContext->fragmentDescriptor.Bandwidth);
					pMediaStreamContext->SetCurrentBandWidth(pMediaStreamContext->fragmentDescriptor.Bandwidth);
				}
				else if(!isFogTsb && mUpdateStreamInfo)
				{
					mUpdateStreamInfo = false;
					int representationCount = 0;
					std::set<uint32_t> blAdaptationIdxs;
					bool bSeenNonWebmCodec = false;

					const auto &blacklistedProfiles = aamp->GetBlacklistedProfiles();
					// Filter the blacklisted profiles in the period
					for (auto &blProfile : blacklistedProfiles)
					{
						if (blProfile.mPeriodId == period->GetId())
						{
							blAdaptationIdxs.insert(blProfile.mAdaptationSetIdx);
						}
					}
					AAMPLOG_WARN("Blacklisted adaptationSet in this period: %zu", blAdaptationIdxs.size());
					const auto &adaptationSets = period->GetAdaptationSets();
					for (uint32_t idx = 0; idx < adaptationSets.size(); idx++ )
					{
						// If index is not present in blacklisted adaptation indexes, proceed further
						if (blAdaptationIdxs.find(idx) == blAdaptationIdxs.end() && mMPDParseHelper->IsContentType(adaptationSets[idx], eMEDIATYPE_VIDEO))
						{
							// count iframe/video representations based on trickplay for allocating streamInfo.
							bool selected = UseIframeTrack() ? IsIframeTrack(adaptationSets[idx]) : !IsIframeTrack(adaptationSets[idx]);
							if (selected)
							{
								representationCount += adaptationSets[idx]->GetRepresentation().size();
								chosenAdaptationIdxs.insert(idx);
							}
						}
					}
					if(adaptationSets.size() > 0)
					{
						IAdaptationSet* adaptationSet = adaptationSets.at(0);
						if ((mNumberOfTracks == 1) && (mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_AUDIO)))
						{
							representationCount += adaptationSet->GetRepresentation().size();
							// Not to be done here, since its handled in a separate if condition below
							// chosenAdaptationIdxs.insert(0);
						}
					}
					if ((representationCount != GetProfileCount()) && mStreamInfo)
					{
						SAFE_DELETE_ARRAY(mStreamInfo);

						// reset representationIndex to -1 to allow updating the currentProfileIndex for period change.
						pMediaStreamContext->representationIndex = -1;
						AAMPLOG_WARN("representationIndex set to (-1) to find currentProfileIndex");
					}
					if (!mStreamInfo)
					{
						mStreamInfo = new StreamInfo[representationCount];
					}
					GetABRManager().clearProfiles();
					mBitrateIndexVector.clear();
					mABRMode = ABRMode::ABR_MANAGER;
					int addedProfiles = 0;
					int idx = 0;
					std::map<int, struct ProfileInfo> iProfileMaps;
					bool resolutionCheckEnabled = false;
					bool bVideoCapped = false;
					bool bIframeCapped = false;
					// Iterate through the chosen adaptation sets instead of whole
					for (const uint32_t &adaptIdx : chosenAdaptationIdxs)
					{
						IAdaptationSet* adaptationSet = adaptationSets.at(adaptIdx);
						const auto &representations = adaptationSet->GetRepresentation();
						for (int reprIdx = 0; reprIdx < representations.size(); reprIdx++)
						{
							IRepresentation *representation = representations.at(reprIdx);
							mStreamInfo[idx].bandwidthBitsPerSecond = representation->GetBandwidth();
							mStreamInfo[idx].isIframeTrack = !(AAMP_NORMAL_PLAY_RATE == rate);
							mStreamInfo[idx].resolution.height = representation->GetHeight();
							mStreamInfo[idx].resolution.width = representation->GetWidth();
							mStreamInfo[idx].resolution.framerate = 0;
							std::string repFrameRate = representation->GetFrameRate();
							// Get codec details for video profile
							if (representation->GetCodecs().size())
							{
								mStreamInfo[idx].codecs = representation->GetCodecs().at(0);
							}
							else if (adaptationSet->GetCodecs().size())
							{
								mStreamInfo[idx].codecs = adaptationSet->GetCodecs().at(0);
							}
							else
							{
								mStreamInfo[idx].codecs.clear();
							}
							// See if a non Webm video code (i.e. not VP8 or VP9) is listed.
							// These will be used in preference to VP8 or VP9.
							if( !bSeenNonWebmCodec )
							{
								if( !IsWebmVideoCodec(mStreamInfo[idx].codecs) )
								{
									bSeenNonWebmCodec = true;
								}
							}

							mStreamInfo[idx].enabled = false;
							mStreamInfo[idx].validity = false;
							if(repFrameRate.empty())
								repFrameRate = adapFrameRate;
							if(!repFrameRate.empty())
							{
								int val1, val2;
								sscanf(repFrameRate.c_str(),"%d/%d",&val1,&val2);
								double frate = val2? ((double)val1/val2):val1;
								mStreamInfo[idx].resolution.framerate = frate;
							}
							// Map profile index to corresponding adaptationSet and representation index
							iProfileMaps[idx].adaptationSetIndex = (int)adaptIdx;
							iProfileMaps[idx].representationIndex = reprIdx;

							if (ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mDisplayWidth > 0 && aamp->mDisplayHeight > 0)
							{
								if (representation->GetWidth() <= aamp->mDisplayWidth)
								{
									resolutionCheckEnabled = true;
								}
								else
								{
									if (mStreamInfo[idx].isIframeTrack)
										bIframeCapped = true;
									else
										bVideoCapped = true;
								}
							}
							idx++;
						}
					}
					// To select profiles bitrates nearer with user configured bitrate list
					if (aamp->userProfileStatus)
					{
						for (int i = 0; i < aamp->bitrateList.size(); i++)
						{
							int curIdx = 0;
							long curValue, diff;
							// traverse all profiles and select closer to user bitrate
							for (int pidx = 0; pidx < idx; pidx++)
							{
								diff = abs(mStreamInfo[pidx].bandwidthBitsPerSecond - aamp->bitrateList.at(i));
								if ((0 == pidx) || (diff < curValue))
								{
									curValue = diff;
									curIdx = pidx;
								}
							}

							mStreamInfo[curIdx].validity = true;
						}
					}
					mProfileCount = idx;
					for (int pidx = 0; pidx < idx; pidx++)
					{
						if (false == aamp->userProfileStatus && resolutionCheckEnabled && (mStreamInfo[pidx].resolution.width > aamp->mDisplayWidth) &&
							((mStreamInfo[pidx].isIframeTrack && bIframeCapped) ||
							(!mStreamInfo[pidx].isIframeTrack && bVideoCapped)))
						{
							AAMPLOG_INFO("Video Profile ignoring for resolution= %d:%d display= %d:%d BW=%" BITSPERSECOND_FORMAT, mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[pidx].bandwidthBitsPerSecond);
						}
						else if( bSeenNonWebmCodec && IsWebmVideoCodec(mStreamInfo[pidx].codecs) )
						{
							// Don't use VP8 or VP9 codecs if others are listed.
							AAMPLOG_INFO("Video Profile ignoring for codec %s resolution= %d:%d display= %d:%d BW=%" BITSPERSECOND_FORMAT, mStreamInfo[pidx].codecs.c_str(), mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, mStreamInfo[pidx].bandwidthBitsPerSecond);
						}
						else
						{
							if (aamp->userProfileStatus || ((mStreamInfo[pidx].bandwidthBitsPerSecond > minBitrate) && (mStreamInfo[pidx].bandwidthBitsPerSecond < maxBitrate)))
							{
								if (aamp->userProfileStatus && false ==  mStreamInfo[pidx].validity)
								{
									AAMPLOG_INFO("Video Profile ignoring user profile range BW=%" BITSPERSECOND_FORMAT, mStreamInfo[pidx].bandwidthBitsPerSecond);
									continue;
								}
								else if (false == aamp->userProfileStatus && ISCONFIGSET(eAAMPConfig_Disable4K) &&
									(mStreamInfo[pidx].resolution.height > 1080 || mStreamInfo[pidx].resolution.width > 1920))
								{
									continue;
								}
								GetABRManager().addProfile({
									mStreamInfo[pidx].isIframeTrack,
									mStreamInfo[pidx].bandwidthBitsPerSecond,
									mStreamInfo[pidx].resolution.width,
									mStreamInfo[pidx].resolution.height,
									"",
									pidx
								});

								mProfileMaps[addedProfiles].adaptationSetIndex = iProfileMaps[pidx].adaptationSetIndex;
								mProfileMaps[addedProfiles].representationIndex = iProfileMaps[pidx].representationIndex;
								addedProfiles++;
								if (resolutionCheckEnabled &&
									((mStreamInfo[pidx].isIframeTrack && bIframeCapped) ||
									(!mStreamInfo[pidx].isIframeTrack && bVideoCapped)))
								{
									aamp->mProfileCappedStatus = true;
								}

								//Update profile resolution with VideoEnd Metrics object.
								aamp->UpdateVideoEndProfileResolution(
									(mStreamInfo[pidx].isIframeTrack ? eMEDIATYPE_IFRAME : eMEDIATYPE_VIDEO ),
								mStreamInfo[pidx].bandwidthBitsPerSecond,
								mStreamInfo[pidx].resolution.width,
								mStreamInfo[pidx].resolution.height);
								if(mStreamInfo[pidx].resolution.height > 1080
									|| mStreamInfo[pidx].resolution.width > 1920)
								{
									defaultBitrate = aamp->GetDefaultBitrate4K();
									iframeBitrate  = aamp->GetIframeBitrate4K();
								}
								mStreamInfo[pidx].enabled = true;
								AAMPLOG_INFO("Added Video Profile to ABR BW= %" BITSPERSECOND_FORMAT" res= %d:%d display= %d:%d pc:%d", mStreamInfo[pidx].bandwidthBitsPerSecond, mStreamInfo[pidx].resolution.width, mStreamInfo[pidx].resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, aamp->mProfileCappedStatus);
							}
						}
					}
					if(adaptationSets.size() > 0)
					{
						// Skipping blacklist check for audio only track at the moment
						IAdaptationSet* adaptationSet = adaptationSets.at(0);
						if ((mNumberOfTracks == 1) && (mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_AUDIO)))
						{
							const auto &representations = adaptationSet->GetRepresentation();
							for (int reprIdx = 0; reprIdx < representations.size(); reprIdx++)
							{
								IRepresentation *representation = representations.at(reprIdx);
								mStreamInfo[idx].bandwidthBitsPerSecond = representation->GetBandwidth();
								mStreamInfo[idx].isIframeTrack = false;
								mStreamInfo[idx].resolution.height = 0;
								mStreamInfo[idx].resolution.width = 0;
								mStreamInfo[idx].resolution.framerate = 0;
								mStreamInfo[idx].enabled = true;
								std::string repFrameRate = representation->GetFrameRate();

								if(repFrameRate.empty())
									repFrameRate = adapFrameRate;
								if(!repFrameRate.empty())
								{
									int val1, val2;
									sscanf(repFrameRate.c_str(),"%d/%d",&val1,&val2);
									double frate = val2? ((double)val1/val2):val1;
									mStreamInfo[idx].resolution.framerate = frate;
								}
								GetABRManager().addProfile({
									mStreamInfo[idx].isIframeTrack,
									mStreamInfo[idx].bandwidthBitsPerSecond,
									mStreamInfo[idx].resolution.width,
									mStreamInfo[idx].resolution.height,
									"",
									(int)idx
								});
								addedProfiles++;
								// Map profile index to corresponding adaptationSet and representation index
								mProfileMaps[idx].adaptationSetIndex = 0;
								mProfileMaps[idx].representationIndex = reprIdx;
								idx++;
							}
						}
					}
					if (0 == addedProfiles)
					{
						ret = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
						AAMPLOG_WARN("No profiles found, minBitrate : %ld maxBitrate: %ld", minBitrate, maxBitrate);
						return ret;
					}
					if (modifyDefaultBW)
					{	// Not for new tune ( for Seek / Trickplay)
						long persistedBandwidth = aamp->GetPersistedBandwidth();
						// If Bitrate persisted over trickplay is true, set persisted BW as default init BW
						if (persistedBandwidth > 0 && (persistedBandwidth < defaultBitrate || aamp->IsBitRatePersistedOverSeek()))
						{
							defaultBitrate = persistedBandwidth;
						}
					}
					else
					{
						// For NewTune
						// Set Default init bitrate according to last PersistBandwidth
						if((ISCONFIGSET(eAAMPConfig_PersistLowNetworkBandwidth)|| ISCONFIGSET(eAAMPConfig_PersistHighNetworkBandwidth)) && !aamp->IsFogTSBSupported())
						{
							long persistbandwidth = aamp->mhAbrManager.getPersistBandwidth();
							long TimeGap   =  aamp_GetCurrentTimeMS() - ABRManager::mPersistBandwidthUpdatedTime;
							//If current Network bandwidth is lower than current default bitrate ,use persistbw as default bandwidth when peristLowNetworkConfig exist
							if(ISCONFIGSET(eAAMPConfig_PersistLowNetworkBandwidth) && TimeGap < 10000 &&  persistbandwidth < aamp->GetDefaultBitrate() && persistbandwidth > 0)
							{
								AAMPLOG_WARN("PersistBitrate used as defaultBitrate. PersistBandwidth : %ld TimeGap : %ld",persistbandwidth,TimeGap);
								aamp->mhAbrManager.setDefaultInitBitrate(persistbandwidth);
							}
							//If current Network bandwidth is higher than current default bitrate and if config for PersistHighBandwidth is true , then network bandwidth will be applied as default bitrate for tune
							else if(ISCONFIGSET(eAAMPConfig_PersistHighNetworkBandwidth) && TimeGap < 10000 && persistbandwidth > 0)
							{
								AAMPLOG_WARN("PersistBitrate used as defaultBitrate. PersistBandwidth : %ld TimeGap : %ld",persistbandwidth,TimeGap);
								aamp->mhAbrManager.setDefaultInitBitrate(persistbandwidth);
							}
							// Set default init bitrate
							else
							{
								AAMPLOG_WARN("Using defaultBitrate %" BITSPERSECOND_FORMAT " . PersistBandwidth : %ld TimeGap : %ld",aamp->GetDefaultBitrate(),persistbandwidth,TimeGap);
								aamp->mhAbrManager.setDefaultInitBitrate(aamp->GetDefaultBitrate());

							}
						}
					}
				}

				if (defaultBitrate != aamp->GetDefaultBitrate())
				{
					GetABRManager().setDefaultInitBitrate(defaultBitrate);
				}
				if( aamp->mCMCDCollector )
				{
					aamp->mCMCDCollector->SetBitrates(eMEDIATYPE_VIDEO, GetVideoBitrates());
				}
			}

			if(-1 == pMediaStreamContext->representationIndex)
			{
				if(!isFogTsb)
				{
					if(i == eMEDIATYPE_VIDEO)
					{
						if(UseIframeTrack())
						{
							if (iframeBitrate > 0)
							{
								GetABRManager().setDefaultIframeBitrate(iframeBitrate);
							}
							UpdateIframeTracks();
						}
						currentProfileIndex = GetDesiredProfile(false);
						// Adaptation Set Index corresponding to a particular profile
						pMediaStreamContext->adaptationSetIdx = mProfileMaps[currentProfileIndex].adaptationSetIndex;
						// Representation Index within a particular Adaptation Set
						pMediaStreamContext->representationIndex  = mProfileMaps[currentProfileIndex].representationIndex;
						pMediaStreamContext->adaptationSet = GetAdaptationSetAtIndex(pMediaStreamContext->adaptationSetIdx);
						pMediaStreamContext->adaptationSetId = pMediaStreamContext->adaptationSet->GetId();
						IRepresentation *selectedRepresentation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);
						// for the profile selected ,reset the abr values with default bandwidth values
						aamp->ResetCurrentlyAvailableBandwidth(selectedRepresentation->GetBandwidth(),trickplayMode,currentProfileIndex);
						aamp->profiler.SetBandwidthBitsPerSecondVideo(selectedRepresentation->GetBandwidth());
					}
					else
					{
						pMediaStreamContext->representationIndex = (int)( pMediaStreamContext->adaptationSet->GetRepresentation().size() / 2); //Select the medium profile on start
						if(i == eMEDIATYPE_AUDIO)
						{
							IRepresentation *selectedRepresentation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);
							aamp->profiler.SetBandwidthBitsPerSecondAudio(selectedRepresentation->GetBandwidth());
						}
					}
				}
				else
				{
					AAMPLOG_WARN("[WARN] !! representationIndex is '-1' override with '0' since Custom MPD has single representation");
					pMediaStreamContext->representationIndex = 0; //Fog custom mpd has single representation
				}
			}
					  //The logic is added to avoid a crash in AAMP due to stream issue in HEVC stream.
					  //Player will be able to end the playback gracefully with the fix.
			if(pMediaStreamContext->representationIndex < pMediaStreamContext->adaptationSet->GetRepresentation().size())
			{
				pMediaStreamContext->representation = pMediaStreamContext->adaptationSet->GetRepresentation().at(pMediaStreamContext->representationIndex);
			}
			else
			{
				AAMPLOG_WARN("Not able to find representation from manifest, sending error event");
				aamp->SendErrorEvent(AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR);
				return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			}

			// Only process content protection when there is a period change.
			// On manifest refresh, the content protection is already processed during period change
			if (eMEDIATYPE_VIDEO == i && mMultiVideoAdaptationPresent && periodChanged)
			{
				// Process content protection for the selected video and queue remaining CPs
				ProcessAllContentProtectionForMediaType(eMEDIATYPE_VIDEO, pMediaStreamContext->adaptationSetIdx, chosenAdaptationIdxs);
			}
			pMediaStreamContext->fragmentDescriptor.ClearMatchingBaseUrl();
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &mpd->GetBaseUrls() );
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &period->GetBaseURLs() );
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &pMediaStreamContext->adaptationSet->GetBaseURLs() );
			pMediaStreamContext->fragmentDescriptor.AppendMatchingBaseUrl( &pMediaStreamContext->representation->GetBaseURLs() );

			pMediaStreamContext->fragmentIndex = 0;

			pMediaStreamContext->fragmentRepeatCount = 0;
			pMediaStreamContext->fragmentOffset = 0;
			pMediaStreamContext->periodStartOffset = pMediaStreamContext->fragmentTime;
			if(0 == pMediaStreamContext->fragmentDescriptor.Bandwidth || !aamp->IsFogTSBSupported())
			{
				pMediaStreamContext->fragmentDescriptor.Bandwidth = pMediaStreamContext->representation->GetBandwidth();
			}
			pMediaStreamContext->fragmentDescriptor.RepresentationID.assign(pMediaStreamContext->representation->GetId());
			pMediaStreamContext->fragmentDescriptor.Time = 0;
			pMediaStreamContext->eos = false;
			pMediaStreamContext->mReachedFirstFragOnRewind = false;
			if(resetTimeLineIndex)
			{
				pMediaStreamContext->timeLineIndex = 0;
			}

			if(periodChanged)
			{
				//update period start and endtimes as period has changed.
				mPeriodEndTime = mMPDParseHelper->GetPeriodEndTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
				mPeriodStartTime = mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs);
				mPeriodDuration = mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
				aamp->mNextPeriodDuration = mPeriodDuration;
				aamp->mNextPeriodStartTime = mPeriodStartTime;
				pMediaStreamContext->fragmentTime = mPeriodStartTime;
				// For playing an ad in a ad break, we should update fragmentTime to PeriodStartTime + basePeriodOffset of ad;
				if (mCdaiObject->mAdState == AdState::IN_ADBREAK_AD_PLAYING && mCdaiObject->mCurAdIdx > 0 )
				{
					// Make sure basePeriodOffset is updated
					if (mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).basePeriodOffset != -1)
					{
						//Set the period start back to the beginning of the base period and then add basePeriodOffset
						//to get the start for this AD
						double absoluteAdBreakStartTime = mCdaiObject->mAdBreaks[mBasePeriodId].mAbsoluteAdBreakStartTime.inSeconds();
						// convert to seconds, standard implicit conversion
						pMediaStreamContext->fragmentTime = absoluteAdBreakStartTime + mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).basePeriodOffset / 1000.0;

						AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Track %d Period changed, but within an adbreak, mPeriodStartTime:%lf basePeriodOffset:%d FragmentTime: %lf mAbsoluteAdBreakStartTime %f",
							i, mPeriodStartTime, mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).basePeriodOffset, pMediaStreamContext->fragmentTime,
							absoluteAdBreakStartTime);
					}
				}
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Track %d Period changed, updating fragmentTime to %lf", i, pMediaStreamContext->fragmentTime);
			}

			SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),pMediaStreamContext->adaptationSet->GetSegmentTemplate());
			if( segmentTemplates.HasSegmentTemplate())
			{
				// In SegmentTemplate case, configure mFirstPTS as per PTO
				// mFirstPTS is used during Flush() for configuring gst_element_seek start position
				const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
				long int startNumber = segmentTemplates.GetStartNumber();
				double fragmentDuration = 0;
				bool timelineAvailable = true;
				if (periodChanged)
				{
					pMediaStreamContext->fragmentDescriptor.TimeScale = segmentTemplates.GetTimescale();
				}
				if(NULL == segmentTimeline)
				{
					uint32_t timeScale = segmentTemplates.GetTimescale();
					uint32_t duration = segmentTemplates.GetDuration();
					fragmentDuration =  ComputeFragmentDuration(duration,timeScale);
					timelineAvailable = false;

					if( timeScale )
					{
						pMediaStreamContext->scaledPTO = (double)segmentTemplates.GetPresentationTimeOffset() / (double)timeScale;
					}
					if(!aamp->IsLive())
					{
						if(segmentTemplates.GetPresentationTimeOffset())
						{
							uint64_t ptoFragmentNumber = 0;
							ptoFragmentNumber = (pMediaStreamContext->scaledPTO / fragmentDuration) + 1;
							AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD: Track %d startnumber:%ld PTO specific fragment Number : %" PRIu64 , i,startNumber, ptoFragmentNumber);
							if(ptoFragmentNumber > startNumber)
							{
								startNumber = ptoFragmentNumber;
							}
						}

						if(i == eMEDIATYPE_VIDEO)
						{
							mFirstPTS = pMediaStreamContext->scaledPTO;

							if(periodChanged)
							{
								aamp->mNextPeriodScaledPtoStartTime = pMediaStreamContext->scaledPTO;
								AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD: Track %d Set mNextPeriodScaledPtoStartTime:%lf",i,aamp->mNextPeriodScaledPtoStartTime);
							}
							AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD: Track %d Set mFirstPTS:%lf",i,mFirstPTS);
							AAMPLOG_DEBUG("PTO= %" PRIu64 " tScale= %u", segmentTemplates.GetPresentationTimeOffset(), timeScale );
						}
					}
				}
				pMediaStreamContext->fragmentDescriptor.Number = startNumber;
				bool liveSync = false;
				if (mMPDParseHelper->GetLiveTimeFragmentSync() && !timelineAvailable)
				{
					pMediaStreamContext->fragmentDescriptor.Number += (long)((mMPDParseHelper->GetPeriodStartTime(0,mLastPlaylistDownloadTimeMs) - mAvailabilityStartTime) / fragmentDuration);
					// For LiveTimeSync cases, availability timeOffset and periodstart is same. So take first fragment time based on fragment number.
					liveSync = true;
				}
				if(aamp->mFirstFragmentTimeOffset < 0 && mLowLatencyMode)
				{
					aamp->mFirstFragmentTimeOffset = liveSync? (((double)(pMediaStreamContext->fragmentDescriptor.Number - startNumber)  * fragmentDuration) + mAvailabilityStartTime)  : mFirstPeriodStartTime;
					AAMPLOG_INFO("mFirstFragmentTimeOffset:%lf mProgressReportOffset:%lf", aamp->mFirstFragmentTimeOffset, aamp->mProgressReportOffset);
				}
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Track %d timeLineIndex %d fragmentDescriptor.Number %" PRIu64 " mFirstPTS:%lf mPTSOffset:%lf", i, pMediaStreamContext->timeLineIndex, pMediaStreamContext->fragmentDescriptor.Number, mFirstPTS, mPTSOffset.inSeconds());
			}
			else
			{
				ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
				if ((segmentBase) && (segmentBase->GetTimescale()))
				{
					uint32_t timeScale = segmentBase->GetTimescale();
					if (periodChanged)
					{
						pMediaStreamContext->fragmentDescriptor.TimeScale = timeScale;
					}
					if( timeScale )
					{
						pMediaStreamContext->scaledPTO = static_cast<double>(segmentBase->GetPresentationTimeOffset()) / static_cast<double>(timeScale);
					}
					if(!aamp->IsLive())
					{
						if(i == eMEDIATYPE_VIDEO)
						{
							mFirstPTS = pMediaStreamContext->scaledPTO;

							if(periodChanged)
							{
								aamp->mNextPeriodScaledPtoStartTime = pMediaStreamContext->scaledPTO;
							}
						}
					}
					AAMPLOG_INFO("SegmentBase: Track %d timescale %u mFirstPTS %f PTO %" PRIu64, i, timeScale, mFirstPTS, segmentBase->GetPresentationTimeOffset());
				}
			}
		}
	}
	return ret;
}

/**
 *   @brief  Get timescale from current period
 *   @retval timescale
 */
uint32_t StreamAbstractionAAMP_MPD::GetCurrPeriodTimeScale()
{
	IPeriod *currPeriod = mCurrentPeriod;
	if(!currPeriod)
	{
		AAMPLOG_WARN("currPeriod is null");  //CID:80891 - Null Returns
		return 0;
	}

	uint32_t timeScale = 0;
	timeScale = mMPDParseHelper->GetPeriodSegmentTimeScale(currPeriod);
	return timeScale;
}

dash::mpd::IMPD *StreamAbstractionAAMP_MPD::GetMPD( void )
{
	return mpd;
}

IPeriod *StreamAbstractionAAMP_MPD::GetPeriod( void )
{
	return mpd->GetPeriods().at(mCurrentPeriodIdx);
}

/**
 * @brief get the first valid (non-empty) period from the current mpd period details
 */
PeriodInfo StreamAbstractionAAMP_MPD::GetFirstValidCurrMPDPeriod(std::vector<PeriodInfo> currMPDPeriodDetails)
{
	/* The culled value of a particular period id is getting missed to add
	 * if the latest manifest refresh resulted in an MPD which contain the same period id with duration of 0 seconds.
	 * The fix is added to skip those empty periods and mark the first non-empty period as current MPD period start for culled value calculation
	 */
	PeriodInfo validPeriod;
	if(currMPDPeriodDetails.size() == 0)
	{
		AAMPLOG_WARN("No period found in the MPD");
	}
	else
	{
		validPeriod = currMPDPeriodDetails[0];
		for(const auto& iter : currMPDPeriodDetails)
		{
			if(iter.duration > 0)
			{
				validPeriod = iter;
				break;
			}
		}
	}
	return validPeriod;
}

/**
 * @brief Update culling state for live manifests
 */
double StreamAbstractionAAMP_MPD::GetCulledSeconds(std::vector<PeriodInfo> &currMPDPeriodDetails)
{
	double newStartTimeSeconds = 0;
	double culled = 0;
	MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_VIDEO];
	if (pMediaStreamContext->adaptationSet)
	{
		SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
					pMediaStreamContext->adaptationSet->GetSegmentTemplate());
		const ISegmentTimeline *segmentTimeline = NULL;
		if(segmentTemplates.HasSegmentTemplate())
		{
			segmentTimeline = segmentTemplates.GetSegmentTimeline();
			if (segmentTimeline)
			{
				int iter1 = 0;
				PeriodInfo currFirstPeriodInfo = GetFirstValidCurrMPDPeriod(currMPDPeriodDetails);
				while (iter1 < aamp->mMPDPeriodsInfo.size())
				{
					PeriodInfo prevPeriodInfo = aamp->mMPDPeriodsInfo.at(iter1);
					if(prevPeriodInfo.periodId == currFirstPeriodInfo.periodId)
					{
						/* Update culled seconds if startTime of existing periods changes
						 * and exceeds previous start time. Updates culled value for ad periods
						 * with startTime = 0 on next refresh if fragments are removed.
						 */
						if(currFirstPeriodInfo.startTime > prevPeriodInfo.startTime)
						{
							uint64_t timeDiff = currFirstPeriodInfo.startTime - prevPeriodInfo.startTime;
							culled += ((double)timeDiff / (double)prevPeriodInfo.timeScale);
							AAMPLOG_INFO("PeriodId %s, prevStart %" PRIu64 " currStart %" PRIu64 " culled %f",
												prevPeriodInfo.periodId.c_str(), prevPeriodInfo.startTime, currFirstPeriodInfo.startTime, culled);
						}
						break;
					}
					else
					{
						double deltaStartTime = currFirstPeriodInfo.periodStartTime - prevPeriodInfo.periodStartTime;
						if (prevPeriodInfo.duration <= deltaStartTime)
						{
							culled += (prevPeriodInfo.duration / 1000);
						}
						else
						{
							culled += deltaStartTime;
						}
						iter1++;
						AAMPLOG_WARN("PeriodId %s , with last known duration %f seems to have got culled",
									 prevPeriodInfo.periodId.c_str(), (prevPeriodInfo.duration / 1000));
					}
				}
				aamp->mMPDPeriodsInfo = currMPDPeriodDetails;
			}
			else
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: NULL segmentTimeline. Hence modifying culling logic based on MPD availabilityStartTime, periodStartTime, fragment number and current time");
				double newStartSegment = 0;
				ISegmentTemplate *firstSegTemplate = NULL;
				int iter1 = 0;
				PeriodInfo currFirstPeriodInfo = currMPDPeriodDetails.at(0);

				// Recalculate the new start fragment after periodic manifest updates
				auto periods = mpd->GetPeriods();
				for (auto period : periods)
				{
					currMPDPeriodDetails.at(iter1).periodStartTime = mMPDParseHelper->GetPeriodStartTime(iter1,mLastPlaylistDownloadTimeMs);
					auto adaptationSets = period->GetAdaptationSets();
					for(auto adaptation : adaptationSets)
					{
						auto segTemplate = adaptation->GetSegmentTemplate();
						if(!segTemplate && adaptation->GetRepresentation().size() > 0)
						{
							segTemplate = adaptation->GetRepresentation().at(0)->GetSegmentTemplate();
						}

						if(segTemplate)
						{
							firstSegTemplate = segTemplate;
							break;
						}
					}
					if(firstSegTemplate)
					{
						break;
					}
					iter1++;
				}

				if(firstSegTemplate)
				{
					newStartSegment = (double)firstSegTemplate->GetStartNumber();
					if(segmentTemplates.GetTimescale() != 0)
					{
						double fragmentDuration = ((double)segmentTemplates.GetDuration()) / segmentTemplates.GetTimescale();
						if (mMPDParseHelper->GetLiveTimeFragmentSync())
						{
							 newStartSegment += (long)((mMPDParseHelper->GetPeriodStartTime(0,mLastPlaylistDownloadTimeMs) - mAvailabilityStartTime) / fragmentDuration);
						}
						if (newStartSegment && mPrevStartTimeSeconds)
						{
							culled = (newStartSegment - mPrevStartTimeSeconds) * fragmentDuration;
							AAMPLOG_TRACE("StreamAbstractionAAMP_MPD:: post-refresh %fs before %f (%f)",  newStartTimeSeconds, mPrevStartTimeSeconds, culled);
						}
						else
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_MPD: newStartTimeSeconds %f mPrevStartTimeSeconds %F", newStartSegment, mPrevStartTimeSeconds);
						}
					}  //CID:163916 - divide by zero
					if(newStartSegment && mPrevStartTimeSeconds && (mPrevStartTimeSeconds > newStartSegment))
					{
						culled = currFirstPeriodInfo.periodStartTime - aamp->mMPDPeriodsInfo.at(0).periodStartTime;
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD:: discontinuity post-refresh %fs before %f (%f)" ,newStartTimeSeconds, mPrevStartTimeSeconds, culled);
					}
					else
					{
						mPrevStartTimeSeconds = newStartSegment;
					}
				}
				aamp->mMPDPeriodsInfo = currMPDPeriodDetails;
			}
		}
		else
		{
			ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
			if (segmentList)
			{
				std::map<string,string> rawAttributes = segmentList->GetRawAttributes();
				if(rawAttributes.find("customlist") != rawAttributes.end())
				{
					//Updated logic for culling,
					vector<IPeriod*> periods =  mpd->GetPeriods();
					long duration = 0;
					long prevLastSegUrlOffset = 0;
					long newOffset = 0;
					bool offsetFound = false;
					std::string newMedia;
					for(int iPeriod = (int)periods.size() - 1 ; iPeriod >= 0; iPeriod--)
					{
						IPeriod* period = periods.at(iPeriod);
						vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
						if (adaptationSets.empty())
						{
							continue;
						}
						IAdaptationSet * adaptationSet = adaptationSets.at(0);
						if(adaptationSet == NULL)
						{
							AAMPLOG_WARN("adaptationSet is null");  //CID:80968 - Null Returns
							return culled;
						}
						vector<IRepresentation *> representations = adaptationSet->GetRepresentation();


						if(representations.empty())
						{
							continue;
						}

						IRepresentation* representation = representations.at(0);
						ISegmentList *segmentList = representation->GetSegmentList();

						if(!segmentList)
						{
							continue;
						}

						duration += segmentList->GetDuration();
						vector<ISegmentURL*> segUrls = segmentList->GetSegmentURLs();
						if(!segUrls.empty())
						{
							for(int iSegurl = (int)segUrls.size() - 1; iSegurl >= 0 && !offsetFound; iSegurl--)
							{
								std::string media = segUrls.at(iSegurl)->GetMediaURI();
								std::string offsetStr = segUrls.at(iSegurl)->GetRawAttributes().at("d");
								uint32_t offset = (uint32_t)stol(offsetStr);
								if(0 == newOffset)
								{
									newOffset = offset;
									newMedia = media;
								}
								if(0 == mPrevLastSegurlOffset && !offsetFound)
								{
									offsetFound = true;
									break;
								}
								else if(mPrevLastSegurlMedia == media)
								{
									offsetFound = true;
									prevLastSegUrlOffset += offset;
									break;
								}
								else
								{
									prevLastSegUrlOffset += offset;
								}
							}//End of segurl for loop
						}
					} //End of Period for loop
					long offsetDiff = 0;
					long currOffset = duration - prevLastSegUrlOffset;
					if(mPrevLastSegurlOffset)
					{
						long timescale = segmentList->GetTimescale();
						offsetDiff = mPrevLastSegurlOffset - currOffset;
						if(offsetDiff > 0)
						{
							culled = (double)offsetDiff / timescale;
						}
					}
					AAMPLOG_INFO("StreamAbstractionAAMP_MPD: PrevOffset %ld CurrentOffset %ld culled (%f)", mPrevLastSegurlOffset, currOffset, culled);
					mPrevLastSegurlOffset = duration - newOffset;
					mPrevLastSegurlMedia = newMedia;
				}
			}
			else
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_MPD: NULL segmentTemplate and segmentList");
			}
		}
	}
	else
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_MPD: NULL adaptationSet");
	}
	return culled;
}

/**
 * @brief Update culled and duration value from periodinfo
 */
void StreamAbstractionAAMP_MPD::UpdateCulledAndDurationFromPeriodInfo(std::vector<PeriodInfo> &currMPDPeriodDetails)
{
	IPeriod* firstPeriod = NULL;
	unsigned firstPeriodIdx = 0;
	unsigned numPeriods	=	 (unsigned)mMPDParseHelper->GetNumberOfPeriods();
	for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
	{
		if(!mMPDParseHelper->IsEmptyPeriod((int)iPeriod, (rate != AAMP_NORMAL_PLAY_RATE)))
		{
			firstPeriod = mpd->GetPeriods().at(iPeriod);
			firstPeriodIdx = iPeriod;
			break;
		}
	}
	if(firstPeriod)
	{
		int lastPeriodIdx = (int)numPeriods - 1;
		for(int iPeriod = (int)numPeriods - 1 ; iPeriod >= 0; iPeriod--)
		{
			if(mMPDParseHelper->IsEmptyPeriod((int)iPeriod, (rate != AAMP_NORMAL_PLAY_RATE)))
			{
				// Empty Period . Ignore processing, continue to next.
				continue;
			}
			else
			{
				lastPeriodIdx = iPeriod;
				break;
			}
		}
		double firstPeriodStart = mMPDParseHelper->GetPeriodStartTime(firstPeriodIdx,mLastPlaylistDownloadTimeMs);
		double availStartTime = mMPDParseHelper->GetAvailabilityStartTime();
		if( ( mUpdateManifestState == true )|| (true == aamp->IsIVODContent() && ( firstPeriodStart != mPrevFirstPeriodStart && availStartTime <=0 )) )
		{

			/*
				IVOD trick play not working after
				asset license expiry time.
				When License get expired the absolute timeline changes to
				different value from manifest. In order to maintain
				continuity of playback when trick play is performed
				it is required to reset the timeline values based on
				new value received from manifest
			*/
			aamp->seek_pos_seconds -= aamp->culledSeconds;
			aamp->prevFirstPeriodStartTime = (aamp->culledSeconds*1000);
			aamp->culledSeconds = firstPeriodStart;
			aamp->seek_pos_seconds += aamp->culledSeconds;
			mCulledSeconds = aamp->culledSeconds;
			aamp->mAbsoluteEndPosition = aamp->culledSeconds;
			seekPosition = aamp->seek_pos_seconds;
			aamp->mProgressReportOffset = -1;
			aamp->mPrevPositionMilliseconds.Invalidate();

			AAMPLOG_WARN("firstPeriodStart: %lf aamp->culledSeconds: %lf aamp->seek_pos_seconds: %lf aamp->mAbsoluteEndPosition: %lf seekPosition: %lf mCulledSeconds: %lf", firstPeriodStart, aamp->culledSeconds, aamp->seek_pos_seconds, aamp->mAbsoluteEndPosition, seekPosition, mCulledSeconds);
		}
		double lastPeriodStart = 0;
		if (firstPeriodIdx == lastPeriodIdx)
		{
			lastPeriodStart = firstPeriodStart;
			if(!mIsLiveManifest && mIsLiveStream)
			{
				lastPeriodStart = aamp->culledSeconds;
			}
		}
		else
		{
			// Expecting no delta between final period PTSOffset and segment start if
			// first and last periods are different.
			lastPeriodStart = mMPDParseHelper->GetPeriodStartTime(lastPeriodIdx,mLastPlaylistDownloadTimeMs);
		}
		double culled = firstPeriodStart - aamp->culledSeconds;
		if (culled != 0 && mIsLiveManifest)
		{
			if(!aamp->IsLocalAAMPTsb())
			{
				aamp->culledSeconds = firstPeriodStart;
			}
			mCulledSeconds = firstPeriodStart;
		}
		
		aamp->mAbsoluteEndPosition = lastPeriodStart + (mMPDParseHelper->GetPeriodDuration(lastPeriodIdx,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB()) / 1000.00);
		
		if(aamp->mAbsoluteEndPosition < aamp->culledSeconds)
		{
			// Handling edge case just before dynamic => static transition.
			// This is an edge case where manifest is still dynamic,
			// Start time remains the same, but PTS offset is reset to segment start time.
			// Updating duration as culled + total duration as manifest is changing to VOD
			AAMPLOG_WARN("Duration is less than culled seconds, updating it wrt actual fragment duration");
			aamp->mAbsoluteEndPosition = 0;
			int iter = 0;
			while (iter < aamp->mMPDPeriodsInfo.size())
			{
				PeriodInfo periodInfo = currMPDPeriodDetails.at(iter);
				if(!mMPDParseHelper->IsEmptyPeriod(iter, (rate != AAMP_NORMAL_PLAY_RATE)))
				{
					aamp->mAbsoluteEndPosition += (periodInfo.duration / 1000.0);
				}
				iter++;
			}
			aamp->mAbsoluteEndPosition += aamp->culledSeconds;
		}
		if(mLowLatencyMode)
		{
			// Logging the stream issue if the latency adjusted end position is less than publish time.
			double latencyAdjustedEndPosition = aamp->mAbsoluteEndPosition + GETCONFIGVALUE(eAAMPConfig_LLTargetLatency);
			if(latencyAdjustedEndPosition < mMPDParseHelper->GetPublishTime())
			{
				AAMPLOG_ERR("latencyAdjustedEnd %lf < publishTime %lf, Bug in the stream!!", aamp->mAbsoluteEndPosition, mMPDParseHelper->GetPublishTime());
			}
		}

		mPrevFirstPeriodStart = firstPeriodStart;
		AAMPLOG_INFO("Culled seconds: %f, Updated culledSeconds: %lf AbsoluteEndPosition: %lf PrevFirstPeriodStart: %lf", culled, mCulledSeconds, aamp->mAbsoluteEndPosition, mPrevFirstPeriodStart);
	}
}

/**
 * @brief Fetch and inject initialization fragment for all available tracks
 */
void StreamAbstractionAAMP_MPD::FetchAndInjectInitFragments(bool discontinuity)
{
	for( int i = 0; i < mNumberOfTracks; i++)
	{
		if (i < mTrackWorkers.size() && ISCONFIGSET(eAAMPConfig_DashParallelFragDownload) && mTrackWorkers[i])
		{
			// Download the video, audio & subtitle init fragments in a separate parallel thread.
			AAMPLOG_DEBUG("Submitting init job for track %d", i);
			mTrackWorkers[i]->SubmitJob([this, i, discontinuity]() { FetchAndInjectInitialization(i,discontinuity); });
		}
		else
		{
			AAMPLOG_INFO("Track %d worker not available, downloading init fragment sequentially", i);
			FetchAndInjectInitialization(i,discontinuity);
		}
	}

	for (int trackIdx = (mNumberOfTracks - 1); (ISCONFIGSET(eAAMPConfig_DashParallelFragDownload) && trackIdx >= 0); trackIdx--)
	{
		if(trackIdx < mTrackWorkers.size() && mTrackWorkers[trackIdx])
		{
			mTrackWorkers[trackIdx]->WaitForCompletion();
		}
	}
}

/**
 * @brief Fetch and inject initialization fragment for media type
 */
void StreamAbstractionAAMP_MPD::FetchAndInjectInitialization(int trackIdx, bool discontinuity)
{
		class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[trackIdx];

		if(discontinuity && pMediaStreamContext->enabled)
		{
			pMediaStreamContext->discontinuity = discontinuity;
		}
		if(pMediaStreamContext->enabled && (pMediaStreamContext->profileChanged || pMediaStreamContext->discontinuity))
		{
			if (pMediaStreamContext->adaptationSet)
			{
				SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
								pMediaStreamContext->adaptationSet->GetSegmentTemplate() );
				if( segmentTemplates.HasSegmentTemplate() )
				{
					std::string initialization = segmentTemplates.Getinitialization();
					if (!initialization.empty())
					{
						std::string media = segmentTemplates.Getmedia();
						pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time;
						pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number;
						if(!mIsFogTSB)
						{
							setNextobjectrequestUrl(media,&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
						}
						pMediaStreamContext->fragmentDescriptor.nextfragmentNum = pMediaStreamContext->fragmentDescriptor.Number+1;
						TrackDownloader(trackIdx, initialization);
					}
					else
					{
						AAMPLOG_WARN("initialization  is null");  //CID:84853 ,86291- Null Return
						pMediaStreamContext->profileChanged = false;
					}
				}
				else
				{
					ISegmentBase *segmentBase = pMediaStreamContext->representation->GetSegmentBase();
					if (segmentBase)
					{
						pMediaStreamContext->fragmentOffset = 0;
						pMediaStreamContext->IDX.Free();
						std::string range;
						std::string nextrange; //CMCD get the next range
						const IURLType *urlType = segmentBase->GetInitialization();
						if (urlType)
						{
							range = urlType->GetRange();
							nextrange =segmentBase->GetIndexRange();
						}
						else
						{
							range = segmentBase->GetIndexRange();
							uint64_t s1,s2;
							sscanf(range.c_str(), "%" PRIu64 "-%" PRIu64 "", &s1,&s2);
							char temp[MAX_RANGE_STRING_CHARS];
							snprintf( temp, sizeof(temp), "0-%" PRIu64 , s1-1 );
							range = temp;
							if (pMediaStreamContext->IDX.GetPtr())
							{
								unsigned int referenced_size;
								float fragmentDuration;
								if (ParseSegmentIndexBox(
										pMediaStreamContext->IDX.GetPtr(),
										pMediaStreamContext->IDX.GetLen(),
										pMediaStreamContext->fragmentIndex,
										&referenced_size,
										&fragmentDuration,
										NULL))
								{
									char temprange[MAX_RANGE_STRING_CHARS];
									snprintf(temprange, sizeof(temprange), "%" PRIu64 "-%" PRIu64 "", pMediaStreamContext->fragmentOffset, pMediaStreamContext->fragmentOffset + referenced_size - 1);
									nextrange = temprange;
								}
							}
						}
						std::string fragmentUrl;
						ConstructFragmentURL(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");

						if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
						{
							pMediaStreamContext->profileChanged = false;
							if(!nextrange.empty())
							{
								setNextRangeRequest(fragmentUrl,nextrange,(&pMediaStreamContext->fragmentDescriptor)->Bandwidth,AampMediaType(pMediaStreamContext->type));
							}
							if(!pMediaStreamContext->CacheFragment(fragmentUrl,
								getCurlInstanceByMediaType(pMediaStreamContext->mediaType),
								pMediaStreamContext->fragmentTime,
								0, // duration - zero for init fragment
								range.c_str(), true ))
							{
								AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
							}
						}
					}
					else
					{
						ISegmentList *segmentList = pMediaStreamContext->representation->GetSegmentList();
						if (segmentList)
						{
							const IURLType *urlType = segmentList->GetInitialization();
							if( !urlType )
							{
								segmentList = pMediaStreamContext->adaptationSet->GetSegmentList();
								urlType = segmentList->GetInitialization();
								if( !urlType )
								{
									AAMPLOG_WARN("initialization is null");
									return;
								}
							}
							std::string initialization = urlType->GetSourceURL();
							if (!initialization.empty())
							{
								const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
								ISegmentURL* nextsegmentURL = segmentURLs.at(pMediaStreamContext->fragmentIndex);
								pMediaStreamContext->fragmentDescriptor.nextfragmentTime = pMediaStreamContext->fragmentDescriptor.Time;
								if(nextsegmentURL != NULL && (mIsFogTSB != true))
								{
									setNextobjectrequestUrl(nextsegmentURL->GetMediaURI(),&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
								}
								TrackDownloader(trackIdx, initialization);
							}
							else
							{
								string range;
												string nextrange;
#ifdef LIBDASH_SEGMENTLIST_GET_INIT_SUPPORT
								const ISegmentURL *segmentURL = NULL;
								segmentURL = segmentList->Getinitialization();

								if (segmentURL)
								{
									range = segmentURL->GetMediaRange();
								}
#else
								const std::vector<ISegmentURL*> segmentURLs = segmentList->GetSegmentURLs();
								ISegmentURL *nextsegurl = segmentURLs.at(pMediaStreamContext->fragmentIndex);
								if (segmentURLs.size() > 0)
								{
									ISegmentURL *firstSegmentURL = segmentURLs.at(0);
									int start, fin;
									if(firstSegmentURL != NULL)
									{
										const char *firstSegmentRange = firstSegmentURL->GetMediaRange().c_str();
										AAMPLOG_INFO("firstSegmentRange %s [%s]",
												GetMediaTypeName(pMediaStreamContext->mediaType), firstSegmentRange);
										if (sscanf(firstSegmentRange, "%d-%d", &start, &fin) == 2)
										{
											if (start > 1)
											{
												char range_c[MAX_RANGE_STRING_CHARS];
												snprintf(range_c, sizeof(range_c), "%d-%d", 0, start - 1);
												range = range_c;
											}
											else
											{
												AAMPLOG_WARN("StreamAbstractionAAMP_MPD: segmentList - cannot determine range for Initialization - first segment start %d",
														start);
											}
										}
									}
									else
									{
										AAMPLOG_WARN("firstSegmentURL is null");  //CID:80649 - Null Returns
									}
								}
#endif
								if (!range.empty())
								{
									std::string fragmentUrl;
									ConstructFragmentURL(fragmentUrl, &pMediaStreamContext->fragmentDescriptor, "");

									AAMPLOG_INFO("%s [%s]", GetMediaTypeName(pMediaStreamContext->mediaType),
											range.c_str());

									if(pMediaStreamContext->WaitForFreeFragmentAvailable(0))
									{
										pMediaStreamContext->profileChanged = false;
										if(nextsegurl != NULL && (mIsFogTSB != true))
										{
											setNextobjectrequestUrl(nextsegurl->GetMediaURI(),&pMediaStreamContext->fragmentDescriptor,AampMediaType(pMediaStreamContext->type));
										}
										if(!pMediaStreamContext->CacheFragment(fragmentUrl,
												getCurlInstanceByMediaType(pMediaStreamContext->mediaType),
												pMediaStreamContext->fragmentTime,
												0.0, // duration - zero for init fragment
												range.c_str(),
												true ))
										{
											AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", fragmentUrl.c_str(), pMediaStreamContext->fragmentTime);
										}
									}
								}
								else
								{
									AAMPLOG_WARN("StreamAbstractionAAMP_MPD: segmentList - empty range string for Initialization");
								}
							}
						}
						else
						{
							if( pMediaStreamContext->mediaType == eMEDIATYPE_SUBTITLE )
							{
								TrackDownloader(trackIdx,"");// BaseUrl used for WebVTT download
							}
							else
							{ // note: this risks flooding logs, as will get called repeatedly
								AAMPLOG_ERR("not-yet-supported mpd format");
							}
						}
					}
				}
			}
		}
}


/**
 * @brief Check if current period is clear
 * @retval true if clear period
 */
bool StreamAbstractionAAMP_MPD::CheckForInitalClearPeriod()
{
	bool ret = true;
	for(int i = 0; i < mNumberOfTracks; i++)
	{
		auto adaptSet = mMediaStreamContext[i]->adaptationSet;
		if( adaptSet )
		{
			vector<IDescriptor*> contentProt = mMPDParseHelper->GetContentProtection(adaptSet);
			if(0 != contentProt.size())
			{
				ret = false;
				break;
			}
		}
	}
	if(ret)
	{
		AAMPLOG_WARN("Initial period is clear period, trying work around");
	}
	return ret;
}

/**
 * @brief Push encrypted headers if available
 * return true for a successful encrypted init fragment push
 */
void StreamAbstractionAAMP_MPD::PushEncryptedHeaders(std::map<int, std::string>& mappedHeaders)
{
	for (std::map<int, std::string>::iterator it=mappedHeaders.begin(); it!=mappedHeaders.end(); ++it)
	{
		if (it->first < mTrackWorkers.size() && ISCONFIGSET(eAAMPConfig_DashParallelFragDownload) && mTrackWorkers[it->first])
		{
			// Download the video, audio & subtitle fragments in a separate parallel thread.
			AAMPLOG_DEBUG("Submitting job for init encrypted header track %d", it->first);
			auto track = it->first;
			auto header = it->second;
			mTrackWorkers[it->first]->SubmitJob([this, track, header]() { CacheEncryptedHeader(track, header); });
		}
		else
		{
			AAMPLOG_INFO("Track %d worker not available, caching init encrypted header sequentially", it->first);
			CacheEncryptedHeader(it->first, it->second);
		}
	}

	for (int trackIdx = (mNumberOfTracks - 1); (ISCONFIGSET(eAAMPConfig_DashParallelFragDownload) && trackIdx >= 0); trackIdx--)
	{
		if(trackIdx < mTrackWorkers.size() && mTrackWorkers[trackIdx])
		{
			mTrackWorkers[trackIdx]->WaitForCompletion();
		}
	}
}

void StreamAbstractionAAMP_MPD::CacheEncryptedHeader(int trackIdx, std::string headerUrl)
{
	//update the next segment for download
	//aamp->mCMCDCollector->CMCDSetNextObjectRequest( fragmentUrl , (long long)fragmentDescriptor->Number,
	//		fragmentDescriptor->Bandwidth,(AampMediaType)i);
	if (mMediaStreamContext[trackIdx]->WaitForFreeFragmentAvailable())
	{
		AAMPLOG_WARN("Pushing encrypted header for %s fragmentUrl %s", GetMediaTypeName(AampMediaType(trackIdx)), headerUrl.c_str());
		//Set the last parameter (overWriteTrackId) true to overwrite the track id if ad and content has different track ids
		bool temp = false;
		try
		{
			temp =  mMediaStreamContext[trackIdx]->CacheFragment(headerUrl, (eCURLINSTANCE_VIDEO + mMediaStreamContext[trackIdx]->mediaType), mMediaStreamContext[trackIdx]->fragmentTime, 0.0, NULL, true, false, false, 0, 0, true);
		}
		catch(const std::regex_error& e)
		{
			AAMPLOG_ERR("regex exception in Calling CacheFragment: %s", e.what());
		}
		if(!temp)
		{
			AAMPLOG_TRACE("StreamAbstractionAAMP_MPD: did not cache fragmentUrl %s fragmentTime %f", headerUrl.c_str(), mMediaStreamContext[trackIdx]->fragmentTime); //CID:84438 - checked return
		}
	}
}

/**
 * @brief Push encrypted headers if available
 * return true for a successful encrypted init fragment push
 */
bool StreamAbstractionAAMP_MPD::GetEncryptedHeaders(std::map<int, std::string>& mappedHeaders)
{
	bool ret = false;
	//Find the first period with contentProtection
	size_t numPeriods = mMPDParseHelper->GetNumberOfPeriods();  //CID:96576 - Removed the  headerCount variable which is initialized but not used
	for(int i = mNumberOfTracks - 1; i >= 0; i--)
	{
		bool encryptionFound = false;
		unsigned iPeriod = 0;
		while(iPeriod < numPeriods && !encryptionFound)
		{
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(period != NULL)
			{
				size_t numAdaptationSets = period->GetAdaptationSets().size();
				for(unsigned iAdaptationSet = 0; iAdaptationSet < numAdaptationSets && !encryptionFound; iAdaptationSet++)
				{
					IAdaptationSet *adaptationSet = period->GetAdaptationSets().at(iAdaptationSet);
					if(adaptationSet != NULL)
					{
						if (mMPDParseHelper->IsContentType(adaptationSet, (AampMediaType)i ))
						{
							vector<IDescriptor*> contentProt = mMPDParseHelper->GetContentProtection(adaptationSet);
							if(0 == contentProt.size())
							{
								continue;
							}
							else
							{
								IRepresentation *representation = NULL;
								size_t representationIndex = 0;
								if(AampMediaType(i) == eMEDIATYPE_VIDEO)
								{
									size_t representationCount = adaptationSet->GetRepresentation().size();
									if(adaptationSet->GetRepresentation().at(representationIndex)->GetBandwidth() > adaptationSet->GetRepresentation().at(representationCount - 1)->GetBandwidth())
									{
										representationIndex = representationCount - 1;
									}
								}
								else if (mAudioType != eAUDIO_UNKNOWN)
								{
									AudioType selectedAudioType = eAUDIO_UNKNOWN;
									uint32_t selectedRepBandwidth = 0;
									bool disableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
									// if EC3 disabled, implicitly disable ATMOS
									bool disableATMOS = (disableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);
									bool disableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3);
									bool disableAC4 = ISCONFIGSET(eAAMPConfig_DisableAC4);
									bool disabled = false;
									representationIndex = GetDesiredCodecIndex(adaptationSet, selectedAudioType, selectedRepBandwidth,disableEC3 , disableATMOS, disableAC4, disableAC3, disabled);
									if(selectedAudioType != mAudioType)
									{
										continue;
									}
									AAMPLOG_WARN("Audio type %d", selectedAudioType);
								}
								else
								{
									AAMPLOG_WARN("Audio type eAUDIO_UNKNOWN");
								}
								representation = adaptationSet->GetRepresentation().at(representationIndex);

								SegmentTemplates segmentTemplates(representation->GetSegmentTemplate(), adaptationSet->GetSegmentTemplate());
								if(segmentTemplates.HasSegmentTemplate())
								{
									std::string initialization = segmentTemplates.Getinitialization();
									if (!initialization.empty())
									{
										std::string fragmentUrl;
										FragmentDescriptor *fragmentDescriptor = new FragmentDescriptor();

										fragmentDescriptor->bUseMatchingBaseUrl	=	ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
										if (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState)
										{
											// For AD playback, replace with source manifest url, as we are parsing source manifest for init header
											fragmentDescriptor->manifestUrl = aamp->GetManifestUrl();
										}
										else
										{
											fragmentDescriptor->manifestUrl = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.manifestUrl;
										}

										QueueContentProtection(period, iAdaptationSet, (AampMediaType)i);

										fragmentDescriptor->Bandwidth = representation->GetBandwidth();

										fragmentDescriptor->ClearMatchingBaseUrl();
										fragmentDescriptor->AppendMatchingBaseUrl(&mpd->GetBaseUrls());
										fragmentDescriptor->AppendMatchingBaseUrl(&period->GetBaseURLs());
										fragmentDescriptor->AppendMatchingBaseUrl(&adaptationSet->GetBaseURLs());
										fragmentDescriptor->AppendMatchingBaseUrl(&representation->GetBaseURLs());

										fragmentDescriptor->RepresentationID.assign(representation->GetId());
										 FragmentDescriptor *fragmentDescriptorCMCD(fragmentDescriptor);
										ConstructFragmentURL(fragmentUrl,fragmentDescriptorCMCD , initialization);

										mappedHeaders[i] = fragmentUrl;

										SAFE_DELETE(fragmentDescriptor);
										ret = true;
										encryptionFound = true;
									}
								}
							}
						}
					}
					else
					{
						AAMPLOG_WARN("adaptationSet is null");  //CID:84361 - Null Returns
					}
				}
			}
			else
			{
				AAMPLOG_WARN("period    is null");  //CID:86137 - Null Returns
			}
			iPeriod++;
		}
	}
	return ret;
}

/**
 * @brief Find subtitle adaptationSet if available. This function assumes that
 * all subtitle adaptation sets use the same mimeType/codec and there are
 * no significant differences in the header fragments, so it looks only for the
 * first, not the one that gets selected according to configured language
 * or other criteria.
 * return true for a successful subtitle media header push
 */
bool StreamAbstractionAAMP_MPD::ExtractAndAddSubtitleMediaHeader()
{
	bool ret = false;
	bool subtitleFound = false;

	for (auto &period: mpd->GetPeriods())
	{
		if(period != nullptr)
		{
			for(auto &adaptationSet: period->GetAdaptationSets())
			{
				if(adaptationSet != nullptr)
				{
					if (mMPDParseHelper->IsContentType(adaptationSet, eMEDIATYPE_SUBTITLE ))
					{
						size_t representationIndex = 0;
						PeriodElement periodElement(adaptationSet, NULL);
						std::string subtitleMimeType = periodElement.GetMimeType();

						IRepresentation *representation = adaptationSet->GetRepresentation().at(representationIndex);
						SegmentTemplates segmentTemplates(representation->GetSegmentTemplate(), adaptationSet->GetSegmentTemplate());
						if( subtitleMimeType.empty() )
						{
							AAMPLOG_MIL("eMEDIATYPE_SUBTITLE:subtitleMimeType is empty. Try getting it from representation");
							PeriodElement periodElement(adaptationSet, representation);
							subtitleMimeType = periodElement.GetMimeType();
						}
						AAMPLOG_MIL("eMEDIATYPE_SUBTITLE:subtitleMimeType = %s", subtitleMimeType.c_str());

						if(segmentTemplates.HasSegmentTemplate())
						{
							std::string initialization = segmentTemplates.Getinitialization();
							if (!initialization.empty())
							{
								std::string fragmentUrl;
								FragmentDescriptor *fragmentDescriptor = new FragmentDescriptor();
								auto subtitleHeader = std::make_shared<AampStreamSinkManager::MediaHeader>();

								fragmentDescriptor->bUseMatchingBaseUrl = ISCONFIGSET(eAAMPConfig_MatchBaseUrl);
								fragmentDescriptor->manifestUrl = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.manifestUrl;
								fragmentDescriptor->Bandwidth = representation->GetBandwidth();
								fragmentDescriptor->ClearMatchingBaseUrl();
								fragmentDescriptor->AppendMatchingBaseUrl(&mpd->GetBaseUrls());
								fragmentDescriptor->AppendMatchingBaseUrl(&period->GetBaseURLs());
								fragmentDescriptor->AppendMatchingBaseUrl(&adaptationSet->GetBaseURLs());
								fragmentDescriptor->AppendMatchingBaseUrl(&representation->GetBaseURLs());
								fragmentDescriptor->RepresentationID.assign(representation->GetId());
								FragmentDescriptor *fragmentDescriptorCMCD(fragmentDescriptor);

								ConstructFragmentURL(fragmentUrl,fragmentDescriptorCMCD , std::move(initialization) );
								AAMPLOG_MIL("[SUBTITLE]: mimeType:%s, init url %s", subtitleMimeType.c_str(), fragmentUrl.c_str());
								subtitleHeader->url = std::move(fragmentUrl);
								subtitleHeader->mimeType =  std::move(subtitleMimeType);
								AampStreamSinkManager::GetInstance().AddMediaHeader(eMEDIATYPE_SUBTITLE, std::move(subtitleHeader));
								AAMPLOG_MIL("Saved subtitleHeader");
								ret = true;
								SAFE_DELETE(fragmentDescriptor);
								subtitleFound = true;
								break;
							}
						}
					}
				}
				else
				{
					AAMPLOG_WARN("adaptationSet is null");  //CID:84361 - Null Returns
				}
			}
		}
		else
		{
			AAMPLOG_WARN("period    is null");  //CID:86137 - Null Returns
		}
		if(subtitleFound)
		{
			break;
		}
	}
	return ret;
}


/**
 * @brief Fetches and caches audio fragment parallelly for video fragment.
 */
void StreamAbstractionAAMP_MPD::AdvanceTrack(int trackIdx, bool trickPlay, double *delta, bool &waitForFreeFrag, bool &bCacheFullState,bool throttleAudioDownload,bool isDiscontinuity)
{
	UsingPlayerId playerId(aamp->mPlayerId);
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[trackIdx];
	bool lowLatency = aamp->GetLLDashServiceData()->lowLatencyMode;
	bool isAllowNextFrag = true;
	int  vodTrickplayFPS = GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS);

	AAMPLOG_TRACE("trackIdx %d, trickPlay %d, delta %p, waitForFreeFrag %d, bCacheFullState %d, throttleAudioDownload %d, isDiscontinuity %d",
			trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState, throttleAudioDownload, isDiscontinuity);

	if (waitForFreeFrag && !trickPlay)
	{
		AAMPPlayerState state = aamp->GetState();
		if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
		{
			state = eSTATE_PLAYING;
		}
		if(state == eSTATE_PLAYING)
		{
			waitForFreeFrag = false;
		}
		else
		{
			int timeoutMs = -1;
			if (bCacheFullState && pMediaStreamContext->IsFragmentCacheFull())
			{
				timeoutMs = MAX_WAIT_TIMEOUT_MS;
			}
			isAllowNextFrag = pMediaStreamContext->WaitForFreeFragmentAvailable(timeoutMs);
		}
	}

	if (isAllowNextFrag)
	{
		if (pMediaStreamContext->adaptationSet )
		{
			bool profileNotChanged = !pMediaStreamContext->profileChanged;
			bool isTsbInjection = aamp->IsLocalAAMPTsbInjection();
			bool cacheNotFull = !pMediaStreamContext->IsFragmentCacheFull();
			bool isTrackDownloadEnabled = aamp->TrackDownloadsAreEnabled(static_cast<AampMediaType>(trackIdx));

			/*
			* When injecting from TSBReader we do not want to stop the fetcher loop because of injector cache full. TSB injection
			* uses numberOfFragmentChunksCached so assuming (pMediaStreamContext->numberOfFragmentsCached != maxCachedFragmentsPerTrack) == true
			*
			* Also aamp->IsLocalAAMPTsbInjection() || aamp->TrackDownloadsAreEnabled(static_cast<AampMediaType>(trackIdx) because a pause in playback
			* should not stop the fetcher loop during TSB injection.
			*/

			if(profileNotChanged && (isTsbInjection || (cacheNotFull && (!lowLatency || isTrackDownloadEnabled))))
			{
				// profile not changed and Cache not full scenario
				if (!pMediaStreamContext->eos)
				{
					if(trickPlay && pMediaStreamContext->mDownloadedFragment.GetPtr() == NULL && !pMediaStreamContext->freshManifest)
					{
						double skipTime = 0;
						if (delta)
						{
							skipTime = *delta;
						}
						//When player started in trickplay rate during player switching, make sure that we are showing at least one frame (mainly to avoid cases where trickplay rate is so high that an ad could get skipped completely)
						//TODO: Check for this condition?? delta is always zero from FetcherLoop
						if(aamp->playerStartedWithTrickPlay)
						{
							AAMPLOG_WARN("Played switched in trickplay, delta set to zero");
							skipTime = 0;
							aamp->playerStartedWithTrickPlay = false;
						}
						else if((rate > 0 && skipTime <= 0) || (rate < 0 && skipTime >= 0))
						{
							skipTime = rate / vodTrickplayFPS;
						}
						double currFragTime = pMediaStreamContext->fragmentTime;
						skipTime = SkipFragments(pMediaStreamContext, skipTime);
						if( delta )
						{
							if (pMediaStreamContext->eos)
							{
								// If we reached end of period, only the remaining delta should be skipped in new period
								// Otherwise we should skip based on formula rate/fps. This will also avoid any issues due to floating precision
								*delta = skipTime;
							}
							else
							{
								*delta = 0;
							}
						}
						mBasePeriodOffset += (pMediaStreamContext->fragmentTime - currFragTime);
					}

					if (PushNextFragment(pMediaStreamContext, getCurlInstanceByMediaType(static_cast<AampMediaType>(trackIdx))))
					{
						if (mIsLiveManifest)
						{
							pMediaStreamContext->GetContext()->CheckForPlaybackStall(true);
						}
						if((!pMediaStreamContext->GetContext()->trickplayMode) && (eMEDIATYPE_VIDEO == trackIdx)&& !pMediaStreamContext->failAdjacentSegment)
						{
							if (aamp->CheckABREnabled())
							{
								pMediaStreamContext->GetContext()->CheckForProfileChange();
							}
						}
					}
					else if (pMediaStreamContext->eos == true && mIsLiveManifest && trackIdx == eMEDIATYPE_VIDEO && !(ISCONFIGSET(eAAMPConfig_InterruptHandling) && mIsFogTSB))
					{
						pMediaStreamContext->GetContext()->CheckForPlaybackStall(false);
					}

					//Determining the current position within the period by calculating the difference between
					//the fragmentTime and the periodStartOffset (both in absolute terms).
					//If this difference exceeds the total duration of the ad, the period is considered to have ended.
					if (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState && rate > 0 && !(pMediaStreamContext->eos)&& mCdaiObject->CheckForAdTerminate(pMediaStreamContext->fragmentTime - pMediaStreamContext->periodStartOffset))
					{
						//Ensuring that Ad playback doesn't go beyond Adbreak
						AAMPLOG_WARN("[CDAI] Track[%d] Adbreak ended early. Terminating Ad playback. fragmentTime[%lf] periodStartOffset[%lf]",
											trackIdx, pMediaStreamContext->fragmentTime, pMediaStreamContext->periodStartOffset);
						pMediaStreamContext->eos = true;
					}
				}
				else
				{
					AAMPLOG_TRACE("Track %s is EOS, not pushing next fragment", GetMediaTypeName((AampMediaType) trackIdx));
				}
			}
			// Fetch init header for both audio and video ,after mpd refresh(stream selection) , profileChanged = true for both tracks .
			// Need to reset profileChanged flag which is done inside FetchAndInjectInitialization
			// Without resetting profileChanged flag , fetch of audio was stopped causing audio drop
			else if(pMediaStreamContext->profileChanged)
			{	// Profile changed case
				FetchAndInjectInitialization(trackIdx,isDiscontinuity);
			}

			if ((isTsbInjection || (!pMediaStreamContext->IsFragmentCacheFull())) &&
				bCacheFullState && (!lowLatency || aamp->TrackDownloadsAreEnabled(static_cast<AampMediaType>(trackIdx))))
			{
				bCacheFullState = false;
			}
		}
		else
		{
			AAMPLOG_ERR("AdaptationSet is NULL for %s", GetMediaTypeName((AampMediaType) trackIdx));
		}
	}
	else
	{
		std::lock_guard<std::mutex> lock(mutex);
		// Important DEBUG area, live downloader is delayed due to some external factors (Injector or Gstreamer)
		if (pMediaStreamContext->IsInjectionFromCachedFragmentChunks())
		{
			AAMPLOG_ERR("%s Live downloader is not advancing at the moment cache (%d / %d)", GetMediaTypeName((AampMediaType)trackIdx), pMediaStreamContext->numberOfFragmentChunksCached, pMediaStreamContext->maxCachedFragmentChunksPerTrack);
		}
		else
		{
			AAMPLOG_ERR("%s Live downloader is not advancing at the moment cache (%d / %d)", GetMediaTypeName((AampMediaType)trackIdx), pMediaStreamContext->numberOfFragmentsCached, pMediaStreamContext->maxCachedFragmentsPerTrack);
		}
	}
	// If throttle audio download is set and prev fragment download happened and cache is not full, attempt to download an additional fragment
	if (throttleAudioDownload && (trackIdx == eMEDIATYPE_AUDIO) && isAllowNextFrag && !bCacheFullState)
	{
		AAMPLOG_INFO("throttleAudioDownload enabled, invoking AdvanceTrack again");
		// Disable throttleAudioDownload this time to prevent continuous looping here
		AdvanceTrack(trackIdx, trickPlay, delta, waitForFreeFrag, bCacheFullState, false);
	}
}

void StreamAbstractionAAMP_MPD::GetStartAndDurationForPtsRestamping(AampTime &start, AampTime &duration)
{
	AampTime videoStart {0};
	AampTime audioStart {0};
	AampTime audioDuration {0};
	AampTime videoDuration {0};

	IPeriod *period = mCurrentPeriod;

	if (mMediaStreamContext[eMEDIATYPE_AUDIO])
	{
		mMPDParseHelper->GetStartAndDurationFromTimeline(period, mMediaStreamContext[eMEDIATYPE_AUDIO]->representationIndex,
														 mMediaStreamContext[eMEDIATYPE_AUDIO]->adaptationSetIdx, audioStart, audioDuration);
	}
	else
	{
		AAMPLOG_WARN("Cannot get details for AUDIO");
	}
	if (mMediaStreamContext[eMEDIATYPE_VIDEO])
	{
		mMPDParseHelper->GetStartAndDurationFromTimeline(period, mMediaStreamContext[eMEDIATYPE_VIDEO]->representationIndex,
														 mMediaStreamContext[eMEDIATYPE_VIDEO]->adaptationSetIdx, videoStart, videoDuration);
	}
	else
	{
		AAMPLOG_WARN("Cannot get details for VIDEO");
	}

	start = std::min(audioStart, videoStart);

	AAMPLOG_INFO("Idx %d Id %s aDur %f vDur %f aStart %f vStart %f",
				 mCurrentPeriodIdx, period->GetId().c_str(), audioDuration.inSeconds(), videoDuration.inSeconds(), audioStart.inSeconds(), videoStart.inSeconds());

	/* Should get here last manifest update before a period change
	 * On live manifests the duration of the current period can be increasing as more segments
	 * added to the timeline. We need the final duration of that period
	 */
	if ((audioDuration != 0.0) && (videoDuration != 0.0))
	{
		// for cases where 2 tracks have slightly different durations, take the maximum to avoid injecting overlapping media
		duration = std::max(audioDuration, videoDuration);
		mAudioSurplus = 0;
		mVideoSurplus = 0;
		if(audioStart == videoStart)
		{
			if(audioDuration > videoDuration)
			{
				mAudioSurplus = audioDuration - videoDuration;
				mVideoSurplus = 0;
			}
			else if(videoDuration > audioDuration)
			{
				mVideoSurplus = videoDuration - audioDuration;
				mAudioSurplus = 0;
			}
		}
	}
	else
	{
		// cannot get duration from timeline so use another algorithm
		duration = CONVERT_MS_TO_SEC(mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs, false, aamp->IsUninterruptedTSB()));
		AAMPLOG_INFO("Idx %d Id %s duration %f", mCurrentPeriodIdx, period->GetId().c_str(), duration.inSeconds());
	}
}

/**
 * @brief Calculate PTS offset value at the start of each period.
 */
void StreamAbstractionAAMP_MPD::UpdatePtsOffset(bool isNewPeriod)
{
	AampTime timelineStart;
	AampTime duration;

	// Nothing to do during trick play, so skip code
	if (rate == AAMP_NORMAL_PLAY_RATE)
	{
		IPeriod *period = mCurrentPeriod;
		GetStartAndDurationForPtsRestamping(timelineStart, duration);

		if (isNewPeriod)
		{

			if (mShortAdOffsetCalc)
			{
				/* This is for the case of a short ad that is not as long as the base period which
				 * it replaces. The ad has been 'played' and now we need to return and play out the remaining
				 * segments in the base period
				 */
				mShortAdOffsetCalc = false;
				double audioStart = mMediaStreamContext[eMEDIATYPE_AUDIO]->fragmentDescriptor.Time /
									mMediaStreamContext[eMEDIATYPE_AUDIO]->fragmentDescriptor.TimeScale;
				double videoStart = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.Time /
									mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.TimeScale;

				AampTime newStart = std::max(audioStart, videoStart);

				mNextPts += timelineStart - newStart;
				AAMPLOG_INFO("newStart %f timelineStart %f", newStart.inSeconds(), timelineStart.inSeconds());
			}
			mPTSOffset += mNextPts - timelineStart;

			AAMPLOG_INFO("Idx %d Id %s mPTSOffsetSec %f mNextPts %f timelineStartSec %f",
						mCurrentPeriodIdx, period->GetId().c_str(), mPTSOffset.inSeconds(), mNextPts.inSeconds(), timelineStart.inSeconds());
		}

		mNextPts = duration + timelineStart;

	}
}

void StreamAbstractionAAMP_MPD::RestorePtsOffsetCalculation(void)
{
	AampTime timelineStart;
	AampTime duration;

	// This variable still contains the period of the ad that failed
	IPeriod *period = mCurrentPeriod;
	GetStartAndDurationForPtsRestamping(timelineStart, duration);

	AAMPLOG_INFO("Idx %d Id %s restoring mNextPts from %f to %f",
				mCurrentPeriodIdx, period->GetId().c_str(), mNextPts.inSeconds(), (mNextPts.inSeconds() - duration.inSeconds()));
	mNextPts -= duration;
}

/**
 * @fn CheckEndOfStream
 *
 * @param[in] waitForAdBreakCatchup flag
 * @return bool - true if end of stream reached, false otherwise
 */
bool StreamAbstractionAAMP_MPD::CheckEndOfStream(bool waitForAdBreakCatchup)
{
	bool ret = false;
	if ((rate < AAMP_NORMAL_PLAY_RATE && mIterPeriodIndex < 0) || (rate > 1 && mIterPeriodIndex >= mNumberOfPeriods) || (!mIsLiveManifest && waitForAdBreakCatchup != true))
	{
		// During rewind, due to miscalculations in fragmentTime, we could end up exiting collector loop without pushing EOS
		// For the time being, will check additionally here to push EOS
		if ((rate < AAMP_NORMAL_PLAY_RATE && mIterPeriodIndex < 0) &&
			(!mMediaStreamContext[eMEDIATYPE_VIDEO]->eosReached))
		{
			AAMPLOG_INFO("EOS Reached. rate:%f mIterPeriodIndex:%d", rate, mIterPeriodIndex);
			mMediaStreamContext[eMEDIATYPE_VIDEO]->eosReached = true;
			mMediaStreamContext[eMEDIATYPE_VIDEO]->AbortWaitForCachedAndFreeFragment(false);
		}
		ret = true;
	}
	return ret;
}

/**
 * @fn SelectSourceOrAdPeriod
 *
 * @param[in,out] periodChanged flag
 * @param[in,out] mpdChanged flag
 * @param[in,out] AdStateChanged flag
 * @param[in,out] waitForAdBreakCatchup flag
 * @param[in,out] requireStreamSelection flag
 * @param[in,out] currentPeriodId string
 * @return bool - true if new period selected, false otherwise
 */
bool StreamAbstractionAAMP_MPD::SelectSourceOrAdPeriod(bool &periodChanged, bool &mpdChanged, bool &adStateChanged, bool &waitForAdBreakCatchup, bool &requireStreamSelection, std::string &currentPeriodId)
{
	bool ret = false;
	waitForAdBreakCatchup = false; // reset this flag
	while ((mIterPeriodIndex < mNumberOfPeriods) && (mIterPeriodIndex >= 0) && !ret) // CID:95090 - No effect
	{
		periodChanged = (mIterPeriodIndex != mCurrentPeriodIdx) || (mBasePeriodId != mpd->GetPeriods().at(mCurrentPeriodIdx)->GetId());
		if (periodChanged || adStateChanged || mpdChanged)
		{
			requireStreamSelection = false;
			if (mpdChanged)
			{
				// After waitForAdBreakCatchup, this mpdChanged flag will be set to true.
				// So, we need to check the period change again.
				mpdChanged = false;
			}
			if (periodChanged)
			{
				IPeriod *newPeriod = mpd->GetPeriods().at(mIterPeriodIndex);
				AAMPLOG_MIL("Period(%s - %d/%d) Offset[%lf] IsLive(%d) IsCdvr(%d)", mBasePeriodId.c_str(), mCurrentPeriodIdx, mNumberOfPeriods, mBasePeriodOffset, mIsLiveStream, aamp->IsInProgressCDVR());
				vector<IAdaptationSet *> adaptationSets = newPeriod->GetAdaptationSets();
				int adaptationSetCount = (int)adaptationSets.size();
				// skip tiny periods (used for audio codec changes) as workaround for soc-specific issue
				if (0 == adaptationSetCount || (mMPDParseHelper->IsEmptyPeriod(mIterPeriodIndex, (rate != AAMP_NORMAL_PLAY_RATE))) ||
					(mMPDParseHelper->GetPeriodDuration(mIterPeriodIndex, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB()) < THRESHOLD_TOIGNORE_TINYPERIOD))
				{
					/*To Handle non fog scenarios where empty periods are
					* present after mpd update causing issues
					*/
					AAMPLOG_INFO("Period %s skipped. Adaptation size:%d, isEmpty:%d duration %f (ms)", newPeriod->GetId().c_str(), adaptationSetCount, mMPDParseHelper->IsEmptyPeriod(mIterPeriodIndex, (rate != AAMP_NORMAL_PLAY_RATE)), (mMPDParseHelper->GetPeriodDuration(mIterPeriodIndex, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB())));
					mIterPeriodIndex += (rate < 0) ? -1 : 1;
					// Skipping period completely, so exit from period selection
					ret = false;
					continue;
				}

				// This is bit controversial to simplify setting of mBasePeriodOffset.
				// But on a period change, would expect the periodOffset to align to either start or end of new period
				// This could have some regressions, need to test thoroughly
				mBasePeriodOffset = (rate > AAMP_RATE_PAUSE) ? 0 : (mMPDParseHelper->GetPeriodDuration(mIterPeriodIndex, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB()) / 1000.00);

				// Update period gaps for playback stats
				if (mIsLiveStream)
				{
					double periodGap = (mMPDParseHelper->GetPeriodEndTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB()) - mMPDParseHelper->GetPeriodStartTime(mIterPeriodIndex, mLastPlaylistDownloadTimeMs)) * 1000;
					if (periodGap > 0)
					{
						// for livestream, period gaps are updated as playback progresses through the periods
						aamp->IncrementGaps();
					}
				}
				mCurrentPeriodIdx = mIterPeriodIndex;
				mBasePeriodId = newPeriod->GetId();
				// If the playing period changes, it will be detected below [if(currentPeriodId != mCurrentPeriod->GetId())]
				periodChanged = false;
			}
			// Calling the function to play ads from first ad break(existing logic).
			adStateChanged = onAdEvent(AdEvent::DEFAULT);
			if(adStateChanged && AdState::OUTSIDE_ADBREAK_WAIT4ADS == mCdaiObject->mAdState)
			{
				// Adbreak was available, but ads were not available and waited for fulfillment. Now, check if ads are available.
				adStateChanged = onAdEvent(AdEvent::DEFAULT);
			}
			// endPeriod for the ad break is not available, so wait for the ad break to complete
			if (AdState::IN_ADBREAK_WAIT2CATCHUP == mCdaiObject->mAdState)
			{
				waitForAdBreakCatchup = true;
				ret = false;
				break;
			}

			// If adStateChanged is true with OUTSIDE_ADBREAK, it means the current ad break playback completed.
			// We need to check if the new period is also having ads.
			if (adStateChanged && AdState::OUTSIDE_ADBREAK == mCdaiObject->mAdState)
			{
				for (mIterPeriodIndex = 0; mIterPeriodIndex < mNumberOfPeriods; mIterPeriodIndex++)
				{
					if (mBasePeriodId == mpd->GetPeriods().at(mIterPeriodIndex)->GetId())
					{
						mCurrentPeriodIdx = GetValidPeriodIdx(mIterPeriodIndex);
						mIterPeriodIndex = mCurrentPeriodIdx;
						mBasePeriodId = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetId();
						if (mMPDParseHelper->IsEmptyPeriod(mCurrentPeriodIdx, (rate != AAMP_NORMAL_PLAY_RATE)))
						{
							AAMPLOG_WARN("Empty period(%s) at the end of manifest BasePeriodId (%s)", mpd->GetPeriods().at(mCurrentPeriodIdx)->GetId().c_str(), mpd->GetPeriods().at(mIterPeriodIndex)->GetId().c_str());
						}
						break;
					}
				}
				// Set the period offset to the boundaries of new period after ad playback
				mBasePeriodOffset = (rate > AAMP_RATE_PAUSE) ? 0 : (mMPDParseHelper->GetPeriodDuration(mIterPeriodIndex, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB()) / 1000.00);

				{
					std::lock_guard<std::mutex> lock(mCdaiObject->mDaiMtx);
					if (mCdaiObject->mContentSeekOffset > 0)
					{
						// Set mBasePeriodOffset as mContentSeekOffset to handle cases of partial ads.
						// mContentSeekOffset will be endPeriodOffset for partial ads and ensures the same ad is not selected again.
						mBasePeriodOffset = mCdaiObject->mContentSeekOffset;
					}
				}
				// Check if the new period is having ads
				adStateChanged = onAdEvent(AdEvent::DEFAULT);
				if(adStateChanged && AdState::OUTSIDE_ADBREAK_WAIT4ADS == mCdaiObject->mAdState)
				{
					// Adbreak was available, but ads were not available and waited for fulfillment. Now, check if ads are available.
					adStateChanged = onAdEvent(AdEvent::DEFAULT);
				}
			}

			if (AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState)
			{
				// Fallback to source period if not playing from ad period
				mCurrentPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);
			}

			vector<IAdaptationSet *> adaptationSets = mCurrentPeriod->GetAdaptationSets();
			int adaptationSetCount = (int)adaptationSets.size();
			if (currentPeriodId != mCurrentPeriod->GetId())
			{
				// If not: playing from aamp tsb, or paused on live before entering tsb
				// Then wait for ad discontinuity to be processed by stream injection before continuing
				if (aamp->GetIsPeriodChangeMarked() &&
					!mMediaStreamContext[eMEDIATYPE_VIDEO]->IsLocalTSBInjection() &&
					!(aamp->IsLocalAAMPTsb() && aamp->pipeline_paused))
				{
					aamp->WaitForDiscontinuityProcessToComplete();
				}

				/*If next period is empty, period ID change is not processed.
				Will check the period change for the same period in the next iteration.*/
				if ((adaptationSetCount > 0 || !(mMPDParseHelper->IsEmptyPeriod(mCurrentPeriodIdx, (rate != AAMP_NORMAL_PLAY_RATE)))) && (mMPDParseHelper->GetPeriodDuration(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB()) >= THRESHOLD_TOIGNORE_TINYPERIOD))
				{
					AAMPLOG_MIL("Period ID changed from \'%s\' to \'%s\' [BasePeriodId=\'%s\']", currentPeriodId.c_str(), mCurrentPeriod->GetId().c_str(), mBasePeriodId.c_str());
					currentPeriodId = mCurrentPeriod->GetId();
					mPrevAdaptationSetCount = adaptationSetCount;
					periodChanged = true;
					if ((rate == AAMP_NORMAL_PLAY_RATE) &&
						!mMediaStreamContext[eMEDIATYPE_VIDEO]->IsLocalTSBInjection() &&
						!(aamp->IsLocalAAMPTsb() && aamp->pipeline_paused))
					{
						aamp->SetIsPeriodChangeMarked(true);
					}
					requireStreamSelection = true;
					AAMPLOG_MIL("playing period %d/%d", mIterPeriodIndex, (int)mNumberOfPeriods);
					if (rate == AAMP_NORMAL_PLAY_RATE)
					{
						if(mAudioSurplus != 0 )
						{
							MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
							audio->UpdateInjectedDuration((double)mAudioSurplus);
							mAudioSurplus = 0;
						}else if ( mVideoSurplus != 0 )
						{
							MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
							video->UpdateInjectedDuration((double)mVideoSurplus);
							mVideoSurplus = 0;
						}

					}
					// If DAI ad is available for the period and we are still not in AD playing state, log a warning
					if ((mCdaiObject->HasDaiAd(mBasePeriodId)) && (AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState) && (mBasePeriodOffset == 0))
					{
						AAMPLOG_WARN("Ad available but skipped!! periodID %s mAdState %s", mBasePeriodId.c_str(), ADSTATE_STR[static_cast<int>(mCdaiObject->mAdState)]);
					}
				}
				else
				{
					for (int i = 0; i < mNumberOfTracks; i++)
					{
						mMediaStreamContext[i]->enabled = false;
					}
					AAMPLOG_WARN("Period ID not changed from \'%s\' to \'%s\',since period is empty [BasePeriodId=\'%s\'] mIterPeriodIndex[%d] mUpperBoundaryPeriod[%d]", currentPeriodId.c_str(), mCurrentPeriod->GetId().c_str(), mBasePeriodId.c_str(), mIterPeriodIndex, mMPDParseHelper->mUpperBoundaryPeriod);
					if (mIsLiveManifest && (mIterPeriodIndex > mMPDParseHelper->mUpperBoundaryPeriod) && aamp->DownloadsAreEnabled())
					{
						// Update manifest and check for period validity in the next iteration
						// For CDAI empty period at the end, we should re-iterate the loop
						AAMPLOG_WARN("Period ID not changed WaitForManifestUpdate");
						if (AAMPStatusType::eAAMPSTATUS_OK != UpdateMPD())
						{
							aamp->interruptibleMsSleep(500); // Sleep for 500ms to avoid tight looping
						}
						mpdChanged = true;
						ret = false;
						break;
					}
				}

				// We are moving to new period so reset the lastsegment time
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					mMediaStreamContext[i]->lastSegmentTime = 0;
					mMediaStreamContext[i]->lastSegmentDuration = 0;
					mMediaStreamContext[i]->lastSegmentNumber = 0; // looks like change in period may happen now. hence reset lastSegmentNumber
					prevTimeScale = 0;
				}
			}
			else if (mPrevAdaptationSetCount != adaptationSetCount)
			{
				AAMPLOG_WARN("Change in AdaptationSet count; adaptationSetCount %d  mPrevAdaptationSetCount %d,updating stream selection", adaptationSetCount, mPrevAdaptationSetCount);
				mPrevAdaptationSetCount = adaptationSetCount;
				requireStreamSelection = true;
			}
			else
			{
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					if (mMediaStreamContext[i]->adaptationSetId != adaptationSets.at(mMediaStreamContext[i]->adaptationSetIdx)->GetId())
					{
						AAMPLOG_WARN("AdaptationSet index changed; updating stream selection, track = %d", i);
						requireStreamSelection = true;
					}
				}
			}
			adStateChanged = false;
		}
		ret = true;
	}
	return ret;
}

/**
 * @fn IndexSelectedPeriod
 *
 * @param[in] periodChanged flag
 * @param[in] AdStateChanged flag
 * @param[in] requireStreamSelection flag
 * @param[in] currentPeriodId string
 * @return bool - true if new period indexed, false otherwise
 */
bool StreamAbstractionAAMP_MPD::IndexSelectedPeriod(bool periodChanged, bool adStateChanged, bool requireStreamSelection, std::string currentPeriodId)
{
	if (requireStreamSelection)
	{
		StreamSelection();
		mUpdateStreamInfo = true;
	}

	// UpdateTrackInfo from Fetcher thread if there is a periodChange
	// Else this will be called as a part of ProcessPlaylist
	// IsLive(), InProgressCdvr, Vod/CDVR for PeriodChange , resetTimeLineIndex = 1
	// If mUpdateStreamInfo is true, first thread which is reaching UpdateTrackInfo will be executed
	if (mUpdateStreamInfo && periodChanged)
	{
		bool resetTimeLineIndex = (mIsLiveStream || periodChanged);
		// Reset the video skip remainder to zero as this is a new period
		mVideoPosRemainder = 0;
		mIsFinalFirstPTS = false;
		AAMPStatusType ret = UpdateTrackInfo(true, resetTimeLineIndex);
		if (ret != eAAMPSTATUS_OK)
		{
			AAMPLOG_WARN("manifest : %d error", ret);
			aamp->DisableDownloads();
			//Exiting FetchLoop with content error
			return false;
		}
		if (ISCONFIGSET(eAAMPConfig_EnableMediaProcessor))
		{
			// For segment timeline based streams, media processor is initialized in passthrough mode
			InitializeMediaProcessor(mIsSegmentTimelineEnabled);
		}
		if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (rate == AAMP_NORMAL_PLAY_RATE) && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled)
		{
			SetSubtitleTrackOffset();
		}
	}

	if (mIsLiveStream && (periodChanged || adStateChanged))
	{
		double seekPositionSeconds = 0.0;

		// CID:190371 - Data race condition
		// Get and clear mContentSeekOffset atomically.
		std::lock_guard<std::mutex> lock(mCdaiObject->mDaiMtx);
		seekPositionSeconds = mCdaiObject->mContentSeekOffset;
		mCdaiObject->mContentSeekOffset = 0;

		if (seekPositionSeconds)
		{
			AAMPLOG_INFO("[CDAI]: Resuming channel playback at PeriodID[%s] at Position[%lf]", currentPeriodId.c_str(), seekPositionSeconds);
			if (rate > 0)
			{
				SeekInPeriod(seekPositionSeconds);
				// Ad shorter than base period, set flag to adjust calculation on next call to UpdatePtsOffset()
				mShortAdOffsetCalc = true;
				mSeekedInPeriod = true;
			}
		}
	}
	return true;
}

/**
 * @fn DetectDiscontinuityAndFetchInit
 *
 * @param[in] periodChanged flag
 * @param[in] nextSegmentTime
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::DetectDiscontinuityAndFetchInit(bool periodChanged, uint64_t nextSegmentTime)
{
	bool discontinuity = false;

	/*Discontinuity handling on period change*/
	if (periodChanged && ISCONFIGSET(eAAMPConfig_MPDDiscontinuityHandling) && mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled &&
		(ISCONFIGSET(eAAMPConfig_MPDDiscontinuityHandlingCdvr) || (!aamp->IsInProgressCDVR())))
	{
		MediaStreamContext *pMediaStreamContext = mMediaStreamContext[eMEDIATYPE_VIDEO];
		SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
										pMediaStreamContext->adaptationSet->GetSegmentTemplate());
		bool ignoreDiscontinuity = false;
		if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			ignoreDiscontinuity = (mMediaStreamContext[eMEDIATYPE_AUDIO] && !mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled && mMediaStreamContext[eMEDIATYPE_AUDIO]->isFragmentInjectorThreadStarted());
		}

		if (ignoreDiscontinuity)
		{
			AAMPLOG_WARN("Error! Audio or Video track missing in period, ignoring discontinuity");
			aamp->SetIsPeriodChangeMarked(false);
		}
		else
		{
			if (segmentTemplates.HasSegmentTemplate())
			{
				// Trying to maintain parity with GetFirstSegmentStartTime() logic, and get video start time
				uint64_t segmentStartTime = 0;
				bool usingPTO = false;
				const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
				if (segmentTimeline)
				{
					std::vector<ITimeline *> &timelines = segmentTimeline->GetTimelines();
					if (timelines.size() > 0)
					{
						segmentStartTime = timelines.at(0)->GetStartTime();
					}
				}
				uint64_t presentationTimeOffset = segmentTemplates.GetPresentationTimeOffset();
				if (presentationTimeOffset > segmentStartTime)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Presentation Time Offset %" PRIu64 " ahead of segment start Time %" PRIu64 ", Set PTO as segment start", presentationTimeOffset, segmentStartTime);
					segmentStartTime = presentationTimeOffset;
					usingPTO = true;
				}

				/* Process the discontinuity,
				* 1. If the next segment time is not matching with the next period segment start time.
				* 2. To reconfigure the pipeline, if there is a change in the Audio Codec even if there is no change in segment start time in multi period content.
				*/
				if ((segmentTemplates.GetSegmentTimeline() != NULL && nextSegmentTime != segmentStartTime) || GetESChangeStatus() || ISCONFIGSET(eAAMPConfig_ForceMultiPeriodDiscontinuity))
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", nextSegmentTime, segmentStartTime);
					discontinuity = true;
					// mFirstPTS should not be updated if we are coming out of partial ad playback mSeekedInPeriod = true
					if (segmentTemplates.GetTimescale() != 0 && !mSeekedInPeriod)
					{
						mFirstPTS = (double)segmentStartTime / (double)segmentTemplates.GetTimescale();
						mIsFinalFirstPTS = true;
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Not updating mFirstPTS TimeScale(0) or mSeekedInPeriod(%d)", mSeekedInPeriod);
					}
					mSeekedInPeriod = false;
					double startTime = (mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs) - mAvailabilityStartTime);
					if ((startTime != 0) && !aamp->IsUninterruptedTSB())
					{
						mStartTimeOfFirstPTS = mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs) * 1000;
					}
				}
				else if (nextSegmentTime != segmentStartTime || ISCONFIGSET(eAAMPConfig_ForceMultiPeriodDiscontinuity))
				{
					discontinuity = true;
					if (usingPTO)
					{
						mFirstPTS = (double)segmentStartTime / (double)segmentTemplates.GetTimescale();
						mIsFinalFirstPTS = true;
					}
					double startTime = (mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs) - mAvailabilityStartTime);
					if ((startTime != 0) && !mIsFogTSB)
					{
						mStartTimeOfFirstPTS = mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs) * 1000;
					}
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", nextSegmentTime, segmentStartTime);
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_MPD: No discontinuity detected nextSegmentTime %" PRIu64 " FirstSegmentStartTime %" PRIu64 " ", nextSegmentTime, segmentStartTime);
					aamp->SetIsPeriodChangeMarked(false);
				}
				if (rate < 0 || rate > 1)
				{
					discontinuity = true;
					AAMPLOG_INFO("Discontinuity set for trickplay");
				}
			}
			else
			{
				AAMPLOG_TRACE("StreamAbstractionAAMP_MPD:: Segment template not available");
				aamp->SetIsPeriodChangeMarked(false);
			}
		}
	}
	FetchAndInjectInitFragments(discontinuity);
}

/**
 * @brief Fetches and caches fragments in a loop
 */
void StreamAbstractionAAMP_MPD::FetcherLoop()
{
	UsingPlayerId playerId(aamp->mPlayerId);
	aamp_setThreadName("aampFragDL");
	bool exitFetchLoop = false;
	bool trickPlay = (AAMP_NORMAL_PLAY_RATE != aamp->rate);

	//If we are injecting from TSB then we are not injecting from the Fetcher hence
	//the fetcher does not need to wait for a free slot in the fragment cache FIFO
	bool waitForFreeFrag = !aamp->IsLocalAAMPTsbInjection();
	double delta = 0;
	bool adStateChanged = false;
	bool resetIterator = true;
	bool mpdChanged = false;

	MediaStreamContext *playlistDownloaderContext = mMediaStreamContext[eMEDIATYPE_VIDEO];

	IPeriod *currPeriod = mCurrentPeriod;
	if (currPeriod == NULL)
	{
		AAMPLOG_WARN("currPeriod is null"); // CID:80891 - Null Returns
		return;
	}
	std::string currentPeriodId = currPeriod->GetId();
	mPrevAdaptationSetCount = (int)currPeriod->GetAdaptationSets().size();

	/*
	 * Initial indexing without updating trackInfo
	 */
	if (mpd)
	{
		IndexNewMPDDocument(false);
	}

	AAMPLOG_MIL("aamp: ready to read fragments");
	/*
	 * Ready to collect fragments
	 */
	do
	{
		AAMPLOG_INFO("inner loop start");
		bool waitForAdBreakCatchup = false;
		bool periodChanged = false;
		if (mpd)
		{
			/*
			 * Reset iterator after MPD update/ IndexMPDDocument
			 */
			if(resetIterator)
			{
				mIterPeriodIndex = mCurrentPeriodIdx;
			}
			else
			{
				resetIterator = true;
			}

			/*
			 * Select the period from ad or source content
			 */
			bool requireStreamSelection = false;
			if (!aamp->DownloadsAreEnabled())
			{
				break;
			}

			/*
			 * Appropriate error handling if period selection fails
			 */
			if (!SelectSourceOrAdPeriod(periodChanged, mpdChanged, adStateChanged, waitForAdBreakCatchup, requireStreamSelection, currentPeriodId))
			{
				if(waitForAdBreakCatchup)
				{
					if (!exitFetchLoop)
					{
						// Reset period info to go back to loop without UpdateTrackInfo.
						if (eAAMPSTATUS_OK != IndexNewMPDDocument(false))
						{
							aamp->DisableDownloads();
							AAMPLOG_WARN("Exit fetcher loop due to manifest error");
							break;
						}
					}
					AAMPStatusType ret = UpdateMPD();
					if (eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR == ret)
					{
						AAMPLOG_TRACE("Wait for manifest refresh");
						aamp->interruptibleMsSleep(MAX_WAIT_TIMEOUT_MS);
					}
					else if (eAAMPSTATUS_MANIFEST_CONTENT_ERROR == ret)
					{
						aamp->DisableDownloads();
						AAMPLOG_WARN("Exiting from fetcher loop due to manifest content error");
						break;
					}
					mpdChanged = true;
					continue; // Continue iteration with updated MPD
				}
				else if(CheckEndOfStream(waitForAdBreakCatchup))
				{
					break;
				}
				else if(mIterPeriodIndex >= mNumberOfPeriods)
				{
					AAMPStatusType ret = UpdateMPD();
					if (eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR == ret)
					{
						AAMPLOG_TRACE("Wait for manifest refresh");
						aamp->interruptibleMsSleep(MAX_WAIT_TIMEOUT_MS);
					}
					else if (eAAMPSTATUS_MANIFEST_CONTENT_ERROR == ret)
					{
						aamp->DisableDownloads();
						AAMPLOG_WARN("Exiting from fetcher loop due to manifest content error");
						break;
					}
					mpdChanged = true;
					continue;
				}
				else
				{
					// Skip iterator reset and continue with same period
					resetIterator = false;
					continue;
				}
			}

			uint64_t nextSegmentTime = mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentDescriptor.Time;
			/*
			 * Index selected period from ad or source content
			 */
			if(!IndexSelectedPeriod(periodChanged, adStateChanged, requireStreamSelection, currentPeriodId))
			{
				// Indexing failed, break the fetcher loop
				CheckEndOfStream(waitForAdBreakCatchup);
				break;
			}
			else
			{
				// Call after StreamSelection() to get correct ->adaptationSetIdx ->representationIndex
				AAMPLOG_TRACE("Update PTS offset after StreamSelection, period changed %d", periodChanged);
				UpdatePtsOffset(periodChanged);
				// Indexing success case
				DetectDiscontinuityAndFetchInit(periodChanged, nextSegmentTime);
				if (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState)
				{
					if (mCdaiObject->mAdBreaks[mBasePeriodId].mAdFailed)
					{
						// AdEvent::AD_FAILED, continue the iteration
						// Added for init failure cases
						adStateChanged = onAdEvent(AdEvent::AD_FAILED);
						mCdaiObject->mAdBreaks[mBasePeriodId].mAdFailed = false;
						// reset period change flag so that we can perform period change to source period
						aamp->SetIsPeriodChangeMarked(false);
						resetIterator = false;
						if(rate == AAMP_NORMAL_PLAY_RATE)
						{
							RestorePtsOffsetCalculation();
						}
						aamp->UnblockWaitForDiscontinuityProcessToComplete();
						continue;
					}
				}
				if (rate < 0 && periodChanged)
				{
					SeekInPeriod(0, true);
				}
			}

			/*
			 * Segment downloader loop
			 */
			double lastPrdOffset = mBasePeriodOffset;
			bool parallelDnld = ISCONFIGSET(eAAMPConfig_DashParallelFragDownload);
			bool *cacheFullStatus = new bool[AAMP_TRACK_COUNT]{false};
			bool throttleAudio = false;
			while (!exitFetchLoop)
			{
				if (mIsLiveStream && !mIsLiveManifest && playlistDownloaderThreadStarted)
				{
					// CDVR moved from "dynamic" to "static"
					playlistDownloaderContext->StopPlaylistDownloaderThread();
					playlistDownloaderThreadStarted = false;
				}
				/* Calling the Refresh audio track, in order to switch to the newly selected Audio Track */
				if(rate == AAMP_NORMAL_PLAY_RATE && mMediaStreamContext[eTRACK_AUDIO] && mMediaStreamContext[eTRACK_AUDIO]->refreshAudio )
				{
					SwitchAudioTrack();
					mMediaStreamContext[eTRACK_AUDIO]->refreshAudio = false;
					throttleAudio = true;
				}
				if(rate == AAMP_NORMAL_PLAY_RATE && mMediaStreamContext[eTRACK_SUBTITLE] && mMediaStreamContext[eTRACK_SUBTITLE]->refreshSubtitles )
				{
					SwitchSubtitleTrack(true);
					mMediaStreamContext[eTRACK_SUBTITLE]->refreshSubtitles = false;
				}
				for (int trackIdx = (mNumberOfTracks - 1); trackIdx >= 0; trackIdx--)
				{
					// When injecting from TSB reader then fetcher should ignore the cache full status
					cacheFullStatus[trackIdx] = !aamp->IsLocalAAMPTsbInjection();
					if (!mMediaStreamContext[trackIdx]->eos)
					{
						if (parallelDnld && trackIdx < mTrackWorkers.size() && mTrackWorkers[trackIdx])
						{
							// Download the video, audio & subtitle fragments in a separate parallel thread.
							AAMPLOG_DEBUG("Submitting job for track %d", trackIdx);
							mTrackWorkers[trackIdx]->SubmitJob([this, trackIdx, &delta, &waitForFreeFrag, &cacheFullStatus, trickPlay, throttleAudio]()
															   { AdvanceTrack(trackIdx, trickPlay, &delta, waitForFreeFrag, cacheFullStatus[trackIdx],
																			  (trackIdx == eMEDIATYPE_AUDIO) ? throttleAudio : false, false); });
						}
						else
						{
							AdvanceTrack(trackIdx, trickPlay, &delta, waitForFreeFrag, cacheFullStatus[trackIdx], false, isVidDiscInitFragFail);
						}
					}
				}

				for (int trackIdx = (mNumberOfTracks - 1); (parallelDnld && trackIdx >= 0); trackIdx--)
				{
					if(trackIdx < mTrackWorkers.size() && mTrackWorkers[trackIdx])
					{
						mTrackWorkers[trackIdx]->WaitForCompletion();
					}
				}

				// If download status is disabled then need to exit from fetcher loop
				if (!aamp->DownloadsAreEnabled())
				{
					AAMPLOG_INFO("Downloads are disabled, so exit FetcherLoop");
					exitFetchLoop = true;
					cacheFullStatus[eMEDIATYPE_VIDEO] = cacheFullStatus[eMEDIATYPE_AUDIO] = false;
				}

				//   -- Exit from fetch loop for period to be done only after audio and video fetch
				// While playing CDVR with EAC3 audio, durations don't match and only video downloads are seen leaving audio behind
				// Audio cache is always full and need for data is not received for more fetch.
				// So after video downloads loop was exiting without audio fetch causing audio drop .
				// Now wait for both video and audio to reach EOS before moving to next period or exit.
				if (eAAMPSTATUS_MANIFEST_CONTENT_ERROR == UpdateMPD())
				{
					aamp->DisableDownloads();
					AAMPLOG_WARN("Exiting from fetcher loop due to manifest content error");
					break;
				}
				bool vEos = mMediaStreamContext[eMEDIATYPE_VIDEO]->eos;
				bool audioEnabled = (mMediaStreamContext[eMEDIATYPE_AUDIO] && mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled);
				bool aEos = (audioEnabled && mMediaStreamContext[eMEDIATYPE_AUDIO]->eos);
				if (vEos || aEos)
				{
					bool eosOutSideAd = (AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState &&
											((rate > 0 && mCurrentPeriodIdx >= mMPDParseHelper->mUpperBoundaryPeriod) || (rate < 0 && mMPDParseHelper->mLowerBoundaryPeriod == mCurrentPeriodIdx)));

					bool eosAdPlayback = (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState &&
											((rate > 0 && mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime >= aamp->mAbsoluteEndPosition) || (rate < 0 && mMediaStreamContext[eMEDIATYPE_VIDEO]->fragmentTime <= aamp->culledSeconds && (mMPDParseHelper->mLowerBoundaryPeriod == mCurrentPeriodIdx)))); // For rewinding, EOS does not need to be set unless the current period is a lower period.
					if ((!mIsLiveManifest || (rate != AAMP_NORMAL_PLAY_RATE)) && (eosOutSideAd || eosAdPlayback))
					{
						if (vEos)
						{
							AAMPLOG_INFO("EOS Reached.eosOutSideAd:%d eosAdPlayback:%d", eosOutSideAd, eosAdPlayback);
							mMediaStreamContext[eMEDIATYPE_VIDEO]->eosReached = true;
							mMediaStreamContext[eMEDIATYPE_VIDEO]->AbortWaitForCachedAndFreeFragment(false);
						}
						if (audioEnabled)
						{
							if (mMediaStreamContext[eMEDIATYPE_AUDIO]->eos)
							{
								mMediaStreamContext[eMEDIATYPE_AUDIO]->eosReached = true;
								mMediaStreamContext[eMEDIATYPE_AUDIO]->AbortWaitForCachedAndFreeFragment(false);
							}
						}
						else
						{ // No Audio enabled , fake the flag to true
							aEos = true;
						}
					}
					else
					{
						if (!audioEnabled)
						{
							aEos = true;
						}
					}
					// If audio and video reached EOS then only break the fetch loop .
					if (vEos && aEos)
					{
						AAMPLOG_DEBUG("EOS - Exit fetch loop ");
						// Disabling this log to avoid flooding, as Fetcher loop maintains track EOS until
						// playlist refreshes in parallel thread.
						// Enable 'info' level to track EOS from PushNextFragment.
						if (AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState)
						{
							adStateChanged = onAdEvent(AdEvent::AD_FINISHED);
						}
						// EOS from both tracks for dynamic (live) manifests for all periods.
						// If ad state is not IN_ADBREAK_WAIT2CATCHUP, go for the manifest update, otherwise break the loop.
						if (mIsLiveManifest && (rate > 0) && (mIterPeriodIndex == mMPDParseHelper->mUpperBoundaryPeriod) &&
							(AdState::IN_ADBREAK_WAIT2CATCHUP != mCdaiObject->mAdState))
						{
							aamp->interruptibleMsSleep(500);
						}
						else
						{
							AAMPLOG_INFO("Exiting from fetcher loop as EOS reached");
							break;
						}
					}
				}
				if (AdState::OUTSIDE_ADBREAK != mCdaiObject->mAdState)
				{
					Period2AdData &curPeriod = mCdaiObject->mPeriodMap[mBasePeriodId];
					if ((rate < 0 && mBasePeriodOffset <= 0) ||
						(rate > 0 && curPeriod.filled && curPeriod.duration <= (uint64_t)(mBasePeriodOffset * 1000)))
					{
						AAMPLOG_INFO("[CDAI]: BasePeriod[%s] completed @%lf. Changing to next ", mBasePeriodId.c_str(), mBasePeriodOffset);
						break;
					}
					else if (lastPrdOffset != mBasePeriodOffset && AdState::IN_ADBREAK_AD_NOT_PLAYING == mCdaiObject->mAdState)
					{
						// In adbreak, but somehow Ad is not playing. Need to check whether the position reached the next Ad start.
						adStateChanged = onAdEvent(AdEvent::BASE_OFFSET_CHANGE);
						if (adStateChanged)
							break;
					}
					lastPrdOffset = mBasePeriodOffset;
				}

				if (cacheFullStatus[eMEDIATYPE_VIDEO] || (vEos && !aEos))
				{
					// play cache is full , wait until cache is available to inject next, max wait of 1sec
					int timeoutMs = MAX_WAIT_TIMEOUT_MS;
					int trackIdx = (vEos && !aEos) ? eMEDIATYPE_AUDIO : eMEDIATYPE_VIDEO;
					AAMPLOG_DEBUG("Cache full state trackIdx %d vEos %d aEos %d timeoutMs %d Time %lld",
						trackIdx, vEos, aEos, timeoutMs, aamp_GetCurrentTimeMS());
					if(aamp->GetLLDashChunkMode() && !aamp->TrackDownloadsAreEnabled(static_cast<AampMediaType>(trackIdx)))
					{
						// Track is already at enough-data state, no need to wait for cache full
						aamp->interruptibleMsSleep(timeoutMs);
						AAMPLOG_DEBUG("Waited for track(%d) need-data", trackIdx);
					}
					else
					{
						bool temp = mMediaStreamContext[trackIdx]->WaitForFreeFragmentAvailable(timeoutMs);
						if (temp == false)
						{
							AAMPLOG_DEBUG("Waited for FreeFragmentAvailable"); // CID:82355 - checked return
						}
					}
				}
				else
				{
					// This sleep will hit when there is no content to download and cache is not full
					// and refresh interval timeout not reached . To Avoid tight loop adding a min delay
					aamp->interruptibleMsSleep(50);
				}
			} // Loop 2: end of while loop (!exitFetchLoop)
			SAFE_DELETE_ARRAY(cacheFullStatus);
			if(exitFetchLoop)
			{
				break;
			}
			// Skip iterator reset and continue with appropriate period based on rate or ad state
			resetIterator = false;
			// If we moved into an ad within the period mostly happens during rewind on partial ads, advance without incrementing the iterator.
			// Added check for IN_ADBREAK_AD_PLAYING to confirm this particular case.
			if (AdState::IN_ADBREAK_WAIT2CATCHUP == mCdaiObject->mAdState ||
				(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState && adStateChanged))
			{
				continue; // Need to finish all the ads in current before period change
			}
			if (rate > 0)
			{
				mIterPeriodIndex++;
			}
			else
			{
				mIterPeriodIndex--;
			}
			// Finished segments in the current period. Get the duration of that period.
			// Needed for live playback where timeline can increase dynamically.
			UpdatePtsOffset(false);
		}
		else
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: null mpd");
		}
	} // Loop 1
	while (!exitFetchLoop);
	AAMPLOG_MIL("FetcherLoop done");
}


/**
 * @fn AdvanceTsbFetch
 * @param[in] trackIdx - trackIndex
 * @param[in] trickPlay - trickplay flag
 * @param[in] delta - delta for skipping fragments
 * @param[out] waitForFreeFrag - waitForFreeFragmentAvailable flag
 * @param[out] bCacheFullState - cache status for track
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::AdvanceTsbFetch(int trackIdx, bool trickPlay, double delta, bool &waitForFreeFrag, bool &bCacheFullState)
{
	class MediaStreamContext *pMediaStreamContext = mMediaStreamContext[trackIdx];
	AampMediaType mediaType = (AampMediaType) trackIdx;
	AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
	std::shared_ptr<AampTsbReader> tsbReader;
	if(tsbSessionManager)
	{
		tsbReader = tsbSessionManager->GetTsbReader(mediaType);
	}
	bool isAllowNextFrag = true;
	int  maxCachedFragmentsPerTrack = (int)pMediaStreamContext->GetCachedFragmentChunksSize();

	if (waitForFreeFrag && !trickPlay)
	{
		AAMPPlayerState state = aamp->GetState();
		if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
		{
			state = eSTATE_PLAYING;
		}
		if(state == eSTATE_PLAYING)
		{
			waitForFreeFrag = false;
		}
		else
		{
			int timeoutMs = -1;
			if(bCacheFullState &&
				(pMediaStreamContext->numberOfFragmentChunksCached == maxCachedFragmentsPerTrack))
			{
				timeoutMs = MAX_WAIT_TIMEOUT_MS;
			}
			isAllowNextFrag = pMediaStreamContext->WaitForCachedFragmentChunkInjected(timeoutMs);
		}
	}

	if (isAllowNextFrag && tsbReader)
	{
			// profile not changed and not at EOS
			if(!pMediaStreamContext->profileChanged && !tsbReader->IsEos())
			{
				bool fragmentCached = tsbSessionManager->PushNextTsbFragment(pMediaStreamContext, maxCachedFragmentsPerTrack - pMediaStreamContext->numberOfFragmentChunksCached);
				AAMPLOG_TRACE("[%s] Fragment %s", GetMediaTypeName((AampMediaType)trackIdx), fragmentCached ? "cached" : "not cached");
			}
			if(pMediaStreamContext->numberOfFragmentChunksCached != maxCachedFragmentsPerTrack && bCacheFullState)
			{
				bCacheFullState = false;
			}
	}
}

/**
 * @brief Reads and caches fragments from AampTsbSessionManager
 */
void StreamAbstractionAAMP_MPD::TsbReader()
{
	aamp_setThreadName("aampTsbReader");
	bool exitLoop = false;
	bool trickPlay = (AAMP_NORMAL_PLAY_RATE != aamp->rate);
	bool waitForFreeFrag = true;
	//bool mpdChanged = false;
	//int direction = 1;
	double delta = 0;
	//bool adStateChanged = false;

	AAMPLOG_MIL("aamp: ready to read fragments");
	/*
	 * Ready to collect fragments
	 */
	do
	{
		// playback
		std::array<bool, AAMP_TRACK_COUNT>cacheFullStatus;
		cacheFullStatus.fill(false);
		AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
		if(NULL != tsbSessionManager)
		{
			while (!exitLoop)
			{
				for (int trackIdx = (mNumberOfTracks - 1); trackIdx >= 0; trackIdx--)
				{
					cacheFullStatus[trackIdx] = true;
					if (!tsbSessionManager->GetTsbReader((AampMediaType) trackIdx)->IsEos())
					{
						AdvanceTsbFetch(trackIdx, trickPlay, delta, waitForFreeFrag, cacheFullStatus[trackIdx]);
					}
				}
				if(abortTsbReader)
				{
					AAMPLOG_INFO("Exit TsbReader due to abort");
					exitLoop = true;
					break;
				}
				tsbSessionManager->LockReadMutex();
				bool vEOS = tsbSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->IsEos();
				bool aEOS = tsbSessionManager->GetTsbReader(eMEDIATYPE_AUDIO)->IsEos();
				tsbSessionManager->UnlockReadMutex();
				if(vEOS || aEOS)
				{
					bool breakLoop = false;
					// For Slow motion, its better to keep the playback in TSB mode, since chunk injection could get stalled
					// Hence slow motion will never hit EOS, once TSB data shows up, vEOS will be cleared
					if(AAMP_NORMAL_PLAY_RATE != aamp->rate && AAMP_SLOWMOTION_RATE != aamp->rate && vEOS)
					{
						// Mark trickplay EOS and inform injector to perform seek or seek to live
						AAMPLOG_INFO("Reader at EOS while trickplay");
						mMediaStreamContext[eMEDIATYPE_VIDEO]->eosReached = true;
						mMediaStreamContext[eMEDIATYPE_VIDEO]->AbortWaitForCachedAndFreeFragment(false);
						breakLoop = true;
					}
					if((vEOS && aEOS && (eTUNETYPE_SEEKTOLIVE == mTuneType)) || breakLoop)
					{

						// In the case of seeking to the Live edge, exit the reader and transfer control to the FetcherLoop.
						// For trick play scenarios, terminate the loop to proceed with end-of-stream processing.
						AAMPLOG_INFO("EOS - Exit TsbReader due to live edge seek");
						exitLoop = true;
						break;
					}
					AAMPLOG_TRACE("EOS from both tracks - Wait for next fragment");
					aamp->interruptibleMsSleep(500);
				}
				if(cacheFullStatus[eMEDIATYPE_VIDEO] || (vEOS && !aEOS))
				{
					// play cache is full , wait until cache is available to inject next, max wait of 1sec
					int timeoutMs = MAX_WAIT_TIMEOUT_MS;
					int trackIdx = (vEOS && !aEOS) ? eMEDIATYPE_AUDIO : eMEDIATYPE_VIDEO;
					//AAMPLOG_INFO("Cache full state track(%d), no download until(%d) Time(%lld)",trackIdx,timeoutMs,aamp_GetCurrentTimeMS());
					bool temp =  mMediaStreamContext[trackIdx]->WaitForCachedFragmentChunkInjected(timeoutMs);
					if(temp == false)
					{
						//AAMPLOG_INFO("Waiting for FreeFragmentAvailable");  //CID:82355 - checked return
					}
				}
				else
				{	// This sleep will hit when there is no content to download and cache is not full
					// and refresh interval timeout not reached . To Avoid tight loop adding a min delay
					aamp->interruptibleMsSleep(50);
				}
			} // Loop 2 : TSB FetchLoop
		}
	}
	while (!exitLoop);
	AAMPLOG_MIL("TsbReader done");
}

/**
 * @brief Check new early available periods
 */
void StreamAbstractionAAMP_MPD::GetAvailableVSSPeriods(std::vector<IPeriod*>& PeriodIds)
{
	//for(IPeriod* tempPeriod : mpd->GetPeriods())
	int numPeriods = (int)PeriodIds.size();
	for(int periodIter = 0; periodIter < numPeriods; periodIter++)
	{
		IPeriod *tempPeriod	=	PeriodIds.at(periodIter);
		if (STARTS_WITH_IGNORE_CASE(tempPeriod->GetId().c_str(), VSS_DASH_EARLY_AVAILABLE_PERIOD_PREFIX))
		{
			if((1 == tempPeriod->GetAdaptationSets().size()) && mMPDParseHelper->IsEmptyPeriod(periodIter, (rate != AAMP_NORMAL_PLAY_RATE)))
			{
				if(std::find(mEarlyAvailablePeriodIds.begin(), mEarlyAvailablePeriodIds.end(), tempPeriod->GetId()) == mEarlyAvailablePeriodIds.end())
				{
					PeriodIds.push_back(tempPeriod);
				}
			}
		}
	}
}

/**
 * @fn UpdateMPD
 * @param init flag to indicate whether call is from init\
 * @return AAMPStatusType
 */
AAMPStatusType StreamAbstractionAAMP_MPD::UpdateMPD(bool init)
{
	AAMPStatusType ret = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;

	if(mIsLiveManifest)
	{
		// let all the track threads to pause for manifest update
		//playlistDownloaderContext->NotifyFragmentCollectorWait();
		//playlistDownloaderContext->WaitForManifestUpdate();
		AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
		// Get the Manifest with a wait of Manifest Timeout time
		ManifestDownloadResponsePtr tmpManifestDnldRespPtr ;
		// No wait needed , pick the manifest from top of the Q
		tmpManifestDnldRespPtr  = dnldInstance->GetManifest(false, 0);
		// Need to check if same last Manifest is given or new refresh happened
		if( tmpManifestDnldRespPtr->mMPDStatus == AAMPStatusType::eAAMPSTATUS_OK )
		{
			if(tmpManifestDnldRespPtr->mMPDInstance != mManifestDnldRespPtr->mMPDInstance)
			{
				mManifestDnldRespPtr    =       tmpManifestDnldRespPtr;
				ret = GetMPDFromManifest(mManifestDnldRespPtr , false);
				// if no parse error
				if(ret == AAMPStatusType::eAAMPSTATUS_OK)
				{
					AAMPLOG_INFO("Got Manifest Updated . Continue with Fetcherloop");
					// mCurrentPeriodIdx, mNumberOfPeriods based on mBasePeriodId
					ret = IndexNewMPDDocument();
				}
			}
		}
	}
	return ret;
}

/**
 * @brief Check for VSS tags
 * @retval bool true if found, false otherwise
 */
bool StreamAbstractionAAMP_MPD::CheckForVssTags()
{
	bool isVss = false;
	IMPDElement* nodePtr = mpd;

	if (!nodePtr)
	{
		AAMPLOG_ERR("API Failed due to Invalid Arguments");
	}
	else
	{
		// get the VSS URI for comparison
		std::string schemeIdUriVss = GETCONFIGVALUE(eAAMPConfig_SchemeIdUriVssStream);
		for (INode* childNode : nodePtr->GetAdditionalSubNodes())
		{
			const std::string& name = childNode->GetName();
			if (name == SUPPLEMENTAL_PROPERTY_TAG)
			{
				if (childNode->HasAttribute("schemeIdUri"))
				{
					// VSS stream should read config independent of RFC availability and compare for URI
					const std::string& schemeIdUri = childNode->GetAttributeValue("schemeIdUri");
					if (schemeIdUri == schemeIdUriVss)
					{
						if (childNode->HasAttribute("value"))
						{
							std::string value = childNode->GetAttributeValue("value");
							mCommonKeyDuration = std::stoi(value);
							AAMPLOG_INFO("Received Common Key Duration : %d of VSS stream", mCommonKeyDuration);
							if (aamp->mDRMLicenseManager)
							{
								AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
								licenseManager->SetCommonKeyDuration(mCommonKeyDuration);
							}
							isVss = true;
						}
					}
				}
			}
		}
	}

	return isVss;
}

/**
 * @brief GetVssVirtualStreamID from manifest
 * @retval return Virtual stream ID string
 */
std::string StreamAbstractionAAMP_MPD::GetVssVirtualStreamID()
{
	std::string ret;
	IMPDElement* nodePtr = mpd;

	if (!nodePtr)
	{
		AAMPLOG_ERR("API Failed due to Invalid Arguments");
	}
	else
	{
		for (auto* childNode : mpd->GetProgramInformations())
		{
				for (auto infoNode : childNode->GetAdditionalSubNodes())
				{
					std::string subNodeName;
					std::string ns;
					ParseXmlNS(infoNode->GetName(), ns, subNodeName);
					const std::string& infoNodeType = infoNode->GetAttributeValue("type");
					if ((subNodeName == "ContentIdentifier") && (infoNodeType == "URI" || infoNodeType == "URN"))
					{
						if (infoNode->HasAttribute("value"))
						{
							std::string value = infoNode->GetAttributeValue("value");
							if (value.find(VSS_VIRTUAL_STREAM_ID_PREFIX) != std::string::npos)
							{
								ret = value.substr(sizeof(VSS_VIRTUAL_STREAM_ID_PREFIX)-1);
								AAMPLOG_INFO("Parsed Virtual Stream ID from manifest:%s", ret.c_str());
								break;
							}
						}
					}
				}
			if (!ret.empty())
			{
				break;
			}
		}
	}
	return ret;
}

/**
 * @brief Init webvtt subtitle parser for sidecar caption support
 * @param webvtt data
 */
void StreamAbstractionAAMP_MPD::InitSubtitleParser(char *data)
{
	mSubtitleParser = this->RegisterSubtitleParser_CB(eSUB_TYPE_WEBVTT);
	if (mSubtitleParser)
	{
		double position = aamp->GetPositionSeconds();
		AAMPLOG_WARN("sending init to webvtt parser %.3f", position);
		mSubtitleParser->init(position,0);
		mSubtitleParser->mute(false);
		AAMPLOG_INFO("sending data");
		if (data != NULL)
			mSubtitleParser->processData(data,strlen(data),0,0);
	}

}

/**
 * @brief Reset subtitle parser created for sidecar caption support
 */
void StreamAbstractionAAMP_MPD::ResetSubtitle()
{
	if (mSubtitleParser)
	{
		mSubtitleParser->reset();
		mSubtitleParser = NULL;
	}
}

/**
 * @brief Mute subtitles on puase
 */
void StreamAbstractionAAMP_MPD::MuteSubtitleOnPause()
{
	if (mSubtitleParser)
	{
		mSubtitleParser->pause(true);
		mSubtitleParser->mute(true);
	}
}

/**
 * @brief Resume subtitle on play
 * @param mute status
 * @param webvtt data
 */
void StreamAbstractionAAMP_MPD::ResumeSubtitleOnPlay(bool mute, char *data)
{
	if (mSubtitleParser)
	{
		mSubtitleParser->pause(false);
		mSubtitleParser->mute(mute);
		if (data != NULL)
			mSubtitleParser->processData(data,strlen(data),0,0);
	}
}

/**
 * @brief Mute/unmute subtitles
 * @param mute/unmute
 */
void StreamAbstractionAAMP_MPD::MuteSidecarSubtitles(bool mute)
{
	if (mSubtitleParser)
		mSubtitleParser->mute(mute);
}

/**
 * @brief Resume subtitle after seek
 * @param mute status
 * @param webvtt data
 */
void  StreamAbstractionAAMP_MPD::ResumeSubtitleAfterSeek(bool mute, char *data)
{
	mSubtitleParser = this->RegisterSubtitleParser_CB(eSUB_TYPE_WEBVTT);
	if (mSubtitleParser)
	{
		mSubtitleParser->updateTimestamp(seekPosition*1000);
		mSubtitleParser->mute(mute);
		if (data != NULL)
			mSubtitleParser->processData(data,strlen(data),0,0);
	}

}

/**
 * @brief StreamAbstractionAAMP_MPD Destructor
 */
StreamAbstractionAAMP_MPD::~StreamAbstractionAAMP_MPD()
{
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		SAFE_DELETE(track);
	}

	AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
	mManifestUpdateHandleFlag       =       false;
	dnldInstance->UnRegisterCallback();

	aamp->SyncBegin();

	SAFE_DELETE_ARRAY(mStreamInfo);
	deIndexTileInfo(indexedTileInfo);
	if(!thumbnailtrack.empty())
	{
		for(int i = 0; i < thumbnailtrack.size() ; i++)
		{
			StreamInfo *tmp = thumbnailtrack[i];
			SAFE_DELETE(tmp);
		}
	}

	aamp->CurlTerm(eCURLINSTANCE_VIDEO, DEFAULT_CURL_INSTANCE_COUNT);
	memset(aamp->GetLLDashServiceData(),0x00,sizeof(AampLLDashServiceData));
	aamp->SetLowLatencyServiceConfigured(false);
	aamp->SyncEnd();
	mManifestDnldRespPtr = nullptr;
}

void StreamAbstractionAAMP_MPD::StartFromOtherThanAampLocalTsb(void)
{
	aamp->mDRMLicenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
	// Start the worker threads for each track
	try
	{
		// Attempting to assign to a running thread will cause std::terminate(), not an exception
		if(!fragmentCollectorThreadID.joinable())
		{
			fragmentCollectorThreadID = std::thread(&StreamAbstractionAAMP_MPD::FetcherLoop, this);
			AAMPLOG_INFO("Thread created for FetcherLoop [%zx]", GetPrintableThreadID(fragmentCollectorThreadID));
		}
		else
		{
			AAMPLOG_INFO("FetcherLoop thread already running, not creating a new one");
		}
	}
	catch (std::exception &e)
	{
		AAMPLOG_ERR("Thread allocation failed for FetcherLoop : %s ", e.what());
	}

	if(aamp->IsPlayEnabled())
	{
		for (int i = 0; i < mNumberOfTracks; i++)
		{
			aamp->ResumeTrackInjection((AampMediaType) i);
			// TODO: This could be moved to StartInjectLoop, but due to lack of testing will keep it here for now
			if(mMediaStreamContext[i]->playContext)
			{
				mMediaStreamContext[i]->playContext->reset();
			}
			mMediaStreamContext[i]->StartInjectLoop();
		}
	}
}

void StreamAbstractionAAMP_MPD::StartFromAampLocalTsb(void)
{
	aamp->ResetTrackDiscontinuityIgnoredStatus();
	aamp->UnblockWaitForDiscontinuityProcessToComplete();
	mTrackState = eDISCONTINUITY_FREE;
	for (int i = 0; i < mNumberOfTracks; i++)
	{
		// Flush fragments from mCachedFragment, potentially cached during Live SLD
		mMediaStreamContext[i]->FlushFetchedFragments();

		// Flush fragments from mCachedFragmentChunks
		mMediaStreamContext[i]->FlushFragments();

		// For seek to live, we will employ chunk cache and hence size has to be increased to max
		// For other tune types, we don't need chunks so revert to max cache fragment size

		if ((mTuneType == eTUNETYPE_SEEKTOLIVE) && (aamp->GetLLDashChunkMode()))
		{
			mMediaStreamContext[i]->SetCachedFragmentChunksSize(size_t(mMediaStreamContext[i]->maxCachedFragmentChunksPerTrack));
		}
		else
		{
			mMediaStreamContext[i]->SetCachedFragmentChunksSize(size_t(mMediaStreamContext[i]->maxCachedFragmentsPerTrack));
		}

		mMediaStreamContext[i]->eosReached = false;
		if(aamp->IsPlayEnabled())
		{
			aamp->ResumeTrackInjection((AampMediaType) i);
			// TODO: This could be moved to StartInjectLoop, but due to lack of testing will keep it here for now
			if(mMediaStreamContext[i]->playContext)
			{
				mMediaStreamContext[i]->playContext->reset();
			}
			mMediaStreamContext[i]->StartInjectLoop(); ///TBD
		}
	}
	try
	{
		abortTsbReader = false;
		if (!tsbReaderThreadID.joinable())
		{
			tsbReaderThreadID = std::thread(&StreamAbstractionAAMP_MPD::TsbReader, this);
			AAMPLOG_INFO("Thread created for TsbReader [%zx]", GetPrintableThreadID(tsbReaderThreadID));
		}
		else
		{
			AAMPLOG_WARN("Attempt to create TsbReader thread while thread is running");
		}
	}
	catch(const std::exception& e)
	{
		AAMPLOG_ERR("Thread allocation failed for TsbReader : %s ", e.what());
	}
}

void StreamAbstractionAAMP_MPD::Start(void)
{
	if (aamp->IsLocalAAMPTsbInjection())
	{
		StartFromAampLocalTsb();
	}
	else
	{
		StartFromOtherThanAampLocalTsb();
	}

	if( (mLowLatencyMode && ISCONFIGSET( eAAMPConfig_EnableLowLatencyCorrection ) ) && \
		(true == aamp->GetLLDashAdjustSpeed() ) )
	{
		StartLatencyMonitorThread();
	}
}

/**
 *   @brief  Stops streaming.
 */
void StreamAbstractionAAMP_MPD::Stop(bool clearChannelData)
{

	if (!aamp->IsLocalAAMPTsb() || aamp->mAampTsbLanguageChangeInProgress)
	{
		aamp->DisableDownloads();
		mCdaiObject->AbortWaitForNextAdResolved();
		aamp->mAampTsbLanguageChangeInProgress = false;
	}

	ReassessAndResumeAudioTrack(true);
	AbortWaitForAudioTrackCatchup(false);
	// Change order of stopping threads. Collector thread has to be stopped at the earliest
	// There is a chance fragment collector is processing StreamSelection() which can change the mNumberOfTracks
	// and Enabled() status of MediaTrack momentarily.
	// Call AbortWaitForCachedAndFreeFragment() to unblock collector thread from WaitForFreeFragmentAvailable
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track)
		{
			if(track->playContext)
			{
				track->playContext->abort();
			}
			if(!aamp->IsLocalAAMPTsb())
			{
				track->AbortWaitForCachedAndFreeFragment(true);
			}
			else
			{
				track->AbortWaitForCachedFragment();
				if (track->IsLocalTSBInjection())
				{
					// TSBReader could be waiting indefinitely WaitForCachedFragmentChunkInjected, this will unblock the same
					track->AbortWaitForCachedFragmentChunk();
				}
			}
		}
	}

	if(latencyMonitorThreadStarted)
	{
		aamp->SetLLDashAdjustSpeed(false);
		aamp->SetLLDashCurrentPlayBackRate(GETCONFIGVALUE(eAAMPConfig_NormalLatencyCorrectionPlaybackRate));
		if (aamp->IsLocalAAMPTsb())
		{
			aamp->WakeupLatencyCheck();
		}
		AAMPLOG_TRACE("Waiting to join StartLatencyMonitorThread");
		latencyMonitorThreadID.join();
		AAMPLOG_INFO("Joined StartLatencyMonitorThread");
		latencyMonitorThreadStarted = false;
	}
	if (!aamp->DownloadsAreEnabled() && fragmentCollectorThreadID.joinable())
	{
		fragmentCollectorThreadID.join();
	}

	if(tsbReaderThreadID.joinable())
	{
		AAMPLOG_INFO("Abort TsbReader");
		abortTsbReader = true;
		tsbReaderThreadID.join();
		AAMPLOG_INFO("Joined tsbReaderThreadID");
	}

	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track)
		{
			aamp->StopTrackInjection((AampMediaType) iTrack);
			track->StopInjectLoop();
			if(!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
			{
				if (iTrack == eMEDIATYPE_SUBTITLE && track->mSubtitleParser)
				{
					track->mSubtitleParser->reset();
				}
			}
			/* Once playing back from TSB we only stop on new tune / retune, or for
				LLD when returning to normal rate at the live edge (done in cacheFragment).
				So avoid clearing the flag unless we are stopping for a new tune / retune. */
			if (aamp->IsLocalAAMPTsb() && clearChannelData)
			{
				track->SetLocalTSBInjection(false);
			}
			track->IDX.Free();
		}
	}

	if (!aamp->IsLocalAAMPTsb())
	{
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);
		if (sink)
		{
			sink->ClearProtectionEvent();
		}
		if (clearChannelData)
		{
			if(ISCONFIGSET(eAAMPConfig_UseSecManager) || ISCONFIGSET(eAAMPConfig_UseFireboltSDK))
			{
				aamp->mDRMLicenseManager->notifyCleanup();
			}
		}
		aamp->mDRMLicenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_INACTIVE);
		if(tsbReaderThreadID.joinable())
		{
			abortTsbReader = true;
			tsbReaderThreadID.join();
		}

	}

	if (!aamp->DownloadsAreEnabled())
	{
		aamp->EnableDownloads();
	}

	if (!aamp->IsLocalAAMPTsb() && !clearChannelData)
	{
		mCdaiObject->NotifyAdLoopWait();
	}
}

StreamOutputFormat GetSubtitleFormat(std::string mimeType)
{
	StreamOutputFormat format = FORMAT_SUBTITLE_MP4; // Should I default to INVALID?

	if (!mimeType.compare("application/mp4"))
		format = FORMAT_SUBTITLE_MP4;
	else if (!mimeType.compare("application/ttml+xml"))
		format = FORMAT_SUBTITLE_TTML;
	else if (!mimeType.compare("text/vtt"))
		format = FORMAT_SUBTITLE_WEBVTT;
	else
		AAMPLOG_INFO("Not found mimeType %s", mimeType.c_str());

	AAMPLOG_DEBUG("Returning format %d for mimeType %s", format, mimeType.c_str());

	return format;
}

/**
 * @brief Get output format of stream.
 *
 */
void StreamAbstractionAAMP_MPD::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
	if(mMediaStreamContext[eMEDIATYPE_VIDEO] && mMediaStreamContext[eMEDIATYPE_VIDEO]->enabled )
	{
		primaryOutputFormat = FORMAT_ISO_BMFF;
	}
	else
	{
		primaryOutputFormat = FORMAT_INVALID;
	}
	if(mMediaStreamContext[eMEDIATYPE_AUDIO] && mMediaStreamContext[eMEDIATYPE_AUDIO]->enabled )
	{
		audioOutputFormat = FORMAT_ISO_BMFF;
	}
	else
	{
		audioOutputFormat = FORMAT_INVALID;
	}
	//if subtitle is disabled, but aux is enabled, then its status is saved in place of eMEDIATYPE_SUBTITLE
	if ((mMediaStreamContext[eMEDIATYPE_AUX_AUDIO] && mMediaStreamContext[eMEDIATYPE_AUX_AUDIO]->enabled) ||
		(mMediaStreamContext[eMEDIATYPE_SUBTITLE] && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled && mMediaStreamContext[eMEDIATYPE_SUBTITLE]->type == eTRACK_AUX_AUDIO))
	{
		auxOutputFormat = FORMAT_ISO_BMFF;
	}
	else
	{
		auxOutputFormat = FORMAT_INVALID;
	}

	//TODO - check whether the ugly hack above is in operation
	// This is again a dirty hack, the check for PTS restamp enabled. TODO: We need to remove this in future
	// For cases where subtitles is enabled mid-playback, we need to configure the pipeline at the beginning. FORMAT_SUBTITLE_MP4 will be set
	if (mMediaStreamContext[eMEDIATYPE_SUBTITLE] && 
		mMediaStreamContext[eMEDIATYPE_SUBTITLE]->type != eTRACK_AUX_AUDIO)
	{
		if (mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled || ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
		{
			AAMPLOG_WARN("Entering GetCurrentMimeType");
			auto mimeType = GetCurrentMimeType(eMEDIATYPE_SUBTITLE);
			if (!mimeType.empty())
			{
				subtitleOutputFormat = GetSubtitleFormat(mimeType);
			}
			// Ensure thatsubtitleOutputFormat is set to FORMAT_INVALID rather than FORMAT_SUBTITLE_MP4 when
			// presenting inband CC with PTS restamping enabled
			else if(isInBandCcAvailable())
			{
				subtitleOutputFormat = FORMAT_INVALID;
			}
			else
			{
				AAMPLOG_INFO("mimeType empty");
				subtitleOutputFormat = FORMAT_SUBTITLE_MP4;
			}
		}

		// If subtitles are not enabled, we need to have an init fragment to inject otherwise
		// a complete pipeline cannot be created; and Rialto will not start playing video
		if (!mMediaStreamContext[eMEDIATYPE_SUBTITLE]->enabled && ISCONFIGSET(eAAMPConfig_useRialtoSink))
		{
			auto subtitleHeader = AampStreamSinkManager::GetInstance().GetMediaHeader(eMEDIATYPE_SUBTITLE);
			if(subtitleHeader && !subtitleHeader->mimeType.empty())
			{
				subtitleOutputFormat = GetSubtitleFormat(subtitleHeader->mimeType);
				AAMPLOG_INFO("Using saved subtitle mime type, subtitleOutputFormat = %d", subtitleOutputFormat);
			}
			else
			{
				subtitleOutputFormat = FORMAT_INVALID;
			}
		}
	}
	else
	{
		subtitleOutputFormat = FORMAT_INVALID;
	}
}

/**
 *   @brief Return MediaTrack of requested type
 *
 *   @retval MediaTrack pointer.
 */
MediaTrack* StreamAbstractionAAMP_MPD::GetMediaTrack(TrackType type)
{
	return mMediaStreamContext[type];
}

double StreamAbstractionAAMP_MPD::GetBufferedDuration()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double retval = -1.0;
	if (video && video->enabled)
	{
		retval = video->GetBufferedDuration();
	}
	return retval;
}


/**
 * @brief Get current stream position.
 *
 * Note: The stream position is sometimes an absolute position (seconds from 1970)
 * and sometimes a relative position (seconds from tuning).
 *
 * @retval current position of stream.
 */
double StreamAbstractionAAMP_MPD::GetStreamPosition()
{
	return seekPosition;
}

/**
 * @brief Get Period Start Time.
 *
 * @retval Period Start Time.
 */
double StreamAbstractionAAMP_MPD::GetFirstPeriodStartTime(void)
{
	return mFirstPeriodStartTime;
}

/**
 * @brief Gets number of profiles
 * @retval number of profiles
 */
int StreamAbstractionAAMP_MPD::GetProfileCount()
{
	int ret = 0;
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;

	if(isFogTsb)
	{
		ret = (int)mBitrateIndexVector.size();
	}
	else
	{
		ret = GetABRManager().getProfileCount();
	}
	return ret;
}


/**
 * @brief Get profile index for TsbBandwidth
 * @retval profile index of the current bandwidth
 */
int StreamAbstractionAAMP_MPD::GetProfileIndexForBandwidth( BitsPerSecond mTsbBandwidth)
{
	int profileIndex = 0;
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;

	if(isFogTsb)
	{
			std::vector<BitsPerSecond>::iterator it = std::find(mBitrateIndexVector.begin(), mBitrateIndexVector.end(), mTsbBandwidth);

			if (it != mBitrateIndexVector.end())
			{
					// Get index of _element_ from iterator
					profileIndex = (int)std::distance(mBitrateIndexVector.begin(), it);
			}
	}
	else
	{
			profileIndex = GetABRManager().getBestMatchedProfileIndexByBandWidth((int)mTsbBandwidth);
	}
	return profileIndex;
}

/**
 *   @brief Get stream information of a profile from subclass.
 *
 *   @retval stream information corresponding to index.
 */
StreamInfo* StreamAbstractionAAMP_MPD::GetStreamInfo(int idx)
{
	bool isFogTsb = mIsFogTSB && !mAdPlayingFromCDN;
	if (isFogTsb)
	{
		return &mStreamInfo[idx];
	}
	else
	{
		int userData = 0;

		if (GetProfileCount() && !aamp->IsFogTSBSupported()) // avoid calling getUserDataOfProfile() for playlist only URL playback.
		{
			userData = GetABRManager().getUserDataOfProfile(idx);
		}
		return &mStreamInfo[userData];
	}
}


/**
 *   @brief  Get (restamped) PTS of first sample.
 *
 *   @retval PTS of first sample, restamped if PTS restamping is enabled.
 */
double StreamAbstractionAAMP_MPD::GetFirstPTS()
{
	AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
	std::shared_ptr<AampTsbReader> reader = nullptr;
	double firstPTS = 0;
	double restampedPTS = 0;
	AampTime ptsOffset = 0;

	if (tsbSessionManager)
	{
		MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
		if (!video)
		{
			AAMPLOG_WARN("Video track unavailable, cannot determine if local tsb injection");
		}
		else if (video->IsLocalTSBInjection())
		{
			reader = tsbSessionManager->GetTsbReader(eMEDIATYPE_VIDEO);
		}
		else
		{
			AAMPLOG_TRACE("Not local TSB injection");
		}
	}

	if (reader)
	{
		firstPTS = reader->GetFirstPTS();
		// PTS restamping is always enabled for TSB
		ptsOffset = reader->GetFirstPTSOffset();
	}
	else
	{
		firstPTS = mFirstPTS;
		if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
		{
			ptsOffset = mPTSOffset;
		}
	}

	restampedPTS = firstPTS + ptsOffset.inSeconds();
	AAMPLOG_INFO("Restamped first pts:%lf, firstPTS:%lf, ptsOffsetSec:%lf", restampedPTS, firstPTS, ptsOffset.inSeconds());

	return restampedPTS;
}

/**
 *   @brief  Get PTS offset for MidFragment Seek
 *
 *   @return seek PTS offset for midfragment seek
 */
double StreamAbstractionAAMP_MPD::GetMidSeekPosOffset()
{
	return mVideoPosRemainder;
}

/**
 *   @brief  Get Start time PTS of first sample.
 *
 *   @retval start time of first sample
 */
double StreamAbstractionAAMP_MPD::GetStartTimeOfFirstPTS()
{
	AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
	double startTime = mStartTimeOfFirstPTS;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if (tsbSessionManager && video && video->IsLocalTSBInjection())
	{
		startTime = 0;
	}
	return startTime;
}

/**
 * @brief Get index of profile corresponds to bandwidth
 * @retval profile index
 */
int StreamAbstractionAAMP_MPD::GetBWIndex(BitsPerSecond bitrate)
{
	int topBWIndex = 0;
	int profileCount = GetProfileCount();
	if (profileCount)
	{
		for (int i = 0; i < profileCount; i++)
		{
			StreamInfo *streamInfo = &mStreamInfo[i];
			if (!streamInfo->isIframeTrack && streamInfo->enabled && streamInfo->bandwidthBitsPerSecond > bitrate)
			{
				--topBWIndex;
			}
		}
	}
	return topBWIndex;
}

/**
 * @brief To get the available video bitrates.
 * @ret available video bitrates
 */
std::vector<BitsPerSecond> StreamAbstractionAAMP_MPD::GetVideoBitrates(void)
{
	std::vector<BitsPerSecond> bitrates;
	int profileCount = GetProfileCount();
	bitrates.reserve(profileCount);
	if (profileCount)
	{
		for (int i = 0; i < mProfileCount; i++)
		{
			StreamInfo *streamInfo = &mStreamInfo[i];
			if (!streamInfo->isIframeTrack && streamInfo->enabled)
			{
				bitrates.push_back(streamInfo->bandwidthBitsPerSecond);
			}
		}
		if (bitrates.size() != profileCount)
		{
			AAMPLOG_WARN("Mismatch in bitrateList, list size:%zu and profileCount:%d", bitrates.size(), profileCount);
		}
	}
	return bitrates;
}

/*
* @brief Gets Max Bitrate available for current playback.
* @ret long MAX video bitrates
*/
BitsPerSecond StreamAbstractionAAMP_MPD::GetMaxBitrate()
{
	BitsPerSecond maxBitrate = 0;
	if( mIsFogTSB )
	{
		maxBitrate = mMaxTSBBandwidth;
	}
	else
	{

		maxBitrate = StreamAbstractionAAMP::GetMaxBitrate();
	}
	return maxBitrate;
}


/**
 * @brief To get the available audio bitrates.
 * @ret available audio bitrates
 */
std::vector<BitsPerSecond> StreamAbstractionAAMP_MPD::GetAudioBitrates(void)
{
	std::vector<BitsPerSecond> audioBitrate;
	int trackSize = (int)mAudioTracks.size();
	if(trackSize)
	{
		audioBitrate.reserve(trackSize);
		std::vector<AudioTrackInfo>::iterator itr;

		for(itr = mAudioTracks.begin(); itr != mAudioTracks.end(); itr++)
		{
			audioBitrate.push_back(itr->bandwidth);
		}
	}
	return audioBitrate;
}

static void indexThumbnails(dash::mpd::IMPD *mpd, int thumbIndexValue, std::vector<TileInfo> &indexedTileInfo,std::vector<StreamInfo*> &thumbnailtrack,StreamAbstractionAAMP_MPD* mpdInstance)
{
	bool trackEmpty = thumbnailtrack.empty();
	AampMPDParseHelper *MPDParseHelper = nullptr;
	MPDParseHelper  = new AampMPDParseHelper();
	MPDParseHelper->Initialize(mpd);
	FragmentDescriptor fragmentDescriptor;

	if(trackEmpty || indexedTileInfo.empty())
	{
		int w = 1, h = 1, bandwidth = 0, periodIndex = 0;
		bool isAdPeriod = true, done = false;
		double adDuration = 0;
		long int prevStartNumber = -1;
		const std::vector<IBaseUrl *> mpdBaseUrls = mpd->GetBaseUrls();
		{
			for(IPeriod* tempPeriod : mpd->GetPeriods())
			{
				const std::vector<IAdaptationSet *> adaptationSets = tempPeriod->GetAdaptationSets();
				const std::vector<IBaseUrl *> PeriodBaseUrls = tempPeriod->GetBaseURLs();
				int adSize = (int)adaptationSets.size();
				for(int j =0; j < adSize; j++)
				{
					if(MPDParseHelper->IsContentType(adaptationSets.at(j), eMEDIATYPE_IMAGE) )
					{
						isAdPeriod = false;
						const std::vector<IBaseUrl *> adapBaseUrls = adaptationSets.at(j)->GetBaseURLs();
						const std::vector<IRepresentation *> representation = adaptationSets.at(j)->GetRepresentation();
						for (int repIndex = 0; repIndex < representation.size(); repIndex++)
						{
							const dash::mpd::IRepresentation *rep = representation.at(repIndex);
							fragmentDescriptor.ClearMatchingBaseUrl();
							fragmentDescriptor.AppendMatchingBaseUrl(&mpdBaseUrls);
							fragmentDescriptor.AppendMatchingBaseUrl(&PeriodBaseUrls);
							fragmentDescriptor.AppendMatchingBaseUrl(&adapBaseUrls);
							fragmentDescriptor.AppendMatchingBaseUrl( &rep->GetBaseURLs() );
							const std::vector<INode *> subnodes = rep->GetAdditionalSubNodes();
							PeriodElement periodElement(adaptationSets.at(j), rep);
							for (unsigned i = 0; i < subnodes.size() && !done; i++)
							{
								INode *xml = subnodes[i];
								if(xml != NULL)
								{
									if (xml->GetName() == "EssentialProperty")
									{
										if (xml->HasAttribute("schemeIdUri"))
										{
											const std::string& schemeUri = xml->GetAttributeValue("schemeIdUri");
											if (schemeUri == "http://dashif.org/guidelines/thumbnail_tile")
											{
												AAMPLOG_TRACE("schemeuri = thumbnail_tile");
											}
											else
											{
												AAMPLOG_WARN("skipping schemeUri %s", schemeUri.c_str());
											}
										}
										if(xml->HasAttribute("value"))
										{
											const std::string& value = xml->GetAttributeValue("value");
											if(!value.empty())
											{
												sscanf(value.c_str(), "%dx%d",&w,&h);
												AAMPLOG_WARN("value=%dx%d",w,h);
												done = true;
											}
										}
									}
									else
									{
										AAMPLOG_WARN("skipping name %s", xml->GetName().c_str());
									}
								}
								else
								{
									AAMPLOG_WARN("xml is null");  //CID:81118 - Null Returns
								}
							}	// end of sub node loop
							bandwidth = rep->GetBandwidth();
							if(thumbIndexValue < 0 || trackEmpty)
							{
								std::string mimeType = periodElement.GetMimeType();
								StreamInfo *tmp = new StreamInfo;
								tmp->bandwidthBitsPerSecond = (long) bandwidth;
								tmp->resolution.width = rep->GetWidth();
								tmp->resolution.height = rep->GetHeight();
								if((tmp->resolution.width == 0) && (tmp->resolution.height == 0))
								{
									IAdaptationSet *adaptationSet = adaptationSets.at(j);
									tmp->resolution.width = adaptationSet->GetWidth();
									tmp->resolution.height = adaptationSet->GetHeight();
								}
								tmp->resolution.width /= w;
								tmp->resolution.height /= h;
								tmp->baseUrl = fragmentDescriptor.GetMatchingBaseUrl();
								thumbnailtrack.push_back(tmp);
								AAMPLOG_TRACE("Thumbnailtrack bandwidth=%" BITSPERSECOND_FORMAT " width=%d height=%d", tmp->bandwidthBitsPerSecond, tmp->resolution.width, tmp->resolution.height);
							}
							if((thumbnailtrack.size() > thumbIndexValue) && thumbnailtrack[thumbIndexValue]->bandwidthBitsPerSecond == (long)bandwidth)
							{
								const ISegmentTemplate *segRep = NULL;
								const ISegmentTemplate *segAdap = NULL;
								segAdap = adaptationSets.at(j)->GetSegmentTemplate();
								segRep = representation.at(repIndex)->GetSegmentTemplate();
								SegmentTemplates segmentTemplates(segRep, segAdap);
								if( segmentTemplates.HasSegmentTemplate() )
								{
									const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
									uint32_t timeScale = segmentTemplates.GetTimescale();
									long int startNumber = segmentTemplates.GetStartNumber();
									std::string media = segmentTemplates.Getmedia();
									if (segmentTimeline)
									{
										AAMPLOG_TRACE("segment timeline");
										std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
										int timeLineIndex = 0;
										uint64_t durationMs = 0;
										std::string RepresentationID = rep->GetId();
										while (timeLineIndex < timelines.size())
										{
											if( prevStartNumber == startNumber )
											{
												/* TODO: This is temporary workaround for MT Ad streams which has redundant tile information
												   This entire condition has to be removed once the manifest is fixed. */
												timeLineIndex++;
												startNumber++;
												continue;
											}
											ITimeline *timeline = timelines.at(timeLineIndex);
											if(timeScale != 0)
											{
												double startTime = (timeline->GetStartTime() /(double)timeScale);  //CID:170361 - Unintended integer division
												int repeatCount = timeline->GetRepeatCount();
												uint32_t timelineDurationMs = ComputeFragmentDuration(timeline->GetDuration(),timeScale);
												while( repeatCount-- >= 0 )
												{
													std::string tmedia = media;
													TileInfo tileInfo;
													memset( &tileInfo,0,sizeof(tileInfo) );
													tileInfo.startTime = startTime + ( adDuration / timeScale) ;
													AAMPLOG_TRACE("timeLineIndex[%d] size [%zu] updated durationMs[%" PRIu64 "] startTime:%f adDuration:%f repeatCount:%d",  timeLineIndex, timelines.size(), durationMs, startTime, adDuration, repeatCount);

													startTime += ( timelineDurationMs );
													replace(tmedia,"RepresentationID",RepresentationID);
													replace(tmedia, "Number", startNumber);
													tileInfo.url = tmedia;
													tileInfo.layout.posterDuration = ((double)segmentTemplates.GetDuration()) / (timeScale * w * h);
													tileInfo.layout.tileSetDuration = ComputeFragmentDuration(timeline->GetDuration(), timeScale);
													tileInfo.layout.numRows = h;
													tileInfo.layout.numCols = w;
													AAMPLOG_TRACE("TileInfo - StartTime:%f posterDuration:%f tileSetDuration:%f numRows:%d numCols:%d",tileInfo.startTime,tileInfo.layout.posterDuration,tileInfo.layout.tileSetDuration,tileInfo.layout.numRows,tileInfo.layout.numCols);
													indexedTileInfo.push_back(tileInfo);
													startNumber++;
												}
												timeLineIndex++;
											}
										} // end of timeLine loop
										prevStartNumber = startNumber - 1;
									}
									else
									{
										// Segment base.
										AAMPLOG_TRACE("segment template");
										uint64_t tDuration = 0; //duration of 1 tile
										uint64_t duration = segmentTemplates.GetDuration();
										uint32_t imgTimeScale = timeScale ? timeScale : 1;
										long int tNumber = 1; //tile Number
										int totalTiles = 0; // total number of tiles in a period
										std::string RepresentationID = rep->GetId();
										double periodStartTime = MPDParseHelper->GetPeriodStartTime(periodIndex,mpdInstance->mLastPlaylistDownloadTimeMs);
										double periodDuration = MPDParseHelper->aamp_GetPeriodDuration(periodIndex,mpdInstance->mLastPlaylistDownloadTimeMs)/1000;
										tDuration = duration/imgTimeScale;//tile duration
										if(tDuration != 0)
										{
											if (MPDParseHelper->GetLiveTimeFragmentSync())
											{
												startNumber += (long)((periodStartTime - MPDParseHelper->GetAvailabilityStartTime()) / tDuration);
											}
											double tStartTime =  periodStartTime + (tNumber-1)*tDuration;
											totalTiles = periodDuration/tDuration;
											AAMPLOG_TRACE("Thumbnail track total tiles in period = %d , periodStartTime = %f periodDuration = %f ",totalTiles,periodStartTime,periodDuration);

											while(totalTiles-- > 0)
											{
												std::string tmedia = media;
												TileInfo tileInfo;
												memset( &tileInfo,0,sizeof(tileInfo));
												tileInfo.startTime = tStartTime;
												tStartTime += tDuration; //increment the nextStartTime by TileDuration
												replace(tmedia,"RepresentationID",RepresentationID);
												replace(tmedia,"Number",startNumber);
												tileInfo.url = tmedia;
												tileInfo.layout.posterDuration = (tDuration/(w*h));
												tileInfo.layout.tileSetDuration = tDuration;
												tileInfo.layout.numRows = h;
												tileInfo.layout.numCols = w;
												AAMPLOG_TRACE("TileInfo - StartTime:%f posterDuration:%f tileSetDuration:%f numRows:%d numCols:%d url : %s",tileInfo.startTime,tileInfo.layout.posterDuration,tileInfo.layout.tileSetDuration,tileInfo.layout.numRows,tileInfo.layout.numCols,tileInfo.url.c_str());
												indexedTileInfo.push_back(tileInfo);
												startNumber++;
											}
										}
									}
								}
							}
						}// end of representation loop
					}// if content type is IMAGE
				}// end of adaptation set loop
				if((thumbIndexValue < 0) && done)
				{
					break;
				}
				if ( isAdPeriod )
				{
					adDuration += MPDParseHelper->aamp_GetPeriodDuration(periodIndex, 0);
				}
				isAdPeriod = true;
				periodIndex++;
			}	// end of Period loop
		}	// end of thumbnail track size
	}
	AAMPLOG_WARN("Exiting");
	SAFE_DELETE(MPDParseHelper);
}

/**
 * @brief To get the available thumbnail tracks.
 * @ret available thumbnail tracks.
 */
std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableThumbnailTracks(void)
{
	if(thumbnailtrack.empty())
	{
		indexThumbnails(mpd, -1, indexedTileInfo, thumbnailtrack,this);
	}
	return thumbnailtrack;
}

/**
 * @fn SetThumbnailTrack
 * @brief Function to set thumbnail track for processing
 *
 * @return bool true on success.
 */
bool StreamAbstractionAAMP_MPD::SetThumbnailTrack(int thumbnailIndex)
{
	bool ret = false;
	if(aamp->mthumbIndexValue != thumbnailIndex)
	{
		if(thumbnailIndex < thumbnailtrack.size() || thumbnailtrack.empty())
		{
			deIndexTileInfo(indexedTileInfo);
			indexThumbnails(mpd, thumbnailIndex, indexedTileInfo, thumbnailtrack,this);
			if(!indexedTileInfo.empty())
			{
				aamp->mthumbIndexValue = thumbnailIndex;
				ret = true;
			}
		}
	}
	else
	{
		ret = true;
	}
	return ret;
}

/**
 * @fn GetThumbnailRangeData
 * @brief Function to fetch the thumbnail data.
 *
 * @return Updated vector of available thumbnail data.
 */
std::vector<ThumbnailData> StreamAbstractionAAMP_MPD::GetThumbnailRangeData(double tStart, double tEnd, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
	std::vector<ThumbnailData> data;
	if(indexedTileInfo.empty())
	{
		if(aamp->mthumbIndexValue >= 0)
		{
			AAMPLOG_WARN("calling indexthumbnail");
			deIndexTileInfo(indexedTileInfo);
			indexThumbnails(mpd, aamp->mthumbIndexValue, indexedTileInfo, thumbnailtrack,this);
		}
		else
		{
			AAMPLOG_WARN("Exiting. Thumbnail track not configured!!!.");
			return data;
		}
	}

	ThumbnailData tmpdata;
	double totalSetDuration = 0;
	bool updateBaseParam = true;
	for(int i = 0; i< indexedTileInfo.size(); i++)
	{
		TileInfo &tileInfo = indexedTileInfo[i];
		tmpdata.t = tileInfo.startTime;
		if( tmpdata.t > tEnd )
		{
			break;
		}
		double tileSetEndTime = tmpdata.t + tileInfo.layout.tileSetDuration;
		totalSetDuration += tileInfo.layout.tileSetDuration;
		if( tileSetEndTime < tStart )
		{
			continue;
		}
		tmpdata.url = tileInfo.url;
		tmpdata.d = tileInfo.layout.posterDuration;
		bool done = false;
		for( int row=0; row<tileInfo.layout.numRows && !done; row++ )
		{
			for( int col=0; col<tileInfo.layout.numCols && !done; col++ )
			{
				double tNext = tmpdata.t+tileInfo.layout.posterDuration;
				if( tNext >= tileSetEndTime )
				{
					tmpdata.d = tileSetEndTime - tmpdata.t;
					done = true;
				}
				if( tEnd >= tmpdata.t && tStart < tNext  )
				{
					tmpdata.x = col * thumbnailtrack[aamp->mthumbIndexValue]->resolution.width;
					tmpdata.y = row * thumbnailtrack[aamp->mthumbIndexValue]->resolution.height;
					data.push_back(tmpdata);
				}
				tmpdata.t = tNext;
			}
		}
		if(updateBaseParam)
		{
			updateBaseParam = false;
			std::string url = thumbnailtrack[aamp->mthumbIndexValue]->baseUrl;
			if(url.empty())
			{
				url = aamp->GetManifestUrl();
			}
			else
			{
				aamp_ResolveURL(url, aamp->GetManifestUrl(), thumbnailtrack[aamp->mthumbIndexValue]->baseUrl.c_str(), false);
			}
			*baseurl = url.substr(0,url.find_last_of("/\\")+1);
			*width = thumbnailtrack[aamp->mthumbIndexValue]->resolution.width;
			*height = thumbnailtrack[aamp->mthumbIndexValue]->resolution.height;
			*raw_w = thumbnailtrack[aamp->mthumbIndexValue]->resolution.width * tileInfo.layout.numCols;
			*raw_h = thumbnailtrack[aamp->mthumbIndexValue]->resolution.height * tileInfo.layout.numRows;
		}
	}
	return data;
}

/**
 *   @brief  Stops injecting fragments to StreamSink.
 */
void StreamAbstractionAAMP_MPD::StopInjection(void)
{
	//invoked at times of discontinuity. Audio injection loop might have already exited here
	ReassessAndResumeAudioTrack(true);
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track)
		{
			if(track->playContext)
			{
				track->playContext->abort();
			}
			track->AbortWaitForCachedFragment();
			aamp->StopTrackInjection((AampMediaType) iTrack);
			track->StopInjectLoop();
		}
	}
}

/**
 * @brief Send any cached init fragments to be injected on disabled streams to generate the pipeline
 */
void StreamAbstractionAAMP_MPD::SendMediaHeaders()
{
	for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track && !track->Enabled())
		{
			auto header = AampStreamSinkManager::GetInstance().GetMediaHeader(iTrack);
			if(header)
			{
				AAMPLOG_INFO("Track is disabled; url for init segment found: %s", header->url.c_str());
				AampGrowableBuffer buffer("init-buffer");
				std::string effectiveUrl;
				int http_error{};
				if (aamp->GetFile(header->url, (AampMediaType) iTrack, &buffer, effectiveUrl, &http_error, NULL, NULL, eCURLINSTANCE_VIDEO + iTrack))
				{
					aamp->SendStreamTransfer((AampMediaType) iTrack, &buffer, 0, 0, 0, 0, true, false);
				}
				else
				{
					AAMPLOG_ERR("Failed to download init segment: %s", header->url.c_str());
				}
				AampStreamSinkManager::GetInstance().RemoveMediaHeader(iTrack);

				// Update the header with injected set
				header->injected = true;
				AampStreamSinkManager::GetInstance().AddMediaHeader(iTrack, std::move(header));
			}
		}
	}
}

/**
 *   @brief  Start injecting fragments to StreamSink.
 */
void StreamAbstractionAAMP_MPD::StartInjection(void)
{
	mTrackState = eDISCONTINUITY_FREE;

	SendMediaHeaders();
	for (int iTrack = 0; iTrack < mNumberOfTracks; iTrack++)
	{
		MediaStreamContext *track = mMediaStreamContext[iTrack];
		if(track && track->Enabled())
		{
			aamp->ResumeTrackInjection((AampMediaType) iTrack);
			// TODO: This could be moved to StartInjectLoop, but due to lack of testing will keep it here for now
			if(track->playContext)
			{
				track->playContext->reset();
			}
			track->StartInjectLoop();
		}
	}
}


void StreamAbstractionAAMP_MPD::SetCDAIObject(CDAIObject *cdaiObj)
{
	if(cdaiObj)
	{
		CDAIObjectMPD *cdaiObjMpd = static_cast<CDAIObjectMPD *>(cdaiObj);
		mCdaiObject = cdaiObjMpd->GetPrivateCDAIObjectMPD();
	}
}
/**
 *   @brief Check whether the period has any valid ad.
 *
 */
bool StreamAbstractionAAMP_MPD::isAdbreakStart(IPeriod *period, uint64_t &startMS, std::vector<EventBreakInfo> &eventBreakVec)
{
	const std::vector<IEventStream *> &eventStreams = period->GetEventStreams();
	bool ret = false;
	uint32_t duration = 0;
	bool isScteEvent = false;
	for(auto &eventStream: eventStreams)
	{
		cJSON* root = cJSON_CreateObject();
		if(root)
		{
			for(auto &attribute : eventStream->GetRawAttributes())
			{
				cJSON_AddStringToObject(root, attribute.first.c_str(), attribute.second.c_str());
			}
			cJSON *eventsRoot = cJSON_AddArrayToObject(root,"Event");
			for(auto &event: eventStream->GetEvents())
			{
				cJSON *item;
				cJSON_AddItemToArray(eventsRoot, item = cJSON_CreateObject() );
				//Currently for linear assets only the SCTE35 events having 'duration' tag present are considered for generating timedMetadata Events.
				//For VOD assets the events are generated irrespective of the 'duration' tag present or not
				if(event)
				{
					bool eventHasDuration = false;

					for(auto &attribute : event->GetRawAttributes())
					{
						cJSON_AddStringToObject(item, attribute.first.c_str(), attribute.second.c_str());
						if (attribute.first == "duration")
						{
							eventHasDuration = true;
						}
					}

					// Try and get the PTS presentation time from the event (and convert to ms)
					uint64_t presentationTime = event->GetPresentationTime();
					if (presentationTime)
					{
						presentationTime *= 1000;

						uint64_t ts = eventStream->GetTimescale();
						if (ts > 1)
						{
							presentationTime /= ts;
						}
					}

					for(auto &evtChild: event->GetAdditionalSubNodes())
					{
						std::string prefix = "scte35:";
						if(evtChild != NULL)
						{
							if(evtChild->HasAttribute("xmlns") && "http://www.scte.org/schemas/35/2016" == evtChild->GetAttributeValue("xmlns"))
							{
								//scte35 namespace defined here. Hence, this & children don't need the prefix 'scte35'
								prefix = "";
							}

							if(prefix+"Signal" == evtChild->GetName())
							{
								isScteEvent = true;

								bool processEvent = (!mIsLiveManifest || ( mIsLiveManifest && (0 != event->GetDuration())));

								bool modifySCTEProcessing = ISCONFIGSET(eAAMPConfig_EnableSCTE35PresentationTime);
								if (modifySCTEProcessing)
								{
									// Assuming the comment above is correct then we should really check for a duration tag
									// rather than duration=0. We will do this to send all the SCTE events in
									// the manifest even ones with duration=0
									processEvent = (!mIsLiveManifest || eventHasDuration);
								}

								if(processEvent)
								{
									for(auto &signalChild: evtChild->GetNodes())
									{
										if(signalChild && prefix+"Binary" == signalChild->GetName())
										{
											uint32_t timeScale = 1;
											if(eventStream->GetTimescale() > 1)
											{
												timeScale = eventStream->GetTimescale();
											}
											//With the current implementation, ComputeFragmentDuration returns 2000 when the event->GetDuration() returns '0'
											//This is not desirable for VOD assets (for linear event->GetDuration() with 0 value will not bring the control to this point)
											if(0 != event->GetDuration())
											{
												duration = ComputeFragmentDuration(event->GetDuration(), timeScale) * 1000; //milliseconds
											}
											else
											{
												//Control gets here only for VOD with no duration data for the event, here set 0 as duration instead of the default 2000
												duration = 0;
											}
											std::string scte35 = signalChild->GetText();
											if(0 != scte35.length())
											{
												bool isValidDAIEvent = parseAndValidateSCTE35(scte35);
												EventBreakInfo scte35Event(scte35, "SCTE35", presentationTime, duration, isValidDAIEvent);
												eventBreakVec.push_back(scte35Event);

												ret = true;
												continue;
											}
											else
											{
												AAMPLOG_WARN("[CDAI]: Found a scte35:Binary in manifest with empty binary data!!");
											}
										}
										else
										{
											AAMPLOG_WARN("[CDAI]: Found a scte35:Signal in manifest without scte35:Binary!!");
										}
									}
								}
							}
							else
							{
								if(!evtChild->GetName().empty())
								{
									cJSON* childItem;
									cJSON_AddItemToObject(item, evtChild->GetName().c_str(), childItem = cJSON_CreateObject());
									for (auto &signalChild: evtChild->GetNodes())
									{
										if(signalChild && !signalChild->GetName().empty())
										{
											std::string text = signalChild->GetText();
											if(!text.empty())
											{
												cJSON_AddStringToObject(childItem, signalChild->GetName().c_str(), text.c_str());
											}
											for(auto &attributes : signalChild->GetAttributes())
											{
												cJSON_AddStringToObject(childItem, attributes.first.c_str(), attributes.second.c_str());
											}
										}
									}
								}
								else
								{
									cJSON_AddStringToObject(item, "Event", evtChild->GetText().c_str());
								}
							}
						}
						else
						{
							AAMPLOG_WARN("evtChild is null");  //CID:85816 - Null Return
						}
					}
				}
			}
			if(!isScteEvent)
			{
				char* finalData = cJSON_PrintUnformatted(root);
				if(finalData)
				{
					std::string eventStreamStr(finalData);
					cJSON_free(finalData);
					EventBreakInfo eventBreak(eventStreamStr, "EventStream", 0, duration, false);
					eventBreakVec.push_back(eventBreak);
					ret = true;
				}
			}
			cJSON_Delete(root);
		}
	}
	return ret;
}

/*
 * @brief CheckAdResolvedStatus
 *
 * @param[in] ads - Ads vector (optional)
 * @param[in] adIdx - AdIndex (optional)
 * @param[in] periodId - periodId (optional)
 */
void StreamAbstractionAAMP_MPD::CheckAdResolvedStatus(AdNodeVectorPtr &ads, int adIdx, const std::string &periodId)
{
	auto waitForAdResolution = [this](int waitTimeMs, const std::string &id = "") -> bool
	{
		if (!id.empty())
		{
			return this->mCdaiObject->WaitForNextAdResolved(waitTimeMs, id);
		}
		return this->mCdaiObject->WaitForNextAdResolved(waitTimeMs);
	};

	// Wait for some time if the ad is not ready yet. The wait time is calculated based on the buffered duration
	// We will wait for the ad to be resolved before proceeding with the playback with a full timeout for backup buffer
	int waitTimeBasedOnBufferedDuration = std::max(0, (int)(GetBufferedDuration() * 1000) - GETCONFIGVALUE(eAAMPConfig_AdFulfillmentTimeout));
	int waitTimeMs = std::min(waitTimeBasedOnBufferedDuration, GETCONFIGVALUE(eAAMPConfig_AdFulfillmentTimeoutMax));
	AAMPLOG_TRACE("[CDAI]: CheckAdResolvedStatus waitTimeMs[%d] bufferedDuration[%f] AdFulfillmentTimeout[%d] AdFulfillmentTimeoutMax[%d]", waitTimeMs, GetBufferedDuration(), GETCONFIGVALUE(eAAMPConfig_AdFulfillmentTimeout), GETCONFIGVALUE(eAAMPConfig_AdFulfillmentTimeoutMax));
	if (ads && adIdx >= 0)
	{
		if (!ads->at(adIdx).resolved)
		{
			AAMPLOG_INFO("[CDAI]: AdIdx[%d] in the AdBreak[%s] is not resolved yet. Waiting for %d ms.", adIdx, ads->at(adIdx).basePeriodId.c_str(), waitTimeMs);
			if (!waitForAdResolution(waitTimeMs))
			{
				AAMPLOG_INFO("[CDAI]: AdIdx[%d] in the AdBreak[%s] wait timed out", adIdx, ads->at(adIdx).basePeriodId.c_str());
				ads->at(adIdx).invalid = true;
			}
		}
	}
	else if (!periodId.empty() && mCdaiObject->isAdBreakObjectExist(periodId))
	{
		AAMPLOG_INFO("[CDAI]: AdBreak[%s] is not resolved yet. Waiting for %d ms.", periodId.c_str(), waitTimeMs);
		if (!waitForAdResolution(waitTimeMs, periodId))
		{
			AAMPLOG_INFO("[CDAI]: AdBreak[%s] wait timed out", periodId.c_str());
		}
	}
}

/**
 * @brief Handling Ad event
 */
bool StreamAbstractionAAMP_MPD::onAdEvent(AdEvent evt)
{
	double adOffset  = 0.0;
	return onAdEvent(evt, adOffset);
}

bool StreamAbstractionAAMP_MPD::onAdEvent(AdEvent evt, double &adOffset)
{
	AAMPCDAIError adErrorCode = eCDAI_ERROR_NONE;
	if(!ISCONFIGSET(eAAMPConfig_EnableClientDai))
	{
		return false;
	}
	int basePeriodIdx = mMPDParseHelper->getPeriodIdx(mBasePeriodId);
	if(basePeriodIdx != -1)
	{
		if(mMPDParseHelper->IsEmptyPeriod(basePeriodIdx, (rate != AAMP_NORMAL_PLAY_RATE)))
		{
			AAMPLOG_WARN("[CDAI] period [%s] is empty not processing adevents if any",mBasePeriodId.c_str());
			return false;
		}
	}
	std::unique_lock<std::mutex> lock(mCdaiObject->mDaiMtx);
	bool stateChanged = false;
	AdState oldState = mCdaiObject->mAdState;
	AAMPEventType reservationEvt2Send = AAMP_MAX_NUM_EVENTS; //None
	/* Caching the currently playing breakId */
	std::string adbreakId2Send = mCdaiObject->mCurPlayingBreakId;
	AAMPEventType placementEvt2Send = AAMP_MAX_NUM_EVENTS; //None
	std::string adId2Send("");
	uint32_t adPos2Send = 0;
	bool sendImmediate = false;
	switch(mCdaiObject->mAdState)
	{
		case AdState::OUTSIDE_ADBREAK:
		case AdState::OUTSIDE_ADBREAK_WAIT4ADS:
			// Default event state or Idle event state is OUTSIDE_ADBREAK
			if(AdEvent::DEFAULT == evt || AdEvent::INIT == evt)
			{
				// Getting called from StreamAbstractionAAMP_MPD::Init or from FetcherLoop
				std::string brkId = "";
				int adIdx = mCdaiObject->CheckForAdStart(rate, (AdEvent::INIT == evt), mBasePeriodId, mBasePeriodOffset, brkId, adOffset);
				// If an adbreak is found for period
				if(!brkId.empty())
				{
					AAMPLOG_INFO("[CDAI] CheckForAdStart found Adbreak[%s] adIdx[%d] mBasePeriodOffset[%lf] adOffset[%lf] SeekOffset:%f.", brkId.c_str(), adIdx, mBasePeriodOffset, adOffset,mCdaiObject->mContentSeekOffset);
					mCdaiObject->mCurPlayingBreakId = brkId;

					// If an ad is found, for the periodOffset
					if(-1 != adIdx && mCdaiObject->mAdBreaks[brkId].ads)
					{
						//Setting  mContentSeekOffset as 0 if player is going to play DAI ad.
						//Otherwise, player will skip initial fragments of DAI ad  sometimes due to the seekoffset.
						//Dai ad playback should start from first fragment.
						mCdaiObject->mContentSeekOffset = 0;

						lock.unlock();
						// Check if ad is resolved, if not wait
						CheckAdResolvedStatus(mCdaiObject->mAdBreaks[brkId].ads, adIdx);
						lock.lock();

						// If the ad is not invalid (failed to reserve), start the ad playback
						if(!(mCdaiObject->mAdBreaks[brkId].ads->at(adIdx).invalid))
						{
							AAMPLOG_WARN("[CDAI]: STARTING ADBREAK[%s] AdIdx[%d] Found at Period[%s].", brkId.c_str(), adIdx, mBasePeriodId.c_str());
							mCdaiObject->mCurAds = mCdaiObject->mAdBreaks[brkId].ads;

							mCdaiObject->mCurAdIdx = adIdx;
							mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

							for(int i=0; i<adIdx; i++)
							{
								adPos2Send += mCdaiObject->mCurAds->at(i).duration;
							}
						}
						else
						{
							AAMPLOG_WARN("[CDAI]: AdIdx[%d] in the AdBreak[%s] is invalid. Skipping.", adIdx, brkId.c_str());
						}
						reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_START;
						adbreakId2Send = brkId;
						if(AdEvent::INIT == evt)
						{
							sendImmediate = true;
						}
						if(AdState::IN_ADBREAK_AD_PLAYING != mCdaiObject->mAdState)
						{
							AAMPLOG_WARN("[CDAI]: BasePeriodId in Adbreak. But Ad not available. BasePeriodId[%s],Adbreak[%s]", mBasePeriodId.c_str(), brkId.c_str());
							mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
						}
						stateChanged = true;
					}
					else
					{
						// On rewind, if the ad is not found at the offset, it could also be a partial ads
						if (rate < AAMP_RATE_PAUSE)
						{
							// This will ensure that we play the ads once the offset reaches a position where ad is available
							mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
							stateChanged = true;
						}
						// If an adbreak exists for this basePeriodId, then ads might be available.
						// Only for scenarios where mBasePeriodOffset is zero, we have to wait for the ads to be added by application.
						// Otherwise for partial ad fill, once the ads are played, we will wait as adIdx will be -1 from CheckForAdStart().
						else if (mBasePeriodOffset == 0)
						{
							// If the adbreak is not invalidated, wait for ads to be added and resolved
							if ((mCdaiObject->mAdState != AdState::OUTSIDE_ADBREAK_WAIT4ADS) && !mCdaiObject->mAdBreaks[mBasePeriodId].invalid)
							{
								mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK_WAIT4ADS;
								stateChanged = true;
								if(mCdaiObject->mAdBreaks[brkId].ads && mCdaiObject->mAdBreaks[brkId].ads->size() > 0)
								{
									AAMPLOG_WARN("[CDAI] ads.size() = %zu breakId = %s mBasePeriodId = %s", mCdaiObject->mAdBreaks[brkId].ads->size(), brkId.c_str(), mBasePeriodId.c_str());
									// We failed to get an ad as its invalid, so we can switch to source content now
									if (mCdaiObject->mAdBreaks[brkId].ads->at(0).invalid)
									{
										mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
									}
								}
								else
								{
									lock.unlock();
									// Wait for some time for the ads to be added
									CheckAdResolvedStatus(mCdaiObject->mAdBreaks[mBasePeriodId].ads, -1, mBasePeriodId);
									lock.lock();
								}
							}
							else
							{
								// Ads are not added or ads failed to resolve, invalidate the adbreak
								AAMPLOG_WARN("[CDAI] AdBreak[%s] is invalidated. Skipping.", mBasePeriodId.c_str());
								if(AdState::OUTSIDE_ADBREAK_WAIT4ADS == mCdaiObject->mAdState)
								{
									stateChanged = true;
									mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
								}
							}
						}
					}
				}
				else
				{
					AAMPLOG_INFO("[CDAI] No AdBreak found for period[%s] at offset:%lf", mBasePeriodId.c_str(), mBasePeriodOffset);
					if (AdState::OUTSIDE_ADBREAK_WAIT4ADS == mCdaiObject->mAdState)
					{
						stateChanged = true;
						mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
					}
				}
			}
			break;
		case AdState::IN_ADBREAK_AD_NOT_PLAYING:
			if(AdEvent::BASE_OFFSET_CHANGE == evt || AdEvent::PERIOD_CHANGE == evt)
			{
				std::string brkId = "";
				int adIdx = mCdaiObject->CheckForAdStart(rate, false, mBasePeriodId, mBasePeriodOffset, brkId, adOffset);
				if(-1 != adIdx && mCdaiObject->mAdBreaks[brkId].ads)
				{
					if (rate >= AAMP_NORMAL_PLAY_RATE)
					{
						// Wait for some time if the ad is not ready yet.
						lock.unlock();
						CheckAdResolvedStatus(mCdaiObject->mAdBreaks[brkId].ads, adIdx);
						lock.lock();
					}
					if(!(mCdaiObject->mAdBreaks[brkId].ads->at(adIdx).invalid))
					{
						AAMPLOG_WARN("[CDAI]: AdIdx[%d] Found at Period[%s].", adIdx, mBasePeriodId.c_str());
						mCdaiObject->mCurAds = mCdaiObject->mAdBreaks[brkId].ads;

						mCdaiObject->mCurAdIdx = adIdx;
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

						for(int i=0; i<adIdx; i++)
						{
							adPos2Send += mCdaiObject->mCurAds->at(i).duration;
						}
						stateChanged = true;
					}
					if(adIdx == (mCdaiObject->mAdBreaks[brkId].ads->size() -1))	//Rewind case only.
					{
						reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_START;
						adbreakId2Send = brkId;
					}
				}
				else if(brkId.empty())
				{
					AAMPLOG_WARN("[CDAI]: ADBREAK[%s] ENDED. Playing the basePeriod[%s].", mCdaiObject->mCurPlayingBreakId.c_str(), mBasePeriodId.c_str());
					mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].mAdFailed = false;
					mCdaiObject->mCurPlayingBreakId = "";
					mCdaiObject->mCurAds = nullptr;
					mCdaiObject->mCurAdIdx = -1;
					//Base content playing already. No need to jump to offset again.
					mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
					stateChanged = true;
				}
			}
			break;
		case AdState::IN_ADBREAK_AD_PLAYING:
			if(AdEvent::AD_FINISHED == evt)
			{
				AAMPLOG_WARN("[CDAI]: Ad finished at Period. Waiting to catchup the base offset.[idx=%d] [period=%s]", mCdaiObject->mCurAdIdx, mBasePeriodId.c_str());
				mCdaiObject->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;

				placementEvt2Send = AAMP_EVENT_AD_PLACEMENT_END;
				AdNode &adNode =  mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
				adId2Send = adNode.adId;
				for(int i=0; i <= mCdaiObject->mCurAdIdx; i++)
					adPos2Send += mCdaiObject->mCurAds->at(i).duration;
				stateChanged = true;
			}
			else if(AdEvent::AD_FAILED == evt)
			{
				mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid = true;
				AAMPLOG_WARN("[CDAI]: Ad Playback failed. Going to the base period[%s] at offset[%lf].Ad[idx=%d]", mBasePeriodId.c_str(), mBasePeriodOffset,mCdaiObject->mCurAdIdx);
				mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING; //TODO: Vinod, It should be IN_ADBREAK_WAIT2CATCHUP, But you need to fix the catchup check logic.

				placementEvt2Send = AAMP_EVENT_AD_PLACEMENT_ERROR;	//Followed by AAMP_EVENT_AD_PLACEMENT_END
				AdNode &adNode =  mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
				adId2Send = adNode.adId;
				sendImmediate = true;
				adPos2Send = 0; //TODO: Vinod, Fix it
				stateChanged = true;
			}

			if(stateChanged)
			{
				for (int iTrack = 0; iTrack < mMaxTracks; iTrack++)
				{
					//after the ad placement, replacing ad manifest url to source manifest url for all the tracks in MediaStreamContext.
					MediaStreamContext *track = mMediaStreamContext[iTrack];
					if(track)
					{
						mMediaStreamContext[iTrack]->fragmentDescriptor.manifestUrl = aamp->GetManifestUrl();
					}
				}
			}
			break;
		case AdState::IN_ADBREAK_WAIT2CATCHUP:
			if(-1 == mCdaiObject->mCurAdIdx)
			{
				AAMPLOG_WARN("[CDAI]: BUG! BUG!! BUG!!! We should not come here.AdIdx[-1].");
				mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].mAdFailed = false;
				mCdaiObject->mCurPlayingBreakId = "";
				mCdaiObject->mCurAds = nullptr;
				mCdaiObject->mCurAdIdx = -1;
				mCdaiObject->mContentSeekOffset = mBasePeriodOffset;
				mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;
				stateChanged = true;
				break;
			}
			//In every event, we need to check this.But do it only on the beginning of the fetcher loop. Hence it is the default event
			if(AdEvent::DEFAULT == evt)
			{
				// For rewind cases, we don't need to wait for the ad to get placed. The below TODO says otherwise,
				// but not seeing any use of base period offset for rewind in the below logic
				if ((rate > 0) &&
					!(mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).placed)) //TODO: Vinod, Need to wait till the base period offset is available. 'placed' won't help in case of rewind.
				{
					break;
				}
				//Wait till placement of current ad is completed
				AAMPLOG_WARN("[CDAI]: Current Ad placement Completed. Ready to play next Ad.");
				mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_READY2PLAY;
			}
		case AdState::IN_ADBREAK_AD_READY2PLAY:
			if(AdEvent::DEFAULT == evt)
			{
				bool curAdFailed = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid;	//TODO: Vinod, may need to check boundary.

				GetNextAdInBreak((rate >= AAMP_NORMAL_PLAY_RATE) ? 1 : -1);

				if(mCdaiObject->mCurAdIdx >= 0 && mCdaiObject->mCurAdIdx < mCdaiObject->mCurAds->size())
				{
					// Wait for some time if the ad is not ready yet.
					lock.unlock();
					CheckAdResolvedStatus(mCdaiObject->mCurAds, mCdaiObject->mCurAdIdx);
					lock.lock();
					if(mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid)
					{
						AAMPLOG_WARN("[CDAI]: AdIdx is invalid. Skipping. AdIdx[%d].", mCdaiObject->mCurAdIdx);
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
					}
					else
					{
						AAMPLOG_WARN("[CDAI]: Next AdIdx[%d] Found at Period[%s].", mCdaiObject->mCurAdIdx, mBasePeriodId.c_str());
						mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_PLAYING;

						for(int i=0; i<mCdaiObject->mCurAdIdx; i++)
							adPos2Send += mCdaiObject->mCurAds->at(i).duration;
					}
					stateChanged = true;
				}
				else
				{
					if(rate > 0)
					{
						// For forward playback, we need to wait till the adbreak is placed. Then only we will know which period to change to
						if (!mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].mAdBreakPlaced)
						{
							// This log might cause log flooding, hence changed to trace
							AAMPLOG_TRACE("[CDAI]: All Ads in the ADBREAK[%s] FINISHED. Waiting for the ADBREAK to place.", mCdaiObject->mCurPlayingBreakId.c_str());
							// Moving the state back to IN_ADBREAK_WAIT2CATCHUP, so that we can further wait till adbreak is placed
							mCdaiObject->mAdState = AdState::IN_ADBREAK_WAIT2CATCHUP;
							if (mCdaiObject->mCurAdIdx >= mCdaiObject->mCurAds->size())
							{
								// Change to index back to valid range
								mCdaiObject->mCurAdIdx--;
							}
							stateChanged = false;
							break;
						}

						mBasePeriodId =	mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].endPeriodId;
						mCdaiObject->mContentSeekOffset = (double)(mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].endPeriodOffset)/ 1000;
					}
					else
					{
						// mCdaiObject->mCurPlayingBreakId is the first period in the Adbreak. Set the previous period as mBasePeriodId to play
						std::string prevPId = "";
						size_t mNumberOfPeriods = mMPDParseHelper->GetNumberOfPeriods();
						for(size_t mIterPeriodIndex=0;mIterPeriodIndex < mNumberOfPeriods;  mIterPeriodIndex++)
						{
							const std::string &pId = mpd->GetPeriods().at(mIterPeriodIndex)->GetId();
							if(mCdaiObject->mCurPlayingBreakId == pId)
							{
								break;
							}
							prevPId = pId;
						}
						if(!prevPId.empty())
						{
							mBasePeriodId = prevPId;
						} //else, it should play the mBasePeriodId
						mCdaiObject->mContentSeekOffset = 0; //Should continue tricking from the end of the previous period.
					}
					AAMPLOG_WARN("[CDAI]: All Ads in the ADBREAK[%s] FINISHED. Playing the basePeriod[%s] at Offset[%lf].", mCdaiObject->mCurPlayingBreakId.c_str(), mBasePeriodId.c_str(), mCdaiObject->mContentSeekOffset);
					mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].mAdFailed = false;
					reservationEvt2Send = AAMP_EVENT_AD_RESERVATION_END;
					sendImmediate = curAdFailed;	//Current Ad failed. Hence may not get discontinuity from gstreamer.
					mCdaiObject->mCurPlayingBreakId = "";
					mCdaiObject->mCurAds = nullptr;
					mCdaiObject->mCurAdIdx = -1;
					mCdaiObject->mAdState = AdState::OUTSIDE_ADBREAK;	//No more offset check needed. Hence, changing to OUTSIDE_ADBREAK
					stateChanged = true;
				}
			}
			break;
		default:
			break;
	}
	if(stateChanged)
	{
		mAdPlayingFromCDN = false;
		bool fogManifestFailed = false;
		if(AdState::IN_ADBREAK_AD_PLAYING == mCdaiObject->mAdState)
		{
			AdNode &adNode = mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx);
			if(NULL == adNode.mpd)
			{
				//Need to ensure that mpd is available, if not available, download it (mostly from FOG)
				bool finalManifest = false;
				int http_error = 0;
				double downloadTime = 0;
				adNode.mpd = mCdaiObject->GetAdMPD(adNode.url, finalManifest, http_error, downloadTime, adErrorCode, false);
				if(CURLE_ABORTED_BY_CALLBACK == http_error)
				{
					AAMPLOG_WARN("[CDAI]: Ad playback failed. Not able to download Ad manifest. Aborted by callback.");
				}
				if(NULL == adNode.mpd)
				{
					AAMPLOG_WARN("[CDAI]: Ad playback failed. Not able to download Ad manifest from FOG.");
					mCdaiObject->mAdState = AdState::IN_ADBREAK_AD_NOT_PLAYING;
					fogManifestFailed = true;
					if(AdState::IN_ADBREAK_AD_NOT_PLAYING == oldState)
					{
						stateChanged = false;
					}
				}
			}
			if(adNode.mpd)
			{
				mCurrentPeriod = adNode.mpd->GetPeriods().at(0);
				/* TODO: Fix redundancy from UpdateTrackInfo */
				for (int i = 0; i < mNumberOfTracks; i++)
				{
					mMediaStreamContext[i]->fragmentDescriptor.manifestUrl = adNode.url.c_str();
				}

				placementEvt2Send = AAMP_EVENT_AD_PLACEMENT_START;
				adId2Send = adNode.adId;

				map<string, string> mpdAttributes = adNode.mpd->GetRawAttributes();
				if(mpdAttributes.find("fogtsb") == mpdAttributes.end())
				{
					//No attribute 'fogtsb' in MPD. Hence, current ad is from CDN
					mAdPlayingFromCDN = true;
				}
			}
		}

		if(stateChanged)
		{
			AAMPLOG_WARN("[CDAI]: State changed from [%s] => [%s].", ADSTATE_STR[static_cast<int>(oldState)],ADSTATE_STR[static_cast<int>(mCdaiObject->mAdState)]);
		}

		if(AAMP_NORMAL_PLAY_RATE == rate)
		{
			//Sending Ad events
			uint64_t resPosMS = 0;
			AampTime absReservationEventPosition;
			AampTime absPlacementEventPosition;
			// Check if the adbreakId is valid
			if (!adbreakId2Send.empty() && mCdaiObject->isAdBreakObjectExist(adbreakId2Send))
			{
				AdBreakObject *abObj = &mCdaiObject->mAdBreaks[adbreakId2Send];
				// Updating the adbreak start time for the first time when the adbreak starts in the current period
				if (abObj)
				{
					if (abObj->mAbsoluteAdBreakStartTime == 0.0)
					{
						abObj->mAbsoluteAdBreakStartTime = mMPDParseHelper->GetPeriodStartTime(mCurrentPeriodIdx, mLastPlaylistDownloadTimeMs);
					}
					absReservationEventPosition = abObj->mAbsoluteAdBreakStartTime;
					absPlacementEventPosition = abObj->mAbsoluteAdBreakStartTime;
				}
			}
			if(AAMP_EVENT_AD_RESERVATION_START == reservationEvt2Send || AAMP_EVENT_AD_RESERVATION_END == reservationEvt2Send)
			{
				const std::string &startStr = mpd->GetPeriods().at(mCurrentPeriodIdx)->GetStart();
				if(!startStr.empty())
				{
					resPosMS = ParseISO8601Duration(startStr.c_str() );
				}
				resPosMS += (uint64_t)(mBasePeriodOffset * 1000);
				absReservationEventPosition += mBasePeriodOffset;
			}

			if(AAMP_EVENT_AD_RESERVATION_START == reservationEvt2Send)
			{
				SendAdReservationEvent(reservationEvt2Send, adbreakId2Send, resPosMS, absReservationEventPosition, sendImmediate);
				aamp->SendAnomalyEvent(ANOMALY_TRACE, "[CDAI] Adbreak of duration=%u sec starts.", (mCdaiObject->mAdBreaks[mCdaiObject->mCurPlayingBreakId].brkDuration)/1000);
			}

			if(AAMP_EVENT_AD_PLACEMENT_START == placementEvt2Send || AAMP_EVENT_AD_PLACEMENT_END == placementEvt2Send || AAMP_EVENT_AD_PLACEMENT_ERROR == placementEvt2Send)
			{
				uint32_t adDuration = 30000;
				if(AAMP_EVENT_AD_PLACEMENT_START == placementEvt2Send)
				{
					adDuration = (uint32_t)mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).duration;
					adPos2Send += adOffset;
					aamp->SendAnomalyEvent(ANOMALY_TRACE, "[CDAI] AdId=%s starts. Duration=%u sec URL=%s",
						adId2Send.c_str(),(adDuration/1000), mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).url.c_str());
				}
				absPlacementEventPosition += adPos2Send / 1000.0;

				SendAdPlacementEvent(placementEvt2Send, adId2Send, adPos2Send, absPlacementEventPosition, adOffset, adDuration, sendImmediate);

				if(fogManifestFailed)
				{
					SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_ERROR, adId2Send, adPos2Send, absPlacementEventPosition, adOffset, adDuration, true);
				}
				if(AAMP_EVENT_AD_PLACEMENT_ERROR == placementEvt2Send || fogManifestFailed)
				{
					SendAdPlacementEvent(AAMP_EVENT_AD_PLACEMENT_END, adId2Send, adPos2Send, absPlacementEventPosition, adOffset, adDuration, true);	//Ad ended with error
					aamp->SendAnomalyEvent(ANOMALY_ERROR, "[CDAI] AdId=%s encountered error.", adId2Send.c_str());
				}
			}

			if(AAMP_EVENT_AD_RESERVATION_END == reservationEvt2Send)
			{
				SendAdReservationEvent(reservationEvt2Send, adbreakId2Send, resPosMS, absReservationEventPosition, sendImmediate);
				aamp->SendAnomalyEvent(ANOMALY_TRACE, "%s", "[CDAI] Adbreak ends.");
			}

		}
	}
	return stateChanged;
}

/**
 * @brief Send Ad reservation event
 */
void StreamAbstractionAAMP_MPD::SendAdReservationEvent(AAMPEventType type, const std::string &adBreakId, uint64_t position, AampTime absolutePosition, bool sendImmediate)
{
	AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
	bool isLocalAAMPTsbInjection = false;

	if (tsbSessionManager)
	{
		isLocalAAMPTsbInjection = aamp->IsLocalAAMPTsbInjection();

		if(AAMP_EVENT_AD_RESERVATION_START == type)
		{
			AAMPLOG_INFO("[CDAI]: Add to TSB, Ad Reservation Start Id %s AbsPos %" PRIu64 " Pos %" PRIu64 " SendImmediate %d", adBreakId.c_str(), absolutePosition.milliseconds(), position, sendImmediate);
			tsbSessionManager->StartAdReservation(adBreakId, position, absolutePosition);
		}
		if(AAMP_EVENT_AD_RESERVATION_END == type)
		{
			AAMPLOG_INFO("[CDAI]: Add to TSB, Ad Reservation End Id %s AbsPos %" PRIu64 " Pos %" PRIu64 " SendImmediate %d", adBreakId.c_str(), absolutePosition.milliseconds(), position, sendImmediate);
			tsbSessionManager->EndAdReservation(adBreakId, position, absolutePosition);
		}
		if (sendImmediate)
		{
			tsbSessionManager->ShiftFutureAdEvents();
		}
	}

	AAMPLOG_INFO("tsbSessionManager %p isLocalAAMPTsbInjection %d", tsbSessionManager, isLocalAAMPTsbInjection);

	if(!isLocalAAMPTsbInjection)
	{
		if(AAMP_EVENT_AD_RESERVATION_START == type)
		{
			AAMPLOG_INFO("[CDAI]: Sending Ad Reservation Start Event. AdBreakId %s ResPos %" PRIu64 " AbsPos %" PRIu64 " SendImmediate %d", adBreakId.c_str(), position, absolutePosition.milliseconds(), sendImmediate);
		}
		if(AAMP_EVENT_AD_RESERVATION_END == type)
		{
			AAMPLOG_INFO("[CDAI]: AdBreak[%s] ended. resPosMS[%" PRIu64 "] absReservationEventPosition[%" PRIu64 "]", adBreakId.c_str(), position, absolutePosition.milliseconds());
		}
		aamp->SendAdReservationEvent(type, adBreakId, position, absolutePosition.milliseconds(), sendImmediate);
	}
}

/**
 * @brief Send Ad placement event
 */
void StreamAbstractionAAMP_MPD::SendAdPlacementEvent(AAMPEventType type, const std::string &adId, uint32_t position, AampTime absolutePosition, uint32_t adOffset, uint32_t adDuration, bool sendImmediate)
{
	AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
	bool isLocalAAMPTsbInjection = false;

	if (tsbSessionManager)
	{
		isLocalAAMPTsbInjection = aamp->IsLocalAAMPTsbInjection();

		if(AAMP_EVENT_AD_PLACEMENT_START == type)
		{
			AAMPLOG_INFO("[CDAI]: Add to TSB, Ad Placement Start Id %s AbsPos %" PRIu64 " Duration %u AdPos %" PRIu32 " adOffset %u sendImmediate %d", adId.c_str(), absolutePosition.milliseconds(), adDuration, position, adOffset, sendImmediate);
			tsbSessionManager->StartAdPlacement(adId, position, absolutePosition, adDuration, adOffset);
		}
		else if(AAMP_EVENT_AD_PLACEMENT_END == type)
		{
			AAMPLOG_INFO("[CDAI]: Add to TSB, Ad Placement End Id %s AbsPos %" PRIu64 " Duration %u AdPos %" PRIu32 " adOffset %u sendImmediate %d", adId.c_str(), absolutePosition.milliseconds(), adDuration, position, adOffset, sendImmediate);
			tsbSessionManager->EndAdPlacement(adId, position, absolutePosition, adDuration, adOffset);
		}
		else if(AAMP_EVENT_AD_PLACEMENT_ERROR == type)
		{
			AAMPLOG_INFO("[CDAI]: Add to TSB, Error during Ad Placement Id %s AbsPos %" PRIu64 " Duration %u AdPos %" PRIu32 " adOffset %u sendImmediate %d", adId.c_str(), absolutePosition.milliseconds(), adDuration, position, adOffset, sendImmediate);
			tsbSessionManager->EndAdPlacementWithError(adId, position, absolutePosition, adDuration, adOffset);
		}
		else
		{
			AAMPLOG_ERR("[CDAI]: Unrecognised type %d", type);
		}
		if (sendImmediate)
		{
			tsbSessionManager->ShiftFutureAdEvents();
		}
	}

	AAMPLOG_INFO("tsbSessionManager %p isLocalAAMPTsbInjection %d", tsbSessionManager, isLocalAAMPTsbInjection);

	if(!isLocalAAMPTsbInjection)
	{
		AAMPLOG_INFO("[CDAI]: AdId[%s] AdPos[ %" PRIu32 "] absPlacementEventPositionMs[%" PRIu64 "] adOffset[%u] adDuration[%u] sendImmediate[%d]", adId.c_str(), position, absolutePosition.milliseconds(), adOffset, adDuration, sendImmediate);
		aamp->SendAdPlacementEvent(type, adId, position, absolutePosition.milliseconds(), adOffset, adDuration, sendImmediate);
	}
}


/**
 * @brief Print the current the track information
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::printSelectedTrack(const std::string &trackIndex, AampMediaType media)
{
	if (!trackIndex.empty())
	{
		if (media == eMEDIATYPE_AUDIO)
		{
			for (auto &audioTrack : mAudioTracks)
			{
				if (audioTrack.index == trackIndex)
				{
					AAMPLOG_INFO("Selected Audio Track: Index:%s language:%s rendition:%s name:%s label:%s type:%s codec:%s bandwidth:%ld Channel:%d Accessibility:%s ",
					audioTrack.index.c_str(), audioTrack.language.c_str(), audioTrack.rendition.c_str(), audioTrack.name.c_str(),
					audioTrack.label.c_str(), audioTrack.mType.c_str(), audioTrack.codec.c_str(),
					audioTrack.bandwidth, audioTrack.channels, audioTrack.accessibilityItem.print().c_str());
					break;
				}
			}
		}
		else if (media == eMEDIATYPE_SUBTITLE)
		{
			for (auto &textTrack : mTextTracks)
			{
				if (textTrack.index == trackIndex)
				{
					AAMPLOG_INFO("Selected Text Track: Index:%s language:%s rendition:%s name:%s label:%s type:%s codec:%s isCC:%d Accessibility:%s ",
					textTrack.index.c_str(), textTrack.language.c_str(), textTrack.rendition.c_str(), textTrack.name.c_str(),
					textTrack.label.c_str(), textTrack.mType.c_str(), textTrack.codec.c_str(), textTrack.isCC, textTrack.accessibilityItem.print().c_str());
					break;
				}
			}
		}
	}
}

/**
 * @brief To set the audio tracks of current period
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::SetAudioTrackInfo(const std::vector<AudioTrackInfo> &tracks, const std::string &trackIndex)
{
	bool tracksChanged = false;
	int audioIndex = -1;

	mAudioTracks = tracks;
	mAudioTrackIndex = trackIndex;
	audioIndex = GetAudioTrack();
	if (-1 != aamp->mCurrentAudioTrackIndex
			&& aamp->mCurrentAudioTrackIndex != audioIndex)
	{
		tracksChanged = true;
	}
	aamp->mCurrentAudioTrackIndex = audioIndex;

	if (tracksChanged)
	{
		aamp->NotifyAudioTracksChanged();
	}
}

/** TBD : Move to Dash Utils */
#define PRESELECTION_PROPERTY_TAG "Preselection"
#define ACCESSIBILITY_PROPERTY_TAG "Accessibility"

#define CHANNEL_PROPERTY_TAG "AudioChannelConfiguration"
#define CHANNEL_SCHEME_ID_TAG "urn:mpeg:mpegB:cicp:ChannelConfiguration"

#define ROLE_PROPERTY_TAG "Role"
#define ROLE_SCHEME_ID_TAG "urn:mpeg:dash:role:2011"

/** TBD : Move to Dash Utils */
/**
 * @brief Get the cannel number from preselection node
 * @param preselection ndoe as nodePtr
 * @return channel number
 */
static int getChannel(INode *nodePtr)
{
	int channel = 0;
	std::vector<INode*> childNodeList = nodePtr->GetNodes();
	for (auto &childNode : childNodeList)
	{
		const std::string& name = childNode->GetName();
		if (name == CHANNEL_PROPERTY_TAG )
		{
			if (childNode->HasAttribute("schemeIdUri"))
			{
				if (childNode->GetAttributeValue("schemeIdUri") == CHANNEL_SCHEME_ID_TAG )
				{
					if (childNode->HasAttribute("value"))
					{
						channel = std::stoi(childNode->GetAttributeValue("value"));
					}
				}
			}
		}
	}

	return channel;
}

/**
 * @fn getRole
 *
 * @brief Get the role from preselection node
 * @param preselection ndoe as nodePtr
 * @return role
 */
static std::string getRole(INode *nodePtr)
{
	std::string role = "";
	std::vector<INode*> childNodeList = nodePtr->GetNodes();
	for (auto &childNode : childNodeList)
	{
		const std::string& name = childNode->GetName();
		if (name == ROLE_PROPERTY_TAG )
		{
			if (childNode->HasAttribute("schemeIdUri"))
			{
				if (childNode->GetAttributeValue("schemeIdUri") == ROLE_SCHEME_ID_TAG )
				{
					if (childNode->HasAttribute("value"))
					{
						role = childNode->GetAttributeValue("value");
					}
				}
			}
		}
	}
	return role;
}

void StreamAbstractionAAMP_MPD::ParseAvailablePreselections(IMPDElement *period, std::vector<AudioTrackInfo> & audioAC4Tracks)
{
	std::vector<INode*> childNodeList = period->GetAdditionalSubNodes();
	if (childNodeList.size() > 0)
	{
		std::string id ;
		std::string codec;
		std::string lang;
		std::string tag ;
		long bandwidth = 0;
		std::string role;
		int channel = 0;
		std::string label;
		std::string type = "audio";
		for (auto &childNode : childNodeList)
		{
			const std::string& name = childNode->GetName();
			if (name == PRESELECTION_PROPERTY_TAG )
			{
				/**
				 * <Preselection id="100" preselectionComponents="11"
				 * tag="10" codecs="ac-4.02.01.01" audioSamplingRate="48000" lang="en">
				 */
				if (childNode->HasAttribute("id")) {
					id = childNode->GetAttributeValue("id");
				}
				if (childNode->HasAttribute("tag")) {
					tag = childNode->GetAttributeValue("tag");
				}
				if (childNode->HasAttribute("lang")) {
					lang = childNode->GetAttributeValue("lang");
				}
				if (childNode->HasAttribute("codecs")) {
					codec = childNode->GetAttributeValue("codecs");
				}
				if (childNode->HasAttribute("audioSamplingRate")) {
					bandwidth = std::stol(childNode->GetAttributeValue("audioSamplingRate"));
				}

				role = getRole (childNode);
				channel = getChannel(childNode);
				/** Preselection node is used for representing muxed audio tracks **/
				AAMPLOG_INFO("Preselection node found with tag %s language %s role %s id %s codec %s bandwidth %ld Channel %d ",
				tag.c_str(), lang.c_str(), role.c_str(), id.c_str(), codec.c_str(), bandwidth, channel);
				audioAC4Tracks.push_back(AudioTrackInfo(tag, lang, role, id, codec, bandwidth, channel, true, true));
			}
		}
	}
}

/**
 * @brief Get the audio track information from all period
 *        updated member variable mAudioTracksAll
 * @return void
 */
void StreamAbstractionAAMP_MPD::PopulateTrackInfo(AampMediaType media, bool reset)
{
	//Clear the current track info , if any
	if (reset && media == eMEDIATYPE_AUDIO)
	{
		mAudioTracksAll.clear();
	}
	/**< Subtitle can be muxed with video adaptation also **/
	else if (reset && ((media == eMEDIATYPE_SUBTITLE) || (media == eMEDIATYPE_VIDEO)))
	{
		mTextTracksAll.clear();
	}
	std::vector<dash::mpd::IPeriod*>  ptrPeriods =  mpd->GetPeriods();
	for (auto &period : ptrPeriods)
	{
		AAMPLOG_TRACE("Traversing Period [%s] ", period->GetId().c_str());

		std::vector<dash::mpd::IAdaptationSet*> adaptationSets = period->GetAdaptationSets();
		uint32_t adaptationIndex = 0;
		for (auto &adaptationSet : adaptationSets)
		{
			AAMPLOG_TRACE("Adaptation Set Content type [%s] ", adaptationSet->GetContentType().c_str());
			if (mMPDParseHelper->IsContentType(adaptationSet, (AampMediaType)media))
			{
				ParseTrackInformation(adaptationSet, adaptationIndex, (AampMediaType)media,  mAudioTracksAll, mTextTracksAll);
			} // Audio Adaptation
			adaptationIndex++;
		} // Adaptation Loop
		{
			std::vector<AudioTrackInfo> ac4Tracks;
			ParseAvailablePreselections(period, ac4Tracks);
			mAudioTracksAll.insert(mAudioTracksAll.end(), ac4Tracks.begin(), ac4Tracks.end());
		}
	}//Period loop
	if (media == eMEDIATYPE_AUDIO)
	{
		std::sort(mAudioTracksAll.begin(), mAudioTracksAll.end());
		auto last = std::unique(mAudioTracksAll.begin(), mAudioTracksAll.end());
		// Resizing the vector so as to remove the undefined terms
		mAudioTracksAll.resize(std::distance(mAudioTracksAll.begin(), last));
	}
	else
	{
		std::sort(mTextTracksAll.begin(), mTextTracksAll.end());
		auto last = std::unique(mTextTracksAll.begin(), mTextTracksAll.end());
		// Resizing the vector so as to remove the undefined terms
		mTextTracksAll.resize(std::distance(mTextTracksAll.begin(), last));
	}
}

/**
 * @brief To set the audio tracks of current period
 */
std::vector<AudioTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableAudioTracks(bool allTrack)
{
	if (!allTrack)
	{
		return mAudioTracks;
	}
	else
	{
		if (aamp->IsLive() || (mAudioTracksAll.size() == 0))
		{
			/** Audio Track not populated yet**/
			PopulateTrackInfo(eMEDIATYPE_AUDIO, true);
		}
		return mAudioTracksAll;
	}
}

/**
 * @brief To set the text tracks of current period
 *
 * @param[in] tracks - available text tracks in period
 * @param[in] trackIndex - index of current text track
 */
std::vector<TextTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableTextTracks(bool allTrack)
{
	if (!allTrack)
	{
		return mTextTracks;
	}
	else
	{
		if (aamp->IsLive() || (mTextTracksAll.size() == 0))
		{
			/** Text Track not populated yet**/
			PopulateTrackInfo(eMEDIATYPE_SUBTITLE, true);
			PopulateTrackInfo(eMEDIATYPE_VIDEO, false);
		}
		return mTextTracksAll;
	}
}

void StreamAbstractionAAMP_MPD::SetTextTrackInfo(const std::vector<TextTrackInfo> &tracks, const std::string &trackIndex)
{
	bool tracksChanged = false;
	int textTrack = -1;

	mTextTracks = tracks;
	mTextTrackIndex = trackIndex;

	textTrack = GetTextTrack();
	if (-1 != aamp->mCurrentTextTrackIndex
			&& aamp->mCurrentTextTrackIndex != textTrack)
	{
		tracksChanged = true;
	}

	aamp->mCurrentTextTrackIndex = textTrack;

	std::vector<TextTrackInfo> textTracksCopy;
	std::copy_if(begin(mTextTracks), end(mTextTracks), back_inserter(textTracksCopy), [](const TextTrackInfo& e){return e.isCC;});

	std::vector<CCTrackInfo> updatedTextTracks;
	aamp->UpdateCCTrackInfo(textTracksCopy,updatedTextTracks);
	PlayerCCManager::GetInstance()->updateLastTextTracks(updatedTextTracks);

	if (tracksChanged)
	{
		aamp->NotifyTextTracksChanged();
	}
}

/**
 * @brief To check if the adaptation set is having matching language and supported mime type
 *
 * @return bool true if the params are matching
 */
bool StreamAbstractionAAMP_MPD::IsMatchingLanguageAndMimeType(AampMediaType type, std::string lang, IAdaptationSet *adaptationSet, int &representationIndex)
{
	   bool ret = false;
	   std::string adapLang = GetLanguageForAdaptationSet(adaptationSet);
	   AAMPLOG_INFO("type %d inlang %s current lang %s", type, lang.c_str(), adapLang.c_str());
	   if (adapLang == lang)
	   {
			   PeriodElement periodElement(adaptationSet, NULL);
			   std::string adaptationMimeType = periodElement.GetMimeType();
			   if (!adaptationMimeType.empty())
			   {
					   if (IsCompatibleMimeType(adaptationMimeType, type))
					   {
							   ret = true;
							   representationIndex = 0;
					   }
			   }
			   else
			   {
					   const std::vector<IRepresentation *> representation = adaptationSet->GetRepresentation();
					   for (int repIndex = 0; repIndex < representation.size(); repIndex++)
					   {
							   const dash::mpd::IRepresentation *rep = representation.at(repIndex);
							   PeriodElement periodElement(adaptationSet, rep);
							   std::string mimeType = periodElement.GetMimeType();
							   if (!mimeType.empty() && (IsCompatibleMimeType(mimeType, type)))
							   {
									   ret = true;
									   representationIndex = repIndex;
							   }
					   }
			   }
			   if (ret != true)
			   {
					   //Even though language matched, mimeType is missing or not supported right now. Log for now
					   AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Found matching track[%d] with language:%s but not supported mimeType and thus disabled!!",
											   type, lang.c_str());
			   }
	   }
	   return ret;
}

double StreamAbstractionAAMP_MPD::GetEncoderDisplayLatency()
{
	/*
	a. If the ProducerReferenceTime _element_ is present as defined in clause 4.X.3.2, then the
	i. WCA is the value of the @wallClockTime
	ii. PTA is the value of the @presentationTime
	iii. If the @inband attribute is set to TRUE, then it should parse the segments to continuously
	update PTA and WCA accordingly
	b. Else
	i. WCA is the value of the PeriodStart
	ii. PTA is the value of the @presentationTimeOffset
	c. Then the presentation latency PL of a presentation time PT presented at wall clock time WC is
	determined as PL => (WC – WCA) - (PT – PTA)

	A segment has a presentation time PT => @t / @timescale (BEST: @PTS/@timescale)
	*/

	double encoderDisplayLatency  = 0;
	double WCA = 0;
	double PTA = 0;
	//double WC = 0;
	double PT = 0;

	struct tm *gmt = NULL;
	time_t tt = 0;
	time_t tt_utc = 0;

	tt       = NOW_SYSTEM_TS_MS/1000;//WC - Need display clock position
	gmt      = gmtime(&tt);
	gmt->tm_isdst = 0;
	tt_utc   = mktime(gmt);

	IProducerReferenceTime *producerReferenceTime = NULL;
#if 0 //FIX-ME - Handle when ProducerReferenceTime _element_ is not available
	double presentationOffset = 0;
#endif
	uint32_t timeScale = 0;
	int numPeriods	=	mMPDParseHelper->GetNumberOfPeriods();
	AAMPLOG_INFO("Current Index: %d Total Period: %d",mCurrentPeriodIdx, numPeriods);

	if(numPeriods)
	{
		IPeriod* tempPeriod = NULL;
		try {
			tempPeriod = mpd->GetPeriods().at(mCurrentPeriodIdx);

			if(tempPeriod && tempPeriod->GetAdaptationSets().size())
			{
				const std::vector<IAdaptationSet *> adaptationSets = tempPeriod->GetAdaptationSets();

				for(int j = 0; j < adaptationSets.size(); j++)
				{
					if( mMPDParseHelper->IsContentType(adaptationSets.at(j), eMEDIATYPE_VIDEO) )
					{
						producerReferenceTime = GetProducerReferenceTimeForAdaptationSet(adaptationSets.at(j));
						break;
					}
				}

				const ISegmentTemplate *representation = NULL;
				const ISegmentTemplate *adaptationSet = NULL;

				IAdaptationSet * firstAdaptation = adaptationSets.at(0);
				if(firstAdaptation != NULL)
				{
					adaptationSet = firstAdaptation->GetSegmentTemplate();
					const std::vector<IRepresentation *> representations = firstAdaptation->GetRepresentation();
					if (representations.size() > 0)
					{
						representation = representations.at(0)->GetSegmentTemplate();
					}
				}

				SegmentTemplates segmentTemplates(representation,adaptationSet);

				if( segmentTemplates.HasSegmentTemplate() )
				{
					std::string media = segmentTemplates.Getmedia();
					timeScale = segmentTemplates.GetTimescale();
					if(!timeScale)
					{
						timeScale = aamp->GetVidTimeScale();
					}
					AAMPLOG_TRACE("timeScale: %" PRIu32 "", timeScale);

#if 0 //FIX-ME - Handle when ProducerReferenceTime _element_ is not available
					presentationOffset = (double) segmentTemplates.GetPresentationTimeOffset();
#endif
					const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
					if (segmentTimeline)
					{
						std::vector<ITimeline*> vec  = segmentTimeline->GetTimelines();
						if (!vec.empty())
						{
							ITimeline* timeline = vec.back();
							uint64_t startTime = 0;
							uint32_t duration = 0;
							uint32_t repeatCount = 0;

							startTime = timeline->GetStartTime();
							duration = timeline->GetDuration();
							repeatCount = timeline->GetRepeatCount();

							AAMPLOG_TRACE("startTime: %" PRIu32 " duration: %" PRIu32 " repeatCount: %" PRIu32, timeScale, duration, repeatCount);

							if(timeScale)
								PT = (double)(startTime+((uint64_t)repeatCount*duration))/timeScale ;
							else
								AAMPLOG_WARN("Empty timeScale !!!");
						}
					}
				}

				if(producerReferenceTime)
				{
					std::string id = "";
					std::string type = "";
					std::string wallClockTime = "";
					std::string presentationTimeOffset = "";
					std::string inband = "";
					long wTime = 0;

					map<string, string> attributeMap = producerReferenceTime->GetRawAttributes();
					map<string, string>::iterator pos = attributeMap.begin();
					pos = attributeMap.find("id");
					if(pos != attributeMap.end())
					{
						id = pos->second;
						if(!id.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@id [%s]", id.c_str());
						}
					}
					pos = attributeMap.find("type");
					if(pos != attributeMap.end())
					{
						type = pos->second;
						if(!type.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@type [%s]", type.c_str());
						}
					}
					pos = attributeMap.find("wallClockTime");
					if(pos != attributeMap.end())
					{
						wallClockTime = pos->second;
						if(!wallClockTime.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@wallClockTime [%s]", wallClockTime.c_str());

							std::tm tmTime;
							const char* format = "%Y-%m-%dT%H:%M:%S.%f%Z";
							char out_buffer[ 80 ];
							memset(&tmTime, 0, sizeof(tmTime));
							strptime(wallClockTime.c_str(), format, &tmTime);
							wTime = mktime(&tmTime);

							AAMPLOG_TRACE("ProducerReferenceTime@wallClockTime [%ld] UTCTime [%f]",wTime, mLocalUtcTime);

							/* Convert the time back to a string. */
							strftime( out_buffer, 80, "That's %D (a %A), at %T",localtime (&wTime) );
							AAMPLOG_TRACE( "%s", out_buffer );
							WCA = (double)wTime ;
						}
					}
					pos = attributeMap.find("presentationTime");
					if(pos != attributeMap.end())
					{
						presentationTimeOffset = pos->second;
						if(!presentationTimeOffset.empty())
						{
							if(timeScale != 0)
							{
								PTA = ((double) std::stoll(presentationTimeOffset))/timeScale;
								AAMPLOG_TRACE("ProducerReferenceTime@presentationTime [%s] PTA [%lf]", presentationTimeOffset.c_str(), PTA);
							}
						}
					}
					pos = attributeMap.find("inband");
					if(pos != attributeMap.end())
					{
						inband = pos->second;
						if(!inband.empty())
						{
							AAMPLOG_TRACE("ProducerReferenceTime@inband [%d]", atoi(inband.c_str()));
						}
					}
				}
				else
				{
					AAMPLOG_WARN("ProducerReferenceTime Not Found for mCurrentPeriodIdx = [%d]", mCurrentPeriodIdx);
#if 0 //FIX-ME - Handle when ProducerReferenceTime _element_ is not available

					//Check more for behavior here
					double periodStartTime = 0;
					periodStartTime =  GetPeriodStartTime(mpd, mCurrentPeriodIdx);
					AAMPLOG_TRACE("mCurrentPeriodIdx=%d periodStartTime=%lf",mCurrentPeriodIdx,periodStartTime);
					WCA =  periodStartTime;
					PTA = presentationOffset;
#endif
				}

				double wc_diff = tt_utc-WCA;
				double pt_diff  = PT-PTA;
				encoderDisplayLatency = (wc_diff - pt_diff);

				AAMPLOG_INFO("tt_utc [%lf] WCA [%lf] PT [%lf] PTA [%lf] tt_utc-WCA [%lf] PT-PTA [%lf] encoderDisplayLatency [%lf]", (double)tt_utc, WCA, PT, PTA, wc_diff, pt_diff, encoderDisplayLatency);
			}
		} catch (const std::out_of_range& oor) {
			AAMPLOG_WARN("mCurrentPeriodIdx: %d mpd->GetPeriods().size(): %d Out of Range error: %s", mCurrentPeriodIdx, numPeriods, oor.what() );
		}
	}

	return encoderDisplayLatency;
}

/**
 * @brief Starts Latency monitor loop
 */
void StreamAbstractionAAMP_MPD::StartLatencyMonitorThread()
{
	assert(!latencyMonitorThreadStarted);
	try
	{
		latencyMonitorThreadID = std::thread(&StreamAbstractionAAMP_MPD::MonitorLatency, this);
		latencyMonitorThreadStarted = true;
		AAMPLOG_INFO("Thread created Latency monitor [%zx]", GetPrintableThreadID(latencyMonitorThreadID));
	}
	catch(const std::exception& e)
	{
		AAMPLOG_WARN("Failed to create LatencyMonitor thread : %s", e.what());
	}
}

/**
 * @brief Monitor Live End Latency and Encoder Display Latency
 */
void StreamAbstractionAAMP_MPD::MonitorLatency()
{
	UsingPlayerId playerId(aamp->mPlayerId);
	int latencyMonitorDelay = GETCONFIGVALUE(eAAMPConfig_LatencyMonitorDelay);
	int latencyMonitorInterval = GETCONFIGVALUE(eAAMPConfig_LatencyMonitorInterval);
	double minbuffer = GETCONFIGVALUE(eAAMPConfig_LowLatencyMinBuffer);
	double targetBuffer = GETCONFIGVALUE(eAAMPConfig_LowLatencyTargetBuffer);
	bool bufferCorrectionStarted = false;

	double normalPlaybackRate =  GETCONFIGVALUE(eAAMPConfig_NormalLatencyCorrectionPlaybackRate);

	AAMPLOG_TRACE("latencyMonitorDelay %d latencyMonitorInterval=%d", latencyMonitorDelay,latencyMonitorInterval );
	double latencyMonitorScheduleTime = latencyMonitorDelay - latencyMonitorInterval;
	//To handle latencyMonitorDelay <latencyMonitorInterval case
	if( latencyMonitorScheduleTime < 0 )
	{ // clamp!
		AAMPLOG_INFO("unexpected latencyMonitorScheduleTime(%lf)", latencyMonitorScheduleTime );
		latencyMonitorScheduleTime = 0.5 ; //TimedWaitForLatencyCheck is 500ms
	}

	aamp->SetLLDashCurrentPlayBackRate(normalPlaybackRate);
	bool keepRunning = false;
	bool latencyCorrected = true;
	if(aamp->DownloadsAreEnabled())
	{
		AAMPLOG_TRACE("latencyMonitorScheduleTime %lf", latencyMonitorScheduleTime );
		if(aamp->IsLocalAAMPTsb())
		{
			aamp->TimedWaitForLatencyCheck(latencyMonitorScheduleTime *1000);
		}
		else
		{
			aamp->interruptibleMsSleep(latencyMonitorScheduleTime *1000);
		}
		keepRunning = true;
	}
	AAMPLOG_TRACE("keepRunning : %d", keepRunning);
	int monitorInterval = latencyMonitorInterval  * 1000;
	AAMPLOG_INFO( "Speed correction state:%d", aamp->GetLLDashAdjustSpeed());

	aamp->SetLLDashCurrentPlayBackRate(normalPlaybackRate);
	bool reportEvent = false;

	while(keepRunning && aamp->GetLLDashAdjustSpeed())
	{
		if(aamp->IsLocalAAMPTsb())
		{
			aamp->TimedWaitForLatencyCheck(monitorInterval);
		}
		else
		{
			aamp->interruptibleMsSleep(monitorInterval);
		}
		if (aamp->DownloadsAreEnabled() && aamp->GetLLDashAdjustSpeed())
		{

			double playRate = aamp->GetLLDashCurrentPlayBackRate();
			AAMPPlayerState state = aamp->GetState();
			if( state != eSTATE_PLAYING || aamp->GetPositionMs() > aamp->DurationFromStartOfPlaybackMs() )
			{
				AAMPLOG_WARN("Player state:%d must be in playing and current position[%lld] must be less than Duration From Start Of Playback[%lld]!!!!:", state,
						aamp->GetPositionMs(), aamp->DurationFromStartOfPlaybackMs());
			}
			else if ((AdState::OUTSIDE_ADBREAK != mCdaiObject->mAdState) && (AdState::IN_ADBREAK_AD_NOT_PLAYING != mCdaiObject->mAdState))
			{
				AAMPLOG_DEBUG("[CDAI] Skip the Latency correction when AD is playing, state : %d reset it if needed", (int)mCdaiObject->mAdState);
				if((aamp->DownloadsAreEnabled() || aamp->mbSeeked) && (rate == AAMP_NORMAL_PLAY_RATE && (normalPlaybackRate != aamp->GetLLDashCurrentPlayBackRate())))
				{
					StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);
					if (sink)
					{
						if(sink->SetPlayBackRate(normalPlaybackRate))
						{
							AAMPLOG_INFO("[CDAI] SetPlayBackRate: reset");
							aamp->SetLLDashCurrentPlayBackRate(normalPlaybackRate);
						}
						else
						{
							AAMPLOG_WARN("[CDAI] SetPlayBackRate: reset failed");
						}
					}
				}
			}
			else
			{
				monitorInterval = latencyMonitorInterval  * 1000;
				AampLLDashServiceData *pAampLLDashServiceData = NULL;
				pAampLLDashServiceData = aamp->GetLLDashServiceData();
				if( NULL != pAampLLDashServiceData )
				{
					assert(pAampLLDashServiceData->minLatency != 0 );
					assert(pAampLLDashServiceData->minLatency <= pAampLLDashServiceData->targetLatency);
					assert(pAampLLDashServiceData->targetLatency !=0 );
					assert(pAampLLDashServiceData->maxLatency !=0 );
					assert(pAampLLDashServiceData->maxLatency >= pAampLLDashServiceData->targetLatency);

					double currentLatency;// = ((aamp->DurationFromStartOfPlaybackMs()) - aamp->GetPositionMs());
					if(aamp->mNewSeekInfo.GetInfo().isPopulated())
					{
						double liveTime = (double)aamp->mNewSeekInfo.GetInfo().getUpdateTime()/1000.0 + mDeltaTime;
						double finalProgressTime = (aamp->mFirstFragmentTimeOffset) + ((double)aamp->mNewSeekInfo.GetInfo().getPosition()/1000.0);
						currentLatency = (liveTime - finalProgressTime) * 1000;
						if(aamp->mProgressReportOffset >= 0)
						{
							// Correction with progress offset
							currentLatency += (aamp->mProgressReportOffset * 1000);
						}
					}
					else
					{
						currentLatency = ((aamp->DurationFromStartOfPlaybackMs()) - aamp->GetPositionMs());
					}
					AAMPLOG_TRACE("LiveLatency=%lf currentPlayRate=%lf dur:%lld pos:%lld",currentLatency, playRate, aamp->DurationFromStartOfPlaybackMs(), aamp->GetPositionMs());
#if 0
					long encoderDisplayLatency = 0;
					encoderDisplayLatency = (long)( GetEncoderDisplayLatency() * 1000)+currentLatency;
					AAMPLOG_INFO("Encoder Display Latency=%ld", encoderDisplayLatency);
#endif
					if(ISCONFIGSET(eAAMPConfig_EnableLowLatencyCorrection) &&
						pAampLLDashServiceData->minPlaybackRate !=0 &&
						pAampLLDashServiceData->minPlaybackRate < pAampLLDashServiceData->maxPlaybackRate &&
						pAampLLDashServiceData->minPlaybackRate < normalPlaybackRate &&
						pAampLLDashServiceData->maxPlaybackRate !=0 &&
						pAampLLDashServiceData->maxPlaybackRate > pAampLLDashServiceData->minPlaybackRate &&
						pAampLLDashServiceData->maxPlaybackRate > normalPlaybackRate)
					{
						double bufferValue = GetBufferedDuration();
						bool isEnoughBuffer = (bufferValue >= targetBuffer);
						//Added Debug log to triage playrate issue
						AAMPLOG_TRACE("targetBuffer=%lf bufferValue=%lf isEnoughBuffer=%d segmentDuration=%lf",	targetBuffer, bufferValue, isEnoughBuffer, pAampLLDashServiceData->fragmentDuration);
						bool bufferLowHit = false;
						static int bufferLowCount = 0;
						static int bufferLowHitCount = 0;
						double currPlaybackRate = aamp->GetLLDashCurrentPlayBackRate();

						if(bufferValue < minbuffer)
						{
							bufferLowCount++;
							if (bufferLowCount == AAMP_LLD_LOW_BUFF_CHECK_COUNT)
							{
								bufferLowHit = true;
								bufferLowHitCount++;
								/** Buffer Low hit so push the data to telemetry*/
								aamp->profiler.SetLLDLowBufferParam(currentLatency, bufferValue, currPlaybackRate, aamp->mNetworkBandwidth, bufferLowHitCount);
								bufferLowCount = 0;
							}
						}
						else
						{
							bufferLowHit = false;
							bufferLowCount = 0;
							bufferLowHitCount = 0;
						}

						AAMPLOG_INFO("currentLatency = %.02lf  AvailableBuffer = %.02lf minbuffer = %.02lf targetBuffer=%.02lf currentPlaybackRate = %.02lf bufferLowHitted = %d isEnoughBuffer = %d latencyCorrected = %d bufferCorrectionStarted = %d",
							currentLatency, bufferValue, minbuffer, targetBuffer, currPlaybackRate, bufferLowHit, isEnoughBuffer, latencyCorrected, bufferCorrectionStarted);

						if ((currentLatency > ((double) pAampLLDashServiceData->maxLatency )) && isEnoughBuffer)
						{
							if (latencyCorrected)
							{
								latencyCorrected = false;
								reportEvent = true;
							}
							playRate = pAampLLDashServiceData->maxPlaybackRate;
						}
						else if (currentLatency < ((double) pAampLLDashServiceData->minLatency) ||
						(bufferLowHit && (currPlaybackRate != pAampLLDashServiceData->minPlaybackRate)) )
						{
							if ((currentLatency < (double) pAampLLDashServiceData->minLatency) && !latencyCorrected)
							{
								/**< Rate change due to latency change; So report event; rare condition*/
								latencyCorrected = true;
								reportEvent = true;
							}
							else
							{
								/**< Rate change due to buffer condition; So no need to report event;*/
								bufferCorrectionStarted = true;
							}
							playRate = pAampLLDashServiceData->minPlaybackRate;
						}
						else if (((currentLatency <= (long)pAampLLDashServiceData->targetLatency) &&  currPlaybackRate ==  pAampLLDashServiceData->maxPlaybackRate))
						{
							/** latency corrected; stop max rate playback*/
							latencyCorrected = true;
							reportEvent = true;
							playRate = normalPlaybackRate;
						}
						else if (((currentLatency >= (long)pAampLLDashServiceData->targetLatency) &&  currPlaybackRate == pAampLLDashServiceData->minPlaybackRate) && (bufferValue > minbuffer))
						{
							if (bufferCorrectionStarted)
							{
								bufferCorrectionStarted = false;
								reportEvent = false;
								/** Buffer corrected, stop min playback; No need to send event **/
								playRate = normalPlaybackRate;
							}
							else
							{
								/** rate corrected; stop min rate playback*/
								latencyCorrected = true;
								reportEvent = true;
								playRate = normalPlaybackRate;
							}
						}
						else if ((currPlaybackRate ==  pAampLLDashServiceData->maxPlaybackRate) && !isEnoughBuffer)
						{
							/**< Stop max playback due to buffer low case; No need to send event*/
							latencyCorrected = false;
							reportEvent = false;
							playRate = normalPlaybackRate;
						}
						else
						{
							/** Nothing to do with rate change*/
						}

						if ( playRate != currPlaybackRate )
						{
							bool rateCorrected=false;

							StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);

							if(sink && false == sink->SetPlayBackRate(playRate))
							{
								 AAMPLOG_WARN("SetPlayBackRate: failed !!!, new rate:%f curr rate: %lf", playRate, currPlaybackRate);
							}
							else if (reportEvent)
							{
								rateCorrected = true;
								aamp->UpdateVideoEndMetrics(playRate);
								aamp->SendAnomalyEvent(ANOMALY_WARNING, "Rate changed to:%lf", playRate);
								AAMPLOG_INFO("PlayBack Rate changed to :  %lf and Event Send", playRate);
								reportEvent = false; /** Reset the flag*/
							}
							else
							{
								AAMPLOG_INFO("PlayBack Rate changed to :  %lf", playRate);
								rateCorrected = true;
							}

							if ( rateCorrected )
							{
								aamp->profiler.IncrementChangeCount(Count_RateCorrection);
								aamp->SetLLDashCurrentPlayBackRate(playRate);
							}
						}
					}
				}
				else
				{
					AAMPLOG_WARN("ServiceDescription _element_ is empty");
				}
			}
		}
		else
		{
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);

			if (sink)
			{
				if((aamp->DownloadsAreEnabled() || aamp->mbSeeked) && (rate == AAMP_NORMAL_PLAY_RATE && (normalPlaybackRate != aamp->GetLLDashCurrentPlayBackRate())))
				{
					if(sink->SetPlayBackRate(normalPlaybackRate))
					{
						AAMPLOG_INFO("SetPlayBackRate: reset");
						aamp->SetLLDashCurrentPlayBackRate(normalPlaybackRate);
					}
					else
					{
						AAMPLOG_WARN("SetPlayBackRate: reset failed");
					}
				}
			}
			AAMPLOG_WARN("Stopping Thread");
			keepRunning = false;
		}
	}
	AAMPLOG_WARN("Thread Done");
}

/**
 * @brief Check if LLProfile is Available in MPD
 * @retval bool true if LL profile. Else false
 */
bool StreamAbstractionAAMP_MPD::CheckLLProfileAvailable(IMPD *mpd)
{
	std::vector<std::string> profiles;
	profiles = this->mpd->GetProfiles();
	size_t numOfProfiles = profiles.size();
	for (int iProfileCnt = 0; iProfileCnt < numOfProfiles; iProfileCnt++)
	{
		std::string profile = profiles.at(iProfileCnt);
		if(!strcmp(LL_DASH_SERVICE_PROFILE , profile.c_str()))
		{
			return true;
		}
	}
	return false;
}

/**
 * @brief Check if ProducerReferenceTime UTCTime type Matches with Other UTCtime type declaration
 * @retval bool true if Match exist. Else false
 */
bool StreamAbstractionAAMP_MPD::CheckProducerReferenceTimeUTCTimeMatch(IProducerReferenceTime *pRT)
{
	bool bMatch = false;
	//1. Check if UTC Time provider in <ProducerReferenceTime> _element_ is same as stored for MPD already

	if(pRT->GetUTCTimings().size())
	{
		IUTCTiming *utcTiming= pRT->GetUTCTimings().at(0);

		// Some timeline may not have attribute for target latency , check it .
		map<string, string> attributeMapTiming = utcTiming->GetRawAttributes();

		if(attributeMapTiming.find("schemeIdUri") == attributeMapTiming.end())
		{
			AAMPLOG_WARN("UTCTiming@schemeIdUri attribute not available");
		}
		else
		{
			UtcTiming utcTimingType = eUTC_HTTP_INVALID;
			AAMPLOG_TRACE("UTCTiming@schemeIdUri: %s", utcTiming->GetSchemeIdUri().c_str());

			if(!strcmp(URN_UTC_HTTP_XSDATE , utcTiming->GetSchemeIdUri().c_str()))
			{
				utcTimingType = eUTC_HTTP_XSDATE;
			}
			else if(!strcmp(URN_UTC_HTTP_ISO , utcTiming->GetSchemeIdUri().c_str()))
			{
				utcTimingType = eUTC_HTTP_ISO;
			}
			else if(!strcmp(URN_UTC_HTTP_NTP , utcTiming->GetSchemeIdUri().c_str()))
			{
				utcTimingType = eUTC_HTTP_NTP;
			}
			else
			{
				AAMPLOG_WARN("UTCTiming@schemeIdUri Value not proper");
			}
			//Check if it matches with MPD provided UTC timing
			if(utcTimingType == aamp->GetLLDashServiceData()->utcTiming)
			{
				bMatch = true;
			}

			//Adaptation set timing didnt match
			if(!bMatch)
			{
				AAMPLOG_WARN("UTCTiming did not Match. !!");
			}
		}
	}
	return bMatch;
}

/**
 * @brief Print ProducerReferenceTime parsed data
 * @retval void
 */
void StreamAbstractionAAMP_MPD::PrintProducerReferenceTimeAttributes(IProducerReferenceTime *pRT)
{
	AAMPLOG_TRACE("Id: %s", pRT->GetId().c_str());
	AAMPLOG_TRACE("Type: %s", pRT->GetType().c_str());
	AAMPLOG_TRACE("WallClockTime %s" , pRT->GetWallClockTime().c_str());
	AAMPLOG_TRACE("PresentationTime : %d" , pRT->GetPresentationTime());
	AAMPLOG_TRACE("Inband : %s" , pRT->GetInband()?"true":"false");
}

/**
 * @brief Check if ProducerReferenceTime available in AdaptationSet
 * @retval IProducerReferenceTime* pointer to parsed ProducerReferenceTime data
 */
IProducerReferenceTime *StreamAbstractionAAMP_MPD::GetProducerReferenceTimeForAdaptationSet(IAdaptationSet *adaptationSet)
{
	IProducerReferenceTime *pRT = NULL;

	if(adaptationSet != NULL)
	{
		const std::vector<IProducerReferenceTime *> producerReferenceTime = adaptationSet->GetProducerReferenceTime();

		if(!producerReferenceTime.size())
			return pRT;

		pRT = producerReferenceTime.at(0);
	}
	else
	{
		AAMPLOG_WARN("adaptationSet  is null");  //CID:85233 - Null Returns
	}
	return pRT;
}

/**
 * @brief EnableAndSetLiveOffsetForLLDashPlayback based on playerconfig/LL-dash
 * profile/availabilityTimeOffset and set the LiveOffset
 */
AAMPStatusType  StreamAbstractionAAMP_MPD::EnableAndSetLiveOffsetForLLDashPlayback(const MPD* mpd)
{
	  AAMPStatusType ret = eAAMPSTATUS_OK;
	 mLowLatencyMode	=	false;
	/*LL DASH VERIFICATION START*/
	//Check if LLD requested
	if (ISCONFIGSET(eAAMPConfig_EnableLowLatencyDash))
	{
		AampLLDashServiceData stLLServiceData;
		double currentOffset = 0;
		int maxLatency=0,minLatency=0,TargetLatency=0;
		AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
		if((dnldInstance->IsMPDLowLatency(stLLServiceData))
			&&	(stLLServiceData.availabilityTimeComplete == false ))
		{
			stLLServiceData.lowLatencyMode = true;
			if( ISCONFIGSET( eAAMPConfig_EnableLowLatencyCorrection ) )
			{
				aamp->SetLLDashAdjustSpeed(true);
			}
			else
			{
				aamp->SetLLDashAdjustSpeed(false);
			}
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: LL-DASH playback enabled availabilityTimeOffset=%lf,fragmentDuration=%lf",
									stLLServiceData.availabilityTimeOffset,stLLServiceData.fragmentDuration);
		}
		else
		{
			aamp->SetLLDashAdjustSpeed(false);
			if(ISCONFIGSET(eAAMPConfig_ForceLLDFlow))
			{
				stLLServiceData.lowLatencyMode = true;
				AAMPLOG_WARN("LL-DASH Mode Forced. Not an LL-DASH Stream");
			}
			else
			{
				stLLServiceData.lowLatencyMode = false;
				AAMPLOG_TRACE("LL-DASH Mode Disabled. Not a LL-DASH Stream");
			}
		}

		//If LLD enabled then check servicedescription requirements
		if( stLLServiceData.lowLatencyMode )
		{
			if(!ParseMPDLLData((MPD*)this->mpd, stLLServiceData))
			{
				ret = eAAMPSTATUS_MANIFEST_PARSE_ERROR;
				return ret;
			}

			if ( 0 == stLLServiceData.maxPlaybackRate )
			{
				stLLServiceData.maxPlaybackRate = GETCONFIGVALUE(eAAMPConfig_MaxLatencyCorrectionPlaybackRate);
			}

			if ( 0 == stLLServiceData.minPlaybackRate )
			{
				stLLServiceData.minPlaybackRate = GETCONFIGVALUE(eAAMPConfig_MinLatencyCorrectionPlaybackRate);
			}

			minLatency = GETCONFIGVALUE(eAAMPConfig_LLMinLatency);
			TargetLatency = GETCONFIGVALUE(eAAMPConfig_LLTargetLatency);
			maxLatency = GETCONFIGVALUE(eAAMPConfig_LLMaxLatency);
			if (aamp->mIsStream4K)
			{
				currentOffset = GETCONFIGVALUE(eAAMPConfig_LiveOffset4K);
			}
			else
			{
				currentOffset = GETCONFIGVALUE(eAAMPConfig_LiveOffset);
			}

			AAMPLOG_INFO("StreamAbstractionAAMP_MPD: Current Offset(s): %ld",(long)currentOffset);

			if(	stLLServiceData.minLatency <= 0)
			{
				if(minLatency <= 0 || minLatency > TargetLatency )
				{
					stLLServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY*1000;
				}
				else
				{
					stLLServiceData.minLatency = minLatency*1000;
				}
			}
			if(	stLLServiceData.maxLatency <= 0 ||
				stLLServiceData.maxLatency < stLLServiceData.minLatency )
			{
				if( maxLatency <=0 || maxLatency < minLatency )
				{
					stLLServiceData.maxLatency = DEFAULT_MAX_LOW_LATENCY*1000;
					stLLServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY*1000;
				}
				else
				{
					stLLServiceData.maxLatency = maxLatency*1000;
				}
			}
			if(	stLLServiceData.targetLatency <= 0 ||
				stLLServiceData.targetLatency < stLLServiceData.minLatency ||
				stLLServiceData.targetLatency > stLLServiceData.maxLatency )

			{
				if(TargetLatency <=0 || TargetLatency < minLatency || TargetLatency > maxLatency )
				{
					stLLServiceData.targetLatency = DEFAULT_TARGET_LOW_LATENCY*1000;
					stLLServiceData.maxLatency = DEFAULT_MAX_LOW_LATENCY*1000;
					stLLServiceData.minLatency = DEFAULT_MIN_LOW_LATENCY*1000;
				}
				else
				{
					stLLServiceData.targetLatency = TargetLatency*1000;
				}
			}
			double latencyOffsetMin = stLLServiceData.minLatency/(double)1000;
			double latencyOffsetMax = stLLServiceData.maxLatency/(double)1000;
			AAMPLOG_MIL("StreamAbstractionAAMP_MPD:[LL-Dash] Min Latency: %ld Max Latency: %ld Target Latency: %ld",(long)latencyOffsetMin,(long)latencyOffsetMax,(long)TargetLatency);
			SETCONFIGVALUE(AAMP_STREAM_SETTING, eAAMPConfig_IgnoreAppLiveOffset, true);
			//Ignore Low latency setting
			if(!ISCONFIGSET(eAAMPConfig_ForceLLDFlow) && !ISCONFIGSET(eAAMPConfig_IgnoreAppLiveOffset) && (((AAMP_DEFAULT_SETTING != GETCONFIGOWNER(eAAMPConfig_LiveOffset4K)) && (currentOffset > latencyOffsetMax) && aamp->mIsStream4K) ||
			((AAMP_DEFAULT_SETTING != GETCONFIGOWNER(eAAMPConfig_LiveOffset)) && (currentOffset > latencyOffsetMax))))
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD: Switch off LL mode: App requested currentOffset > latencyOffsetMax");
				stLLServiceData.lowLatencyMode = false;
			}
			else
			{
				double latencyOffset = 0;
				if(!aamp->GetLowLatencyServiceConfigured())
				{
					latencyOffset =(double) (stLLServiceData.targetLatency/1000);
					if(!ISCONFIGSET(eAAMPConfig_ForceLLDFlow))
					{
						//Override Latency offset with Min Value if config enabled
						AAMPLOG_INFO("StreamAbstractionAAMP_MPD: currentOffset:%lf LL-DASH offset(s): %lf",currentOffset,latencyOffset);
						if(((AAMP_STREAM_SETTING >= GETCONFIGOWNER(eAAMPConfig_LiveOffset4K)) && aamp->mIsStream4K) ||
						((AAMP_STREAM_SETTING >= GETCONFIGOWNER(eAAMPConfig_LiveOffset))))
						{
							SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_LiveOffset,latencyOffset);
							if (AAMP_STREAM_SETTING >= GETCONFIGOWNER(eAAMPConfig_LiveOffset))
							{
								aamp->UpdateLiveOffset();
							}
						}
						else
						{
							if(ISCONFIGSET(eAAMPConfig_IgnoreAppLiveOffset) && (GETCONFIGOWNER(eAAMPConfig_LiveOffset) == AAMP_APPLICATION_SETTING))
							{
								SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_LiveOffset,latencyOffset);
								aamp->UpdateLiveOffset();
							}
						}
					}
					//Set LL Dash Service Configuration Data in Pvt AAMP instance
					aamp->SetLLDashServiceData(stLLServiceData);
					aamp->SetLowLatencyServiceConfigured(true);
				}
			}
		}
		mLowLatencyMode	=	stLLServiceData.lowLatencyMode;
	}
	else
	{
		AAMPLOG_INFO("StreamAbstractionAAMP_MPD: LL-DASH playback disabled in config");
	}
	return ret;
}
/**
 * @brief get Low Latency Parameters from segment template and timeline
 */
bool StreamAbstractionAAMP_MPD::GetLowLatencyParams(const MPD* mpd,AampLLDashServiceData &LLDashData)
{
	bool isSuccess=false;
	if(mpd != NULL)
	{
		size_t numPeriods = mpd->GetPeriods().size();

		for (unsigned iPeriod = 0; iPeriod < numPeriods; iPeriod++)
		{
			IPeriod *period = mpd->GetPeriods().at(iPeriod);
			if(NULL != period )
			{
				if(mMPDParseHelper->IsEmptyPeriod((int)iPeriod, (rate != AAMP_NORMAL_PLAY_RATE)))
				{
					// Empty Period . Ignore processing, continue to next.
					continue;
				}
				const std::vector<IAdaptationSet *> adaptationSets = period->GetAdaptationSets();
				if (adaptationSets.size() > 0)
				{
					IAdaptationSet * pFirstAdaptation = adaptationSets.at(0);
					if ( NULL != pFirstAdaptation )
					{
						ISegmentTemplate *pSegmentTemplate = NULL;
						pSegmentTemplate = pFirstAdaptation->GetSegmentTemplate();
						if( NULL != pSegmentTemplate )
						{
							map<string, string> attributeMap = pSegmentTemplate->GetRawAttributes();
							if(attributeMap.find("availabilityTimeOffset") == attributeMap.end())
							{
								AAMPLOG_WARN("Latency availabilityTimeOffset attribute not available");
							}
							else
							{
								LLDashData.availabilityTimeOffset = pSegmentTemplate->GetAvailabilityTimeOffset();
								LLDashData.availabilityTimeComplete = pSegmentTemplate->GetAvailabilityTimeComplete();
								AAMPLOG_INFO("AvailabilityTimeOffset=%lf AvailabilityTimeComplete=%d",
												pSegmentTemplate->GetAvailabilityTimeOffset(),pSegmentTemplate->GetAvailabilityTimeComplete());
								isSuccess=true;
								if( isSuccess )
								{
									uint32_t timeScale=0;
									uint32_t duration =0;
									const ISegmentTimeline *segmentTimeline = pSegmentTemplate->GetSegmentTimeline();
									if (segmentTimeline)
									{
										timeScale = pSegmentTemplate->GetTimescale();
										std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();
										ITimeline *timeline = timelines.at(0);
										duration = timeline->GetDuration();
										LLDashData.fragmentDuration = ComputeFragmentDuration(duration,timeScale);
										LLDashData.isSegTimeLineBased = true;
									}
									else
									{
										timeScale = pSegmentTemplate->GetTimescale();
										duration = pSegmentTemplate->GetDuration();
										LLDashData.fragmentDuration = ComputeFragmentDuration(duration,timeScale);
										LLDashData.isSegTimeLineBased = false;
									}
									AAMPLOG_INFO("timeScale=%u duration=%u fragmentDuration=%lf",
												timeScale,duration,LLDashData.fragmentDuration);
								}
								break;
							}
						}
						else
						{
							AAMPLOG_ERR("NULL segmenttemplate");
						}
					}
					else
					{
						AAMPLOG_INFO("NULL adaptationSets");
					}
				}
				else
				{
					AAMPLOG_WARN("empty adaptationSets");
				}
			}
			else
			{
				AAMPLOG_WARN("empty period ");
			}
		}
	}
	else
	{
		AAMPLOG_WARN("NULL mpd");
	}
	return isSuccess;
}
/**
 * @brief Parse MPD LL elements
 */
bool StreamAbstractionAAMP_MPD::ParseMPDLLData(MPD* mpd, AampLLDashServiceData &stAampLLDashServiceData)
{
	//check if <ServiceDescription> available->raise error if not
	if(!mpd->GetServiceDescriptions().size())
	{
	   AAMPLOG_TRACE("GetServiceDescriptions not available");
	}
	else
	{
		//check if <scope> _element_ is available in <ServiceDescription> _element_->raise error if not
		if(!mpd->GetServiceDescriptions().at(0)->GetScopes().size())
		{
			AAMPLOG_TRACE("Scope _element_ not available");
		}
		//check if <Latency> _element_ is available in <ServiceDescription> _element_->raise error if not
		if(!mpd->GetServiceDescriptions().at(0)->GetLatencys().size())
		{
			AAMPLOG_TRACE("Latency _element_ not available");
		}
		else
		{
			//check if attribute @target is available in <latency> _element_->raise error if not
			ILatency *latency= mpd->GetServiceDescriptions().at(0)->GetLatencys().at(0);

			// Some timeline may not have attribute for target latency , check it .
			map<string, string> attributeMap = latency->GetRawAttributes();

			if(attributeMap.find("target") == attributeMap.end())
			{
				AAMPLOG_TRACE("target Latency attribute not available");
			}
			else
			{
				stAampLLDashServiceData.targetLatency = latency->GetTarget();
				AAMPLOG_INFO("targetLatency: %d", stAampLLDashServiceData.targetLatency);
			}

			//check if attribute @max or @min is available in <Latency> element->raise info if not
			if(attributeMap.find("max") == attributeMap.end())
			{
				AAMPLOG_TRACE("Latency max attribute not available");
			}
			else
			{
				stAampLLDashServiceData.maxLatency = latency->GetMax();
				AAMPLOG_INFO("maxLatency: %d", stAampLLDashServiceData.maxLatency);
			}
			if(attributeMap.find("min") == attributeMap.end())
			{
				AAMPLOG_TRACE("Latency min attribute not available");
			}
			else
			{
				stAampLLDashServiceData.minLatency = latency->GetMin();
				AAMPLOG_INFO("minLatency: %d", stAampLLDashServiceData.minLatency);
			}
		}

		if(!mpd->GetServiceDescriptions().at(0)->GetPlaybackRates().size())
		{
			AAMPLOG_TRACE("Play Rate _element_ not available");
		}
		else
		{
			//check if attribute @max or @min is available in <PlaybackRate> element->raise info if not
			IPlaybackRate *playbackRate= mpd->GetServiceDescriptions().at(0)->GetPlaybackRates().at(0);

			// Some timeline may not have attribute for target latency , check it .
			map<string, string> attributeMapRate = playbackRate->GetRawAttributes();

			if(attributeMapRate.find("max") == attributeMapRate.end())
			{
				AAMPLOG_TRACE("Latency max attribute not available");
				stAampLLDashServiceData.maxPlaybackRate = GETCONFIGVALUE(eAAMPConfig_MaxLatencyCorrectionPlaybackRate);
			}
			else
			{
				stAampLLDashServiceData.maxPlaybackRate = playbackRate->GetMax();
				AAMPLOG_INFO("maxPlaybackRate: %0.2f",stAampLLDashServiceData.maxPlaybackRate);
			}
			if(attributeMapRate.find("min") == attributeMapRate.end())
			{
				AAMPLOG_TRACE("Latency min attribute not available");
				stAampLLDashServiceData.minPlaybackRate = GETCONFIGVALUE(eAAMPConfig_MinLatencyCorrectionPlaybackRate);;
			}
			else
			{
				stAampLLDashServiceData.minPlaybackRate = playbackRate->GetMin();
				AAMPLOG_INFO("minPlaybackRate: %0.2f", stAampLLDashServiceData.minPlaybackRate);
			}
		}
	}
	//check if UTCTiming _element_ available
	if(!mpd->GetUTCTimings().size())
	{
		AAMPLOG_WARN("UTCTiming _element_ not available");
	}
	else
	{

		//check if attribute @max or @min is available in <PlaybackRate> element->raise info if not
		IUTCTiming *utcTiming= mpd->GetUTCTimings().at(0);

		// Some timeline may not have attribute for target latency , check it .
		map<string, string> attributeMapTiming = utcTiming->GetRawAttributes();

		if(attributeMapTiming.find("schemeIdUri") == attributeMapTiming.end())
		{
			AAMPLOG_WARN("UTCTiming@schemeIdUri attribute not available");
		}
		else
		{
			AAMPLOG_TRACE("UTCTiming@schemeIdUri: %s", utcTiming->GetSchemeIdUri().c_str());
			if(!strcmp(URN_UTC_HTTP_XSDATE , utcTiming->GetSchemeIdUri().c_str()))
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_XSDATE;
			}
			else if(!strcmp(URN_UTC_HTTP_ISO , utcTiming->GetSchemeIdUri().c_str()))
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_ISO;
			}
			else if(!strcmp(URN_UTC_HTTP_NTP , utcTiming->GetSchemeIdUri().c_str()))
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_NTP;
			}
			else
			{
				stAampLLDashServiceData.utcTiming = eUTC_HTTP_INVALID;
				AAMPLOG_WARN("UTCTiming@schemeIdUri Value not proper");
			}

		}
	}
	return true;
}

/***************************************************************************
 * @brief Function to get available video tracks
 *
 * @return vector of available video tracks.
 ***************************************************************************/
std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableVideoTracks(void)
{
	std::vector<StreamInfo*> videoTracks;
	for( int i = 0; i < mProfileCount; i++ )
	{
		struct StreamInfo *streamInfo = &mStreamInfo[i];
		videoTracks.push_back(streamInfo);
	}
	return videoTracks;
}


bool StreamAbstractionAAMP_MPD::SetTextStyle(const std::string &options)
{
	bool retVal;
	// If sidecar subtitles
	if (mSubtitleParser)
	{
		AAMPLOG_INFO("Calling SubtitleParser::SetTextStyle(%s)", options.c_str());
		mSubtitleParser->setTextStyle(options);
		retVal = true;
	}
	else
	{
		retVal = StreamAbstractionAAMP::SetTextStyle(options);
	}
	return retVal;
}

/**
 * @fn ProcessAllContentProtectionForMediaType
 * @param[in] type - media type
 * @param[in] priorityAdaptationIdx - selected adaption index, to be processed with priority
 * @param[in] chosenAdaptationIdxs - selected adaption indexes, might be empty for certain playback cases
 * @brief process content protection of all the adaptation for the given media type
 * @retval true on success
 */
void StreamAbstractionAAMP_MPD::ProcessAllContentProtectionForMediaType(AampMediaType type, uint32_t priorityAdaptationIdx, std::set<uint32_t> &chosenAdaptationIdxs)
{
	// Select the current period
	IPeriod *period = mCurrentPeriod;
	bool chosenAdaptationPresent = !chosenAdaptationIdxs.empty();
	if (period)
	{
		const auto &adaptationSets = period->GetAdaptationSets();
		// Queue the priority adaptation, which is the one ABR selected to start initially
		// No need to check if priorityAdaptationIdx is in chosenAdaptationIdxs as its selected after filtering
		QueueContentProtection(period, priorityAdaptationIdx, type);

		for (uint32_t iAdaptationSet = 0; iAdaptationSet < adaptationSets.size(); iAdaptationSet++)
		{
			if (iAdaptationSet == priorityAdaptationIdx)
			{
				// This is already queued. Skip!
				continue;
			}
			// Split the conditions for better readability
			if (chosenAdaptationPresent && (chosenAdaptationIdxs.find(iAdaptationSet) == chosenAdaptationIdxs.end()))
			{
				// This is not chosen. Skip!
				continue;
			}
			IAdaptationSet *adaptationSet = adaptationSets.at(iAdaptationSet);
			if (adaptationSet && mMPDParseHelper->IsContentType(adaptationSet, type))
			{
				// Only enabled for video streams for now
				if (eMEDIATYPE_VIDEO == type && !IsIframeTrack(adaptationSet))
				{
					// No need to queue protection event in GetStreamSink() for remaining adaptation sets
					// For encrypted iframe, the contentprotection is already queued in StreamSelection
					QueueContentProtection(period, iAdaptationSet, type, false);
				}
			}
		}
	}
}

/**
 * @fn UpdateFailedDRMStatus
 * @brief Function to update the failed DRM status to mark the adaptation sets to be omitted
 * @param[in] object  - Prefetch object instance which failed
 */
 // TODO: Add implementation to mark the failed DRM's adaptation set as failed/un-usable
void StreamAbstractionAAMP_MPD::UpdateFailedDRMStatus(LicensePreFetchObject *object)
{
	IPeriod *period = nullptr;
	std::vector<BitsPerSecond> profilesToRemove;
	bool updateABR = false;

	if (object == nullptr)
	{
		AAMPLOG_ERR("LicensePreFetchObject received is NULL!");
		return;
	}

	AAMPLOG_WARN("Failed DRM license acquisition for periodId:%s and adaptationIdx:%u", object->mPeriodId.c_str(), object->mAdaptationIdx);
	if (mCurrentPeriod && (mCurrentPeriod->GetId() == object->mPeriodId))
	{
		period = mCurrentPeriod;
		// Update ABR profiles if the DRM response is for the current period
		// Otherwise, the period might be over and hence just persist the info
		updateABR = true;
	}

	if (updateABR && period != nullptr)
	{
		try
		{
			auto adaptationSetToRemove = period->GetAdaptationSets().at(object->mAdaptationIdx);
			for (auto representation : adaptationSetToRemove->GetRepresentation())
			{
				AAMPLOG_WARN("Remove ABR profile adaptationSetIdx:%d and BW:%u due to DRM failure", object->mAdaptationIdx, representation->GetBandwidth());
				profilesToRemove.push_back(representation->GetBandwidth());
			}
		}
		catch (const std::out_of_range& oor)
		{
			AAMPLOG_ERR("Failed to find the adaptationSetIdx:%u in the period ID:%s to update ABR! Out of Range error: %s", object->mAdaptationIdx, object->mPeriodId.c_str(), oor.what());
		}
	}
	AAMPLOG_INFO("Removing profile count:%zu from ABR list", profilesToRemove.size());
	if (profilesToRemove.size())
	{
		int profileIdx = currentProfileIndex;
		// Pass empty period Id as the same is used in addProfile
		profileIdx = GetABRManager().removeProfiles(profilesToRemove, profileIdx);
		if (profileIdx == ABRManager::INVALID_PROFILE)
		{
			AAMPLOG_WARN("After ABR profile update, profileIndex=INVALID_PROFILE and currentProfileIndex=%d, discard it", currentProfileIndex);
		}
		else if (profileIdx != currentProfileIndex)
		{
			AAMPLOG_WARN("After ABR profile update, currentProfileIndex changed from %d -> %d", currentProfileIndex, profileIdx);
			currentProfileIndex = profileIdx;
		}
	}

	// Blacklist the failed adaptationSet to prevent it from used again
	AAMPLOG_WARN("Add periodId:%s and adaptationSetIdx:%u to blacklist profiles", object->mPeriodId.c_str(), object->mAdaptationIdx);
	StreamBlacklistProfileInfo blProfileInfo = { object->mPeriodId, object->mAdaptationIdx, PROFILE_BLACKLIST_DRM_FAILURE };
	aamp->AddToBlacklistedProfiles(blProfileInfo);
}

static std::string getrelativenorurl(std::string media)
{
	if(!media.empty())
	{
		size_t urlpos = media.find_last_of('/');
		if (urlpos != std::string::npos) {
			media = media.substr(urlpos+1);
		}
	}
	return media;
}

void StreamAbstractionAAMP_MPD::setNextobjectrequestUrl(std::string media,const FragmentDescriptor *fragmentDescriptor,AampMediaType mediaType)
{
	if( !media.empty() )
	{
		replace(media, "Bandwidth", fragmentDescriptor->Bandwidth);
		replace(media, "RepresentationID", fragmentDescriptor->RepresentationID);
		replace(media, "Number", fragmentDescriptor->nextfragmentNum);
		replace(media, "Time", (uint64_t)fragmentDescriptor->nextfragmentTime );
	}
	AAMPLOG_DEBUG("Current Frag Number %" PRIu64 "  nextfragmentNum : %" PRIu64 ",Current fragstarttime : %f nextfragmentTime : %f",fragmentDescriptor->Number,fragmentDescriptor->nextfragmentNum,fragmentDescriptor->Time,fragmentDescriptor->nextfragmentTime);
	media = getrelativenorurl(media);
	aamp->mCMCDCollector->CMCDSetNextObjectRequest( media ,(fragmentDescriptor)->Bandwidth,mediaType);
}

void StreamAbstractionAAMP_MPD::setNextRangeRequest(std::string fragmentUrl,std::string nextrange,long bandwidth,AampMediaType mediaType)
{
	aamp->mCMCDCollector->CMCDSetNextRangeRequest(nextrange,bandwidth,mediaType);
}

/**
 * @brief Retrieves the index of a valid period based on the given period index.
 *
 * This function checks if the period at the given index is empty or has a duration below a threshold.
 * If the period is valid, the function returns the given period index.
 * If the period is empty or has a tiny duration, the function searches for the next or previous non-empty period
 * or a period with a duration greater than a threshold. If a valid period is found, its index is returned.
 * If no valid period is found, the function returns the given period index.
 * Any empty period followed by non-empty period will never be instantiated, will remain empty, and will eventually slide out of the live window.
 * These forever-empty periods must never be presented using alternate content, but should be skipped.
 *
 * @param periodIdx The index of the period to check.
 * @return The index of a valid period.
 */
int StreamAbstractionAAMP_MPD::GetValidPeriodIdx(int periodIdx)
{
	int periodIter = periodIdx;
	bool isPeriodEmpty = mMPDParseHelper->IsEmptyPeriod(periodIter, (rate != AAMP_NORMAL_PLAY_RATE));
	double periodDuration = mMPDParseHelper->GetPeriodDuration(periodIter, mLastPlaylistDownloadTimeMs, (rate != AAMP_NORMAL_PLAY_RATE), aamp->IsUninterruptedTSB());
	if (!isPeriodEmpty && periodDuration >= THRESHOLD_TOIGNORE_TINYPERIOD)
	{
		AAMPLOG_WARN("[CDAI] Landed at period (%s) periodIdx: %d duration(ms):%f",mpd->GetPeriods().at(periodIter)->GetId().c_str(), periodIter, periodDuration);
		return periodIter;
	}

	if(periodIdx >= 0 && (periodIdx < mNumberOfPeriods))
	{
		AAMPLOG_WARN("[CDAI] current period [id:%s d:%f] is empty or tiny, check if the immediate next period is non-empty",
					mpd->GetPeriods().at(periodIter)->GetId().c_str(), periodDuration);
		// Find the next or prev non-empty period or period with duration greater than THRESHOLD_TOIGNORE_TINYPERIOD
		bool bvalidperiodfound = false;
		int direction = (rate < 0) ? -1 : 1;
		periodIter += direction; //point periodIter to next period
		while((periodIter < mNumberOfPeriods) && (periodIter >= 0))
		{
			//Is empty/tiny period followed by another empty or tiny period
			if ((!mMPDParseHelper->IsEmptyPeriod(periodIter, (rate != AAMP_NORMAL_PLAY_RATE))) &&
				(mMPDParseHelper->GetPeriodDuration(periodIter,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB()) >= THRESHOLD_TOIGNORE_TINYPERIOD))
			{
				AAMPLOG_WARN("[CDAI] valid period(%s) after non-empty period(%s) found at index (%d)",
							mpd->GetPeriods().at(periodIter)->GetId().c_str(), mpd->GetPeriods().at(periodIdx)->GetId().c_str(), periodIter);
				bvalidperiodfound = true;
				break;
			}
			periodIter += direction;;
		}
		if(!bvalidperiodfound)
		{
			periodIter = periodIdx;
		}
	}
	return periodIter;
}

/**
 * @brief Function to update seek position
 *         Kept public as its called from outside StreamAbstraction class
 * @param[in] secondsRelativeToTuneTime - can be the offset (seconds from tune time) or absolute position (seconds from 1970)
 */
void StreamAbstractionAAMP_MPD::SeekPosUpdate(double secondsRelativeToTuneTime)
{
	if(secondsRelativeToTuneTime>=0.0)
	{
		seekPosition = secondsRelativeToTuneTime;
	}
	else
	{
		AAMPLOG_WARN("Invalid seek position %f, ignored", secondsRelativeToTuneTime);
	}
}

/**
 * @brief Function to notify first video pts value
 *
 * @param[in] pts
 * @param[in] timescale
 */
void StreamAbstractionAAMP_MPD::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
	double firstPTS = ((double)pts / (double)timeScale);
	if (!mIsFinalFirstPTS)
	{
		AAMPLOG_MIL("First PTS %lf -> %lf", mFirstPTS, firstPTS);
		mFirstPTS = firstPTS;
		mIsFinalFirstPTS = true;
	}
}

/**
 *  @brief Function to return return AvailabilityStartTime from the manifest
 *  @retval double . AvailabilityStartTime
 */
double StreamAbstractionAAMP_MPD::GetAvailabilityStartTime()
{
		return mMPDParseHelper?mMPDParseHelper->GetAvailabilityStartTime():0;
}

void StreamAbstractionAAMP_MPD::UpdateMPDPeriodDetails(std::vector<PeriodInfo>& currMPDPeriodDetails,uint64_t &durMs)
{
	auto periods = mpd->GetPeriods();
	for (int iter = 0; iter < periods.size(); iter++)
	{
		auto period = periods.at(iter);
		PeriodInfo periodInfo;
		periodInfo.periodId = period->GetId();
		periodInfo.duration = mMPDParseHelper->GetPeriodDuration(iter,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
		periodInfo.startTime = mMPDParseHelper->GetFirstSegmentStartTime(period);
		periodInfo.timeScale = mMPDParseHelper->GetPeriodSegmentTimeScale(period);
		periodInfo.periodIndex = iter;
		periodInfo.periodStartTime = mMPDParseHelper->GetPeriodStartTime(iter,mLastPlaylistDownloadTimeMs);
		periodInfo.periodEndTime = mMPDParseHelper->GetPeriodEndTime(iter,mLastPlaylistDownloadTimeMs,(rate != AAMP_NORMAL_PLAY_RATE),aamp->IsUninterruptedTSB());
		currMPDPeriodDetails.push_back(periodInfo);
		mMPDParseHelper->SetMPDPeriodDetails(currMPDPeriodDetails);
		if(!mMPDParseHelper->IsEmptyPeriod(iter, (rate != AAMP_NORMAL_PLAY_RATE)))
		{
			durMs += periodInfo.duration;
		}
	}
}

/**
 * @brief Get a timeline segment repeat count
 * @param[in] pMediaStreamContext Media stream context
 * @param[in] timeLineIndex Timeline index
 * @return The timeline segment repeat count or zero
 */
uint32_t StreamAbstractionAAMP_MPD::GetSegmentRepeatCount(MediaStreamContext *pMediaStreamContext, int timeLineIndex)
{
	uint32_t repeatCount = 0;
	SegmentTemplates segmentTemplates(pMediaStreamContext->representation->GetSegmentTemplate(),
									  pMediaStreamContext->adaptationSet->GetSegmentTemplate());

	if (segmentTemplates.HasSegmentTemplate())
	{
		const ISegmentTimeline *segmentTimeline = segmentTemplates.GetSegmentTimeline();
		if (segmentTimeline)
		{
			std::vector<ITimeline *>&timelines = segmentTimeline->GetTimelines();

			if ((int)timelines.size() > timeLineIndex)
			{
				repeatCount = timelines.at(timeLineIndex)->GetRepeatCount();
			}
		}
	}

	return repeatCount;
}

/**
* @fn SetSubtitleTrackOffset
* @brief Function to calculate the start time offset between subtitle and video tracks
*/
void StreamAbstractionAAMP_MPD::SetSubtitleTrackOffset()
{
	double videoSegmentStartTime = mMPDParseHelper->GetFirstSegmentScaledStartTime(mCurrentPeriod, eMEDIATYPE_VIDEO);
	double subtitleSegmentStartTime = mMPDParseHelper->GetFirstSegmentScaledStartTime(mCurrentPeriod, eMEDIATYPE_SUBTITLE);

	if (videoSegmentStartTime != -1 && subtitleSegmentStartTime != -1)
	{
		AAMPLOG_INFO("Calculating offset, videoSegmentStartTime=%lf, subtitleSegmentStartTime=%lf", videoSegmentStartTime, subtitleSegmentStartTime);
		double offsetInSecs = subtitleSegmentStartTime - videoSegmentStartTime;
		if (mMediaStreamContext[eMEDIATYPE_SUBTITLE]->playContext)
		{
			mMediaStreamContext[eMEDIATYPE_SUBTITLE]->playContext->setTrackOffset(offsetInSecs);
		}
	}
}

/**
 * @fn InitializeWorkers
 * @brief Initialize worker threads
 *
 * @return void
 */
void StreamAbstractionAAMP_MPD::InitializeWorkers()
{
	if(mTrackWorkers.empty())
	{
		for (int i = 0; i < mMaxTracks; i++)
		{
			mTrackWorkers.push_back(aamp_utils::make_unique<aamp::AampTrackWorker>(aamp, static_cast<AampMediaType>(i)));
		}
	}
}

/**
 * @fn UseIframeTrack
 * @brief Check if AAMP is using an iframe track
 *
 * @return true if AAMP is using an iframe track, false otherwise
 */
bool StreamAbstractionAAMP_MPD::UseIframeTrack(void)
{
	bool useIframe = trickplayMode;
	/* If using AAMP Local TSB and IFrame track extraction is enabled, iframe are extracted from the video track stored
	in TSB instead of the iframe track. */
	if (aamp->IsLocalAAMPTsb() && aamp->IsIframeExtractionEnabled())
	{
		useIframe = false;
	}
	return useIframe;
}

/**
 * @fn GetNextAdInBreak
 * @brief Get the next valid ad in the ad break
 * @param[in] direction will be 1 or -1 depending on the playback rate
 */
void StreamAbstractionAAMP_MPD::GetNextAdInBreak(int direction)
{
	if (direction == 1 || direction == -1)
	{
		mCdaiObject->mCurAdIdx += direction;
		while( mCdaiObject->mCurAdIdx < mCdaiObject->mCurAds->size() && mCdaiObject->mCurAdIdx >= 0 )
		{
			if( mCdaiObject->mCurAds->at(mCdaiObject->mCurAdIdx).invalid )
			{
				AAMPLOG_INFO("Ad index to be played[%d] is invalid, moving to next one",mCdaiObject->mCurAdIdx);
				mCdaiObject->mCurAdIdx += direction;
			}
			else
			{
				AAMPLOG_INFO("Ad index to be played[%d] is valid",mCdaiObject->mCurAdIdx);
				break;
			}
		}
	}
	else
	{
		AAMPLOG_ERR("Invalid value[%d] for direction, not expected!", direction);
	}
}

/*
 * @fn DoEarlyStreamSinkFlush
 * @brief Checks if the stream need to be flushed or not
 *
 * @param newTune true if this is a new tune, false otherwise
 * @param rate playback rate
 * @return true if stream should be flushed, false otherwise
 */
bool StreamAbstractionAAMP_MPD::DoEarlyStreamSinkFlush(bool newTune, float rate)
{
	/* Determine if early stream sink flush is needed based on configuration and playback state
	 * Do flush to PTS position from manifest when:
	 * 1. EnableMediaProcessor is disabled or EnableMediaProcessor enabled but segment timeline enabled (media processor will not flush in this case), OR
	 * 2. EnablePTSReStamp is disabled, or play rate is normal (AAMP_NORMAL_PLAY_RATE). Here, we are using the flush(0) that occurs else where
	 */
	bool enableMediaProcessor = ISCONFIGSET(eAAMPConfig_EnableMediaProcessor);
	bool enablePTSReStamp = ISCONFIGSET(eAAMPConfig_EnablePTSReStamp);
	bool doFlush = ((!enableMediaProcessor || mIsSegmentTimelineEnabled) &&
					(!enablePTSReStamp || rate == AAMP_NORMAL_PLAY_RATE));
	AAMPLOG_INFO("doFlush=%d, newTune=%d, rate=%f", doFlush, newTune, rate);
	return doFlush;
}

/**
 * @brief Should flush the stream sink on discontinuity or not.
 * When segment timeline is enabled, media processor will be in pass-through mode
 * and will not do delayed flush.
 * @return true if stream should be flushed, false otherwise
 */
bool StreamAbstractionAAMP_MPD::DoStreamSinkFlushOnDiscontinuity()
{
	/*
	 * Truth table for DoStreamSinkFlushOnDiscontinuity:
	 * | enableMediaProcessor | mIsSegmentTimelineEnabled | Result |
	 * |----------------------|---------------------------|--------|
	 * | false                | false                     | true   | (there will be no delayed flush)
	 * | false                | true                      | true   |
	 * | true                 | false                     | false  | (media processor will do delayed flush)
	 * | true                 | true                      | true   | (media processor will not flush)
	 */
	bool doFlush = (!ISCONFIGSET(eAAMPConfig_EnableMediaProcessor) || mIsSegmentTimelineEnabled);
	AAMPLOG_INFO("doFlush=%d", doFlush);
	return doFlush;
}
/**
 * @fn clearFirstPTS
 * @brief Clears the mFirstPTS value to trigger update of first PTS
 */
void StreamAbstractionAAMP_MPD::clearFirstPTS(void)
{
	mFirstPTS = 0.0;
}
