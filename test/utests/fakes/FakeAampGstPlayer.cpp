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

#include "aampgstplayer.h"
#include "MockAampGstPlayer.h"
#include "priv_aamp.h"
#include "AampLogManager.h"

MockAAMPGstPlayer *g_mockAampGstPlayer = nullptr;
// // Required by AampGstPlayer mocks
// AAMPGstPlayer::id3_callback_t mock_id3_callback = [](MediaType , const uint8_t * , size_t , const SegmentInfo_t & ){ };

AAMPGstPlayer::AAMPGstPlayer(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames )
{
}

AAMPGstPlayer::~AAMPGstPlayer()
{
}

void AAMPGstPlayer::Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation)
{
}

bool AAMPGstPlayer::SendCopy( AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double duration)
{
	return true;
}

bool AAMPGstPlayer::SendTransfer(AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment, bool discontinuity)
{
	return true;
}

bool AAMPGstPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	return true;
}

void AAMPGstPlayer::EndOfStreamReached(AampMediaType mediaType)
{
}

void AAMPGstPlayer::Stream(void)
{
}

void AAMPGstPlayer::Stop(bool keepLastFrame)
{
}

void AAMPGstPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		g_mockAampGstPlayer->Flush(position, rate, shouldTearDown);
	}
}

bool AAMPGstPlayer::SetPlayBackRate ( double rate )
{
	if (g_mockAampGstPlayer != nullptr)
	{
		return g_mockAampGstPlayer->SetPlayBackRate(rate);
	}
	return true;
}

bool AAMPGstPlayer::Pause(bool pause, bool forceStopGstreamerPreBuffering)
{
	return true;
}

long AAMPGstPlayer::GetDurationMilliseconds(void)
{
	return 0;
}

long long AAMPGstPlayer::GetPositionMilliseconds(void)
{
	return 0;
}

long long AAMPGstPlayer::GetVideoPTS(void)
{
	return 0;
}

void AAMPGstPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
}

void AAMPGstPlayer::SetVideoZoom(VideoZoomMode zoom)
{
}

void AAMPGstPlayer::SetVideoMute(bool muted)
{
}

void AAMPGstPlayer::ResetFirstFrame(void)
{
}

void AAMPGstPlayer::SetSubtitleMute(bool muted)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		g_mockAampGstPlayer->SetSubtitleMute(muted);
	}
}

void AAMPGstPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
}

void AAMPGstPlayer::SetAudioVolume(int volume)
{
}

bool AAMPGstPlayer::Discontinuity( AampMediaType mediaType)
{
	return true;
}

bool AAMPGstPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	return true;
}

bool AAMPGstPlayer::IsCacheEmpty(AampMediaType mediaType)
{
	return true;
}

void AAMPGstPlayer::ResetEOSSignalledFlag()
{
}

void AAMPGstPlayer::NotifyFragmentCachingComplete()
{
}

void AAMPGstPlayer::NotifyFragmentCachingOngoing()
{
}

void AAMPGstPlayer::GetVideoSize(int &w, int &h)
{
}

void AAMPGstPlayer::QueueProtectionEvent(const char *protSystemId, const void *ptr, size_t len, AampMediaType type)
{
}

void AAMPGstPlayer::ClearProtectionEvent()
{
}

void AAMPGstPlayer::SignalTrickModeDiscontinuity()
{
}

void AAMPGstPlayer::SeekStreamSink(double position, double rate)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		g_mockAampGstPlayer->SeekStreamSink(position, rate);
	}
}

std::string AAMPGstPlayer::GetVideoRectangle()
{
	return std::string();
}

void AAMPGstPlayer::StopBuffering(bool forceStop)
{
}

void AAMPGstPlayer::InitializeAAMPGstreamerPlugins()
{
}

bool AAMPGstPlayer::SetTextStyle(const std::string &options)
{
	return false;
}

PlaybackQualityStruct* AAMPGstPlayer::GetVideoPlaybackQuality(void)
{
	return nullptr;
}

bool AAMPGstPlayer::IsAssociatedAamp(PrivateInstanceAAMP *aamp)
{
	return false;
}

void AAMPGstPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		g_mockAampGstPlayer->ChangeAamp(newAamp, id3HandlerCallback);
	}
}

void AAMPGstPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		g_mockAampGstPlayer->SetEncryptedAamp(aamp);
	}
}

void AAMPGstPlayer::FlushTrack(AampMediaType mediaType,double pos)
{
}

bool AAMPGstPlayer::SignalSubtitleClock( void )
{
	return false;
}

bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		return g_mockAampGstPlayer->IsCodecSupported(codecName);
	}
	return false;
}

void AAMPGstPlayer::GetBufferControlData(AampMediaType mediaType, BufferControlData &data) const
{
}

void AAMPGstPlayer::SetPauseOnStartPlayback(bool enable)
{
	if (g_mockAampGstPlayer != nullptr)
	{
		g_mockAampGstPlayer->SetPauseOnStartPlayback(enable);
	}
}

void AAMPGstPlayer::NotifyInjectorToPause()
{
}
void AAMPGstPlayer::NotifyInjectorToResume()
{
}
