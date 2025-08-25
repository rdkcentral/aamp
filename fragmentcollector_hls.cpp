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
* @file fragmentcollector_hls.cpp
*
* Fragment Collect file includes class implementation for handling
* HLS streaming of AAMP player.
* Various functionality like manifest download , fragment collection,
* DRM initialization , synchronizing audio / video media of the stream,
* trick play handling etc are handled in this file .
*
*/
#include <assert.h>
#include "iso639map.h"
#include "fragmentcollector_hls.h"
#include "_base64.h"
#include "base16.h"
#include <algorithm> // for std::min
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "priv_aamp.h"
#include <signal.h>
#include <semaphore.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <openssl/sha.h>
#include <set>
#include <math.h>
#include <vector>
#include <string>
#include "HlsDrmBase.h"
#include "AampCacheHandler.h"
#include "PlayerHlsDrmSessionInterface.h"
#ifdef AAMP_VANILLA_AES_SUPPORT
#include "Aes.h"
#endif
#include "webvttParser.h"
#include "SubtecFactory.hpp"
#include "tsprocessor.h"
#include "isobmffprocessor.h"
#include "MetadataProcessor.hpp"
#include "AampUtils.h"
#include "AampStreamSinkManager.h"
#include "PlayerCCManager.h"

#include "VanillaDrmHelper.h"
#include "AampDRMLicManager.h"
static const int DEFAULT_STREAM_WIDTH = 720;
static const int DEFAULT_STREAM_HEIGHT = 576;
static const double  DEFAULT_STREAM_FRAMERATE = 25.0;

// checks if current state is going to use IFRAME ( Fragment/Playlist )
#define IS_FOR_IFRAME(rate, type) ((type == eTRACK_VIDEO) && (rate != AAMP_NORMAL_PLAY_RATE))

extern DrmHelperPtr ProcessContentProtection(std::string attrName, bool propagateURIParam , bool isSamplesRequired);

#define UseProgramDateTimeIfAvailable() (ISCONFIGSET(eAAMPConfig_HLSAVTrackSyncUsingStartTime) || aamp->mIsVSS)

/// Variable initialization for media profiler buckets
static const ProfilerBucketType mediaTrackBucketTypes[AAMP_TRACK_COUNT] =
	{ PROFILE_BUCKET_FRAGMENT_VIDEO, PROFILE_BUCKET_FRAGMENT_AUDIO, PROFILE_BUCKET_FRAGMENT_SUBTITLE, PROFILE_BUCKET_FRAGMENT_AUXILIARY };
/// Variable initialization for media decrypt buckets
static const ProfilerBucketType mediaTrackDecryptBucketTypes[AAMP_DRM_CURL_COUNT] =
	{ PROFILE_BUCKET_DECRYPT_VIDEO, PROFILE_BUCKET_DECRYPT_AUDIO, PROFILE_BUCKET_DECRYPT_SUBTITLE, PROFILE_BUCKET_DECRYPT_AUXILIARY};

/***************************************************************************
* @fn ParseKeyAttributeCallback
* @brief Callback function to decode Key and attribute
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] TrackState pointer for storage
* @return void
***************************************************************************/
static void ParseKeyAttributeCallback(lstring attrName, lstring valuePtr, void* arg)
{
	TrackState *ts = (TrackState *)arg;
	if (attrName.equal("METHOD"))
	{
		if (valuePtr.SubStringMatch("NONE"))
		{ // used by DAI
			if(ts->fragmentEncrypted)
			{
				if (!ts->mIndexingInProgress)
				{
					AAMPLOG_WARN("Track %s encrypted to clear ", ts->name);
					if(eTRACK_AUDIO == ts->type)
					{
						ts->fragmentEncChange = true;
					}
				}
				ts->fragmentEncrypted = false;
				ts->UpdateDrmCMSha1Hash("");
			}
			ts->mDrmMethod = eDRM_KEY_METHOD_NONE;
		}
		else if (valuePtr.SubStringMatch("AES-128"))
		{
			if(!ts->fragmentEncrypted)
			{
				if (!ts->mIndexingInProgress)
				{
					AAMPLOG_WARN("Track %s clear to encrypted ", ts->name);
					if(eTRACK_AUDIO == ts->type)
					{
						ts->fragmentEncChange = true;
					}
				}
				ts->fragmentEncrypted = true;
			}
			ts->mDrmInfo.method = eMETHOD_AES_128;
			ts->mDrmMethod = eDRM_KEY_METHOD_AES_128;
			ts->mKeyTagChanged = true;
			ts->mDrmInfo.keyFormat = VERIMATRIX_KEY_SYSTEM_STRING;
		}
		else if (valuePtr.SubStringMatch("SAMPLE-AES-CTR"))
		{

			if(!ts->fragmentEncrypted)
			{
				if (!ts->mIndexingInProgress)
				{
					AAMPLOG_WARN("Track %s clear to encrypted", ts->name);
					if(eTRACK_AUDIO == ts->type)
					{
						ts->fragmentEncChange = true;
					}
				}
				ts->fragmentEncrypted = true;
			}
			ts->mDrmMethod = eDRM_KEY_METHOD_SAMPLE_AES_CTR;
		}
		else if (valuePtr.SubStringMatch("SAMPLE-AES"))
		{
			ts->mDrmMethod = eDRM_KEY_METHOD_SAMPLE_AES;
			AAMPLOG_ERR("SAMPLE-AES unsupported");
		}
		else
		{
			ts->mDrmMethod = eDRM_KEY_METHOD_UNKNOWN;
			AAMPLOG_ERR("unsupported METHOD");
		}
	}
	else if (attrName.equal("KEYFORMAT"))
	{
		std::string keyFormat = valuePtr.GetAttributeValueString();
		ts->mDrmInfo.keyFormat = keyFormat;

		if( keyFormat.rfind("urn:uuid:",0)==0 )
		{
			ts->mDrmInfo.systemUUID = keyFormat.substr(9);
		}
	}
	else if (attrName.equal("URI"))
	{
		std::string uri;
		if( !valuePtr.startswith(CHAR_QUOTE) && !valuePtr.equal("NONE") )
		{
			// Handling keys with relative URIs
			// This condition is used to extract key URI from unquoted / NONE strings
			uri = valuePtr.tostring();
		}
		else
		{
			uri = valuePtr.GetAttributeValueString();
		}
		if( !uri.empty() )
		{
			ts->mIVKeyChanged = (ts->mDrmInfo.keyURI != uri);
			ts->mDrmInfo.keyURI = uri;
		}
	}
	else if (attrName.equal("IV"))
	{ // 16 bytes
		if( valuePtr.removePrefix("0x") || valuePtr.removePrefix("0X") )
		{
			std::string temp = valuePtr.tostring();
			ts->UpdateDrmIV(temp);
			ts->mDrmInfo.bUseMediaSequenceIV = false;
		}
	}
	else if (attrName.equal("CMSha1Hash"))
	{ // 20 bytes; Metadata Hash.
		if( valuePtr.removePrefix("0x") || valuePtr.removePrefix("0X") )
		{
			std::string temp = valuePtr.tostring();
			ts->UpdateDrmCMSha1Hash(temp);
		}
	}
}

/***************************************************************************
* @fn ParseTileInfCallback
* @brief Callback function to decode Key and attribute
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] Updated TileInfo structure instance
* @return void
***************************************************************************/
static void ParseTileInfCallback(lstring attrName, lstring valuePtr, void* arg)
{
	// #EXT-X-TILES:RESOLUTION=416x234,LAYOUT=9x17,DURATION=2.002
	TileLayout *var = (TileLayout *)arg;
	if (attrName.equal("LAYOUT"))
	{
		std::string temp = valuePtr.tostring();
		sscanf(temp.c_str(), "%dx%d", &var->numCols, &var->numRows);
	}
	else if (attrName.equal("DURATION"))
	{
		var->posterDuration = valuePtr.atof();
	}
}

/***************************************************************************
* @fn ParseXStartAttributeCallback
* @brief Callback function to decode XStart attributes
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] TrackState pointer for storage
* @return void
***************************************************************************/
static void ParseXStartAttributeCallback(lstring attrName, lstring valuePtr, void* arg)
{
	HLSXStart *var = (HLSXStart *)arg;
	if (attrName.equal("TIME-OFFSET"))
	{
		var->offset = valuePtr.atof();
	}
	else if (attrName.equal("PRECISE"))
	{
		// Precise attribute is not considered . By default NO option is selected
		var->precise = false;
	}
}

/***************************************************************************
* @fn ParseStreamInfCallback
* @brief Callback function to extract stream tag attributes
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] StreamAbstractionAAMP_HLS pointer for storage
* @return void
***************************************************************************/
static void ParseStreamInfCallback( lstring attrName, lstring valuePtr, void* arg)
{
	StreamAbstractionAAMP_HLS *context = (StreamAbstractionAAMP_HLS *) arg;
	HlsStreamInfo &streamInfo = context->streamInfoStore[context->GetTotalProfileCount()];
	if (attrName.equal("URI"))
	{
		streamInfo.uri = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("BANDWIDTH"))
	{
		streamInfo.bandwidthBitsPerSecond = valuePtr.atol();
	}
	else if (attrName.equal("PROGRAM-ID"))
	{
		streamInfo.program_id = valuePtr.atol();
	}
	else if (attrName.equal("AUDIO"))
	{
		streamInfo.audio = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("CODECS"))
	{
		streamInfo.codecs = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("RESOLUTION"))
	{
		std::string temp = valuePtr.tostring();
		sscanf(temp.c_str(), "%dx%d", &streamInfo.resolution.width, &streamInfo.resolution.height);
	}
	// following are rarely present
	else if (attrName.equal("AVERAGE-BANDWIDTH"))
	{
		streamInfo.averageBandwidth = valuePtr.atol();
	}
	else if (attrName.equal("FRAME-RATE"))
	{
		streamInfo.resolution.framerate = valuePtr.atof();
	}
	else if (attrName.equal("CLOSED-CAPTIONS"))
	{
		streamInfo.closedCaptions = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("SUBTITLES"))
	{
		streamInfo.subtitles = valuePtr.GetAttributeValueString();
	}
	else
	{
		AAMPLOG_INFO("unknown stream inf attribute %.*s", attrName.getLen(), attrName.getPtr() );
	}
}

/***************************************************************************
* @fn ParseMediaAttributeCallback
* @brief Callback function to extract media tag attributes
*
* @param attrName[in] input string
* @param delimEqual[in] delimiter string
* @param fin[in] string end pointer
* @param arg[out] StreamAbstractionAAMP_HLS pointer for storage
* @return void
***************************************************************************/
static void ParseMediaAttributeCallback( lstring attrName, lstring valuePtr, void *arg)
{
	StreamAbstractionAAMP_HLS *context = (StreamAbstractionAAMP_HLS *) arg;
	MediaInfo &mediaInfo = context->mediaInfoStore[context->GetMediaCount()];
	/*
	#EXT - X - MEDIA:TYPE = AUDIO, GROUP - ID = "g117600", NAME = "English", LANGUAGE = "en", DEFAULT = YES, AUTOSELECT = YES
	#EXT - X - MEDIA:TYPE = AUDIO, GROUP - ID = "g117600", NAME = "Spanish", LANGUAGE = "es", URI = "HBOHD_HD_NAT_15152_0_5939026565177792163/format-hls-track-sap-bandwidth-117600-repid-root_audio103.m3u8"
	*/
	if (attrName.equal("TYPE"))
	{
		if (valuePtr.SubStringMatch("AUDIO"))
		{
			mediaInfo.type = eMEDIATYPE_AUDIO;
		}
		else if (valuePtr.SubStringMatch("VIDEO"))
		{
			mediaInfo.type = eMEDIATYPE_VIDEO;
		}
		else if (valuePtr.SubStringMatch("SUBTITLES"))
		{
			mediaInfo.type = eMEDIATYPE_SUBTITLE;
		}
		else if (valuePtr.SubStringMatch("CLOSED-CAPTIONS"))
		{
			mediaInfo.type = eMEDIATYPE_SUBTITLE;
			mediaInfo.isCC = true;
		}
	}
	else if (attrName.equal("GROUP-ID"))
	{
		mediaInfo.group_id = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("NAME"))
	{
		mediaInfo.name = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("LANGUAGE"))
	{
		mediaInfo.language = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("AUTOSELECT"))
	{
		if (valuePtr.SubStringMatch("YES"))
		{
			mediaInfo.autoselect = true;
		}
	}
	else if (attrName.equal("DEFAULT"))
	{
		if (valuePtr.SubStringMatch("YES"))
		{
			mediaInfo.isDefault = true;
		}
		else
		{
			mediaInfo.isDefault = false;
		}
	}
	else if (attrName.equal("URI"))
	{
		mediaInfo.uri = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("CHANNELS"))
	{
		mediaInfo.channels = valuePtr.atoi();
	}
	else if (attrName.equal("INSTREAM-ID"))
	{
		mediaInfo.instreamID = valuePtr.GetAttributeValueString();
	}
	else if (attrName.equal("FORCED"))
	{
		if (valuePtr.SubStringMatch("YES"))
		{
			mediaInfo.forced = true;
		}
	}
	else if (attrName.equal("CHARACTERISTICS"))
	{
		mediaInfo.characteristics = valuePtr.GetAttributeValueString();
	}
	else
	{
		AAMPLOG_WARN("unk MEDIA attr %.*s", attrName.getLen(), attrName.getPtr() );
	}
}

/***************************************************************************
* @fn ParseXStartTimeOffset
* @brief Helper function to Parse XStart Tag and attributes
*
* @param arg[in] char *ptr , string with X-START
* @return double offset value
***************************************************************************/
static AampTime ParseXStartTimeOffset(lstring xstartStr)
{
	HLSXStart xstart;
	xstartStr.ParseAttrList(ParseXStartAttributeCallback, &xstart);
	AampTime retOffSet = xstart.offset;
	return retOffSet;
}

void static setupStreamInfo(HlsStreamInfo & streamInfo)
{
	streamInfo.resolution.width = DEFAULT_STREAM_WIDTH;
	streamInfo.resolution.height = DEFAULT_STREAM_HEIGHT;
	streamInfo.resolution.framerate = DEFAULT_STREAM_FRAMERATE;
}

/**
 *  @brief Function to initiate drm process
 */
void StreamAbstractionAAMP_HLS::InitiateDrmProcess()
{
	/** If fragments are CDM encrypted KC **/
	if (aamp->fragmentCdmEncrypted && ISCONFIGSET(eAAMPConfig_Fragmp4PrefetchLicense))
	{
		std::lock_guard<std::mutex> guard(aamp->drmParserMutex);
		DrmHelperPtr drmHelperToUse = nullptr;
		for (int i = 0; i < aamp->aesCtrAttrDataList.size(); i++ )
		{
			if (!aamp->aesCtrAttrDataList.at(i).isProcessed)
			{
				aamp->aesCtrAttrDataList.at(i).isProcessed = true;
				DrmHelperPtr drmHelper = ProcessContentProtection( aamp->aesCtrAttrDataList.at(i).attrName, ISCONFIGSET(eAAMPConfig_PropagateURIParam),  aamp->isDecryptClearSamplesRequired());
				if (nullptr != drmHelper)
				{
					/* This needs effort from MSO as to what they want to do viz-a-viz preferred DRM, */
					drmHelperToUse = drmHelper;
				}
			}
		}
		if ((drmHelperToUse != nullptr) && (aamp->mDRMLicenseManager))
		{
			AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
			/** Queue protection event to the pipeline **/
			licenseManager->QueueProtectionEvent(drmHelperToUse, "1", 0, eMEDIATYPE_VIDEO);
			/** Queue content protection in DRM license fetcher **/
			licenseManager->QueueContentProtection(drmHelperToUse, "1", 0, eMEDIATYPE_VIDEO);
		}
	}
}

static void LogUnknownTag( lstring ptr, int count, const char *ignoreList[] )
{
	for( int i=0; i<count; i++ )
	{
		if( ptr.removePrefix(ignoreList[i]) )
		{
			return;
		}
	}
	int len = ptr.getLen();
	if( len>24 ) len = 24; // clamp
	AAMPLOG_INFO("***unknown tag:%.*s", len, ptr.getPtr() );
}

/**
 *  @brief Function to parse main manifest
 */
AAMPStatusType StreamAbstractionAAMP_HLS::ParseMainManifest()
{
	int vProfileCount, iFrameCount , lineNum;
	AAMPStatusType retval = eAAMPSTATUS_OK;
	mMediaCount = 0;
	mProfileCount = 0;
	vProfileCount = iFrameCount = lineNum = 0;
	aamp->mhAbrManager.clearProfiles();
	bool useavgbw = ISCONFIGSET(eAAMPConfig_AvgBWForABR);
	streamInfoStore.clear();
	mediaInfoStore.clear();

	lstring iter = lstring(mainManifest.GetPtr(),mainManifest.GetLen());
	while( !iter.empty() )
	{
		lstring ptr = iter.mystrpbrk();
		if( !ptr.empty() )
		{
			if (ptr.removePrefix("#EXT"))
			{
				if (ptr.removePrefix("-X-I-FRAME-STREAM-INF:"))
				{
					streamInfoStore.emplace_back(HlsStreamInfo{});
					HlsStreamInfo &streamInfo = streamInfoStore[mProfileCount];
					ptr.ParseAttrList(ParseStreamInfCallback, this);
					if (streamInfo.uri.empty() )
					{ // uri on following line
						streamInfo.uri = iter.mystrpbrk().tostring();
					}
					if(streamInfo.averageBandwidth !=0 && useavgbw)
					{
						streamInfo.bandwidthBitsPerSecond = streamInfo.averageBandwidth;
					}
					streamInfo.isIframeTrack = true;
					streamInfo.enabled = false;
					iFrameCount++;
					mProfileCount++;
					mIframeAvailable = true;
				}
				else if (ptr.removePrefix("-X-IMAGE-STREAM-INF:"))
				{
					streamInfoStore.emplace_back(HlsStreamInfo{});
					HlsStreamInfo &streamInfo = streamInfoStore[mProfileCount];
					ptr.ParseAttrList(ParseStreamInfCallback, this);
					if (streamInfo.uri.empty())
					{ // uri on following line
						streamInfo.uri = iter.mystrpbrk().tostring();
					}
					if(streamInfo.averageBandwidth !=0 && useavgbw)
					{
						streamInfo.bandwidthBitsPerSecond = streamInfo.averageBandwidth;
					}
					streamInfo.isIframeTrack = true;
					streamInfo.enabled = false;
					mProfileCount++;
				}
				else if (ptr.removePrefix("-X-STREAM-INF:"))
				{
					streamInfoStore.emplace_back(HlsStreamInfo{});
					HlsStreamInfo &streamInfo = streamInfoStore[mProfileCount];
					setupStreamInfo(streamInfo);
					ptr.ParseAttrList(ParseStreamInfCallback, this);
					if (streamInfo.uri.empty())
					{ // uri on following line
						streamInfo.uri = iter.mystrpbrk().tostring();
					}
					if(streamInfo.averageBandwidth!=0 && useavgbw)
					{
						streamInfo.bandwidthBitsPerSecond = streamInfo.averageBandwidth;
					}
					const FormatMap *map = GetAudioFormatForCodec(streamInfo.codecs.c_str());
					if( map )
					{
						streamInfo.audioFormat = map->format;
					}
					else
					{
						streamInfo.audioFormat = FORMAT_UNKNOWN;
					}
					streamInfo.enabled = false;
					mProfileCount++;
					vProfileCount++;
				}
				else if (ptr.removePrefix("-X-MEDIA:"))
				{
					mediaInfoStore.emplace_back(MediaInfo{});
					ptr.ParseAttrList(ParseMediaAttributeCallback, this);
					if( mediaInfoStore[mMediaCount].language.empty() )
					{ // handle non-compliant manifest missing language attribute
						mediaInfoStore[mMediaCount].language =  mediaInfoStore[mMediaCount].name;
					}
					if (mediaInfoStore[mMediaCount].type == eMEDIATYPE_AUDIO && !mediaInfoStore[mMediaCount].language.empty() )
					{
						mLangList.insert(GetLanguageCode(mMediaCount));
					}
					mMediaCount++;
				}
				else if (ptr.removePrefix("M3U"))
				{
					// Spec :: 4.3.1.1.  EXTM3U - It MUST be the first line of every Media Playlist and every Master Playlist
					if(lineNum)
					{
						AAMPLOG_WARN("M3U tag not the first line[%d] of Manifest",lineNum);
						retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
						break;
					}
				}
				else if (ptr.removePrefix("-X-START:"))
				{ // i.e. "TIME-OFFSET=2.336, PRECISE=YES" - specifies the preferred point in the video to start playback; not yet supported

					// check if App has not configured any liveoffset
					{
						AampTime offsetval{ParseXStartTimeOffset(ptr)};
						if (offsetval != 0.0)
						{
							if(!aamp->IsLiveAdjustRequired())
							{
								SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_CDVRLiveOffset, abs(offsetval));
							}
							else
							{
								SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_LiveOffset, abs(offsetval));
							}
							aamp->UpdateLiveOffset();
							AAMPLOG_WARN("WARNING:found EXT-X-START in MainManifest Offset:%f  liveOffset:%f",offsetval.inSeconds(),aamp->mLiveOffset);
						}
					}
				}
				else if (ptr.removePrefix("INF:"))
				{
					// its not a main manifest, instead its playlist given for playback . Consider not a error
					// Report it , so that Init flow can be changed accordingly
					retval = eAAMPSTATUS_PLAYLIST_PLAYBACK;
					break;
				}
				else if (ptr.removePrefix("-X-SESSION-KEY:"))
				{
					if (ISCONFIGSET(eAAMPConfig_Fragmp4PrefetchLicense))
					{
						std::string KeyTagStr = ptr.tostring();
						{
							std::lock_guard<std::mutex> guard(aamp->drmParserMutex);
							attrNameData* aesCtrAttrData = new attrNameData(KeyTagStr);
							if (std::find(aamp->aesCtrAttrDataList.begin(), aamp->aesCtrAttrDataList.end(),
										  *aesCtrAttrData) == aamp->aesCtrAttrDataList.end()) {
								AAMPLOG_INFO("Adding License data from Main Manifest %s",KeyTagStr.c_str());
								aamp->aesCtrAttrDataList.push_back(*aesCtrAttrData);
							}
							SAFE_DELETE(aesCtrAttrData);
						}
						aamp->fragmentCdmEncrypted = true;
						InitiateDrmProcess();
					}
				}
				else
				{
					static const char *ignoreList[] = {
						"-X-VERSION:", "-X-INDEPENDENT-SEGMENTS", "-X-FAXS-CM", "-X-CONTENT-IDENTIFIER:", "-X-FOG",
						"-X-XCAL-CONTENTMETADATA","-NOM-I-FRAME-DISTANCE","-X-ADVERTISING","-UPLYNK-LIVE","-X-TARGETDURATION",
						"-X-DISCONTINUITY", "-X-KEY", "-X-CUE" };
					LogUnknownTag( ptr, ARRAY_SIZE(ignoreList), ignoreList );
				}
				lineNum++;
			}
		}
	}// while till end of file

	if(retval == eAAMPSTATUS_OK)
	{
		// Check if there are are valid profiles to do playback
		if(vProfileCount == 0)
		{
			AAMPLOG_WARN("ERROR No video profiles available in manifest for playback");
			retval = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}
		else
		{	// If valid video profiles , check for Media and iframe and report warnings only
			if(mMediaCount == 0)
			{
				// just a warning .Play the muxed content with default audio
				AAMPLOG_WARN("WARNING !!! No media definitions in manifest for playback");
			}
			if(iFrameCount == 0)
			{
				// just a warning
				AAMPLOG_WARN("WARNING !!! No iframe definitions .Trickplay not supported");
			}

			// if separate Media exists then find the codec for the audio type
			for (auto& media : mediaInfoStore)
			{
				if( media.type == eMEDIATYPE_AUDIO && !media.group_id.empty() )
				{
					//Find audio codec from X-STREAM-INF: or streamInfo
					for (auto& stream : streamInfoStore)
					{
						//Find the X-STREAM_INF having same group id as audio track to parse codec info
						if (!stream.isIframeTrack && !stream.audio.empty() &&
								stream.audio==media.group_id )
						{
							// assign the audioformat from streamInfo to mediaInfo
							media.audioFormat = stream.audioFormat;
							break;
						}
					}
				}
			}
		}
		aamp->StoreLanguageList(mLangList); // For playlist playback, there will be no languages available
	}
	return retval;
}


/**
 * @brief Function to get fragment URI from index count
 */
lstring TrackState::GetIframeFragmentUriFromIndex(bool &bSegmentRepeated)
{
	lstring uri;
	auto count = index.size();
	if( count>0 )
	{
		const IndexNode *idxNode = NULL;
		if( context->rate > 0 )
		{
			const IndexNode &lastIndexNode = index[count - 1];
			AampTime seekWindowEnd{lastIndexNode.completionTimeSecondsFromStart - aamp->mLiveOffset};
			if (IsLive() && playTarget > seekWindowEnd)
			{
				AAMPLOG_WARN("rate - %f playTarget(%f) > seekWindowEnd(%f), forcing EOS",
							 context->rate, playTarget.inSeconds(), seekWindowEnd.inSeconds());
				return uri;
			}
			if( currentIdx == -1 )
			{ // search forward from beginning
				currentIdx = 0;
			}
			for( int idx = currentIdx; idx < count; idx++ )
			{ // search in direction until out-of-bounds
				if( index[idx].completionTimeSecondsFromStart >= playTarget )
				{ // found target iframe
					currentIdx = idx;
					idxNode = &index[idx];
					break;
				}
			}
		}
		else
		{
			if(-1 == currentIdx)
			{ // search backward from end
				currentIdx = (int)(count - 1);
			}
			for( int idx = currentIdx; idx >= 0; idx-- )
			{ // search in direction until out-of-bounds
				if( index[idx].completionTimeSecondsFromStart <= playTarget )
				{ // found target iframe
					currentIdx = idx;
					idxNode = &index[idx];
					break;
				}
			}
		}
		if( idxNode )
		{
			// For Fragmented MP4 check if initFragment injection is required
			if ( !idxNode->initFragmentPtr.empty() &&
				(mInitFragmentInfo.empty() || !mInitFragmentInfo.equal(idxNode->initFragmentPtr)) )
			{
				mInitFragmentInfo = idxNode->initFragmentPtr;
				mInjectInitFragment = true;
			}
			byteRangeOffset = 0;
			byteRangeLength = 0;
			lstring fragmentInfo = idxNode->pFragmentInfo;
			fragmentDurationSeconds = idxNode->completionTimeSecondsFromStart.inSeconds();
			if( currentIdx > 0 )
			{
				fragmentDurationSeconds -= index[currentIdx - 1].completionTimeSecondsFromStart.inSeconds();
			}
			if(lastDownloadedIFrameTarget != -1.0 && idxNode->completionTimeSecondsFromStart == lastDownloadedIFrameTarget)
			{ // found playtarget  and lastdownloaded target on same segment .
				bSegmentRepeated = true;
			}
			else
			{ // diff segment
				bSegmentRepeated = false;
				lastDownloadedIFrameTarget = idxNode->completionTimeSecondsFromStart;
			}
			while (fragmentInfo.startswith('#'))
			{
                const char *fragmentPtr = fragmentInfo.getPtr();
                size_t offs = fragmentPtr - playlist.GetPtr();
                lstring iter( fragmentPtr, playlist.GetLen() - offs );
                fragmentInfo = iter.mystrpbrk(); // #EXTINF
                fragmentInfo = iter.mystrpbrk(); // #EXT-X-BYTERANGE (or url)
				if( IsExtXByteRange(fragmentInfo, &byteRangeLength, &byteRangeOffset) )
                {
                    fragmentInfo = iter.mystrpbrk(); // url
                }
            }

			{
				mFragmentURIFromIndex = fragmentInfo;
				uri = mFragmentURIFromIndex;

				//The EXT-X-TARGETDURATION tag specifies the maximum Media Segment   duration.
				//The EXTINF duration of each Media Segment in the Playlist   file, when rounded to the nearest integer,
				//MUST be less than or equal   to the target duration
				if(!uri.empty() && std::round(fragmentDurationSeconds) > targetDurationSeconds)
				{
					AAMPLOG_WARN("WARN - Fragment duration[%f] > TargetDuration[%f] for URI:%.*s",fragmentDurationSeconds, targetDurationSeconds.inSeconds(), uri.getLen(), uri.getPtr() );
				}
			}

			if (-1 == idxNode->drmMetadataIdx)
			{
				fragmentEncrypted = false;
			}
			else
			{
				fragmentEncrypted = true;
				// for each iframe , need to see if KeyTag changed and get the drminfo .
				// Get the key Index position .
				int keyIndexPosn = idxNode->drmMetadataIdx;
				if(keyIndexPosn != mLastKeyTagIdx)
				{
					AAMPLOG_WARN("[%d] KeyTable Size [%d] keyIndexPosn[%d] lastKeyIdx[%d]",type, (int)mKeyHashTable.size(), keyIndexPosn, mLastKeyTagIdx);
					if(keyIndexPosn < mKeyHashTable.size() )
					{
						std::string &keyStr = mKeyHashTable[keyIndexPosn].mKeyTagStr;
						lstring key = lstring( keyStr.c_str(), keyStr.size() );
						if( !key.empty() )
						{
							key.ParseAttrList( ParseKeyAttributeCallback, this );
						}
					}
					mKeyTagChanged = true;
					mLastKeyTagIdx = keyIndexPosn;
				}
			}
		}
		else
		{
			AAMPLOG_WARN("Couldn't find node - rate %f playTarget %f",
						 context->rate, playTarget.inSeconds());
		}
	}
	return uri;
}

/**
 * @brief Function to get next fragment URI from playlist based on playtarget
 */
