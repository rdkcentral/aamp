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

#include "AampEvent.h"

AAMPEventObject::AAMPEventObject(AAMPEventType type, std::string sid) : mType(type), mSessionID{std::move(sid)}
{
}

const std::string &MediaErrorEvent::getDescription() const
{
	return mDescription;
}

void DrmMetaDataEvent::SetVerboseErrorCode(int32_t statusCode,  int32_t reasonCode, int32_t businessStatus )
{
}

void DrmMetaDataEvent::ConvertToVerboseErrorCode(int32_t httpCode, int32_t httpExtStatusCode )
{
}

void DrmMetaDataEvent::setAccessStatusValue(int value)
{
}

const std::string &MediaErrorEvent::getResponseData() const
{
	return mResponseData;
}

int32_t MediaErrorEvent::getReason( void ) const { return 0; }

AAMPEventType AAMPEventObject::getType() const
{
	return mType;
}

ID3MetadataEvent::ID3MetadataEvent(const std::vector<uint8_t> &metadata, const std::string &schIDUri, std::string &id3Value, uint32_t timeScale, uint64_t presentationTime, uint32_t eventDuration, uint32_t id, uint64_t timestampOffset, std::string sid):
		AAMPEventObject(AAMP_EVENT_ID3_METADATA, std::move(sid))
{
}

const std::vector<uint8_t> &ID3MetadataEvent::getMetadata() const
{
	return mMetadata;
}

int ID3MetadataEvent::getMetadataSize() const
{
	return 0;
}

uint32_t ID3MetadataEvent::getTimeScale() const
{
	return 0;
}

uint32_t ID3MetadataEvent::getId() const
{
	return 0;
}

uint64_t ID3MetadataEvent::getPresentationTime() const
{
	return 0;
}

uint64_t ID3MetadataEvent::getTimestampOffset() const
{
        return 0;
}

uint32_t ID3MetadataEvent::getEventDuration() const
{
	return 0;
}

const std::string& ID3MetadataEvent::getValue() const
{
	return mValue;
}

const std::string& ID3MetadataEvent::getSchemeIdUri() const
{
	return mSchemeIdUri;
}

MediaMetadataEvent::MediaMetadataEvent(long duration, int width, int height, bool hasDrm, bool isLive, const std::string &DrmType, double programStartTime, int tsbDepthMs, std::string sid, const std::string &url):
		AAMPEventObject(AAMP_EVENT_MEDIA_METADATA, std::move(sid))
{
}

void MediaMetadataEvent::addSupportedSpeed(float speed)
{
}

void MediaMetadataEvent::addBitrate(BitsPerSecond bitrate)
{
}

void MediaMetadataEvent::addLanguage(const std::string &lang)
{
}

const std::string &MediaMetadataEvent::getDrmType(void) const{return mDrmType;}
const std::string &MediaMetadataEvent::getUrl(void) const{return mUrl;}
const std::vector<BitsPerSecond> &MediaMetadataEvent::getBitrates(void) const{ return mBitrates; }
long MediaMetadataEvent::getDuration(void) const{ return 0; }
int MediaMetadataEvent::getTsbDepth(void) const{ return 0; }
const std::vector<std::string> & MediaMetadataEvent::getLanguages(void) const{ return mLanguages; }
int MediaMetadataEvent::getBitratesCount(void) const{ return 0; }
int MediaMetadataEvent::getLanguagesCount(void) const{ return 0; }
const std::vector<float> &MediaMetadataEvent::getSupportedSpeeds(void) const{ return mSupportedSpeeds; }
double MediaMetadataEvent::getProgramStartTime(void) const{ return 0.0; }
int MediaMetadataEvent::getSupportedSpeedCount(void) const{ return 0; }
bool MediaMetadataEvent::hasDrm(void) const{ return false; }
bool MediaMetadataEvent::isLive(void) const{ return false;  }
int MediaMetadataEvent::getWidth(void) const{ return 0; }
int MediaMetadataEvent::getHeight(void) const{ return 0; }

DrmMetaDataEvent::DrmMetaDataEvent(AAMPTuneFailure failure, const std::string &accessStatus, int statusValue, int responseCode, bool secclientErr, std::string sid):
    AAMPEventObject(AAMP_EVENT_DRM_METADATA, std::move(sid))
{
}

void DrmMetaDataEvent::setFailure(AAMPTuneFailure failure)
{	
}

