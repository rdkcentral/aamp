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
 * @file AampProfiler.cpp
 * @brief ProfileEventAAMP class impl
 */

#include "AampProfiler.h"
#include "AampConstants.h"
#include "AampUtils.h"
#include "AampConfig.h"
#ifdef AAMP_TELEMETRY_SUPPORT
#include "AampTelemetry2.hpp"
#endif //AAMP_TELEMETRY_SUPPORT

#include <algorithm>
#include <iomanip>
#define MAX std::max

/**
 * @brief ProfileEventAAMP Constructor
 */
ProfileEventAAMP::ProfileEventAAMP():
	tuneStartMonotonicBase(0), tuneStartBaseUTCMS(0), bandwidthBitsPerSecondVideo(0),
        bandwidthBitsPerSecondAudio(0), buckets(), drmErrorCode(0), enabled(false), xreTimeBuckets(), tuneEventList(),
	tuneEventListMtx(), mTuneFailBucketType(PROFILE_BUCKET_MANIFEST), mTuneFailErrorCode(0), rateCorrection(0), bitrateChange(0), bufferChange(0), telemetryParam(NULL), mLldLowBuffObject(NULL),discontinuityParamMutex()
{
}

std::string ProfileEventAAMP::GetTuneTimeMetricAsJson(TuneEndMetrics tuneMetricsData, const char *tuneTimeStrPrefix,
				unsigned int licenseAcqNWTime, bool playerPreBuffered,
				unsigned int durationSeconds, bool interfaceWifi, std::string failureReason, std::string appName)
{
	//Convert to JSON format
	std::string metrics = "";
	cJSON *item = nullptr;
	item = cJSON_CreateObject();

	if( nullptr == item )
	{
		return metrics;
	}

	cJSON_AddStringToObject(item, "pre", tuneTimeStrPrefix);
	cJSON_AddNumberToObject(item, "ver", AAMP_TUNETIME_VERSION);
	cJSON_AddStringToObject(item, "bld", AAMP_VERSION);
	cJSON_AddNumberToObject(item, "tbu", tuneStartBaseUTCMS);

	cJSON_AddNumberToObject(item, "mms", buckets[PROFILE_BUCKET_MANIFEST].tStart);
	cJSON_AddNumberToObject(item, "mmt", bucketDuration(PROFILE_BUCKET_MANIFEST));
	cJSON_AddNumberToObject(item, "mme", buckets[PROFILE_BUCKET_MANIFEST].errorCount);

	cJSON_AddNumberToObject(item, "vps", buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].tStart);
	cJSON_AddNumberToObject(item, "vpt", bucketDuration(PROFILE_BUCKET_PLAYLIST_VIDEO));
	cJSON_AddNumberToObject(item, "vpe", buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].errorCount);

	cJSON_AddNumberToObject(item, "aps", buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].tStart);
	cJSON_AddNumberToObject(item, "apt", bucketDuration(PROFILE_BUCKET_PLAYLIST_AUDIO));
	cJSON_AddNumberToObject(item, "ape", buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].errorCount);

	cJSON_AddNumberToObject(item, "vis", buckets[PROFILE_BUCKET_INIT_VIDEO].tStart);
	cJSON_AddNumberToObject(item, "vit", bucketDuration(PROFILE_BUCKET_INIT_VIDEO));
	cJSON_AddNumberToObject(item, "vie", buckets[PROFILE_BUCKET_INIT_VIDEO].errorCount);

	cJSON_AddNumberToObject(item, "ais", buckets[PROFILE_BUCKET_INIT_AUDIO].tStart);
	cJSON_AddNumberToObject(item, "ait", bucketDuration(PROFILE_BUCKET_INIT_AUDIO));
	cJSON_AddNumberToObject(item, "aie", buckets[PROFILE_BUCKET_INIT_AUDIO].errorCount);

	cJSON_AddNumberToObject(item, "vfs", buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].tStart);
	cJSON_AddNumberToObject(item, "vft", bucketDuration(PROFILE_BUCKET_FRAGMENT_VIDEO));
	cJSON_AddNumberToObject(item, "vfe", buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].errorCount);
	cJSON_AddNumberToObject(item, "vfb", bandwidthBitsPerSecondVideo);

	cJSON_AddNumberToObject(item, "afs", buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].tStart);
	cJSON_AddNumberToObject(item, "aft", bucketDuration(PROFILE_BUCKET_FRAGMENT_AUDIO));
	cJSON_AddNumberToObject(item, "afe", buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].errorCount);
	cJSON_AddNumberToObject(item, "afb", bandwidthBitsPerSecondAudio);

	cJSON_AddNumberToObject(item, "las", buckets[PROFILE_BUCKET_LA_TOTAL].tStart);
	cJSON_AddNumberToObject(item, "lat", bucketDuration(PROFILE_BUCKET_LA_TOTAL));
	cJSON_AddNumberToObject(item, "dfe", drmErrorCode);

	cJSON_AddNumberToObject(item, "lpr", bucketDuration(PROFILE_BUCKET_LA_PREPROC));
	cJSON_AddNumberToObject(item, "lnw", licenseAcqNWTime);
	cJSON_AddNumberToObject(item, "lps", bucketDuration(PROFILE_BUCKET_LA_POSTPROC));

	cJSON_AddNumberToObject(item, "vdd", bucketDuration(PROFILE_BUCKET_DECRYPT_VIDEO));
	cJSON_AddNumberToObject(item, "add", bucketDuration(PROFILE_BUCKET_DECRYPT_AUDIO));

	cJSON_AddNumberToObject(item, "gps", (playerPreBuffered && tuneMetricsData.success > 0) ? buckets[PROFILE_BUCKET_FIRST_BUFFER].tStart - buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : buckets[PROFILE_BUCKET_FIRST_BUFFER].tStart);
	cJSON_AddNumberToObject(item, "gff", (playerPreBuffered && tuneMetricsData.success > 0) ? buckets[PROFILE_BUCKET_FIRST_FRAME].tStart - buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : buckets[PROFILE_BUCKET_FIRST_FRAME].tStart);

	cJSON_AddNumberToObject(item, "cnt", tuneMetricsData.contentType);
	cJSON_AddNumberToObject(item, "stt", tuneMetricsData.streamType);
	cJSON_AddBoolToObject(item, "ftt", tuneMetricsData.mFirstTune);

	cJSON_AddNumberToObject(item, "pbm", playerPreBuffered);
	cJSON_AddNumberToObject(item, "tpb", playerPreBuffered ? buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart : 0);

	cJSON_AddNumberToObject(item, "dus", durationSeconds);
	cJSON_AddNumberToObject(item, "ifw", interfaceWifi);

	cJSON_AddNumberToObject(item, "tat", tuneMetricsData.mTuneAttempts);
	cJSON_AddNumberToObject(item, "tst", tuneMetricsData.success);
	cJSON_AddStringToObject(item, "frs", failureReason.c_str());
	cJSON_AddStringToObject(item, "app", appName.c_str());

	cJSON_AddNumberToObject(item, "tsb", tuneMetricsData.mFogTSBEnabled);
	cJSON_AddNumberToObject(item, "tot", tuneMetricsData.mTotalTime);

	//lets use cJSON_PrintUnformatted , cJSON_Print is formated adds whitespace n hence takes more memory also eats up more logs if logged.
	char *jsonStr = cJSON_PrintUnformatted(item);
	if (jsonStr)
	{
		metrics.assign(jsonStr);

#ifdef AAMP_TELEMETRY_SUPPORT
		AAMPTelemetry2 at2;
		at2.send("VideoStartTime", jsonStr);
#endif // AAMP_TELEMETRY_SUPPORT

		cJSON_free(jsonStr);
	}
	cJSON_Delete(item);
	return metrics;
}