lstring TrackState::GetNextFragmentUriFromPlaylist(bool& reloadUri, bool ignoreDiscontinuity)
{
	lstring rc;

	auto p = fragmentURI.getPtr(); // pointer inside playlist
	auto l = playlist.GetLen();
	size_t offs = p - playlist.GetPtr(); // offset from playlist start
	if( offs>=l ) return rc;
	lstring iter( p, l-offs );

	size_t byteRangeLength = 0; // default, when optional byterange offset is left unspecified
	size_t byteRangeOffset = 0;
	bool discontinuity = false;
	const char* programDateTime = NULL;

	if (playTarget < 0)
	{
		AAMPLOG_WARN("[!!!!! WARNING !!!!] Invalid playTarget %f, so set playTarget to 0", playTarget.inSeconds());
		playTarget = 0;
		//return fragmentURI; // leads to buffer overrun/crash
	}
	if ((playlistPosition == playTarget)
			|| (isFirstFragmentAfterABR && (type == eTRACK_VIDEO) && (-1.0 != playlistPosition) && (playlistPosition.seconds() == playTarget.seconds())))
			// Check the playposition and playtarget matches in case of fragment duration mismatch after changing profile in ABR.
	{
		isFirstFragmentAfterABR = false;

		//AAMPLOG_WARN("[PLAYLIST_POSITION==PLAY_TARGET]");
		return fragmentURI;
	}

	if ( playlistPosition != -1.0 && !fragmentURI.empty() )
	{ // already presenting - skip past previous segment
		//AAMPLOG_WARN("[PLAYLIST_POSITION!= -1]");
		iter.mystrpbrk();
	}
	if ((playlistPosition > playTarget) && (fragmentDurationSeconds > PLAYLIST_TIME_DIFF_THRESHOLD_SECONDS) &&
		((playlistPosition - playTarget) > fragmentDurationSeconds))
	{
		AAMPLOG_WARN("playlistPosition[%f] > playTarget[%f] more than last fragmentDurationSeconds[%f]",
					playlistPosition.inSeconds(), playTarget.inSeconds(), fragmentDurationSeconds);
	}
	if (-1 == playlistPosition)
	{
		// Starts parsing from beginning, so change to default
		fragmentEncrypted = false;
	}

	//AAMPLOG_WARN("before loop, ptr = %p fragmentURI %p", ptr, fragmentURI);
	while (!iter.empty())
	{
        lstring ptr = iter.mystrpbrk();
		if(!ptr.empty())
		{
			if (ptr.removePrefix("#EXT"))
			{ // tags begins with #EXT
				if (ptr.removePrefix("M3U"))
				{ // "Extended M3U file" - always first line
				}
				else if (ptr.removePrefix("INF:"))
				{// precedes each advertised fragment in a playlist
					if (-1 != playlistPosition)
					{
						playlistPosition += fragmentDurationSeconds;
					}
					else
					{
						playlistPosition = 0;
					}
					fragmentDurationSeconds = ptr.atof();
				}
				else if( IsExtXByteRange(ptr,&byteRangeLength,&byteRangeOffset) )
                { // -X-BYTERANGE:
					mByteOffsetCalculation = true;
					if (0 != byteRangeLength && 0 == byteRangeOffset)
					{
						byteRangeOffset = this->byteRangeOffset + this->byteRangeLength;
					}
					AAMPLOG_TRACE("byteRangeOffset:%zu Last played fragment Offset:%zu byteRangeLength:%zu Last fragment Length:%zu", byteRangeOffset, this->byteRangeOffset, byteRangeLength, this->byteRangeLength);
				}
				else if (ptr.removePrefix("-X-TARGETDURATION:"))
				{ // max media segment duration; required; appears once
					targetDurationSeconds = ptr.atof();
				}
				else if (ptr.removePrefix("-X-MEDIA-SEQUENCE:"))
				{// first media URI's unique integer sequence number
					nextMediaSequenceNumber = ptr.atoll();
				}
				else if (ptr.removePrefix("-X-KEY:"))
				{ // identifies licensing server to contact for authentication
					ptr.ParseAttrList(ParseKeyAttributeCallback, this);
				}
				else if(ptr.removePrefix("-X-MAP:"))
				{
					if( !mInitFragmentInfo.equal(ptr) )
					{
						mInitFragmentInfo = ptr;
						mInjectInitFragment = true;
						AAMPLOG_INFO("Found #EXT-X-MAP data: %.*s", mInitFragmentInfo.getLen(), mInitFragmentInfo.getPtr() );
					}
				}
				else if (ptr.removePrefix("-X-PROGRAM-DATE-TIME:"))
				{ // associates following media URI with absolute date/time
					// if used, should supplement any EXT-X-DISCONTINUITY tags
					if (context->mNumberOfTracks > 1)
					{
						programDateTime = ptr.getPtr();
						// The first X-PROGRAM-DATE-TIME tag holds the start time for each track
						if (startTimeForPlaylistSync == 0.0 )
						{
							/* discarding timezone assuming audio and video tracks has same timezone and we use this time only for synchronization*/
							startTimeForPlaylistSync = ISO8601DateTimeToUTCSeconds(ptr.getPtr());
							AAMPLOG_WARN("%s StartTimeForPlaylistSync : %f ", name, startTimeForPlaylistSync.inSeconds());
						}
					}
				}
				else if (ptr.removePrefix("-X-ALLOW-CACHE:"))
				{ // YES or NO - authorizes client to cache segments for later replay
					if (ptr.removePrefix("YES"))
					{
						context->allowsCache = true;
					}
					else if (ptr.removePrefix("NO"))
					{
						context->allowsCache = false;
					}
					else
					{
						AAMPLOG_ERR("unknown ALLOW-CACHE setting");
					}
				}
				else if (ptr.removePrefix("-X-ENDLIST"))
				{ // indicates that no more media segments are available
					AAMPLOG_WARN("#EXT-X-ENDLIST");
					mReachedEndListTag = true;
				}
				else if ( ptr.removePrefix("-X-DISCONTINUITY"))
				{
					if( !ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) )
					{ // ignore discontinuities when presenting muxed hls/ts
						discontinuity = true;
					}
				}
				else
				{
					static const char *ignoreList[] = {
						"-X-PLAYLIST-TYPE:", "-X-I-FRAMES-ONLY", "-X-VERSION:", "-X-FAXS-CM:", "-X-FAXS-PACKAGINGCERT", "-X-FAXS-SIGNATURE", "-X-CUE", "-X-CM-SEQUENCE", "-X-MARKER", "-X-MAP", "-X-MEDIA-TIME", "-X-END-TOP-TAGS", "-X-CONTENT-IDENTIFIER", "-X-TRICKMODE-RESTRICTION", "-X-INDEPENDENT-SEGMENTS", "-X-BITRATE", "-X-FOG", "-UPLYNK-LIVE", "-X-START:", "-X-XCAL-CONTENTMETADATA", "-NOM-I-FRAME-DISTANCE", "-X-ADVERTISING", "-X-SOURCE-STREAM", "-X-X1-LIN-CK", "-X-SCTE35", "-X-ASSET", "-X-CUE-OUT", "-X-CUE-IN", "-X-DATERANGE", "-X-SPLICEPOINT-SCTE35" };
					LogUnknownTag( ptr, ARRAY_SIZE(ignoreList), ignoreList );
				}
			}
			else if ( ptr.startswith('#') )
			{ // all other lines beginning with # are comments
			}
			else
			{ // URI
				nextMediaSequenceNumber++;
				if (((playlistPosition + fragmentDurationSeconds) > playTarget) || ((playTarget - playlistPosition) < PLAYLIST_TIME_DIFF_THRESHOLD_SECONDS))
				{
					//AAMPLOG_WARN("Return fragment %s playlistPosition %f playTarget %f", ptr, playlistPosition, playTarget.inSeconds());
					this->byteRangeOffset = byteRangeOffset;
					this->byteRangeLength = byteRangeLength;
					mByteOffsetCalculation = false;
					if (discontinuity)
					{
						if (!ignoreDiscontinuity)
						{
							AAMPLOG_WARN("#EXT-X-DISCONTINUITY in track[%d] playTarget %f total mCulledSeconds %f", type, playTarget.inSeconds(), mCulledSeconds.inSeconds());
							// Check if X-DISCONTINUITY tag is seen without explicit X-MAP tag
							// Reuse last parsed/seen X-MAP tag in such cases
							if (!mInitFragmentInfo.empty() && mInjectInitFragment == false)
							{
								mInjectInitFragment = true;
								AAMPLOG_WARN("Reusing last seen #EXT-X-MAP for this discontinuity, data: %.*s", mInitFragmentInfo.getLen(), mInitFragmentInfo.getPtr() );
							}

							TrackType otherType = (type == eTRACK_VIDEO)? eTRACK_AUDIO: eTRACK_VIDEO;
							TrackState *other = context->trackState[otherType];
							if (other->enabled)
							{
								AampTime diff{};
								AampTime position{};
								AampTime playPosition{playTarget - mCulledSeconds};
								AampTime iCulledSeconds{mCulledSeconds};
								AampTime iProgramDateTime{mProgramDateTime};
								if (!programDateTime)
								{
									position = playPosition;
								}
								else
								{
									position = ISO8601DateTimeToUTCSeconds(programDateTime );
									AAMPLOG_WARN("[%s] Discontinuity - position from program-date-time %f", name, position.inSeconds());
								}

								// To get last playlist indexed time reference
								long long playlistIndexedTimeMS = lastPlaylistIndexedTimeMS;
								// Release playlist lock before entering into discontinuity mutex
								ReleasePlaylistLock();
								AAMPLOG_WARN("%s Acquiring Discontinuity mutex playlist Indexed:%lld", name, playlistIndexedTimeMS);
								// Acquire discontinuity ongoing lock
								bool isDiffChkReq=true;
								{
									std::lock_guard<std::mutex> guard(context->mDiscoCheckMutex);
									AAMPLOG_WARN("%s Checking HasDiscontinuity for position :%f, playposition :%f playtarget:%f", name, position.inSeconds(), playPosition.inSeconds(), playTarget.inSeconds());
									bool result = other->HasDiscontinuityAroundPosition(position.inSeconds(), (NULL != programDateTime), diff, playPosition.inSeconds(), iCulledSeconds.inSeconds(), iProgramDateTime.inSeconds(), isDiffChkReq);

									if (false == result)
									{
										AAMPLOG_WARN("[%s] Ignoring discontinuity as %s track does not have discontinuity", name, other->name);
										discontinuity = false;
									}
								}
								AAMPLOG_WARN("%s Released Discontinuity mutex last playlist Indexed:%lld", name, lastPlaylistIndexedTimeMS);
								AcquirePlaylistLock();
								if (playlistIndexedTimeMS != lastPlaylistIndexedTimeMS)
								{
									reloadUri = true;
									AAMPLOG_WARN("[%s] Playlist Changed on discontinuity wait, reloading uri prev:%lld cur:%lld", name, playlistIndexedTimeMS, lastPlaylistIndexedTimeMS);
									nextMediaSequenceNumber--;
									fragmentURI = FindMediaForSequenceNumber();
									return fragmentURI;
								}

								if (discontinuity && programDateTime && isDiffChkReq)
								{
									AAMPLOG_WARN("[WARN] [%s] diff %f with other track discontinuity position!!", name, diff.inSeconds());

									/*If other track's discontinuity is in advanced position more than the targetDuration, then skip it.*/
									if (diff > targetDurationSeconds)
									{
										/*Skip fragment*/
										AAMPLOG_WARN("[WARN!! Can be a Stream Issue!!] [%s] Other track's discontinuity time greater by %f. updating playTarget %f to %f",
											name, diff.inSeconds(), playTarget.inSeconds(), playlistPosition.inSeconds() + diff.inSeconds());
										mSyncAfterDiscontinuityInProgress = true;
										playTarget = playlistPosition + diff;
										discontinuity = false;
										programDateTime = NULL;
										ptr = iter.mystrpbrk();
										continue;
									}
								}

							}
						}
						else
						{
							discontinuity = false;
						}
					}
					this->discontinuity = discontinuity || mSyncAfterDiscontinuityInProgress;
					mSyncAfterDiscontinuityInProgress = false;
					AAMPLOG_TRACE("  [%s] Discontinuity: %s", name, (discontinuity ? "true" : "false"));

					if (type == eTRACK_AUDIO && this->discontinuity && fragmentEncChange && ISCONFIGSET(eAAMPConfig_ReconfigPipelineOnDiscontinuity))
					{
						fragmentEncChange = false;
						context->SetESChangeStatus();
					}
					rc = ptr;
					//The EXT-X-TARGETDURATION tag specifies the maximum Media Segment   duration.
					//The EXTINF duration of each Media Segment in the Playlist   file,
					//when rounded to the nearest integer,
					//MUST be less than or equal   to the target duration
					if(!rc.empty() && std::round(fragmentDurationSeconds) > targetDurationSeconds)
					{
						AAMPLOG_WARN("WARN - Fragment duration[%f] > TargetDuration[%f] for URI:%.*s", fragmentDurationSeconds, targetDurationSeconds.inSeconds(), rc.getLen(), rc.getPtr() );
					}
					break;
				}
				else
				{
					if(mByteOffsetCalculation)
					{
						byteRangeOffset += byteRangeLength;
					}
					discontinuity = false;
					programDateTime = NULL;
					// AAMPLOG_WARN("Skipping fragment %s playlistPosition %f playTarget %f", ptr, playlistPosition, playTarget);
				}
			}
		}
	}
	return rc;
} // GetNextFragmentUriFromPlaylist

/**
 * @brief Get fragment tag based on media sequence number
 *        Function to find the media sequence after refresh for continuity
 */
lstring TrackState::FindMediaForSequenceNumber()
{
	lstring iter = lstring( playlist.GetPtr(), playlist.GetLen() );
	long long mediaSequenceNumber = nextMediaSequenceNumber - 1;
	std::string key;
	lstring initFragment;
	long long seq = 0;
	while (!iter.empty())
	{
		lstring ptr = iter.mystrpbrk();
		if (!ptr.empty())
		{
			if (ptr.removePrefix("#EXTINF:"))
			{
				fragmentDurationSeconds = ptr.atof();
			}
			else if (ptr.removePrefix("#EXT-X-MEDIA-SEQUENCE:"))
			{
				seq = ptr.atoll();
			}
			else if (ptr.removePrefix("#EXT-X-KEY:"))
			{
				key = ptr.tostring();
			}
			else if (ptr.removePrefix("#EXT-X-MAP:"))
			{
				initFragment = ptr;
			}
			else if( !ptr.startswith('#') )
			{ // URI
				if (seq >= mediaSequenceNumber)
				{
					if ((mDrmKeyTagCount >1) && !key.empty() )
					{
						lstring temp = lstring(key.c_str(),key.size());
						temp.ParseAttrList(ParseKeyAttributeCallback, this);
					}
					if( !initFragment.empty() )
					{
						// mInitFragmentInfo will be cleared after calling FlushIndex() from IndexPlaylist()
						if( mInitFragmentInfo.empty() )
						{
							mInitFragmentInfo = initFragment;
							AAMPLOG_INFO("Found #EXT-X-MAP data: %.*s", initFragment.getLen(), initFragment.getPtr() );
						}
					}
					if (seq != mediaSequenceNumber)
					{
						AAMPLOG_WARN("seq gap %lld!=%lld", seq, mediaSequenceNumber);
						nextMediaSequenceNumber = seq + 1;
					}
					return ptr;
				}
				seq++;
			}
		}
	} // !iter.empty()
	return lstring();
}


/**
 * @brief Helper function to download fragment
 */
bool TrackState::FetchFragmentHelper(int &http_error, bool &decryption_error, bool & bKeyChanged, int * fogError, double &downloadTime)
{
		http_error = 0;
		bool bSegmentRepeated = false;
		AcquirePlaylistLock();
		if (context->UseIframeTrack() && ABRManager::INVALID_PROFILE != context->GetIframeTrack())
		{
			// Note :: only for IFrames , there is a possibility of same segment getting downloaded again
			// Target of next download is not based on segment duration but fixed interval .
			// If iframe segment duration is 1.001sec , and based on rate if increment is happening at 1sec
			// same segment will be downloaded twice .
			AampTime delta{context->rate / context->mTrickPlayFPS};
			fragmentURI = GetIframeFragmentUriFromIndex(bSegmentRepeated);
			if (context->rate < 0)
			{ // rewind
				if (fragmentURI.empty() || (playTarget == 0))
				{
					AAMPLOG_WARN("aamp rew to beginning");
					eosReached = true;
				}
				else if (playTarget > -delta)
				{
					playTarget += delta;
				}
				else
				{
					playTarget = 0;
				}
			}
			else
			{// fast-forward
				if (fragmentURI.empty())
				{
					AAMPLOG_WARN("aamp ffw to end");
					eosReached = true;
				}
				playTarget += delta;
			}

			//AAMPLOG_WARN("Updated playTarget to %f", playTarget);
		}
		else
		{// normal speed
			bool reloadUri = false;

			fragmentURI = GetNextFragmentUriFromPlaylist(reloadUri);
			if (!fragmentURI.empty() || reloadUri)
			{
				// Reload fragment uri, if playlist refreshed during other track discontinuity wait
				while (!fragmentURI.empty() && reloadUri)
				{
					AAMPLOG_WARN("Reload FragmentURI due to playlist Change");
					reloadUri = false;
					fragmentURI = GetNextFragmentUriFromPlaylist(reloadUri);
				}
				if (!fragmentURI.empty() )
				{
					if (!mInjectInitFragment)
						playTarget = playlistPosition + fragmentDurationSeconds;

					if (IsLive())
					{
						context->CheckForPlaybackStall(true);
					}
				}
			}

			if (fragmentURI.empty() )
			{
				if ((!IsLive() || mReachedEndListTag) && (playlistPosition != -1))
				{
					AAMPLOG_WARN("aamp play to end. playTarget %f fragmentURI %p ReachedEndListTag %d Type %d", playTarget.inSeconds(), fragmentURI.getPtr(), mReachedEndListTag,type);
					eosReached = true;
				}
				else if (IsLive() && type == eTRACK_VIDEO)
				{
					context->CheckForPlaybackStall(false);
				}
			}
		}
		getNextFetchRequestUri();

		if (!mInjectInitFragment && !fragmentURI.empty() && !bSegmentRepeated)
		{
			std::string fragmentUrl;
			CachedFragment* cachedFragment = GetFetchBuffer(true);
			std::string temp = fragmentURI.tostring();
			aamp_ResolveURL(fragmentUrl, mEffectiveUrl, temp.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
			ReleasePlaylistLock();
			AAMPLOG_TRACE("Got next fragment url %s fragmentEncrypted %d discontinuity %d mDrmMethod %d", fragmentUrl.c_str(), fragmentEncrypted, (int)discontinuity, mDrmMethod);

			aamp->profiler.ProfileBegin(mediaTrackBucketTypes[type]);
			const char *range;
			char rangeStr[MAX_RANGE_STRING_CHARS];
			if (byteRangeLength)
			{
				size_t next = byteRangeOffset + byteRangeLength;
				snprintf(rangeStr, sizeof(rangeStr), "%zu-%zu", byteRangeOffset, next - 1);
				AAMPLOG_WARN("FetchFragmentHelper rangeStr %s ", rangeStr);

				range = rangeStr;
			}
			else
			{
				range = NULL;
			}
			// if fragment URI uses relative path, we don't want to replace effective URI
			std::string tempEffectiveUrl;
			AAMPLOG_TRACE(" Calling Getfile . buffer %p avail %d", &cachedFragment->fragment, (int)cachedFragment->fragment.GetAvail());
			double downloadTime = 0;
			
			cachedFragment->discontinuityIndex = 0;
			if( ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) )
			{ // TODO: optimize me
				lstring iter = lstring( playlist.GetPtr(), playlist.GetLen() );
				while( !iter.empty() )
				{
					lstring ptr = iter.mystrpbrk();
					if( !ptr.empty() )
					{
						if( ptr.getPtr() >= fragmentURI.getPtr() )
						{
							break;
						}
						if(ptr.removePrefix("#EXT-X-DISCONTINUITY-SEQUENCE:"))
						{ // expected to appear once
							cachedFragment->discontinuityIndex = ptr.atoll();
						}
						else if( ptr.removePrefix("#EXT-X-DISCONTINUITY"))
						{
							cachedFragment->discontinuityIndex++;
						}
					}
					else
					{
						break;
					}
				}
			}
			
			bool fetched = aamp->GetFile(fragmentUrl, (AampMediaType)(type), &cachedFragment->fragment,
			 tempEffectiveUrl, &http_error, &downloadTime, range, type, false, NULL, NULL, fragmentDurationSeconds);
			//Workaround for 404 of subtitle fragments
			//TODO: This needs to be handled at server side and this workaround has to be removed
			if (!fetched && http_error == 404 && type == eTRACK_SUBTITLE)
			{
				cachedFragment->fragment.AppendBytes( "WEBVTT", 7);
				fetched = true;
			}
			if (!fetched)
			{
				//cleanup is done in aamp_GetFile itself

				aamp->profiler.ProfileError(mediaTrackBucketTypes[type], http_error);
				aamp->profiler.ProfileEnd(mediaTrackBucketTypes[type]);
				if (mSkipSegmentOnError)
				{
					// Skipping segment on error, increase fail count
					segDLFailCount += 1;
				}
				else
				{
					// Already attempted rampdown on same segment
					// Skip segment if there is no profile to rampdown.
					mSkipSegmentOnError = true;
				}
				if (AampLogManager::isLogworthyErrorCode(http_error))
				{
					AAMPLOG_WARN("FetchFragmentHelper aamp_GetFile failed");
				}
				//Adding logic to report error if fragment downloads are failing continuously
				//Avoid sending error for failure to download subtitle fragments
				int FragmentDownloadFailThreshold = GETCONFIGVALUE(eAAMPConfig_FragmentDownloadFailThreshold);
				if((FragmentDownloadFailThreshold <= segDLFailCount) && aamp->DownloadsAreEnabled() && type != eTRACK_SUBTITLE)
				{
					AAMPLOG_ERR("Not able to download fragments; reached failure threshold sending tune failed event");
					abortWaitForVideoPTS();
					aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, http_error);
				}
				cachedFragment->fragment.Free();
				lastDownloadedIFrameTarget = -1;
				return false;
			}
			else
			{
				// Track the end of buffer from the last downloaded fragment
				// Use the playlistPosition instead of a rolling count in case segments are dropped
				playTargetBufferCalc = playlistCulledOffset + playlistPosition + fragmentDurationSeconds;
			}

			if((eTRACK_VIDEO == type)  && (aamp->IsFogTSBSupported()))
			{
				std::size_t pos = fragmentUrl.find(FOG_FRAG_BW_IDENTIFIER);
				if (pos != std::string::npos)
				{
					std::string bwStr = fragmentUrl.substr(pos + FOG_FRAG_BW_IDENTIFIER_LEN);
					if (!bwStr.empty())
					{
						pos = bwStr.find(FOG_FRAG_BW_DELIMITER);
						if (pos != std::string::npos)
						{
							bwStr = bwStr.substr(0, pos);
							context->SetTsbBandwidth(std::stol(bwStr));
						}
					}
				}
			}

			aamp->profiler.ProfileEnd(mediaTrackBucketTypes[type]);
			segDLFailCount = 0;

			if (cachedFragment->fragment.GetLen() && fragmentEncrypted && mDrmMethod == eDRM_KEY_METHOD_AES_128)
			{
				// DrmDecrypt resets mKeyTagChanged , take a back up here to give back to caller
				bKeyChanged = mKeyTagChanged;
				{
					/*
					 * From RFC8216 - Section 5.2,
					 * An EXT-X-KEY tag with a KEYFORMAT of "identity" that does not have an
					 * IV attribute indicates that the Media Sequence Number is to be used
					 * as the IV when decrypting a Media Segment, by putting its big-endian
					 * binary representation into a 16-octet (128-bit) buffer and padding
					 * (on the left) with zeros.
					 * Eg:
					 * When manifest has no IV attribute as below, client has to create IV data.
					 * #EXT-X-KEY:METHOD=AES-128,URI="https://someserver.com/hls.key"
					 * When manifest has IV attribute as below, client will use this IV, no need to create IV data.
					 * #EXT-X-KEY:METHOD=AES-128,URI="https://someserver.com/hls.key",IV=0xABCDABCD
					 */
					if ( eMETHOD_AES_128 == mDrmInfo.method && true == mDrmInfo.bUseMediaSequenceIV )
					{
						CreateInitVectorByMediaSeqNo( nextMediaSequenceNumber-1 );
						// seed the newly created IV to corresponding DRM instance
						mKeyTagChanged = true;
					}

					AAMPLOG_TRACE(" [%s] uri %s - calling  DrmDecrypt()",name, fragmentUrl.c_str());
					DrmReturn drmReturn = DrmDecrypt(cachedFragment, mediaTrackDecryptBucketTypes[type]);

					if(eDRM_SUCCESS != drmReturn)
					{
						if (aamp->DownloadsAreEnabled())
						{
							AAMPLOG_WARN("FetchFragmentHelper : drm_Decrypt failed. fragmentURL %s - RetryCount %d", fragmentUrl.c_str(), segDrmDecryptFailCount);
							if (eDRM_KEY_ACQUISITION_TIMEOUT == drmReturn)
							{
								decryption_error = true;
								AAMPLOG_WARN("FetchFragmentHelper : drm_Decrypt failed due to license acquisition timeout");
								aamp->SendErrorEvent(AAMP_TUNE_LICENCE_TIMEOUT, NULL, true);
							}
							else
							{
								/* Added to send tune error when fragments decryption failed */
								segDrmDecryptFailCount +=1;

								if(aamp->mDrmDecryptFailCount <= segDrmDecryptFailCount)
								{
									decryption_error = true;
									AAMPLOG_ERR("FetchFragmentHelper : drm_Decrypt failed for fragments, reached failure threshold (%d) sending failure event", aamp->mDrmDecryptFailCount);
									aamp->SendErrorEvent(AAMP_TUNE_DRM_DECRYPT_FAILED);
								}
							}
						}
						cachedFragment->fragment.Free();
						lastDownloadedIFrameTarget = -1;
						return false;
					}
					segDrmDecryptFailCount = 0; /* Resetting the retry count in the case of decryption success */
				}
				if (!context->firstFragmentDecrypted)
				{
					aamp->NotifyFirstFragmentDecrypted();
					context->firstFragmentDecrypted = true;
				}
			}
			else if(!cachedFragment->fragment.GetLen())
			{
				AAMPLOG_WARN("fragment. len zero for %s", fragmentUrl.c_str());
			}
		}
		else
		{
			bool ret = false;
			if (mInjectInitFragment)
			{
				AAMPLOG_INFO("FetchFragmentHelper : Found init fragment playTarget(%f), playlistPosition(%f)", playTarget.inSeconds(), playlistPosition.inSeconds());
				ret = true; // we need to ret success here to avoid failure cases in FetchFragment
			}
			else
			{
				// null fragment URI technically not an error - live manifest may simply not have updated yet
				// if real problem exists, underflow will eventually be detected/reported
				AAMPLOG_INFO("FetchFragmentHelper : fragmentURI %.*s playTarget(%f), playlistPosition(%f)",
							 fragmentURI.getLen(), fragmentURI.getPtr(), playTarget.inSeconds(), playlistPosition.inSeconds());
			}
			ReleasePlaylistLock();
			return ret;
		}
		return true;
}

/**
 * @brief Function to update skip duration on PTS restamp
 */
void TrackState::updateSkipPoint(double position, double duration )
{
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
	{
		if(playContext)
		{
			playContext->updateSkipPoint( position,duration );
		}
	}
}

/**
 *  @brief Function to set discontinuity state
 */
void TrackState::setDiscontinuityState(bool isDiscontinuity)
{
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
	{
		if(playContext)
		{
			playContext->setDiscontinuityState(isDiscontinuity);
		}
	}
 }

/**
 *  @brief Function to abort wait for video PTS
 */
 void TrackState::abortWaitForVideoPTS()
 {
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
	{
		if(playContext)
		{
			AAMPLOG_WARN(" %s abort waiting for video PTS arrival",name );
		playContext->abortWaitForVideoPTS();
		}
	}
 }
/**
 * @brief Function to Fetch the fragment and inject for playback
 */
void TrackState::FetchFragment()
{
	int timeoutMs = -1;
	int http_error = 0;
	double downloadTime = 0;
	bool decryption_error = false;
	if (IsLive())
	{
		timeoutMs = context->maxIntervalBtwPlaylistUpdateMs - (int) (aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
		if(timeoutMs < 0)
		{
			timeoutMs = 0;
		}
	}
	if (!WaitForFreeFragmentAvailable(timeoutMs))
	{
		return;
	}
	//set the rampdown flag to false .
	mCheckForRampdown = false;
	bool bKeyChanged = false;
	int iFogErrorCode = -1;
	int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
	if (aamp->DownloadsAreEnabled() && !abort)
	{
		if(false == FetchFragmentHelper(http_error, decryption_error, bKeyChanged, &iFogErrorCode, downloadTime))
		{
			if (!fragmentURI.empty() )
			{
				// Profile RampDown check and rampdown is needed only for Video . If audio fragment download fails
				// should continue with next fragment,no retry needed .
				if (eTRACK_VIDEO == type && http_error != 0 && aamp->CheckABREnabled())
				{
					// Check whether player reached rampdown limit, then rampdown
					if(!context->CheckForRampDownLimitReached())
					{
						if (context->CheckForRampDownProfile(http_error))
						{
							if (context->rate == AAMP_NORMAL_PLAY_RATE)
							{
								playTarget -= fragmentDurationSeconds;
							}
							else
							{
								playTarget -= context->rate / context->mTrickPlayFPS;
							}
							//-- if rampdown attempted , then set the flag so that abr is not attempted.
							mCheckForRampdown = true;
							// Rampdown attempt success, download same segment from lower profile.
							mSkipSegmentOnError = false;

							AAMPLOG_WARN("Error while fetching fragment:%s, failedCount:%d. decrementing profile", name, segDLFailCount);
						}
						else
						{
							double duration = fragmentDurationSeconds;
							double position = (double)(playTarget - playTargetOffset);
							AAMPLOG_WARN("%s Already at the lowest profile, skipping segment at pos = %lf duration=%lf",name,position,duration);
							updateSkipPoint(position, duration);
							context->mRampDownCount = 0;
						}
					}
				}
				else if (decryption_error)
				{
					AAMPLOG_ERR("%s Error while decrypting fragments. failedCount:%d", name, segDLFailCount);
				}
				else if (AampLogManager::isLogworthyErrorCode(http_error))
				{
					double duration = fragmentDurationSeconds;
					double position = (double)(playTarget - playTargetOffset);
					AAMPLOG_ERR("Error on fetching %s fragment. failedCount:%d %lf %lf", name, segDLFailCount,duration, position);
					updateSkipPoint(position, duration);
				}

			}
			else
			{
				// technically not an error - live manifest may simply not have updated yet
				// if real problem exists, underflow will eventually be detected/reported
				AAMPLOG_TRACE("NULL fragmentURI for %s track ", name);
			}

			// in case of tsb, GetCurrentBandWidth does not return correct bandwidth as it is updated after this point
			// hence getting from context which is updated in FetchFragmentHelper
			long lbwd = aamp->IsFogTSBSupported() ? context->GetTsbBandwidth() : this->GetCurrentBandWidth();
			//update videoend info
			aamp->UpdateVideoEndMetrics((IS_FOR_IFRAME(iCurrentRate, type) ? eMEDIATYPE_IFRAME : (AampMediaType)(type)),
										lbwd,
										((iFogErrorCode > 0) ? iFogErrorCode : http_error), this->mEffectiveUrl, fragmentDurationSeconds, downloadTime, bKeyChanged, fragmentEncrypted);
			return;
		}

		if (mInjectInitFragment)
		{
			return;
		}

		if (eTRACK_VIDEO == type)
		{
			// reset rampdown count on success
			context->mRampDownCount = 0;
		}

		CachedFragment* cachedFragment = GetFetchBuffer(false);
		if (cachedFragment->fragment.GetPtr())
		{
			AampTime duration{fragmentDurationSeconds};
			AampTime position{playTarget - playTargetOffset};
			if (context->rate == AAMP_NORMAL_PLAY_RATE)
			{
				position -= fragmentDurationSeconds;
				cachedFragment->discontinuity = discontinuity;
			}
			else
			{
				position -= context->rate / context->mTrickPlayFPS;
				cachedFragment->discontinuity = true;
				AAMPLOG_TRACE("rate %f position %f", context->rate, position.inSeconds());
			}

			if (context->trickplayMode && (0 != context->rate))
			{
				duration = floor(duration * context->rate / context->mTrickPlayFPS);
			}
			cachedFragment->duration = duration.inSeconds();
			cachedFragment->position = position.inSeconds();
			cachedFragment->absPosition = playlistPosition.inSeconds();
			// in case of tsb, GetCurrentBandWidth does not return correct bandwidth as it is updated after this point
			// hence getting from context which is updated in FetchFragmentHelper
			long lbwd = aamp->IsFogTSBSupported() ? context->GetTsbBandwidth() : this->GetCurrentBandWidth();

			// update videoend info
			aamp->UpdateVideoEndMetrics( (IS_FOR_IFRAME(iCurrentRate,type)? eMEDIATYPE_IFRAME:(AampMediaType)(type) ),
									lbwd,
									((iFogErrorCode > 0 ) ? iFogErrorCode : http_error), this->mEffectiveUrl, cachedFragment->duration, downloadTime, bKeyChanged, fragmentEncrypted);

			const auto early_processing = aamp->mConfig->IsConfigSet(eAAMPConfig_EarlyID3Processing);
			if (early_processing && playContext && aamp->IsEventListenerAvailable(AAMP_EVENT_ID3_METADATA))
			{
				if (const auto & metadata_processor = context->GetMetadataProcessor(mSourceFormat))
				{
					AAMPLOG_INFO(" Processing info # discontinuity: %s - position: %f - duration: %f", (discontinuity ? "true" : "false"), position.inSeconds(), duration.inSeconds());

					const AampMediaType media_type = static_cast<AampMediaType>(type);
					const AampTime proc_position{context->mStartTimestampZero ? 0.0 : cachedFragment->position};

					metadata_processor->ProcessFragmentMetadata(cachedFragment, media_type,
						discontinuity, proc_position.inSeconds(),
						ptsError, fragmentURI.tostring() );
				}
				else
				{
					AAMPLOG_WARN(" %p [%u] - Unable to instantiate or retrieve metadata processor.",
						this, static_cast<uint16_t>(type));
				}
			}
		}
		else
		{
			AAMPLOG_WARN("%s cachedFragment->fragment.ptr is NULL", name);
		}
		mSkipAbr = false; //To enable ABR since we have cached fragment after init fragment
		UpdateTSAfterFetch(false);
	}
}

void TrackState::resetPTSOnAudioSwitch(CachedFragment* cachedFragment)
{
	if (playContext)
	{
		AAMPLOG_WARN("%s pos=%lf dur=%lf", name,cachedFragment->position,cachedFragment->duration);
		playContext->resetPTSOnAudioSwitch(&cachedFragment->fragment, cachedFragment->position);
	}
}

/**
 * @brief Injected decrypted fragment for playback
 */
void TrackState::InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded,bool isDiscontinuity)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		return;
	}
	if (playContext)
	{
		AampTime position{};
		if(!context->mStartTimestampZero || streamOutputFormat == FORMAT_ISO_BMFF)
		{
			position = cachedFragment->position;
		}

		MediaProcessor::process_fcn_t processor = [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
		{
			// This case has already been dealt with
			if (type == eMEDIATYPE_DSM_CC && streamOutputFormat != FORMAT_ISO_BMFF)
			{
				// NOP
			}
			else
			{
				aamp->SendStreamCopy(type, buf.data(), buf.size(), info.pts_s, info.dts_s, info.duration);
			}
		};
		
		if( demuxOp == eStreamOp_DEMUX_ALL && ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) )
		{
			if( context->mPtsOffsetMap.count(cachedFragment->discontinuityIndex)==0 )
			{ // compute muxed AV track pts offset and save for use by subtitle track
				double firstPts = playContext->getFirstPts(&cachedFragment->fragment);
				double ptsOffset = m_totalDurationForPtsRestamping - firstPts;
				AAMPLOG_MIL( "video pts_offset[%lld]=%lldms", cachedFragment->discontinuityIndex, llround(ptsOffset*1000) );
				playContext->setPtsOffset( ptsOffset );
				context->mPtsOffsetMap[cachedFragment->discontinuityIndex] = ptsOffset;
			}
			m_totalDurationForPtsRestamping += cachedFragment->duration;
		}
		
		fragmentDiscarded = !playContext->sendSegment( &cachedFragment->fragment,
			position.inSeconds(),
			cachedFragment->duration,
			cachedFragment->PTSOffsetSec,
			isDiscontinuity,
			cachedFragment->initFragment,
			processor,
			ptsError );
	}
	else
	{
		fragmentDiscarded = false;
		aamp->SendStreamCopy(
							 (AampMediaType)type,
							 cachedFragment->fragment.GetPtr(),
							 cachedFragment->fragment.GetLen(),
							 cachedFragment->position,
							 cachedFragment->position,
							 cachedFragment->duration);
	}
} // InjectFragmentInternal

/***************************************************************************
* @fn GetCompletionTimeForFragment
* @brief Function to get end time of fragment
*
* @param trackState[in] TrackState structure
* @param mediaSequenceNumber[in] sequence number
* @return double end time
***************************************************************************/
static AampTime GetCompletionTimeForFragment(const TrackState *trackState, long long mediaSequenceNumber)
{
	AampTime rc{};
	int indexCount = (int)trackState->index.size(); // number of fragments
	if (indexCount>0)
	{
		int idx = (int)(mediaSequenceNumber - trackState->indexFirstMediaSequenceNumber);
		if (idx >= 0)
		{
			if (idx >= indexCount)
			{ // clamp
				idx = indexCount - 1;
			}
			const IndexNode &node = trackState->index[idx];
			rc = node.completionTimeSecondsFromStart; // pick up from indexed playlist
		}
		else
		{
			AAMPLOG_WARN("bad index! mediaSequenceNumber=%lld, indexFirstMediaSequenceNumber=%lld", mediaSequenceNumber, trackState->indexFirstMediaSequenceNumber);
		}
	}
	return rc;
}

/**
 * @brief Function to flush all stored data before refresh and stop
 */
void TrackState::FlushIndex()
{
	index.clear();
	indexFirstMediaSequenceNumber = 0;
	mProgramDateTime = 0.0; // new member - stored first program date time (if any) from playlist
	currentIdx = -1;
	mDrmKeyTagCount = 0;
	mLastKeyTagIdx = -1;
	mDeferredDrmKeyMaxTime = 0;
	mKeyHashTable.clear();
	mDiscontinuityIndex.clear();
	int drmMetaDataIndexCount = (int)mDrmMetaDataIndex.size();
	if( drmMetaDataIndexCount )
	{
		AAMPLOG_TRACE("TrackState::[%s]mDrmMetaDataIndexCount %d", name, drmMetaDataIndexCount);
		for (int i = 0; i < drmMetaDataIndexCount; i++)
		{
			DrmMetadataNode &drmMetadataNode = mDrmMetaDataIndex[i];
			if( drmMetadataNode.sha1Hash.empty() )
			{
				AAMPLOG_ERR("TrackState: **** metadataPtr/sha1Hash is NULL, give attention and analyze it... mDrmMetaDataIndexCount[%d]", drmMetaDataIndexCount);
			}
			drmMetadataNode.sha1Hash.clear();
		}
		mDrmMetaDataIndex.clear();
		mDrmMetaDataIndexPosition = 0;
	}
	mInitFragmentInfo.clear();
}

/**
 * @brief Function to set DRM Context when KeyTag changes
 */
void TrackState::SetDrmContext()
{
	// Set the appropriate DrmContext for Decryption
	// This function need to be called where KeyMethod != None is found after indexplaylist
	// or when new KeyMethod is found , None to AES or between AES with different Method
	// or between KeyMethod when IV or URL changes (for Vanilla AES)

	//CID:93939 - Removed the drmContextUpdated variable which is initialized but not used
	mDrmInfo.bPropagateUriParams = ISCONFIGSET(eAAMPConfig_PropagateURIParam);
	if (PlayerHlsDrmSessionInterface::getInstance()->isDrmSupported(mDrmInfo))
	{
		// OCDM-based DRM decryption is available via the HLS OCDM bridge
		AAMPLOG_INFO("Drm support available");
		mDrmInterface->RegisterHlsInterfaceCb( PlayerHlsDrmSessionInterface::getInstance());
		mDrm = PlayerHlsDrmSessionInterface::getInstance()->createSession( mDrmInfo,(int)(type));
		if (!mDrm)
		{
			AAMPLOG_WARN("Failed to create Drm Session");
		}
	}
	else
	{
		// No DRM helper located, assuming standard AES encryption
#ifdef AAMP_VANILLA_AES_SUPPORT
		AAMPLOG_INFO("StreamAbstractionAAMP_HLS::Get AesDec");
		mDrm = AesDec::GetInstance();
		mDrmInterface->RegisterAesInterfaceCb((std::shared_ptr <HlsDrmBase>) mDrm);

		aamp->setCurrentDrm(std::make_shared<VanillaDrmHelper>());

#else
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS: AAMP_VANILLA_AES_SUPPORT not defined");
#endif
	}

	if(mDrm)
	{
		mDrmInterface->UpdateAamp(aamp);
		mDrm->SetDecryptInfo( &mDrmInfo,  aamp->mConfig->GetConfigValue(eAAMPConfig_LicenseKeyAcquireWaitTime) );
	}
}

/**
 * @brief Function to to handle parse and indexing of individual tracks
 */
