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

#include "fragmentcollector_mpd.h"
#include "MockStreamAbstractionAAMP_MPD.h"

MockStreamAbstractionAAMP_MPD *g_mockStreamAbstractionAAMP_MPD = nullptr;

StreamAbstractionAAMP_MPD::StreamAbstractionAAMP_MPD(class PrivateInstanceAAMP *aamp,double seek_pos, float rate, id3_callback_t id3Handler)
    : StreamAbstractionAAMP(aamp), mMinUpdateDurationMs(DEFAULT_INTERVAL_BETWEEN_MPD_UPDATES_MS)
{
}

StreamAbstractionAAMP_MPD::~StreamAbstractionAAMP_MPD()
{
}

Accessibility StreamAbstractionAAMP_MPD::getAccessibilityNode(AampJsonObject &accessNode)
{
   	Accessibility accessibilityNode;
    return accessibilityNode;
}

AAMPStatusType StreamAbstractionAAMP_MPD::Init(TuneType tuneType)
{
    if (g_mockStreamAbstractionAAMP_MPD)
    {
        return g_mockStreamAbstractionAAMP_MPD->Init(tuneType);
    }
    return eAAMPSTATUS_OK;
}

AAMPStatusType StreamAbstractionAAMP_MPD::InitTsbReader(TuneType tuneType)
{
    AAMPStatusType status = eAAMPSTATUS_OK;
    AAMPLOG_WARN("g_mockStreamAbstractionAAMP_MPD = %p", g_mockStreamAbstractionAAMP_MPD);
    if (g_mockStreamAbstractionAAMP_MPD)
    {
        status = g_mockStreamAbstractionAAMP_MPD->InitTsbReader(tuneType);
    }
    return status;
}

void StreamAbstractionAAMP_MPD::Start() {  }

void StreamAbstractionAAMP_MPD::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_MPD::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_MPD::GetFirstPTS() { return 0; }

double StreamAbstractionAAMP_MPD::GetMidSeekPosOffset() { 

    if (g_mockStreamAbstractionAAMP_MPD)
    {
        return g_mockStreamAbstractionAAMP_MPD->GetMidSeekPosOffset();
    }

    return 0;
}

double StreamAbstractionAAMP_MPD::GetStartTimeOfFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_MPD::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_MPD::GetBufferedDuration (void) { return 0; }

int StreamAbstractionAAMP_MPD::GetBWIndex( BitsPerSecond bandwidth) { return 0; }

std::vector<BitsPerSecond> StreamAbstractionAAMP_MPD::GetVideoBitrates(void) { std::vector<BitsPerSecond> temp; return temp; }

std::vector<BitsPerSecond> StreamAbstractionAAMP_MPD::GetAudioBitrates(void) { std::vector<BitsPerSecond> temp; return temp; }

void StreamAbstractionAAMP_MPD::StopInjection(void) {  }

void StreamAbstractionAAMP_MPD::StartInjection(void) {  }

std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableVideoTracks(void) { std::vector<StreamInfo*> temp; return temp; }

std::vector<StreamInfo*> StreamAbstractionAAMP_MPD::GetAvailableThumbnailTracks(void) { std::vector<StreamInfo*> temp; return temp; }

bool StreamAbstractionAAMP_MPD::SetThumbnailTrack(int) { return false; }

std::vector<ThumbnailData> StreamAbstractionAAMP_MPD::GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) { std::vector<ThumbnailData> temp; return temp; }

StreamInfo* StreamAbstractionAAMP_MPD::GetStreamInfo(int idx) { return nullptr; }

double StreamAbstractionAAMP_MPD::GetFirstPeriodStartTime(void)
{
    return 0.0;
}

uint32_t StreamAbstractionAAMP_MPD::GetCurrPeriodTimeScale()
{
    return 0;
}

int StreamAbstractionAAMP_MPD::GetProfileCount()
{
    return 0;
}

int StreamAbstractionAAMP_MPD::GetProfileIndexForBandwidth(BitsPerSecond mTsbBandwidth)
{
    return 0;
}

BitsPerSecond StreamAbstractionAAMP_MPD::GetMaxBitrate()
{
    if (g_mockStreamAbstractionAAMP_MPD)
    {
        return g_mockStreamAbstractionAAMP_MPD->GetMaxBitrate();
    }
    else
    {
        return 0;
    }
}

void StreamAbstractionAAMP_MPD::StartSubtitleParser()
{
}

void StreamAbstractionAAMP_MPD::PauseSubtitleParser(bool pause)
{
}

void StreamAbstractionAAMP_MPD::SetCDAIObject(CDAIObject *cdaiObj)
{
}

