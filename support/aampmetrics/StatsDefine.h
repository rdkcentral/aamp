/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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


#ifndef __STATS_DEFINE_H__
#define __STATS_DEFINE_H__

#define DEFAULT_START_VALUE 1
#define LATENCY_WINDOW_BUCKET_DURATION 250

extern bool g_ForPartnerApps;
//if the stats are for partner apps like peacock, then tags are set to be descriptive
#define TAG_VERSION 				(g_ForPartnerApps ?"version":"vr" )	 		/**< time to reach top profile */
#define TAG_TIME_TO_TOP 			(g_ForPartnerApps ?"timeToTopProfile":"tt" ) 		/**< time to reach top profile */
#define TAG_TIME_AT_TOP                         (g_ForPartnerApps ?"timeInTopProfile":"ta" ) 		/**< time for which video remain on top profile */
#define TAG_TIME_PLAYBACK_DURATION 	        (g_ForPartnerApps ?"duration":"d" ) 			/**< time for which playback was done, this is measured at the time of fragment download , hence play-back duration may be slightly less due to g-streamer and aamp buffers */
#define TAG_NET_DROP 				(g_ForPartnerApps ?"profileStepDown_Network":"dn") 	/**< Step down profile count happened due to Bad network bandwidth */
#define TAG_ERR_DROP				(g_ForPartnerApps ?"profileStepDown_Err":"de" ) 	/**< Step down profile count happened due to Bad download errors/failures */
#define TAG_PROFILE_CAPPING      	        (g_ForPartnerApps ?"profileCappingPresent":"pc" ) 	/**< profile filter status by display resolution */
#define TAG_DISPLAY_WIDTH			(g_ForPartnerApps ?"displayWidth":"w" ) 		/**< Display Width */
#define TAG_DISPLAY_HEIGHT			(g_ForPartnerApps ?"displayHeight":"h" ) 		/**< Display Height */
#define TAG_TSB_AVAILABILITY 		        (g_ForPartnerApps ?"tsbAvailable":"t" ) 		/**< indicates if TSB used for playback */
#define TAG_MAIN 				(g_ForPartnerApps ?"main":"m" ) 			/**< Main manifest */
#define TAG_VIDEO				(g_ForPartnerApps ?"video":"v" ) 			/**< Video Profile */
#define TAG_IFRAME				(g_ForPartnerApps ?"iframe":"i" ) 			/**< Iframe Profile */
#define TAG_AD_AUDIO				(g_ForPartnerApps ?"ad_audio":"aa") 			/**< Ad audio track */
#define TAG_AD_VIDEO				(g_ForPartnerApps ?"ad_video":"av") 			/**< Ad video track Audio track 1 */
#define TAG_AUDIO				(g_ForPartnerApps ?"audio":"a" ) 			/**< Audio profile */
#define TAG_SUBTITLE				(g_ForPartnerApps ?"subtitle":"st") 			/**< Subtitle track */
#define TAG_UNKNOWN				(g_ForPartnerApps ?"unknown":"u" ) 			/**< unknown track */
#define TAG_SUPPORTED_LANG 			(g_ForPartnerApps ?"languageSupported":"l" ) 		/**< Supported language */
#define TAG_PROFILES 				(g_ForPartnerApps ?"profiles":"p" )	 		/**< Encapsulates Different Profile available in stream */
#define TAG_LICENSE_STAT			(g_ForPartnerApps ?"licenseStat":"ls" ) 		/**< License statistics */
#define TAG_SESSION_SUMMARY 		        (g_ForPartnerApps ?"sessionSummary":"S" ) 		/**< Session summary */
#define TAG_MANIFEST_INFO	 		(g_ForPartnerApps ?"info":"mi" ) 			/**< Manifest details */
#define TAG_URL 		       		(g_ForPartnerApps ?"lastFailedUrl":"u" ) 		/**< URL of last failed download */
#define TAG_NORMAL_FRAGMENT_STAT                (g_ForPartnerApps ?"media":"n" ) 			/**< Normal Fragment Stats */
#define TAG_NORMAL_INIT_FRAGMENT_STAT           (g_ForPartnerApps ?"init":"i" ) 			/**< Init Fragment Stats used in case of DASH */
#define TAG_TIME_WINDOW_PREFIX			(g_ForPartnerApps ?"timeWindow_":"T" ) 			/**< Time window prefix */
#define TAG_LATENCY_REPORT 			(g_ForPartnerApps ?"latencyReport":"l" ) 		/**< Latency report */
#define TAG_TOTAL_CLEAR_TO_ENCRYPTED            (g_ForPartnerApps ?"totalClearToEncrypted":"c" ) 		/**< Total Clear to Encrypted Switch */
#define TAG_TOTAL_ENCRYPTED_TO_CLEAR            (g_ForPartnerApps ?"totalEncryptedToClear":"e" ) 	/**< Total Encrypted to Clear Switch */
#define TAG_TOTAL_ROTATIONS 		        (g_ForPartnerApps ?"totalLicenseRotations":"r" ) 	/**< Total License rotation or stream switches */
#define TAG_MANIFEST_STAT 			(g_ForPartnerApps ?"manifestStat":"ms" ) 		/**< Manifest Stats */
#define TAG_FRAGMENT_STAT			(g_ForPartnerApps ?"fragmentStat":"fs" ) 		/**< Fragment Stats */
#define TAG_PROFILE_WIDTH 			(g_ForPartnerApps ?"width":"w" ) 			/**< profile Width */
#define TAG_PROFILE_HEIGHT			(g_ForPartnerApps ?"height":"h" ) 			/**< profile Height */
#define TAG_PROFILE_CAPPED      	        (g_ForPartnerApps ?"profileCapped":"pc") 		/**< profile Capping */
#define TAG_MEDIA_TYPE		     	        (g_ForPartnerApps ?"mediaType":"mt") 			/**< Media type */
#define TAG_PLAY_MODE		      	        (g_ForPartnerApps ?"playbackMode":"pm") 		/**< Playback mode */
#define TAG_LIVE_LATENCY	      	        (g_ForPartnerApps ?"liveLatency":"ll") 			/**< Live latency */
#define TAG_TOTAL_ERROR                         (g_ForPartnerApps ?"totalError":"tl") 			/**< total error so far */
#define TAG_NUM_GAPS		      	        (g_ForPartnerApps ?"numOfGaps":"ng") 			/**< total number of period gaps in the case of DASH playback */
#define TAG_REPORT_TIME                         (g_ForPartnerApps ?"creationTime":"ct") 		/**< stats report creation time */
#define TAG_RATE_CORRECTION                     (g_ForPartnerApps ?"rateCorrection":"rc") 		/**< Total rate correction for latency adjustment */