void TrackState::IndexPlaylist(bool IsRefresh, AampTime &culledSec)
{
	AampTime totalDuration{};
	AampTime prevProgramDateTime{mProgramDateTime};
	long long commonPlayPosition = nextMediaSequenceNumber - 1;
	AampTime prevSecondsBeforePlayPoint{};
	lstring initFragmentPtr;
	uint64_t discontinuitySequenceIndex = 0;

	if(IsRefresh && !UseProgramDateTimeIfAvailable())
	{
		prevSecondsBeforePlayPoint = GetCompletionTimeForFragment(this, commonPlayPosition);
	}

	FlushIndex();
	mIndexingInProgress = true;
	lstring iter = lstring(playlist.GetPtr(),playlist.GetLen());
	if( !iter.empty() ){
		lstring ptr = iter.mystrpbrk();
		if( !ptr.equal("#EXTM3U") )
		{
			int numChars = iter.getLen();
			if( numChars>MANIFEST_TEMP_DATA_LENGTH ) numChars = MANIFEST_TEMP_DATA_LENGTH;
			AAMPLOG_ERR("ERROR: Invalid Playlist URL:%s ", mPlaylistUrl.c_str());
			AAMPLOG_ERR("ERROR: Invalid Playlist DATA:%.*s ", numChars, iter.getPtr() );
			aamp->SendErrorEvent(AAMP_TUNE_INVALID_MANIFEST_FAILURE);
			mDuration = totalDuration.inSeconds();
			return;
		}
		IndexNode node;
		int drmMetadataIdx = -1;
		//CID:100252,131 , 93918 - Removed the deferDrmVal,endOfTopTags and deferDrmTagPresent variable which is initialized but never used
		bool mediaSequence = false;
		const char* programDateTimeIdxOfFragment = NULL;
		bool discontinuity = false;
		bool pdtAtTopAvailable=false;

		mDrmInfo.mediaFormat = eMEDIAFORMAT_HLS;
		mDrmInfo.manifestURL = mEffectiveUrl;
		mDrmInfo.masterManifestURL = aamp->GetManifestUrl();
		mDrmInfo.initData = aamp->GetDrmInitData();
		mDrmInfo.bDecryptClearSamplesRequired = aamp->isDecryptClearSamplesRequired();
		AampTime fragDuration{};

		while(!iter.empty())
		{
			ptr = iter.mystrpbrk();
			lstring prev = ptr;
			if( ptr.removePrefix("#EXT"))
			{
				if (ptr.removePrefix("INF:"))
				{
					if (discontinuity)
					{
						DiscontinuityIndexNode discontinuityIndexNode;
						discontinuityIndexNode.discontinuitySequenceIndex = discontinuitySequenceIndex;
						discontinuityIndexNode.fragmentIdx = (int)index.size();
						discontinuityIndexNode.position = totalDuration.inSeconds();
						discontinuityIndexNode.discontinuityPDT = 0.0;
						if (programDateTimeIdxOfFragment)
						{
							discontinuityIndexNode.discontinuityPDT = ISO8601DateTimeToUTCSeconds(programDateTimeIdxOfFragment);
						}
						discontinuityIndexNode.fragmentDuration = ptr.atof();
						mDiscontinuityIndex.push_back(discontinuityIndexNode);
						discontinuity = false;
					}
					programDateTimeIdxOfFragment = NULL;
					node.pFragmentInfo = prev; //Point back to beginning of #EXTINF
					fragDuration = ptr.atof();
					totalDuration += fragDuration;
					node.completionTimeSecondsFromStart = totalDuration.inSeconds();
					node.drmMetadataIdx = drmMetadataIdx;
					node.initFragmentPtr = initFragmentPtr;
					if (node.mediaSequenceNumber != -1)
					{
						node.mediaSequenceNumber++;
					}
					index.push_back(node);
				}
				else if(ptr.removePrefix("-X-MEDIA-SEQUENCE:"))
				{
					indexFirstMediaSequenceNumber = ptr.atoll();
					mediaSequence = true;
					node.mediaSequenceNumber = indexFirstMediaSequenceNumber;
				}
				else if(ptr.removePrefix("-X-TARGETDURATION:"))
				{
					targetDurationSeconds = ptr.atof();
					AAMPLOG_INFO("aamp: EXT-X-TARGETDURATION = %f", targetDurationSeconds.inSeconds());
				}
				else if(ptr.removePrefix("-X-X1-LIN-CK:"))
				{
					// get the deferred drm key acquisition time
					mDeferredDrmKeyMaxTime = ptr.atoi();
					AAMPLOG_INFO("#EXT-X-LIN [%d]",mDeferredDrmKeyMaxTime);
				}
				else if(ptr.removePrefix("-X-PLAYLIST-TYPE:"))
				{
					// EVENT or VOD (optional); VOD if playlist will never change
					if (ptr.removePrefix("VOD"))
					{
						AAMPLOG_WARN("aamp: EXT-X-PLAYLIST-TYPE - VOD");
						mPlaylistType = ePLAYLISTTYPE_VOD;
					}
					else if (ptr.removePrefix("EVENT"))
					{
						AAMPLOG_WARN("aamp: EXT-X-PLAYLIST-TYPE = EVENT");
						mPlaylistType = ePLAYLISTTYPE_EVENT;
					}
					else
					{
						AAMPLOG_ERR("unknown PLAYLIST-TYPE");
					}
				}
				else if(ptr.removePrefix("-X-FAXS-CM:"))
				{
					// AVE DRM Not supported
				}
				else if(ptr.removePrefix("-X-DISCONTINUITY-SEQUENCE:"))
				{
					discontinuitySequenceIndex = ptr.atoll();
				}
				else if( ptr.removePrefix("-X-DISCONTINUITY"))
				{
					if( demuxOp != eStreamOp_DEMUX_ALL || !ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) )
					{ // ignore discontinuities when presenting muxed hls/ts
						discontinuitySequenceIndex++;
						discontinuity = true;
						if(ISCONFIGSET(eAAMPConfig_StreamLogging))
						{
							AAMPLOG_MIL("%s [%zu] Discontinuity Posn : %f, discontinuitySequenceIndex=%" PRIu64, name, index.size(), totalDuration.inSeconds(), discontinuitySequenceIndex );
						}
					}
				}
				else if (ptr.removePrefix("-X-PROGRAM-DATE-TIME:"))
				{
					programDateTimeIdxOfFragment = ptr.getPtr();
					if(ISCONFIGSET(eAAMPConfig_StreamLogging))
					{
						AAMPLOG_INFO("%s EXT-X-PROGRAM-DATE-TIME: %.*s ",name, 30, programDateTimeIdxOfFragment);
					}
					if(!pdtAtTopAvailable)
					{
						// Fix : mProgramDateTime to be updated only for playlist at top of playlist , not with each segment of Discontinuity
						if(index.size())
						{
							// Found PDT in between , not at the top . Need to extrapolate and find the ProgramDateTime of first segment
							AampTime tmppdt{ISO8601DateTimeToUTCSeconds(ptr.getPtr())};
							mProgramDateTime = tmppdt.inSeconds() - totalDuration.inSeconds();
							AAMPLOG_WARN("%s mProgramDateTime From Extrapolation : %f ",name, mProgramDateTime.inSeconds());
						}
						else
						{
							mProgramDateTime = ISO8601DateTimeToUTCSeconds(ptr.getPtr());
							AAMPLOG_INFO("%s mProgramDateTime From Start : %f ",name, mProgramDateTime.inSeconds());
						}
						pdtAtTopAvailable = true;
					}
					// The first X-PROGRAM-DATE-TIME tag holds the start time for each track
					if (startTimeForPlaylistSync == 0.0 )
					{
						/* discarding timezone assuming audio and video tracks has same timezone and we use this time only for synchronization*/
						startTimeForPlaylistSync = mProgramDateTime;
						AAMPLOG_WARN("%s StartTimeForPlaylistSync : %f ",name, startTimeForPlaylistSync.inSeconds());
					}
				}
				else if (ptr.removePrefix("-X-KEY:"))
				{
					std::string key = ptr.tostring();
					AAMPLOG_TRACE("aamp: EXT-X-KEY");

					// Need to store the Key tag to a list . Needs listed below
					// a) When a new Meta is added , its hash need to be compared
					//with available keytags to determine if its a deferred KeyAcquisition or not(VSS)
					// b) If there is a stream with varying IV in keytag with single Meta,
					// check if during trickplay drmInfo is considered.
					KeyTagStruct keyinfo;
					keyinfo.mKeyStartDuration = totalDuration.inSeconds();
					keyinfo.mKeyTagStr = key;
					if (!mDrm) // we don't want to keep calling this on updating the playlist as it may update the key info incorrectly
							   // if there are are multiple keys in the manifest
					{
						ptr.ParseAttrList(ParseKeyAttributeCallback, this);
					}
					//Each fragment should store the corresponding keytag indx to decrypt, MetaIdx may work with
					// adobe mapping , then if or any attribute of Key tag is different ?
					// At present , second Key parsing is done inside GetNextFragmentUriFromPlaylist(that saved)
					//Need keytag idx to pick the corresponding keytag and get drmInfo,so that second parsing can be removed
					//drmMetadataIdx = mDrmMetaDataIndexPosition;
					if(mDrmMethod == eDRM_KEY_METHOD_SAMPLE_AES_CTR){
						if (ISCONFIGSET(eAAMPConfig_Fragmp4PrefetchLicense)){
							{
								std::lock_guard<std::mutex> guard(aamp->drmParserMutex);
								attrNameData* aesCtrAttrData = new attrNameData(keyinfo.mKeyTagStr);
								if (std::find(aamp->aesCtrAttrDataList.begin(), aamp->aesCtrAttrDataList.end(),
											  *aesCtrAttrData) == aamp->aesCtrAttrDataList.end()) {
									// attrName not in aesCtrAttrDataList, add it
									aamp->aesCtrAttrDataList.push_back(*aesCtrAttrData);
								}
								/** No more use **/
								SAFE_DELETE(aesCtrAttrData);
							}
							/** Mark as CDM encryption is found in HLS **/
							aamp->fragmentCdmEncrypted = true;
							context->InitiateDrmProcess();
						}
					}

					drmMetadataIdx = mDrmKeyTagCount;
					if(!fragmentEncrypted || mDrmMethod == eDRM_KEY_METHOD_SAMPLE_AES_CTR)
					{
						drmMetadataIdx = -1;
						AAMPLOG_TRACE("Not encrypted - fragmentEncrypted %d mCMSha1Hash %s mDrmMethod %d", fragmentEncrypted, mCMSha1Hash.c_str(), mDrmMethod);
					}

					// mCMSha1Hash is populated after ParseAttrList , hence added here
					if( !mCMSha1Hash.empty() )
					{
						keyinfo.mShaID = mCMSha1Hash;
					}
					mKeyHashTable.push_back(keyinfo);
					mKeyTagChanged = false;
					mDrmKeyTagCount++;
				}
				else if(ptr.removePrefix("-X-MAP:"))
				{
					initFragmentPtr = ptr;
					if (mCheckForInitialFragEnc)
					{
						// Map tag present indicates ISOBMFF fragments. We need to store an encrypted fragment's init header
						// Ensure order of tags 1. EXT-X-KEY, 2. EXT-X-MAP
						if (fragmentEncrypted && mDrmMethod == eDRM_KEY_METHOD_SAMPLE_AES_CTR && mFirstEncInitFragmentInfo == NULL)
						{
							//AAMPLOG_TRACE("mFirstEncInitFragmentInfo - %s", ptr);
							mFirstEncInitFragmentInfo = ptr.getPtr();
						}
					}
				}
				else if (ptr.removePrefix("-X-START:"))
				{
					// X-Start can have two attributes . Time-Offset & Precise .
					// check if App has not configured any liveoffset
					{
						AampTime offsetval{ParseXStartTimeOffset(ptr)};
						if(!aamp->IsLiveAdjustRequired())
						{
							SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_CDVRLiveOffset,offsetval.inSeconds());
							aamp->UpdateLiveOffset();
						}
						else
						{
							/** 4K stream and 4K offset configured is below stream settings ; override liveOffset */
							if (aamp->mIsStream4K && (GETCONFIGOWNER(eAAMPConfig_LiveOffset4K) <= AAMP_STREAM_SETTING))
							{
								aamp->mLiveOffset = offsetval.inSeconds();
							}
							else
							{
								SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_LiveOffset,offsetval.inSeconds());
								aamp->UpdateLiveOffset();
							}
						}

						SetXStartTimeOffset(aamp->mLiveOffset);
					}
				}
				else if (ptr.removePrefix("-X-ENDLIST"))
				{
					// ENDLIST found .Check playlist tag with vod was missing or not.If playlist still undefined
					// mark it as VOD
					if (IsLive())
					{
						//required to avoid live adjust kicking in
						AAMPLOG_WARN("aamp: Changing playlist type from[%d] to ePLAYLISTTYPE_VOD as ENDLIST tag present.",mPlaylistType);
						mPlaylistType = ePLAYLISTTYPE_VOD;
					}
				}
			}
			//ptr = iter.mystrpbrk();
		}

		if(mediaSequence==false)
		{
			AAMPLOG_INFO("warning: no EXT-X-MEDIA-SEQUENCE tag");
			indexFirstMediaSequenceNumber = 0;
		}
		// When setting live status to stream, check the playlist type of both video/audio(demuxed)
		aamp->SetIsLive(context->IsLive());
		if(!IsLive())
		{
			aamp->getAampCacheHandler()->InsertToPlaylistCache(mPlaylistUrl, &playlist, mEffectiveUrl,IsLive(),TrackTypeToMediaType(type));
		}
		if(eTRACK_VIDEO == type)
		{
			aamp->UpdateDuration(totalDuration.inSeconds());
		}

	}
	// No condition checks for call . All checks n balances inside the module
	// which is been called.
	// Store the all the Metadata received from playlist indexing .
	// IF already stored , AveDrmManager will ignore it
	// ProcessDrmMetadata -> to be called only from one place , after playlist indexing. Not to call from other places
	if(mDrmMethod != eDRM_KEY_METHOD_SAMPLE_AES_CTR
		&& !PlayerHlsDrmSessionInterface::getInstance()->isDrmSupported(mDrmInfo)
	  )
	{
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_LA_TOTAL);
		//ProcessDrmMetadata();
		// Initiating key request for Meta present.If already key received ,call will be ignored.
		//InitiateDRMKeyAcquisition();
		// default MetaIndex is 0 , for single Meta . If Multi Meta is there ,then Hash is the criteria
		// for selection
		mDrmMetaDataIndexPosition = 0;
	}
	firstIndexDone = true;
	mIndexingInProgress = false;
	AAMPLOG_TRACE("Exit indexCount %zu mDrmMetaDataIndexCount %zu", index.size(), mDrmMetaDataIndex.size() );
	mDuration = totalDuration.inSeconds();

	if(IsRefresh)
	{
		if(!UseProgramDateTimeIfAvailable())
		{
			AampTime newSecondsBeforePlayPoint{GetCompletionTimeForFragment(this, commonPlayPosition)};
			culledSec = prevSecondsBeforePlayPoint - newSecondsBeforePlayPoint;

			if (culledSec > 0.0)
			{
				// Only positive values
				mCulledSeconds += culledSec;
			}
			else
			{
				culledSec = 0.0;
			}

			AAMPLOG_INFO("(%s) Prev:%f Now:%f culled with sequence:(%f -> %f) TrackCulled:%f",
				 name, prevSecondsBeforePlayPoint.inSeconds(), newSecondsBeforePlayPoint.inSeconds(), aamp->culledSeconds,(aamp->culledSeconds+culledSec.inSeconds()), mCulledSeconds.inSeconds());
		}
		else if (prevProgramDateTime > 0.0)
		{
			if(mProgramDateTime > 0.0)
			{
				culledSec = mProgramDateTime - prevProgramDateTime;
				// Both negative and positive values added
				mCulledSeconds += culledSec;
				AAMPLOG_INFO("(%s) Prev:%f Now:%f culled with ProgramDateTime:(%f -> %f) TrackCulled:%f",
					 name, prevProgramDateTime.inSeconds(), mProgramDateTime.inSeconds(), aamp->culledSeconds, (aamp->culledSeconds+culledSec.inSeconds()),mCulledSeconds.inSeconds());
			}
			else
			{
				AAMPLOG_INFO("(%s) Failed to read ProgramDateTime:(%f) Retained last PDT (%f) TrackCulled:%f",
					 name, mProgramDateTime.inSeconds(), prevProgramDateTime.inSeconds(), mCulledSeconds.inSeconds());
				mProgramDateTime = prevProgramDateTime;
			}
		}
	}
}

/**
 *  @brief Function to handle Profile change after ABR
 */
void TrackState::ABRProfileChanged()
{
	// If not live, reset play position since sequence number doesn't ensure the fragments
	// from multiple streams are in sync
	std::string pcontext = context->GetPlaylistURI(type);
	if( !pcontext.empty() )// != NULL)
	{
		AAMPLOG_TRACE("playlistPosition %f", playlistPosition.inSeconds());
		aamp_ResolveURL(mPlaylistUrl, aamp->GetManifestUrl(), pcontext.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
		std::lock_guard<std::mutex> guard(mutex);
		//playlistPosition reset will be done by RefreshPlaylist once playlist downloaded successfully
		//refreshPlaylist is used to reset the profile index if playlist download fails! Be careful with it.
		//Video profile change will definitely require new init headers
		mInjectInitFragment = true;
		refreshPlaylist = true;
		/*For some VOD assets, different video profiles have different DRM meta-data.*/
		mForceProcessDrmMetadata = true;
	}
	else
	{
		AAMPLOG_WARN(" GetPlaylistURI  is null");  //CID:83060 - Null Returns
	}

}

/**
* @brief Function to get minimum playlist update duration in ms
*/
long TrackState::GetMinUpdateDuration()
{
	return targetDurationSeconds.milliseconds();
}

/**
 * @brief Function to Returns default playlist update duration in ms
*/
int TrackState::GetDefaultDurationBetweenPlaylistUpdates()
{
	return context->maxIntervalBtwPlaylistUpdateMs;
}

/**
* @brief Function to Parse/Index playlist after being downloaded.
*/
void TrackState::ProcessPlaylist(AampGrowableBuffer& newPlaylist, int http_error)
{
	AAMPLOG_TRACE("[%s] Enter", name);
	if (newPlaylist.GetLen() )
	{ // download successful
		//lastPlaylistDownloadTimeMS = aamp_GetCurrentTimeMS();
		if (context->mNetworkDownDetected)
		{
			context->mNetworkDownDetected = false;
		}

		AcquirePlaylistLock();
		// Free previous playlist buffer and load with new one
		playlist.Free();
		playlist.Replace( &newPlaylist );
		AampTime culled{};
		IndexPlaylist(true, culled);
        
		// Update culled seconds if playlist download was successful
		// We need culledSeconds to find the timedMetadata position in playlist
		// culledSeconds and FindTimedMetadata have been moved up here, because FindMediaForSequenceNumber
		// uses mystrpbrk internally which modifies line terminators in playlist.ptr and results in
		// FindTimedMetadata failing to parse playlist
		if (IsLive())
		{
			if(eTRACK_VIDEO == type)
			{
				AAMPLOG_INFO("Updating PDT (%f) and culled (%f)", mProgramDateTime.inSeconds(), culled.inSeconds());
				aamp->mProgramDateTime = mProgramDateTime.inSeconds();
				aamp->UpdateCullingState(culled.inSeconds()); // report amount of content that was implicitly culled since last playlist download
			}
			// Metadata refresh is needed for live content only , not for VOD
			// Across ABR , for VOD no metadata change is expected from initial reported ones
			FindTimedMetadata();
		}
		if( mDuration > 0.0f )
		{
			// If we are loading new audio playlist for seamless switching, we should not seek based on sequence number
			if (IsLive() && !seamlessAudioSwitchInProgress)
			{
				fragmentURI = FindMediaForSequenceNumber();
			}
			else
			{
				lstring iter( playlist.GetPtr(),playlist.GetLen() );
				fragmentURI = iter.mystrpbrk();
				playlistPosition = -1;
			}
			manifestDLFailCount = 0;
		}
		lastPlaylistIndexedTimeMS = aamp_GetCurrentTimeMS();
		mPlaylistIndexed.notify_one();
		ReleasePlaylistLock();
	}
	else
	{
		// Clear data if any
		if (newPlaylist.GetPtr() )
		{
			newPlaylist.Free();
		}

		if (aamp->DownloadsAreEnabled())
		{
			if (CURLE_OPERATION_TIMEDOUT == http_error || CURLE_COULDNT_CONNECT == http_error)
			{
				context->mNetworkDownDetected = true;
				AAMPLOG_WARN(" Ignore curl timeout");
				return;
			}
			manifestDLFailCount++;
			//Send Error Event when new audio playlist fails after audio track change on seamlessAudioSwitch
			if((fragmentURI.empty() && (manifestDLFailCount > MAX_MANIFEST_DOWNLOAD_RETRY)) || (seamlessAudioSwitchInProgress))//No more fragments to download
			{
				aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
				return;
			}
		}
	}
}

/**
 * @brief Acquire playlist lock.
 */
void TrackState::AcquirePlaylistLock()
{ // TBR - use (scoped) lock directly
	// We need to implement HLS Downloader module which would deprecate this mutex altogether
	mPlaylistMutex.lock();
}

/**
 * @brief Release playlist lock.
 */
void TrackState::ReleasePlaylistLock()
{ // TBR - use (scoped) lock directly
	// We need to implement HLS Downloader module which would deprecate this mutex altogether
	mPlaylistMutex.unlock();
}

/**
 * @brief Function to filter the audio codec based on the configuration
 */
bool StreamAbstractionAAMP_HLS::FilterAudioCodecBasedOnConfig(StreamOutputFormat audioFormat)
{
	bool ignoreProfile = false;
	bool bDisableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
	bool bDisableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3);
	// if EC3 disabled, implicitly disable ATMOS
	bool bDisableATMOS = (bDisableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);

	switch (audioFormat)
	{
		case FORMAT_AUDIO_ES_AC3:
			if (bDisableAC3)
			{
				ignoreProfile = true;
			}
			break;

		case FORMAT_AUDIO_ES_ATMOS:
			if (bDisableATMOS)
			{
				ignoreProfile = true;
			}
			break;

		case FORMAT_AUDIO_ES_EC3:
			if (bDisableEC3)
			{
				ignoreProfile = true;
			}
			break;

		default:
			break;
	}

	return ignoreProfile;
}

/**
 * @brief Function to get best audio track based on the profile availability and language setting.
 */
int StreamAbstractionAAMP_HLS::GetBestAudioTrackByLanguage( void )
{
	int bestTrack{-1};
	int bestScore{-1};
	int i{};

	for (auto& mediaInfo : mediaInfoStore)
	{
		if(mediaInfo.type == eMEDIATYPE_AUDIO)
		{
			int score = 0;
			if (!FilterAudioCodecBasedOnConfig(mediaInfo.audioFormat))
			{ // allowed codec
				std::string trackLanguage = GetLanguageCode(i);
				if( aamp->preferredLanguagesList.size() > 0 )
				{
					auto iter = std::find(aamp->preferredLanguagesList.begin(), aamp->preferredLanguagesList.end(), trackLanguage);
					if(iter != aamp->preferredLanguagesList.end())
					{ // track is in preferred language list
						int distance = (int)std::distance(aamp->preferredLanguagesList.begin(),iter);
						score += (aamp->preferredLanguagesList.size()-distance)*100000; // big bonus for language match
					}
				}
				if( !aamp->preferredRenditionString.empty() &&
						aamp->preferredRenditionString.compare(mediaInfo.group_id)==0 )
				{
					score += 1000; // medium bonus for rendition match
				}
				if( aamp->preferredCodecList.size() > 0 )
				{
					auto iter = std::find(aamp->preferredCodecList.begin(), aamp->preferredCodecList.end(), GetAudioFormatStringForCodec(mediaInfo.audioFormat) );
					if(iter != aamp->preferredCodecList.end())
					{ // track is in preferred codec list
						int distance = (int)std::distance(aamp->preferredCodecList.begin(),iter);
						score += (aamp->preferredCodecList.size()-distance)*100; //  bonus for codec match
					}
				}
				else if(mediaInfo.audioFormat != FORMAT_UNKNOWN)
				{
					score += mediaInfo.audioFormat; // small bonus for better codecs like ATMOS
				}
				if( mediaInfo.isDefault || mediaInfo.autoselect )
				{ // bonus for designated "default"
					score += 10;
				}
				if( !aamp->preferredNameString.empty() &&
					aamp->preferredNameString.compare(mediaInfo.name)==0 )
				{
						score += 100; // small bonus for name match
				}
			}

			AAMPLOG_INFO( "track#%d score = %d", i, score );
			if( score > bestScore )
			{
				bestScore = score;
				bestTrack = i;
			}
		} // next track
		i++;
	}
	return bestTrack;
}

/**
 *  @brief Function to get playlist URI based on media selection
 */
std::string StreamAbstractionAAMP_HLS::GetPlaylistURI(TrackType trackType, StreamOutputFormat* format)
{
	std::string playlistURI;

	switch (trackType)
	{
	case eTRACK_VIDEO:
		{
			HlsStreamInfo *streamInfo  = (HlsStreamInfo *)GetStreamInfo(currentProfileIndex);
			if( streamInfo )
			{
				playlistURI = streamInfo->uri;
			}
			if (format)
			{
				*format = FORMAT_MPEGTS;
			}
		}
		break;
	case eTRACK_AUDIO:
		{
			if (currentAudioProfileIndex >= 0)
			{
				//aamp->UpdateAudioLanguageSelection( GetLanguageCode(currentAudioProfileIndex).c_str() );
				AAMPLOG_WARN("GetPlaylistURI : AudioTrack: language selected is %s", GetLanguageCode(currentAudioProfileIndex).c_str());
				playlistURI = mediaInfoStore[currentAudioProfileIndex].uri;
				mAudioTrackIndex = std::to_string(currentAudioProfileIndex);
				if (format)
				{
					*format = GetStreamOutputFormatForTrack(trackType);
				}
			}
		}
		break;
	case eTRACK_SUBTITLE:
		{
			if (currentTextTrackProfileIndex != -1)
			{
				playlistURI = mediaInfoStore[currentTextTrackProfileIndex].uri;
				mTextTrackIndex = std::to_string(currentTextTrackProfileIndex);
				SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_SubTitleLanguage,(std::string)mediaInfoStore[currentTextTrackProfileIndex].language);
				if (format) *format = (mediaInfoStore[currentTextTrackProfileIndex].type == eMEDIATYPE_SUBTITLE) ? FORMAT_SUBTITLE_WEBVTT : FORMAT_UNKNOWN;
//				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: subtitle found language %s, uri %s", mediaInfoStore[currentTextTrackProfileIndex].language, playlistURI);
			}
			else
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Couldn't find subtitle URI for preferred language: %s", aamp->mSubLanguage.c_str());
				if (format != NULL)
				{
					*format = FORMAT_INVALID;
				}
			}
		}
		break;
	case eTRACK_AUX_AUDIO:
		{
			int index = -1;
			// Plain comparison to get the audio track with matching language
			index = GetMediaIndexForLanguage(aamp->GetAuxiliaryAudioLanguage(), trackType);
			if (index != -1)
			{
				playlistURI = mediaInfoStore[index].uri;
				AAMPLOG_WARN("GetPlaylistURI : Auxiliary Track: Audio selected name is %s", GetLanguageCode(index).c_str());
				//No need to update back, matching track is either there or not
				if (format)
				{
					*format = GetStreamOutputFormatForTrack(trackType);
				}
			}
		}
		break;
	}
	return playlistURI;
}

/***************************************************************************
* @fn GetFormatFromFragmentExtension
* @brief Function to get media format based on fragment extension
*
* @param playlist[in] playlist to scan to infer stream format
* @return StreamOutputFormat stream format
***************************************************************************/
StreamOutputFormat GetFormatFromFragmentExtension( const AampGrowableBuffer &playlist )
{
    StreamOutputFormat format = FORMAT_INVALID;
	lstring iter(playlist.GetPtr(),playlist.GetLen());
	while( !iter.empty() )
	{
		lstring ptr = iter.mystrpbrk();
		if( ptr.SubStringMatch("#EXT-X-MAP") )
		{
            format = FORMAT_ISO_BMFF;
		}
		else if( ptr.startswith('#') )
        {
            continue;
        }
        else
        {
            auto len = ptr.find('?'); // strip any URI paratmeters
            if( len>0 )
            { // skip empty lines
                ptr = lstring( ptr.getPtr(), len );
                size_t delim = ptr.find('.');
                if( delim < ptr.length() )
                {
                    for(;;)
                    {
                        ptr = ptr.substr((int)delim+1);
                        delim = ptr.find('.');
                        if( delim == ptr.length() )
                        {
                            break;
                        }
                    }
                    if( ptr.equal("ts") )
                    {
                        format = FORMAT_MPEGTS;
                    }
                    else if ( ptr.equal("aac") )
                    {
                        format = FORMAT_AUDIO_ES_AAC;
                    }
                    else if ( ptr.equal("ac3") )
                    {
                        format = FORMAT_AUDIO_ES_AC3;
                    }
                    else if ( ptr.equal("ec3") )
                    {
                        format = FORMAT_AUDIO_ES_EC3;
                    }
                    else if( ptr.equal("vtt") || ptr.equal("webvtt") )
                    {
                        format = FORMAT_SUBTITLE_WEBVTT;
                    }
                    else
                    {
                        AAMPLOG_WARN("Not TS or MP4 extension, probably ES. fragment extension %.*s", ptr.getLen(), ptr.getPtr() );
                    }
                }
                break;
            }
        }
	}
    AAMPLOG_MIL( "format=%d", format );
	return format;
}


/**
 * @brief Function to check if both tracks in demuxed HLS are in live mode
 *        Function to check for live status comparing both playlist(audio&video)
 *        Kept public as its called from outside StreamAbstraction class
 */
bool StreamAbstractionAAMP_HLS::IsLive()
{
	// Check for both the tracks if its in Live state
	// In Demuxed content , Hot CDVR playlist update for audio n video happens at a small time delta
	// To avoid missing contents ,until both tracks are not moved to VOD , stream has to be in Live mode
	bool retValIsLive = false;
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			retValIsLive |= track->IsLive();
		}
	}
	return retValIsLive;
}


/**
 * @brief Function to update play target based on audio video exact discontinuity positions.
 */
void StreamAbstractionAAMP_HLS::CheckDiscontinuityAroundPlaytarget(void)
{ // FIXME!
	TrackState *audio = trackState[eMEDIATYPE_AUDIO];
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	for( DiscontinuityIndexNode &videoDiscontinuity : video->mDiscontinuityIndex )
	{
		if (videoDiscontinuity.position.seconds() == video->playTarget.seconds())
		{
			for( DiscontinuityIndexNode &audioDiscontinuity : audio->mDiscontinuityIndex )
			{
				if (videoDiscontinuity.position < audioDiscontinuity.position)
				{
					AAMPLOG_WARN("video->playTarget %f -> %f audio->playTarget %f -> %f",
								 video->playTarget.inSeconds(), videoDiscontinuity.position.inSeconds(), audio->playTarget.inSeconds(), audioDiscontinuity.position.inSeconds());
					video->playTarget = videoDiscontinuity.position;
					audio->playTarget = audioDiscontinuity.position;
				}
				else
				{
					AAMPLOG_WARN("video->playTarget %f -> %" PRIi64 "audio->playTarget %f -> %" PRIi64,
								 video->playTarget.inSeconds(), audioDiscontinuity.position.seconds(), audio->playTarget.inSeconds(), audioDiscontinuity.position.seconds());
					video->playTarget = audio->playTarget = audioDiscontinuity.position.seconds();
				}
				return;
			}
		}
	}
}

/**
 * @brief Function to synchronize time between audio & video for VOD stream with discontinuities and uneven track length
 */