/**
 *  @brief Get tune time events in JSON format
 */
void ProfileEventAAMP::getTuneEventsJSON(std::string &outStr, const std::string &streamType, const char *url, bool success)
{
	bool siblingEvent = false;
	unsigned int tEndTime = (unsigned int)NOW_STEADY_TS_MS;
	unsigned int td = (unsigned int)(tEndTime - tuneStartMonotonicBase);
	size_t end = 0;

	std::string tempUrl = url;
	end = tempUrl.find("?");

	if (end != std::string::npos)
	{
		tempUrl = tempUrl.substr(0, end);
	}

	char outPtr[512];
	memset(outPtr, '\0', 512);

	snprintf(outPtr, 512, "{\"s\":%lld,\"td\":%d,\"st\":\"%s\",\"u\":\"%s\",\"tf\":{\"i\":%d,\"er\":%d},\"r\":%d,\"v\":[",tuneStartBaseUTCMS, td, streamType.c_str(), tempUrl.c_str(), mTuneFailBucketType, mTuneFailErrorCode, (success ? 1 : 0));

	outStr.append(outPtr);

	std::lock_guard<std::mutex> lock(tuneEventListMtx);
	for(auto &te:tuneEventList)
	{
		if(siblingEvent)
		{
			outStr.append(",");
		}
		char eventPtr[256];
		memset(eventPtr, '\0', 256);
		snprintf(eventPtr, 256, "{\"i\":%d,\"b\":%d,\"d\":%d,\"o\":%d}", te.id, te.start, te.duration, te.result);
		outStr.append(eventPtr);

		siblingEvent = true;
	}
	outStr.append("]}");

	tuneEventList.clear();
	mTuneFailErrorCode = 0;
	mTuneFailBucketType = PROFILE_BUCKET_MANIFEST;
}

