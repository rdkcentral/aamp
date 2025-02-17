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
 * @file VideoStat.h
 * @brief VideoStat data packing and storage
 */

#ifndef __VIDEO_STAT_H__
#define __VIDEO_STAT_H__

#include "IPProfileInfo.h"
#include "ManifestGenericStats.h"
#include "StatsDefine.h"
// Map of profile bitrate(Bits per sec) and CProfileInfo
// 0 is reserved for Main HLS manifest or DASH manifest
typedef std::map<long, CProfileInfo> MapProfileInfo;
typedef std::map<Track, MapProfileInfo>  MapStreamInfo; // collection of all Audio/Video/Profile info
typedef std::map<Track, CLicenseStatistics>  MapLicenceInfo; // Licence stats for each track
typedef std::map<Track, CFragmentStatistics> MapAdStreamInfo; //collection of ad audio/video fragments statistics

/**
 * @class CVideoStat
 * @brief Contains the information/stat on Video
 */
class CVideoStat
{
private:
	long long mTmeToTopProfile;
	long long mTimeAtTopProfile;
	// this is not an asset total duration, this is total duration video was played by user
	// this may be more than asset duration if user rewinds and playagain
	long long mTotalVideoDuration;
	int mNetworkDropCount;
	int mErrorDropCount;
	int mRateCorrectionCount;
	bool mbTsb;
	int mDisplayWidth;  	/**< TV Display Width */
	int mDisplayHeight; 	/**< TV Display Height */
	bool mbProfileCapped; 	/**< Profile capping status */
	MapLicenceInfo mMapLicenseInfo;
	MapStreamInfo mMapStreamInfo;
	std::map<Track ,std::string> mMapLang;
	std::string mMediaFormat;
	long mLiveLatency;
	std::string mPlaybackMode;

public:

	/**
	 *   @brief Default constructor
	 *
	 *   @param[in]  mediaFormat - format of the current playback
         *
	 *   @return None
	 */
	CVideoStat(std::string mediaFormat = "") : mMediaFormat(mediaFormat), mTmeToTopProfile(0), mTimeAtTopProfile(0),mTotalVideoDuration(0), mNetworkDropCount(COUNT_NONE), mErrorDropCount (COUNT_NONE),
					mMapStreamInfo(),mMapLang(),mMapLicenseInfo(),mbTsb(false),mDisplayWidth(0),mDisplayHeight(0),mbProfileCapped(false), mLiveLatency(0),mPlaybackMode("NOT SET"), mRateCorrectionCount(COUNT_NONE)
	{
		ManifestGenericStats::totalGaps = 0;
		CSessionSummary::totalErrorCount = 0;
	}

	/**
	 *   @brief Default Destructor
	 *
	 *   @return None
	 */
	~CVideoStat()
	{

	}
	
	/**
	 *   @brief Sets time latency for live playback
	 *
	 *   @param[in] latency  long long latency
         *
	 *   @return None
	 */
	void setLiveLatency (long latency) { mLiveLatency = latency; }
	
	/**
	 *   @brief Sets playback mode (live/vod etc)
	 *
	 *   @param[in] playbackmode in string format
         *
	 *   @return None
	 */
	void setPlaybackMode (std::string playbackMode)
	{
		mPlaybackMode = playbackMode;
	}

	/**
	 *   @fn IncrementGaps
	 *
	 *   @return None
	 */
	void IncrementGaps();
	/**
	 *   @brief Sets time to top Profile Time stat
	 *
	 *   @param[in] time long long time
         *
	 *   @return None
	 */
	void SetTimeToTopProfile(long long time) { mTmeToTopProfile = time; }

	/**
	 *   @brief Sets time AT top Profile Time stat
	 *
	 *   @param[in] time long long time
         *
	 *   @return None
	 */
	void SetTimeAtTopProfile(long long time) { mTimeAtTopProfile = time; }

	/**
	 *   @brief Sets total duration of videoFragments downloaded
	 *   Note that this is not Video Duration of Asset, as fragments can be re-downloaded in case of trick play
	 *
	 *   @param[in] duration total duration of videoFragments downloaded
         *
	 *   @return None
	 */
	void SetTotalDuration(long long duration) { mTotalVideoDuration =  duration; }

	/**
	 *   @brief Increment NetworkDropCount
	 *   This is count which indicates if bitrate drop happned due to Network bandwidth issue
	 *
	 *   @return None
	 */
	void Increment_NetworkDropCount() { mNetworkDropCount++; }

	/**
	 *   @brief Increment ErrorDropCount
	 *   This is count which indicates if bitrate drop happned due to erros in downloads
	 *
	 *   @return None
	 */
	void Increment_ErrorDropCount() { mErrorDropCount++; }