AAMPStatusType StreamAbstractionAAMP_HLS::SyncTracksForDiscontinuity()
{ // FIXME!
	TrackState *audio = trackState[eMEDIATYPE_AUDIO];
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	TrackState *aux = NULL;
	if (!audio->enabled)
	{
		AAMPLOG_WARN("Attempting to sync between muxed track and auxiliary audio track");
		audio = trackState[eMEDIATYPE_AUX_AUDIO];
	}
	else
	{
		aux = trackState[eMEDIATYPE_AUX_AUDIO];
	}
	AAMPStatusType retVal = eAAMPSTATUS_OK;

	AampTime roundedPlayTarget{(double)video->playTarget.nearestSecond()};
	// Offset value to add . By default it will playtarget
	AampTime offsetVideoToAdd{roundedPlayTarget};
	AampTime offsetAudioToAdd{roundedPlayTarget};
	// Start of audio and Period for current Period and previous period
	AampTime audioPeriodStartCurrentPeriod{};
	AampTime videoPeriodStartCurrentPeriod{};
	AampTime audioPeriodStartPrevPeriod{};
	AampTime videoPeriodStartPrevPeriod{};

	if (audio->GetNumberOfPeriods() != video->GetNumberOfPeriods())
	{
		AAMPLOG_WARN("WARNING audio's number of period %d video number of period: %d", audio->GetNumberOfPeriods(), video->GetNumberOfPeriods());
		return eAAMPSTATUS_INVALID_PLAYLIST_ERROR;
	}

	if (video->playTarget !=0)
	{
		/*If video playTarget is just before a discontinuity, move playTarget to the discontinuity position*/
		for( auto i=0; i<video->mDiscontinuityIndex.size(); i++ )
		{
			DiscontinuityIndexNode &videoDiscontinuity = video->mDiscontinuityIndex[i];
			AampTime roundedIndexPosition{(double)videoDiscontinuity.position.nearestSecond()};
			AampTime roundedFragDuration{(double)videoDiscontinuity.fragmentDuration.nearestSecond()};
			// check if playtarget is on discontinuity , around it or away from discontinuity
			AampTime diff{roundedIndexPosition - roundedPlayTarget};

			videoPeriodStartCurrentPeriod = videoDiscontinuity.position;

			DiscontinuityIndexNode &audioDiscontinuity = audio->mDiscontinuityIndex[i];
			audioPeriodStartCurrentPeriod = audioDiscontinuity.position;

			// if play position is same as start of discontinuity , just start there , no checks
			// if play position is within fragmentduration window , just start at discontinuity
			if(fabs(diff) <= roundedFragDuration || diff == 0)
			{
				// in this case , no offset to add . On discontinuity index position
				offsetVideoToAdd = offsetAudioToAdd = 0;
				AAMPLOG_WARN ("PlayTarget around the discontinuity window,rounding position to discontinuity index");//,videoPeriodStartCurrentPeriod,audioPeriodStartCurrentPeriod);
				break;
			}
			else if(diff < 0 )
			{
				// this case : playtarget is after the discontinuity , but not sure if this is within
				// current period .
				offsetVideoToAdd = (roundedPlayTarget - roundedIndexPosition);
				offsetAudioToAdd = (roundedPlayTarget - audioDiscontinuity.position.nearestSecond());
				// Not sure if this is last period or not ,so update the Offset
			}
			else if(diff > 0 )
			{
				// this case : discontinuity Index is after the playtarget
				// need to break the loop. Before that get offset with ref to prev period
				audioPeriodStartCurrentPeriod  = audioPeriodStartPrevPeriod;
				videoPeriodStartCurrentPeriod  = videoPeriodStartPrevPeriod;
				// Get offset from last period start
				offsetVideoToAdd = (roundedPlayTarget - videoPeriodStartPrevPeriod.nearestSecond());
				offsetAudioToAdd = (roundedPlayTarget - audioPeriodStartPrevPeriod.nearestSecond());
				break;
			}
			// store the current period as prev period before moving to next
			videoPeriodStartPrevPeriod = videoPeriodStartCurrentPeriod;
			audioPeriodStartPrevPeriod = audioPeriodStartCurrentPeriod;
		}

		// Calculate Audio and Video playtarget
		audio->playTarget = audioPeriodStartCurrentPeriod + offsetAudioToAdd;
		video->playTarget = videoPeriodStartCurrentPeriod + offsetVideoToAdd;
		// Based on above playtarget , find the exact segment to pick to reduce audio loss
		{
			int periodIdx;
			AampTime offsetFromPeriod{};
			AampTime audioOffsetFromPeriod{};
			int fragmentIdx;
			video->GetNextFragmentPeriodInfo(periodIdx, offsetFromPeriod, fragmentIdx);

			if(-1 != periodIdx)
			{
				AampTime audioPeriodStart{audio->GetPeriodStartPosition(periodIdx)};
				AampTime videoPeriodStart{video->GetPeriodStartPosition(periodIdx)};
				int audioFragmentIdx;
				audio->GetNextFragmentPeriodInfo(periodIdx, audioOffsetFromPeriod, audioFragmentIdx);

				AAMPLOG_WARN("video periodIdx: %d, video-offsetFromPeriod: %f, videoPeriodStart: %f, audio-offsetFromPeriod: %f, audioPeriodStart: %f",
							 periodIdx, offsetFromPeriod.inSeconds(), videoPeriodStart.inSeconds(), audioOffsetFromPeriod.inSeconds(), audioPeriodStart.inSeconds());

				if (0 != audioPeriodStart)
				{
					if ((fragmentIdx != -1) && (audioFragmentIdx != -1) && (fragmentIdx != audioFragmentIdx) && (audioPeriodStart.seconds() == videoPeriodStart.seconds()))
					{
						if (audioPeriodStart > videoPeriodStart)
						{
							audio->playTarget = audioPeriodStart + audioOffsetFromPeriod;
							video->playTarget = videoPeriodStart + audioOffsetFromPeriod;
							AAMPLOG_WARN("(audio > video) - vid start: %f, audio start: %f", video->playTarget.inSeconds(), audio->playTarget.inSeconds() );
						}
						else
						{
							audio->playTarget = audioPeriodStart + offsetFromPeriod;
							video->playTarget = videoPeriodStart + offsetFromPeriod;
							AAMPLOG_WARN("(video > audio) - vid start: %f, audio start: %f",  video->playTarget.inSeconds(), audio->playTarget.inSeconds() );
						}
					}
					else
					{
						audio->playTarget = audioPeriodStart + audioOffsetFromPeriod;
						video->playTarget = videoPeriodStart + offsetFromPeriod;
						AAMPLOG_WARN("(audio != video) - vid start: %f, audio start: %f", video->playTarget.inSeconds(), audio->playTarget.inSeconds() );
					}

					SeekPosUpdate(video->playTarget.inSeconds());

					AAMPLOG_WARN("VP: %f, AP: %f, seek_pos_seconds changed to %f based on video playTarget", video->playTarget.inSeconds(), audio->playTarget.inSeconds(), seekPosition.inSeconds());

					retVal = eAAMPSTATUS_OK;
				}
				else
				{
					AAMPLOG_WARN("audioDiscontinuityOffset: 0");
				}
			}
			else
			{
				AAMPLOG_WARN("WARNING audio's number of period %d subtitle number of period %d", audio->GetNumberOfPeriods(), subtitle->GetNumberOfPeriods());
			}
		}

		//lets go with a simple sync operation for the moment for subtitle and aux
		for (int index = eMEDIATYPE_SUBTITLE; index <= eMEDIATYPE_AUX_AUDIO; index++)
		{
			TrackState *track = trackState[index];
			if (index == eMEDIATYPE_AUX_AUDIO && !trackState[eMEDIATYPE_AUDIO]->enabled)
			{
				// Case of muxed track and separate aux track - its already sync'ed
				break;
			}
			if (track->enabled)
			{
				if (audio->GetNumberOfPeriods() == track->GetNumberOfPeriods())
				{
					int periodIdx;
					AampTime offsetFromPeriod{};
					int audioFragmentIdx;
					audio->GetNextFragmentPeriodInfo(periodIdx, offsetFromPeriod, audioFragmentIdx);

					if (-1 != periodIdx)
					{
						AAMPLOG_WARN("audio periodIdx: %d, offsetFromPeriod: %f", periodIdx, offsetFromPeriod.inSeconds());
						AampTime trackPeriodStart{track->GetPeriodStartPosition(periodIdx)};
						if (0 != trackPeriodStart)
						{
							track->playTarget = trackPeriodStart + offsetFromPeriod;
						}
						else
						{
							AAMPLOG_WARN("subtitleDiscontinuityOffset: 0");
						}
					}
				}
				else
				{
					AAMPLOG_WARN("WARNING audio's number of period %d, %s number of period: %d",
							audio->GetNumberOfPeriods(), track->name, track->GetNumberOfPeriods());
				}
			}
		}

		if (!trackState[eMEDIATYPE_AUDIO]->enabled)
		{
			AAMPLOG_WARN("Exit : aux track start %f, muxed track start %f sub track start %f",
					audio->playTarget.inSeconds(), video->playTarget.inSeconds(), subtitle->playTarget.inSeconds());
		}
		else if (aux)
		{
			AAMPLOG_WARN("Exit : audio track start %f, vid track start %f sub track start %f aux track start %f",
					audio->playTarget.inSeconds(), video->playTarget.inSeconds(), subtitle->playTarget.inSeconds(), aux->playTarget.inSeconds());
		}
	}

	return retVal;
}

/**
 * @brief Function to synchronize time between A/V for Live/Event assets
 */
AAMPStatusType StreamAbstractionAAMP_HLS::SyncTracks(void)
{
	bool useProgramDateTimeIfAvailable = UseProgramDateTimeIfAvailable();
	AAMPStatusType retval = eAAMPSTATUS_OK;
	bool startTimeAvailable = true;
	bool syncedUsingSeqNum = false;
	long long mediaSequenceNumber[AAMP_TRACK_COUNT] = {0};
	TrackState *audio = trackState[eMEDIATYPE_AUDIO];
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	TrackState *aux = NULL;
	AampTime diffBetweenStartTimes{};

	for(int i = 0; i<AAMP_TRACK_COUNT; i++)
	{
		TrackState *ts = trackState[i];

		// CID:335490 - Data race condition
		ts->AcquirePlaylistLock();
		if (ts->enabled)
		{
			bool reloadUri = false;
			ts->fragmentURI = trackState[i]->GetNextFragmentUriFromPlaylist(reloadUri, true); //To parse track playlist
			/*Update playTarget to playlistPostion to correct the seek position to start of a fragment*/
			ts->playTarget = ts->playlistPosition;
			AAMPLOG_WARN("syncTracks loop : track[%d] pos %f start %f frag-duration %f trackState->fragmentURI %.*s ts->nextMediaSequenceNumber %lld", i, ts->playlistPosition.inSeconds(), ts->playTarget.inSeconds(), ts->fragmentDurationSeconds, ts->fragmentURI.getLen(), ts->fragmentURI.getPtr(), ts->nextMediaSequenceNumber);
			if (ts->startTimeForPlaylistSync == 0.0 )
			{
				AAMPLOG_WARN("startTime not available for track %d", i);
				startTimeAvailable = false;
			}
			mediaSequenceNumber[i] = ts->nextMediaSequenceNumber - 1;
		}
		ts->ReleasePlaylistLock();
	}

	if (audio->enabled)
	{
		aux = trackState[eMEDIATYPE_AUX_AUDIO];
	}
	else
	{
		mediaSequenceNumber[eMEDIATYPE_AUDIO] = mediaSequenceNumber[eMEDIATYPE_AUX_AUDIO];
		audio = trackState[eMEDIATYPE_AUX_AUDIO];
	}

	if (startTimeAvailable)
	{
		//Logging irregularities in the playlist for debugging purposes
		diffBetweenStartTimes = audio->startTimeForPlaylistSync - video->startTimeForPlaylistSync;
		AAMPLOG_WARN("Difference in PDT between A/V: %f Audio:%f Video:%f ", diffBetweenStartTimes.inSeconds(), audio->startTimeForPlaylistSync.inSeconds(),
								video->startTimeForPlaylistSync.inSeconds());
		if (!useProgramDateTimeIfAvailable)
		{
			if (video->targetDurationSeconds != audio->targetDurationSeconds)
			{
				AAMPLOG_WARN("WARNING seqno based track synchronization when video->targetDurationSeconds[%f] != audio->targetDurationSeconds[%f]",
						video->targetDurationSeconds.inSeconds(), audio->targetDurationSeconds.inSeconds());
			}
			else
			{
				AampTime diffBasedOnSeqNumber{(mediaSequenceNumber[eMEDIATYPE_AUDIO]
						- mediaSequenceNumber[eMEDIATYPE_VIDEO]) * video->fragmentDurationSeconds};
				if (fabs(diffBasedOnSeqNumber - diffBetweenStartTimes) > video->fragmentDurationSeconds)
				{
					AAMPLOG_WARN("WARNING - inconsistency between startTime and seqno  startTime diff %f diffBasedOnSeqNumber %f",
							diffBetweenStartTimes.inSeconds(), diffBasedOnSeqNumber.inSeconds());
				}
			}
		}

		if((diffBetweenStartTimes < -10.0 || diffBetweenStartTimes > 10.0))
		{
			AAMPLOG_WARN("syncTracks diff debug : Audio start time : %f  Video start time : %f ",
					audio->startTimeForPlaylistSync.inSeconds(), video->startTimeForPlaylistSync.inSeconds() );
		}
	}

	//Sync using sequence number since startTime is not available or not desired
	if (!startTimeAvailable || !useProgramDateTimeIfAvailable)
	{
		AampMediaType mediaType;
		TrackState *laggingTS = NULL;
		long long diff = 0;
		if (mediaSequenceNumber[eMEDIATYPE_AUDIO] > mediaSequenceNumber[eMEDIATYPE_VIDEO])
		{
			laggingTS = video;
			diff = mediaSequenceNumber[eMEDIATYPE_AUDIO] - mediaSequenceNumber[eMEDIATYPE_VIDEO];
			mediaType = eMEDIATYPE_VIDEO;
			AAMPLOG_WARN("video track lag in seqno. diff %lld", diff);
		}
		else if (mediaSequenceNumber[eMEDIATYPE_VIDEO] > mediaSequenceNumber[eMEDIATYPE_AUDIO])
		{
			laggingTS = audio;
			diff = mediaSequenceNumber[eMEDIATYPE_VIDEO] - mediaSequenceNumber[eMEDIATYPE_AUDIO];
			mediaType = eMEDIATYPE_AUDIO;
			AAMPLOG_WARN("audio track lag in seqno. diff %lld", diff);
		}
		if (laggingTS)
		{
			if (startTimeAvailable && (diff > MAX_SEQ_NUMBER_DIFF_FOR_SEQ_NUM_BASED_SYNC))
			{
				AAMPLOG_WARN("falling back to synchronization based on start time as diff = %lld", diff);
			}
			else if ((diff <= MAX_SEQ_NUMBER_LAG_COUNT) && (diff > 0))
			{
				AAMPLOG_WARN("sync using sequence number. diff [%lld] A [%lld] V [%lld] a-f-uri [%.*s] v-f-uri [%.*s]",
						diff, mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[eMEDIATYPE_VIDEO],
							 audio->fragmentURI.getLen(), audio->fragmentURI.getPtr(),
							 video->fragmentURI.getLen(), video->fragmentURI.getPtr() );
				while (diff > 0)
				{
					laggingTS->playTarget += laggingTS->fragmentDurationSeconds;
					laggingTS->playTargetOffset += laggingTS->fragmentDurationSeconds;
					if( !laggingTS->fragmentURI.empty() )
					{
						bool reloadUri = false;
						laggingTS->fragmentURI = laggingTS->GetNextFragmentUriFromPlaylist(reloadUri, true);
					}
					else
					{
						AAMPLOG_WARN("laggingTS->fragmentURI NULL, seek might be out of window");
					}
					diff--;
				}
				syncedUsingSeqNum = true;
			}
			else
			{
				AAMPLOG_WARN("Lag in '%s' seq no, diff[%lld] > maxValue[%d]", GetMediaTypeName(mediaType), diff, MAX_SEQ_NUMBER_LAG_COUNT);
			}
		}
		else
		{
			AAMPLOG_WARN("No lag in seq no b/w AV");
			syncedUsingSeqNum = true;
		}

		//lets go with a simple sync operation for the moment for subtitle and aux
		for (int index = eMEDIATYPE_SUBTITLE; (syncedUsingSeqNum && index <= eMEDIATYPE_AUX_AUDIO); index++)
		{
			TrackState *track = trackState[index];
			if (index == eMEDIATYPE_AUX_AUDIO && !trackState[eMEDIATYPE_AUDIO]->enabled)
			{
				// Case of muxed track and separate aux track and its already sync'ed
				break;
			}
			if (track->enabled)
			{
				long long diff = mediaSequenceNumber[eMEDIATYPE_AUDIO] - mediaSequenceNumber[index];
				//We can only support track to catch-up to audio. The opposite will cause a/v sync issues
				if (diff > 0 && diff <= MAX_SEQ_NUMBER_LAG_COUNT)
				{
					AAMPLOG_WARN("sync %s using sequence number. diff [%lld] A [%lld] T [%lld] a-f-uri [%.*s] t-f-uri [%.*s]",
							track->name, diff, mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[index],
								 audio->fragmentURI.getLen(), audio->fragmentURI.getPtr(),
								 track->fragmentURI.getLen(), track->fragmentURI.getPtr() );
					//Track catch up to audio
					while (diff > 0)
					{
						track->playTarget += track->fragmentDurationSeconds;
						track->playTargetOffset += track->fragmentDurationSeconds;
						if (!track->fragmentURI.empty() )
						{
							bool reloadUri = false;
							track->fragmentURI = track->GetNextFragmentUriFromPlaylist(reloadUri, false);
						}
						else
						{
							AAMPLOG_WARN("%s fragmentURI NULL, seek might be out of window", track->name);
						}
						diff--;
					}
				}
				else if (diff < 0)
				{
					//Audio can't catch up with track, since its already sync-ed with video.
					AAMPLOG_WARN("sync using sequence number failed, %s will be starting late. diff [%lld] A [%lld] T [%lld] a-f-uri [%.*s] t-f-uri [%.*s]",
							track->name, diff, mediaSequenceNumber[eMEDIATYPE_AUDIO], mediaSequenceNumber[index],
								 audio->fragmentURI.getLen(), audio->fragmentURI.getPtr(),
								 track->fragmentURI.getLen(), track->fragmentURI.getPtr() );
				}
				else
				{
					AAMPLOG_WARN("No lag in seq no b/w audio and %s", track->name);
				}
			}
		}
	}

	if (!syncedUsingSeqNum)
	{
		if (startTimeAvailable)
		{
			if (diffBetweenStartTimes > 0)
			{
				TrackState *ts = trackState[eMEDIATYPE_VIDEO];
				if (diffBetweenStartTimes > (ts->fragmentDurationSeconds / 2))
				{
					if (video->mDuration > (ts->playTarget + diffBetweenStartTimes))
					{
						ts->playTarget += diffBetweenStartTimes;
						ts->playTargetOffset = diffBetweenStartTimes;
						AAMPLOG_WARN("Audio track in front, catchup videotrack video playTarget:%f playTargetOffset:%f",ts->playTarget.inSeconds() ,ts->playTargetOffset.inSeconds());
					}
					else
					{
						AAMPLOG_WARN("invalid diff %f ts->playTarget %f trackDuration %f",
							diffBetweenStartTimes.inSeconds(), ts->playTarget.inSeconds(), video->mDuration.inSeconds());
						retval = eAAMPSTATUS_TRACKS_SYNCHRONIZATION_ERROR;
					}
				}
				else
				{
					AAMPLOG_WARN("syncTracks : Skip playTarget update diff %f, vid track start %f fragmentDurationSeconds %f",
							diffBetweenStartTimes.inSeconds(), ts->playTarget.inSeconds(), ts->fragmentDurationSeconds);
				}
			}
			else if (diffBetweenStartTimes < 0)
			{
				TrackState *ts = trackState[eMEDIATYPE_AUDIO];
				if (fabs(diffBetweenStartTimes) > (ts->fragmentDurationSeconds / 2))
				{
					if (audio->mDuration > (ts->playTarget - diffBetweenStartTimes))
					{
						ts->playTarget -= diffBetweenStartTimes;
						ts->playTargetOffset = -diffBetweenStartTimes;
						AAMPLOG_WARN("Video track in front, catchup audiotrack audio playTarget:%f playTargetOffset:%f",ts->playTarget.inSeconds() ,ts->playTargetOffset.inSeconds());
					}
					else
					{
						AAMPLOG_ERR("invalid diff %f ts->playTarget %f trackDuration %f",
								diffBetweenStartTimes.inSeconds(), ts->playTarget.inSeconds(), audio->mDuration.inSeconds());
						retval = eAAMPSTATUS_TRACKS_SYNCHRONIZATION_ERROR;
					}
				}
				else
				{
					AAMPLOG_WARN("syncTracks : Skip playTarget update diff %f, aud track start %f fragmentDurationSeconds %f",
							fabs(diffBetweenStartTimes), ts->playTarget.inSeconds(), ts->fragmentDurationSeconds);
				}
			}

			//lets go with a simple sync operation for the moment for subtitle and aux
			for (int index = eMEDIATYPE_SUBTITLE; (syncedUsingSeqNum && index <= eMEDIATYPE_AUX_AUDIO); index++)
			{
				TrackState *track =  trackState[index];
				if (index == eMEDIATYPE_AUX_AUDIO && !trackState[eMEDIATYPE_AUDIO]->enabled)
				{
					// Case of muxed track and separate aux track and its already sync'ed
					break;
				}
				if (track->enabled)
				{
					//Compare track and audio start time
					const AampTime diff{audio->startTimeForPlaylistSync - subtitle->startTimeForPlaylistSync};
					if (diff > 0)
					{
						//Audio is at a higher start time that track. Track needs to catch-up
						if (diff > (track->fragmentDurationSeconds / 2))
						{
							if (track->mDuration > (track->playTarget + diff))
							{
								track->playTarget += diff;
								track->playTargetOffset = diff.inSeconds();
								AAMPLOG_WARN("Audio track in front, catchup %s playTarget:%f playTargetOffset:%f",
										track->name,
										track->playTarget.inSeconds(), track->playTargetOffset.inSeconds());
							}
							else
							{
								AAMPLOG_WARN("invalid diff(%f) greater than duration, ts->playTarget %f trackDuration %f, %s may start early",
										diff.inSeconds(), track->playTarget.inSeconds(), track->mDuration.inSeconds(), track->name);
							}
						}
						else
						{
							AAMPLOG_WARN("syncTracks : Skip %s playTarget update diff %f, track start %f fragmentDurationSeconds %f",
									track->name, diff.inSeconds(), track->playTarget.inSeconds(), track->fragmentDurationSeconds);
						}
					}
					else if (diff < 0)
					{
						//Can't catch-up audio to subtitle, since audio and video are already sync-ed
						AAMPLOG_WARN("syncTracks : Skip %s sync to audio for subtitle startTime %f, audio startTime %f. Subtitle will be starting late",
								track->name, track->startTimeForPlaylistSync.inSeconds(), audio->startTimeForPlaylistSync.inSeconds());
					}
				}
			}
		}
		else
		{
			AAMPLOG_ERR("Could not sync using seq num and start time not available., cannot play this content.!!");
			retval = eAAMPSTATUS_TRACKS_SYNCHRONIZATION_ERROR;
		}
	}
	// New calculated playTarget assign back for buffer calculation
	video->playTargetBufferCalc = video->playTarget;
	if (!trackState[eMEDIATYPE_AUDIO]->enabled)
	{
		AAMPLOG_WARN("Exit : aux track start %f, muxed track start %f sub track start %f",
				audio->playTarget.inSeconds(), video->playTarget.inSeconds(), subtitle->playTarget.inSeconds());
	}
	else if (aux)
	{
		AAMPLOG_WARN("Exit : audio track start %f, vid track start %f sub track start %f aux track start %f",
				audio->playTarget.inSeconds(), video->playTarget.inSeconds(), subtitle->playTarget.inSeconds(), aux->playTarget.inSeconds());
	}

	return retval;
}


/**
 * @brief Function to get the language code
 */
std::string StreamAbstractionAAMP_HLS::GetLanguageCode(int iMedia)
{
	std::string lang = mediaInfoStore[iMedia].language;
	lang = Getiso639map_NormalizeLanguageCode(lang,aamp->GetLangCodePreference());
	return lang;
}

/**
 *  @brief Function to initialize member variables,download main manifest and parse
 */