/**
 *  @brief Profiler method to perform tune begin related operations.
 */
void ProfileEventAAMP::TuneBegin(void)
{ // start tune
	memset(buckets, 0, sizeof(buckets));
	tuneStartBaseUTCMS = NOW_SYSTEM_TS_MS;
	tuneStartMonotonicBase = NOW_STEADY_TS_MS;
	bandwidthBitsPerSecondVideo = 0;
	bandwidthBitsPerSecondAudio = 0;
	drmErrorCode = 0;
	enabled = true;
	mTuneFailBucketType = PROFILE_BUCKET_MANIFEST;
	mTuneFailErrorCode = 0;
	tuneEventList.clear();
	rateCorrection = 0;
	bitrateChange = 0;
	bufferChange = 0;
	if(telemetryParam != NULL)
	{
		cJSON_Delete(telemetryParam);
	}
	mLldLowBuffObject = NULL;
	telemetryParam = cJSON_CreateObject();
}

/**
 * @brief Logging performance metrics after successful tune completion. Metrics starts with IP_AAMP_TUNETIME
 *
 * <h4>Format of IP_AAMP_TUNETIME:</h4>
 * version,                       // version for this protocol, initially zero<br>
 * build,                         // incremented when there are significant player changes/optimizations<br>
 * tunestartUtcMs,                // when tune logically started from AAMP perspective<br>
 * <br>
 * ManifestDownloadStartTime,     // offset in milliseconds from tunestart when main manifest begins download<br>
 * ManifestDownloadTotalTime,     // time (ms) taken for main manifest download, relative to ManifestDownloadStartTime<br>
 * ManifestDownloadFailCount,     // if >0 ManifestDownloadTotalTime spans multiple download attempts<br>
 * <br>
 * PlaylistDownloadStartTime,     // offset in milliseconds from tunestart when playlist subManifest begins download<br>
 * PlaylistDownloadTotalTime,     // time (ms) taken for playlist subManifest download, relative to PlaylistDownloadStartTime<br>
 * PlaylistDownloadFailCount,     // if >0 otherwise PlaylistDownloadTotalTime spans multiple download attempts<br>
 * <br>
 * InitFragmentDownloadStartTime, // offset in milliseconds from tunestart when init fragment begins download<br>
 * InitFragmentDownloadTotalTime, // time (ms) taken for fragment download, relative to InitFragmentDownloadStartTime<br>
 * InitFragmentDownloadFailCount, // if >0 InitFragmentDownloadTotalTime spans multiple download attempts<br>
 * <br>
 * Fragment1DownloadStartTime,    // offset in milliseconds from tunestart when fragment begins download<br>
 * Fragment1DownloadTotalTime,    // time (ms) taken for fragment download, relative to Fragment1DownloadStartTime<br>
 * Fragment1DownloadFailCount,    // if >0 Fragment1DownloadTotalTime spans multiple download attempts<br>
 * Fragment1Bandwidth,            // intrinsic bitrate of downloaded fragment<br>
 * <br>
 * drmLicenseRequestStart,        // offset in milliseconds from tunestart<br>
 * drmLicenseRequestTotalTime,    // time (ms) for license acquisition relative to drmLicenseRequestStart<br>
 * drmFailErrorCode,              // nonzero if drm license acquisition failed during tuning<br>
 * <br>
 * LAPreProcDuration,             // License acquisition pre-processing duration in ms<br>
 * LANetworkDuration,             // License acquisition network duration in ms<br>
 * LAPostProcDuration,            // License acquisition post-processing duration in ms<br>
 * <br>
 * VideoDecryptDuration,          // Video fragment decrypt duration in ms<br>
 * AudioDecryptDuration,          // Audio fragment decrypt duration in ms<br>
 * <br>
 * gstStart,                      // offset in ms from tunestart when pipeline creation/setup begins<br>
 * gstFirstFrame,                 // offset in ms from tunestart when first frame of video is decoded/presented<br>
 * contentType,                   //Playback Mode. Values: CDVR, VOD, LINEAR, IVOD, EAS, CAMERA, DVR, MDVR, IPDVR, PPV<br>
 * streamType,                    //Stream Type. Values: 10-HLS/Clear, 11-HLS/Consec, 12-HLS/Access, 13-HLS/Vanilla AES, 20-DASH/Clear, 21-DASH/WV, 22-DASH/PR<br>
 * firstTune                      //First tune after reboot/crash<br>
 * Prebuffered                    //If the Player was in preBuffer(BG) mode)<br>
 * PreBufferedTime                //Player spend Time in BG<br> 
 * success                        //Tune status
 * contentType                    //Content Type. Eg: LINEAR, VOD, etc
 * streamType                     //Stream Type. Eg: HLS, DASH, etc
 * firstTune                      //Is it a first tune after reboot/crash.
 * <br>
 */