void DrmMetaDataEvent::setResponseCode(int code)
{
}

void DrmMetaDataEvent::setSecclientError(bool secClientError)
{
}

void DrmMetaDataEvent::setHeaderResponses(const std::vector<std::string> &responses)
{
}

void DrmMetaDataEvent::setBodyResponse(const std::string &responses)
{
}

void DrmMetaDataEvent::setSecManagerReasonCode(int32_t code)
{
}

AAMPTuneFailure DrmMetaDataEvent::getFailure() const
{
	return AAMP_TUNE_INIT_FAILED;
}

int DrmMetaDataEvent::getResponseCode() const
{
    return 0;
}

const std::string &DrmMetaDataEvent::getAccessStatus() const
{
	return mAccessStatus;
}

void DrmMetaDataEvent::setAccessStatus(const std::string &status)
{
}

const std::string &DrmMetaDataEvent::getResponseData() const
{
	return mResponseData;
}

void DrmMetaDataEvent::setResponseData(const std::string &data)
{
}

const std::string &DrmMetaDataEvent::getNetworkMetricData() const
{
	return mNetworkMetrics;
}

void DrmMetaDataEvent::setNetworkMetricData(const std::string &data)
{
}

int DrmMetaDataEvent::getAccessStatusValue() const
{
	return 0;
}

bool DrmMetaDataEvent::getSecclientError() const
{
	return false;
}

int32_t DrmMetaDataEvent::getSecManagerReasonCode() const
{
	return 0;
}

int32_t DrmMetaDataEvent::getSecManagerClassCode() const
{
	return 0;
}

int32_t DrmMetaDataEvent::getBusinessStatus() const
{
	return 0;
}

DrmMessageEvent::DrmMessageEvent(const std::string &msg, std::string sid):
		AAMPEventObject(AAMP_EVENT_DRM_MESSAGE, std::move(sid))
{
}

const std::string &DrmMessageEvent::getMessage() const { return mMessage; }

const std::vector<std::string> &DrmMetaDataEvent::getHeaderResponses( void ) const { return mHeaderResponses; }

const std::string &DrmMetaDataEvent::getBodyResponse( void ) const { return mBodyResponses; }

AnomalyReportEvent::AnomalyReportEvent(int severity, const std::string &msg, std::string sid):
		AAMPEventObject(AAMP_EVENT_REPORT_ANOMALY, std::move(sid))
{
}
const std::string &AnomalyReportEvent::getMessage( void ) const { return mMsg; }
int AnomalyReportEvent::getSeverity() const
{
	return 0;
}

BufferingChangedEvent::BufferingChangedEvent(bool buffering, std::string sid):
		AAMPEventObject(AAMP_EVENT_BUFFERING_CHANGED, std::move(sid))
{
}

bool BufferingChangedEvent::buffering() const
{
	return false;
}

ProgressEvent::ProgressEvent(double duration, double position, double start, double end, float speed, long long pts, double videoBufferedDuration, double audioBufferedDuration, std::string seiTimecode,double liveLatency, long profileBandwidth, long networkBandwidth, double currentPlayRate, std::string sid):
		AAMPEventObject(AAMP_EVENT_PROGRESS, std::move(sid))
{
}

double ProgressEvent::getDuration(void) const{ return 0.0; }
double ProgressEvent::getPosition(void) const{ return 0.0; }
double ProgressEvent::getLiveLatency(void) const{ return 0.0; }
const char* ProgressEvent::getSEITimeCode(void) const{ return NULL; }
double ProgressEvent::getCurrentPlayRate(void) const{ return 0.0; }
double ProgressEvent::getVideoBufferedDuration(void) const{ return 0.0; }
double ProgressEvent::getAudioBufferedDuration(void) const{ return 0.0; }
long ProgressEvent::getNetworkBandwidth(void) const{ return 0; }
long ProgressEvent::getProfileBandwidth(void) const{ return 0; }
double ProgressEvent::getEnd(void) const{ return 0.0; }
long long ProgressEvent::getPTS(void) const{ return 0; }
float ProgressEvent::getSpeed(void) const{ return 0.0; }
double ProgressEvent::getStart(void) const{ return 0.0; }

SpeedChangedEvent::SpeedChangedEvent(float rate, std::string sid):
		AAMPEventObject(AAMP_EVENT_SPEED_CHANGED, std::move(sid))
{
	mRate = rate;
}