AAMPStatusType StreamAbstractionAAMP_HLS::Init(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_GENERIC_ERROR;
	mTuneType = tuneType;
	bool newTune = aamp->IsNewTune();
	aamp->IsTuneTypeNew = newTune;

	int http_error = 0;   //CID:81873 - Initialization
	mainManifest.Clear();

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs, (AampCurlInstance)i);
	}

	if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(aamp->GetManifestUrl(), &mainManifest, aamp->GetManifestUrl(), eMEDIATYPE_MANIFEST))
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Main manifest retrieved from cache");
	}

	bool updateVideoEndMetrics = false;
	double mainManifestdownloadTime = 0;
	int parseTimeMs = 0;
	if (!this->mainManifest.GetLen() )
	{
		aamp->profiler.ProfileBegin(PROFILE_BUCKET_MANIFEST);
		AAMPLOG_TRACE("StreamAbstractionAAMP_HLS::downloading manifest");
		// take the original url before its gets changed in GetFile
		std::string mainManifestOrigUrl = aamp->GetManifestUrl();
		aamp->SetCurlTimeout(aamp->mManifestTimeoutMs, eCURLINSTANCE_MANIFEST_MAIN);
		(void) aamp->GetFile(aamp->GetManifestUrl(), eMEDIATYPE_MANIFEST, &this->mainManifest, aamp->GetManifestUrl(), &http_error, &mainManifestdownloadTime, NULL, eCURLINSTANCE_MANIFEST_MAIN, true,NULL,NULL,0);//CID:82578 - checked return
		// Set playlist curl timeouts.
		for (int i = eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO; i < (eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO + AAMP_TRACK_COUNT); i++)
		{
			aamp->SetCurlTimeout(aamp->mPlaylistTimeoutMs, (AampCurlInstance) i);
		}
		//update videoend info
		updateVideoEndMetrics = true;
		if (this->mainManifest.GetLen())
		{
			aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
			AAMPLOG_TRACE("StreamAbstractionAAMP_HLS::downloaded manifest");
			aamp->getAampCacheHandler()->InsertToPlaylistCache(mainManifestOrigUrl, &mainManifest, aamp->GetManifestUrl(),false,eMEDIATYPE_MANIFEST);
		}
		else
		{
			if (http_error == 512 && aamp->mFogDownloadFailReason.find("PROFILE_NONE") != std::string::npos)
			{
				aamp->mFogDownloadFailReason.clear();
				AAMPLOG_ERR("StreamAbstractionAAMP_HLS: No playable profiles found");
				retval = AAMPStatusType::eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
			}
			else{
			aamp->UpdateDuration(0);
			AAMPLOG_ERR("Manifest download failed : http response : %d", http_error);
			retval = eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
			}
		}
	}
	if (!this->mainManifest.GetLen() && aamp->DownloadsAreEnabled()) //!aamp->GetFile(aamp->GetManifestUrl(), &this->mainManifest, aamp->GetManifestUrl()))
	{
		aamp->profiler.ProfileError(PROFILE_BUCKET_MANIFEST, http_error);
		aamp->profiler.ProfileEnd(PROFILE_BUCKET_MANIFEST);
		if(retval == AAMPStatusType::eAAMPSTATUS_MANIFEST_CONTENT_ERROR)
		{
			aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FAILED_MANIFEST_CONTENT_ERROR, http_error);
		}
		else
		{
			aamp->SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED, http_error);
		}
	}
	if (this->mainManifest.GetLen() )
	{
		if( AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE) )
		{ // use printf to avoid 2048 char syslog limitation
			printf("***Main Manifest***:\n\n%.*s\n************\n", (int)this->mainManifest.GetLen(), this->mainManifest.GetPtr());
		}

		AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
		bool forceClearSession = (!ISCONFIGSET(eAAMPConfig_SetLicenseCaching) && (tuneType == eTUNETYPE_NEW_NORMAL));
		licenseManager->clearDrmSession(forceClearSession);
		licenseManager->clearFailedKeyIds();
		licenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
		licenseManager->setLicenseRequestAbort(false);
		// Parse the Main manifest ( As Parse function modifies the original data,InsertCache had to be called before it .
		long long tStartTime = NOW_STEADY_TS_MS;
		AAMPStatusType mainManifestResult = ParseMainManifest();
		parseTimeMs = (int)(NOW_STEADY_TS_MS - tStartTime);
		// Check if Main manifest is good or not
		if(mainManifestResult != eAAMPSTATUS_OK)
		{
			if(mainManifestResult == eAAMPSTATUS_PLAYLIST_PLAYBACK)
			{ // support tune to playlist, without main manifest
				if(mProfileCount == 0)
				{
					streamInfoStore.emplace_back(HlsStreamInfo{});
					HlsStreamInfo &streamInfo = streamInfoStore[mProfileCount];
					setupStreamInfo(streamInfo);
					streamInfo.uri = aamp->GetManifestUrl().c_str();
					SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_EnableABR,false);
					mainManifestResult = eAAMPSTATUS_OK;
					AAMPLOG_INFO("StreamAbstractionAAMP_HLS::Playlist only playback.");
					aamp->getAampCacheHandler()->RemoveFromPlaylistCache(aamp->GetManifestUrl());
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Invalid manifest format.");
					mainManifestResult = eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
				}
			}
			// check for the error type , if critical error return immediately
			if(mainManifestResult == eAAMPSTATUS_MANIFEST_CONTENT_ERROR || mainManifestResult == eAAMPSTATUS_MANIFEST_PARSE_ERROR)
			{ // use printf to avoid 2048 char syslog limitation
				// Dump the invalid manifest content before reporting error
				printf("ERROR: Invalid Main Manifest : %.*s\n", (int)this->mainManifest.GetLen(), this->mainManifest.GetPtr() );
				return mainManifestResult;
			}
		}

		if(mProfileCount)
		{
			if (!newTune)
			{
				long persistedBandwidth = aamp->GetPersistedBandwidth();
				long defaultBitRate 	= aamp->GetDefaultBitrate();
				//We were tuning to a lesser profile previously, so we use it as starting profile
				// If bitrate to be persisted during trickplay is true, set persisted BW as default init BW
				if (persistedBandwidth > 0 && (persistedBandwidth < defaultBitRate || aamp->IsBitRatePersistedOverSeek()))
				{
					aamp->mhAbrManager.setDefaultInitBitrate(persistedBandwidth);
				}
			}
			else
			{
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
					//set default bitrate
					else
					{
						AAMPLOG_WARN("Using defaultBitrate %ld . PersistBandwidth : %ld TimeGap : %ld",aamp->GetDefaultBitrate(),persistbandwidth,TimeGap);
						aamp->mhAbrManager.setDefaultInitBitrate(aamp->GetDefaultBitrate());

					}
				}
			}

			if(rate == AAMP_NORMAL_PLAY_RATE)
			{
				// Step 1: Configure the Audio for the playback .Get the audio index/group
				ConfigureAudioTrack();
			}

			// Step 3: Based on the audio selection done , configure the profiles required
			ConfigureVideoProfiles();

			if(rate == AAMP_NORMAL_PLAY_RATE)
			{
				// Step 2: Configure Subtitle track for the playback
				ConfigureTextTrack();
				// Generate audio and text track structures
				PopulateAudioAndTextTracks();
				if(ISCONFIGSET(eAAMPConfig_useRialtoSink) && (currentTextTrackProfileIndex == -1))
				{
					AAMPLOG_INFO("usingRialtoSink - No default text track is selected,configure default text track for rialto");
					SelectSubtitleTrack();
				}
			}



			currentProfileIndex = GetDesiredProfile(false);
			HlsStreamInfo *streamInfo = (HlsStreamInfo*)GetStreamInfo(currentProfileIndex);
			long bandwidthBitsPerSecond = streamInfo->bandwidthBitsPerSecond;
			aamp->ResetCurrentlyAvailableBandwidth(bandwidthBitsPerSecond, trickplayMode, currentProfileIndex);
			aamp->profiler.SetBandwidthBitsPerSecondVideo(bandwidthBitsPerSecond);
			AAMPLOG_INFO("Selected BitRate: %ld, Max BitRate: %ld", bandwidthBitsPerSecond, GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond);
		}
		InitTracks();
		TrackState *audio = trackState[eMEDIATYPE_AUDIO];
		TrackState *video = trackState[eMEDIATYPE_VIDEO];
		TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
		TrackState *aux = trackState[eMEDIATYPE_AUX_AUDIO];

		//Store Bitrate info to Video Track
		if(video)
		{
			video->SetCurrentBandWidth( (int)(GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond) );
			aamp->mCMCDCollector->SetBitrates(eMEDIATYPE_VIDEO, GetVideoBitrates());
		}
		if(ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
		{
			if(audio->enabled)
			{
				if(video)    //CID:136262 - Forward null
				{
					video->enabled = false;
					video->streamOutputFormat = FORMAT_INVALID;
				}
			}
			else
			{
				trackState[eTRACK_VIDEO]->type = eTRACK_AUDIO;
			}
			subtitle->enabled = false;
			subtitle->streamOutputFormat = FORMAT_INVALID;

			//No need to enable auxiliary audio feature for audio only playback scenarios
			aux->enabled = false;
			aux->streamOutputFormat = FORMAT_INVALID;
		}
		aamp->profiler.SetBandwidthBitsPerSecondAudio(audio->GetCurrentBandWidth());
		if (audio->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(audio->mPlaylistUrl, &audio->playlist, audio->mEffectiveUrl, eMEDIATYPE_PLAYLIST_AUDIO))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::audio playlist retrieved from cache");
			}
			if(!audio->playlist.GetLen() )
			{
				audio->FetchPlaylist();
			}
		}
		if (video && video->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(video->mPlaylistUrl, &video->playlist, video->mEffectiveUrl, eMEDIATYPE_PLAYLIST_VIDEO))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::video playlist retrieved from cache");
			}
			if(!video->playlist.GetLen() )
			{
				int lastSelectedProfileIndex = currentProfileIndex;
				int limitCount = 0;
				int numberOfLimit = GETCONFIGVALUE(eAAMPConfig_InitRampDownLimit);
				do{
					video->FetchPlaylist();
					limitCount++;
					if ((!video->playlist.GetLen() ) && (limitCount <= numberOfLimit) ){
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Video playlist download failed, retrying with rampdown logic : %d ( %d )",
						 limitCount, numberOfLimit );
						/** Choose rampdown profile for next retry */
						currentProfileIndex = aamp->mhAbrManager.getRampedDownProfileIndex(currentProfileIndex);
						long bandwidthBitsPerSecond = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
						if(lastSelectedProfileIndex == currentProfileIndex){
							AAMPLOG_INFO("Failed to rampdown from bandwidth : %ld", bandwidthBitsPerSecond);
							break;
						}

						lastSelectedProfileIndex = currentProfileIndex;
						AAMPLOG_INFO("Trying BitRate: %ld, Max BitRate: %ld", bandwidthBitsPerSecond,
						GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond);
						std::string uri = GetPlaylistURI(eTRACK_VIDEO, &video->streamOutputFormat);
						if( !uri.empty() ){
							aamp_ResolveURL(video->mPlaylistUrl, aamp->GetManifestUrl(), uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));

						}else{
							AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Failed to get URL after %d rampdown attempts",
								 limitCount);
							break;
						}

					}else if (video->playlist.GetLen()){
						long bandwidthBitsPerSecond = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
						aamp->ResetCurrentlyAvailableBandwidth(
							bandwidthBitsPerSecond,
							trickplayMode,currentProfileIndex);
						aamp->profiler.SetBandwidthBitsPerSecondVideo(
							bandwidthBitsPerSecond);
						AAMPLOG_INFO("Selected BitRate: %ld, Max BitRate: %ld",
							bandwidthBitsPerSecond,
							GetStreamInfo(GetMaxBWProfile())->bandwidthBitsPerSecond);
						break;
					}
				}while(limitCount <= numberOfLimit);
				if (!video->playlist.GetLen() )
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Video playlist download failed still after %d rampdown attempts",
							limitCount);
				}
			}
		}
		if (subtitle->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(subtitle->mPlaylistUrl, &subtitle->playlist, subtitle->mEffectiveUrl, eMEDIATYPE_PLAYLIST_SUBTITLE))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::subtitle playlist retrieved from cache");
			}
			if (!subtitle->playlist.GetLen() )
			{
				subtitle->FetchPlaylist();
			}
			if (!subtitle->playlist.GetLen() )
			{
				//This is logged as a warning. Not critical to playback
				AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Subtitle playlist download failed");
				subtitle->enabled = false;
			}
		}
		if (aux->enabled)
		{
			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(aux->mPlaylistUrl, &aux->playlist, aux->mEffectiveUrl, eMEDIATYPE_PLAYLIST_AUX_AUDIO))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::auxiliary audio playlist retrieved from cache");
			}
			if (!aux->playlist.GetLen() )
			{
				aux->FetchPlaylist();
			}
			if (!aux->playlist.GetLen() )
			{
				//TODO: This is logged as a warning. Decide if its critical for playback
				AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Auxiliary audio playlist download failed");
				aux->enabled = false;
				aux->streamOutputFormat = FORMAT_INVALID;
			}
		}
		if (video && video->enabled && !video->playlist.GetLen() )
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Video Playlist download failed");
			return eAAMPSTATUS_PLAYLIST_VIDEO_DOWNLOAD_ERROR;
		}
		else if (audio->enabled && !audio->playlist.GetLen() )
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_HLS::Audio Playlist download failed");
			return eAAMPSTATUS_PLAYLIST_AUDIO_DOWNLOAD_ERROR;
		}

		if (rate != AAMP_NORMAL_PLAY_RATE)
		{
			trickplayMode = true;
			if(aamp->IsFogTSBSupported())
			{
				mTrickPlayFPS = GETCONFIGVALUE(eAAMPConfig_LinearTrickPlayFPS);
			}
			else
			{
				mTrickPlayFPS = GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS);
			}
		}
		else
		{
			trickplayMode = false;
		}

		AampTime programStartTime{-1};
		//by default it is true, but it will set to False if the audio format is not MPEGTS.
		//if it is false, no need to apply 500ms offset to pts in processPacket API
		bool audioFormatMPEGTS = true;
		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			TrackState *ts = trackState[iTrack];
			if(ts->enabled)
			{
				AampTime culled{};
				bool playContextConfigured = false;
				if( AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE) )
				{ // use printf to avoid 2048 char syslog limitation
					printf("***Initial Playlist:******\n\n%.*s\n*****************\n", (int)ts->playlist.GetLen(), ts->playlist.GetPtr() );
				}
				// Flag also denotes if first encrypted init fragment was pushed or not
				ts->mCheckForInitialFragEnc = true; //force encrypted header at the start
				ts->IndexPlaylist(!newTune, culled);
				ts->lastPlaylistIndexedTimeMS = aamp_GetCurrentTimeMS();
				if (IsLive() && eTRACK_VIDEO == ts->type)
				{
					programStartTime = ts->mProgramDateTime;

					if( culled > 0)
					{
						AAMPLOG_INFO("Updating PDT (%f) and culled (%f) Updated seek_pos_seconds:%f ",ts->mProgramDateTime.inSeconds(), culled.inSeconds(), (seekPosition.inSeconds() - culled.inSeconds()));
						aamp->mProgramDateTime = ts->mProgramDateTime.inSeconds();
						aamp->UpdateCullingState(culled.inSeconds()); // report amount of content that was implicitly culled since last playlist download
						SeekPosUpdate(seekPosition.inSeconds() - culled.inSeconds());
					}
				}
				if(ts->mDrmMetaDataIndex.size() > 0)
				{
					AAMPLOG_ERR("TrackState: Sending Error event DRM unsupported");
					return eAAMPSTATUS_UNSUPPORTED_DRM_ERROR;
				}
				if (ts->mDuration == 0.0f)
				{
					//TODO: Confirm if aux audio playlist has issues, it should be deemed as a playback failure
					if (iTrack == eTRACK_SUBTITLE || iTrack == eTRACK_AUX_AUDIO)
					{
						//Subtitle is optional and not critical to playback
						ts->enabled = false;
						ts->streamOutputFormat = FORMAT_INVALID;
						AAMPLOG_ERR("StreamAbstractionAAMP_HLS::%s playlist duration is zero!!",
								 ts->name);
					}
					else
					{
						break;
					}
				}
				// Send Metadata for Video playlist
				if(iTrack == eTRACK_VIDEO)
				{
					bool bMetadata = ISCONFIGSET(eAAMPConfig_BulkTimedMetaReport);
					ts->FindTimedMetadata(bMetadata, true);
					if(bMetadata && newTune)
					{
						// Send bulk report
						aamp->ReportBulkTimedMetadata();
					}
				}

				if (iTrack == eMEDIATYPE_VIDEO)
				{
					maxIntervalBtwPlaylistUpdateMs = (int)(2 * ts->targetDurationSeconds.milliseconds()); //Time interval for periodic playlist update
					if (maxIntervalBtwPlaylistUpdateMs > DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS)
					{
						maxIntervalBtwPlaylistUpdateMs = DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS;
					}
					aamp->UpdateRefreshPlaylistInterval(maxIntervalBtwPlaylistUpdateMs/1000.0);
				}

				lstring iter = lstring(ts->playlist.GetPtr(),ts->playlist.GetLen());
				ts->fragmentURI = iter.mystrpbrk();
				StreamOutputFormat format = GetFormatFromFragmentExtension(ts->playlist);
				if (FORMAT_ISO_BMFF == format)
				{
					//Disable subtitle in mp4 format, as we don't support it for now
					if (eMEDIATYPE_SUBTITLE == iTrack)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Unsupported subtitle format from fragment extension:%d",  format);
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI.clear();
						ts->enabled = false;
					}
					//TODO: Extend auxiliary audio support for fragmented mp4 asset in future
					else if (eMEDIATYPE_AUX_AUDIO == iTrack)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Auxiliary audio not supported for FORMAT_ISO_BMFF, disabling!");
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI.clear();
						ts->enabled = false;
					}
					else
					{
						ts->streamOutputFormat = FORMAT_ISO_BMFF;
						if (eMEDIATYPE_VIDEO == iTrack)
						{
							InitializeMediaProcessor();
						}
					}
					continue;
				}
				// Not ISOBMFF, no need for encrypted header check and associated logic
				// But header identification might have been already done, if EXT-X-MAP is present in playlist
				ts->mCheckForInitialFragEnc = false;
				// Elementary stream, we can skip playContext creation
				if (FORMAT_AUDIO_ES_AAC == format)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_AUDIO_ES_AAC", ts->name);
					ts->streamOutputFormat = FORMAT_AUDIO_ES_AAC;
					aamp->SetAudioPlayContextCreationSkipped( true );
					audioFormatMPEGTS = false;
					continue;
				}

				// Elementary stream, we can skip playContext creation
				if (FORMAT_AUDIO_ES_AC3 == format)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_AUDIO_ES_AC3", ts->name);
					ts->streamOutputFormat = FORMAT_AUDIO_ES_AC3;
					aamp->SetAudioPlayContextCreationSkipped( true );
					audioFormatMPEGTS = false;
					continue;
				}

				// Elementary stream, we can skip playContext creation
				if (FORMAT_AUDIO_ES_EC3 == format)
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Track[%s] - FORMAT_AUDIO_ES_EC3", ts->name);
					ts->streamOutputFormat = FORMAT_AUDIO_ES_EC3;
					aamp->SetAudioPlayContextCreationSkipped( true );
					audioFormatMPEGTS = false;
					continue;
				}

				if (eMEDIATYPE_SUBTITLE == iTrack)
				{
					bool subtitleDisabled = false;
					if (this->rate != AAMP_NORMAL_PLAY_RATE)
					{
						subtitleDisabled = true;
					}
					else if (format != FORMAT_SUBTITLE_WEBVTT)
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Unsupported subtitle format from fragment extension:%d", format);
						subtitleDisabled = true;
					}

					//Configure parser for subtitle
					if (!subtitleDisabled)
					{
						ts->streamOutputFormat = format;
						SubtitleMimeType type = (format == FORMAT_SUBTITLE_WEBVTT) ? eSUB_TYPE_WEBVTT : eSUB_TYPE_UNKNOWN;
						if(!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
						{
							AAMPLOG_WARN("Legacy subtec");
							ts->mSubtitleParser = this->RegisterSubtitleParser_CB(type);
						}
						else
						{
							AAMPLOG_WARN("GST subtec");
							if (aamp->WebVTTCueListenersRegistered())
							{
								int width = 0, height = 0;
								PlayerCallbacks playerCallBack = {};
								this->InitializePlayerCallbacks(playerCallBack);
								aamp->GetPlayerVideoSize(width, height);
								ts->mSubtitleParser = subtec_make_unique<WebVTTParser>(type, width, height);
								if(ts->mSubtitleParser)
								{
									ts->mSubtitleParser->RegisterCallback(playerCallBack);
								}
							}
						}
						if (!ts->mSubtitleParser)
						{
							if(!ISCONFIGSET(eAAMPConfig_GstSubtecEnabled))
							{
								AAMPLOG_WARN("No subtec, no sub parser");
								ts->streamOutputFormat = FORMAT_INVALID;
								ts->fragmentURI.clear();
								ts->enabled = false;
							}
						}
						aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
					}
					else
					{
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI.clear();
						ts->enabled = false;
					}
					continue; //no playcontext config for subtitle
				}
				else if (eMEDIATYPE_AUX_AUDIO == iTrack)
				{
					if (this->rate == AAMP_NORMAL_PLAY_RATE)
					{
						if (format == FORMAT_MPEGTS)
						{
							AAMPLOG_WARN("Configure auxiliary audio TS track demuxing");
							ts->playContext = std::make_shared<TSProcessor>(aamp, eStreamOp_DEMUX_AUX, mID3Handler);
							ts->SourceFormat(FORMAT_MPEGTS);
							if (ts->playContext)
							{
								ts->playContext->setRate(this->rate, PlayMode_normal);
								ts->playContext->setThrottleEnable(false);
								playContextConfigured = true;
							}
							else
							{
								ts->streamOutputFormat = format;
							}
						}
						else if (FORMAT_INVALID != format)
						{
							AAMPLOG_WARN("Configure auxiliary audio format based on extension");
							ts->streamOutputFormat = format;
						}
						else
						{
							AAMPLOG_WARN("Keeping auxiliary audio format from playlist");
						}
					}
					else
					{
						AAMPLOG_WARN("Disable auxiliary audio format - trick play");
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI.clear();
						ts->enabled = false;
					}
				}
				else if (eMEDIATYPE_AUDIO == iTrack)
				{
					if (this->rate == AAMP_NORMAL_PLAY_RATE)
					{
						// Creation of playContext is required only for TS fragments
						if (format == FORMAT_MPEGTS)
						{
							AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Configure audio TS track demuxing");
							ts->playContext = std::make_shared<TSProcessor>(aamp, eStreamOp_DEMUX_AUDIO, mID3Handler);
							ts->SourceFormat(FORMAT_MPEGTS);
							if(ts->playContext)
							{
								if (currentAudioProfileIndex >= 0 )
								{
									std::string groupId = mediaInfoStore[currentAudioProfileIndex].group_id;
									ts->playContext->SetAudioGroupId(groupId);
								}
							}

							if (ts->playContext)
							{
								ts->playContext->setRate(this->rate, PlayMode_normal);
								ts->playContext->setThrottleEnable(false);
								playContextConfigured = true;
							}
							else
							{
								ts->streamOutputFormat = format;
							}
						}
						else if (FORMAT_INVALID != format)
						{
							AAMPLOG_WARN("Configure audio format based on extension");
							ts->streamOutputFormat = format;
						}
						else
						{
							AAMPLOG_WARN("Keeping audio format from playlist");
						}
					}
					else
					{
						AAMPLOG_WARN("Disable audio format - trick play");
						ts->streamOutputFormat = FORMAT_INVALID;
						ts->fragmentURI.clear();
						ts->enabled = false;
					}
				}
				else if (iTrack == eMEDIATYPE_VIDEO)
				{
					/*Populate format from codec data*/
					format = GetStreamOutputFormatForTrack(eTRACK_VIDEO);

					if (FORMAT_INVALID != format)
					{
						ts->streamOutputFormat = format;
						// Check if auxiliary audio is muxed here, by confirming streamOutputFormat != FORMAT_INVALID
						if (!aux->enabled && (aux->streamOutputFormat != FORMAT_INVALID) && (AAMP_NORMAL_PLAY_RATE == rate))
						{
							ts->demuxOp = eStreamOp_DEMUX_VIDEO_AND_AUX;
						}
						else if ((trackState[eTRACK_AUDIO]->enabled) || (AAMP_NORMAL_PLAY_RATE != rate))
						{
							ts->demuxOp = eStreamOp_DEMUX_VIDEO;
						}
						else
						{
							// In case of muxed, where there is no X-MEDIA tag but CODECS show presence of audio
							// This could be changed later, once we let TSProcessor configure tracks based on demux status
							StreamOutputFormat audioFormat = GetStreamOutputFormatForTrack(eTRACK_AUDIO);
							if (audioFormat != FORMAT_UNKNOWN)
							{
								trackState[eMEDIATYPE_AUDIO]->streamOutputFormat = audioFormat;
							}

							// Even if audio info is not present in manifest, we let TSProcessor run a full sweep
							// If audio is found, then TSProcessor will configure stream sink accordingly
							if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
							{
								// For muxed tracks, demux audio and video
								ts->demuxOp = eStreamOp_DEMUX_ALL;
							}
							else
							{
								// Audio only playback, disable video
								ts->demuxOp = eStreamOp_DEMUX_AUDIO;
								video->streamOutputFormat = FORMAT_INVALID;
							}
						}
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : Configure video TS track demuxing demuxOp %d", ts->demuxOp);
						ts->playContext = std::make_shared<TSProcessor>(aamp, ts->demuxOp, mID3Handler, eMEDIATYPE_VIDEO,
							std::static_pointer_cast<TSProcessor> (trackState[eMEDIATYPE_AUDIO]->playContext).get(),
							std::static_pointer_cast<TSProcessor>(trackState[eMEDIATYPE_AUX_AUDIO]->playContext).get());
						ts->SourceFormat(FORMAT_MPEGTS);

						if(ts->playContext)
						{
							if(!audioFormatMPEGTS)
							{
								//video track is MPEGTS but not the audio track. So setting a variable as false
								// to avoid applying offset to video pts  in processPacket for this particular playback.
								// Otherwise it might cause av sync issues.
								ts->playContext->setApplyOffsetFlag(false);
							}
							else
							{
								//both audio and video in TS format or muxed
								ts->playContext->setApplyOffsetFlag(true);
							}
							ts->playContext->setThrottleEnable(this->enableThrottle);
							if (currentAudioProfileIndex >= 0 )
							{
								std::string groupId = mediaInfoStore[currentAudioProfileIndex].group_id;
								ts->playContext->SetAudioGroupId(groupId);
							}
						}
						if (this->rate == AAMP_NORMAL_PLAY_RATE)
						{
							ts->playContext->setRate(this->rate, PlayMode_normal);
						}
						else
						{
							ts->playContext->setRate(this->rate, PlayMode_retimestamp_Ionly);
							ts->playContext->setFrameRateForTM(mTrickPlayFPS);
						}
						playContextConfigured = true;
					}
					else
					{
						AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : VideoTrack -couldn't determine format from streamInfo->codec %s",
							 streamInfoStore[currentProfileIndex].codecs.c_str() );
					}
				}

				if (!playContextConfigured && (ts->streamOutputFormat == FORMAT_MPEGTS))
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Init : track %p context configuring for eStreamOp_NONE", ts);
					ts->playContext = std::make_shared<TSProcessor>(aamp, eStreamOp_NONE, mID3Handler, iTrack);
					ts->SourceFormat(FORMAT_MPEGTS);
					ts->playContext->setThrottleEnable(this->enableThrottle);
					if (this->rate == AAMP_NORMAL_PLAY_RATE)
					{
						this->trickplayMode = false;
						ts->playContext->setRate(this->rate, PlayMode_normal);
					}
					else
					{
						this->trickplayMode = true;
						if(aamp->IsFogTSBSupported())
						{
							mTrickPlayFPS = GETCONFIGVALUE(eAAMPConfig_LinearTrickPlayFPS);
						}
						else
						{
							mTrickPlayFPS = GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS);
						}
						ts->playContext->setRate(this->rate, PlayMode_retimestamp_Ionly);
						ts->playContext->setFrameRateForTM(mTrickPlayFPS);
					}
				}
			}
		}
		// Set mIsLiveStream to keep live the history.
		if(newTune)
		{
			aamp->SetIsLiveStream(aamp->IsLive());
		}

		//reiterate loop when player receive an update in seek position
		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			trackState[iTrack]->playTarget = seekPosition;
			trackState[iTrack]->playTargetBufferCalc = seekPosition;
			trackState[iTrack]->playlistCulledOffset = 0;
		}

		if ((video->enabled && video->mDuration == 0.0f) || (audio->enabled && audio->mDuration == 0.0f))
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_HLS: Track Duration is 0. Cannot play this content");
			return eAAMPSTATUS_MANIFEST_CONTENT_ERROR;
		}

		if (newTune)
		{
			aamp->mIsIframeTrackPresent = mIframeAvailable;
			mProgramStartTime = programStartTime.inSeconds();
			// Delay "preparing" state until all tracks have been processed.
			// JS Player assumes all onTimedMetadata event fire before "preparing" state.
			aamp->SetState(eSTATE_PREPARING);
		}

		//Currently un-used playlist indexed event, might save some JS overhead
		if (!ISCONFIGSET(eAAMPConfig_DisablePlaylistIndexEvent))
		{
			aamp->SendEvent(std::make_shared<AAMPEventObject>(AAMP_EVENT_PLAYLIST_INDEXED, aamp->GetSessionId()),AAMP_EVENT_ASYNC_MODE);
		}
		if (newTune)
		{
			if(ContentType_UNKNOWN == aamp->GetContentType())
			{
				if(aamp->IsLive())
				{
					aamp->SetContentType("LINEAR_TV");
				}
				else
				{
					aamp->SetContentType("VOD");
				}
			}


			if (eTUNED_EVENT_ON_PLAYLIST_INDEXED == aamp->GetTuneEventConfig(aamp->IsLive()))
			{
				if (aamp->SendTunedEvent())
				{
					AAMPLOG_WARN("aamp: hls - sent tune event after indexing playlist");
				}
			}
		}

		if (aamp->IsLive())
		{
			/** Set preferred live Offset for 4K or non 4K; Default value of mIsStream4K = false */
			aamp->mIsStream4K = GetPreferredLiveOffsetFromConfig();

			/* Check if we are seeking to live using aamp->seek_pos_seconds */
			if (tuneType == eTUNETYPE_SEEK && IsSeekedToLive(aamp->seek_pos_seconds))
			{
				tuneType = eTUNETYPE_SEEKTOLIVE;
			}
		}

		/*Do live adjust on live streams on 1. eTUNETYPE_NEW_NORMAL, 2. eTUNETYPE_SEEKTOLIVE,
		 * 3. Seek to a point beyond duration*/
		bool liveAdjust = (eTUNETYPE_NEW_NORMAL == tuneType)  && aamp->IsLiveAdjustRequired() && (aamp->IsLive());
		if ((eTUNETYPE_SEEKTOLIVE == tuneType) && aamp->IsLive())
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_HLS: eTUNETYPE_SEEKTOLIVE, reset playTarget and enable liveAdjust");
			liveAdjust = true;

			audio->playTarget = 0;
			video->playTarget = 0;
			subtitle->playTarget = 0;
			aux->playTarget = 0;
			aamp->NotifyOnEnteringLive();
			aamp->mDisableRateCorrection = false;
		}
		else if (((eTUNETYPE_SEEK == tuneType) || (eTUNETYPE_RETUNE == tuneType) || (eTUNETYPE_NEW_SEEK == tuneType)) && (this->rate > 0))
		{
			AampTime seekWindowEnd{video->mDuration};
			if(aamp->IsLive())
			{
				seekWindowEnd -= aamp->mLiveOffset ;
			}
			// check if seek beyond live point
			if (video->playTarget.nearestSecond() >= seekWindowEnd.nearestSecond())
			{
				if (aamp->IsLive())
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: playTarget > seekWindowEnd , playTarget:%f and seekWindowEnd:%f",
							video->playTarget.inSeconds() , seekWindowEnd.inSeconds());
					liveAdjust = true;

					audio->playTarget = 0;
					video->playTarget = 0;
					subtitle->playTarget = 0;
					aux->playTarget = 0;
					if (eTUNETYPE_SEEK == tuneType)
					{
						aamp->NotifyOnEnteringLive();
					}
					AAMPLOG_INFO("StreamAbstractionAAMP_HLS: Live latency correction is enabled due to the seek (rate=%f) to live window!!", this->rate);
					aamp->mDisableRateCorrection = false;
				}
				else
				{
					video->eosReached = true;
					video->fragmentURI.clear();
					audio->eosReached = true;
					audio->fragmentURI.clear();
					subtitle->eosReached = true;
					subtitle->fragmentURI.clear();
					aux->eosReached = true;
					aux->fragmentURI.clear();
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: seek target out of range, mark EOS. playTarget:%f End:%f. ",
							video->playTarget.inSeconds(), seekWindowEnd.inSeconds());

					return eAAMPSTATUS_SEEK_RANGE_ERROR;
				}
			}
		}

		// in case of muxed a/v and auxiliary track scenario
		// For demuxed a/v, we will handle it in SyncTracks...() function
		if (audio->enabled || aux->enabled)
		{
			TrackState *other = audio->enabled ? audio : aux;
			if (!aamp->IsLive())
			{
				retval = SyncTracksForDiscontinuity();
				if (eAAMPSTATUS_OK != retval)
				{
					return retval;
				}
			}
			else
			{
				if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
				{
					auto count = video->mDiscontinuityIndex.size();
					if (!liveAdjust && count>0 && count == other->mDiscontinuityIndex.size() )
					{ // FIXME
						SyncTracksForDiscontinuity();
					}
				}
			}
		}
		else if (subtitle->enabled)
		{
			//TODO:Muxed track with subtitles. Need to sync tracks
		}

		if (liveAdjust)
		{
			AampTime xStartOffset{video->GetXStartTimeOffset()};
			AampTime offsetFromLive{aamp->mLiveOffset};
			// check if there is xStartOffSet , if non zero value present ,check if it is > 3 times TD(Spec requirement)
			if(xStartOffset != 0 && abs(xStartOffset.inSeconds()) > (3*video->targetDurationSeconds))
			{
				// code added for negative offset values
				// that is offset from last duration
				if(xStartOffset < 0)
				{
					offsetFromLive = abs(xStartOffset.inSeconds());
					AAMPLOG_WARN("liveOffset modified with X-Start to :%f",offsetFromLive.inSeconds());
				}
				// if xStartOffset is positive value , then playposition to be considered from beginning
				// TBD for later.Only offset from end is supported now . That too only for live . Not for VOD!!!!
			}

			if (video->mDuration > (offsetFromLive.inSeconds() + video->playTargetOffset))
			{
				// a) Get OffSet to Live for Video and Audio separately.
				// b) Set to minimum value among video /audio instead of setting to 0 position
				AampTime offsetToLiveVideo{}, offsetToLiveAudio{}, offsetToLive{};
				offsetToLiveVideo = offsetToLiveAudio = video->mDuration - offsetFromLive.inSeconds() - video->playTargetOffset;
				//TODO: Handle case for muxed a/v and aux track
				if (audio->enabled)
				{
					offsetToLiveAudio = 0;
					// if audio is not having enough total duration to adjust , then offset value set to 0
					if( audio->mDuration > (offsetFromLive + audio->playTargetOffset))
						offsetToLiveAudio = audio->mDuration - offsetFromLive -  audio->playTargetOffset;
					else
						AAMPLOG_WARN("aamp: live adjust not possible A-total[%f]< (A-offsetFromLive[%f] + A-playTargetOffset[%f]) A-target[%f]", audio->mDuration.inSeconds(), offsetFromLive.inSeconds(), audio->playTargetOffset.inSeconds(), audio->playTarget.inSeconds());
				}
				// pick the min of video/audio offset
				offsetToLive = (std::min)(offsetToLiveVideo,offsetToLiveAudio);
				video->playTarget += offsetToLive;
				video->playTargetBufferCalc = video->playTarget;
				if (audio->enabled )
				{
					audio->playTarget += offsetToLive;
					audio->playTargetBufferCalc = audio->playTarget;
				}
				if (subtitle->enabled)
				{
					subtitle->playTarget += offsetToLive;
					subtitle->playTargetBufferCalc = subtitle->playTarget;
				}
				if (aux->enabled)
				{
					aux->playTarget += offsetToLive;
					aux->playTargetBufferCalc = aux->playTarget;
				}
				// Entering live will happen if offset is adjusted , if its 0 playback is starting from beginning
				if(offsetToLive != 0.0)
					mIsAtLivePoint = true;
				AAMPLOG_WARN("aamp: after live adjust - V-target %f A-target %f S-target %f Aux-target %f offsetFromLive %f offsetToLive %f offsetVideo[%f] offsetAudio[%f] AtLivePoint[%d]",
						video->playTarget.inSeconds(), audio->playTarget.inSeconds(), subtitle->playTarget.inSeconds(), aux->playTarget.inSeconds(), offsetFromLive.inSeconds(), offsetToLive.inSeconds(),offsetToLiveVideo.inSeconds(),offsetToLiveAudio.inSeconds(),mIsAtLivePoint);
			}
			else
			{
				AAMPLOG_WARN("aamp: live adjust not possible V-total[%f] < (V-offsetFromLive[%f] + V-playTargetOffset[%f]) V-target[%f]",
					video->mDuration.inSeconds(), offsetFromLive.inSeconds(), video->playTargetOffset.inSeconds(), video->playTarget.inSeconds());
			}
			//Set live adjusted position to seekPosition
			SeekPosUpdate(video->playTarget.inSeconds());

		}
		/*Adjust for discontinuity*/
		if ((audio->enabled || aux->enabled) && (aamp->IsLive()) && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
		{
			TrackState *otherTrack = audio->enabled ? audio : aux;
			auto discontinuityIndexCount = video->mDiscontinuityIndex.size();
			if (discontinuityIndexCount > 0)
			{
				if (discontinuityIndexCount == otherTrack->mDiscontinuityIndex.size())
				{
					if (liveAdjust)
					{
						SyncTracksForDiscontinuity();
					}
					AampTime videoPrevDiscontinuity{};
					AampTime audioPrevDiscontinuity{};
					AampTime videoNextDiscontinuity{};
					AampTime audioNextDiscontinuity{};
					for (auto i = 0; i <= video->mDiscontinuityIndex.size(); i++)
					{
						DiscontinuityIndexNode & videoDiscontinuity = video->mDiscontinuityIndex[i];
						DiscontinuityIndexNode & audioDiscontinuity = audio->mDiscontinuityIndex[i];
						if (i < discontinuityIndexCount)
						{
							videoNextDiscontinuity = videoDiscontinuity.position;
							audioNextDiscontinuity = audioDiscontinuity.position;
						}
						else
						{
							videoNextDiscontinuity = aamp->GetDurationMs() / 1000;
							audioNextDiscontinuity = videoNextDiscontinuity;
						}
						if ((videoNextDiscontinuity > (video->playTarget + 5))
							&& (audioNextDiscontinuity > (otherTrack->playTarget + 5)))
						{
							AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: video->playTarget %f videoPrevDiscontinuity %f videoNextDiscontinuity %f",
									video->playTarget.inSeconds(), videoPrevDiscontinuity.inSeconds(), videoNextDiscontinuity.inSeconds());
							AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: %s->playTarget %f audioPrevDiscontinuity %f audioNextDiscontinuity %f",
									otherTrack->name, otherTrack->playTarget.inSeconds(), audioPrevDiscontinuity.inSeconds(), audioNextDiscontinuity.inSeconds());
							if (video->playTarget < videoPrevDiscontinuity)
							{
								AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: [video] playTarget(%f) advance to discontinuity(%f)",
										video->playTarget.inSeconds(), videoPrevDiscontinuity.inSeconds());
								video->playTarget = videoPrevDiscontinuity;
								video->playTargetBufferCalc = video->playTarget;
							}
							if (otherTrack->playTarget < audioPrevDiscontinuity)
							{
								AAMPLOG_WARN( "StreamAbstractionAAMP_HLS: [%s] playTarget(%f) advance to discontinuity(%f)",
										otherTrack->name, otherTrack->playTarget.inSeconds(), audioPrevDiscontinuity.inSeconds());
								otherTrack->playTarget = audioPrevDiscontinuity;
								otherTrack->playTargetBufferCalc = otherTrack->playTarget;
							}
							break;
						}
						videoPrevDiscontinuity = videoNextDiscontinuity;
						audioPrevDiscontinuity = audioNextDiscontinuity;
					}
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: videoPeriodPositionIndex.size %zu audioPeriodPositionIndex.size %zu",
							video->mDiscontinuityIndex.size(), otherTrack->mDiscontinuityIndex.size());
				}
			}
			else
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: videoPeriodPositionIndex.size 0");
			}
		}

		audio->lastPlaylistDownloadTimeMS = aamp_GetCurrentTimeMS();
		video->lastPlaylistDownloadTimeMS = audio->lastPlaylistDownloadTimeMS;
		subtitle->lastPlaylistDownloadTimeMS = audio->lastPlaylistDownloadTimeMS;
		aux->lastPlaylistDownloadTimeMS = audio->lastPlaylistDownloadTimeMS;
		/*Use start timestamp as zero when audio is not elementary stream*/
		mStartTimestampZero = ((video->streamOutputFormat == FORMAT_ISO_BMFF || audio->streamOutputFormat == FORMAT_ISO_BMFF) || (rate == AAMP_NORMAL_PLAY_RATE && (!audio->enabled || audio->playContext)));

		if (rate == AAMP_NORMAL_PLAY_RATE)
		{
			// this functionality needed for normal playback , not for trickplay .
			// After calling GetNextFragmentUriFromPlaylist , all LFs are removed from fragment info
			// inside GetIframeFragmentUriFromIndex , there is check for LF which fails as its already removed and ends up returning NULL uri
			// So enforcing this strictly for normal playrate

			for (int iTrack = 0; iTrack <= AAMP_TRACK_COUNT - 1; iTrack++)
			{
				TrackState *ts = trackState[iTrack];
				if(ts->enabled)
				{
					bool reloadUri = false;
					ts->fragmentURI = ts->GetNextFragmentUriFromPlaylist(reloadUri, true);
					ts->playTarget = ts->playlistPosition;
					ts->playTargetBufferCalc = ts->playTarget;
				}

				// To avoid audio loss while seeking HLS/TS AV of different duration w/o affecting VOD Discontinuities
				if(iTrack == 0 && ISCONFIGSET(eAAMPConfig_SyncAudioFragments) && !(ISCONFIGSET(eAAMPConfig_MidFragmentSeek) || audio->mDiscontinuityIndex.size()))
				{
					AAMPLOG_TRACE("Setting audio playtarget %f to video playtarget %f", audio->playTarget.inSeconds(), ts->playTarget.inSeconds());
					audio->playTarget = ts->playTarget;
					subtitle->playTarget = ts->playTarget;
				}
			}
			if (IsLive() && audio->enabled && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback))
			{
				AAMPStatusType retValue = SyncTracks();
				if (eAAMPSTATUS_OK != retValue)
					return retValue;
			}

			//Set live adjusted position to seekPosition
			if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
			{
				midSeekPtsOffset = seekPosition - video->playTarget.inSeconds();
				if(midSeekPtsOffset > video->fragmentDurationSeconds/2)
				{
					if(aamp->GetInitialBufferDuration() == 0)
					{
						AAMPPlayerState state = aamp->GetState();
						if(state == eSTATE_SEEKING)
						{
							// To prevent underflow when seeked to end of fragment.
							// Added +1 to ensure next fragment is fetched.
							SETCONFIGVALUE(AAMP_STREAM_SETTING,eAAMPConfig_InitialBuffer,(int)video->fragmentDurationSeconds + 1);
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

				if(midSeekPtsOffset > 0.0)
				{
					midSeekPtsOffset += 0.5 ;  // Adding 0.5 to neutralize PTS-500ms in BasePTS calculation.
				}
				SeekPosUpdate(seekPosition.inSeconds());
			}
			else
			{
				SeekPosUpdate(video->playTarget.inSeconds());
			}
			AAMPLOG_WARN("seekPosition updated with corrected playtarget : %f midSeekPtsOffset : %f",seekPosition.inSeconds(), midSeekPtsOffset.inSeconds());
		}

		if (subtitle->enabled && subtitle->mSubtitleParser)
		{
			//Need to set reportProgressOffset to subtitleParser
			//playTarget becomes seek_pos_seconds and playlistPosition is the actual position in playlist
			double offset = (double)(subtitle->playlistPosition.milliseconds() - seekPosition.milliseconds());
			AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Setting setProgressEventOffset value of %.3f ms", offset);
			subtitle->mSubtitleParser->setProgressEventOffset(offset);
		}

		// negative buffer calculation fix
		for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
		{
			if (aamp->culledSeconds > 0)
			{
				trackState[iTrack]->playTargetBufferCalc = aamp->culledSeconds + seekPosition;
				trackState[iTrack]->playlistCulledOffset = aamp->culledSeconds;
			}
		}


		if (newTune && ISCONFIGSET(eAAMPConfig_PrefetchIFramePlaylistDL))
		{
			int iframeStreamIdx = GetIframeTrack();
			if (0 <= iframeStreamIdx)
			{
				std::string defaultIframePlaylistUrl;
				std::string defaultIframePlaylistEffectiveUrl;
				//To avoid clashing with the http error for master manifest
				int http_error = 0;
				AampGrowableBuffer defaultIframePlaylist("defaultIframePlaylist");
				HlsStreamInfo *streamInfo = (HlsStreamInfo *)GetStreamInfo(iframeStreamIdx);
				aamp_ResolveURL(defaultIframePlaylistUrl, aamp->GetManifestUrl(), streamInfo->uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
				AAMPLOG_TRACE("StreamAbstractionAAMP_HLS:: Downloading iframe playlist");
				bool bFiledownloaded = false;
				if( !aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(defaultIframePlaylistUrl, &defaultIframePlaylist, defaultIframePlaylistEffectiveUrl, eMEDIATYPE_PLAYLIST_IFRAME) )
				{
					double tempDownloadTime{0.0};
					bFiledownloaded = aamp->GetFile(defaultIframePlaylistUrl, eMEDIATYPE_PLAYLIST_IFRAME, &defaultIframePlaylist, defaultIframePlaylistEffectiveUrl, &http_error, &tempDownloadTime, NULL,eCURLINSTANCE_MANIFEST_MAIN);
					AampTime downloadTime{tempDownloadTime};
					//update videoend info
					ManifestData manifestData(downloadTime.milliseconds(), defaultIframePlaylist.GetLen());
					aamp->UpdateVideoEndMetrics( eMEDIATYPE_MANIFEST,streamInfo->bandwidthBitsPerSecond,http_error,defaultIframePlaylistEffectiveUrl, downloadTime.inSeconds(), &manifestData);
				}
				if (defaultIframePlaylist.GetLen() && bFiledownloaded)
				{
					aamp->getAampCacheHandler()->InsertToPlaylistCache(defaultIframePlaylistUrl, &defaultIframePlaylist, defaultIframePlaylistEffectiveUrl,aamp->IsLive(),eMEDIATYPE_PLAYLIST_IFRAME);
					AAMPLOG_TRACE("StreamAbstractionAAMP_HLS:: Cached iframe playlist");
				}
				else
				{
					AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Error Download iframe playlist. http_error %d",
							http_error);
				}
			}
		}

		if (newTune && !aamp->IsLive() && (aamp->mPreCacheDnldTimeWindow > 0) && (aamp->durationSeconds > aamp->mPreCacheDnldTimeWindow*60))
		{
			// Special requirement
			// work around Adobe SSAI session lifecycle problem
			// If stream is VOD ( SSAI) , and if App configures PreCaching enabled ,
			// then all the playlist needs to be downloaded lazily and cached . This is to overcome gap
			// in VOD Server as it looses the Session Context after playback starts
			// This caching is for all substream ( video/audio/webvtt)
			PreCachePlaylist();
		}


		retval = eAAMPSTATUS_OK;
	}

	if(updateVideoEndMetrics)
	{
		//update videoend info
		ManifestData manifestData((long)(mainManifestdownloadTime*1000), this->mainManifest.GetLen(), parseTimeMs);
		aamp->UpdateVideoEndMetrics( eMEDIATYPE_MANIFEST,0,http_error,aamp->GetManifestUrl(), mainManifestdownloadTime, &manifestData);
	}
	return retval;
}

/***************************************************************************
 * @brief  Function to initiate tracks
 *
 *************************************************************************/
void StreamAbstractionAAMP_HLS::InitTracks()
{
	for (int iTrack = AAMP_TRACK_COUNT - 1; iTrack >= 0; iTrack--)
	{
		const char* trackName = "subs";
		if (eTRACK_VIDEO == iTrack)
		{
			if(trackState[eTRACK_AUDIO]->enabled)
			{
				trackName = "video";
			}
			else if (rate != AAMP_NORMAL_PLAY_RATE)
			{
				trackName = "iframe";
			}
			else
			{
				trackName = "muxed";
			}
		}
		else if (eTRACK_AUDIO == iTrack)
		{
			trackName = "audio";
		}
		else if (eTRACK_AUX_AUDIO == iTrack)
		{
			trackName = "aux-audio";
		}
		trackState[iTrack] = new TrackState((TrackType)iTrack, this, aamp, trackName, mID3Handler, mPtsOffsetUpdate);
		TrackState *ts = trackState[iTrack];
		ts->playlistPosition = -1;
		ts->playTarget = seekPosition;
		ts->playTargetBufferCalc = seekPosition;
		if (iTrack == eTRACK_SUBTITLE && !aamp->IsSubtitleEnabled())
		{
			AAMPLOG_INFO("StreamAbstractionAAMP_HLS::subtitles disabled by application");
			ts->enabled = false;
			ts->streamOutputFormat = FORMAT_INVALID;
			continue;
		}
		if (iTrack == eTRACK_AUX_AUDIO)
		{
			if (!aamp->IsAuxiliaryAudioEnabled())
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::auxiliary audio disabled");
				ts->enabled = false;
				ts->streamOutputFormat = FORMAT_INVALID;
				continue;
			}
			else if (aamp->GetAuxiliaryAudioLanguage() == aamp->mAudioTuple.language)
			{
				AAMPLOG_INFO("StreamAbstractionAAMP_HLS::auxiliary audio same as primary audio, set forward audio flag");
				ts->enabled = false;
				ts->streamOutputFormat = FORMAT_INVALID;
				SetAudioFwdToAuxStatus(true);
				continue;
			}
		}
		std::string uri = GetPlaylistURI((TrackType)iTrack, &ts->streamOutputFormat);
		if( !uri.empty() )
		{
			aamp_ResolveURL(ts->mPlaylistUrl, aamp->GetManifestUrl(), uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
			if(ts->streamOutputFormat != FORMAT_INVALID)
			{
				ts->enabled = true;
				mNumberOfTracks++;
			}
			else
			{
				AAMPLOG_WARN("StreamAbstractionAAMP_HLS: %s format could not be determined. codecs %s", ts->name, streamInfoStore[currentProfileIndex].codecs.c_str() );
			}
		}
	}
}