/**
 *  @enum E_VideoStatTrackType
 *  @brief Stores Track types, With respect to Video Stat
 *  Main Manifest download is also considered as seperate track as Stats are same.
 *  Max of 5 Audio tracks data collection supported and seperate IFRAME stats are collected in single track type STAT_IFRAME
 */
typedef enum E_VideoStatTrackType {
	STAT_UNKNOWN,
	STAT_MAIN, // HLS Main manifest or DASH Manifest
	STAT_VIDEO,
	STAT_IFRAME,
	STAT_AD_VIDEO,
	STAT_AD_AUDIO,
	STAT_AUDIO,
	STAT_SUBTITLE
} VideoStatTrackType;

/**
 * @enum E_VideoStatDataType
 * @brief Video stat
 */
typedef enum E_VideoStatDataType{
	VE_DATA_UNKNOWN,
	VE_DATA_MANIFEST,
	VE_DATA_FRAGMENT,
	VE_DATA_INIT_FRAGMENT,
	VE_DATA_LICENSE
} VideoStatDataType;

/**
 *  @struct Track
 *  @brief  Structure to keep the track type and number
 *  	    Main Manifest download is also considered as seperate track as Stats are same.
 *  	    Max of 5 Audio tracks data collection supported and seperate IFRAME stats are collected in single track type STAT_IFRAME
 */
struct Track
{
	Track(VideoStatTrackType type, int number) :type(type), number(number){}
	const VideoStatTrackType type;
	/**
	 * @brief Just to assist in storing Track in std::maps
	 */
	bool operator < (Track t) const
	{
		return (type < t.type || number < t.number);
	}
	int number;
	static std::map<VideoStatTrackType, std::string> StatTrackTypeStr;
	
	/**
	 * @brief As predefined macros for TAGs would be set initially, without resting, it 
	 *        will not consider the "g_ForPartnerApps" condition
	 */
	static void setTrackNames()
	{
		StatTrackTypeStr = {
			{STAT_MAIN, TAG_MAIN},
			{STAT_VIDEO, TAG_VIDEO},
			{STAT_IFRAME, TAG_IFRAME},
			{STAT_AD_AUDIO, TAG_AD_AUDIO},
			{STAT_AD_VIDEO, TAG_AD_VIDEO},
			{STAT_AUDIO, TAG_AUDIO},
			{STAT_SUBTITLE, TAG_SUBTITLE},
			{STAT_UNKNOWN, TAG_UNKNOWN}
		};
	}

	/**
	 * @brief Gives track name and id in string form
	 */
	std::string toString() const
	{
		if(type == STAT_AUDIO)
		{
			return (StatTrackTypeStr.at(type) + std::to_string(number));
		}
		else
		{
			return StatTrackTypeStr.at(type);
		}
	}
};








#endif /* __STATS_DEFINE_H__ */
