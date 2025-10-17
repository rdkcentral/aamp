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

#ifndef __MP4_DEMUX_HPP__
#define __MP4_DEMUX_HPP__

#include <cstdint>
#include <stddef.h>
#include <vector>
#include <assert.h>
#include <inttypes.h>
#include <cstdio>
#include <cstring> // for memcpy
#include <string>
#include "AampLogManager.h"
#include "AampDemuxDataTypes.h" // for AampCodecInfo, AampPsshData, AampMediaSample

// convert multi-character constants like 'cenc' to equivalent 32 bit integer - pass as four character string
#define MultiChar_Constant(TEXT) ( \
(static_cast<uint32_t>(TEXT[0]) << 0x18) | \
(static_cast<uint32_t>(TEXT[1]) << 0x10) | \
(static_cast<uint32_t>(TEXT[2]) << 0x08) | \
(static_cast<uint32_t>(TEXT[3]) << 0x00) )

enum mp4LogLevel
{
	MP4_LOG_NONE = 0,
	MP4_LOG_ERROR = 1,
	MP4_LOG_WARNING = 2,
	MP4_LOG_INFO = 3,
	MP4_LOG_DEBUG = 4,
	MP4_LOG_VERBOSE = 5
};

#define MP4_LOGGER(level, ...) AAMPLOG_WARN(__VA_ARGS__)

class Mp4Demux
{
private:

	uint32_t stream_format;
	uint32_t data_reference_index;
	uint8_t iv_size;
	uint8_t crypt_byte_block;
	uint8_t skip_byte_block;
	uint8_t constant_iv_size;
	std::vector<uint8_t> constant_iv;
	
	uint32_t timescale;
	std::vector<AampMediaSample> samples;
	
	// encryption-specific data
	std::string default_kid;
	bool got_auxiliary_information_offset;
	uint64_t auxiliary_information_offset;
	uint32_t scheme_type; // 'cenc' or 'cbcs'
	uint32_t scheme_version;
	uint32_t original_media_type;
	std::vector<uint8_t> cenc_aux_info_sizes;
	std::vector<AampPsshData> protectionData;
	AampCodecInfo mCodecInfo;

	const uint8_t *moof_ptr; // base address for sample data
	const uint8_t *ptr; // parser state
	
	uint8_t version;
	uint32_t flags;
	uint64_t baseMediaDecodeTime;
	uint32_t fragment_duration;
	uint32_t track_id;
	uint64_t base_data_offset;
	uint32_t default_sample_description_index;
	uint32_t default_sample_duration;
	uint32_t default_sample_size;
	uint32_t default_sample_flags;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t duration;
	uint32_t rate;
	uint32_t volume;
	int32_t matrix[9];
	uint16_t layer;
	uint16_t alternate_group;
	uint32_t width_fixed;
	uint32_t height_fixed;
	uint16_t language;
	uint64_t sampleOffset;
	bool sencPresent;
	bool verbose;

	StreamOutputFormat GetStreamOutputFormatFromFourCC(const uint32_t fourCC)
	{
		switch (fourCC)
		{
			case MultiChar_Constant("avc1"):
				return FORMAT_VIDEO_ES_H264;
			case MultiChar_Constant("hvc1"):
			case MultiChar_Constant("hev1"):
				return FORMAT_VIDEO_ES_HEVC;
			case MultiChar_Constant("mp4a"):
				return FORMAT_AUDIO_ES_AAC;
			case MultiChar_Constant("ac-3"):
			case MultiChar_Constant("dac3"):
				return FORMAT_AUDIO_ES_AC3;
			case MultiChar_Constant("ec-3"):
			case MultiChar_Constant("dec3"):
				return FORMAT_AUDIO_ES_EC3;
			case MultiChar_Constant("ac-4"):
				return FORMAT_AUDIO_ES_AC4;
			default:
				return FORMAT_UNKNOWN;
		}
	}

