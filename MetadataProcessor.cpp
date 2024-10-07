/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#include "MetadataProcessor.hpp"

#include "isobmff/isobmffprocessor.h"
#include "tsprocessor.h"

#include "ID3Metadata.hpp"

#include "StreamAbstractionAAMP.h"

#include <iomanip>
#include <assert.h>

class CachedFragment;

namespace aamp
{

IsoBMFFMetadataProcessor::IsoBMFFMetadataProcessor(id3_callback_t id3_hdl,
	ptsoffset_update_t ptsoffset_callback, std::weak_ptr<IsoBmffProcessor> video_processor)
	: MetadataProcessorIntf(id3_hdl, ptsoffset_callback),
	MetadataProcessorImpl(video_processor),
	processPTSComplete(false)
{ }

void IsoBMFFMetadataProcessor::ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity_pending,
		const double proc_position,
		bool & ptsError,
		const std::string & uri)
{
	AAMPLOG_INFO(" [metadata][%p] Processing metadata.", this);
	AAMPLOG_INFO(" [metadata][%p] - Starting processing fragment - uri: %s", this, uri.c_str());

	char * data_ptr = const_cast<char *>(cachedFragment->fragment.GetPtr());
	auto data_len = cachedFragment->fragment.GetLen();

	if (discontinuity_pending && mPtsOffsetUpdate)
	{
		AAMPLOG_INFO(" [metadata][%p] - Processing discontinuity with current PTS: %" PRIu64 " | %f", this, mCurrentMaxPTS, mCurrentMaxPTS_s);

		mPtsOffsetUpdate(mCurrentMaxPTS_s, false);
		mCurrentMaxPTS = 0;
		mCurrentMaxPTS_s = 0.;
	}

	if (!processPTSComplete)
	{
		if (!SetTuneTimePTS())
		{
			return;
		}
	}

	AAMPLOG_INFO(" [metadata][%p] Has valid PTS, processing the fragment", this);

	ProcessID3Metadata(type, data_ptr, data_len);
}

bool IsoBMFFMetadataProcessor::SetTuneTimePTS()
{
	bool ret = false;

	AAMPLOG_INFO(" [metadata] Setting tune time PTS...");

	// Wait for video to parse PTS
	if (!processPTSComplete)
	{
		AAMPLOG_INFO(" [metadata][%p] Trying to obtain base PTS value.", this);

		if (auto video = mVideoProcessor.lock())
		{
			const auto pts = video->GetBasePTS();
			if (pts.second)
			{
				mBasePTS = pts.first;
				processPTSComplete = true;
				ret = true;

				AAMPLOG_INFO(" [metadata][%p]  Base PTS value: %" PRIu64 " ", this, pts.first);
			}
			else
			{
				AAMPLOG_INFO(" [metadata] Base PTS not yet available.");
			}
		}
		else
		{
			AAMPLOG_WARN(" [metadata][%p] Cannot lock() video processor!", this);
		}

		AAMPLOG_INFO(" [metadata][%p] Tune time PTS processing completed.", this);
	}

	AAMPLOG_WARN(" [metadata][%p] Tune time result: %" PRIu64 " | %s", this, mBasePTS, (processPTSComplete ? "true" : "false"));

	return ret;
}

