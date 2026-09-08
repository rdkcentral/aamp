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
 * @file FakeInterfacePlayerRDK.cpp
 * @brief Fake implementation of InterfacePlayerRDK for AampGstPlayer L1 tests.
 *
 * Provides stub implementations so that aampgstplayer.cpp can be tested in
 * isolation without compiling the real middleware pipeline code.
 */

#include "InterfacePlayerRDK.h"

static MonitorAVState sMonitorAVState{};
static Configs sDefaultConfigs{};
static GstPlaybackQualityStruct sPlaybackQuality{};

InterfacePlayerRDK::InterfacePlayerRDK(bool /*isRialto*/)
	: trickTeardown(false)
	, m_gstConfigParam(&sDefaultConfigs)
	, mDrmSystem(nullptr)
	, mEncrypt(nullptr)
	, mDRMSessionManager(nullptr)
	, mProtectionLock()
	, mFirstFrameRequired(false)
	, mPauseInjector(false)
	, mResumeInjector(false)
	, PipelineSetToReady(false)
	, mSchedulerStarted(false)
	, interfacePlayerPriv(nullptr)
{
}

InterfacePlayerRDK::~InterfacePlayerRDK()
{
}

InterfacePlayerPriv* InterfacePlayerRDK::GetPrivatePlayer()
{
	return interfacePlayerPriv;
}

void InterfacePlayerRDK::SetPlayerName(std::string /*name*/) {}
void InterfacePlayerRDK::SetPreferredDRM(const char* /*drmID*/) {}
void InterfacePlayerRDK::setEncryption(void* /*mEncrypt*/, void* /*mDRMSessionManager*/) {}
void InterfacePlayerRDK::EnableGstDebugLogging(std::string /*debugLevel*/) {}

void InterfacePlayerRDK::ConfigurePipeline(int, int, int, bool, bool, bool, int32_t, gint, const char*, int, bool, std::string, bool) {}
bool InterfacePlayerRDK::CreatePipeline(const char* /*pipelineName*/, int /*PipelinePriority*/) { return false; }
void InterfacePlayerRDK::DestroyPipeline() {}
void InterfacePlayerRDK::Stop(bool /*keepLastFrame*/) {}
bool InterfacePlayerRDK::Pause(bool /*pause*/, bool /*forceStopGstreamerPreBuffering*/) { return false; }
bool InterfacePlayerRDK::Flush(double, int, bool, bool) { return false; }
double InterfacePlayerRDK::FlushTrack(int, double, double, double) { return 1.0; }

void InterfacePlayerRDK::SetVideoRectangle(int, int, int, int) {}
void InterfacePlayerRDK::SetVideoZoom(int) {}
void InterfacePlayerRDK::SetVideoMute(bool) {}
void InterfacePlayerRDK::SetSubtitlePtsOffset(std::uint64_t) {}
void InterfacePlayerRDK::SetSubtitleMute(bool) {}
bool InterfacePlayerRDK::SetTextStyle(const std::string&) { return false; }
void InterfacePlayerRDK::SetAudioVolume(int) {}
void InterfacePlayerRDK::SetVolumeOrMuteUnMute() {}
bool InterfacePlayerRDK::SetPlayBackRate(double) { return false; }
void InterfacePlayerRDK::SetPauseOnStartPlayback(bool) {}
void InterfacePlayerRDK::ResetFirstFrame() {}

long long InterfacePlayerRDK::GetPositionMilliseconds() { return 0; }
long InterfacePlayerRDK::GetDurationMilliseconds() { return 0; }
long long InterfacePlayerRDK::GetVideoPTS() { return 0; }
long long InterfacePlayerRDK::GetVideoPosition() { return 0; }
void InterfacePlayerRDK::GetVideoSize(int& width, int& height) { width = 0; height = 0; }
std::string InterfacePlayerRDK::GetVideoRectangle() { return ""; }
GstPlaybackQualityStruct* InterfacePlayerRDK::GetVideoPlaybackQuality() { return &sPlaybackQuality; }
unsigned long InterfacePlayerRDK::GetCCDecoderHandle() { return 0; }
const MonitorAVState& InterfacePlayerRDK::GetMonitorAVState() { return sMonitorAVState; }

