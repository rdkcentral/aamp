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

	// Apply restamped PTS and duration to the sample
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

	AAMPLOG_INFO("[%s][FINAL] Sending sample - PTS=%.6f DTS=%.6f Duration=%.6f",
		GetMediaTypeName(mMediaType),
		sample.mPts,
		sample.mDts,
		sample.mDuration);
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
bool AampMp4Demuxer::sendSegment(std::vector<uint8_t>& buffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
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

		// Combine trickmode rate change and init fragment handling for state reset
		if (isTrickMode && ((mAamp->rate != mLastTrickRate && isInit) || mTrickmodeState == TrickmodeState::UNDEF))
		{
			mTrickmodeState = TrickmodeState::FIRST_SAMPLE;
			mRestampedPts = 0.0;
			mLastSamplePts = 0.0;
			mLastTrickRate = mAamp->rate;
			AAMPLOG_INFO("Trickmode state reset: rate=%.2f, isInit=%d, state set to FIRST_SAMPLE", mAamp->rate, (int)isInit);
		}
		
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
				if (isTrickMode)
				{
					for (auto& sample : samples)
					{
						
						// Apply trickmode PTS restamping to the sample. This modifies the sample timestamps to create a smooth trickplay experience, 
						// especially for fast-forward and rewind modes. The restamping logic is based on a state machine that handles the first sample differently to establish a baseline for subsequent samples.
						TrickmodePtsRestamp(sample, duration);						
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
					mLastTrickRate = mAamp->rate;
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