	AampMediaType GetMediaTypeForStreamOutputFormat(const StreamOutputFormat format)
	{
		switch (format)
		{
			case FORMAT_VIDEO_ES_H264:
			case FORMAT_VIDEO_ES_HEVC:
			case FORMAT_VIDEO_ES_MPEG2:
				return eMEDIATYPE_VIDEO;
			case FORMAT_AUDIO_ES_MP3:
			case FORMAT_AUDIO_ES_AAC:
			case FORMAT_AUDIO_ES_AC3:
			case FORMAT_AUDIO_ES_EC3:
			case FORMAT_AUDIO_ES_ATMOS:
			case FORMAT_AUDIO_ES_AC4:
				return eMEDIATYPE_AUDIO;
			case FORMAT_SUBTITLE_WEBVTT:
			case FORMAT_SUBTITLE_TTML:
				return eMEDIATYPE_SUBTITLE;
			default:
				return eMEDIATYPE_DEFAULT;
		}
	}

	uint64_t ReadBytes( int n )
	{
		uint64_t rc = 0;
		for( int i=0; i<n; i++ )
		{
			rc <<= 8;
			rc |= *ptr++;
		}
		return rc;
	}
	
	uint16_t ReadU16()
	{
		return (uint16_t)ReadBytes(2);
	}
	
	uint32_t ReadU32()
	{
		return (uint32_t)ReadBytes(4);
	}
	
	int32_t ReadI32()
	{
		return (int32_t)ReadBytes(4);
	}
	
	uint64_t ReadU64()
	{
		return ReadBytes(8);
	}
	
	void ReadHeader( void )
	{
		version = *ptr++;
		flags = (uint32_t)ReadBytes(3);
	}
	
	void SkipBytes( size_t len )
	{
		ptr += len;
	}
	
	void parseOriginalFormat( void )
	{
		original_media_type = ReadU32();
		// For encv and enca formats
		if (mCodecInfo.mCodecFormat == FORMAT_UNKNOWN)
		{
			mCodecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC( original_media_type );
			mCodecInfo.mType = GetMediaTypeForStreamOutputFormat( mCodecInfo.mCodecFormat );
		}
	}
	
	void parseSchemeManagementBox( void )
	{
		ReadHeader();
		scheme_type = ReadU32(); // 'cenc' or 'cbcs'
		scheme_version = ReadU32();
	}
	
	void parseTrackEncryptionBox( void )
	{
		ReadHeader();

		ptr++; // skip
		uint8_t pattern = *ptr++;
		if( scheme_type == MultiChar_Constant("cbcs") )
		{
			crypt_byte_block = (pattern>>4) & 0xf;
			skip_byte_block = pattern & 0xf;
		}
		mCodecInfo.mIsEncrypted= *ptr++;
		iv_size = *ptr++;

		default_kid = std::string((char *)ptr,16);
		ptr += 16;
		if( scheme_type == MultiChar_Constant("cbcs") )
		{
			constant_iv_size = *ptr++;
			// TODO : Send proper error event, instead of assert
			assert( constant_iv_size==8 || constant_iv_size==16 );
			constant_iv = std::vector<uint8_t>(ptr, ptr + constant_iv_size);
			ptr += constant_iv_size;
		}
	}
	
