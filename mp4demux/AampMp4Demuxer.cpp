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
	// Initialize trickplay FPS from config
	mTrickPlayFPS = mAamp->mConfig->GetConfigValue(eAAMPConfig_VODTrickPlayFPS);
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
 * @brief Apply trickmode PTS restamping to a sample
 * @param[in,out] sample - Sample to restamp
 * @param[in] duration - Fragment duration
 */
void AampMp4Demuxer::TrickmodePtsRestamp(AampMediaSample& sample, double duration)
{
	// Store original values for logging
	double originalPts = sample.mPts;
	double originalDts = sample.mDts;
	double originalDuration = sample.mDuration;
	double fragmentPtsDelta = 0.0;
	bool applyRestamping = true;

	// Log before restamping
	AAMPLOG_INFO("[%s][BEFORE] State=%d PTS=%.6f DTS=%.6f Duration=%.6f LastSamplePts=%.6f rate=%.2f",
		GetMediaTypeName(mMediaType),
		static_cast<int>(mTrickmodeState),
		originalPts,
		originalDts,
		originalDuration,
		mLastSamplePts,
		mAamp->rate);

	switch (mTrickmodeState)
	{
		case TrickmodeState::INIT:
			// Init fragment detected - transition to FIRST_SAMPLE for next media fragment
			// No restamping needed for init fragment samples (if any)
			mTrickmodeState = TrickmodeState::FIRST_SAMPLE;
			applyRestamping = false;
			AAMPLOG_INFO("[%s] Init fragment processed, state transitioned to FIRST_SAMPLE", GetMediaTypeName(mMediaType));
			break;

		case TrickmodeState::FIRST_SAMPLE:
			// First sample: estimate duration based on rate and trickPlayFPS
			// Use MAX to avoid too small a number (minimum 0.25 seconds)
			mRestampedDuration = MAX(duration / std::fabs(mAamp->rate), 1.0 / mTrickPlayFPS);
			mRestampedPts = 0.0;
			AAMPLOG_INFO("[%s] FIRST_SAMPLE: FragmentDuration=%.6f RestampedDuration=%.6f RestampedPts=%.6f",
				GetMediaTypeName(mMediaType),
				duration,
				mRestampedDuration,
				mRestampedPts);
			mTrickmodeState = TrickmodeState::STEADY;
			break;		
			
		case TrickmodeState::STEADY:
			// Calculate the duration between the current sample and the previous sample
			// and divide it by the rate to determine the restamped duration
			fragmentPtsDelta = fabs(sample.mPts - mLastSamplePts);
			mRestampedDuration = fragmentPtsDelta / std::fabs(mAamp->rate);
			mRestampedPts += mRestampedDuration;
			AAMPLOG_INFO("[%s] STEADY: PtsDelta=%.6f RestampedDuration=%.6f RestampedPts=%.6f",
				GetMediaTypeName(mMediaType),
				fragmentPtsDelta,
				mRestampedDuration,
				mRestampedPts);
			break;

		case TrickmodeState::UNDEF:
		
		default:
			// Should not happen, but handle gracefully
			AAMPLOG_WARN("[%s] Unexpected trickmode state %d in trickmode", 
				GetMediaTypeName(mMediaType),
				static_cast<int>(mTrickmodeState));
			mRestampedDuration = MAX(duration / std::fabs(mAamp->rate), 1.0 / mTrickPlayFPS);
			mRestampedPts = 0.0;
			mTrickmodeState = TrickmodeState::STEADY;
			break;
	}

	// Store the current sample PTS for next iteration
	mLastSamplePts = sample.mPts;

	// Apply restamped PTS and duration to the sample (skip for INIT state)
	if (applyRestamping)
	{
		sample.mPts = mRestampedPts;
		sample.mDts = mRestampedPts;
		sample.mDuration = mRestampedDuration;
		AAMPLOG_INFO("[%s][AFTER_RESTAMP] PTS=%.6f DTS=%.6f Duration=%.6f (Delta: PTS=%.6f DTS=%.6f Duration=%.6f)",
			GetMediaTypeName(mMediaType),
			sample.mPts,
			sample.mDts,
			sample.mDuration,
			sample.mPts - originalPts,
			sample.mDts - originalDts,
			sample.mDuration - originalDuration);
	}

	AAMPLOG_INFO("[%s][FINAL] Sending sample - PTS=%.6f DTS=%.6f Duration=%.6f",
		GetMediaTypeName(mMediaType),
		sample.mPts,
		sample.mDts,
		sample.mDuration);
}

/**
 * @brief Apply trickmode PTS offset similar to qtdemux approach
 * Uses first PTS as offset and applies rate-based adjustment with jump detection
 * @param[in,out] sample - Sample to adjust
 * @param[in] duration - Fragment duration
 */
