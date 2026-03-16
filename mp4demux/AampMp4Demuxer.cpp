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


/**
 * @brief MP4 Demuxer Constructor
 * @param[in] aamp - Pointer to the PrivateInstanceAAMP
 * @param[in] type - Media type (audio/video/subtitle)
 * @param[in] enablePtsRestamp - Flag to enable PTS restamping
 */
AampMp4Demuxer::AampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type, bool enablePtsRestamp) :
	MediaProcessor(), mMp4Demux(aamp_utils::make_unique<Mp4Demux>()), mAamp(aamp),
	mMediaType(type), mEnablePtsRestamp(enablePtsRestamp)
{
	AAMPLOG_MIL("Created AampMp4Demuxer(%p) for type %d, PTS restamp: %s", this, type, enablePtsRestamp ? "enabled" : "disabled");
	// TODO: Should we limit the media types here to only video/audio?
	// Make restamp logging configurable as it might cause log flooding, since logs will come for each demuxed frames per fragment
	mEnablePtsRestampLogging = mAamp->mConfig->IsConfigSet(eAAMPConfig_EnablePTSReStampLogging);
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
 * @fn sendSegment
 *
 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
 * @param[in] position - position of fragment
 * @param[in] duration - duration of fragment
 * @param[in] fragmentPTSoffset - offset PTS of fragment
 * @param[in] discontinuous - true if discontinuous fragment
 * @param[in] isInit - flag for buffer type (init, data)
 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
 * @param[out] ptsError - flag indicates if any PTS error occurred
 * @return true if fragment was sent, false otherwise
 */
bool AampMp4Demuxer::sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
								bool isInit, process_fcn_t processor, bool &ptsError)
{
	bool ret = true;
	(void) processor;
	if (mMp4Demux.get() && pBuffer && pBuffer->data() && pBuffer->size())
	{
		AAMPLOG_INFO("Processing segment with type:%d position: %f, duration: %f, isInit: %d", mMediaType, position, duration, isInit);
		ret = mMp4Demux->Parse(pBuffer->data(), pBuffer->size());
		if (!ret)
		{
			AAMPLOG_ERR("Failed to parse MP4 segment [err:%d] for type:%d position: %f, duration: %f, isInit: %d", mMp4Demux->GetLastError(), mMediaType, position, duration, isInit);
		}
		else
		{
			auto samples = mMp4Demux->GetSamples();
			if (samples.size() > 0)
			{
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
					mAamp->SendStreamTransfer(mMediaType, sample);
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
		AAMPLOG_ERR("Demuxer instance(%p) is invalid or buffer invalid (%p, %p, %zu)", mMp4Demux.get(), pBuffer, pBuffer ? pBuffer->data() : nullptr, pBuffer ? pBuffer->size() : 0);
		ret = false;
	}
	ptsError = false;
	return ret;
}
