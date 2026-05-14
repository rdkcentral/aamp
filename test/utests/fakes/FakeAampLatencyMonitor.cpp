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
 * @file FakeAampLatencyMonitor.cpp
 * @brief Stub implementation of AampLatencyMonitor for use in unit tests
 *        of components that depend on AampLatencyMonitor.
 */

#include "AampLatencyMonitor.h"
#include "MockAampLatencyMonitor.h"

MockAampLatencyMonitor *g_mockAampLatencyMonitor = nullptr;

AampLatencyMonitor::AampLatencyMonitor(PrivateInstanceAAMP* aamp)
	: mAamp{aamp}
	, mConfig{}
	, mThresholdMutex{}
	, mMinLatencyMs{0.0}
	, mTargetLatencyMs{0.0}
	, mMaxLatencyMs{0.0}
	, mLatencyIncrementAccumulatedMs{0.0}
	, mState{State::kIdle}
	, mCurrentRate{1.0}
	, mCorrectionEnabled{true}
	, mSleepMutex{}
	, mSleepCv{}
	, mWakeupSignalled{false}
	, mThread{}
{
}

AampLatencyMonitor::~AampLatencyMonitor()
{
}

void AampLatencyMonitor::Start(const LatencyConfig& config)
{
	if (g_mockAampLatencyMonitor != nullptr)
	{
		g_mockAampLatencyMonitor->Start(config);
	}
}

void AampLatencyMonitor::Stop()
{
}

void AampLatencyMonitor::EnableRateCorrection(bool enabled)
{
}

double AampLatencyMonitor::GetCurrentRate() const
{
    return 1.0;
}

// Private stubs — never invoked through the fake's public interface,
// but required to satisfy the linker.
void AampLatencyMonitor::Run()
{
}
void AampLatencyMonitor::WaitMs(int)
{
}

void AampLatencyMonitor::WaitUntilSignalled()
{
}

void AampLatencyMonitor::ApplyRate(double newRate)
{
}

void AampLatencyMonitor::ResetToNormalRate()
{
}

void AampLatencyMonitor::OnBufferLevelUpdate(double bufferMs)
{
}

std::tuple<double, double, double> AampLatencyMonitor::GetCurrentThresholds() const
{
	return std::make_tuple(mMinLatencyMs, mTargetLatencyMs, mMaxLatencyMs);
}

double AampLatencyMonitor::GetAccumulatedLatencyIncrementMs() const
{
	if (g_mockAampLatencyMonitor != nullptr)
	{
		return g_mockAampLatencyMonitor->GetAccumulatedLatencyIncrementMs();
	}
	return 0.0;
}

void AampLatencyMonitor::ResetLatencyThresholdsLocked()
{
}

void AampLatencyMonitor::IncreaseThresholdsLocked()
{
}

void AampLatencyMonitor::UpdateDangerBufferState(double /*bufferMs*/)
{
}

void AampLatencyMonitor::TryRestoreThresholdsLocked()
{
}