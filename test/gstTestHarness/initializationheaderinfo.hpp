/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#ifndef initializationheaderinfo_hpp
#define initializationheaderinfo_hpp

#include <cstdint>
#include <stdlib.h>

class InitializationHeaderInfo
{
public:
	// audio
	uint16_t channel_count;
	uint16_t samplesize;
	uint16_t samplerate;
	uint8_t object_type_id;
	uint8_t stream_type;
	uint8_t upStream;
	uint16_t buffer_size;
	uint32_t maxBitrate;
	uint32_t avgBitrate;
	
	// video
	uint16_t width;
	uint16_t height;
	uint16_t frame_count;
	uint16_t depth;
	uint32_t horizresolution;
	uint32_t vertresolution;
	// common
	uint32_t stream_format;
	uint32_t data_reference_index;
	uint32_t codec_type;
	char *compressor_name;
	size_t codec_data_len;
	uint8_t *codec_data;
	
	InitializationHeaderInfo():
	channel_count(), samplesize(), samplerate(),
	width(), height(), frame_count(), depth(), horizresolution(), vertresolution(),
	stream_format(), data_reference_index(), codec_type(), compressor_name(), codec_data_len(), codec_data()
	{
	}
	
	~InitializationHeaderInfo()
	{
		if( codec_data )
		{
			free( codec_data );
		}
		if( compressor_name )
		{
			free( compressor_name );
		}
	}
};

#endif /* initializationheaderinfo_Hpp */