bool InterfacePlayerRDK::SendHelper(int, MediaSample&&, bool, bool&, bool&, bool&, bool&, bool&) { return false; }
void InterfacePlayerRDK::PauseInjector() {}
void InterfacePlayerRDK::ResumeInjector() {}
bool InterfacePlayerRDK::HandleVideoBufferSent() { return false; }
void InterfacePlayerRDK::QueueProtectionEvent(const std::string&, const char*, const void*, size_t, int) {}
void InterfacePlayerRDK::ClearProtectionEvent() {}
void InterfacePlayerRDK::EndOfStreamReached(int, bool&) {}
void InterfacePlayerRDK::NotifyFragmentCachingComplete() {}
bool InterfacePlayerRDK::StopBuffering(bool, bool&) { return false; }
void InterfacePlayerRDK::ResetEOSSignalledFlag() {}
void InterfacePlayerRDK::EnablePendingPlayState() {}
void InterfacePlayerRDK::DisableDecoderHandleNotified() {}
void InterfacePlayerRDK::SignalTrickModeDiscontinuity() {}
bool InterfacePlayerRDK::SignalSubtitleClock(gint64, bool) { return false; }

bool InterfacePlayerRDK::PipelineConfiguredForMedia(int) { return false; }
bool InterfacePlayerRDK::IsCacheEmpty(int) { return true; }
bool InterfacePlayerRDK::IsStreamReady(int) { return false; }
bool InterfacePlayerRDK::GetBufferControlData(int) { return false; }
bool InterfacePlayerRDK::IsPipelinePaused() { return false; }
bool InterfacePlayerRDK::CheckDiscontinuity(int, int, bool, bool&, bool&) { return false; }
bool InterfacePlayerRDK::CheckForPTSChangeWithTimeout(long) { return false; }

void InterfacePlayerRDK::FirstFrameCallback(std::function<void(int, bool, bool, bool&, bool&)> callback)
{
	notifyFirstFrameCallback = std::move(callback);
}

void InterfacePlayerRDK::StopCallback(std::function<void(bool)> callback)
{
	stopCallback = std::move(callback);
}

void InterfacePlayerRDK::TearDownCallback(std::function<void(bool, int)> callback)
{
	tearDownCb = std::move(callback);
}

void InterfacePlayerRDK::TimerAdd(GSourceFunc, int, guint&, gpointer, const char*) {}
void InterfacePlayerRDK::TimerRemove(guint& taskId, const char*) { taskId = 0; }
bool InterfacePlayerRDK::TimerIsRunning(guint&) { return false; }

void InterfacePlayerRDK::SetStreamCaps(GstMediaType, MediaCodecInfo&&) {}

void InterfacePlayerRDK::IdleTaskClearFlags(GstTaskControlData&) {}
bool InterfacePlayerRDK::IdleTaskRemove(GstTaskControlData&) { return false; }
bool InterfacePlayerRDK::IdleTaskAdd(GstTaskControlData&, BackgroundTask) { return false; }

gboolean InterfacePlayerRDK::IdleCallbackOnFirstFrame(gpointer) { return FALSE; }
gboolean InterfacePlayerRDK::IdleCallbackOnEOS(gpointer) { return FALSE; }
gboolean InterfacePlayerRDK::ProgressCallbackOnTimeout(gpointer) { return FALSE; }
gboolean InterfacePlayerRDK::IdleCallback(gpointer) { return FALSE; }
gboolean InterfacePlayerRDK::IdleCallbackFirstVideoFrameDisplayed(gpointer) { return FALSE; }

void InterfacePlayerRDK::TearDownStream(int) {}
void InterfacePlayerRDK::InitializeSourceForPlayer(void*, void*, int) {}
void InterfacePlayerRDK::SetupClosedCaptionControlStream() {}
int InterfacePlayerRDK::SetupStream(int, void*, std::string) { return 0; }
int InterfacePlayerRDK::InterfacePlayer_SetupStream(int, std::string) { return 0; }
void InterfacePlayerRDK::TriggerEvent(InterfaceCB, int) {}
void InterfacePlayerRDK::TriggerEvent(InterfaceCB) {}
void InterfacePlayerRDK::NotifyFirstFrame(int) {}
void InterfacePlayerRDK::NotifyEOS() {}
bool InterfacePlayerRDK::WaitForSourceSetup(int) { return true; }
void InterfacePlayerRDK::SetSeekPosition(double) {}
void InterfacePlayerRDK::DisconnectSignals() {}
void InterfacePlayerRDK::RemoveProbes() {}
void InterfacePlayerRDK::RemoveProbe(int) {}
void InterfacePlayerRDK::SetPendingSeek(bool) {}
bool InterfacePlayerRDK::IsUsingRialtoSink() { return false; }
void InterfacePlayerRDK::ResetGstEvents() {}
bool InterfacePlayerRDK::GetTrickTeardown() { return trickTeardown; }
void InterfacePlayerRDK::SetTrickTearDown(bool state) { trickTeardown = state; }
void InterfacePlayerRDK::DumpDiagnostics() {}
void InterfacePlayerRDK::InitializePlayerGstreamerPlugins() {}