float SpeedChangedEvent::getRate() const
{
	return mRate;
}

TimedMetadataEvent::TimedMetadataEvent(const std::string &name, const std::string &id, double time, double duration, const std::string &content, std::string sid):
		AAMPEventObject(AAMP_EVENT_TIMED_METADATA, std::move(sid))
{
}

const std::string &TimedMetadataEvent::getContent( void ) const{ return mContent; }
double TimedMetadataEvent::getDuration( void  ) const{ return 0.0; }
const std::string & TimedMetadataEvent::getId( void ) const{ return mId; }
const std::string & TimedMetadataEvent::getName( void ) const{ return mName; }
double TimedMetadataEvent::getTime( void ) const{ return 0.0; }
const std::string & TuneProfilingEvent::getProfilingData( void ) const{ return mProfilingData; }

CCHandleEvent::CCHandleEvent(unsigned long handle, std::string sid):
		AAMPEventObject(AAMP_EVENT_CC_HANDLE_RECEIVED, std::move(sid)), mHandle(handle)
{
}

unsigned long CCHandleEvent::getCCHandle() const
{
	return mHandle;
}

SupportedSpeedsChangedEvent::SupportedSpeedsChangedEvent(std::string sid):
		AAMPEventObject(AAMP_EVENT_SPEEDS_CHANGED, std::move(sid))
{
}

void SupportedSpeedsChangedEvent::addSupportedSpeed(float speed)
{
}

const std::vector<float> &SupportedSpeedsChangedEvent::getSupportedSpeeds() const
{
	return mSupportedSpeeds;
}

int SupportedSpeedsChangedEvent::getSupportedSpeedCount() const
{
	return 0;
}

MediaErrorEvent::MediaErrorEvent(AAMPTuneFailure failure, int code, const std::string &desc, bool shouldRetry, int classCode, int reason, int businessStatus, const std::string &responseData, std::string sid):
		AAMPEventObject(AAMP_EVENT_TUNE_FAILED, std::move(sid))
{
}
bool MediaErrorEvent::shouldRetry(void) const { return false; }
int32_t MediaErrorEvent::getBusinessStatus(void) const { return 0; }
int32_t MediaErrorEvent::getClass(void) const { return 0; }
int MediaErrorEvent::getCode(void) const { return 0; }

BitrateChangeEvent::BitrateChangeEvent(int time, BitsPerSecond bitrate, const std::string &desc, int width, int height, double frameRate, double position, bool cappedProfile, int displayWidth, int displayHeight, VideoScanType videoScanType, int aspectRatioWidth, int aspectRatioHeight, std::string sid):
		AAMPEventObject(AAMP_EVENT_BITRATE_CHANGED, std::move(sid))
{
}
BitsPerSecond BitrateChangeEvent::getBitrate( void ) const { return 0; }
double BitrateChangeEvent::getPosition( void ) const { return 0.0; }
const std::string &BitrateChangeEvent::getDescription() const { return mDescription; }
int BitrateChangeEvent::getDisplayWidth() const { return 0; }
int BitrateChangeEvent::getDisplayHeight() const { return 0; }
VideoScanType BitrateChangeEvent::getScanType() const { return mVideoScanType; }
double BitrateChangeEvent::getFrameRate( void ) const { return 0.0; }
int BitrateChangeEvent::getAspectRatioWidth() const { return 0; }
int BitrateChangeEvent::getAspectRatioHeight() const { return 0; }
bool BitrateChangeEvent::getCappedProfileStatus() const { return false; }
int BitrateChangeEvent::getTime( void ) const { return 0; }
int BitrateChangeEvent::getWidth( void ) const { return 0; }
int BitrateChangeEvent::getHeight( void ) const { return 0; }

BulkTimedMetadataEvent::BulkTimedMetadataEvent(const std::string &content, std::string sid):
		AAMPEventObject(AAMP_EVENT_BULK_TIMED_METADATA, std::move(sid))
{
}
const std::string &BulkTimedMetadataEvent::getContent() const { return mContent; }

StateChangedEvent::StateChangedEvent(AAMPPlayerState state, std::string sid):
		AAMPEventObject(AAMP_EVENT_STATE_CHANGED, std::move(sid))
{
	mState = state;
}

AAMPPlayerState StateChangedEvent::getState() const
{
	return mState;
}