	/*
	 12 24 "8de6242e-6601-5218-8841-ace2761b413f"	// kid
	 12 24 "2e9b8068-fa3a-c50f-4781-550aae5986ad"	// kid
	 22 13 "6112559918033517163" 					// ContentID
	 */
	void parseProtectionSystemSpecificHeaderBox( const uint8_t *next )
	{
		ReadHeader();
		char system_id[37]; // 32 hex chars + 4 hyphens + 1 null terminator
		snprintf( system_id, sizeof(system_id), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
										   ptr[0x0], ptr[0x1], ptr[0x2], ptr[0x3], ptr[0x4], ptr[0x5], ptr[0x6], ptr[0x7],
										   ptr[0x8], ptr[0x9], ptr[0xa], ptr[0xb], ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf] );
		ptr += 16;
		size_t pssh_size = next - ptr;
		AampPsshData psshData;
		psshData.systemID = std::string(system_id);
		psshData.pssh = std::vector<uint8_t>(ptr, ptr + pssh_size);
		protectionData.push_back(std::move(psshData));
	}
	
	void process_auxiliary_information( void )
	{
		//Backup the ptr value
		const uint8_t* bptr = ptr;
		size_t sample_count = cenc_aux_info_sizes.size();
		if (sample_count && got_auxiliary_information_offset)
		{
			ptr = moof_ptr + auxiliary_information_offset;
			uint64_t maxSampleCount = sampleOffset + sample_count;
			assert (samples.size() == maxSampleCount);
			for (auto i = sampleOffset; i < maxSampleCount; i++)
			{
				samples[i].mDrmMetadata.mIsEncrypted = true;
				samples[i].mDrmMetadata.mKeyId = default_kid;
				// TODO: Original media type is skipped for now
				if (scheme_type == MultiChar_Constant("cbcs"))
				{
					samples[i].mDrmMetadata.mCipher = "cbcs";
					samples[i].mDrmMetadata.mCryptByteBlock = crypt_byte_block;
					samples[i].mDrmMetadata.mSkipByteBlock = skip_byte_block;
				}
				else
				{
					samples[i].mDrmMetadata.mCipher = "cenc";
				}
				// Skip IV data if present (comes before subsample data in auxiliary info)
				if( iv_size )
				{
					// Read IV if not already present from senc box
					if( samples[i].mDrmMetadata.mIV.empty() )
					{
						samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + iv_size);
					}
					ptr += iv_size;
				}
				else if (scheme_type == MultiChar_Constant("cbcs") && !constant_iv.empty())
				{
					samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(constant_iv.begin(), constant_iv.end());
				}

				if (cenc_aux_info_sizes[i-sampleOffset] > iv_size)
				{
					// Read subsample data
					uint16_t n_subsamples = ReadU16();
					size_t subsamples_size = n_subsamples * 6;
					samples[i].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subsamples_size);
					ptr += subsamples_size;
				}
			}
		}
		ptr = bptr;
	}
	
	void parseSampleAuxiliaryInformationSizes( void )
	{
		ReadHeader();
		// 00 00 00 01
		// 63 65 6e 63 'cenc'
		// 00 00 00 00
		// 00 // default_info_size
		// 00 00 00 4c // sampleCount
		// 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 ...
		if( flags&1 )
		{
			parseAuxInfo();
		}
		uint8_t default_info_size = *ptr++;
		uint32_t sampleCount  = ReadU32();
		if( default_info_size )
		{
			for( int i=0; i<sampleCount; i++ )
			{
				cenc_aux_info_sizes.push_back(default_info_size);
			}
		}
		else
		{
			for( int i=0; i<sampleCount; i++ )
			{
				cenc_aux_info_sizes.push_back(ptr[i]);
			}
			ptr += sampleCount;
		}
	}
	
	void parseAuxInfo( void )
	{
		uint32_t aux_info_type = ReadU32(); // cenc or cbcs
		// TODO: Return proper error here instead of assert
		assert( aux_info_type == MultiChar_Constant("cenc") || aux_info_type == MultiChar_Constant("cbcs") );
		
		uint32_t aux_info_type_parameter = ReadU32();
		(void)aux_info_type_parameter;
	}
	
	// ISO/IEC 23001-7
	void parseSampleAuxiliaryInformationOffsets( void )
	{ // offsets to auxiliary information for samples or groups of samples
		// 00 00 00 01
		// 63 65 6e 63 'cenc'
		// 00 00 00 00
		// 00 00 00 01
		// 00 00 05 2c
		ReadHeader();
		if( flags&1 )
		{
			parseAuxInfo();
		}
		uint32_t entry_count = ReadU32();
		assert( entry_count == 1 );
		if( version == 0 )
		{
			auxiliary_information_offset = ReadU32();
		}
		else
		{
			auxiliary_information_offset = ReadU64();
		}
		got_auxiliary_information_offset = true;
	}
	
	void parseSampleEncryptionBox( void )
	{
		ReadHeader();
		uint32_t sampleCount = ReadU32();
		uint64_t maxSampleCount = sampleOffset + sampleCount;
		assert( samples.size() == maxSampleCount );
		// Start from sampleOffset to map samples from mdat
		for( auto iSample=sampleOffset; iSample<maxSampleCount; iSample++ )
		{
			samples[iSample].mDrmMetadata.mIsEncrypted = true;
			samples[iSample].mDrmMetadata.mKeyId = default_kid;
			// TODO: Original media type is skipped for now
			if (scheme_type == MultiChar_Constant("cbcs"))
			{
				samples[iSample].mDrmMetadata.mCipher = "cbcs";
				samples[iSample].mDrmMetadata.mCryptByteBlock = crypt_byte_block;
				samples[iSample].mDrmMetadata.mSkipByteBlock = skip_byte_block;
			}
			else
			{
				samples[iSample].mDrmMetadata.mCipher = "cenc";
			}
			if( iv_size )
			{
				samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + iv_size);
				ptr += iv_size;
			}
			else if (scheme_type == MultiChar_Constant("cbcs") && !constant_iv.empty())
			{
				samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(constant_iv.begin(), constant_iv.end());
			}

			if( flags&2 )
			{ // sub sample encryption
				uint16_t n_subsamples = ReadU16();
				size_t subsamples_size = n_subsamples * 6;
				samples[iSample].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subsamples_size);
				ptr += subsamples_size;
			}
		}
		sencPresent = true;
	}
	
	void parseMovieFragmentHeaderBox( void )
	{
		ReadHeader();
		uint32_t sequence_number = ReadU32();
		(void)sequence_number;
	}
	
	void parseTrackFragmentHeaderBox( void )
	{
		ReadHeader();
		track_id = ReadU32();
		if (flags & 0x00001)
		{
			base_data_offset = ReadU64();
		}
		if (flags & 0x00002)
		{
			default_sample_description_index = ReadU32();
		}
		if (flags & 0x00008)
		{
			default_sample_duration = ReadU32();
		}
		if (flags & 0x00010)
		{
			default_sample_size = ReadU32();
		}
		if (flags & 0x00020)
		{
			default_sample_flags = ReadU32();
		}
	}
	
	void parseTrackFragmentBaseMediaDecodeTimeBox( void  )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		baseMediaDecodeTime  = ReadBytes(sz);
	}
	
	void parseTrackFragmentRunBox( void )
	{
		ReadHeader();
		uint32_t sample_count = ReadU32();
		const unsigned char *data_ptr = moof_ptr;
		//0xE01
		if( flags & 0x0001 )
		{ // offset from start of Moof box field
			int32_t data_offset = ReadI32();
			data_ptr += data_offset;
		}
		else
		{ // mandatory field? should never reach here
			assert(0);
		}
		uint32_t sample_flags = 0;
		if(flags & 0x0004)
		{
			sample_flags = ReadU32();
		}
		uint64_t dts = baseMediaDecodeTime;
		for( unsigned int i=0; i<sample_count; i++ )
		{
			struct AampMediaSample sample;
			uint32_t sample_len = default_sample_size;
			uint32_t sample_duration = default_sample_duration;
			sample.mPts = 0.0;
			sample.mDts = 0.0;
			sample.mDuration = 0.0;
			if (flags & 0x0100)
			{
				sample_duration = ReadU32();
			}
			if (flags & 0x0200)
			{
				sample_len = ReadU32();
			}
			sample.mData.AppendBytes( data_ptr, sample_len );
			data_ptr += sample_len;
			if (flags & 0x0400)
			{ // rarely present?
				sample_flags = ReadU32();
			}
			int32_t sample_composition_time_offset = 0;
			if (flags & 0x0800)
			{ // for samples where pts and dts differ (overriding 'trex')
				sample_composition_time_offset = ReadI32();
			}
			sample.mDts = dts/(double)timescale;
			sample.mPts = (dts+sample_composition_time_offset)/(double)timescale;
			sample.mDuration = sample_duration / (double)timescale;
			dts += sample_duration;
			samples.push_back( std::move(sample) );
		}
	}
	
	void parseMovieHeaderBox( void )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		creation_time = ReadBytes(sz);
		modification_time = ReadBytes(sz);
		timescale = ReadU32();
		duration = ReadU32();
		rate = ReadU32();
		volume = ReadU32(); // fixed point
		ptr += 8;
		for( int  i=0; i<9; i++ )
		{
			matrix[i] = ReadI32();
		}
	}
	
	void parseMovieExtendsHeader( void )
	{
		ReadHeader();
		fragment_duration = ReadU32();
	}
	
	void parseTrackExtendsBox( void )
	{
		ReadHeader();
		track_id = ReadU32();
		default_sample_description_index = ReadU32();
		default_sample_duration = ReadU32();
		default_sample_size = ReadU32();
		default_sample_flags = ReadU32();
	}
	
	void parseTrackHeaderBox( void )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		creation_time = ReadBytes(sz);
		modification_time = ReadBytes(sz);
		track_id = ReadU32();
		ptr += 20+sz; // duration, layer, alternate_group, volume
		for( int i=0; i<9; i++ )
		{
			matrix[i] = ReadI32();
		}
		width_fixed = ReadU32();
		height_fixed = ReadU32();
	}
	
	void parseMediaHeaderBox( void )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		creation_time = ReadBytes(sz);
		modification_time = ReadBytes(sz);
		timescale = ReadU32();
		duration = ReadU32();
		language = ReadU16();
	}
	
	void parseSampleDescriptionBox( const uint8_t *next )
	{ // stsd
		ReadHeader();
		uint32_t count = ReadU32();
		// TODO: Return proper error here instead of assert
		assert( count == 1 );
		DemuxHelper(next);
	}
	
	void parseVideoInformation( void )
	{
		SkipBytes(4); // always zero?
		data_reference_index = ReadU32();
		SkipBytes(16); // always zero?
		mCodecInfo.mInfo.video.mWidth = ReadU16();
		mCodecInfo.mInfo.video.mHeight = ReadU16();
		// Debug: Log the parsed width/height values
		mCodecInfo.mInfo.video.mHorizontalResolution = ReadU32();
		mCodecInfo.mInfo.video.mVerticalResolution = ReadU32();
		SkipBytes(4);
		mCodecInfo.mInfo.video.mFrameCount = ReadU16();
		SkipBytes(32); // compressor_name
		mCodecInfo.mInfo.video.mDepth = ReadU16();
		int pad = ReadU16();
		// TODO: Return proper error here instead of assert
		assert( pad == 0xffff );
	}
	
	void parseAudioInformation( void )
	{
		SkipBytes(4); // zero
		data_reference_index = ReadU32();
		SkipBytes(8); // zero
		mCodecInfo.mInfo.audio.mChannelCount = ReadU16();
		mCodecInfo.mInfo.audio.mSampleSize = ReadU16();
		SkipBytes(4); // zero
		mCodecInfo.mInfo.audio.mSampleRate = ReadU16();
		SkipBytes(2); // zero
	}
	
	void parseStreamFormatBox( uint32_t type, const uint8_t *next )
	{
		mCodecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC( type );
		mCodecInfo.mType = GetMediaTypeForStreamOutputFormat( mCodecInfo.mCodecFormat );
		stream_format = type;
		switch( stream_format )
		{
			case MultiChar_Constant("hev1"):
			case MultiChar_Constant("avc1"):
			case MultiChar_Constant("hvc1"):
			case MultiChar_Constant("encv"):
				parseVideoInformation();
				break;
				
			case MultiChar_Constant("mp4a"):
			case MultiChar_Constant("ec-3"):
			case MultiChar_Constant("enca"):
				parseAudioInformation();
				break;
				
			default:
				// TODO: Return proper error here instead of assert
				assert(0);
				break;
		}
		DemuxHelper( next );
	}
	
	int readLen( void )
	{
		int rc = 0;
		for(;;)
		{
			unsigned char octet = *ptr++;
			rc <<= 7;
			rc |= octet&0x7f;
			if( (octet&0x80)==0 ) return rc;
		}
	}
	
	void parseCodecConfigHelper( const uint8_t *next )
	{
		while( ptr < next )
		{
			uint32_t tag = *ptr++;
			uint32_t len = readLen();
			const uint8_t *end = ptr + len;
			switch( tag )
			{
				case 0x03:
					SkipBytes(3);
					parseCodecConfigHelper( end );
					break;
					
				case 0x04:
					mCodecInfo.mInfo.audio.mObjectTypeId = *ptr++; // 5 = AAC LC
					mCodecInfo.mInfo.audio.mStreamType = *ptr++; // >>2
					mCodecInfo.mInfo.audio.mUpStream = *ptr++;
					mCodecInfo.mInfo.audio.mBufferSize = ReadU16();
					mCodecInfo.mInfo.audio.mMaxBitrate = ReadU32();
					mCodecInfo.mInfo.audio.mAvgBitrate = ReadU32();
					parseCodecConfigHelper( end );
					break;
					
				case 0x05:
					mCodecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + len);
					ptr += len;
					break;
					
				case 0x06:
					SkipBytes( len );
					break;
					
				default:
					assert(0);
					break;
			}
			assert( ptr == end );
			ptr = end;
		}
	}
	
	void parseCodecConfigurationBox( uint32_t type, const uint8_t *next )
	{
		// mCodecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC( type );
		if( type == MultiChar_Constant("esds") )
		{
			SkipBytes(4);
			parseCodecConfigHelper( next );
		}
		else if ( type != MultiChar_Constant("dec3") )
		{
			//TODO: No need to read this for dec3 box. Filter this against other types if any.
			size_t codec_data_len = next - ptr;
			mCodecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + codec_data_len);
		}
	}
	
	void DemuxHelper( const uint8_t *fin )
	{
		while( ptr < fin )
		{
			uint32_t size = ReadU32();
			const uint8_t *next = ptr+size-4;
			uint32_t type = ReadU32();
			switch( type )
			{
				case MultiChar_Constant("hev1"):
				case MultiChar_Constant("hvc1"):
				case MultiChar_Constant("avc1"):
				case MultiChar_Constant("mp4a"):
				case MultiChar_Constant("ec-3"):
				case MultiChar_Constant("enca"):
				case MultiChar_Constant("encv"):
					parseStreamFormatBox( type, next );
					break;
					
				case MultiChar_Constant("hvcC"):
				case MultiChar_Constant("dec3"):
				case MultiChar_Constant("avcC"):
				case MultiChar_Constant("esds"): // Elementary Stream Descriptor
					parseCodecConfigurationBox( type, next );
					break;
					
				case MultiChar_Constant("pssh"):
					parseProtectionSystemSpecificHeaderBox(next);
					break;
					
				case MultiChar_Constant("saio"): // points to first IV in senc box
					parseSampleAuxiliaryInformationOffsets();
					assert( ptr == next );
					break;
					
				case MultiChar_Constant("saiz"): // defines size of senc entries
					parseSampleAuxiliaryInformationSizes();
					assert( ptr == next );
					break;
					
				case MultiChar_Constant("senc"): // modern, optional
					parseSampleEncryptionBox();
					break;
					
				case MultiChar_Constant("mfhd"):
					parseMovieFragmentHeaderBox();
					break;
					
				case MultiChar_Constant("tfhd"):
					parseTrackFragmentHeaderBox();
					break;
					
				case MultiChar_Constant("trun"):
					parseTrackFragmentRunBox();
					break;
					
				case MultiChar_Constant("tfdt"):
					parseTrackFragmentBaseMediaDecodeTimeBox();
					break;
					
				case MultiChar_Constant("mvhd"):
					parseMovieHeaderBox();
					break;
					
				case MultiChar_Constant("mehd"):
					parseMovieExtendsHeader();
					break;
					
				case MultiChar_Constant("trex"):
					parseTrackExtendsBox();
					break;
					
				case MultiChar_Constant("tkhd"):
					parseTrackHeaderBox();
					break;
					
				case MultiChar_Constant("mdhd"):
					parseMediaHeaderBox();
					break;
					
				case MultiChar_Constant("stsd"): // Sample Description
					parseSampleDescriptionBox(next);
					break;
					
				case MultiChar_Constant("ftyp"): // FileType (major_brand, minor_version, compatible_brands)
				case MultiChar_Constant("hdlr"): // Handler Reference (handler, name)
				case MultiChar_Constant("vmhd"): // Video Media Header (graphics_mode, op_color)
				case MultiChar_Constant("smhd"): // Sound Media Header (balance)
				case MultiChar_Constant("dref"): // Data Reference (url) (under dinf box)
				case MultiChar_Constant("stts"): // Decoding Time To Sample (under stb boxl)
				case MultiChar_Constant("stsc"): // Sample To Chunk (under stbl box)
				case MultiChar_Constant("stsz"): // Sample Size Boxes (under stbl box)
				case MultiChar_Constant("stco"): // Chunk Offsets (under stbl box)
				case MultiChar_Constant("stss"): // Sync Sample (under stbl box)
				case MultiChar_Constant("prft"): // Producer Reference Time
				case MultiChar_Constant("edts"): // Edit (under trak box)
				case MultiChar_Constant("fiel"): // Field (progressive or interlaced)
				case MultiChar_Constant("colr"): // Color Pattern Atom
				case MultiChar_Constant("pasp"): // Pixel Aspect Ratio (hSpacing, vSpacing)
				case MultiChar_Constant("btrt"): // Buffer Time to Render Time (bufferSizeDB, maxBitrate, avgBitrate)
				case MultiChar_Constant("styp"): // Segment Type (under file box)
				case MultiChar_Constant("sidx"): // Segment Index
				case MultiChar_Constant("udta"): // User Data (can appear under moov, trak, moof, traf)
				case MultiChar_Constant("mdat"): // Movie Data (under file box)
					// TODO - parse if needed
					break;
					
				case MultiChar_Constant("schm"):
					parseSchemeManagementBox();
					break;
					
				case MultiChar_Constant("frma"):
					parseOriginalFormat();
					break;
					
				case MultiChar_Constant("tenc"):
					parseTrackEncryptionBox();
					assert( ptr == next );
					break;
					
				case MultiChar_Constant("moof"):  // Movie Fragment
					moof_ptr = ptr-8;
					// For LLD streams, we may have multiple moof boxes
					// so we need to track sampleOffset to map samples to mdat
					sampleOffset = samples.size();
					// Reset encryption state for each moof
					got_auxiliary_information_offset = false;
					cenc_aux_info_sizes.clear();
					sencPresent = false;

					DemuxHelper(next );

					if (!sencPresent && got_auxiliary_information_offset)
					{
						// If no 'senc' box, we need to get IVs and subsample data from auxiliary info
						process_auxiliary_information();
					}
					break;
					
				case MultiChar_Constant("schi"): // Scheme Information
				case MultiChar_Constant("traf"): // Track Fragment
				case MultiChar_Constant("moov"): // Movie
				case MultiChar_Constant("trak"): // Track
				case MultiChar_Constant("minf"): // Media Information
				case MultiChar_Constant("dinf"): // Data Information
				case MultiChar_Constant("mvex"): // Movie Extends
				case MultiChar_Constant("mdia"): // Media
				case MultiChar_Constant("stbl"): // Sample Table
				case MultiChar_Constant("sinf"): // Protection Scheme Information
					DemuxHelper(next );
					break;
					
				default:
					break;
			}
			ptr = next;
		}
	}
	