void ProfileEventAAMP::TuneEnd(TuneEndMetrics &mTuneEndMetrics,std::string appName, std::string playerActiveMode, int playerId, bool playerPreBuffered, unsigned int durationSeconds, bool interfaceWifi, std::string failureReason, std::string *tuneMetricData)
{
	if(!enabled )
	{
		return;
	}
	enabled = false;
	unsigned int licenseAcqNWTime = bucketDuration(PROFILE_BUCKET_LA_NETWORK);
	char tuneTimeStrPrefix[64];
	memset(tuneTimeStrPrefix, '\0', sizeof(tuneTimeStrPrefix));
	int mTotalTime;
 	int mTimedMetadataStartTime = static_cast<int> (mTuneEndMetrics.mTimedMetadataStartTime - tuneStartMonotonicBase);

	auto tFirstFrameStart = buckets[PROFILE_BUCKET_FIRST_FRAME].tStart;
	auto tDecryptVideoFinish = buckets[PROFILE_BUCKET_DECRYPT_VIDEO].tFinish;
	auto tFirstBufferStart = buckets[PROFILE_BUCKET_FIRST_BUFFER].tStart;
	auto tPreBufferStart = buckets[PROFILE_BUCKET_PLAYER_PRE_BUFFERED].tStart;
	
	// compute gstreamer decode time, excluding decryption. For clear streams, measure from first buffer start time
	auto tDecode = tFirstFrameStart - (tDecryptVideoFinish?tDecryptVideoFinish:tFirstBufferStart);

	if (mTuneEndMetrics.success > 0)
	{
		mTotalTime = playerPreBuffered ? tFirstFrameStart - tPreBufferStart : tFirstFrameStart;
	}
	else
	{
		mTotalTime = static_cast<int> (mTuneEndMetrics.mTotalTime - tuneStartMonotonicBase);
	}
	if (!appName.empty())
	{
		snprintf(tuneTimeStrPrefix, sizeof(tuneTimeStrPrefix), "%s PLAYER[%d] APP: %s IP_AAMP_TUNETIME", playerActiveMode.c_str(),playerId,appName.c_str());
	}
	else
	{
		snprintf(tuneTimeStrPrefix, sizeof(tuneTimeStrPrefix), "%s PLAYER[%d] IP_AAMP_TUNETIME", playerActiveMode.c_str(),playerId);
	}

	AAMPLOG_WARN("%s:%d,%s,%lld," // prefix, version, build, tuneStartBaseUTCMS
		"%d,%d,%d,"		// main manifest (start,total,err)
		"%d,%d,%d,"		// video playlist (start,total,err)
		"%d,%d,%d,"		// audio playlist (start,total,err)

		"%d,%d,%d,"		// video init-segment (start,total,err)
		"%d,%d,%d,"		// audio init-segment (start,total,err)

		"%d,%d,%d,%ld,"	// video fragment (start,total,err, bitrate)
		"%d,%d,%d,%ld,"	// audio fragment (start,total,err, bitrate)

		"%d,%d,%d,"		// licenseAcqStart, licenseAcqTotal, drmFailErrorCode
		"%d,%d,%d,"		// LAPreProcDuration, LANetworkDuration, LAPostProcDuration

		"%d,%d,"		// VideoDecryptDuration, AudioDecryptDuration
		"%d,%d,%d,"		// gstPlayStartTime, gstFirstFrameTime
		"%d,%d,%d,"		// contentType, streamType, firstTune
		"%d,%d,"		// If Player was in prebuffered mode, time spent in prebuffered(BG) mode
		"%d,%d,"		// Asset duration in seconds, Connection is wifi or not - wifi(1) ethernet(0)
		"%d,%d,%s,%s,"		// TuneAttempts ,Tunestatus -success(1) failure (0) ,Failure Reason, AppName
		"%d,%d,%d,%d,%d",       // TimedMetadata (count,start,total) ,TSBEnabled or not - enabled(1) not enabled(0)
					//  TotalTime -for failure and interrupt tune -it is time at which failure /interrupt reported	
		// TODO: settop type, flags, isFOGEnabled, isDDPlus, isDemuxed, assetDurationMs

		tuneTimeStrPrefix,
		AAMP_TUNETIME_VERSION, // version for this protocol, initially zero
		AAMP_VERSION, // build - incremented when there are significant player changes/optimizations
		tuneStartBaseUTCMS, // when tune logically started from AAMP perspective

		buckets[PROFILE_BUCKET_MANIFEST].tStart, bucketDuration(PROFILE_BUCKET_MANIFEST), buckets[PROFILE_BUCKET_MANIFEST].errorCount,
		buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].tStart, bucketDuration(PROFILE_BUCKET_PLAYLIST_VIDEO), buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].errorCount,
		buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].tStart, bucketDuration(PROFILE_BUCKET_PLAYLIST_AUDIO), buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].errorCount,

		buckets[PROFILE_BUCKET_INIT_VIDEO].tStart, bucketDuration(PROFILE_BUCKET_INIT_VIDEO), buckets[PROFILE_BUCKET_INIT_VIDEO].errorCount,
		buckets[PROFILE_BUCKET_INIT_AUDIO].tStart, bucketDuration(PROFILE_BUCKET_INIT_AUDIO), buckets[PROFILE_BUCKET_INIT_AUDIO].errorCount,

		buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].tStart, bucketDuration(PROFILE_BUCKET_FRAGMENT_VIDEO), buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].errorCount,bandwidthBitsPerSecondVideo,
		buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].tStart, bucketDuration(PROFILE_BUCKET_FRAGMENT_AUDIO), buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].errorCount,bandwidthBitsPerSecondAudio,

		buckets[PROFILE_BUCKET_LA_TOTAL].tStart, bucketDuration(PROFILE_BUCKET_LA_TOTAL), drmErrorCode,
		bucketDuration(PROFILE_BUCKET_LA_PREPROC), licenseAcqNWTime, bucketDuration(PROFILE_BUCKET_LA_POSTPROC),
		bucketDuration(PROFILE_BUCKET_DECRYPT_VIDEO),bucketDuration(PROFILE_BUCKET_DECRYPT_AUDIO),

		(playerPreBuffered && mTuneEndMetrics.success > 0) ? tFirstBufferStart - tPreBufferStart : tFirstBufferStart, // gstPlaying: offset in ms from tunestart when pipeline first fed data
		(playerPreBuffered && mTuneEndMetrics.success > 0) ? tFirstFrameStart - tPreBufferStart : tFirstFrameStart,  // gstFirstFrame: offset in ms from tunestart when first frame of video is decoded/presented
		tDecode, // gstDecode: time taken to decode first frame, excluding decryption time
		mTuneEndMetrics.contentType,mTuneEndMetrics.streamType,mTuneEndMetrics.mFirstTune,
		playerPreBuffered,playerPreBuffered ? tPreBufferStart : 0,
		durationSeconds,interfaceWifi,
		mTuneEndMetrics.mTuneAttempts, mTuneEndMetrics.success,failureReason.c_str(),appName.c_str(),
		mTuneEndMetrics.mTimedMetadata,mTimedMetadataStartTime < 0 ? 0 : mTimedMetadataStartTime , mTuneEndMetrics.mTimedMetadataDuration,mTuneEndMetrics.mFogTSBEnabled,mTotalTime
		);

		// Telemetry is generated in GetTuneTimeMetricAsJson hence calling always,
		std::string metricsDataJson = GetTuneTimeMetricAsJson(mTuneEndMetrics, tuneTimeStrPrefix, licenseAcqNWTime, playerPreBuffered, durationSeconds, interfaceWifi, failureReason, appName);

		// tuneMetricData could be NULL if application has not registered for tuneMetrics event,
		if( NULL != tuneMetricData)
		{
			//provided the time tune metric data as an json format to application
			*tuneMetricData = metricsDataJson;
		}
}