/***************************************************************************
* @fn CachePlaylistThreadFunction
* @brief Thread function created for PreCaching playlist
*
* @param This[in] PrivateAampInstance Context
* @return none
***************************************************************************/
void StreamAbstractionAAMP_HLS::CachePlaylistThreadFunction(void)
{
	// required to work around Adobe SSAI session lifecycle problem
	// Temporary workaround code 
	aamp->PreCachePlaylistDownloadTask();
	return;
}

/**
 * @brief Function to initiate precaching of playlist
 */
void StreamAbstractionAAMP_HLS::PreCachePlaylist()
{
	// required to work around Adobe SSAI session lifecycle problem
	// Tasks to be done
	// Run through all the streamInfo and get uri for download , push to a download list
	// Start a thread and return back . This thread will wake up after Tune completion
	// and start downloading the uri in the list
	PreCacheUrlList dnldList ;
	for (auto& streamInfo : streamInfoStore)
	{
		// Add Video and IFrame Profiles
		PreCacheUrlStruct newelem;
		aamp_ResolveURL(newelem.url, aamp->GetManifestUrl(), streamInfo.uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
		newelem.type = streamInfo.isIframeTrack?eMEDIATYPE_PLAYLIST_IFRAME:eMEDIATYPE_PLAYLIST_VIDEO;
		dnldList.push_back(newelem);
	}

	for (auto& mediaInfo : mediaInfoStore)
	{
		// Add Media uris ( Audio and WebVTT)
		if( !mediaInfo.uri.empty() )
		{
			PreCacheUrlStruct newelem;
			aamp_ResolveURL( newelem.url, aamp->GetManifestUrl(), mediaInfo.uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam) );
			newelem.type = ((mediaInfo.type==eMEDIATYPE_AUDIO)?eMEDIATYPE_PLAYLIST_AUDIO:eMEDIATYPE_PLAYLIST_SUBTITLE);
			dnldList.push_back(newelem);
		}
	}

	// Set the download list to PrivateInstance to download it
	aamp->SetPreCacheDownloadList(dnldList);

	try
	{
		aamp->mPreCachePlaylistThreadId = std::thread(&StreamAbstractionAAMP_HLS::CachePlaylistThreadFunction, this);
		AAMPLOG_INFO("Thread created for CachePlaylistThreadFunction [%zx]", GetPrintableThreadID(aamp->mPreCachePlaylistThreadId));
	}
	catch(const std::exception& e)
	{
		AAMPLOG_ERR("Thread creation failed for PreCachePlaylist : %s", e.what());
	}
}


/**
 *  @brief Function to return first PTS
 */
double StreamAbstractionAAMP_HLS::GetFirstPTS()
{
	AampTime pts{};
	if(mStartTimestampZero)
	{
		// For CMAF assets, we employ isobmffprocessor to get the PTS value since its not
		// known from manifest. mFirstPTS will be populated only if platform has qtdemux override enabled.
		// We check for only video, since mFirstPTS is first video frame's PTS.
		if (trackState[eMEDIATYPE_VIDEO]->streamOutputFormat == FORMAT_ISO_BMFF && mFirstPTS != 0)
		{
			pts += mFirstPTS;
		}
		if(ISCONFIGSET(eAAMPConfig_MidFragmentSeek))
		{
			pts += midSeekPtsOffset;
		}
	}
	else
	{
		pts = seekPosition;
	}
	return pts.inSeconds();
}

/**
 * @brief Function to get the buffer duration of stream
 */
double StreamAbstractionAAMP_HLS::GetBufferedDuration()
{
	TrackState *video = trackState[eTRACK_VIDEO];
	AampTime retval{-1.0};
	if (video && video->enabled)
	{
		retval = video->GetBufferedDuration();
	}
	return retval.inSeconds();
}

/**
 * @brief Function to retune buffered duration
 */
double TrackState::GetBufferedDuration()
{
	return (playTargetBufferCalc.inSeconds() - (aamp->GetPositionMs() / 1000));
}


/**
 *  @brief Flushes out all old segments and sets up new playlist
 *         Used to switch subtitle tracks without restarting the pipeline
 */
void TrackState::SwitchSubtitleTrack()
{
	if (eTRACK_SUBTITLE == type && mSubtitleParser)
	{
		std::lock_guard<std::mutex> guard(mutex);

		AAMPLOG_INFO("Preparing to flush fragments and switch playlist");
		// Flush all counters, reset the playlist URL and refresh the playlist
		FlushFragments();
		aamp_ResolveURL(mPlaylistUrl, aamp->GetManifestUrl(), context->GetPlaylistURI(type).c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
		if(aamp->IsLive())
		{
			// Abort ongoing playlist download if any.
			aamp->DisableMediaDownloads(playlistMediaType);
			// Abort playlist timed wait for immediate download.
			AbortWaitForPlaylistDownload();
			// Notify that fragment collector is waiting
			NotifyFragmentCollectorWait();
			WaitForManifestUpdate();
		}
		else
		{
			// Download VOD playlist for new subtitle track without any wait
			PlaylistDownloader();
		}

		playTarget = 0.0;
		bool reloadUri = false;
		AcquirePlaylistLock();
		fragmentURI = GetNextFragmentUriFromPlaylist(reloadUri);
		ReleasePlaylistLock();
		context->AbortWaitForAudioTrackCatchup(true);

		mSubtitleParser->init(aamp->GetPositionSeconds(), aamp->GetBasePTS());
	}
}


/**
 * @brief Fragment collector thread execution function to download fragments
 */
void TrackState::RunFetchLoop()
{
	bool skipFetchFragment = false;
	bool abortedDownload = false;

	for (;;)
	{
		while (!abortedDownload && (!fragmentURI.empty() || refreshAudio) && aamp->DownloadsAreEnabled())
		{
			skipFetchFragment = false;
			if(refreshAudio)
			{
				SwitchAudioTrack();
				refreshAudio = false;
				abort = false;
				if(fragmentURI.empty())
				{
					break;
				}
			}
			if (mInjectInitFragment)
			{
				// mInjectInitFragment marks if init fragment has to be pushed whereas mInitFragmentInfo
				// holds the init fragment URL. Both has to be present for init fragment fetch & injection to work.
				// During ABR, mInjectInitFragment is set and for live assets,  mInitFragmentInfo is found
				// in FindMediaForSequenceNumber() and for VOD its found in GetNextFragmentUriFromPlaylist()
				// which also sets mInjectInitFragment to true, so below reset will not have an impact
				if (!mInitFragmentInfo.empty())
				{
					FetchInitFragment();
					//Inject init fragment failed due to no free cache
					if (mInjectInitFragment)
					{
						skipFetchFragment = true;
					}
					else
					{
						skipFetchFragment = false;
					}
				}
				else
				{
					mInjectInitFragment = false;
				}
			}

			if (!skipFetchFragment)
			{
				FetchFragment();
			}

			// FetchFragment involves multiple wait operations, so check download status again
			if (!aamp->DownloadsAreEnabled())
			{
				break;
			}

			/*Check for profile change only for video track*/
			// Avoid ABR if we have seen or just pushed an init fragment
			if((eTRACK_VIDEO == type) && (!context->trickplayMode) && !(mInjectInitFragment || mSkipAbr))
			{
				// if rampdown is attempted to any failure , no abr change to be attempted .
				// else profile be reset to top one leading to looping of bad fragment
				if(!mCheckForRampdown)
				{
					if (aamp->CheckABREnabled())
					{
						context->CheckForProfileChange();
					}
				}
			}

			// This will switch the subtitle track without restarting AV
			// Should be a smooth transition to new language
			if (refreshSubtitles)
			{
				// Reset abort flag (this was set to exit the fetch loop)
				abort = false;
				refreshSubtitles = false;
				SwitchSubtitleTrack();
			}

			{
				std::lock_guard<std::mutex> guard(mutex);
				if(refreshPlaylist)
				{
					// Refresh playlist for ABR
					AAMPLOG_INFO("Refreshing '%s' playlist for ABR", name);
					if(aamp->IsLive())
					{
						// Abort ongoing playlist download if any.
						aamp->DisableMediaDownloads(playlistMediaType);
						// Abort playlist timed wait for immediate download.
						AbortWaitForPlaylistDownload();
						// Notify that fragment collector is waiting
						NotifyFragmentCollectorWait();
						WaitForManifestUpdate();
					}
					else
					{
						// Download VOD playlist for new profile
						PlaylistDownloader();
					}
					refreshPlaylist = false;
					isFirstFragmentAfterABR = true;
				}
			}
		}
		// reached end of vod stream
		//teststreamer_EndOfStreamReached();
		if(!abortedDownload && context->aamp->IsFogTSBSupported() && eosReached)
		{
			AbortWaitForCachedAndFreeFragment(false);
			/* Make the aborted variable to true to avoid
			* further fragment fetch loop running and abort sending multiple time */
			abortedDownload = true;
		}
		else if ((eosReached && !context->aamp->IsFogTSBSupported()) || mReachedEndListTag || !context->aamp->DownloadsAreEnabled())
		{
			/* Check whether already aborted or not */
			if(!abortedDownload)
			{
				AbortWaitForCachedAndFreeFragment(false);
			}
			break;
		}

		// wait for manifest refresh on EOS case
		if(!abortedDownload && aamp->IsLive())
		{
			// Notify that fragment collector is waiting
			AAMPLOG_INFO("EOS wait for playlist refresh");
			NotifyFragmentCollectorWait();
			WaitForManifestUpdate();
		}

		if( ISCONFIGSET(eAAMPConfig_FailoverLogging) )
		{
			AAMPLOG_WARN("fragmentURI [%.*s] timeElapsedSinceLastFragment [%f]",
						 fragmentURI.getLen(), fragmentURI.getPtr(), (aamp_GetCurrentTimeMS() - context->LastVideoFragParsedTimeMS()));
		}
		/* Added to handle an edge case for cdn failover, where we found valid sub-manifest but no valid fragments.
		 * In this case we have to stall the playback here. */
		if( fragmentURI.empty() && IsLive() && type == eTRACK_VIDEO)
		{
			if( ISCONFIGSET(eAAMPConfig_FailoverLogging) )
			{
				AAMPLOG_WARN("fragmentURI is NULL, playback may stall in few seconds..");
			}
			context->CheckForPlaybackStall(false);
		}
	}
	AAMPLOG_WARN("fragment collector done. track %s", name);
}


/***************************************************************************
* @fn FragmentCollector
* @brief Fragment collector thread function

* @return void
***************************************************************************/
void TrackState::FragmentCollector(void)
{
	aamp_setThreadName("aampHLSFetcher");
	UsingPlayerId player(aamp->mPlayerId);
	RunFetchLoop();
	return;
}

/**
 * @brief Constructor function
 */
StreamAbstractionAAMP_HLS::StreamAbstractionAAMP_HLS(class PrivateInstanceAAMP *aamp,double seekpos, float rate,
	id3_callback_t id3Handler,
	ptsoffset_update_t ptsUpdate)
: StreamAbstractionAAMP(aamp, id3Handler),
	rate(rate), maxIntervalBtwPlaylistUpdateMs(DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS), mainManifest("mainManifest"), allowsCache(false), seekPosition(seekpos), mTrickPlayFPS(),
	enableThrottle(false), firstFragmentDecrypted(false), mStartTimestampZero(false), mNumberOfTracks(0), midSeekPtsOffset(0),
	segDLFailCount(0), segDrmDecryptFailCount(0), mMediaCount(0),mProfileCount(0),
	mLangList(),mIframeAvailable(false), thumbnailManifest("thumbnailManifest"), indexedTileInfo(),
	mFirstPTS(0),mDiscoCheckMutex(),
	mPtsOffsetUpdate{ptsUpdate},
	mDrmInterface(aamp),
	mMetadataProcessor{nullptr}
{
	if (aamp->mDRMLicenseManager)
	{
		AampDRMLicenseManager *licenseManager = aamp->mDRMLicenseManager;
		licenseManager->SetLicenseFetcher(this);
	}
	trickplayMode = false;
	enableThrottle = ISCONFIGSET(eAAMPConfig_Throttle);
	AAMPLOG_WARN("hls fragment collector seekpos = %f", seekpos);
	if (rate == AAMP_NORMAL_PLAY_RATE)
	{
		this->trickplayMode = false;
	}
	else
	{
		this->trickplayMode = true;
	}
	streamInfoStore.reserve(initial_stream_info_store_capacity);
	mediaInfoStore.reserve(initial_media_info_store_capacity);
	//targetDurationSeconds = 0.0;
	aamp->mhAbrManager.clearProfiles();
	aamp->CurlInit(eCURLINSTANCE_VIDEO, DEFAULT_CURL_INSTANCE_COUNT,aamp->GetNetworkProxy());
	// Initializing curl instances for playlists.
	aamp->CurlInit(eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO, AAMP_TRACK_COUNT, aamp->GetNetworkProxy());
}


/**
 * @brief TrackState Constructor
 */
TrackState::TrackState(TrackType type, StreamAbstractionAAMP_HLS* parent, PrivateInstanceAAMP* aamp, const char* name,
			id3_callback_t id3Handler,
			ptsoffset_update_t ptsUpdate
		) :
		MediaTrack(type, aamp, name),
		currentIdx(0), indexFirstMediaSequenceNumber(0), fragmentURI(), lastPlaylistDownloadTimeMS(0), lastPlaylistIndexedTimeMS(0),
		byteRangeLength(0), byteRangeOffset(0), nextMediaSequenceNumber(0), playlistPosition(0), playTarget(0),playTargetBufferCalc(0),playlistCulledOffset(0),
		lastDownloadedIFrameTarget(-1),
		streamOutputFormat(FORMAT_INVALID),
		playTargetOffset(0),
		discontinuity(false),
		refreshPlaylist(false), fragmentCollectorThreadID(), isFirstFragmentAfterABR(false),
		manifestDLFailCount(0),
		mCMSha1Hash(), mDrmTimeStamp(0), firstIndexDone(false), mDrm(NULL), mDrmLicenseRequestPending(false),
		mInjectInitFragment(false), mInitFragmentInfo(), mDrmKeyTagCount(0), mIndexingInProgress(false), mForceProcessDrmMetadata(false),
		mDuration(0), mLastMatchedDiscontPosition(-1), mCulledSeconds(0),mCulledSecondsOld(0),
		mEffectiveUrl(""), mPlaylistUrl(""), mFragmentURIFromIndex(),
		mSyncAfterDiscontinuityInProgress(false), playlist("playlist"),
		index(), targetDurationSeconds(1), mDeferredDrmKeyMaxTime(0), startTimeForPlaylistSync(0.0),
		context(parent), fragmentEncrypted(false), mKeyTagChanged(false), mIVKeyChanged(false), mLastKeyTagIdx(0), mDrmInfo(),
		mDrmMetaDataIndexPosition(0), mDrmMetaDataIndex(), mDiscontinuityIndex(), mKeyHashTable(), mPlaylistMutex(),
		mPlaylistIndexed(), mTrackDrmMutex(), mPlaylistType(ePLAYLISTTYPE_UNDEFINED), mReachedEndListTag(false),
		mByteOffsetCalculation(false),mSkipAbr(false),
		mCheckForInitialFragEnc(false), mFirstEncInitFragmentInfo(NULL), mDrmMethod(eDRM_KEY_METHOD_NONE)
		,mXStartTimeOFfset(0), mCulledSecondsAtStart(0.0)//, mCMCDNetworkMetrics{-1,-1,-1}
		,mProgramDateTime(0.0)
		,mSkipSegmentOnError(true)
		,playlistMediaType()
		,fragmentEncChange(false)
		,demuxOp(eStreamOp_NONE)
{
	playlist.Clear();
	index.clear();
	startTimeForPlaylistSync = 0.0;
	mDrmMetaDataIndex.clear();
	mDiscontinuityIndex.clear();
	mCulledSecondsAtStart = aamp->culledSeconds;
	mProgramDateTime = aamp->mProgramDateTime;
	AAMPLOG_INFO("Restore PDT (%f) ",mProgramDateTime.inSeconds());
	playlistMediaType = GetPlaylistMediaTypeFromTrack(type, IS_FOR_IFRAME(aamp->rate,type));
	mDrmInterface  = DrmInterface::GetInstance(aamp);
}


/**
 *  @brief Destructor function
 */
TrackState::~TrackState()
{
	playlist.Free();
	// We could remove this. This is already done in MediaTrack destructor
	int maxCachedFragmentsPerTrack = GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached);
	for (int j=0; j< maxCachedFragmentsPerTrack; j++)
	{
		mCachedFragment[j].fragment.Free();
	}
	FlushIndex();
	memset( mDrmInfo.iv, 0, sizeof(mDrmInfo.iv) );
}


/**
 * @brief Function to stop track download/playback
 */
void TrackState::Stop(bool clearDRM)
{
	AbortWaitForCachedAndFreeFragment(true);

	if (playContext)
	{
		playContext->abort();
	}
	if(aamp->IsLive())
	{
		StopPlaylistDownloaderThread();
	}
	if (fragmentCollectorThreadID.joinable())
	{
		fragmentCollectorThreadID.join();
	}

	aamp->StopTrackInjection((AampMediaType) type);
	StopInjectLoop();

	//To be called after StopInjectLoop to avoid cues to be injected after cleanup
	if (mSubtitleParser)
	{
		mSubtitleParser->reset();
		mSubtitleParser->close();
	}

	// While waiting on fragmentCollectorThread to join the mDrm
	// can get initialized in fragmentCollectorThread.
	// Clear DRM data after join if this is required.
	if(mDrm && clearDRM)
	{
		mDrm->Release();
	}
}


/**
 * @brief Destructor function for StreamAbstractionAAMP_HLS
 */
StreamAbstractionAAMP_HLS::~StreamAbstractionAAMP_HLS()
{
	/*Exit from ongoing  http fetch, drm operation,throttle. Mark fragment collector exit*/

	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		TrackState *track = trackState[i];
		SAFE_DELETE(track);
	}

	aamp->SyncBegin();
	this->thumbnailManifest.Free();
	this->mainManifest.Free();
	aamp->CurlTerm(eCURLINSTANCE_VIDEO, DEFAULT_CURL_INSTANCE_COUNT);
	aamp->CurlTerm(eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO, AAMP_TRACK_COUNT);
	aamp->SyncEnd();
}

/**
 * @brief Function to create threads for track download
 */
void TrackState::Start(void)
{
	if(playContext)
	{
		playContext->reset();
	}

	if(aamp->IsLive())
	{
		StartPlaylistDownloaderThread();
	}

	try
	{
    	// Attempting to assign to a running thread will cause std::terminate(), not an exception
		if(!fragmentCollectorThreadID.joinable())
		{
			fragmentCollectorThreadID = std::thread(&TrackState::FragmentCollector, this);
			AAMPLOG_INFO("Thread created for FragmentCollector [%zx]", GetPrintableThreadID(fragmentCollectorThreadID));
		}
		else
		{
			AAMPLOG_WARN("FragmentCollector thread already running, not creating a new one");
		}
	}
	catch(const std::exception& e)
	{
		AAMPLOG_WARN("Failed to create FragmentCollector thread : %s", e.what());
	}

	if(aamp->IsPlayEnabled())
	{
		aamp->ResumeTrackInjection((AampMediaType) type);
		StartInjectLoop();
	}
}


/**
 * @brief Function to start track initialization
 */
void StreamAbstractionAAMP_HLS::Start(void)
{
	aamp->mDRMLicenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_ACTIVE);
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track->Enabled())
		{
			track->Start();
		}
	}
}


/**
 *  @brief Function to stop the HLS streaming
 *         Function to handle stop processing of all tracks within stream
 */
void StreamAbstractionAAMP_HLS::Stop(bool clearChannelData)
{
	aamp->DisableDownloads();
	ReassessAndResumeAudioTrack(true);
	AbortWaitForAudioTrackCatchup(false);

	//This is purposefully kept in a separate loop to avoid being hung
	//on pthread_join of fragmentCollectorThread
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			track->CancelDrmOperation(clearChannelData);
		}
	}

	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];

		/*Stop any waits for other track's playlist update*/
		TrackState *otherTrack = trackState[(iTrack == eTRACK_VIDEO)? eTRACK_AUDIO: eTRACK_VIDEO];
		if(otherTrack && otherTrack->Enabled())
		{
			otherTrack->StopWaitForPlaylistRefresh();
		}

		if(track && track->Enabled())
		{
			track->Stop(clearChannelData);
			if (!clearChannelData)
			{
				//Restore drm key state which was reset by drm_CancelKeyWait earlier since drm data is persisted
				track->RestoreDrmState();
			}
		}
	}

	if (clearChannelData)
	{
		if (aamp->GetCurrentDRM() != nullptr)
		{
			aamp->GetCurrentDRM()->cancelDrmSession();
		}
		if(aamp->fragmentCdmEncrypted)
		{
			// check for WV and PR , if anything to be flushed
			StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);
			if (sink)
			{
				sink->ClearProtectionEvent();
			}
		}
		if(ISCONFIGSET(eAAMPConfig_UseSecManager) || ISCONFIGSET(eAAMPConfig_UseFireboltSDK))
		{
			aamp->mDRMLicenseManager->notifyCleanup();
		}
		aamp->mDRMLicenseManager->setSessionMgrState(SessionMgrState::eSESSIONMGR_INACTIVE);
	}
	if(!clearChannelData)
	{
		aamp->EnableDownloads();
	}
}

/***************************************************************************
* @brief Function to get stream format
***************************************************************************/
void StreamAbstractionAAMP_HLS::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subOutputFormat)
{
	primaryOutputFormat = trackState[eMEDIATYPE_VIDEO]->streamOutputFormat;
	audioOutputFormat = trackState[eMEDIATYPE_AUDIO]->streamOutputFormat;
	auxOutputFormat = trackState[eMEDIATYPE_AUX_AUDIO]->streamOutputFormat;
	subOutputFormat = trackState[eMEDIATYPE_SUBTITLE]->streamOutputFormat;
}
/***************************************************************************
* @brief Function to get available video bitrates
***************************************************************************/
std::vector<BitsPerSecond> StreamAbstractionAAMP_HLS::GetVideoBitrates(void)
{
	std::vector<BitsPerSecond> bitrates;
	bitrates.reserve(GetProfileCount());
	if (mProfileCount)
	{
		for (auto& streamInfo : streamInfoStore)
		{
			//Not send iframe bw info, since AAMP has ABR disabled for trickmode
			if (!streamInfo.isIframeTrack && streamInfo.enabled)
			{
				bitrates.push_back(streamInfo.bandwidthBitsPerSecond);
			}
		}
	}
	return bitrates;
}


/***************************************************************************
* @fn isThumbnailStream
* @brief Function to check if the provided stream is a thumbnail stream
*
* @return bool true on success
***************************************************************************/
static bool isThumbnailStream( const HlsStreamInfo &streamInfo )
{
	lstring lptr = lstring(streamInfo.codecs.c_str(),4);
	return lptr.SubStringMatch("jpeg");
}

/**
 * @brief Function to get available thumbnail tracks
 */
std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableThumbnailTracks(void)
{
	std::vector<StreamInfo*> thumbnailTracks;
	for (auto& streamInfo : streamInfoStore)
	{
		if( streamInfo.isIframeTrack && isThumbnailStream(streamInfo) )
		{
			struct StreamInfo *sptr = &streamInfo;
			thumbnailTracks.push_back(sptr);
		}

	}
	return thumbnailTracks;
}

/***************************************************************************
* @fn IndexThumbnails
* @brief Function to index thumbnail manifest.
*
* @param *ptr pointer to thumbnail manifest
* @return Updated vector of available thumbnail tracks.
***************************************************************************/
std::vector<TileInfo> IndexThumbnails( lstring iter , double stTime=0 )
{
	std::vector<TileInfo> rc;
	AampTime startTime = stTime;
	TileLayout layout;
	memset( &layout, 0, sizeof(layout) );

	layout.numRows = DEFAULT_THUMBNAIL_TILE_ROWS;
	layout.numCols = DEFAULT_THUMBNAIL_TILE_COLUMNS;

	while(!iter.empty())
	{
		lstring ptr = iter.mystrpbrk();
		if(!ptr.empty())
		{
			if (ptr.removePrefix("#EXT"))
			{
				if (ptr.removePrefix("INF:"))
				{
					layout.tileSetDuration = ptr.atof();
				}
				else if (ptr.removePrefix("-X-TILES:"))
				{
					ptr.ParseAttrList(ParseTileInfCallback, &layout);
				}
			}
			else if( !ptr.startswith('#') )
			{
				TileInfo tileInfo;
				if( 0.0f == layout.posterDuration )
				{
					if( layout.tileSetDuration )
					{
						layout.posterDuration = layout.tileSetDuration;
					}
					else
					{
						layout.posterDuration = DEFAULT_THUMBNAIL_TILE_DURATION;
					}
				}
				tileInfo.layout = layout;
				tileInfo.url = ptr.tostring();
				tileInfo.startTime = startTime.inSeconds();
				startTime += layout.tileSetDuration;
				rc.push_back( tileInfo );
			}
		}
	}
	if(rc.empty() )
	{
		AAMPLOG_WARN("IndexThumbnails failed");
	}
	return rc;
}


/**
 * @brief Function to set thumbnail track for processing
 */
bool StreamAbstractionAAMP_HLS::SetThumbnailTrack( int thumbIndex )
{
	bool rc = false;
	indexedTileInfo.clear();
	thumbnailManifest.Free();
	int iProfile{};

	for (auto& streamInfo : streamInfoStore)
	{
		if( streamInfo.isIframeTrack && isThumbnailStream(streamInfo) )
		{
			if( thumbIndex>0 )
			{
				thumbIndex--;
			}
			else
			{
				aamp->mthumbIndexValue = iProfile;

				std::string url;
				aamp_ResolveURL(url, aamp->GetManifestUrl(), streamInfo.uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
				int http_error = 0;
				AampTime downloadTime{};
				std::string tempEffectiveUrl;
				double tempDownloadTime;
				if( aamp->GetFile(url, eMEDIATYPE_PLAYLIST_IFRAME, &thumbnailManifest, tempEffectiveUrl, &http_error, &tempDownloadTime, NULL, eCURLINSTANCE_MANIFEST_MAIN,true) )
				{
					downloadTime = tempDownloadTime;
					AAMPLOG_WARN("In StreamAbstractionAAMP_HLS: Configured Thumbnail");
					ContentType type = aamp->GetContentType();
					if( ContentType_LINEAR == type  || ContentType_SLE == type )
					{
						if( aamp->getAampCacheHandler()->IsPlaylistUrlCached(streamInfo.uri) )
						{
							aamp->getAampCacheHandler()->RemoveFromPlaylistCache(streamInfo.uri);
						}
						rc=true;
					}
					aamp->getAampCacheHandler()->InsertToPlaylistCache(streamInfo.uri, &thumbnailManifest, tempEffectiveUrl,false,eMEDIATYPE_PLAYLIST_IFRAME);
					if( ContentType_SLE != type && ContentType_LINEAR != type )
					{
						lstring iter = lstring(thumbnailManifest.GetPtr(), thumbnailManifest.GetLen());
						indexedTileInfo = IndexThumbnails( iter );
						rc = !indexedTileInfo.empty();
					}
					if( !rc )
					{
						AAMPLOG_WARN("Thumbnail index failed");
					}
				}
				else
				{
					AAMPLOG_WARN("In StreamAbstractionAAMP_HLS: Unable to fetch the Thumbnail Manifest");
				}
				break;
			}
		}
		iProfile++;
	}
	return rc;
}


/**
 * @brief Function to fetch the thumbnail data.
 */
std::vector<ThumbnailData> StreamAbstractionAAMP_HLS::GetThumbnailRangeData(double tStart, double tEnd, std::string *baseurl, int *raw_w, int *raw_h, int *width, int *height)
{
	std::vector<ThumbnailData> data{};
	HlsStreamInfo &streamInfo = streamInfoStore[aamp->mthumbIndexValue];
	ContentType type = aamp->GetContentType();
	if(!thumbnailManifest.GetPtr() || ( type == ContentType_SLE || type == ContentType_LINEAR ) )
	{
		thumbnailManifest.Free();
		std::string tmpurl;
		if(aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(streamInfo.uri, &thumbnailManifest, tmpurl,eMEDIATYPE_PLAYLIST_IFRAME))
		{
			lstring iter = lstring(thumbnailManifest.GetPtr(),thumbnailManifest.GetLen());
			indexedTileInfo = IndexThumbnails( iter, tStart );
		}
		else
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_HLS: Failed to retrieve the thumbnail playlist from cache.");
		}
	}

	ThumbnailData tmpdata{};
	AampTime totalSetDuration{};
	for( TileInfo &tileInfo : indexedTileInfo )
	{
		tmpdata.t = tileInfo.startTime;
		if( tmpdata.t > tEnd )
		{ // done
			break;
		}
		AampTime tileSetEndTime{tmpdata.t + tileInfo.layout.tileSetDuration};
		totalSetDuration += tileInfo.layout.tileSetDuration;
		if( tileSetEndTime < tStart )
		{ // skip over
			continue;
		}
		tmpdata.url = tileInfo.url;
		*raw_w = streamInfo.resolution.width * tileInfo.layout.numCols;
		*raw_h = streamInfo.resolution.height * tileInfo.layout.numRows;
		tmpdata.d = tileInfo.layout.posterDuration;
		bool done{false};
		for( int row=0; row<tileInfo.layout.numRows && !done; row++ )
		{
			for( int col=0; col<tileInfo.layout.numCols && !done; col++ )
			{
				AampTime tNext{tmpdata.t+tileInfo.layout.posterDuration};
				if( tNext >= tileSetEndTime )
				{ // clamp & bail
					tmpdata.d = tileSetEndTime.inSeconds() - tmpdata.t;
					done = true;
				}
				if( tEnd >= tmpdata.t && tStart < tNext  )
				{
					tmpdata.x = col * streamInfo.resolution.width;
					tmpdata.y = row * streamInfo.resolution.height;
					data.push_back(tmpdata);
				}
				tmpdata.t = tNext.inSeconds();
			}
		}

		std::string url;
		aamp_ResolveURL(url, aamp->GetManifestUrl(), streamInfo.uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
		*baseurl = url.substr(0,url.find_last_of("/\\")+1);
	}
	*width = streamInfo.resolution.width;
	*height = streamInfo.resolution.height;
	return data;
}

/**
 *  @brief Function to notify first video pts value from tsprocessor/demux
 *         Kept public as its called from outside StreamAbstraction class
 */
void StreamAbstractionAAMP_HLS::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale)
{
	mFirstPTS = ((double)pts / (double)timeScale);
	StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(aamp);
	if (sink)
	{
		// The pts_offset is expected to be in seconds for RialtoSink, so we convert it to GstClockTime (nanoseconds).
				// For non-Rialto sinks, we need to convert the pts_offset to milliseconds to maintain consistency.
		sink->SetSubtitlePtsOffset(mFirstPTS.inSeconds());
	}
}

/**
 * @brief Signal start of subtitle rendering - should be sent at start of video presentation
 */
void StreamAbstractionAAMP_HLS::StartSubtitleParser()
{
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("sending init isLive : %d firstPTS : %.3f seek_pos :%f ",aamp->IsLive(), mFirstPTS.inSeconds() * 1000.0,aamp->seek_pos_seconds);
		if( ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) || aamp->IsLive())
		{
			subtitle->mSubtitleParser->init( mFirstPTS.inSeconds(),0);
		}
		else
		{
			subtitle->mSubtitleParser->init(seekPosition.inSeconds(),mFirstPTS.milliseconds());
		}
		subtitle->mSubtitleParser->mute(aamp->subtitles_muted);
	}
}

/**
 * @brief Set subtitle pause state
 */
void StreamAbstractionAAMP_HLS::PauseSubtitleParser(bool pause)
{
	TrackState *subtitle = trackState[eMEDIATYPE_SUBTITLE];
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("setting subtitles pause state = %d", pause);
		subtitle->mSubtitleParser->pause(pause);
	}
}


const std::unique_ptr<aamp::MetadataProcessorIntf> & StreamAbstractionAAMP_HLS::GetMetadataProcessor(StreamOutputFormat fmt)
{
	const std::lock_guard<std::mutex> lock(mMP_mutex);
	if (!mMetadataProcessor)
	{
		AAMPLOG_INFO(" Creating MetadataProcessor - %d", static_cast<int16_t>(fmt));

		if (fmt == FORMAT_MPEGTS)
		{
			auto video_processor = std::make_shared<TSProcessor>(aamp, eStreamOp_DEMUX_ALL, mID3Handler, eMEDIATYPE_DSM_CC);
			mMetadataProcessor = aamp_utils::make_unique<aamp::TSMetadataProcessor>(mID3Handler, mPtsOffsetUpdate, std::move(video_processor));
		}
		else if (fmt == FORMAT_ISO_BMFF)
		{
			auto video_processor = std::dynamic_pointer_cast<IsoBmffProcessor>(GetMediaTrack(eTRACK_VIDEO)->playContext);
			if (video_processor)
			{
				mMetadataProcessor = aamp_utils::make_unique<aamp::IsoBMFFMetadataProcessor>(mID3Handler, mPtsOffsetUpdate, video_processor);
			}
			else
			{
				AAMPLOG_WARN(" There is no video processor track.");
			}
		}
		else
		{
			AAMPLOG_WARN(" Undefined format: %d", static_cast<int16_t>(fmt));
		}
	}

	return mMetadataProcessor;
}


/**
 * @brief Function to decrypt the fragment for playback
 */
DrmReturn TrackState::DrmDecrypt( CachedFragment * cachedFragment, ProfilerBucketType bucketTypeFragmentDecrypt)
{
	DrmReturn drmReturn = eDRM_ERROR;
	{
		std::lock_guard<std::mutex> guard(mTrackDrmMutex);
		if (aamp->DownloadsAreEnabled())
		{
			// Update the DRM Context , if current active Drm Session is not received (mDrm)
			// or if Key Tag changed ( either with hash change )
			// For DAI scenario-> Clear to Encrypted or Encrypted to Clear can happen.
			//      For Encrypted to Clear, don't call SetDrmContext
			//      For Clear to Encrypted, SetDrmContext is called.
			if (fragmentEncrypted && (!mDrm || mKeyTagChanged || mIVKeyChanged))
			{
				SetDrmContext();
				mKeyTagChanged = false;
				mIVKeyChanged = false;
			}
			if(mDrm)
			{
				drmReturn = mDrm->Decrypt(bucketTypeFragmentDecrypt, cachedFragment->fragment.GetPtr(),
										  cachedFragment->fragment.GetLen(), MAX_LICENSE_ACQ_WAIT_TIME);

			}
		}
	}
	if (drmReturn != eDRM_SUCCESS)
	{
		aamp->profiler.ProfileError(bucketTypeFragmentDecrypt, drmReturn);
	}
	return drmReturn;
}

/**
 * @brief Function to create init vector using current media sequence number
 */
void TrackState::CreateInitVectorByMediaSeqNo ( long long seqNo )
{
	/* From RFC8216 - Section 5.2,
	 * Keeping the Media Sequence Number's big-endian binary
	 * representation into a 16-octet (128-bit) buffer and padding
	 * (on the left) with zeros.
	 * Eg: Assume Media Seq No is 0x00045d23, then IV data will hold
	 * value : 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x04,0x5d,0x23.
	 */
	int idx = DRM_IV_LEN;
	while( idx>0 )
	{
		mDrmInfo.iv[--idx] = seqNo&0xff;
		seqNo >>= 8;
	}
}