SeekedEvent::SeekedEvent(double positionMS, std::string sid):
		AAMPEventObject(AAMP_EVENT_SEEKED, std::move(sid))
{
}
double SeekedEvent::getPosition( void ) const { return 0.0; }


TuneProfilingEvent::TuneProfilingEvent(std::string &profilingData, std::string sid):
		AAMPEventObject(AAMP_EVENT_TUNE_PROFILING, std::move(sid))
{
}

AdResolvedEvent::AdResolvedEvent(bool resolveStatus, const std::string &adId, uint64_t startMS, uint64_t durationMs, const std::string &errorCode, const std::string &errorDescription, std::string sid):
		AAMPEventObject(AAMP_EVENT_AD_RESOLVED, std::move(sid))
{
	mResolveStatus = resolveStatus;
	mAdId = adId;
	mErrorCode = errorCode;
	mErrorDescription = errorDescription;
}

AdReservationEvent::AdReservationEvent(AAMPEventType evtType, const std::string &breakId, uint64_t position, uint64_t absolutePositionMs, std::string sid):
		AAMPEventObject(evtType, std::move(sid))
{
}

uint64_t AdReservationEvent::getAbsolutePositionMs() const
{
	return 0;
}
uint64_t AdReservationEvent::getPosition( void ) const { return 0; }
const std::string &AdReservationEvent::getAdBreakId() const { return mAdBreakId; }

bool AdResolvedEvent::getResolveStatus() const { return mResolveStatus; }
const std::string &AdResolvedEvent::getAdId() const { return mAdId; }
uint64_t AdResolvedEvent::getStart() const { return mStartMS; }
uint64_t AdResolvedEvent::getDuration(void) const { return mDurationMs; }
const std::string &AdResolvedEvent::getErrorCode() const { return mErrorCode; }
const std::string &AdResolvedEvent::getErrorDescription() const { return mErrorDescription; }

AdPlacementEvent::AdPlacementEvent(AAMPEventType evtType, const std::string &adId, uint32_t position, uint64_t absolutePositionMs, std::string sid, uint32_t offset, uint32_t duration, int errorCode):
		AAMPEventObject(evtType, std::move(sid))
{
}
const std::string &AdPlacementEvent::getAdId() const { return mAdId; }
uint32_t AdPlacementEvent::getPosition( void ) const { return 0; }
uint64_t AdPlacementEvent::getAbsolutePositionMs() const { return 0; }
int AdPlacementEvent::getErrorCode( void ) const { return 0; }

uint32_t AdPlacementEvent::getOffset() const
{
	return 0;
}

uint32_t AdPlacementEvent::getDuration() const
{
	return 0;
}

WebVttCueEvent::WebVttCueEvent(VTTCue* cueData, std::string sid):
		AAMPEventObject(AAMP_EVENT_WEBVTT_CUE_DATA, std::move(sid))
{
}
VTTCue* WebVttCueEvent::getCueData(void) const { return NULL; }

ContentGapEvent::ContentGapEvent(double time, double duration, std::string sid):
		AAMPEventObject(AAMP_EVENT_CONTENT_GAP, std::move(sid))
{
}
double ContentGapEvent::getDuration(void) const { return 0.0; }
double ContentGapEvent::getTime(void) const { return 0.0; }

HTTPResponseHeaderEvent::HTTPResponseHeaderEvent(const std::string &header, const std::string &response, std::string sid):
		AAMPEventObject(AAMP_EVENT_HTTP_RESPONSE_HEADER, std::move(sid))
{
}

const std::string &HTTPResponseHeaderEvent::getHeader() const
{
	return mHeaderName;
}

const std::string &HTTPResponseHeaderEvent::getResponse() const
{
	return mHeaderResponse;
}

ContentProtectionDataEvent::ContentProtectionDataEvent(const std::vector<uint8_t> &keyID, const std::string &streamType, std::string sid):
	AAMPEventObject(AAMP_EVENT_CONTENT_PROTECTION_DATA_UPDATE, std::move(sid))
{
}
const std::string &ContentProtectionDataEvent::getStreamType() const { return mStreamType; }
const std::vector<uint8_t> &ContentProtectionDataEvent::getKeyID() const { return mKeyID; }

/*
 * @brief ManifestRefreshEvent Constructor
 */