/**
 *  @brief Method converting the AAMP style tune performance data to IP_EX_TUNETIME style data
 */
void ProfileEventAAMP::GetClassicTuneTimeInfo(bool success, int tuneRetries, int firstTuneType, long long playerLoadTime, int streamType, bool isLive,unsigned int durationS, char *TuneTimeInfoStr)
{
	// Prepare String for Classic TuneTime data
	// Note: Certain buckets won't be available; will take the tFinish of the previous bucket as the start & finish those buckets.
	xreTimeBuckets[TuneTimeBeginLoad]               =       tuneStartMonotonicBase ;
	xreTimeBuckets[TuneTimePrepareToPlay]           =       tuneStartMonotonicBase + buckets[PROFILE_BUCKET_MANIFEST].tFinish;
	xreTimeBuckets[TuneTimePlay]                    =       tuneStartMonotonicBase + MAX(buckets[PROFILE_BUCKET_MANIFEST].tFinish, MAX(buckets[PROFILE_BUCKET_PLAYLIST_VIDEO].tFinish, buckets[PROFILE_BUCKET_PLAYLIST_AUDIO].tFinish));
	xreTimeBuckets[TuneTimeDrmReady]                =       MAX(xreTimeBuckets[TuneTimePlay], (tuneStartMonotonicBase +  buckets[PROFILE_BUCKET_LA_TOTAL].tFinish));
	long long fragmentReadyTime                     =       tuneStartMonotonicBase + MAX(buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].tFinish, buckets[PROFILE_BUCKET_FRAGMENT_AUDIO].tFinish);
	xreTimeBuckets[TuneTimeStartStream]             =       MAX(xreTimeBuckets[TuneTimeDrmReady], (tuneStartMonotonicBase +  buckets[PROFILE_BUCKET_FRAGMENT_VIDEO].tFinish));
	xreTimeBuckets[TuneTimeStreaming]               =       tuneStartMonotonicBase + buckets[PROFILE_BUCKET_FIRST_FRAME].tStart;

	unsigned int failRetryBucketTime                =       (unsigned int)(tuneStartMonotonicBase - playerLoadTime);
	unsigned int prepareToPlayBucketTime            =       (unsigned int)(xreTimeBuckets[TuneTimePrepareToPlay] - xreTimeBuckets[TuneTimeBeginLoad]);
	unsigned int playBucketTime                     =       (unsigned int)(xreTimeBuckets[TuneTimePlay]- xreTimeBuckets[TuneTimePrepareToPlay]);
	unsigned int fragmentBucketTime                 =       (unsigned int)(fragmentReadyTime - xreTimeBuckets[TuneTimePlay]) ;
	unsigned int decoderStreamingBucketTime         =       (unsigned int)(xreTimeBuckets[TuneTimeStreaming] - xreTimeBuckets[TuneTimeStartStream]);
	/*Note: 'Drm Ready' to 'decrypt start' gap is not in any of the buckets.*/

	unsigned int manifestTotal      =       bucketDuration(PROFILE_BUCKET_MANIFEST);
	unsigned int profilesTotal      =       effectiveBucketTime(PROFILE_BUCKET_PLAYLIST_VIDEO, PROFILE_BUCKET_PLAYLIST_AUDIO);
	unsigned int initFragmentTotal  =       effectiveBucketTime(PROFILE_BUCKET_INIT_VIDEO, PROFILE_BUCKET_INIT_AUDIO);
	unsigned int fragmentTotal      =       effectiveBucketTime(PROFILE_BUCKET_FRAGMENT_VIDEO, PROFILE_BUCKET_FRAGMENT_AUDIO);
	// DrmReadyBucketTime is licenseTotal, time taken for complete license acquisition
	// licenseNWTime is the time taken for network request.
	unsigned int licenseTotal       =       bucketDuration(PROFILE_BUCKET_LA_TOTAL);
	unsigned int licenseNWTime      =       bucketDuration(PROFILE_BUCKET_LA_NETWORK);
	if(licenseNWTime == 0)
	{
		licenseNWTime = licenseTotal;  //A HACK for HLS
	}

	// Total Network Time
	unsigned int networkTime = manifestTotal + profilesTotal + initFragmentTotal + fragmentTotal + licenseNWTime;

	snprintf(TuneTimeInfoStr,AAMP_MAX_PIPE_DATA_SIZE,"%d,%lld,%d,%d," //totalNetworkTime, playerLoadTime , failRetryBucketTime, prepareToPlayBucketTime,
			"%d,%d,%d,"                                             //playBucketTime ,licenseTotal , decoderStreamingBucketTime
			"%d,%d,%d,%d,"                                          // manifestTotal,profilesTotal,fragmentTotal,effectiveFragmentDLTime
			"%d,%d,%d,%d,"                                          // licenseNWTime,success,durationMs,isLive
			"%lld,%lld,%lld,"                                       // TuneTimeBeginLoad,TuneTimePrepareToPlay,TuneTimePlay,
			"%lld,%lld,%lld,"                                       //TuneTimeDrmReady,TuneTimeStartStream,TuneTimeStreaming
			"%d,%d,%d,%lld",                                             //streamType, tuneRetries, TuneType, TuneCompleteTime(UTC MSec)
			networkTime,playerLoadTime, failRetryBucketTime, prepareToPlayBucketTime,playBucketTime,licenseTotal,decoderStreamingBucketTime,
			manifestTotal,profilesTotal,(initFragmentTotal + fragmentTotal),fragmentBucketTime, licenseNWTime,success,durationS*1000,isLive,
			xreTimeBuckets[TuneTimeBeginLoad],xreTimeBuckets[TuneTimePrepareToPlay],xreTimeBuckets[TuneTimePlay] ,xreTimeBuckets[TuneTimeDrmReady],
			xreTimeBuckets[TuneTimeStartStream],xreTimeBuckets[TuneTimeStreaming],streamType,tuneRetries,firstTuneType,(long long)NOW_SYSTEM_TS_MS
	);
