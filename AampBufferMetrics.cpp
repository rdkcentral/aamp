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
 * @file AampBufferMetrics.cpp
 * @brief Implementation of AampBufferMetrics.
 */

#include "AampBufferMetrics.h"
#include "AampLogManager.h"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{
/**
 * @brief Compute the EMA smoothing coefficient.
 *
 * alpha = 1 - exp(-T / tau)
 *
 * Clamped to [0.0, 1.0] to guard against degenerate config values.
 */
double ComputeAlpha(double sampleIntervalSec, double tauSec)
{
	if (tauSec <= 0.0 || sampleIntervalSec <= 0.0)
	{
		return 1.0; // No smoothing — raw passthrough.
	}
	const double alpha = 1.0 - std::exp(-sampleIntervalSec / tauSec);
	return std::max(0.0, std::min(1.0, alpha));
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AampBufferMetrics::AampBufferMetrics(const BufferMetricsConfig& config)
	: mMutex{}
	, mConfig{config}
	, mAlpha{ComputeAlpha(config.sampleIntervalSec, config.tauSec)}
	, mVideo{}
	, mAudio{}
	, mListeners{}
{
	AAMPLOG_INFO("[BufferMetrics] Created: criticalThreshold=%.2f s "
	             "lowThreshold=%.2f s hysteresis=%.2f s "
	             "alpha=%.4f (tau=%.2f s, T=%.3f s)",
	             config.criticalThresholdSec,
	             config.lowThresholdSec,
	             config.hysteresisSec,
	             mAlpha,
	             config.tauSec,
	             config.sampleIntervalSec);
}

// ---------------------------------------------------------------------------
// Listener management
// ---------------------------------------------------------------------------

void AampBufferMetrics::AddListener(IBufferMetricsListener* listener)
{
	if (listener == nullptr)
	{
		AAMPLOG_WARN("[BufferMetrics] AddListener called with null listener");
		return;
	}
	std::lock_guard<std::mutex> lock(mMutex);
	// Avoid duplicate registrations.
	for (const auto* existing : mListeners)
	{
		if (existing == listener)
		{
			AAMPLOG_WARN("[BufferMetrics] Listener %p already registered", listener);
			return;
		}
	}
	mListeners.push_back(listener);
}

void AampBufferMetrics::RemoveListener(IBufferMetricsListener* listener)
{
	std::lock_guard<std::mutex> lock(mMutex);
	auto it = std::find(mListeners.begin(), mListeners.end(), listener);
	if (it != mListeners.end())
	{
		mListeners.erase(it);
	}
}

// ---------------------------------------------------------------------------
// Sample ingestion
// ---------------------------------------------------------------------------

void AampBufferMetrics::AddSample(AampMediaType mediaType, double rawSec)
{
	if (rawSec < 0.0)
	{
		AAMPLOG_WARN("[BufferMetrics] Negative buffer sample %.4f s clamped to 0",
		             rawSec);
		rawSec = 0.0;
	}

	std::lock_guard<std::mutex> lock(mMutex);

	TrackMetrics* track = GetTrack(mediaType);
	if (track == nullptr)
	{
		// Unsupported media type; silently ignore.
		return;
	}

	// Prime the EMA with the first raw sample to avoid a cold-start ramp-in.
	if (!track->primed)
	{
		track->ema    = rawSec;
		track->primed = true;
	}
	else
	{
		// EMA: ema_new = alpha * raw + (1 - alpha) * ema_old
		track->ema = mAlpha * rawSec + (1.0 - mAlpha) * track->ema;
	}

	const BufferState newState = Classify(track->ema, track->state);
	if (newState != track->state)
	{
		const BufferState oldState = track->state;
		track->state = newState;
		AAMPLOG_INFO("[BufferMetrics] Track %d state %d -> %d (ema=%.3f s)",
		             static_cast<int>(mediaType),
		             static_cast<int>(oldState),
		             static_cast<int>(newState),
		             track->ema);
		NotifyListeners(mediaType, oldState, newState, track->ema);
	}
}

void AampBufferMetrics::NotifyUnderflow(AampMediaType mediaType)
{
	std::lock_guard<std::mutex> lock(mMutex);

	TrackMetrics* track = GetTrack(mediaType);
	if (track == nullptr)
	{
		return;
	}

	// Immediately force the EMA to zero and state to CRITICAL.
	track->ema    = 0.0;
	track->primed = true;

	if (track->state != BufferState::CRITICAL)
	{
		const BufferState oldState = track->state;
		track->state = BufferState::CRITICAL;
		AAMPLOG_WARN("[BufferMetrics] Track %d underflow forced CRITICAL",
		             static_cast<int>(mediaType));
		NotifyListeners(mediaType, oldState, BufferState::CRITICAL, 0.0);
	}
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

double AampBufferMetrics::GetSmoothedLevel(AampMediaType mediaType) const
{
	std::lock_guard<std::mutex> lock(mMutex);
	const TrackMetrics* track = GetTrack(mediaType);
	return (track != nullptr) ? track->ema : 0.0;
}

BufferState AampBufferMetrics::GetState(AampMediaType mediaType) const
{
	std::lock_guard<std::mutex> lock(mMutex);
	const TrackMetrics* track = GetTrack(mediaType);
	return (track != nullptr) ? track->state : BufferState::CRITICAL;
}

// ---------------------------------------------------------------------------
// Reset hooks
// ---------------------------------------------------------------------------

void AampBufferMetrics::ResetOnSeek()
{
	std::lock_guard<std::mutex> lock(mMutex);
	AAMPLOG_INFO("[BufferMetrics] ResetOnSeek");
	ResetTrack(mVideo);
	ResetTrack(mAudio);
}

void AampBufferMetrics::ResetOnTrickplay()
{
	std::lock_guard<std::mutex> lock(mMutex);
	AAMPLOG_INFO("[BufferMetrics] ResetOnTrickplay");
	ResetTrack(mVideo);
	ResetTrack(mAudio);
}

void AampBufferMetrics::ResetOnStop()
{
	std::lock_guard<std::mutex> lock(mMutex);
	AAMPLOG_INFO("[BufferMetrics] ResetOnStop");
	ResetTrack(mVideo);
	ResetTrack(mAudio);
	mListeners.clear();
}

// ---------------------------------------------------------------------------
// Configuration update
// ---------------------------------------------------------------------------

void AampBufferMetrics::UpdateConfig(const BufferMetricsConfig& config)
{
	std::lock_guard<std::mutex> lock(mMutex);
	mConfig = config;
	mAlpha  = ComputeAlpha(config.sampleIntervalSec, config.tauSec);
	AAMPLOG_INFO("[BufferMetrics] Config updated: criticalThreshold=%.2f s "
	             "lowThreshold=%.2f s hysteresis=%.2f s alpha=%.4f",
	             config.criticalThresholdSec,
	             config.lowThresholdSec,
	             config.hysteresisSec,
	             mAlpha);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

BufferState AampBufferMetrics::Classify(double ema, BufferState currentState) const
{
	// Hysteresis logic: upward transitions require clearing the threshold by
	// hysteresisSec; downward transitions fire immediately at the threshold.

	switch (currentState)
	{
		case BufferState::CRITICAL:
			// Upgrade to LOW only when ema rises above
			// (criticalThreshold + hysteresis).
			if (ema >= mConfig.criticalThresholdSec + mConfig.hysteresisSec)
			{
				// May further upgrade to GOOD if already above low threshold.
				if (ema >= mConfig.lowThresholdSec + mConfig.hysteresisSec)
				{
					return BufferState::GOOD;
				}
				return BufferState::LOW;
			}
			return BufferState::CRITICAL;

		case BufferState::LOW:
			// Downgrade to CRITICAL without hysteresis (immediate).
			if (ema < mConfig.criticalThresholdSec)
			{
				return BufferState::CRITICAL;
			}
			// Upgrade to GOOD with hysteresis.
			if (ema >= mConfig.lowThresholdSec + mConfig.hysteresisSec)
			{
				return BufferState::GOOD;
			}
			return BufferState::LOW;

		case BufferState::GOOD:
		default:
			// Downgrade to LOW without hysteresis.
			if (ema < mConfig.criticalThresholdSec)
			{
				return BufferState::CRITICAL;
			}
			if (ema < mConfig.lowThresholdSec)
			{
				return BufferState::LOW;
			}
			return BufferState::GOOD;
	}
}

void AampBufferMetrics::NotifyListeners(AampMediaType mediaType,
                                        BufferState   oldState,
                                        BufferState   newState,
                                        double        smoothedSec) const
{
	// mMutex is already held by the caller.
	for (auto* listener : mListeners)
	{
		listener->OnBufferStateChanged(mediaType, oldState, newState, smoothedSec);
	}
}

AampBufferMetrics::TrackMetrics* AampBufferMetrics::GetTrack(AampMediaType mediaType)
{
	if (mediaType == eMEDIATYPE_VIDEO) { return &mVideo; }
	if (mediaType == eMEDIATYPE_AUDIO) { return &mAudio; }
	return nullptr;
}

const AampBufferMetrics::TrackMetrics* AampBufferMetrics::GetTrack(AampMediaType mediaType) const
{
	if (mediaType == eMEDIATYPE_VIDEO) { return &mVideo; }
	if (mediaType == eMEDIATYPE_AUDIO) { return &mAudio; }
	return nullptr;
}

void AampBufferMetrics::ResetTrack(TrackMetrics& track)
{
	track.ema    = 0.0;
	track.primed = false;
	track.state  = BufferState::GOOD;
}
