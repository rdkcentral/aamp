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

#include "StreamAbstractionAAMP.h"
#include "MockStreamAbstractionAAMP.h"

MockStreamAbstractionAAMP *g_mockStreamAbstractionAAMP = nullptr;

StreamAbstractionAAMP::StreamAbstractionAAMP(PrivateInstanceAAMP* aamp, id3_callback_t mID3Handler) : aamp(nullptr), mAudiostateChangeCount(0), mESChangeStatus(false)
{
}

StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
}

void StreamAbstractionAAMP::DisablePlaylistDownloads()
{
}

bool StreamAbstractionAAMP::IsMuxedStream()
{
	return false;
}

double StreamAbstractionAAMP::GetLastInjectedFragmentPosition()
{
	return 0.0;
}

double StreamAbstractionAAMP::GetBufferedVideoDurationSec()
{
	return 0.0;
}

void StreamAbstractionAAMP::MuteSubtitles(bool mute)
{
	if (g_mockStreamAbstractionAAMP != nullptr)
	{
		g_mockStreamAbstractionAAMP->MuteSubtitles(mute);
	}
}

void StreamAbstractionAAMP::ResetTrickModePtsRestamping()
{
}

void StreamAbstractionAAMP::RefreshSubtitles()
{
}

long StreamAbstractionAAMP::GetVideoBitrate(void)
{
	return 0;
}

void StreamAbstractionAAMP::SetAudioTrackInfoFromMuxedStream(std::vector<AudioTrackInfo>& vector)
{
}

long StreamAbstractionAAMP::GetAudioBitrate(void)
{
	return 0;
}

int StreamAbstractionAAMP::GetAudioTrack()
{
	return 0;
}

bool StreamAbstractionAAMP::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
	return false;
}

int StreamAbstractionAAMP::GetTextTrack()
{
	return 0;
}

bool StreamAbstractionAAMP::GetCurrentTextTrack(TextTrackInfo &textTrack)
{
	return 0;
}

bool StreamAbstractionAAMP::isInBandCcAvailable()
{
	return 0;
}

bool StreamAbstractionAAMP::IsInitialCachingSupported()
{
	return false;
}

void StreamAbstractionAAMP::NotifyPlaybackPaused(bool paused)
{
	if (g_mockStreamAbstractionAAMP != nullptr)
	{
		g_mockStreamAbstractionAAMP->NotifyPlaybackPaused(paused);
	}
}

bool StreamAbstractionAAMP::IsEOSReached()
{
	bool eos = false;
	if (g_mockStreamAbstractionAAMP != nullptr)
	{
		eos = g_mockStreamAbstractionAAMP->IsEOSReached();
	}
	return eos;
}

bool StreamAbstractionAAMP::GetPreferredLiveOffsetFromConfig()
{
	return false;
}

void MediaTrack::OnSinkBufferFull()
{
}

BufferHealthStatus MediaTrack::GetBufferStatus()
{
	return BUFFER_STATUS_GREEN;
}

bool StreamAbstractionAAMP::SetTextStyle(const std::string &options)
{
	if (g_mockStreamAbstractionAAMP != nullptr)
	{
		return g_mockStreamAbstractionAAMP->SetTextStyle(options);
	}
	return false;
}

void MediaTrack::AbortWaitForCachedAndFreeFragment(bool immediate)
{
}

void MediaTrack::AbortWaitForCachedFragment()
{
}

void MediaTrack::AbortWaitForPlaylistDownload()
{
}

bool MediaTrack::Enabled()
{
	return true;
}

void MediaTrack::FlushFragments()
{
}

int MediaTrack::GetCurrentBandWidth()
{
	return 0;
}

CachedFragment* MediaTrack::GetFetchBuffer(bool initialize)
{
	return NULL;
}

AampMediaType MediaTrack::GetPlaylistMediaTypeFromTrack(TrackType type, bool isIframe)
{
	return eMEDIATYPE_DEFAULT;
}

void MediaTrack::PlaylistDownloader()
{
}

void MediaTrack::SetCurrentBandWidth(int bandwidthBps)
{
}

void MediaTrack::StartInjectLoop()
{
}

void MediaTrack::StartPlaylistDownloaderThread()
{
}

MediaTrack::MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name) : parsedBufferChunk("parsedBufferChunk"), unparsedBufferChunk("unparsedBufferChunk"), name(name)
{
}

MediaTrack::~MediaTrack()
{
}

void MediaTrack::StopInjectLoop()
{
}

void MediaTrack::StopPlaylistDownloaderThread()
{
}