/**
 * @brief Function to get current StreamAbstractionAAMP instance value
 */
StreamAbstractionAAMP* TrackState::GetContext()
{
	return context;
}

/**
 * @brief Function to get Media information for track type
 */
MediaTrack* StreamAbstractionAAMP_HLS::GetMediaTrack(TrackType type)
{
	return trackState[(int)type];
}

/**
 * @brief Function to Update SHA1 Id for DRM Metadata
 */
void TrackState::UpdateDrmCMSha1Hash( const std::string &newSha1Hash )
{
	bool drmDataChanged = false;
	if( newSha1Hash.empty() )
	{
		mCMSha1Hash.clear();
	}
	else if( !mCMSha1Hash.empty() )
	{
		if( mCMSha1Hash != newSha1Hash )
		{
			if (!mIndexingInProgress)
			{
				AAMPLOG_MIL("[%s] Different DRM metadata hash. old=%s new=%s", name, mCMSha1Hash.c_str(), newSha1Hash.c_str() );
			}
			drmDataChanged = true;
			mCMSha1Hash = newSha1Hash;
		}
		else if (!mIndexingInProgress)
		{
			AAMPLOG_INFO("Same DRM Metadata");
		}
	}
	else
	{
		if (!mIndexingInProgress)
		{
			AAMPLOG_WARN("[%s] New DRM metadata hash - %s", name, newSha1Hash.c_str() );
		}
		mCMSha1Hash = newSha1Hash;
		drmDataChanged = true;
	}
	if(drmDataChanged)
	{
		size_t i = 0;
		while( i < mDrmMetaDataIndex.size() )
		{
			DrmMetadataNode &drmMetadataNode = mDrmMetaDataIndex[i];
			if( !drmMetadataNode.sha1Hash.empty() )
			{
				if( mCMSha1Hash == drmMetadataNode.sha1Hash )
				{
					if (!mIndexingInProgress)
					{
						AAMPLOG_INFO("mDrmMetaDataIndexPosition %d->%zu", mDrmMetaDataIndexPosition, i);
					}
					mDrmMetaDataIndexPosition = (int)i;
					break;
				}
			}
			i++;
		}
		if( i == mDrmMetaDataIndex.size() )
		{
			AAMPLOG_WARN("[%s] Couldn't find matching hash mDrmMetaDataIndexCount %zu", name, mDrmMetaDataIndex.size() );
			for( auto j = 0; j < mDrmMetaDataIndex.size(); j++ )
			{
				DrmMetadataNode &drmMetadataNode = mDrmMetaDataIndex[j];
				AAMPLOG_MIL("drmMetadataNode[%d].sha1Hash = %s", j, drmMetadataNode.sha1Hash.c_str() );
			}
			// use printf to avoid 2048 char syslog limitation
			printf("***playlist***:\n\n%.*s\n************\n", (int)playlist.GetLen(), playlist.GetPtr());
			assert(false);
		}
	}
}


/**
 * @brief Function to update IV from DRM
 */
void TrackState::UpdateDrmIV(const std::string &ptr)
{
	size_t len = 0;
	unsigned char *iv = base16_Decode(ptr.c_str(), (DRM_IV_LEN*2), &len); // 32 characters encoding 128 bits (16 bytes)
	if( iv )
	{
		assert(len == DRM_IV_LEN);
		if(0 != memcmp(mDrmInfo.iv, iv, DRM_IV_LEN))
		{
			memcpy( mDrmInfo.iv, iv, DRM_IV_LEN );
			mIVKeyChanged = true;
		}
		free( iv );
	}
	AAMPLOG_TRACE(" [%s] Exit mDrmInfo.iv %p", name, mDrmInfo.iv);
}


/**
 * @brief Function to fetch playlist file
 */
void TrackState::FetchPlaylist()
{
	int http_error = 0;   //CID:81884 - Initialization
	AampTime downloadTime{};
	int  main_error = 0;

	ProfilerBucketType bucketId = PROFILE_BUCKET_PLAYLIST_VIDEO; //type == eTRACK_VIDEO, eTRACK_AUDIO,...
	AampMediaType mType = eMEDIATYPE_PLAYLIST_VIDEO;

	if (type == eTRACK_AUDIO)
	{
		bucketId = PROFILE_BUCKET_PLAYLIST_AUDIO;
		mType = eMEDIATYPE_PLAYLIST_AUDIO;
	}
	else if (type == eTRACK_SUBTITLE)
	{
		bucketId = PROFILE_BUCKET_PLAYLIST_SUBTITLE;
		mType = eMEDIATYPE_PLAYLIST_SUBTITLE;
	}
	else if (type == eTRACK_AUX_AUDIO)
	{
		bucketId = PROFILE_BUCKET_PLAYLIST_AUXILIARY;
		mType = eMEDIATYPE_PLAYLIST_AUX_AUDIO;
	}

	int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
	AampCurlInstance dnldCurlInstance = aamp->GetPlaylistCurlInstance(mType , true);
	aamp->SetCurlTimeout(aamp->mPlaylistTimeoutMs,dnldCurlInstance);
	aamp->profiler.ProfileBegin(bucketId);

	double tempDownloadTime{};
	(void) aamp->GetFile(mPlaylistUrl, mType, &playlist, mEffectiveUrl, &http_error, &tempDownloadTime, NULL, (unsigned int)dnldCurlInstance, true );
	downloadTime = tempDownloadTime;
	// update videoend info
	main_error = context->getOriginalCurlError(http_error);

	ManifestData manifestData(downloadTime.milliseconds(), playlist.GetLen());
	aamp->UpdateVideoEndMetrics((IS_FOR_IFRAME(iCurrentRate, this->type) ? eMEDIATYPE_PLAYLIST_IFRAME : mType), this->GetCurrentBandWidth(),
								main_error, mEffectiveUrl, downloadTime.inSeconds(), &manifestData);
	if (playlist.GetLen())
		aamp->profiler.ProfileEnd(bucketId);

	aamp->SetCurlTimeout(aamp->mNetworkTimeoutMs,dnldCurlInstance);
	if (!playlist.GetLen())
	{
		AAMPLOG_WARN("Playlist download failed : %s  http response : %d", mPlaylistUrl.c_str(), http_error);
		aamp->mPlaylistFetchFailError = http_error;
		aamp->profiler.ProfileError(bucketId, main_error);
		aamp->profiler.ProfileEnd(bucketId);
	}

}

/**
 * @brief Function to get bandwidth index corresponding to bitrate
 */
int StreamAbstractionAAMP_HLS::GetBWIndex(BitsPerSecond bitrate)
{
	int topBWIndex = 0;
	if (mProfileCount)
	{
		for (auto& streamInfo : streamInfoStore)
		{
			if (!streamInfo.isIframeTrack && streamInfo.enabled && streamInfo.bandwidthBitsPerSecond > bitrate)
			{
				--topBWIndex;
			}
		}
	}
	return topBWIndex;
}


/**
 * @brief Function to get next playback position from start, to handle discontinuity
 */
void TrackState::GetNextFragmentPeriodInfo(int &periodIdx, AampTime &offsetFromPeriodStart, int &fragmentIdx)
{
	const IndexNode *idxNode = NULL;
	periodIdx = -1;
	fragmentIdx = -1;
	offsetFromPeriodStart = 0;
	int idx;
	AampTime prevCompletionTimeSecondsFromStart{};
	assert(context->rate > 0);
	for (idx = 0; idx < index.size(); idx++)
	{
		const IndexNode &node = index[idx];
		if (node.completionTimeSecondsFromStart > playTarget)
		{
			AAMPLOG_WARN("(%s) Found node - rate %f completionTimeSecondsFromStart %f playTarget %f", name,
					context->rate, node.completionTimeSecondsFromStart.inSeconds(), playTarget.inSeconds());
			idxNode = &index[idx];
			break;
		}
		prevCompletionTimeSecondsFromStart = node.completionTimeSecondsFromStart;
	}
	if (idxNode)
	{
		if (idx > 0)
		{
			offsetFromPeriodStart = prevCompletionTimeSecondsFromStart;
			AampTime periodStartPosition{};
			for( auto i = 0; i < mDiscontinuityIndex.size(); i++ )
			{
				DiscontinuityIndexNode &discontinuity = mDiscontinuityIndex[i];
				AAMPLOG_TRACE("TrackState:: [%s] Loop periodItr %d idx %d first %d second %f",  name, i, idx, discontinuity.fragmentIdx, discontinuity.position.inSeconds());
				if (discontinuity.fragmentIdx > idx)
				{
					AAMPLOG_WARN("TrackState: [%s] Found periodItr %d idx %d first %d offsetFromPeriodStart %f",
							name, i, idx, discontinuity.fragmentIdx, periodStartPosition.inSeconds());

					fragmentIdx = discontinuity.fragmentIdx;
					break;
				}
				periodIdx = i;
				periodStartPosition = discontinuity.position;
			}
			offsetFromPeriodStart -= periodStartPosition;
		}
		AAMPLOG_WARN("TrackState: [%s] periodIdx %d offsetFromPeriodStart %f", name, periodIdx,
				offsetFromPeriodStart.inSeconds());
	}
	else
	{
		AAMPLOG_WARN("TrackState: [%s] idxNode NULL", name);
	}
}


/**
 * @brief Function to get Period start position for given period index,to handle discontinuity
 */
AampTime TrackState::GetPeriodStartPosition(int periodIdx)
{
	AampTime offset{};
	AAMPLOG_MIL("TrackState: [%s] periodIdx %d periodCount %zu", name, periodIdx, mDiscontinuityIndex.size() );
	if (periodIdx < mDiscontinuityIndex.size() )
	{
		int count = 0;
		for( auto i = 0; i < mDiscontinuityIndex.size(); i++ )
		{
			if (count == periodIdx)
			{ // FIXME
				offset = mDiscontinuityIndex[i].position;
				AAMPLOG_MIL("TrackState: [%s] offset %f periodCount %zu", name, offset.inSeconds(),
						mDiscontinuityIndex.size() );
				break;
			}
			else
			{
				count++;
			}
		}
	}
	else
	{
		AAMPLOG_WARN("TrackState: [%s] WARNING periodIdx %d periodCount %zu", name, periodIdx,
					 mDiscontinuityIndex.size() );
	}
	return offset;
}


/**
 * @brief Function to return number of periods stored in playlist
 */
int TrackState::GetNumberOfPeriods()
{
	return (int)mDiscontinuityIndex.size();
}


/**
 * @brief Check if discontinuity present around given position
 */
bool TrackState::HasDiscontinuityAroundPosition(AampTime position, bool useDiscontinuityDateTime, AampTime &diffBetweenDiscontinuities, AampTime playPosition, AampTime inputCulledSec, AampTime inputProgramDateTime,bool &isDiffChkReq)
{
	bool discontinuityFound = false;
	bool useProgramDateTimeIfAvailable = UseProgramDateTimeIfAvailable();
	AampTime discDiscardToleranceInSec{3 * targetDurationSeconds}; /* Used by discontinuity handling logic to ensure both tracks have discontinuity tag around same area */
	int playlistRefreshCount = 0;
	diffBetweenDiscontinuities = DBL_MAX;

	std::unique_lock<std::mutex> lock(mPlaylistMutex);

	while (aamp->DownloadsAreEnabled())
	{
		// No condition to check DiscontinuityCount.Possible that in next refresh it will be available,
		// Case where one discontinuity in one track ,but other track not having it
		AampTime deltaCulledSec{inputCulledSec - mCulledSeconds};
		bool foundmatchingdisc = false;
		for( auto i = 0; i < mDiscontinuityIndex.size(); i++ )
		{
			const DiscontinuityIndexNode &discontinuity = mDiscontinuityIndex[i];
			// Live is complicated lets finish that
			AampTime discdatetime{discontinuity.discontinuityPDT};

			if (IsLive())
			{
				AAMPLOG_WARN("[%s] Host loop %d mDiscontinuityIndexCount %zu discontinuity-pos %f mCulledSeconds %f playlistRefreshTime:%f discdatetime=%f",name, i,
															mDiscontinuityIndex.size(), discontinuity.position.inSeconds(), mCulledSeconds.inSeconds(), mProgramDateTime.inSeconds(), discdatetime.inSeconds());

				AAMPLOG_WARN("Visitor loop %d Input track position:%f useDateTime:%d CulledSeconds :%f playlistRefreshTime :%f DeltaCulledSec:%f", i,
																		position.inSeconds(), useDiscontinuityDateTime, inputCulledSec.inSeconds(), inputProgramDateTime.inSeconds(), deltaCulledSec.inSeconds());
			}
			// check if date and time for discontinuity tag exists
			if(useDiscontinuityDateTime && discdatetime != 0)
			{
				// unfortunately date and time of calling track is passed in position argument
				AAMPLOG_WARN("Comparing two disc date&time input pdt:%f pdt:%f diff:%f", position.inSeconds(), discdatetime.inSeconds(), fabs(discdatetime - position));
				if( fabs( discdatetime - position ) <= targetDurationSeconds )
				{
					foundmatchingdisc = true;
					diffBetweenDiscontinuities = discdatetime - position;
					AAMPLOG_WARN("[%s] Found the matching discontinuity with pdt at position:%f", name, position.inSeconds());
					break;
				}
			}
			else
			{
				// No PDT , now compare the position based on culled delta
				// Additional fragmentDuration is considered as rounding with decimal is missing the position when culled delta is same
				// Ignore millisecond accuracy
				AampTime tempLimit1 = (discontinuity.position - abs(deltaCulledSec) - targetDurationSeconds - 1.0);
				AampTime tempLimit2 = (discontinuity.position + abs(deltaCulledSec) + targetDurationSeconds + 1.0);
				int64_t limit1 = tempLimit1.seconds();
				int64_t limit2 = tempLimit2.seconds();
				// Due to increase in fragment duration and mismatch between audio and video,
				// Discontinuity pairing is missed
				// Example : input posn:2290 index[12] position:2293 deltaCulled:0.000000 limit1:2291 limit2:2295
				// As a workaround , adding a buffer of +/- 1sec around the limit check .
				// We have seen some of the playlist is missing PDT for either of the track and this is leading
				//to ignoring the discontinuity and eventually 30 seconds tick and playback stalled. To fix the issue we need
				//to check is either of the track is missing PDT for discontinuity, if not missing use  position else
				//playposition(e.g:85.6 seconds instead of epoc PDT time 1652300037)
				int64_t roundedPosn;

				if(discdatetime == 0)
				{
					roundedPosn = playPosition.nearestSecond();
					isDiffChkReq = false;
				}
				else
				{
					roundedPosn = position.nearestSecond();
				}

				AAMPLOG_WARN("Comparing position input posn:%" PRIi64 " index[%d] position:%d deltaCulled:%f limit1:%" PRIi64" limit2:%" PRIi64, roundedPosn, i, (int)(discontinuity.position.inSeconds()), deltaCulledSec.inSeconds(), limit1, limit2);
				if(roundedPosn >= limit1 && roundedPosn <= limit2 )
				{
					foundmatchingdisc = true;
					AAMPLOG_WARN("[%s] Found the matching discontinuity at position:%f for position:%f", name, discontinuity.position.inSeconds(), position.inSeconds());
					break;
				}
			}
		}

		// Now the worst part . Not found matching discontinuity.How long to wait ???
		if(!foundmatchingdisc)
		{
			AAMPLOG_WARN("##[%s] Discontinuity not found mDuration %f playPosition %f  playlistType %d useStartTime %d ",
										 name, mDuration.inSeconds(), playPosition.inSeconds(), (int)mPlaylistType, (int)useDiscontinuityDateTime);
			if (IsLive())
			{
				int maxPlaylistRefreshCount;
				bool liveNoTSB;
				if (aamp->IsFogTSBSupported() || aamp->IsInProgressCDVR())
				{
					maxPlaylistRefreshCount = MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_EVENT;
					liveNoTSB = false;
				}
				else
				{
					maxPlaylistRefreshCount = MAX_PLAYLIST_REFRESH_FOR_DISCONTINUITY_CHECK_LIVE;
					liveNoTSB = true;
				}
				// how long to wait?? Two ways to check .
				// 1. using Program Date and Time of playlist update .
				// 2. using the position and count of target duration.
				if(useProgramDateTimeIfAvailable)
				{
					// check if the track called have higher PDT or not
					// if my refresh time is higher to calling track playlist track by an extra target duration,no point in waiting
					if (mProgramDateTime >= inputProgramDateTime+targetDurationSeconds || playlistRefreshCount > maxPlaylistRefreshCount)
					{
						AAMPLOG_WARN("%s Discontinuity not found mProgramDateTime:%f > inputProgramDateTime:%f playlistRefreshCount:%d maxPlaylistRefreshCount:%d",name,
						mProgramDateTime.inSeconds(), inputProgramDateTime.inSeconds(), playlistRefreshCount, maxPlaylistRefreshCount);
						break;
					}
				}
				else
				{
					if(!((playlistRefreshCount < maxPlaylistRefreshCount) && (liveNoTSB || (mDuration < (playPosition + discDiscardToleranceInSec)))))
					{
						AAMPLOG_WARN("%s Discontinuity not found After playlistRefreshCount:%d",name,playlistRefreshCount);
						break;
					}
				}
				AAMPLOG_WARN("Wait for [%s] playlist update over for playlistRefreshCount %d", name, playlistRefreshCount);
				mPlaylistIndexed.wait(lock);
				playlistRefreshCount++;
			}
			else
			{
				break;
			}
		}
		else
		{
			discontinuityFound = true;
			break;
		}
	}
	return discontinuityFound;
}


/**
 * @brief Function to fetch init fragment
 */
void TrackState::FetchInitFragment()
{
	int timeoutMs = -1;

	if (IsLive())
	{
		timeoutMs = context->maxIntervalBtwPlaylistUpdateMs - (int) (aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
		if(timeoutMs < 0)
		{
			timeoutMs = 0;
		}
	}
	if (mInjectInitFragment && !mInitFragmentInfo.empty() )
	{
		if (!WaitForFreeFragmentAvailable(timeoutMs))
		{
			return;
		}

		int http_code = -1;
		bool forcePushEncryptedHeader = (!fragmentEncrypted && mCheckForInitialFragEnc);
		// Check if we have encrypted header successfully parsed to push ahead
		if (forcePushEncryptedHeader && mFirstEncInitFragmentInfo == NULL)
		{
			AAMPLOG_WARN("TrackState::[%s] first encrypted init-fragment is NULL! fragmentEncrypted-%d",  name, fragmentEncrypted);
			forcePushEncryptedHeader = false;
		}

		ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia((AampMediaType)type, true);
		aamp->profiler.ProfileBegin(bucketType);

		if(discontinuity )
		{
			setDiscontinuityState(true);
		}

		if(FetchInitFragmentHelper(http_code, forcePushEncryptedHeader))
		{
			aamp->profiler.ProfileEnd(bucketType);

			CachedFragment* cachedFragment = GetFetchBuffer(false);
			if (cachedFragment->fragment.GetPtr())
			{
				cachedFragment->duration = 0;
				cachedFragment->position = playTarget.inSeconds() - playTargetOffset.inSeconds();
				cachedFragment->discontinuity = discontinuity;
			}

			// If forcePushEncryptedHeader, don't reset the playTarget as the original init header has to be pushed next
			if (!forcePushEncryptedHeader)
			{
				mInjectInitFragment = false;
			}

			discontinuity = false; //reset discontinuity which has been set for init fragment now
			mSkipAbr = true; //Skip ABR, since last fragment cached is init fragment.
			mCheckForInitialFragEnc = false; //Push encrypted header is a one-time operation
			mFirstEncInitFragmentInfo = NULL; //reset init fragment, since encrypted header already pushed
			UpdateTSAfterFetch(true);
		}
		else if (type == eTRACK_VIDEO && aamp->CheckABREnabled() && !context->CheckForRampDownLimitReached())
		{
			// Attempt rampdown for init fragment to get playable profiles.
			// TODO: Remove profile if init fragment is not available from ABR.

			mFirstEncInitFragmentInfo = NULL; // need to reset the previous profile's first encrypted init fragment in case of init fragment rampdown.
			AAMPLOG_WARN("Reset mFirstEncInitFragmentInfo since rampdown for another profile");

			if (context->CheckForRampDownProfile(http_code))
			{
				AAMPLOG_INFO("Init fragment fetch failed, Successfully ramped down to lower profile");
				mCheckForRampdown = true;
			}
			else
			{
				// Failed to get init fragment from all attempted profiles
				if (aamp->DownloadsAreEnabled())
				{
					AAMPLOG_ERR("%s TrackState::Init fragment fetch failed", name);
					aamp->profiler.ProfileError(bucketType, http_code);
					abortWaitForVideoPTS();
					aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
				}
				context->mRampDownCount = 0;
			}
			AAMPLOG_WARN("Error while fetching init fragment:%s, failedCount:%d. decrementing profile", name, segDLFailCount);
		}
		else if (aamp->DownloadsAreEnabled())
		{
			int http_error = context->getOriginalCurlError(http_code);
			AAMPLOG_ERR("%s TrackState::Init fragment fetch failed", name);
			abortWaitForVideoPTS();
			aamp->profiler.ProfileError(bucketType, http_error);
			aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, http_code);
		}
	}
	else if (mInitFragmentInfo.empty())
	{
		AAMPLOG_ERR("TrackState::Need to push init fragment but fragment info is missing! mInjectInitFragment(%d)", mInjectInitFragment);
		mInjectInitFragment = false;
	}

}


/**
 * @brief Helper to fetch init fragment for fragmented mp4 format
 */
bool TrackState::FetchInitFragmentHelper(int &http_code, bool forcePushEncryptedHeader)
{
	bool ret = false;
	std::istringstream initFragmentUrlStream;

	AcquirePlaylistLock();
	// If the first init fragment is of a clear fragment, we push an encrypted fragment's
	// init data first to let qtdemux know we will need decryptor plugins
	AAMPLOG_TRACE("TrackState::[%s] fragmentEncrypted-%d mFirstEncInitFragmentInfo-%s", name, fragmentEncrypted, mFirstEncInitFragmentInfo);
	if (forcePushEncryptedHeader)
	{
		//Push encrypted fragment's init data first
		AAMPLOG_WARN("TrackState::[%s] first init-fragment is unencrypted.! Pushing encrypted init-header", name);
		initFragmentUrlStream = std::istringstream(std::string(mFirstEncInitFragmentInfo));
	}
	else
	{
		initFragmentUrlStream = std::istringstream(mInitFragmentInfo.tostring());
	}
	std::string line;
	std::getline(initFragmentUrlStream, line);
	ReleasePlaylistLock();
	if (!line.empty())
	{
		const char *range = NULL;
		char rangeStr[MAX_RANGE_STRING_CHARS];
		std::string uri;
		AAMPLOG_TRACE(" line %s", line.c_str());
		size_t uriTagStart = line.find("URI=");
		if (uriTagStart != std::string::npos)
		{
			std::string uriStart = line.substr(uriTagStart + 5);
			AAMPLOG_TRACE(" uriStart %s", uriStart.c_str());
			size_t uriTagEnd = uriStart.find("\"");
			if (uriTagEnd != std::string::npos)
			{
				AAMPLOG_TRACE(" uriTagEnd %d", (int) uriTagEnd);
				uri = uriStart.substr(0, uriTagEnd);
				AAMPLOG_TRACE(" uri %s", uri.c_str());
			}
			else
			{
				AAMPLOG_ERR("URI parse error. Tag end not found");
			}
		}
		else
		{
			AAMPLOG_ERR("URI parse error. URI= not found");
		}
		size_t byteRangeTagStart = line.find("BYTERANGE=");
		if (byteRangeTagStart != std::string::npos)
		{
			std::string byteRangeStart = line.substr(byteRangeTagStart + 11);
			size_t byteRangeTagEnd = byteRangeStart.find("\"");
			if (byteRangeTagEnd != std::string::npos)
			{
				std::string byteRange = byteRangeStart.substr(0, byteRangeTagEnd);
				AAMPLOG_TRACE(" byteRange %s", byteRange.c_str());
				if (!byteRange.empty())
				{
					size_t offsetIdx = byteRange.find("@");
					if (offsetIdx != std::string::npos)
					{
						int offsetVal = stoi(byteRange.substr(offsetIdx + 1));
						int rangeVal = stoi(byteRange.substr(0, offsetIdx));
						int next = offsetVal + rangeVal;
						snprintf(rangeStr, sizeof(rangeStr), "%d-%d", offsetVal, next - 1);
						AAMPLOG_INFO("TrackState::rangeStr %s", rangeStr);
						range = rangeStr;
					}
				}
			}
			else
			{
				AAMPLOG_ERR("TrackState::byteRange parse error. Tag end not found byteRangeStart %s",
						 byteRangeStart.c_str());
			}
		}
		if (!uri.empty())
		{
			getNextFetchRequestUri();
			std::string fragmentUrl;
			aamp_ResolveURL(fragmentUrl, mEffectiveUrl, uri.c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
			std::string tempEffectiveUrl;
			CachedFragment* cachedFragment = GetFetchBuffer(true);
			AAMPLOG_WARN("TrackState::[%s] init-fragment = %s", name, fragmentUrl.c_str());
			int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play

			AampMediaType actualType = eMEDIATYPE_INIT_VIDEO;
			if(IS_FOR_IFRAME(iCurrentRate,type))
			{
				actualType = eMEDIATYPE_INIT_IFRAME;
			}
			else if (eTRACK_AUDIO == type)
			{
				actualType = eMEDIATYPE_INIT_AUDIO;
			}
			else if (eTRACK_SUBTITLE == type)
			{
				actualType = eMEDIATYPE_INIT_SUBTITLE;
			}
			else if (eTRACK_AUX_AUDIO == type)
			{
				actualType = eMEDIATYPE_INIT_AUX_AUDIO;
			}

#ifdef CHECK_PERFORMANCE
			long long ts_start, ts_end;
			ts_start = aamp_GetCurrentTimeMS();
#endif /* CHECK_PERFORMANCE */
			bool fetched = aamp->getAampCacheHandler()->RetrieveFromInitFragmentCache(fragmentUrl, &cachedFragment->fragment, tempEffectiveUrl);

#ifdef CHECK_PERFORMANCE
			ts_end = aamp_GetCurrentTimeMS();
			if(fetched)
			AAMPLOG_TRACE("---------------CacheRead Time diff:%llu---------------" , ts_end-ts_start);
#endif /* CHECK_PERFORMANCE */

			if ( !fetched )
			{
				double tempDownloadTime{0.0};
				fetched = aamp->GetFile(fragmentUrl, actualType, &cachedFragment->fragment, tempEffectiveUrl, &http_code, &tempDownloadTime, range,
						type, false );
				AampTime downloadTime{tempDownloadTime};

#ifdef CHECK_PERFORMANCE
				if(fetched)
				AAMPLOG_TRACE("---------------CurlReq Time diff:%llu---------------" , downloadTime.seconds());
#endif /* CHECK_PERFORMANCE */

				int main_error = context->getOriginalCurlError(http_code);
				aamp->UpdateVideoEndMetrics(actualType, this->GetCurrentBandWidth(), main_error, mEffectiveUrl, downloadTime.inSeconds());

				if ( fetched )
				aamp->getAampCacheHandler()->InsertToInitFragCache ( fragmentUrl, &cachedFragment->fragment, tempEffectiveUrl, actualType);
			}
			if (!fetched)
			{
				AAMPLOG_ERR("TrackState::aamp_GetFile failed");
				cachedFragment->fragment.Free();
			}
			else
			{
				ret = true;
			}
		}
		else
		{
			AAMPLOG_ERR("TrackState::Could not parse init fragment URI. line %s", line.c_str());
		}
	}
	else
	{
		AAMPLOG_ERR("TrackState::Init fragment URI parse error");
	}
	return ret;
}

/**
 * @brief Function to stop fragment injection
 */
void StreamAbstractionAAMP_HLS::StopInjection(void)
{
	//invoked at times of discontinuity. Audio injection loop might have already exited here
	ReassessAndResumeAudioTrack(true);

	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			track->StopInjection();
		}
	}
}


/**
 * @brief Stop fragment injection
 */
void TrackState::StopInjection()
{
	AbortWaitForCachedFragment();
	aamp->StopTrackInjection((AampMediaType) type);
	if (playContext)
	{
		playContext->abort();
	}
	StopInjectLoop();
}


/**
 * @brief Function to start fragment injection
 */
void TrackState::StartInjection()
{
	AAMPLOG_INFO("StartInjection()");

	aamp->ResumeTrackInjection((AampMediaType) type);
	if (playContext)
	{
		playContext->reset();
	}
	StartInjectLoop();
}


/**
 * @brief starts fragment injection
 */
void StreamAbstractionAAMP_HLS::StartInjection(void)
{
	mTrackState = eDISCONTINUITY_FREE;
	for (int iTrack = 0; iTrack < AAMP_TRACK_COUNT; iTrack++)
	{
		TrackState *track = trackState[iTrack];
		if(track && track->Enabled())
		{
			track->StartInjection();
		}
	}
}

/**
 * @brief Stop wait for playlist refresh
 */
void TrackState::StopWaitForPlaylistRefresh()
{
	AAMPLOG_WARN("track [%s]", name);
	std::lock_guard<std::mutex> guard(mPlaylistMutex);
	mPlaylistIndexed.notify_one();
}

/**
 * @brief Cancel all DRM operations
 */
void TrackState::CancelDrmOperation(bool clearDRM)
{
	//Calling mDrm is required for AES encrypted assets which doesn't have AveDrmManager
	if (mDrm)
	{
		//To force release mTrackDrmMutex mutex held by drm_Decrypt in case of clearDRM
		mDrm->CancelKeyWait();
		if (clearDRM)
		{
			if ((aamp->GetCurrentDRM() == nullptr) || (!aamp->GetCurrentDRM()->canCancelDrmSession()))
			{
				std::lock_guard<std::mutex> guard(mTrackDrmMutex);
				mDrm->Release();
			}
		}
	}
}

/**
 * @brief Restore DRM states
 */
void TrackState::RestoreDrmState()
{
	if (mDrm)
	{
		mDrm->RestoreKeyState();
	}
}

/**
 *  @brief Function to search playlist for subscribed tags
 */
void TrackState::FindTimedMetadata(bool reportBulkMeta, bool bInitCall)
{
	AampTime totalDuration{};
	if (ISCONFIGSET(eAAMPConfig_EnableSubscribedTags) && (eTRACK_VIDEO == type))
	{
		lstring iter = lstring(playlist.GetPtr(),playlist.GetLen());
		if( !iter.empty() )
		{
			lstring ptr = iter.mystrpbrk();
			while (!ptr.empty() )
			{
				if(ptr.removePrefix("#EXT"))
				{
					if (ptr.removePrefix("INF:"))
					{
						totalDuration += ptr.atof();
					}
					for (int i = 0; i < aamp->subscribedTags.size(); i++)
					{
						const char* data = aamp->subscribedTags.at(i).data();
						if(ptr.removePrefix(data + 4)) // remove the TAG and only keep value(content) in PTR
						{
							ptr.removePrefix(); // skip the ":"
							size_t nb = ptr.length(); // (int)FindLineLength(ptr);
							AampTime tempPosition{mCulledSecondsAtStart + mCulledSeconds + totalDuration};
							uint64_t positionMilliseconds = tempPosition.milliseconds();
							//AAMPLOG_INFO("mCulledSecondsAtStart:%f mCulledSeconds :%f totalDuration: %f posnMs:%lld playposn:%lld",mCulledSecondsAtStart,mCulledSeconds,totalDuration,positionMilliseconds,aamp->GetPositionMs());
							//AAMPLOG_WARN("Found subscribedTag[%d]: @%f cull:%f Posn:%lld '%.*s'", i, totalDuration, mCulledSeconds, positionMilliseconds, nb, ptr);
							std::string temp = ptr.tostring();
							if(reportBulkMeta)
							{
								aamp->SaveTimedMetadata(positionMilliseconds, data, temp.c_str(), (int)nb);
							}
							else
							{
								aamp->ReportTimedMetadata(positionMilliseconds, data, temp.c_str(), (int)nb, bInitCall);
							}
							break;
						}
					}
				}
				ptr=iter.mystrpbrk();
			}
		}
	}
	AAMPLOG_TRACE(" Exit");
}

/**
 * @brief Function to select the audio track and update AudioProfileIndex
 */
void StreamAbstractionAAMP_HLS::ConfigureAudioTrack()
{
	currentAudioProfileIndex = -1;
	if(mMediaCount)
	{
		currentAudioProfileIndex = GetBestAudioTrackByLanguage();
	}
	AAMPLOG_WARN("Audio profileIndex selected :%d", currentAudioProfileIndex);
}

/**
 * @brief Check whether stream is 4K stream or not
 * @param[out] resolution of stream if 4K
 * @param[out] bandwidth of stream if 4K
 *
 * @return true or false
 */
bool StreamAbstractionAAMP_HLS::Is4KStream(int &height, BitsPerSecond &bandwidth)
{
	bool Stream4k = false;

	auto stream = std::find_if(streamInfoStore.begin(), streamInfoStore.end(), [](const HlsStreamInfo& stream) { return stream.resolution.height > AAMP_FHD_HEIGHT;});

	if (stream != streamInfoStore.end())
	{
		height = stream->resolution.height;
		bandwidth = stream->bandwidthBitsPerSecond;
		Stream4k = true;
		AAMPLOG_INFO("4K profile found resolution : %d*%d bandwidth %ld", stream->resolution.height, stream->resolution.width, stream->bandwidthBitsPerSecond);
	}
	return Stream4k;
}

/**
 * @brief Function to select the best match video profiles based on audio and filters
 */
