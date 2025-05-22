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
namespace aamp
{

IsoBMFFMetadataProcessor::IsoBMFFMetadataProcessor(id3_callback_t id3_hdl,
	ptsoffset_update_t ptsoffset_callback, std::weak_ptr<IsoBmffProcessor> video_processor)
	: MetadataProcessorIntf(id3_hdl, ptsoffset_callback), MetadataProcessorImpl(video_processor),
	processPTSComplete(false)
{}

void IsoBMFFMetadataProcessor::ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity, 
		const double proc_position,
		bool & ptsError, 
		const std::string & uri)
{}

bool IsoBMFFMetadataProcessor::SetTuneTimePTS()
{
	return true;
}

void IsoBMFFMetadataProcessor::ProcessID3Metadata(AampMediaType type, const char * data_ptr, size_t data_len)
{}

TSMetadataProcessor::TSMetadataProcessor(id3_callback_t id3_hdl,
	ptsoffset_update_t ptsoffset_callback,
	std::shared_ptr<TSProcessor> video_processor)
	: MetadataProcessorIntf(id3_hdl, ptsoffset_callback), MetadataProcessorImpl(std::move(video_processor))
{}

void TSMetadataProcessor::ProcessFragmentMetadata(const CachedFragment * cachedFragment,
		AampMediaType type,
		bool discontinuity, 
		const double proc_position,
		bool & ptsError, 
		const std::string & uri)
{}
}
