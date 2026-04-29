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
 * @file AampMp4Demuxer.h
 * @brief Header file for MP4 Demuxer
 */

#ifndef __AAMPMP4DEMUXER_H__
#define __AAMPMP4DEMUXER_H__

#include "mediaprocessor.h"
#include "MP4Demux.h"
#include "priv_aamp.h"
#include <memory>

class AampMp4Demuxer : public MediaProcessor
{
public:
	/**
	 * @brief MP4 Demuxer Constructor
	 * @param[in] aamp - Pointer to the PrivateInstanceAAMP
	 * @param[in] type - Media type (audio/video/subtitle)
	 * @param[in] enablePtsRestamp - Flag to enable PTS restamping
	 */
	AampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type, bool enablePtsRestamp = false);
	~AampMp4Demuxer();


	AampMp4Demuxer(const AampMp4Demuxer&) = delete;
	AampMp4Demuxer& operator=(const AampMp4Demuxer&) = delete;

	/**
	 * @brief given TS media segment (not yet injected), extract and report first PTS
	 */
	double getFirstPts( const std::vector<uint8_t>& buffer ) override { return 0.0; };

	/**
	 * @brief optionally specify new pts offset to apply for subsequently injected TS media segments
	 */
	void setPtsOffset( double ptsOffset ) override { };

	/**
	 * @fn sendSegment
	 *
	 * @param[in] buffer - fragment data; ownership is transferred (moved) into
	 *                    an internal shared_ptr.  The buffer will be empty on
	 *                    return — callers must not access it afterwards.
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] fragmentPTSoffset - offset PTS of fragment
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return true if fragment was sent, false otherwise
	 */
	bool sendSegment(std::vector<uint8_t>&& buffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
					bool isInit, process_fcn_t processor, bool &ptsError) override;

	/**
	 * @brief Set playback rate
	 *
	 * @param[in] rate - playback rate
	 * @param[in] mode - playback mode
	 * @return void
	 */
	void setRate(double rate, PlayMode mode) override;

	/**
	 * @brief Enable or disable throttle
	 *
	 * @param[in] enable - throttle enable/disable
	 * @return void
	 */
	void setThrottleEnable(bool enable) override { }

	/**
	 * @brief Set frame rate for trickmode
	 *
	 * @param[in] frameRate - rate per second
	 * @return void
	 */
	void setFrameRateForTM (int frameRate) override;

	/**
	 * @brief Abort all operations
	 *
	 * @return void
	 */
	void abort() override;

	/**
	 * @brief Reset all variables
	 *
	 * @return void
	 */
	void reset() override;

	/**
	 * @brief Function to abort wait for injecting the segment
	 */
	void abortInjectionWait() override { }

	/**
	 * @brief Function to enable/disable the processor
	 * @param[in] enable true to enable, false otherwise
	 */
	void enable(bool enable) override { }

	/**
	 * @brief Function to set a track offset for restamping
	 * @param[in] offset offset value in seconds
	 */
	void setTrackOffset(double offset) override { }

private:
	enum class Mp4TrickPhase
	{
		UNDEF,
		INIT,
		FIRST_SAMPLE,
		STEADY
	};

	/**
	 * @brief Apply trickmode PTS restamping to a sample
	 * @param[in,out] sample - Sample to restamp
	 * @param[in] duration - Fragment duration
	 */
	void TrickmodePtsRestamp(AampMediaSample& sample, double duration);

	/**
	 * @brief Apply trickmode PTS offset similar to qtdemux approach
	 * Uses first PTS as offset and applies rate-based adjustment
	 * @param[in,out] sample - Sample to adjust
	 * @param[in] duration - Fragment duration
	 */
	void TrickmodePtsOffset(AampMediaSample& sample, double duration);

	std::unique_ptr<Mp4Demux> mMp4Demux;
	PrivateInstanceAAMP* mAamp;
	AampMediaType mMediaType;
	bool mEnablePtsRestamp; // Flag to enable PTS restamping
	// A separate flag to enable logging for PTS restamping for better control.
	bool mEnablePtsRestampLogging {false}; // Flag to enable logging for PTS restamping
	
	// Trickmode state variables
	int mTrickPlayFPS {0};					/**< Trickplay frames per second */
	double mRate {1.0};						/**< Current playback rate */
	bool mIsTrickMode {false};				/**< True if in trickmode (rate != 1.0) */
	double mLastSamplePts {0.0};			/**< PTS of the previous sample, used in trick modes */
	double mRestampedPts {0.0};				/**< Restamped PTS of the sample, used in trick modes */
		
	Mp4TrickPhase mTrickPhase {Mp4TrickPhase::UNDEF}; /**< Current trick mode state */
	double mLastTrickRate {0.0}; /**< Last used trickplay rate for state reset */
};

#endif /* __AAMPMP4DEMUXER_H__ */