void IsoBMFFMetadataProcessor::ProcessID3Metadata(AampMediaType type, const char * data_ptr, size_t data_len)
{
	namespace aih = aamp::id3_metadata::helpers;

	if (data_ptr)
	{
		uint8_t * seg_buffer = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(data_ptr));

		IsoBmffBuffer buffer;
		buffer.setBuffer(seg_buffer, data_len);
		buffer.parseBuffer();
		if(!buffer.isInitSegment())
		{
			uint8_t* message = nullptr;
			uint32_t messageLen = 0;
			char * schemeIDUri = nullptr;
			uint8_t* value = nullptr;
			uint64_t presTime = 0;
			uint32_t timeScale = 0;
			uint32_t eventDuration = 0;
			uint32_t id = 0;
			if (buffer.getEMSGData(message, messageLen, schemeIDUri, value, presTime, timeScale, eventDuration, id))
			{
				if (message && messageLen > 0 && aih::IsValidHeader(message, messageLen))
				{
					AAMPLOG_TRACE(" Found ID3 metadata[%d]", type);

					AAMPLOG_INFO(" packet size: %zu | message size: %u", data_len, messageLen);
					std::stringstream ss;
					ss << "Found ID3 metadata - PTS: " << presTime << " - timeScale: " << timeScale << " - duration: " << eventDuration;
					AAMPLOG_INFO(" %s", ss.str().c_str());

					constexpr bool dbg_print{false};
					if (dbg_print)
					{
						size_t curOffset = 0;
						while (curOffset < data_len)
						{
							uint8_t * box_ptr = seg_buffer + curOffset;
							Box *box = Box::constructBox(box_ptr, (uint32_t)(data_len - curOffset), false, -1);

							box->setOffset((uint32_t)curOffset);
							const auto box_size = box->getSize();

							if (IS_TYPE(box->getType(), Box::EMSG))
							{
								uint8_t * ptr = seg_buffer + curOffset;
								const uint8_t * src_box_ptr = ptr;
								std::stringstream ss;

								ss << std::setfill ('0') << std::setw(2) << std::hex;
								for (auto i = 0; i < box_size; i++, box_ptr++)
								{
									ss << static_cast<uint16_t>(*box_ptr) << " ";
								}

								AAMPLOG_INFO(" EMSG box %p -> %p", src_box_ptr, box_ptr);
								AAMPLOG_INFO(" EMSG box [%zu][%u]: %s", curOffset, box_size, ss.str().c_str());
							}
							else
							{
								AAMPLOG_INFO(" %s box [%zu][%u]", box->getType(), curOffset, box_size);
							}

							curOffset += box->getSize();
						}
					}

					const double delta_pts = static_cast<double>(presTime - mBasePTS) / static_cast<double>(timeScale);

					AAMPLOG_INFO(" Found ID3 metadata - Delta PTS (s): %lf", delta_pts);

					if (mID3Handler)
					{
						const SegmentInfo_t info {delta_pts, delta_pts, static_cast<double>(eventDuration)};
						mID3Handler(type, message, messageLen, std::move(info), schemeIDUri );
					}
					else
					{
						AAMPLOG_ERR(" Metadata handler is undefined!");
					}
				}
			}
		}
	}
}



TSMetadataProcessor::TSMetadataProcessor(id3_callback_t id3_hdl,
	ptsoffset_update_t ptsoffset_callback,
	std::shared_ptr<TSProcessor> video_processor)
	: MetadataProcessorIntf(id3_hdl, ptsoffset_callback),
	MetadataProcessorImpl(std::move(video_processor))
{
	mProcessor = aamp_utils::make_unique<aamp_ts::TSFragmentProcessor>();
}

void TSMetadataProcessor::ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity_pending,
		double proc_position,
		bool & ptsError,
		const std::string & uri // Debug only
	)
{
	AAMPLOG_INFO(" [metadata][%p] - Starting processing fragment - uri: %s", this, uri.c_str());

	if (discontinuity_pending)
	{
		AAMPLOG_INFO(" [metadata][%p] - Processing discontinuity with current PTS: %" PRIu64 " | %f",
			this, mCurrentMaxPTS, mCurrentMaxPTS_s);

		if (mPtsOffsetUpdate)
			mPtsOffsetUpdate(mCurrentMaxPTS_s, false);
		mCurrentMaxPTS = 0;
		mCurrentMaxPTS_s = 0.;
	}

	MediaProcessor::process_fcn_t processor = [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
	{
		// Early emission of ID3 metadata event
		if (type == eMEDIATYPE_DSM_CC)
		{
			if (mID3Handler)
			{
				namespace aih = aamp::id3_metadata::helpers;
				const uint8_t * data_ptr = static_cast<const uint8_t*>(buf.data());
				const auto data_len = buf.size();

				if (aih::IsValidMediaType(type) &&
					aih::IsValidHeader(data_ptr, data_len))
				{
					mID3Handler(type, data_ptr, data_len, info, nullptr);
				}
			}
		}
		else if ((type == eMEDIATYPE_AUDIO) || (type == eMEDIATYPE_VIDEO))
		{
			// Update the local max PTS value
			if (info.pts_s > mCurrentMaxPTS_s)
			{
				AAMPLOG_DEBUG(" [%d] Updating max PTS %f -> %f", static_cast<int16_t>(type), mCurrentMaxPTS_s, info.pts_s);
				mCurrentMaxPTS_s = info.pts_s;
			}
		}
	};

	const AampGrowableBuffer & frag_ptr = cachedFragment->fragment;
	mProcessor->ProcessFragment(frag_ptr,
		proc_position,
		cachedFragment->duration,
		discontinuity_pending,
		processor);

	AAMPLOG_INFO(" [metadata][%p] - Max PTS: %f", this, mCurrentMaxPTS_s);
	AAMPLOG_INFO(" [metadata][%p] - Terminated processing fragment - uri: %s", this, uri.c_str());

}

}