#ifndef CREATE_PIPE_SESSION_TO_XRE
	AAMPLOG_WARN("AAMP=>XRE: %s", TuneTimeInfoStr);
#endif
}

/**
 * @brief Marking the beginning of a bucket
 */
void ProfileEventAAMP::ProfileBegin(ProfilerBucketType type)
{
	struct ProfilerBucket *bucket = &buckets[type];
	if (!bucket->complete && (0==bucket->tStart))	//No other Begin should record before the End
	{
		bucket->tStart 		= (unsigned int)(NOW_STEADY_TS_MS - tuneStartMonotonicBase);
		bucket->tFinish 	= bucket->tStart;
		bucket->profileStarted = true;
	}
}

/**
 *  @brief Marking error while executing a bucket
 */
void ProfileEventAAMP::ProfileError(ProfilerBucketType type, int result)
{
	struct ProfilerBucket *bucket = &buckets[type];
	if (!bucket->complete && bucket->profileStarted)
	{
		SetTuneFailCode(result, type);
		bucket->errorCount++;

	}
}

/**
 *  @brief Marking the end of a bucket
 */
void ProfileEventAAMP::ProfileEnd(ProfilerBucketType type)
{
	struct ProfilerBucket *bucket = &buckets[type];
	if (!bucket->complete && bucket->profileStarted)
	{
		bucket->tFinish = (unsigned int)(NOW_STEADY_TS_MS - tuneStartMonotonicBase);
		bucket->complete = true;
	}
}

