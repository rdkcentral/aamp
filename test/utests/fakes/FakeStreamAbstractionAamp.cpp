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

StreamAbstractionAAMP::StreamAbstractionAAMP(AampLogManager *logObj, PrivateInstanceAAMP* aamp)
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
    return false;
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

MediaType MediaTrack::GetPlaylistMediaTypeFromTrack(TrackType type, bool isIframe)
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

MediaTrack::MediaTrack(AampLogManager *logObj, TrackType type, PrivateInstanceAAMP* aamp, const char* name)
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

void MediaTrack::UpdateTSAfterFetch()
{
}

bool MediaTrack::WaitForFreeFragmentAvailable( int timeoutMs)
{
    return true;
}

void MediaTrack::WaitForManifestUpdate()
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

