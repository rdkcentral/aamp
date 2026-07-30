/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file FakeAampRialtoPlayer.cpp
 * @brief Linker stubs for AampRialtoPlayer used in L1 tests that pull in
 *        AampStreamSinkManager.cpp but do not exercise the Rialto path.
 */

#include "AampRialtoPlayer.h"
#include "AampRialtoMediaSource.h"
#include "PrivateInstanceAAMPNotifiable.h"
#include "AampLogManager.h"

AampRialtoPlayer::AampRialtoPlayer(
	PrivateInstanceAAMP *aamp,
	id3_callback_t /*id3HandlerCallback*/,
	std::function<void(const unsigned char *, int, int, int)> /*exportFrames*/)
	: m_aamp(aamp)
	, m_drmBridge(nullptr)
	, m_controlBackend(nullptr)
	, m_client(nullptr)
	, m_pipeline(nullptr)
{
}

AampRialtoPlayer::~AampRialtoPlayer() {}

void AampRialtoPlayer::Configure(
	StreamOutputFormat, StreamOutputFormat, StreamOutputFormat,
	bool, bool) {}

bool AampRialtoPlayer::SendCopy(
	AampMediaType, std::vector<uint8_t> &&, double, double, double)
{ return false; }

bool AampRialtoPlayer::SendTransfer(
	AampMediaType, std::vector<uint8_t> &&, double, double, double, double,
	bool, bool)
{ return false; }

bool AampRialtoPlayer::SendSample(AampMediaType, AampMediaSample &&, bool /*morePending*/)
{ return false; }

bool AampRialtoPlayer::PipelineConfiguredForMedia(AampMediaType)
{ return false; }

void AampRialtoPlayer::EndOfStreamReached(AampMediaType) {}
void AampRialtoPlayer::Stream() {}
void AampRialtoPlayer::Stop(bool) {}
void AampRialtoPlayer::Flush(double, int, bool) {}
void AampRialtoPlayer::FlushTrack(AampMediaType, double) {}

bool AampRialtoPlayer::SetPlayBackRate(double) { return false; }
bool AampRialtoPlayer::Pause(bool, bool) { return false; }

long AampRialtoPlayer::GetDurationMilliseconds() { return 0; }
long long AampRialtoPlayer::GetPositionMilliseconds() { return 0; }
long long AampRialtoPlayer::GetVideoPTS() { return 0; }

void AampRialtoPlayer::SetVideoRectangle(int, int, int, int) {}
void AampRialtoPlayer::SetVideoZoom(VideoZoomMode) {}
void AampRialtoPlayer::SetVideoMute(bool) {}
void AampRialtoPlayer::SetSubtitleMute(bool) {}
void AampRialtoPlayer::SetSubtitlePtsOffset(std::uint64_t) {}
void AampRialtoPlayer::SetAudioVolume(int) {}

bool AampRialtoPlayer::Discontinuity(AampMediaType) { return false; }
bool AampRialtoPlayer::CheckForPTSChangeWithTimeout(long) { return false; }
bool AampRialtoPlayer::IsCacheEmpty(AampMediaType) { return true; }

void AampRialtoPlayer::ResetEOSSignalledFlag() {}
void AampRialtoPlayer::NotifyFragmentCachingComplete() {}
void AampRialtoPlayer::NotifyFragmentCachingOngoing() {}
void AampRialtoPlayer::GetVideoSize(int &, int &) {}

void AampRialtoPlayer::QueueProtectionEvent(
	const char *, const void *, size_t, AampMediaType) {}
void AampRialtoPlayer::ClearProtectionEvent() {}
void AampRialtoPlayer::SignalTrickModeDiscontinuity() {}
void AampRialtoPlayer::SeekStreamSink(double, double) {}

std::string AampRialtoPlayer::GetVideoRectangle() { return {}; }

bool AampRialtoPlayer::setTextTrackIdentifier(const std::string &) { return false; }
bool AampRialtoPlayer::setCCMute(bool) { return false; }

void AampRialtoPlayer::StopBuffering(bool) {}
bool AampRialtoPlayer::SetTextStyle(const std::string &) { return false; }
PlaybackQualityStruct *AampRialtoPlayer::GetVideoPlaybackQuality() { return nullptr; }
bool AampRialtoPlayer::SignalSubtitleClock() { return false; }
void AampRialtoPlayer::SetPauseOnStartPlayback(bool) {}
void AampRialtoPlayer::NotifyInjectorToResume() {}
void AampRialtoPlayer::NotifyInjectorToPause() {}
void AampRialtoPlayer::StopTrackInjection(AampMediaType type) {}
void AampRialtoPlayer::ResumeTrackInjection(AampMediaType type) {}

void AampRialtoPlayer::SetStreamCaps(AampMediaType, MediaCodecInfo &&) {}
bool AampRialtoPlayer::IsAssociatedAamp(PrivateInstanceAAMP *) { return false; }
void AampRialtoPlayer::ChangeAamp(PrivateInstanceAAMP *, id3_callback_t) {}
void AampRialtoPlayer::SetEncryptedAamp(PrivateInstanceAAMP *) {}
void AampRialtoPlayer::ResetFirstFrame() {}

void AampRialtoPlayer::StartProgressTimer() {}
void AampRialtoPlayer::StopProgressTimer() {}
void AampRialtoPlayer::OnProgressTimerTick() {}

AampRialtoPlayer::ProgressTimer::~ProgressTimer() {}

void AampRialtoPlayer::RialtoLogHandler::log(
	Level, const std::string &, int,
	const std::string &, const std::string &) {}
