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
 * @brief MP4 Demuxer constructor
 */
AampMp4Demuxer::AampMp4Demuxer(PrivateInstanceAAMP* aamp, AampMediaType type) :
	MediaProcessor(), mMp4Demux(aamp_utils::make_unique<Mp4Demux>()), mAamp(aamp), mMediaType(type)
{
	AAMPLOG_WARN("Created AampMp4Demuxer(%p) for type %d", this, type);
	// TODO: Should we limit the media types here to only video/audio?
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
 * @param[in] pBuffer - Pointer to the buffer vector
 * @param[in] position - position of fragment
 * @param[in] duration - duration of fragment
 * @param[in] fragmentPTSoffset - offset PTS of fragment
 * @param[in] discontinuous - true if discontinuous fragment
 * @param[in] isInit - flag for buffer type (init, data)
 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
 * @param[out] ptsError - flag indicates if any PTS error occurred
 * @return true if fragment was sent, false otherwise
 */
bool AampMp4Demuxer::sendSegment(std::vector<uint8_t>* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
								bool isInit, process_fcn_t processor, bool &ptsError)
{
	bool ret = true;
	(void) processor;
	if (mMp4Demux.get() && pBuffer && !pBuffer->empty())
	{
		AAMPLOG_INFO("Processing segment with type:%d position: %f, duration: %f, isInit: %d", mMediaType, position, duration, isInit);
		ret = mMp4Demux->Parse(reinterpret_cast<char*>(pBuffer->data()), pBuffer->size());
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