void AampMp4Demuxer::TrickmodePtsOffset(AampMediaSample& sample, double duration)
{
	double pts = sample.mPts;
	double dts = sample.mDts;
	double rate = mAamp->rate;
	
	AAMPLOG_INFO("[TrickmodePtsOffset][%s] rate=%.2f mAampBasePts=%.6f", GetMediaTypeName(mMediaType), rate, mAampBasePts);
	
	// Initialize base PTS on first sample of trickmode session
	// mAampBasePts is reset to -1.0 when init fragment is processed or when exiting trickmode
	if (mAampBasePts < 0.0)
	{
		mAampBasePts = pts;
		mAampPtsOffset = 0.0;
		AAMPLOG_INFO("[TrickmodePtsOffset][%s] First media sample - initializing base_pts=%.6f aamp_pts_offset=%.6f rate=%.2f",
			GetMediaTypeName(mMediaType), mAampBasePts, mAampPtsOffset, rate);
	}
	
	if (rate > 0)
	{
		// Forward playback or fast-forward
		AAMPLOG_INFO("[TrickmodePtsOffset][%s] Normal or fast forward rate %.2f", GetMediaTypeName(mMediaType), rate);
		
		// For normal playback, skip frames that are before base PTS
		if ((rate == 1.0) && ((pts < mAampBasePts) || (dts < mAampBasePts)))
		{
			AAMPLOG_WARN("[TrickmodePtsOffset][%s] Skipping frame - rate %.2f orig pts %.6f base pts %.6f",
				GetMediaTypeName(mMediaType), rate, pts, mAampBasePts);
			// Mark sample as invalid by setting PTS to -1
			sample.mPts = -1.0;
			return;
		}
		
		double newDts = mAampPtsOffset + (dts - mAampBasePts) / rate;
		double newPts = mAampPtsOffset + (pts - mAampBasePts) / rate;

		AAMPLOG_INFO("[TrickmodePtsOffset][%s] Before jump check - rate %.2f orig pts %.6f new pts %.6f last pts %.6f",
			GetMediaTypeName(mMediaType), rate, pts, newPts, mAampLastPts);
		
		// For trickplay, check for PTS jumps > 2 seconds
		if ((rate > 1.0) && (mAampLastPts >= 0.0))
		{
			AAMPLOG_INFO("[TrickmodePtsOffset][%s] Checking for PTS jumps - rate %.2f new pts %.6f last pts %.6f delta %.6f",
				GetMediaTypeName(mMediaType), rate, newPts, mAampLastPts, (newPts - mAampLastPts));

			if ((pts < mAampBasePts) ||
				(newPts <= mAampLastPts) ||
				((newPts > mAampLastPts) && ((newPts - mAampLastPts) > 2.0)))
			{
				AAMPLOG_WARN("[TrickmodePtsOffset][%s] PTS jump detected - rate %.2f new pts %.6f last pts %.6f",
					GetMediaTypeName(mMediaType), rate, newPts, mAampLastPts);	
				// Reset base_pts due to unexpected jump
				mAampBasePts = pts;
				
				// Guess next PTS based on FPS (PTS is in seconds, so calculate frame duration in seconds)
				int fps = (rate < mTrickPlayFPS) ? 1 : mTrickPlayFPS;
				mAampPtsOffset = mAampLastPts + (1.0 / fps);
				
				AAMPLOG_WARN("[TrickmodePtsOffset][%s] PTS jump detected - rate %.2f new pts %.6f last pts %.6f",
					GetMediaTypeName(mMediaType), rate, newPts, mAampLastPts);
				AAMPLOG_WARN("[TrickmodePtsOffset][%s] PTS jump - rate %.2f base pts %.6f aamp_pts_offset %.6f fps %d",
					GetMediaTypeName(mMediaType), rate, mAampBasePts, mAampPtsOffset, fps);
				
				newDts = mAampPtsOffset;
				newPts = mAampPtsOffset;
			}
		}
		
		sample.mDts = newDts;
		sample.mPts = newPts;
		AAMPLOG_INFO("[TrickmodePtsOffset][%s] Forward - rate %.2f orig pts %.6f restamped pts %.6f",
			GetMediaTypeName(mMediaType), rate, pts, sample.mPts);
	}
	else
	{
		// Reverse playback
		rate = -rate;
		double newDts = mAampPtsOffset + (mAampBasePts - dts) / rate;
		double newPts = mAampPtsOffset + (mAampBasePts - pts) / rate;
		
		// For trickplay, check for PTS jumps > 2 seconds
		if ((rate > 1.0) && (mAampLastPts >= 0.0))
		{
			if ((pts > mAampBasePts) ||
				(newPts <= mAampLastPts) ||
				((newPts > mAampLastPts) && ((newPts - mAampLastPts) > 2.0)))
			{
				// Reset base_pts due to unexpected jump
				mAampBasePts = pts;
				
				// Guess next PTS based on FPS (PTS is in seconds, so calculate frame duration in seconds)
				int fps = (rate < mTrickPlayFPS) ? 1 : mTrickPlayFPS;
				mAampPtsOffset = mAampLastPts + (1.0 / fps);
				
				AAMPLOG_WARN("[TrickmodePtsOffset][%s] PTS jump detected - rate %.2f new pts %.6f last pts %.6f",
					GetMediaTypeName(mMediaType), rate, newPts, mAampLastPts);
				AAMPLOG_WARN("[TrickmodePtsOffset][%s] PTS jump - rate %.2f base pts %.6f aamp_pts_offset %.6f fps %d",
					GetMediaTypeName(mMediaType), rate, mAampBasePts, mAampPtsOffset, fps);
				
				newDts = mAampPtsOffset;
				newPts = mAampPtsOffset;
			}
		}
		
		sample.mDts = newDts;
		sample.mPts = newPts;
		AAMPLOG_INFO("[TrickmodePtsOffset][%s] Reverse - rate %.2f orig pts %.6f restamped pts %.6f",
			GetMediaTypeName(mMediaType), -rate, pts, sample.mPts);
	}
	
	// Update last PTS
	mAampLastPts = sample.mPts;
	
	// Adjust duration by rate
	//sample.mDuration = sample.mDuration / fabs(mAamp->rate);
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
	if (mMp4Demux && !buffer.empty())
	{
		// Move the caller's buffer into a shared_ptr and pass ownership into
		// Parse(), which stamps each sample's mData (via aliasing shared_ptr)
		// so each sample keeps the segment buffer alive for its lifetime.
		auto segment = std::make_shared<std::vector<uint8_t>>(std::move(buffer));
		AAMPLOG_INFO("Processing segment with type:%d position: %f, duration: %f, isInit: %d", mMediaType, position, duration, isInit);
		
		// Check if we are in trickmode (fast-forward or rewind)
		bool isTrickMode = (mAamp->rate > AAMP_NORMAL_PLAY_RATE) || (mAamp->rate < 0);
		
		// Handle init fragment for trickmode state management
		if (isInit && isTrickMode)
		{
			// Init fragment is injected after any rate change
			// Set INIT state to indicate we are processing init fragment
			if (mTrickmodeState == TrickmodeState::UNDEF)
			{
				mTrickmodeState = TrickmodeState::INIT;
				// Reset offset variables for new trickmode session
				mAampBasePts = -1.0;
				mAampPtsOffset = 0.0;
				mAampLastPts = -1.0;  // Use sentinel value to avoid false jump detection on first sample
				AAMPLOG_INFO("Trickmode init fragment detected, state set to INIT, all offset variables reset");
			}
		}
		
		ret = mMp4Demux->Parse(std::make_shared<std::vector<uint8_t>>(buffer));
		if (!ret)
		{
			AAMPLOG_ERR("Failed to parse MP4 segment [err:%d] for type:%d position: %f, duration: %f, isInit: %d", mMp4Demux->GetLastError(), mMediaType, position, duration, isInit);
		}
		else
		{
			auto samples = mMp4Demux->GetSamples();
			if (!samples.empty())
			{
				if (isTrickMode)
				{
					for (auto& sample : samples)
					{
						// Choose trickmode method based on PTS restamp configuration
						// If PTS restamping is disabled, use qtdemux-style offset approach
						// Otherwise, use the custom restamping logic
						if (!mEnablePtsRestamp)
						{
							// qtdemux-style: use first PTS as offset and apply rate adjustment
							TrickmodePtsOffset(sample, duration);
							
							// Skip sample if marked invalid (PTS < 0)
							if (sample.mPts < 0.0)
							{
								AAMPLOG_INFO("[TrickmodePtsOffset][%s] Skipping invalid sample", GetMediaTypeName(mMediaType));
								continue;
							}
						}
						else
						{
							// Custom AAMP restamping with state machine
							TrickmodePtsRestamp(sample, duration);
						}
						
						// Send the sample to the pipeline
						mAamp->SendStreamTransfer(mMediaType, std::move(sample));
					}
				}
				else
				{
					// Normal playback mode - reset trickmode state
					mTrickmodeState = TrickmodeState::UNDEF;
					mRestampedPts = 0.0;
					mLastSamplePts = 0.0;
					mAampBasePts = -1.0;
					mAampPtsOffset = 0.0;
					mAampLastPts = -1.0;  // Use sentinel value
					for (auto& sample : samples)
					{
						// Apply PTS offset if restamping is enabled. This modifies the sample timestamps before sending them to AAMP, which will use the adjusted values for playback timing.
						if (mEnablePtsRestamp)
						{
							double beforeDTS = sample.mDts;
							sample.mPts += fragmentPTSoffset;
							sample.mDts += fragmentPTSoffset;
							// Log the restamping if enabled. This can be helpful for debugging and verifying correct behavior, but may cause log flooding for large segments.
							if (mEnablePtsRestampLogging)
							{
								uint32_t timeScale = mMp4Demux->GetTimeScale();
								AAMPLOG_INFO("[RestampPts][%s] timeScale %u beforeDTS %.3f afterDTS %.3f duration %.3f",
								GetMediaTypeName(mMediaType),
								timeScale,
								beforeDTS * timeScale,
								sample.mDts * timeScale,
								sample.mDuration * timeScale);
							}
						}
						mAamp->SendStreamTransfer(mMediaType, std::move(sample));
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