void StreamAbstractionAAMP_HLS::ConfigureVideoProfiles()
{
	std::string audiogroupId ;
	long minBitrate = aamp->GetMinimumBitrate();
	long maxBitrate = aamp->GetMaximumBitrate();
	bool iProfileCapped = false;
	bool resolutionCheckEnabled = ISCONFIGSET(eAAMPConfig_LimitResolution);
	if(resolutionCheckEnabled && (0 == aamp->mDisplayWidth || 0 == aamp->mDisplayHeight))
	{
		resolutionCheckEnabled = false;
	}

	if(rate != AAMP_NORMAL_PLAY_RATE && mIframeAvailable)
	{
		// Add all the iframe tracks
		int iFrameSelectedCount = 0;
		int iFrameAvailableCount = 0;

		if (aamp->userProfileStatus)
		{
			// To select profile bitrates are nearer with user configured bitrate range
			for (int i = 0; i < aamp->bitrateList.size(); i++)
			{
				int curIdx = 0;
				long curValue, diff;
				HlsStreamInfo *streamInfo ;
				// Add nearest bitrate profiles until user provided bitrate count
				for (int pidx = 0; pidx < mProfileCount; pidx++)
				{
					streamInfo = &streamInfoStore[pidx];
					diff = abs(streamInfo->bandwidthBitsPerSecond - aamp->bitrateList.at(i));
					if ((0 == pidx) || (diff < curValue))
					{
						curValue = diff;
						curIdx = pidx;
					}
				}
				streamInfo = &streamInfoStore[curIdx];
				streamInfo->validity = true;
			}
		}

		for (;;)
		{
			bool loopAgain{false};
			int j{};
			for (auto &streamInfo : streamInfoStore)
			{
				streamInfo.enabled = false;
				if(streamInfo.isIframeTrack && !(isThumbnailStream(streamInfo)))
				{
					iFrameAvailableCount++;
					if (false == aamp->userProfileStatus && resolutionCheckEnabled && (streamInfo.resolution.width > aamp->mDisplayWidth))
					{
						AAMPLOG_INFO("Iframe Video Profile ignoring higher res=%d:%d display=%d:%d BW=%ld", streamInfo.resolution.width, streamInfo.resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, streamInfo.bandwidthBitsPerSecond);
						iProfileCapped = true;
					}
					else if (aamp->userProfileStatus || ((streamInfo.bandwidthBitsPerSecond >= minBitrate) && (streamInfo.bandwidthBitsPerSecond <= maxBitrate)))
					{
						if (aamp->userProfileStatus && false == streamInfo.validity)
						{
							AAMPLOG_INFO("Iframe Video Profile ignoring by User profile range BW=%ld", streamInfo.bandwidthBitsPerSecond);
							continue;
						}
						else if (false == aamp->userProfileStatus && ISCONFIGSET(eAAMPConfig_Disable4K) &&
							(streamInfo.resolution.height > 1080 || streamInfo.resolution.width > 1920))
						{
							continue;
						}
						//Update profile resolution with VideoEnd Metrics object.
						aamp->UpdateVideoEndProfileResolution( eMEDIATYPE_IFRAME,
								streamInfo.bandwidthBitsPerSecond,
								streamInfo.resolution.width,
								streamInfo.resolution.height );

						aamp->mhAbrManager.addProfile({
								streamInfo.isIframeTrack,
								streamInfo.bandwidthBitsPerSecond,
								streamInfo.resolution.width,
								streamInfo.resolution.height,
								"",
								j});

						streamInfo.enabled = true;
						iFrameSelectedCount++;

						AAMPLOG_INFO("Video Profile added to ABR for Iframe, userData=%d BW =%ld res=%d:%d display=%d:%d pc:%d", j, streamInfo.bandwidthBitsPerSecond, streamInfo.resolution.width, streamInfo.resolution.height,aamp->mDisplayWidth,aamp->mDisplayHeight,iProfileCapped);
					}
				}
				j++;
			}
			if (iFrameAvailableCount > 0 && 0 == iFrameSelectedCount && resolutionCheckEnabled)
			{
				resolutionCheckEnabled = iProfileCapped = false;
				loopAgain = true;
			}
			if (false == loopAgain)
			{
				break;
			}
		}
		if (!aamp->IsFogTSBSupported() && iProfileCapped)
		{
			aamp->mProfileCappedStatus = true;
		}
		if(iFrameSelectedCount == 0 && iFrameAvailableCount !=0)
		{
			// Something wrong , though iframe available , but not selected due to bitrate restriction
			AAMPLOG_WARN("No Iframe available matching bitrate criteria Low[%ld] High[%ld]. Total Iframe available:%d",minBitrate,maxBitrate,iFrameAvailableCount);
		}
		else if(iFrameSelectedCount)
		{
			// this is to sort the iframe tracks
			aamp->mhAbrManager.updateProfile();
		}
	}
	else if(rate == AAMP_NORMAL_PLAY_RATE || rate == AAMP_RATE_PAUSE)
	{
		// Filters to add a video track
		// 1. It should match the audio groupId selected
		// 2. Last filter for min and max bitrate
		// 3. Make sure filters for disableATMOS/disableEC3/disableAAC is applied

		// Get the initial configuration to filter the profiles
		bool bDisableEC3 = ISCONFIGSET(eAAMPConfig_DisableEC3);
		bool bDisableAC3 = ISCONFIGSET(eAAMPConfig_DisableAC3);
		// if EC3 disabled, implicitly disable ATMOS
		bool bDisableATMOS = (bDisableEC3) ? true : ISCONFIGSET(eAAMPConfig_DisableATMOS);
		bool bDisableAAC = false;

		// Check if any demuxed audio exists , if muxed it will be -1
		if (currentAudioProfileIndex >= 0 )
		{
			// Check if audio group id exists
			audiogroupId = mediaInfoStore[currentAudioProfileIndex].group_id;
			AAMPLOG_WARN("Audio groupId selected:%s", audiogroupId.c_str());
		}

		if (aamp->userProfileStatus)
		{
			// To select profile based on user configured bitrate range
			for (int i = 0; i < aamp->bitrateList.size(); i++)
			{
				int tempIdx = 0;
				long tval, diff;
				struct HlsStreamInfo *streamInfo;
				// select profiles bitrate closer with user profiles bitrate
				for (int pidx = 0; pidx < mProfileCount; pidx++)
				{
					streamInfo = &streamInfoStore[pidx];
					diff = abs(streamInfo->bandwidthBitsPerSecond - aamp->bitrateList.at(i));
					if ((0 == pidx) || (diff < tval))
					{
						tval = diff;
						tempIdx = pidx;
					}
				}
				streamInfo = &streamInfoStore[tempIdx];
				streamInfo->validity = true;
			}
		}

		int vProfileCountSelected = 0;
		bool ignoreBitRateRangeCheck = false;
		do{
			int aacProfiles = 0, ac3Profiles = 0, ec3Profiles = 0, atmosProfiles = 0;
			vProfileCountSelected = 0;
			int vProfileCountAvailable = 0;
			int audioProfileMatchedCount = 0;
			int bitrateMatchedCount = 0;
			int resolutionMatchedCount = 0;
			int availableCountATMOS = 0, availableCountEC3 = 0, availableCountAC3 = 0;
			StreamOutputFormat selectedAudioType = FORMAT_INVALID;
			for (int j = 0; j < mProfileCount; j++)
			{
				HlsStreamInfo &streamInfo = streamInfoStore[j];
				streamInfo.enabled = false;
				bool ignoreProfile = false;
				bool clearProfiles = false;
				if(!streamInfo.isIframeTrack)
				{
					vProfileCountAvailable++;

					// complex criteria
					// 1. First check if same audio group available
					//		1.1 If available , pick the profiles for the bw range
					//		1.2 Pick the best audio type

					if( (!audiogroupId.empty() && !streamInfo.audio.empty() && !audiogroupId.compare(streamInfo.audio)) || audiogroupId.empty())
					{
						audioProfileMatchedCount++;
						if(false == aamp->userProfileStatus && resolutionCheckEnabled && (streamInfo.resolution.width > aamp->mDisplayWidth))
						{
							iProfileCapped = true;
							AAMPLOG_INFO("Video Profile ignored Bw=%ld res=%d:%d display=%d:%d", streamInfo.bandwidthBitsPerSecond, streamInfo.resolution.width, streamInfo.resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight);
						}
						else if(false == aamp->userProfileStatus && ISCONFIGSET(eAAMPConfig_Disable4K) && (streamInfo.resolution.height > 1080 || streamInfo.resolution.width > 1920))
						{
							AAMPLOG_INFO("Video Profile ignored for disabled 4k content");
						}
						else
						{
							resolutionMatchedCount++;
							if (false == aamp->userProfileStatus &&
								((streamInfo.bandwidthBitsPerSecond < minBitrate) || (streamInfo.bandwidthBitsPerSecond > maxBitrate)) && !ignoreBitRateRangeCheck)
							{
								iProfileCapped = true;
							}
							else if (aamp->userProfileStatus && false == streamInfo.validity && !ignoreBitRateRangeCheck)
							{
								iProfileCapped = true;
								AAMPLOG_INFO("Video Profile ignored User Profile range Bw=%ld", streamInfo.bandwidthBitsPerSecond);
							}
							else
							{
								bitrateMatchedCount++;

								switch(streamInfo.audioFormat)
								{
									case FORMAT_AUDIO_ES_AAC:
										if(bDisableAAC)
										{
											AAMPLOG_INFO("AAC Profile ignored[%s]", streamInfo.uri.c_str() );
											ignoreProfile = true;
										}
										else
										{
											aacProfiles++;
										}
										break;

									case FORMAT_AUDIO_ES_AC3:
										availableCountAC3++;
										if(bDisableAC3)
										{
											AAMPLOG_INFO("AC3 Profile ignored[%s]",streamInfo.uri.c_str() );
											ignoreProfile = true;
										}
										else
										{
											// found AC3 profile , disable AAC profiles from adding
											ac3Profiles++;
											bDisableAAC = true;
											if(aacProfiles)
											{
												// if already aac profiles added , clear it from local table and ABR table
												aacProfiles = 0;
												clearProfiles = true;
											}
										}
										break;

									case FORMAT_AUDIO_ES_EC3:
										availableCountEC3++;
										if(bDisableEC3)
										{
											AAMPLOG_INFO("EC3 Profile ignored[%s]", streamInfo.uri.c_str() );
											ignoreProfile = true;
										}
										else
										{ // found EC3 profile , disable AAC and AC3 profiles from adding
											ec3Profiles++;
											bDisableAAC = true;
											bDisableAC3 = true;
											if(aacProfiles || ac3Profiles)
											{
												// if already aac or ac3 profiles added , clear it from local table and ABR table
												aacProfiles = ac3Profiles = 0;
												clearProfiles = true;
											}
										}
										break;

									case FORMAT_AUDIO_ES_ATMOS:
										availableCountATMOS++;
										if(bDisableATMOS)
										{
											AAMPLOG_INFO("ATMOS Profile ignored[%s]", streamInfo.uri.c_str());
											ignoreProfile = true;
										}
										else
										{ // found ATMOS Profile , disable AC3, EC3 and AAC profile from adding
											atmosProfiles++;
											bDisableAAC = true;
											bDisableAC3 = true;
											bDisableEC3 = true;
											if(aacProfiles || ac3Profiles || ec3Profiles)
											{
												// if already aac or ac3 or ec3 profiles added , clear it from local table and ABR table
												aacProfiles = ac3Profiles = ec3Profiles = 0;
												clearProfiles = true;
											}
										}
										break;

									default:
										AAMPLOG_WARN("unknown codec string to categorize :%s", streamInfo.codecs.c_str() );
										break;
								}

								if(clearProfiles)
								{
									j = 0;
									vProfileCountAvailable = 0;
									audioProfileMatchedCount = 0;
									bitrateMatchedCount = 0;
									vProfileCountSelected = 0;
									availableCountEC3 = 0;
									availableCountAC3 = 0;
									availableCountATMOS = 0;
									// Continue the loop from start of profile
									continue;
								}

								if(!ignoreProfile)
								{
									streamInfo.enabled = true;
									vProfileCountSelected ++;
									selectedAudioType = streamInfo.audioFormat;
								}
							}
						}
					}
				}
				else if( isThumbnailStream(streamInfo) )
				{
					vProfileCountSelected ++;
				}
			}

			if (aamp->mPreviousAudioType != selectedAudioType)
			{
				AAMPLOG_WARN("AudioType Changed %d -> %d",
						 aamp->mPreviousAudioType, selectedAudioType);
				aamp->mPreviousAudioType = selectedAudioType;
				SetESChangeStatus();
			}

			// Now comes next set of complex checks for bad streams
			if(vProfileCountSelected)
			{
				for (int j = 0; j < mProfileCount; j++)
				{
					HlsStreamInfo &streamInfo = streamInfoStore[j];
					if(streamInfo.enabled)
					{
						//Update profile resolution with VideoEnd Metrics object.
						aamp->UpdateVideoEndProfileResolution( eMEDIATYPE_VIDEO,
								streamInfo.bandwidthBitsPerSecond,
								streamInfo.resolution.width,
								streamInfo.resolution.height );

						aamp->mhAbrManager.addProfile({
								streamInfo.isIframeTrack,
								streamInfo.bandwidthBitsPerSecond,
								streamInfo.resolution.width,
								streamInfo.resolution.height,
								"",
								j});
						AAMPLOG_INFO("Video Profile added to ABR, userData=%d BW=%ld res=%d:%d display=%d:%d pc=%d", j, streamInfo.bandwidthBitsPerSecond, streamInfo.resolution.width, streamInfo.resolution.height, aamp->mDisplayWidth, aamp->mDisplayHeight, iProfileCapped);
					}
					else if( isThumbnailStream(streamInfo) )
					{
						//Updating Thumbnail profiles along with Video profiles.
						aamp->UpdateVideoEndProfileResolution( eMEDIATYPE_IFRAME,
								streamInfo.bandwidthBitsPerSecond,
								streamInfo.resolution.width,
								streamInfo.resolution.height );

						aamp->mhAbrManager.addProfile({
								streamInfo.isIframeTrack,
								streamInfo.bandwidthBitsPerSecond,
								streamInfo.resolution.width,
								streamInfo.resolution.height,
								"",
								j});
						AAMPLOG_INFO("Adding image track, userData=%d BW = %ld ", j, streamInfo.bandwidthBitsPerSecond);
					}
				}
				if (!aamp->IsFogTSBSupported() && iProfileCapped)
				{
					aamp->mProfileCappedStatus = true;
				}
				break;
			}
			else
			{
				if(vProfileCountAvailable && audioProfileMatchedCount==0)
				{
					// Video Profiles available , but not finding anything with audio group .
					// As fallback recovery ,lets play with any other available video profiles
					AAMPLOG_WARN("ERROR No Video Profile found for matching audio group [%s]", audiogroupId.c_str());
					audiogroupId.clear();
					continue;
				}
				else if(vProfileCountAvailable && audioProfileMatchedCount && resolutionMatchedCount==0)
				{
					// Video Profiles available , but not finding anything within configured display resolution
					// As fallback recovery ,lets ignore display resolution check and add available video profiles for playback to happen
					AAMPLOG_WARN("ERROR No Video Profile found for display res = %d:%d",aamp->mDisplayWidth, aamp->mDisplayHeight);
					resolutionCheckEnabled = false;
					iProfileCapped = false;
					continue;
				}
				else if(vProfileCountAvailable && audioProfileMatchedCount && bitrateMatchedCount==0)
				{
					// Video Profiles available , but not finding anything within bitrate range configured
					// As fallback recovery ,lets ignore bitrate limit check and add available video profiles for playback to happen
					AAMPLOG_WARN("ERROR No video profiles available in manifest for playback, minBitrate:%" BITSPERSECOND_FORMAT " maxBitrate:%" BITSPERSECOND_FORMAT, minBitrate, maxBitrate);
					ignoreBitRateRangeCheck = true;
					continue;
				}
				else if(vProfileCountAvailable && bitrateMatchedCount)
				{
					// No profiles selected due to disable config added
					if(bDisableATMOS && availableCountATMOS)
					{
						AAMPLOG_WARN("Resetting DisableATMOS flag as no Video Profile could be selected. ATMOS Count[%d]", availableCountATMOS);
						bDisableATMOS = false;
						continue;
					}
					else if(bDisableEC3 && availableCountEC3)
					{
						AAMPLOG_WARN("Resetting DisableEC3 flag as no Video Profile could be selected. EC3 Count[%d]", availableCountEC3);
						bDisableEC3 = false;
						continue;
					}
					else if(bDisableAC3 && availableCountAC3)
					{
						AAMPLOG_WARN("Resetting DisableAC3 flag as no Video Profile could be selected. AC3 Count[%d]", availableCountAC3);
						bDisableAC3 = false;
						continue;
					}
					else
					{
						AAMPLOG_WARN("Unable to select any video profiles due to unknown codec selection , mProfileCount : %d vProfileCountAvailable:%d", mProfileCount,vProfileCountAvailable);
						break;
					}

				}
				else
				{
					AAMPLOG_WARN("Unable to select any video profiles , mProfileCount : %d vProfileCountAvailable:%d", mProfileCount,vProfileCountAvailable);
					break;
				}
			}
		}while(vProfileCountSelected == 0);
	}
}

/**
 * @brief Function to select the text track and update TextTrackProfileIndex
 */
void StreamAbstractionAAMP_HLS::ConfigureTextTrack()
{
	TextTrackInfo track = aamp->GetPreferredTextTrack();
	currentTextTrackProfileIndex = -1;
	if (!track.index.empty())
	{
		currentTextTrackProfileIndex = std::stoi(track.index);
	}
	else
	{
		for (const auto& LangStr : aamp->preferredSubtitleLanguageVctr)
		{
			currentTextTrackProfileIndex = GetMediaIndexForLanguage(LangStr, eTRACK_SUBTITLE);

			if(currentTextTrackProfileIndex > -1 )
			{
				break;
			}
		}
	}
	AAMPLOG_WARN("TextTrack Selected :%d", currentTextTrackProfileIndex);
}
/**
 * @brief Stops the Track Injection,Restarts once the track has been changed
 */
void StreamAbstractionAAMP_HLS::RefreshTrack(AampMediaType type)
{
	TrackState *track = trackState[type];
	if(track && track->Enabled())
	{
		if(type == eMEDIATYPE_AUDIO)
		{
			track->refreshAudio = true;
		}
		track->AbortWaitForCachedAndFreeFragment(true);
		aamp->StopTrackInjection(type);
		aamp->mDisableRateCorrection = true;
		if(aamp->IsLive() && !track->seamlessAudioSwitchInProgress)
		{
			track->AbortFragmentDownloaderWait();
		}
	}
}

void TrackState::SwitchAudioTrack()
{
	if (eTRACK_AUDIO == type)
	{
		std::lock_guard<std::mutex> guard(mutex);
		// We have an age-old issue to tackle here. playTarget and playlistPosition are hypothetical values as soon as the live window advances
		// mediaSequence number is the source of truth for the playlist position thereafter. Hence we need to find the diff between
		// relative positions in the playlist to find the jump

		// Cache old values before playlist update
		long long oldMediaSequenceNumber = nextMediaSequenceNumber - 1;
		double oldPlaylistPosition = playlistPosition.inSeconds();

		AAMPLOG_INFO("Preparing to flush fragments and switch playlist");
		LoadNewAudio(true);
		seamlessAudioSwitchInProgress = true;

		FlushFragments();
		context->ReassessAndResumeAudioTrack(true);
		context->ConfigureAudioTrack();

		//Update audio track index and notify based on new track configured in ConfigureAudioTrack, Populating AudioAndTextTracks will again append to the older vector
		aamp->mCurrentAudioTrackIndex = context->currentAudioProfileIndex;
		aamp->NotifyAudioTracksChanged();

		aamp_ResolveURL(mPlaylistUrl, aamp->GetManifestUrl(), context->GetPlaylistURI(type).c_str(), ISCONFIGSET(eAAMPConfig_PropagateURIParam));
		mInjectInitFragment = true;
		if(aamp->IsLive())
		{
			// Abort ongoing playlist download or wait for refresh if any.
			aamp->DisableMediaDownloads(playlistMediaType);
			AbortWaitForPlaylistDownload();
			// Notify that fragment collector is waiting
			NotifyFragmentCollectorWait();
			WaitForManifestUpdate();
		}
		else
		{
			PlaylistDownloader();
		}

		//Abort the playback when new audio playlist download fails as we do in InitTracks, to avoid continuing with older audio
		if(!context->aamp->DownloadsAreEnabled())
		{
			NotifyCachedAudioFragmentAvailable();
			seamlessAudioSwitchInProgress = false;
			return;
		}
		// Get the current playback position, we need to seek to this in the new playlist
		AampTime gstSeek{aamp->GetPositionSeconds()};
		TrackState *video = context->trackState[eTRACK_VIDEO];
		if(video && video->enabled)
		{
			video->AcquirePlaylistLock();
			// Adjust for video and audio playlist start time difference and culled seconds
			AampTime delta = mProgramDateTime - video->mProgramDateTime;
			AAMPLOG_DEBUG("gstSeek:%lf, mProgramDateTime:%lf, video->mProgramDateTime:%lf, culledSeconds:%f, delta:%lf", gstSeek.inSeconds(), mProgramDateTime.inSeconds(), video->mProgramDateTime.inSeconds(), aamp->culledSeconds, delta.inSeconds());
			gstSeek = gstSeek - (aamp->culledSeconds + delta);
			video->ReleasePlaylistLock();
		}
		else
		{
			gstSeek = gstSeek - aamp->culledSeconds;
		}
		if(gstSeek < 0)
		{
			gstSeek = 0;
		}
		AAMPLOG_MIL("Updated gstSeek %lf to find new playTarget. Current Playtarget %lf , playlistPosition %lf", gstSeek.inSeconds(), playTarget.inSeconds(), oldPlaylistPosition);

		AcquirePlaylistLock();
		// Relative position in playlist
		double oldPosInPlaylist = GetCompletionTimeForFragment(this, oldMediaSequenceNumber).inSeconds();

		// Iterate from the beginning of the playlist again
		playTarget = gstSeek;
		bool reloadUri = false;

		fragmentURI = GetNextFragmentUriFromPlaylist(reloadUri, true);
		AAMPLOG_DEBUG("After GetNextFragmentUriFromPlaylist Playtarget %lf , playlistPosition %lf", playTarget.inSeconds(), playlistPosition.inSeconds() );
		aamp->mAudioDelta = (gstSeek - playTarget).inSeconds();
		// diff with 2 -> nextMediaSeqNo is always ahead of one fragment hence -1, and, we are yet to download the new MediaSeqNo whereas oldMediaSeq is based on already downloaded fragment hence -2.
		long long newMediaSequenceNumber = nextMediaSequenceNumber - 2;

		// Diff in playlist position. Diff in PDT should be used here???
		double diffInFetchedDuration = (oldPosInPlaylist - GetCompletionTimeForFragment(this, newMediaSequenceNumber)).inSeconds();
		int diffFragmentsDownloaded = (int)(oldMediaSequenceNumber - (newMediaSequenceNumber));
		AAMPLOG_INFO("oldMediaSequenceNumber %lld, newMediaSequenceNumber %lld, oldPosInPlaylist %lf, newPosInPlaylist %lf", oldMediaSequenceNumber, newMediaSequenceNumber, oldPosInPlaylist, GetCompletionTimeForFragment(this, newMediaSequenceNumber).inSeconds() );
		AAMPLOG_INFO("Calculated diffInFetchDuration %lf", diffInFetchedDuration);
		// Try to keep the same playlist position
		// This is because we are using playTarget as position values in cacheFragment
		playlistPosition = (oldPlaylistPosition - diffInFetchedDuration);
		//While injection, playTargetOffset is considered to determine the position of the fragment to sync with the other track, albeit the actual fragment position in the track's playlist may be ahead/behind.
		double diffInInjectedDuration = ((GetLastInjectedFragmentPosition() + playTargetOffset) - playlistPosition).inSeconds();
		//playlistPosition above calculated w.r.t newMediaSequenceNumber that holds fragment till downloaded. Need to add fragDurSecs to get position for the following fragment to be downloaded.
		playlistPosition += fragmentDurationSeconds;
		playTarget = playlistPosition;
		playTargetBufferCalc = playTarget;
		//PlayTargetOffset is determined at Init, hence keep it un-reset.
		//playTargetOffset = 0;
		AAMPLOG_INFO("Calculated diffInFetchDuration %lf diffInInjectedDuration %lf  LastInjectedFragmentPosition() %lf", diffInFetchedDuration, diffInInjectedDuration, GetLastInjectedFragmentPosition());

		AAMPLOG_MIL("Updated Playtarget %lf , playlistPosition %lf", playTarget.inSeconds(), playlistPosition.inSeconds());

		// Reset before we release the playlist lock, to avoid any race conditions
		seamlessAudioSwitchInProgress = false;
		ReleasePlaylistLock();
		if(video && video->enabled)
		{
			AAMPLOG_INFO("video Injected Duration %f ",video->GetTotalInjectedDuration());
		}
		OffsetTrackParams(diffInFetchedDuration, diffInInjectedDuration, diffFragmentsDownloaded);
	}
}

/**
 * @brief Function to populate available audio and text tracks info from manifest
 */
void StreamAbstractionAAMP_HLS::PopulateAudioAndTextTracks()
{
	if (mMediaCount > 0 && mProfileCount > 0)
	{
		bool tracksChanged{false};
		int i{};
		for (auto& media : mediaInfoStore)
		{
			if (media.type == eMEDIATYPE_AUDIO)
			{
				std::string index = std::to_string(i);
				std::string language = (!media.language.empty()) ? GetLanguageCode(i) : std::string();
				std::string codec = GetAudioFormatStringForCodec(media.audioFormat) ;
				//AAMPLOG_WARN("streamAbstractionAAMP_HLS:: Audio Track - lang:%s, group_id:%s, name:%s, codec:%s, characteristics:%s, channels:%d isDefault=%d", language.c_str(), group_id.c_str(), name.c_str(), codec.c_str(), characteristics.c_str(), media.channels,media.isDefault);
				mAudioTracks.push_back(AudioTrackInfo(index, language, media.group_id, media.name, codec, media.characteristics, media.channels,media.isDefault));
			}
			else if (media.type == eMEDIATYPE_SUBTITLE)
			{
				std::string index = std::to_string(i);
				std::string language = (!media.language.empty()) ? GetLanguageCode(i) : std::string();
//				AAMPLOG_WARN("StreamAbstractionAAMP_HLS:: Text Track - lang:%s, isCC:%d, group_id:%s, name:%s, instreamID:%s, characteristics:%s", language.c_str(), media.isCC, group_id.c_str(), name.c_str(), instreamID.c_str(), characteristics.c_str());
				mTextTracks.push_back(TextTrackInfo(index, language, media.isCC, media.group_id, media.name, media.instreamID, media.characteristics,0));
			}
			i++;
		}

		if (-1 != aamp->mCurrentAudioTrackIndex && aamp->mCurrentAudioTrackIndex != currentAudioProfileIndex)
		{
			tracksChanged = true;
		}
		aamp->mCurrentAudioTrackIndex = currentAudioProfileIndex;
		if (tracksChanged)
		{
			aamp->NotifyAudioTracksChanged();
		}

		tracksChanged = false;
		if (-1 != aamp->mCurrentTextTrackIndex && aamp->mCurrentTextTrackIndex != currentTextTrackProfileIndex)
		{
			tracksChanged = true;
		}
		aamp->mCurrentTextTrackIndex = currentTextTrackProfileIndex;
		if (tracksChanged)
		{
			aamp->NotifyTextTracksChanged();
		}
		std::vector<TextTrackInfo> textTracksCopy;
		std::copy_if(begin(mTextTracks), end(mTextTracks), back_inserter(textTracksCopy), [](const TextTrackInfo& e){return e.isCC;});
		std::vector<CCTrackInfo> updatedTextTracks;
		aamp->UpdateCCTrackInfo(textTracksCopy,updatedTextTracks);
		PlayerCCManager::GetInstance()->updateLastTextTracks(updatedTextTracks);
	}
	else
	{
		PlayerCCManager::GetInstance()->updateLastTextTracks({});
		AAMPLOG_ERR("StreamAbstractionAAMP_HLS:: Fail to get available audio/text tracks, mMediaCount=%d and profileCount=%d!", mMediaCount, mProfileCount);
	}

}

/**
 * @brief Function to update seek position
 */
void StreamAbstractionAAMP_HLS::SeekPosUpdate(double secondsRelativeToTuneTime)
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
 * @brief Function to get matching mediaInfo index for a language and track type
 */
int StreamAbstractionAAMP_HLS::GetMediaIndexForLanguage(std::string lang, TrackType type)
{
	int index = -1;
	const char* group = NULL;
	HlsStreamInfo* streamInfo = (HlsStreamInfo*)GetStreamInfo(this->currentProfileIndex);

	if(streamInfo != nullptr)
	{
		if (type == eTRACK_AUX_AUDIO)
		{
			group = streamInfo->audio.c_str();
		}
		else if (type == eTRACK_SUBTITLE)
		{
			group = streamInfo->subtitles.c_str();
		}
	}
	if (group)
	{
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS:: track [%d] group [%s], language [%s]", type, group, lang.c_str());
		int ii{};
		for (auto& mediaInfo : mediaInfoStore)
		{
			if (!mediaInfo.group_id.empty() && !strcmp(group, mediaInfo.group_id.c_str()))
			{
				std::string mediaLang = GetLanguageCode(ii);
				if (lang == mediaLang)
				{
					//Found media tag with preferred language
					index = ii;
					break;
				}
			}
			ii++;
		}
	}

	return index;
}

/**
 * @brief Function to get output format for audio track
 */
StreamOutputFormat StreamAbstractionAAMP_HLS::GetStreamOutputFormatForTrack(TrackType type)
{
	StreamOutputFormat format = FORMAT_UNKNOWN;

	HlsStreamInfo *streamInfo = (HlsStreamInfo *)GetStreamInfo(currentProfileIndex);
	const FormatMap *map = NULL;
	if(streamInfo != nullptr)
	{
		if (type == eTRACK_VIDEO)
		{
			map = GetVideoFormatForCodec(streamInfo->codecs.c_str());
		}
		else if ((type == eTRACK_AUDIO) || (type ==  eTRACK_AUX_AUDIO))
		{
			map = GetAudioFormatForCodec(streamInfo->codecs.c_str());
		}
	}
	if (map)
	{ // video profile specifies audio format
		format = map->format;
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS::Track[%d] format is %d [%s]", type, map->format, map->codec);
	}
	else if ((type == eTRACK_AUDIO) || (type ==  eTRACK_AUX_AUDIO))
	{ // HACK
		AAMPLOG_WARN("StreamAbstractionAAMP_HLS::assuming stereo");
		format = FORMAT_AUDIO_ES_AAC;
	}
	return format;
}

/***************************************************************************
* @brief Function to get available video tracks
*
***************************************************************************/
std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableVideoTracks(void)
{
	std::vector<StreamInfo*> videoTracks{};
	for (auto& streamInfo : streamInfoStore)
	{
		videoTracks.push_back(&streamInfo);
	}
	return videoTracks;
}

/***************************************************************************
* @brief Function to get streamInfo for the profileIndex
*
***************************************************************************/
StreamInfo * StreamAbstractionAAMP_HLS::GetStreamInfo(int idx)
{
	int userData = 0;
	StreamInfo *sInfo = nullptr;

	if (mProfileCount) // avoid calling getUserDataOfProfile() for playlist only URL playback.
	{
		userData = aamp->mhAbrManager.getUserDataOfProfile(idx);
	}
	if(userData >= 0 && userData<streamInfoStore.size() )
	{
		sInfo = &streamInfoStore[userData];
	}

	return sInfo;
}


/****************************************************************************
 *   @brief Change muxed audio track index
 *
 *   @param[in] string index
 *   @return void
****************************************************************************/
void StreamAbstractionAAMP_HLS::ChangeMuxedAudioTrackIndex(std::string& index)
{
	std::string muxPrefix = "mux-";
	std::string trackIndex = index.substr(muxPrefix.size());
	unsigned char indexNum = (unsigned char) stoi(trackIndex);
	if(trackState[eMEDIATYPE_AUDIO] && trackState[eMEDIATYPE_AUDIO]->playContext)
	{
		trackState[eMEDIATYPE_AUDIO]->playContext->ChangeMuxedAudioTrack(indexNum);
	}
	else if(trackState[eMEDIATYPE_VIDEO] && trackState[eMEDIATYPE_VIDEO]->playContext && IsMuxedStream())
	{
		trackState[eMEDIATYPE_VIDEO]->playContext->ChangeMuxedAudioTrack(indexNum);
	}
}

/****************************************************************************
 *   @brief  CMCD Get next object request url(nor)
 *
 *   @param iter playlist cursor
 *   @return void
****************************************************************************/
void TrackState::getNextFetchRequestUri( void )
{
	auto ptr = fragmentURI.getPtr();
	if( ptr )
	{
		size_t offs = ptr - playlist.GetPtr();
		lstring iter( ptr, playlist.GetLen() - offs );
		while( !iter.empty() )
		{
			lstring next = iter.mystrpbrk();
			if( next.removePrefix("#EXTINF") )
			{
				next = iter.mystrpbrk();
				aamp->mCMCDCollector->CMCDSetNextObjectRequest( next.tostring(), 0, (AampMediaType)type);
				break;
			}
		}
	}
}

StreamAbstractionAAMP::ABRMode StreamAbstractionAAMP_HLS::GetABRMode()
{
	ABRMode mode;

	if (aamp->IsFogTSBSupported())
	{
		// Fog manages ABR.
		mode = ABRMode::FOG_TSB;
	}
	else
	{
		// ABR manager is used by AAMP.
		mode = ABRMode::ABR_MANAGER;
	}

	return mode;
}

bool TrackState::IsExtXByteRange( lstring ptr, size_t *byteRangeLength, size_t *byteRangeOffset)
{
    if( ptr.removePrefix("#EXT-X-BYTERANGE:") || ptr.removePrefix("-X-BYTERANGE:") )
    {
        ptr.stripLeadingSpaces();
        *byteRangeLength = ptr.atoll();
        size_t offsetDelim = ptr.find('@');
        if( offsetDelim<ptr.length() )
        {
            ptr.removePrefix(offsetDelim+1); // skip past '@'
            *byteRangeOffset = ptr.atoll();
        }
        return true;
    }
    return false;
}

//Enable default text track for Rialto
void StreamAbstractionAAMP_HLS::SelectSubtitleTrack()
{
	if( currentTextTrackProfileIndex  == -1)
	{
		TextTrackInfo *firstAvailTextTrack = nullptr;
		for (int j = 0; j < mTextTracks.size(); j++)
		{
			if (!mTextTracks[j].isCC)
			{
		firstAvailTextTrack = &mTextTracks[j];
				break;
			}
		}
		if(firstAvailTextTrack != nullptr)
		{
			currentTextTrackProfileIndex = std::stoi(firstAvailTextTrack->index);
			aamp->mIsInbandCC = false;
			aamp->SetCCStatus(false); //mute the subtitle track
			aamp->SetPreferredTextTrack(*firstAvailTextTrack);
		}
	}
	AAMPLOG_INFO("using RialtoSink TextTrack Selected :%d", currentTextTrackProfileIndex);
}

bool StreamAbstractionAAMP_HLS::SelectPreferredTextTrack(TextTrackInfo& selectedTextTrack)
{
	bool bestTrackFound = false;
	unsigned long long bestScore = 0;

	std::vector<TextTrackInfo> availableTracks = GetAvailableTextTracks();

	for (const auto& track : availableTracks)
	{
		unsigned long long score = 1; // Default score for each track

		// Check for language match
		if (!aamp->preferredTextLanguagesString.empty() && track.language == aamp->preferredTextLanguagesString)
		{
			score += AAMP_LANGUAGE_SCORE; // Add score for language match
		}

		if( !aamp->preferredTextRenditionString.empty() && aamp->preferredTextRenditionString.compare(track.rendition) == 0)
		{
			score += AAMP_ROLE_SCORE; // Add score for rendition match
		}

		// Check for name match
		if( !aamp->preferredTextNameString.empty() && aamp->preferredTextNameString.compare(track.name) == 0)
		{
			score += AAMP_TYPE_SCORE; // Add score for name match
		}
		if(score > bestScore)
		{
			bestTrackFound = true;
			bestScore = score;
			selectedTextTrack = track;
		}
	}
	return bestTrackFound;
}

/*
 * @fn DoEarlyStreamSinkFlush
 * @brief Checks if the stream need to be flushed or not
 *
 * @param newTune true if this is a new tune, false otherwise
 * @param rate playback rate
 * @return true if stream should be flushed, false otherwise
 */
bool StreamAbstractionAAMP_HLS::DoEarlyStreamSinkFlush(bool newTune, float rate)
{
	// Live adjust or syncTrack occurred, send an updated flush event
	bool doFlush = !newTune;
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	if (video && video->streamOutputFormat == FORMAT_ISO_BMFF)
	{
		// doFlush for non mp4 formats. HLS MP4 uses media processor to handle flushes
		doFlush = false;
	}
	AAMPLOG_INFO("doFlush=%d, newTune=%d, rate=%f", doFlush, newTune, rate);
	return doFlush;
}

/*
 * @brief Should flush the stream sink on discontinuity or not.
 *
 * @return true if stream should be flushed, false otherwise
 */
bool StreamAbstractionAAMP_HLS::DoStreamSinkFlushOnDiscontinuity()
{
	// doFlush for non mp4 formats.
	bool doFlush = true;
	TrackState *video = trackState[eMEDIATYPE_VIDEO];
	if (video && video->streamOutputFormat == FORMAT_ISO_BMFF)
	{
		// HLS MP4 uses media processor to handle flushes
		doFlush = false;
	}
	AAMPLOG_INFO("doFlush=%d", doFlush);
	return doFlush;
}