/**
 * @brief Resetting the buckets
 */
void ProfileEventAAMP::ProfileReset(ProfilerBucketType type)
{
	struct ProfilerBucket *bucket = &buckets[type];
	bucket->complete = false;
	bucket->profileStarted = false;
	bucket->tFinish = 0;
	bucket->tStart = 0;
}

/**
 *  @brief Method to mark the end of a bucket, for which beginning is not marked
 */
void ProfileEventAAMP::ProfilePerformed(ProfilerBucketType type)
{
	ProfileBegin(type);
	buckets[type].complete = true;
}

/**
 *  @brief Method to set Failure code and Bucket Type used for microevents
 */
void ProfileEventAAMP::SetTuneFailCode(int tuneFailCode, ProfilerBucketType failBucketType)
{
	if(!mTuneFailErrorCode)
	{
		AAMPLOG_INFO("Tune Fail: ProfilerBucketType: %d, tuneFailCode: %d", failBucketType, tuneFailCode);
		mTuneFailErrorCode = tuneFailCode;
		mTuneFailBucketType = failBucketType;
	}
}

/**
 * @brief to mark the discontinuity switch and save the parameters
 */
void ProfileEventAAMP::SetDiscontinuityParam()
{
	ProfileEnd(PROFILE_BUCKET_DISCO_FIRST_FRAME);
	ProfileEnd(PROFILE_BUCKET_DISCO_TOTAL);
	std::lock_guard<std::mutex> lock(discontinuityParamMutex);
	if(telemetryParam)
	{
		cJSON* discontinuity = cJSON_AddArrayToObject(telemetryParam, "disc");
		if(discontinuity)
		{
			cJSON* item;
			unsigned int total = bucketDuration(PROFILE_BUCKET_DISCO_TOTAL);
			unsigned int flush = bucketDuration(PROFILE_BUCKET_DISCO_FLUSH);
			unsigned int fframe = bucketDuration(PROFILE_BUCKET_DISCO_FIRST_FRAME);
			unsigned int diff = total - (flush + fframe);
			cJSON_AddItemToArray(discontinuity, item = cJSON_CreateObject());
			cJSON_AddNumberToObject(item,"tt",total);
			cJSON_AddNumberToObject(item,"ft",flush);
			cJSON_AddNumberToObject(item,"fft",fframe);
			cJSON_AddNumberToObject(item,"d",diff);
			ProfileReset(PROFILE_BUCKET_DISCO_TOTAL);
			ProfileReset(PROFILE_BUCKET_DISCO_FLUSH);
			ProfileReset(PROFILE_BUCKET_DISCO_FIRST_FRAME);
		}
	}
}