	/**
	 *   @fn Increment_Fragment_Count
	 *
	 *   @param[in] Track track - Indicates track for which Increment required
	 *   @param[in] bitrate  profile bitrate
	 *   @param[in] download time - download time
	 *   @param[in] response - HTTP/CURL response
	 *   @param[in] connectivity - connection status flag
	 *   @return None
	 */
	void Increment_Fragment_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity);

	/**
	 *   @fn Increment_Init_Fragment_Count
	 *
	 *   @param[in] Track track - Indicates track for which Increment required
	 *   @param[in] bitrate : profile bitrate
	 *   @param[in] download time - download time
	 *   @param[in] response - HTTP/CURL response
	 *   @param[in] connectivity - connection status flag
	 *
	 *   @return None
	 */
	void Increment_Init_Fragment_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity);

	/**
	 *   @fn Increment_Manifest_Count
	 *
	 *   @param[in] Track track - Indicates track for which Increment required
	 *   @param[in] bitrate : profile bitrate ( 0 means Main HLS Mainifest or DASH manifest )
	 *   @param[in] download time - download time
	 *   @param[in] response - HTTP/CURL response
	 *   @param[in] connectivity - connection status flag
	 *
	 *   @return None
	 */
	void Increment_Manifest_Count(Track track, long bitrate, long downloadTimeMs, int response, bool connectivity, ManifestData * manifestData = NULL);

	/**
	 *   @fn Record_License_EncryptionStat
	 *
	 *   @param[in] VideoStatTrackType - Indicates track
	 *   @param[in] isEncrypted - Indicates clear(false) or encrypted ( true)
	 *   @param[in] isKeyChanged - indicates if key is changed for encrypted fragment
	 *   @return None
	 */
	void Record_License_EncryptionStat(VideoStatTrackType eType, bool isEncrypted, bool isKeyChanged, int audioIndex = 1);

	/**
	 *   @fn Increment_Data
	 *
	 *   @param[in] VideoStatDataType - indicates type of Data ( e.g manifest/fragment/license etc )
	 *   @param[in] VideoStatTrackType - Indicates track for which Increment required
	 *   @param[in] bitrate : profile bitrate
	 *   @param[in] download time - download time
	 *   @param[in] response - HTTP/CURL response
	 *   @param[in] bool - connection status flag
	 *   @param[in] audioIndex- Audio track index
	 *   @param[in] manifestData - manifest details structure
	 *   @return None
	 */
	void Increment_Data(VideoStatDataType dataType,VideoStatTrackType eType, long bitrate, long downloadTimeMs, int response, bool connectivity, int audioIndex = 1, ManifestData * manifestData = nullptr);

	/**
	 *   @brief Increment RateCorrectionCount
	 *   This is count which indicates the number of time rate corrected for catching up the live latency
	 *
	 *   @return None
	 */
	void Increment_RateCorrectionCount() { mRateCorrectionCount++; }

	/**
	 *   @fn SetFailedFragmentUrl
	 *
	 *   @param[in] VideoStatTrackType - Indicates track for which Increment required
	 *   @param[in] bitrate : profile bitrate
	 *   @param[in] url : failed content URL
	 *   @param[in] audioIndex- Audio track index
	 *   @return None
	 */
	void SetFailedFragmentUrl(VideoStatTrackType eType, long bitrate, std::string url, int audioIndex = 1);

	/**
	 *   @fn SetProfileResolution
	 *   @param[in] eType Profile or track type
	 *   @param[in] width int width
	 *   @param[in] height int height
	 *   @param[in] audioIndex- Audio track index
	 *   @return None
	 */
	void SetProfileResolution(VideoStatTrackType eType, long bitrate, int width, int height, int audioIndex = 1);

	/**
	 *   @fn SetDisplayResolution
	 *   @param[in] width int width
	 *   @param[in] height int height
	 *   @return None
	 */
	void SetDisplayResolution(int width, int height);

	/**
	 *   @fn Setlanguage
	 *
	 *   @param[in] eType  VideoStatTrackType - Audio Track
	 *   @param[in] strLang lang string
	 *   @param[in] audioIndex Audio Track index
	 *   @return None
	 */
	void Setlanguage(VideoStatTrackType eType, std::string strLang, int audioIndex);

	/**
	 *   @brief sets time shift buffer status
	 *
	 *   @param[in]  bEnable = true means Tsb used.
	 *   @return None
	 */
	void SetTsbStatus(bool bEnable) { mbTsb = bEnable;}

	/**
	  *   @brief sets status of profile capping
	  *
	  *   @param[in]  mbProfileCapped = true means profile restriction applied
	  *   @return None
	  */
        void SetProfileCappedStatus(bool bProfileCapped) { mbProfileCapped = bProfileCapped;}

	/**
	 *   @fn ToJsonString
	 *
	 *   @return Json string - Note that caller is responsible for deleting memory allocated for string
	 */
	char * ToJsonString(const char* additionalData = nullptr, bool forPA = false) const;
};
#endif /* __VIDEO_STAT_H__ */