public:
	void Parse( const void *ptr, size_t len )
	{
		// scrub sample data from previous segment, but leave other metadata intact
		samples.clear();
		cenc_aux_info_sizes.clear();
		got_auxiliary_information_offset = false;
		moof_ptr = NULL;
		if( ptr )
		{
			this->ptr = (const uint8_t *)ptr;
			DemuxHelper( &this->ptr[len] );
		}
	}
	
	Mp4Demux( bool verbose=false ):
		stream_format(),
		data_reference_index(),
		iv_size(),
		crypt_byte_block(), skip_byte_block(),
		constant_iv_size(), constant_iv(), timescale(),
		samples(), default_kid(), got_auxiliary_information_offset(),
		auxiliary_information_offset(), scheme_type(), scheme_version(),
		original_media_type(), cenc_aux_info_sizes(), protectionData(),
		moof_ptr(), ptr(),
		version(), flags(), baseMediaDecodeTime(),
		fragment_duration(), track_id(), base_data_offset(),
		default_sample_description_index(), default_sample_duration(), default_sample_size(),
		default_sample_flags(), creation_time(), modification_time(),
		duration(), rate(), volume(),
		matrix{}, layer(), alternate_group(),
		width_fixed(), height_fixed(), language(),
		verbose(verbose), sampleOffset(), sencPresent(false),
		mCodecInfo(FORMAT_UNKNOWN)
	{
	}
	
	uint32_t getTimeScale( void )
	{
		return timescale;
	}
	
	~Mp4Demux()
	{
	}
	
	Mp4Demux(const Mp4Demux & other) = delete;	
	Mp4Demux& operator=(const Mp4Demux & other) = delete;

	const AampCodecInfo& getCodecInfo( void ) const
	{
		return mCodecInfo;
	}

	const std::vector<AampPsshData>& getProtectionEvents( void )
	{
		return protectionData;
	}

	std::vector<AampMediaSample>& getSamples( void )
	{
		return samples;
	}
};

#endif /* __MP4_DEMUX_HPP__ */