/**
 * @brief API to add LLD specific Low buffer object 
 */
void ProfileEventAAMP::AddLLDLowBufferObject()
{
	if(telemetryParam)
	{
		if (mLldLowBuffObject == NULL)
		{
			mLldLowBuffObject = cJSON_AddArrayToObject(telemetryParam, "lldlb");
		}
	}
}

/**
 * @brief API to add double with pre defined precision
 */
void ProfileEventAAMP::AddWithPrecisionNumber(cJSON* item, const char* label, double num)
{
	if (item)
	{
		std::ostringstream streamObj;
		streamObj << std::fixed << std::setprecision(2) << num;/**< Set precision to 2 digits */
		std::string str = streamObj.str(); /**<  Converts double to string */
		AAMPLOG_TRACE("[DEBUG]string num = %s double num : %lf", str.c_str(), std::stod(str));
		//cJSON_AddStringToObject(item, label, str.c_str());
		cJSON_AddNumberToObject(item, label, std::stod(str));
		
	}
}


/**
 * @brief to mark the discontinuity switch and save the parameters
 */
void ProfileEventAAMP::SetLLDLowBufferParam(double latency, double buff, double rate, double bw, double buffLowCount)
{
	std::lock_guard<std::mutex> lock(discontinuityParamMutex);
	if(telemetryParam)
	{
		AddLLDLowBufferObject();
		if(mLldLowBuffObject)
		{
			cJSON* item;
			cJSON_AddItemToArray(mLldLowBuffObject, item = cJSON_CreateObject());
			AddWithPrecisionNumber(item,"lt",latency);
			AddWithPrecisionNumber(item,"buf",buff);
			AddWithPrecisionNumber(item,"pbr",rate);
			cJSON_AddNumberToObject(item,"bw",bw);
			cJSON_AddNumberToObject(item,"lbc",buffLowCount);
		}
		else
		{
			AAMPLOG_WARN("LLD Low buffer Object is not initialized !!");
		}
	}
}

/**
 * @brief to mark the latency parameters
 */
void ProfileEventAAMP::SetLatencyParam(double latency, double buffer, double playbackRate, double bw)
{
	std::lock_guard<std::mutex> lock(discontinuityParamMutex);
	if(latency >= 0)
	{
		AddWithPrecisionNumber(telemetryParam,"lt",latency);
	}
	if(buffer >= 0)
	{
		AddWithPrecisionNumber(telemetryParam,"buf",buffer);
	}
	AddWithPrecisionNumber(telemetryParam,"pbr", playbackRate);
	if(bw > 0)
	{
		cJSON_AddNumberToObject(telemetryParam,"bw", bw);
	}
	if(rateCorrection != 0)
	{
		cJSON_AddNumberToObject(telemetryParam,"rtc",rateCorrection);
		rateCorrection = 0;
	}
	if(bitrateChange != 0)
	{
		cJSON_AddNumberToObject(telemetryParam,"btc",bitrateChange);
		bitrateChange = 0;
	}
	if(bufferChange != 0)
	{
		cJSON_AddNumberToObject(telemetryParam,"bfc",bufferChange);
		bufferChange = 0;
	}
}

/**
 * @brief IncrementChangeCount - to increment the changes in buffer, ratecorrection and bitrate
 */
void ProfileEventAAMP::IncrementChangeCount(CountType type)
{
	if(type == Count_RateCorrection)
	{
		rateCorrection++;
	}
	else if(type == Count_BitrateChange)
	{
		bitrateChange++;
	}
	else if(type == Count_BufferChange)
	{
		bufferChange++;
	}
}

/**
 * @brief to log the telemetry parameters
 */
void ProfileEventAAMP::GetTelemetryParam()
{
	std::lock_guard<std::mutex> lock(discontinuityParamMutex);
	if(telemetryParam != NULL)
	{
		std::string jsonStr = cJSON_PrintUnformatted(telemetryParam);
		AAMPLOG_MIL("Telemetry values %s", jsonStr.c_str());
		cJSON_Delete(telemetryParam);
		mLldLowBuffObject = NULL;
		telemetryParam = cJSON_CreateObject();
	}
}