ManifestRefreshEvent::ManifestRefreshEvent(uint32_t manifestDuration,int noOfPeriods, uint32_t manifestPublishedTime, const std::string &manifestType, std::string sid):
	AAMPEventObject(AAMP_EVENT_MANIFEST_REFRESH_NOTIFY, std::move(sid))
	, mManifestDuration(manifestDuration),mNoOfPeriods(noOfPeriods),mManifestPublishedTime(manifestPublishedTime)
{

}

/**
 * @brief Get ManifestFile Duration for Linear DASH
 *
 * @return ManifestFile Duration
 */
uint32_t ManifestRefreshEvent::getManifestDuration() const
{
   return mManifestDuration;
}

/**
 * @brief Get ManifestType
 *
 * @return ManifestType
 */
const std::string& ManifestRefreshEvent::getManifestType() const
{
   return mManifestType;
}

/**
 * @brief Get No of Periods for Linear DASH
 *
 * @return NoOfPeriods
 */
uint32_t ManifestRefreshEvent::getNoOfPeriods() const
{
   return mNoOfPeriods;
}

/**
 * @brief Get ManifestPublishedTime for Linear DASH
 *
 * @return ManifestFile PublishedTime
 */
uint32_t ManifestRefreshEvent::getManifestPublishedTime() const
{
   return mManifestPublishedTime;
}



TuneTimeMetricsEvent::TuneTimeMetricsEvent(const std::string &timeMetricData, std::string sid):
	AAMPEventObject(AAMP_EVENT_TUNE_TIME_METRICS, std::move(sid))
{

}

const std::string &TuneTimeMetricsEvent::getTuneMetricsData() const
{
		return mTuneMetricsData;
}

void MediaMetadataEvent::SetAudioMetaData(const std::string &audioCodec,const std::string &mixType,bool  isAtmos  )
{
}

void MediaMetadataEvent::SetVideoMetaData(float frameRate,VideoScanType videoScanType,int aspectRatioWidth,int  aspectRatioHeight, const std::string &  videoCodec, const std::string  & strHdrType, const std::string & strPCRating, int ssi)
{
}

/**
 * @brief MetricsDataEvent Constructor
 */
MetricsDataEvent::MetricsDataEvent(MetricsDataType dataType, const std::string &uuid, const std::string &data, std::string sid):
		AAMPEventObject(AAMP_EVENT_REPORT_METRICS_DATA, std::move(sid))
{
}

MetricsDataType MetricsDataEvent::getMetricsDataType() const { return mMetricsDataType; }
const std::string &MetricsDataEvent::getMetricUUID() const { return mMetricUUID; }
const std::string &MetricsDataEvent::getMetricsData() const { return mMetricsData; }

/**
 * @fn MonitorAVStatusEvent Constructor                                                                                               */
MonitorAVStatusEvent::MonitorAVStatusEvent(const std::string &status, int64_t videoPositionMS, int64_t audioPositionMS, uint64_t timeInStateMS, std::string sid, uint64_t droppedFrames):
		AAMPEventObject(AAMP_EVENT_MONITORAV_STATUS, std::move(sid)), mMonitorAVStatus(status), mVideoPositionMS(videoPositionMS),
		mAudioPositionMS(audioPositionMS), mTimeInStateMS(timeInStateMS), mDroppedFrames(droppedFrames)
{
}

/**
 * @brief getMonitorAVStatus
 *
 * @return MonitorAVStatus
 */
const std::string &MonitorAVStatusEvent::getMonitorAVStatus() const
{
	return mMonitorAVStatus;
}

/**
 * @brief getVideoPositionMS
 *
 * @return Video Position in MS
 */
int64_t MonitorAVStatusEvent::getVideoPositionMS() const
{
	return mVideoPositionMS;
}

/**
 * @brief getAudioPositionMS
 *
 * @return Audio Position in MS
 */
int64_t MonitorAVStatusEvent::getAudioPositionMS() const
{
	return mAudioPositionMS;
}

/**
 * @brief getTimeInStateMS
 *
 * @return Time in the current state in MS
 */
uint64_t MonitorAVStatusEvent::getTimeInStateMS() const
{
	return mTimeInStateMS;
}

/**
 * @brief getDroppedFrames
 *
 * @return Dropped Frames Count
 */
uint64_t MonitorAVStatusEvent::getDroppedFrames() const
{
	return mDroppedFrames;
}