std::vector<AudioTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableAudioTracks(bool allTrack)
{
    return mAudioTracksAll;
}

std::vector<TextTrackInfo>& StreamAbstractionAAMP_MPD::GetAvailableTextTracks(bool allTrack)
{
    return mTextTracksAll;
}

void StreamAbstractionAAMP_MPD::InitSubtitleParser(char *data)
{
}

void StreamAbstractionAAMP_MPD::ResetSubtitle()
{
}

void StreamAbstractionAAMP_MPD::MuteSubtitleOnPause()
{
}

void StreamAbstractionAAMP_MPD::ResumeSubtitleOnPlay(bool mute, char *data)
{
}

void  StreamAbstractionAAMP_MPD::ResumeSubtitleAfterSeek(bool mute, char *data)
{
}

void StreamAbstractionAAMP_MPD::MuteSidecarSubtitles(bool mute)
{
}

bool StreamAbstractionAAMP_MPD::Is4KStream(int &height, BitsPerSecond &bandwidth)
{
    return false;
}

bool StreamAbstractionAAMP_MPD::SetTextStyle(const std::string &options)
{
    return false;
}

void StreamAbstractionAAMP_MPD::UpdateFailedDRMStatus(LicensePreFetchObject *object)
{
}

void StreamAbstractionAAMP_MPD::QueueContentProtection(IPeriod* period, uint32_t adaptationSetIdx, AampMediaType mediaType, bool qGstProtectEvent, bool isVssPeriod)
{
}

void StreamAbstractionAAMP_MPD::ProcessAllContentProtectionForMediaType(AampMediaType type, uint32_t priorityAdaptationIdx, std::set<uint32_t> &chosenAdaptationIdxs)
{
}

const IAdaptationSet* StreamAbstractionAAMP_MPD::GetAdaptationSetAtIndex(int idx)
{
    return NULL;
}

dash::mpd::IMPD *StreamAbstractionAAMP_MPD::GetMPD( void )
{
	return mpd;
}

IPeriod *StreamAbstractionAAMP_MPD::GetPeriod( void )
{
	IPeriod *period = nullptr;
	if (g_mockStreamAbstractionAAMP_MPD)
	{
		period = g_mockStreamAbstractionAAMP_MPD->GetPeriod();
	}
	return period;
}

ProfileInfo StreamAbstractionAAMP_MPD::GetAdaptationSetAndRepresentationIndicesForProfile(int profileIndex)
{
    return mProfileMaps.at(0);
}

void StreamAbstractionAAMP_MPD::SeekPosUpdate(double secondsRelativeToTuneTime)
{
    if (g_mockStreamAbstractionAAMP_MPD)
    {
        g_mockStreamAbstractionAAMP_MPD->SeekPosUpdate(secondsRelativeToTuneTime);
    }
}

double StreamAbstractionAAMP_MPD::GetStreamPosition()
{
	double position = 0;
	if (g_mockStreamAbstractionAAMP_MPD)
	{
		position = g_mockStreamAbstractionAAMP_MPD->GetStreamPosition();
	}

	return position;
}

void StreamAbstractionAAMP_MPD::NotifyFirstVideoPTS(unsigned long long, unsigned long)
{
}

uint32_t StreamAbstractionAAMP_MPD::GetSegmentRepeatCount(MediaStreamContext *pMediaStreamContext, int timeLineIndex)
{
	return 0;
}

void StreamAbstractionAAMP_MPD::SetSubtitleTrackOffset()
{
}

double StreamAbstractionAAMP_MPD::GetAvailabilityStartTime()
{
    return 0.0;
}

void StreamAbstractionAAMP_MPD::RefreshTrack(AampMediaType type)
{
}

void StreamAbstractionAAMP_MPD::CheckAdResolvedStatus(AdNodeVectorPtr &ads, int adIdx, const std::string &periodId)
{
}

bool StreamAbstractionAAMP_MPD::UseIframeTrack(void)
{
	return true;
}

void StreamAbstractionAAMP_MPD::TsbReader()
{
    
}
bool StreamAbstractionAAMP_MPD::DoEarlyStreamSinkFlush(bool newTune, float rate)
{
	bool shouldFlush = false;
	if (g_mockStreamAbstractionAAMP_MPD)
	{
		shouldFlush = g_mockStreamAbstractionAAMP_MPD->DoEarlyStreamSinkFlush(newTune, rate);
	}
	return shouldFlush;
}
bool StreamAbstractionAAMP_MPD::DoStreamSinkFlushOnDiscontinuity() { return false; }

void StreamAbstractionAAMP_MPD::clearFirstPTS(void)
{

}

bool StreamAbstractionAAMP_MPD::ExtractAndAddSubtitleMediaHeader()
{
	return false;
}
