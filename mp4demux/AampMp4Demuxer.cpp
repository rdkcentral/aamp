/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file AampMp4Demuxer.cpp
 * @brief Implementation for MP4 Demuxer
 */

#include "AampMp4Demuxer.h"
#include "AampLogManager.h"
#include "AampUtils.h"
#include "AampConfig.h"
#include <cinttypes>
#include <cmath>


/**
 * @brief MP4 Demuxer Constructor
 * @param[in] aamp - Pointer to the PrivateInstanceAAMP
 * @param[in] type - Media type (audio/video/subtitle)
 * @param[in] enablePtsRestamp - Flag to enable PTS restamping
 */
AampMp4Demuxer::AampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type, bool enablePtsRestamp) :
	MediaProcessor(), mMp4Demux(aamp_utils::make_unique<Mp4Demux>()), mAamp(aamp), mMediaType(type), mEnablePtsRestamp(enablePtsRestamp)
{
	AAMPLOG_MIL("Created AampMp4Demuxer(%p) for type %d, PTS restamp: %s", this, type, enablePtsRestamp ? "enabled" : "disabled");
	// TODO: Should we limit the media types here to only video/audio?
	// Make restamp logging configurable as it might cause log flooding, since logs will come for each demuxed frames per fragment
	mEnablePtsRestampLogging = mAamp->mConfig->IsConfigSet(eAAMPConfig_EnablePTSReStampLogging);
	// mTrickPlayFPS should be set via setFrameRateForTM()
}

/**
 * @brief Provide the manifest-declared fallback timescale
 * @see AampMp4Demuxer.h
 */
void AampMp4Demuxer::setFallbackTimeScale(uint32_t timeScale)
{
	mMp4Demux->SetFallbackTimeScale(timeScale);
}

/**
 * @brief Set frame rate for trickmode
 * @param[in] frameRate - rate per second
 */
void AampMp4Demuxer::setFrameRateForTM(int frameRate)
{
	if (frameRate <= 0)
	{
		AAMPLOG_WARN("Invalid trickplay FPS %d for media type %s, using default %d",
			frameRate, GetMediaTypeName(mMediaType), TRICKPLAY_VOD_PLAYBACK_FPS);
		frameRate = TRICKPLAY_VOD_PLAYBACK_FPS;
	}
	mTrickPlayFPS = frameRate;
	AAMPLOG_INFO("TrickPlay FPS set to %d for media type %s", frameRate, GetMediaTypeName(mMediaType));
}

/**
 * @brief Set playback rate
 * @param[in] rate - playback rate
 * @param[in] mode - playback mode
 */
void AampMp4Demuxer::setRate(double rate, PlayMode mode)
{
	if (mRate != rate)
	{
		// Rate changed - reset all trickmode state once here so the next
		// keyframe starts a fresh restamp sequence from 0.
		resetTrickMode();
		mLastTrickRate = rate;
	}
	mRate = rate;
	mIsTrickMode = (rate > AAMP_NORMAL_PLAY_RATE) || (rate < 0);
	AAMPLOG_INFO("Rate set to %.2f, trickmode: %s for media type %s",
		rate, mIsTrickMode ? "enabled" : "disabled", GetMediaTypeName(mMediaType));
}

/**
 * @brief Abort all operations and reset trickmode state
 */
void AampMp4Demuxer::abort()
{
	// Set first so any sendSegment() call already looping over samples on
	// another thread observes it as soon as possible and stops sending
	// further samples for the in-flight fragment.
	mAborted.store(true, std::memory_order_relaxed);
	mTrickPhase = Mp4TrickPhase::FIRST_SAMPLE;
	mLastSamplePts = 0.0;
	mRestampedPts = 0.0;
	mRestampedDuration = 0.0;
	mLastTrickRate = 0.0;
	AAMPLOG_INFO("Abort: Reset trickmode state for media type %s", GetMediaTypeName(mMediaType));
}

