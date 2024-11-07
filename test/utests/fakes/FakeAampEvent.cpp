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

const std::string &MediaErrorEvent::getResponseData() const
{
	return mResponseData;
}

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

MediaMetadataEvent::MediaMetadataEvent(long duration, int width, int height, bool hasDrm, bool isLive, const std::string &DrmType, double programStartTime, int tsbDepthMs, std::string sid):
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

AnomalyReportEvent::AnomalyReportEvent(int severity, const std::string &msg, std::string sid):
		AAMPEventObject(AAMP_EVENT_REPORT_ANOMALY, std::move(sid))
{
}

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

ProgressEvent::ProgressEvent(double duration, double position, double start, double end, float speed, long long pts, double bufferedDuration, std::string seiTimecode,double liveLatency, long profileBandwidth, long networkBandwidth, double currentPlayRate, std::string sid):
		AAMPEventObject(AAMP_EVENT_PROGRESS, std::move(sid))
{
}

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

BitrateChangeEvent::BitrateChangeEvent(int time, BitsPerSecond bitrate, const std::string &desc, int width, int height, double frameRate, double position, bool cappedProfile, int displayWidth, int displayHeight, VideoScanType videoScanType, int aspectRatioWidth, int aspectRatioHeight, std::string sid):
		AAMPEventObject(AAMP_EVENT_BITRATE_CHANGED, std::move(sid))
{
}

BulkTimedMetadataEvent::BulkTimedMetadataEvent(const std::string &content, std::string sid):
		AAMPEventObject(AAMP_EVENT_BULK_TIMED_METADATA, std::move(sid))
{
}

StateChangedEvent::StateChangedEvent(PrivAAMPState state, std::string sid):
		AAMPEventObject(AAMP_EVENT_STATE_CHANGED, std::move(sid))
{
	mState = state;
}

PrivAAMPState StateChangedEvent::getState() const
{
	return mState;
}

SeekedEvent::SeekedEvent(double positionMS, std::string sid):
		AAMPEventObject(AAMP_EVENT_SEEKED, std::move(sid))
{
}

TuneProfilingEvent::TuneProfilingEvent(std::string &profilingData, std::string sid):
		AAMPEventObject(AAMP_EVENT_TUNE_PROFILING, std::move(sid))
{
}

AdResolvedEvent::AdResolvedEvent(bool resolveStatus, const std::string &adId, uint64_t startMS, uint64_t durationMs, std::string sid):
		AAMPEventObject(AAMP_EVENT_AD_RESOLVED, std::move(sid))
{
}

AdReservationEvent::AdReservationEvent(AAMPEventType evtType, const std::string &breakId, uint64_t position, uint64_t absolutePositionMs, std::string sid):
		AAMPEventObject(evtType, std::move(sid))
{
}

uint64_t AdReservationEvent::getAbsolutePositionMs() const
{
	return 0;
}

AdPlacementEvent::AdPlacementEvent(AAMPEventType evtType, const std::string &adId, uint32_t position, uint64_t absolutePositionMs, std::string sid, uint32_t offset, uint32_t duration, int errorCode):
		AAMPEventObject(evtType, std::move(sid))
{
}

const std::string &AdPlacementEvent::getAdId() const
{
	return mAdId;
}

uint32_t AdPlacementEvent::getPosition() const
{
	return 0;
}

uint64_t AdPlacementEvent::getAbsolutePositionMs() const
{
	return 0;
}

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

ContentGapEvent::ContentGapEvent(double time, double duration, std::string sid):
		AAMPEventObject(AAMP_EVENT_CONTENT_GAP, std::move(sid))
{
}

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

/*
 * @brief ManifestRefreshEvent Constructor
 */
ManifestRefreshEvent::ManifestRefreshEvent(uint32_t manifestDuration,int noOfPeriods, uint32_t manifestPublishedTime, std::string sid):
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
 * @brief Get No of Periods for Linear DASH
 *
 * @return NoOfPeriods
 */
uint32_t ManifestRefreshEvent::getNoOfPeriods() const
{
   return mNoOfPeriods;
}

/**
 * @brief Get ManifestPuablishedTime for Linear DASH
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

/**
 * @brief MetricsDataEvent Constructor
 */
MetricsDataEvent::MetricsDataEvent(MetricsDataType dataType, const std::string &uuid, const std::string &data, std::string sid):
		AAMPEventObject(AAMP_EVENT_REPORT_METRICS_DATA, std::move(sid))
{
}