void MediaTrack::UpdateTSAfterFetch(bool isInitSegment)
{
}

bool MediaTrack::WaitForFreeFragmentAvailable( int timeoutMs)
{
	return true;
}

void MediaTrack::WaitForManifestUpdate()
{
}

bool MediaTrack::WaitForCachedFragmentChunkInjected(int timeoutMs)
{
	return true;
}
bool MediaTrack::CheckForDiscontinuity(CachedFragment* cachedFragment, bool& fragmentDiscarded, bool& isDiscontinuity, bool &ret)
{
	return false;
}

void MediaTrack::ProcessAndInjectFragment(CachedFragment *cachedFragment, bool fragmentDiscarded, bool isDiscontinuity, bool &ret)
{
	return;
}

double MediaTrack::GetTotalInjectedDuration()
{
	return 0.0;
}

bool MediaTrack::SignalIfEOSReached()
{
	return false;
}

void MediaTrack::SetLocalTSBInjection(bool value)
{
}

bool StreamAbstractionAAMP::CheckForRampDownLimitReached()
{
	return true;
}

bool StreamAbstractionAAMP::CheckForRampDownProfile(int http_error)
{
	return true;
}

double StreamAbstractionAAMP::LastVideoFragParsedTimeMS(void)
{
	return 0;
}

int StreamAbstractionAAMP::GetDesiredProfile(bool getMidProfile)
{
	return 0;
}

int StreamAbstractionAAMP::GetDesiredProfileBasedOnCache(void)
{
	return 0;
}

int StreamAbstractionAAMP::GetIframeTrack()
{
	return 0;
}

int StreamAbstractionAAMP::GetMaxBWProfile()
{
	return 0;
}

int StreamAbstractionAAMP::getOriginalCurlError(int http_error)
{
	return 0;
}

void StreamAbstractionAAMP::AbortWaitForAudioTrackCatchup(bool force)
{
}

void StreamAbstractionAAMP::CheckForPlaybackStall(bool fragmentParsed)
{
}

void StreamAbstractionAAMP::CheckForProfileChange(void)
{
}

void StreamAbstractionAAMP::GetDesiredProfileOnBuffer(int currProfileIndex, int &newProfileIndex)
{
}

void StreamAbstractionAAMP::GetDesiredProfileOnSteadyState(int currProfileIndex, int &newProfileIndex, long nwBandwidth)
{
}

void StreamAbstractionAAMP::ReassessAndResumeAudioTrack(bool abort)
{
}
bool StreamAbstractionAAMP::IsStreamerAtLivePoint(double seekPosition)
{
	return false;
}

CachedFragment* MediaTrack::GetFetchChunkBuffer(bool initialize)
{
	 return NULL;
}

void MediaTrack::UpdateTSAfterChunkFetch()
{
}

void StreamAbstractionAAMP::UpdateRampUpOrDownProfileReason(void)
{
}

void MediaTrack::WaitForCachedAudioFragmentAvailable()
{
}

void MediaTrack::LoadNewAudio(bool)
{
}

void MediaTrack::AbortWaitForCachedFragmentChunk()
{
}

double StreamAbstractionAAMP::GetBufferValue(MediaTrack *video)
{
	return 0;
}
void MediaTrack::SetCachedFragmentChunksSize(size_t size)
{
}
void MediaTrack::UpdateTSAfterInject()
{
}
void StreamAbstractionAAMP::UpdateStreamInfoBitrateData(int profileIndex, StreamInfo &cacheFragStreamInfo)
{
}

void StreamAbstractionAAMP::SetVideoPlaybackRate(float rate)
{
	if (g_mockStreamAbstractionAAMP != nullptr)
	{
		g_mockStreamAbstractionAAMP->SetVideoPlaybackRate(rate);
	}
}

void StreamAbstractionAAMP::InitializeMediaProcessor()
{
}

void StreamAbstractionAAMP::UpdateIframeTracks()
{
}

void StreamAbstractionAAMP::ResumeTrackDownloadsHandler()
{
}

void StreamAbstractionAAMP::StopTrackDownloadsHandler()
{
}

void StreamAbstractionAAMP::GetPlayerPositionsHandler(long long& getPositionMS, double& seekPositionSeconds)
{
}

void StreamAbstractionAAMP::SendVTTCueDataHandler(VTTCue* cueData)
{
}

void MediaTrack::FlushFragmentChunks()
{
}

bool MediaTrack::IsInjectionFromCachedFragmentChunks()
{
	bool ret = false;
	return ret;
}