/**
 * @brief Reset all trickmode state variables
 */
void AampMp4Demuxer::reset()
{
	mAborted.store(false, std::memory_order_relaxed);
	resetTrickMode();
}

/**
 * @brief Reset only trickmode-specific state variables.
 * Separated from reset() so future additions to the public reset() API
 * do not inadvertently affect the trickmode path in sendSegment().
 */
void AampMp4Demuxer::resetTrickMode()
{
	mTrickPhase = Mp4TrickPhase::FIRST_SAMPLE;
	mLastSamplePts = 0.0;
	mRestampedPts = 0.0;
	mRestampedDuration = 0.0;
	mLastTrickRate = 0.0;
	AAMPLOG_INFO("Reset trickmode state for media type %s", GetMediaTypeName(mMediaType));
}

/**
 * @brief MP4 Demuxer destructor
 */
AampMp4Demuxer::~AampMp4Demuxer()
{
	AAMPLOG_DEBUG("AampMp4Demuxer destructor");
	// std::unique_ptr automatically handles cleanup
}

/**
 * @brief Advance trickmode restamp timeline on discontinuity boundary.
 */
void AampMp4Demuxer::HandleTrickModeDiscontinuity()
{
	if (mTrickPhase == Mp4TrickPhase::STEADY)
	{
		mRestampedPts += mRestampedDuration;
		mTrickPhase = Mp4TrickPhase::DISCONTINUITY;
		AAMPLOG_WARN("[%s] Trickmode discontinuity: advancing restampedPts by %.6f to %.6f",
			GetMediaTypeName(mMediaType), mRestampedDuration, mRestampedPts);
	}
}

/**
 * @brief Apply trickmode PTS restamping to a sample, dynamically adjusting duration based on rate and frame rate
 * Similar to qtdemux approach but with dynamic duration calculation for smoother trickmode playback
 * @param[in,out] sample - Sample to restamp
 * @param[in] duration - Fragment duration
 * @param[in] discontinuous - True if this sample begins a discontinuous segment
 */
void AampMp4Demuxer::TrickmodePtsRestamp(AampMediaSample& sample, double duration, bool discontinuous)
{
	// Store original values for logging
	double originalPts = sample.mPts;
	double originalDts = sample.mDts;
	double originalDuration = sample.mDuration;
	double fragmentPtsDelta = 0.0;
	double restampedDuration = 0.0;

	// All phase transitions are owned here.
	switch (mTrickPhase)
	{
		case Mp4TrickPhase::FIRST_SAMPLE:
			// First sample after initial start or rate change: estimate duration from
			// rate and trickPlayFPS (no previous PTS delta available).
			// mTrickPlayFPS is guaranteed > 0 by setFrameRateForTM().
			restampedDuration = MAX(duration / std::fabs(mRate), 1.0 / mTrickPlayFPS);
			mRestampedDuration = restampedDuration;
			mRestampedPts = 0.0;
			mTrickPhase = Mp4TrickPhase::STEADY;
			AAMPLOG_INFO("Trickmode FIRST_SAMPLE->STEADY: rate=%.2f", mRate);
			break;
		case Mp4TrickPhase::DISCONTINUITY:
			// First keyframe after a discontinuity. mRestampedPts was already advanced
			// by sendSegment() before the sample loop; reuse the last known duration
			// (same as MediaTrack::TrickModePtsRestamp DISCONTINUITY handling).
			restampedDuration = mRestampedDuration;
			mTrickPhase = Mp4TrickPhase::STEADY;
			break;
		case Mp4TrickPhase::STEADY:
			// Delta-based duration: distance between current and previous original PTS
			// divided by |rate|.
			fragmentPtsDelta = fabs(sample.mPts - mLastSamplePts);
			restampedDuration = fragmentPtsDelta / std::fabs(mRate);
			mRestampedDuration = restampedDuration;
			mRestampedPts += restampedDuration;
			break;
	} // end switch

	// Store the current sample PTS before overwriting it
	mLastSamplePts = sample.mPts;

	// Apply restamped PTS and duration to the sample
	sample.mPts = mRestampedPts;
	sample.mDts = mRestampedPts;
	sample.mDuration = restampedDuration;

	// Single comprehensive log line
	AAMPLOG_INFO("state %d rate %.2f trickPlayFPS %d origPTS %.6f origDTS %.6f origDur %.6f restampedPTS %.6f restampedDTS %.6f restampedDur %.6f lastSamplePTS %.6f inputDuration %.6f",
		static_cast<int>(mTrickPhase),
		mRate,
		mTrickPlayFPS,
		originalPts,
		originalDts,
		originalDuration,
		sample.mPts,
		sample.mDts,
		sample.mDuration,
		mLastSamplePts,
		duration);
}
/**
 * @fn sendSegment
 *
 * @param[in] buffer - fragment data; ownership is transferred (consumed by this call).
 *                     Callers must pass via std::move() and must not read the buffer after
 *                     sendSegment() returns.
 * @param[in] position - position of fragment
 * @param[in] duration - duration of fragment
 * @param[in] fragmentPTSoffset - offset PTS of fragment
 * @param[in] discontinuous - true if discontinuous fragment
 * @param[in] isInit - flag for buffer type (init, data)
 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
 * @param[out] ptsError - flag indicates if any PTS error occurred
 * @return true if fragment was sent, false otherwise
 */
