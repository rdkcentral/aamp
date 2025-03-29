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

#include "fragmentcollector_hls.h"

StreamAbstractionAAMP_HLS::StreamAbstractionAAMP_HLS(class PrivateInstanceAAMP *aamp,double seekpos, float rate,
			id3_callback_t id3Handler,
			ptsoffset_update_t ptsOffsetUpdate)
    : StreamAbstractionAAMP(aamp), mainManifest("mainManifest"), thumbnailManifest("thumbnailManifest")
{
}

StreamAbstractionAAMP_HLS::~StreamAbstractionAAMP_HLS()
{
}

AAMPStatusType StreamAbstractionAAMP_HLS::Init(TuneType tuneType) { return eAAMPSTATUS_OK; }

void StreamAbstractionAAMP_HLS::Start() {  }

void StreamAbstractionAAMP_HLS::Stop(bool clearChannelData) {  }

void StreamAbstractionAAMP_HLS::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat) {  }

double StreamAbstractionAAMP_HLS::GetFirstPTS() { return 0; }

MediaTrack* StreamAbstractionAAMP_HLS::GetMediaTrack(TrackType type) { return nullptr; }

double StreamAbstractionAAMP_HLS::GetBufferedDuration (void) { return 0; }

int StreamAbstractionAAMP_HLS::GetBWIndex(BitsPerSecond bandwidth) { return 0; }

std::vector<BitsPerSecond> StreamAbstractionAAMP_HLS::GetVideoBitrates(void) { std::vector<BitsPerSecond> temp; return temp; }

void StreamAbstractionAAMP_HLS::StopInjection(void) {  }

void StreamAbstractionAAMP_HLS::StartInjection(void) {  }

std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableVideoTracks(void) { std::vector<StreamInfo*> temp; return temp; }

std::vector<StreamInfo*> StreamAbstractionAAMP_HLS::GetAvailableThumbnailTracks(void) { std::vector<StreamInfo*> temp; return temp; }

bool StreamAbstractionAAMP_HLS::SetThumbnailTrack(int) { return false; }

std::vector<ThumbnailData> StreamAbstractionAAMP_HLS::GetThumbnailRangeData(double, double, std::string*, int*, int*, int*, int*) { std::vector<ThumbnailData> temp; return temp; }

StreamInfo* StreamAbstractionAAMP_HLS::GetStreamInfo(int idx) { return nullptr; }

void StreamAbstractionAAMP_HLS::StartSubtitleParser() { }

void StreamAbstractionAAMP_HLS::PauseSubtitleParser(bool pause) { }

void StreamAbstractionAAMP_HLS::NotifyFirstVideoPTS(unsigned long long pts, unsigned long timeScale){ }

void StreamAbstractionAAMP_HLS::SeekPosUpdate(double secondsRelativeToTuneTime) { }

void StreamAbstractionAAMP_HLS::ChangeMuxedAudioTrackIndex(std::string& index) { }

bool StreamAbstractionAAMP_HLS::Is4KStream(int &height, BitsPerSecond &bandwidth) { return false; }

StreamAbstractionAAMP::ABRMode StreamAbstractionAAMP_HLS::GetABRMode() { return StreamAbstractionAAMP::ABRMode::ABR_MANAGER; }

void StreamAbstractionAAMP_HLS::RefreshTrack(AampMediaType type) { }

bool StreamAbstractionAAMP_HLS::SelectPreferredTextTrack(TextTrackInfo& selectedTextTrack) { return true; }