bool AampMp4Demuxer::sendSegment(std::vector<uint8_t>&& buffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
				bool isInit, process_fcn_t processor, bool &ptsError)
{
	bool ret = true;
	(void) processor;
	if (mAborted.load(std::memory_order_relaxed))
	{
		AAMPLOG_WARN("Aborted - not processing segment for type:%d position: %f, duration: %f, isInit: %d", mMediaType, position, duration, isInit);
		ptsError = false;
		return false;
	}
	if (mMp4Demux && !buffer.empty())
	{
		// Move the caller's buffer into a shared_ptr and pass ownership into
		// Parse(), which stamps each sample's mData (via aliasing shared_ptr)
		// so each sample keeps the segment buffer alive for its lifetime.
		auto segment = std::make_shared<std::vector<uint8_t>>(std::move(buffer));
		AAMPLOG_INFO("Processing segment with type:%d position: %f, duration: %f, isInit: %d", mMediaType, position, duration, isInit);
		
	
		ret = mMp4Demux->Parse(std::move(segment));
		
		if (!ret)
		{
			AAMPLOG_ERR("Failed to parse MP4 segment [err:%d] for type:%d position: %f, duration: %f, isInit: %d", mMp4Demux->GetLastError(), mMediaType, position, duration, isInit);
		}
		else
		{
			auto samples = mMp4Demux->GetSamples();
			if (!samples.empty())
			{
				size_t sampleIndex = 0;
				const size_t totalSamples = samples.size();
				if (mIsTrickMode)
				{
					// Trickmode: the demuxer yields exactly one sample — the iframe.
					if (mAborted.load(std::memory_order_relaxed))
					{
						AAMPLOG_WARN("Aborted - not injecting trickmode sample for type:%d position: %f", mMediaType, position);
						ret = false;
					}
					else
					{
						auto& iframe = samples.front();
						TrickmodePtsRestamp(iframe, duration, discontinuous);

						++sampleIndex;
						bool morePending = (sampleIndex < totalSamples);
						mAamp->SendStreamTransfer(mMediaType, std::move(iframe), morePending);
					}
				}
				else
				{
					double firstBeforeDTS = 0.0;
					double firstAfterDTS  = 0.0;
					double totalDuration  = 0.0;
					if (mEnablePtsRestamp && !samples.empty())
					{
						firstBeforeDTS = samples.front().mDts;
						firstAfterDTS  = samples.front().mDts + fragmentPTSoffset;
					}
					for (auto& sample : samples)
					{
						// Re-checked on every iteration: abort() can be called from
						// another thread (e.g. alongside Stop()/Flush() invalidating
						// the injection generation) while this loop is mid-fragment.
						// Without this check, a bailed-out sample would simply be
						// followed by the next sample re-entering the same blocked/
						// gated injection path.
						if (mAborted.load(std::memory_order_relaxed))
						{
							AAMPLOG_WARN("Aborted mid-segment - stopping sample injection for type:%d position: %f (sent %zu/%zu samples)",
								mMediaType, position, sampleIndex, totalSamples);
							ret = false;
							break;
						}
						if (mEnablePtsRestamp)
						{
							const double beforeDTS = sample.mDts;
							sample.mPts += fragmentPTSoffset;
							sample.mDts += fragmentPTSoffset;
							// Carry the applied restamp as a display-timing correction
							// for subtitles.
							sample.mDisplayOffsetMs = static_cast<int64_t>(fragmentPTSoffset * 1000.0);
							totalDuration += sample.mDuration;
							// Log the restamping if enabled. This can be helpful for debugging and verifying correct behavior, but may cause log flooding for large segments.
							if (mEnablePtsRestampLogging)
							{
								const uint32_t timeScale = mMp4Demux->GetTimeScale();
								AAMPLOG_INFO("[RestampPts][%s] timeScale %u beforeDTS %.3f afterDTS %.3f duration %.3f",
								GetMediaTypeName(mMediaType),
								timeScale,
								beforeDTS * timeScale,
								sample.mDts * timeScale,
								sample.mDuration * timeScale);
							}
						}
						++sampleIndex;
						bool morePending = (sampleIndex < totalSamples);
						mAamp->SendStreamTransfer(mMediaType, std::move(sample), morePending);
					}
					if (mEnablePtsRestamp && sampleIndex > 0)
					{
						const uint32_t timeScale = mMp4Demux->GetTimeScale();
						AAMPLOG_INFO("[%s] timeScale %u before %" PRIu64 " after %" PRIu64 " duration %" PRIu64 " mp4demux",
							GetMediaTypeName(mMediaType),
							timeScale,
							static_cast<uint64_t>(firstBeforeDTS * timeScale),
							static_cast<uint64_t>(firstAfterDTS  * timeScale),
							static_cast<uint64_t>(totalDuration  * timeScale));
					}
				}
			}
			else
			{
				auto codecInfo = mMp4Demux->GetCodecInfo();
				if (codecInfo.mCodecFormat != GST_FORMAT_INVALID &&
					codecInfo.mCodecFormat != GST_FORMAT_UNKNOWN)
				{
					// Invoke SetStreamCaps for proper codec info
					AAMPLOG_INFO("Updating codecInfo with format:%d", codecInfo.mCodecFormat);
					mAamp->SetStreamCaps(mMediaType, std::move(codecInfo));
				}
				else
				{
					AAMPLOG_ERR("No samples for type:%d and invalid codec format:%d", mMediaType, codecInfo.mCodecFormat);
					ret = false;
				}
				// Init segments can carry discontinuity without samples.
				// Pre-mark discontinuity so next data sample avoids cross-period PTS delta spikes.
				if (ret && mIsTrickMode && isInit && discontinuous)
				{
					HandleTrickModeDiscontinuity();
				}
			}
		}
	}
	else
	{
		AAMPLOG_ERR("Demuxer instance(%p) is invalid or buffer is empty (size=%zu)", static_cast<void*>(mMp4Demux.get()), buffer.size());
		ret = false;
	}
	ptsError = false;
	return ret;
}

/**
 * @brief Check whether MP4 demux performs PTS restamping internally.
 * @return true if internal PTS restamping is configured and active
 */
bool AampMp4Demuxer::getPTSRestampStatus() const
{
	return mEnablePtsRestamp;
}
