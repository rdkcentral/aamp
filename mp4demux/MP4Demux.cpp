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
 * @file MP4Demux.cpp
 * @brief MP4 Demultiplexer implementation for AAMP
 */

#include "MP4Demux.h"
#include <assert.h>
#include <inttypes.h>
#include <cstdio>
#include <cstring>
#include <memory>

/**
 * @brief Constructor for Mp4Demux
 * @param verbose Enable verbose logging for debugging purposes
 * 
 * Initializes all member variables to their default values and sets up
 * the demuxer for MP4 parsing operations.
 */
Mp4Demux::Mp4Demux(bool verbose) :
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
	sampleOffset(), sencPresent(false), verbose(verbose),
	mCodecInfo(FORMAT_UNKNOWN)
{
}

/**
 * @brief Destructor for Mp4Demux
 * 
 * Cleans up resources and performs any necessary cleanup operations.
 */
Mp4Demux::~Mp4Demux()
{
}

/**
 * @brief Convert FourCC code to stream output format
 * @param fourCC Four character code from MP4 container
 * @return StreamOutputFormat corresponding to the codec type
 * 
 * Maps MP4 codec FourCC codes to AAMP stream output formats:
 * - avc1: H.264 video
 * - hvc1/hev1: HEVC video  
 * - mp4a: AAC audio
 * - ac-3/dac3: AC3 audio
 * - ec-3/dec3: Enhanced AC3 audio
 * - ac-4: AC4 audio
 */
StreamOutputFormat Mp4Demux::GetStreamOutputFormatFromFourCC(const uint32_t fourCC)
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

/**
 * @brief Convert stream output format to media type
 * @param format Stream output format identifier
 * @return AampMediaType for the given format
 * 
 * Categorizes stream formats into their respective media types:
 * - Video formats (H.264, HEVC, MPEG2) -> VIDEO
 * - Audio formats (AAC, AC3, EC3, etc.) -> AUDIO  
 * - Subtitle formats (WebVTT, TTML) -> SUBTITLE
 */
AampMediaType Mp4Demux::GetMediaTypeForStreamOutputFormat(const StreamOutputFormat format)
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

/**
 * @brief Read n bytes from current position in big-endian format
 * @param n Number of bytes to read (1-8)
 * @return Value read as uint64_t in host byte order
 * 
 * Reads bytes from the current parser position and converts from
 * big-endian (network byte order) to host byte order. Advances
 * the parser position by n bytes.
 */
uint64_t Mp4Demux::ReadBytes(int n)
{
	uint64_t rc = 0;
	for (int i = 0; i < n; i++)
	{
		rc <<= 8;
		rc |= *ptr++;
	}
	return rc;
}

/**
 * @brief Read 16-bit unsigned integer in big-endian format
 * @return 16-bit unsigned integer value
 */
uint16_t Mp4Demux::ReadU16()
{
	return (uint16_t)ReadBytes(2);
}

/**
 * @brief Read 32-bit unsigned integer in big-endian format
 * @return 32-bit unsigned integer value
 */
uint32_t Mp4Demux::ReadU32()
{
	return (uint32_t)ReadBytes(4);
}

/**
 * @brief Read 32-bit signed integer in big-endian format
 * @return 32-bit signed integer value
 */
int32_t Mp4Demux::ReadI32()
{
	return (int32_t)ReadBytes(4);
}

/**
 * @brief Read 64-bit unsigned integer in big-endian format
 * @return 64-bit unsigned integer value
 */
uint64_t Mp4Demux::ReadU64()
{
	return ReadBytes(8);
}

/**
 * @brief Read MP4 box header (version and flags)
 * 
 * Reads the standard MP4 box header consisting of:
 * - 1 byte version
 * - 3 bytes flags
 * Updates the parser state with these values.
 */
void Mp4Demux::ReadHeader()
{
	version = *ptr++;
	flags = (uint32_t)ReadBytes(3);
}

/**
 * @brief Skip specified number of bytes
 * @param len Number of bytes to skip
 * 
 * Advances the parser position by len bytes without reading the data.
 * Used to skip over unused or reserved fields in MP4 boxes.
 */
void Mp4Demux::SkipBytes(size_t len)
{
	ptr += len;
}

/**
 * @brief Parse original format box for encrypted media
 * 
 * Extracts the original media format from encrypted content (encv/enca).
 * The original format is stored before encryption was applied and is
 * used to determine the actual codec type for encrypted streams.
 */
void Mp4Demux::parseOriginalFormat()
{
	original_media_type = ReadU32();
	// For encv and enca formats
	if (mCodecInfo.mCodecFormat == FORMAT_UNKNOWN)
	{
		mCodecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC(original_media_type);
	}
}

/**
 * @brief Parse scheme management box for DRM information
 * 
 * Extracts DRM scheme information including:
 * - scheme_type: 'cenc' (AES-CTR) or 'cbcs' (AES-CBC with pattern)
 * - scheme_version: Version of the encryption scheme
 */
void Mp4Demux::parseSchemeManagementBox()
{
	ReadHeader();
	scheme_type = ReadU32(); // 'cenc' or 'cbcs'
	scheme_version = ReadU32();
}

/**
 * @brief Parse track encryption box
 * 
 * Extracts encryption parameters for the track:
 * - Pattern encryption settings for CBCS
 * - Encryption flag and IV size
 * - Default key identifier (KID)
 * - Constant IV for CBCS scheme
 */
void Mp4Demux::parseTrackEncryptionBox()
{
	ReadHeader();

	ptr++; // skip
	uint8_t pattern = *ptr++;
	if (scheme_type == MultiChar_Constant("cbcs"))
	{
		crypt_byte_block = (pattern >> 4) & 0xf;
		skip_byte_block = pattern & 0xf;
	}
	mCodecInfo.mIsEncrypted = *ptr++;
	iv_size = *ptr++;

	default_kid = std::string((char *)ptr, 16);
	ptr += 16;
	if (scheme_type == MultiChar_Constant("cbcs"))
	{
		constant_iv_size = *ptr++;
		// TODO : Send proper error event, instead of assert
		assert(constant_iv_size == 8 || constant_iv_size == 16);
		constant_iv = std::vector<uint8_t>(ptr, ptr + constant_iv_size);
		ptr += constant_iv_size;
	}
}

/**
 * @brief Parse protection system specific header box (PSSH)
 * @param next Pointer to next box boundary
 * 
 * Extracts DRM protection system data including:
 * - System ID (formatted as UUID string)
 * - PSSH data blob for DRM license acquisition
 * The parsed data is stored for later DRM initialization.
 */
void Mp4Demux::parseProtectionSystemSpecificHeaderBox(const uint8_t *next)
{
	ReadHeader();
	char system_id[37]; // 32 hex chars + 4 hyphens + 1 null terminator
	snprintf(system_id, sizeof(system_id), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			ptr[0x0], ptr[0x1], ptr[0x2], ptr[0x3], ptr[0x4], ptr[0x5], ptr[0x6], ptr[0x7],
			ptr[0x8], ptr[0x9], ptr[0xa], ptr[0xb], ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf]);
	ptr += 16;
	size_t pssh_size = next - ptr;
	// TODO: Limit the number of PSSH boxes stored to avoid excessive memory usage
	protectionData.emplace_back();
	AampPsshData &psshData = protectionData.back();
	psshData.systemID = std::string(system_id);
	psshData.pssh = std::vector<uint8_t>(ptr, ptr + pssh_size);
}

/**
 * @brief Process auxiliary information for encrypted samples
 * 
 * Reads encryption metadata from auxiliary information when no SENC box
 * is present. Processes initialization vectors and subsample encryption
 * data for each sample, applying the appropriate cipher mode (CENC/CBCS).
 */
void Mp4Demux::process_auxiliary_information()
{
	//Backup the ptr value
	const uint8_t* bptr = ptr;
	size_t sample_count = cenc_aux_info_sizes.size();
	if (sample_count && got_auxiliary_information_offset)
	{
		ptr = moof_ptr + auxiliary_information_offset;
		uint64_t maxSampleCount = sampleOffset + sample_count;
		assert(samples.size() == maxSampleCount);
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
			if (iv_size)
			{
				// Read IV if not already present from senc box
				if (samples[i].mDrmMetadata.mIV.empty())
				{
					samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + iv_size);
				}
				ptr += iv_size;
			}
			else if (scheme_type == MultiChar_Constant("cbcs") && !constant_iv.empty())
			{
				samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(constant_iv.begin(), constant_iv.end());
			}

			if (cenc_aux_info_sizes[i - sampleOffset] > iv_size)
			{
				// Sub-sample encryption info present
				uint16_t n_subsamples = ReadU16();
				size_t subsamples_size = n_subsamples * 6;
				samples[i].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subsamples_size);
				ptr += subsamples_size;
			}
		}
	}
	//Restore the ptr value
	ptr = bptr;
}

/**
 * @brief Parse sample auxiliary information sizes box (SAIZ)
 * 
 * Reads the sizes of auxiliary information entries for encrypted samples.
 * Each entry corresponds to the size of encryption metadata (IV + subsample info)
 * for one sample. Supports both version 0 (1-byte sizes) and version 1 (4-byte sizes).
 */
void Mp4Demux::parseSampleAuxiliaryInformationSizes()
{
	ReadHeader();
	// 00 00 00 01
	// 63 65 6e 63 'cenc'
	// 00 00 00 00
	// 00 // default_info_size
	// 00 00 00 4c // sampleCount
	// 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 ...
	if (flags & 1)
	{
		parseAuxInfo();
	}
	uint8_t default_info_size = *ptr++;
	uint32_t sampleCount = ReadU32();
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

/**
 * @brief Parse auxiliary information type parameters
 * 
 * Reads optional auxiliary information type and parameters when present.
 * Used in conjunction with SAIZ and SAIO boxes to specify encryption
 * auxiliary information format.
 */
void Mp4Demux::parseAuxInfo()
{
	uint32_t aux_info_type = ReadU32(); // cenc or cbcs
	// TODO : Send proper error event, instead of assert
	assert( aux_info_type == MultiChar_Constant("cenc") || aux_info_type == MultiChar_Constant("cbcs") );

	uint32_t aux_info_type_parameter = ReadU32();
	(void)aux_info_type_parameter;
}

/**
 * @brief Parse sample auxiliary information offsets box (SAIO)
 * 
 * Reads the offset to auxiliary information data within the movie fragment.
 * This offset points to where encryption metadata (IVs, subsample info) is
 * stored for encrypted samples. Supports both 32-bit and 64-bit offsets.
 */
void Mp4Demux::parseSampleAuxiliaryInformationOffsets()
{
	// offsets to auxiliary information for samples or groups of samples
	// 00 00 00 01
	// 63 65 6e 63 'cenc'
	// 00 00 00 00
	// 00 00 00 01
	// 00 00 05 2c
	ReadHeader();
	if (flags & 1)
	{
		parseAuxInfo();
	}
	uint32_t entry_count = ReadU32();
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

/**
 * @brief Parse sample encryption box (SENC)
 * 
 * Processes encryption metadata directly embedded in the SENC box.
 * For each encrypted sample, extracts:
 * - Initialization vector (IV)
 * - Subsample encryption information (clear/encrypted byte pairs)
 * - Cipher mode and pattern encryption settings
 */
void Mp4Demux::parseSampleEncryption()
{
	ReadHeader();
	uint32_t sampleCount = ReadU32();
	uint64_t maxSampleCount = sampleOffset + sampleCount;
	assert(samples.size() == maxSampleCount);
	for (auto iSample = sampleOffset; iSample < maxSampleCount; iSample++)
	{
		samples[iSample].mDrmMetadata.mIsEncrypted = true;
		samples[iSample].mDrmMetadata.mKeyId = default_kid;
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
		if (iv_size)
		{
			samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + iv_size);
			ptr += iv_size;
		}
		else if (scheme_type == MultiChar_Constant("cbcs") && !constant_iv.empty())
		{
			samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(constant_iv.begin(), constant_iv.end());
		}

		if (flags & 2)
		{ // sub sample encryption
			uint16_t n_subsamples = ReadU16();
			size_t subsamples_size = n_subsamples * 6;
			samples[iSample].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subsamples_size);
			ptr += subsamples_size;
		}
	}
	sencPresent = true;
}

/**
 * @brief Parse track run box (TRUN)
 * 
 * Extracts sample information from the track run including:
 * - Sample data offsets and sizes
 * - Sample durations and composition time offsets
 * - Sample flags and media timing information
 * Creates AampMediaSample objects with proper PTS/DTS timing.
 */
void Mp4Demux::parseTrackRun()
{
	ReadHeader();
	uint32_t sample_count = ReadU32();
	const unsigned char *data_ptr = moof_ptr;
	//0xE01
	if (flags & 0x0001)
	{
		// offset from start of Moof box field
		int32_t data_offset = ReadI32();
		data_ptr += data_offset;
	}
	else
	{
		// mandatory field? should never reach here
		//TODO: Return proper error here instead of assert
		assert(0);
	}
	uint32_t sample_flags = 0;
	if (flags & 0x0004)
	{
		sample_flags = ReadU32();
	}
	uint64_t dts = baseMediaDecodeTime;
	for (unsigned int i = 0; i < sample_count; i++)
	{
		samples.emplace_back();
		// Get reference to newly added sample
		AampMediaSample& newSample = samples.back();
		uint32_t sample_len = default_sample_size;
		uint32_t sample_duration = default_sample_duration;
		if (flags & 0x0100)
		{
			sample_duration = ReadU32();
		}
		if (flags & 0x0200)
		{
			sample_len = ReadU32();
		}
		if (flags & 0x0400)
		{ // rarely present?
			sample_flags = ReadU32();
		}
		int32_t sample_composition_time_offset = 0;
		if (flags & 0x0800)
		{ // for samples where pts and dts differ (overriding 'trex')
			sample_composition_time_offset = ReadI32();
		}
		newSample.mData.AppendBytes(data_ptr, sample_len);
		data_ptr += sample_len;
		newSample.mDts = dts / (double)timescale;
		newSample.mPts = (dts + sample_composition_time_offset) / (double)timescale;
		newSample.mDuration = sample_duration / (double)timescale;
		dts += sample_duration;
	}
}

void Mp4Demux::parseTrackFragmentHeader()
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

void Mp4Demux::parseTrackFragmentDecodeTime()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	baseMediaDecodeTime = ReadBytes(sz);
}

/**
 * @brief Parse video information from sample entry
 * 
 * Extracts video-specific properties from the sample description:
 * - Video dimensions (width, height)
 * - Display resolution information
 * - Frame count and color depth
 * - Data reference index for media data location
 */
void Mp4Demux::parseVideoInformation()
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
	assert(pad == 0xffff);
}

/**
 * @brief Parse audio information from sample entry
 * 
 * Extracts audio-specific properties from the sample description:
 * - Channel count and sample size
 * - Sample rate information
 * - Data reference index for media data location
 */
void Mp4Demux::parseAudioInformation()
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

void Mp4Demux::parseMovieHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	creation_time = ReadBytes(sz);
	modification_time = ReadBytes(sz);
	timescale = ReadU32();
	duration = ReadU32();
	rate = ReadU32();
	volume = ReadU32(); // fixed point
	ptr += 8;
	for (int i = 0; i < 9; i++)
	{
		matrix[i] = ReadI32();
	}
}

void Mp4Demux::parseTrackHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	creation_time = ReadBytes(sz);
	modification_time = ReadBytes(sz);
	track_id = ReadU32();
	ptr += 20 + sz; // duration, layer, alternate_group, volume
	for (int i = 0; i < 9; i++)
	{
		matrix[i] = ReadI32();
	}
	width_fixed = ReadU32();
	height_fixed = ReadU32();
}

void Mp4Demux::parseMediaHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	creation_time = ReadBytes(sz);
	modification_time = ReadBytes(sz);
	timescale = ReadU32();
	duration = ReadU32();
	language = ReadU16();
}

void Mp4Demux::parseHandlerReference()
{
	ReadHeader();
	SkipBytes(4); // pre_defined
	uint32_t handler_type = ReadU32();
	SkipBytes(12); // reserved
	// handler name is null-terminated string (remaining bytes)
}

/**
 * @brief Parse movie fragment header box
 */
void Mp4Demux::parseMovieFragmentHeaderBox()
{
	ReadHeader();
	uint32_t sequence_number = ReadU32();
	(void)sequence_number;
}

/**
 * @brief Parse movie extends header box
 */
void Mp4Demux::parseMovieExtendsHeader()
{
	ReadHeader();
	fragment_duration = ReadU32();
}

/**
 * @brief Parse track extends box
 */
void Mp4Demux::parseTrackExtendsBox()
{
	ReadHeader();
	track_id = ReadU32();
	default_sample_description_index = ReadU32();
	default_sample_duration = ReadU32();
	default_sample_size = ReadU32();
	default_sample_flags = ReadU32();
}

/**
 * @brief Parse sample description box
 * @param next Pointer to next box
 */
void Mp4Demux::parseSampleDescriptionBox(const uint8_t *next)
{ // stsd
	ReadHeader();
	uint32_t count = ReadU32();
	// TODO: Return proper error here instead of assert
	assert(count == 1);
	DemuxHelper(next);
}

/**
 * @brief Parse stream format box
 * @param type FourCC type
 * @param next Pointer to next box
 */
void Mp4Demux::parseStreamFormatBox(uint32_t type, const uint8_t *next)
{
	mCodecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC(type);
	stream_format = type;
	switch (stream_format)
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
	DemuxHelper(next);
}

/**
 * @brief Read length field with variable encoding
 * @return Length value decoded from variable-length encoding
 * 
 * Reads a variable-length encoded integer used in Elementary Stream
 * Descriptor (ESDS) boxes. Each byte contains 7 bits of data and
 * 1 continuation bit. Used for AAC codec configuration parsing.
 */
int Mp4Demux::readLen()
{
	int rc = 0;
	for (;;)
	{
		unsigned char octet = *ptr++;
		rc <<= 7;
		rc |= octet & 0x7f;
		if ((octet & 0x80) == 0) return rc;
	}
}

/**
 * @brief Parse codec configuration helper for ESDS
 * @param next Pointer to end of data
 * 
 * Recursively parses Elementary Stream Descriptor structure for AAC audio:
 * - Tag 0x03: ES descriptor
 * - Tag 0x04: Decoder config descriptor (object type, stream type, bitrates)
 * - Tag 0x05: Decoder specific info (actual codec configuration data)
 * - Tag 0x06: SL config descriptor
 */
void Mp4Demux::parseCodecConfigHelper(const uint8_t *next)
{
	while (ptr < next)
	{
		uint32_t tag = *ptr++;
		uint32_t len = readLen();
		const uint8_t *end = ptr + len;
		switch (tag)
		{
			case 0x03:
				SkipBytes(3);
				parseCodecConfigHelper(end);
				break;

			case 0x04:
				mCodecInfo.mInfo.audio.mObjectTypeId = *ptr++; // 5 = AAC LC
				mCodecInfo.mInfo.audio.mStreamType = *ptr++; // >>2
				mCodecInfo.mInfo.audio.mUpStream = *ptr++;
				mCodecInfo.mInfo.audio.mBufferSize = ReadU16();
				mCodecInfo.mInfo.audio.mMaxBitrate = ReadU32();
				mCodecInfo.mInfo.audio.mAvgBitrate = ReadU32();
				parseCodecConfigHelper(end);
				break;

			case 0x05:
				mCodecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + len);
				ptr += len;
				break;

			case 0x06:
				SkipBytes(len);
				break;

			default:
				assert(0);
				break;
		}
		assert(ptr == end);
		ptr = end;
	}
}

/**
 * @brief Parse codec configuration box
 * @param type FourCC type identifier
 * @param next Pointer to next box boundary
 * 
 * Handles codec-specific configuration data:
 * - ESDS: AAC Elementary Stream Descriptor (complex structure)
 * - avcC: H.264 configuration (SPS/PPS data)
 * - hvcC: HEVC configuration (VPS/SPS/PPS data)
 * - dec3: Enhanced AC3 configuration (skipped for now)
 */
void Mp4Demux::parseCodecConfigurationBox(uint32_t type, const uint8_t *next)
{
	if (type == MultiChar_Constant("esds"))
	{
		SkipBytes(4);
		parseCodecConfigHelper(next);
	}
	else if (type != MultiChar_Constant("dec3"))
	{
		//TODO: No need to read this for dec3 box. Filter this against other types if any.
		size_t codec_data_len = next - ptr;
		mCodecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + codec_data_len);
	}
}

/**
 * @brief Main demux helper function
 * @param fin Pointer to end of data to parse
 * 
 * Core MP4 parsing engine that recursively processes MP4 boxes.
 * Handles box size calculation, type identification, and dispatches
 * to appropriate parsing functions based on box type (FourCC).
 * Supports both standard and encrypted MP4 container formats.
 */
void Mp4Demux::DemuxHelper(const uint8_t *fin)
{
	while (ptr < fin)
	{
		uint32_t size = ReadU32();
		const uint8_t *next = ptr + size - 4;
		uint32_t type = ReadU32();
		switch (type)
		{
			case MultiChar_Constant("hev1"):
			case MultiChar_Constant("hvc1"):
			case MultiChar_Constant("avc1"):
			case MultiChar_Constant("mp4a"):
			case MultiChar_Constant("ec-3"):
			case MultiChar_Constant("enca"):
			case MultiChar_Constant("encv"):
				parseStreamFormatBox(type, next);
				break;

			case MultiChar_Constant("hvcC"):
			case MultiChar_Constant("dec3"):
			case MultiChar_Constant("avcC"):
			case MultiChar_Constant("esds"): // Elementary Stream Descriptor
				parseCodecConfigurationBox(type, next);
				break;

			case MultiChar_Constant("pssh"):
				parseProtectionSystemSpecificHeaderBox(next);
				break;

			case MultiChar_Constant("saio"): // points to first IV in senc box
				parseSampleAuxiliaryInformationOffsets();
				assert(ptr == next);
				break;

			case MultiChar_Constant("saiz"): // defines size of senc entries
				parseSampleAuxiliaryInformationSizes();
				assert(ptr == next);
				break;

			case MultiChar_Constant("senc"): // modern, optional
				parseSampleEncryption();
				break;

			case MultiChar_Constant("mfhd"):
				parseMovieFragmentHeaderBox();
				break;

			case MultiChar_Constant("tfhd"):
				parseTrackFragmentHeader();
				break;

			case MultiChar_Constant("trun"):
				parseTrackRun();
				break;

			case MultiChar_Constant("tfdt"):
				parseTrackFragmentDecodeTime();
				break;

			case MultiChar_Constant("mvhd"):
				parseMovieHeader();
				break;

			case MultiChar_Constant("mehd"):
				parseMovieExtendsHeader();
				break;

			case MultiChar_Constant("trex"):
				parseTrackExtendsBox();
				break;

			case MultiChar_Constant("tkhd"):
				parseTrackHeader();
				break;

			case MultiChar_Constant("mdhd"):
				parseMediaHeader();
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
				assert(ptr == next);
				break;

			case MultiChar_Constant("moof"):  // Movie Fragment
				moof_ptr = ptr - 8;
				// For LLD streams, we may have multiple moof boxes
				// so we need to track sampleOffset to map samples to mdat
				sampleOffset = samples.size();
				// Reset encryption state for each moof
				got_auxiliary_information_offset = false;
				cenc_aux_info_sizes.clear();
				sencPresent = false;

				DemuxHelper(next);

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
				DemuxHelper(next);
				break;

			default:
				break;
		}
		ptr = next;
	}
}

/**
 * @brief Parse MP4 data segment
 * @param ptr Pointer to MP4 data buffer
 * @param len Length of data buffer in bytes
 * 
 * Main entry point for MP4 parsing. Resets sample data from previous
 * segments while preserving metadata, then initiates recursive parsing
 * of the MP4 container structure. Handles both initialization segments
 * and media fragments.
 */
void Mp4Demux::Parse(const void *ptr, size_t len)
{
	// scrub sample data from previous segment, but leave other metadata intact
	samples.clear();
	cenc_aux_info_sizes.clear();
	got_auxiliary_information_offset = false;
	moof_ptr = NULL;
	if (ptr)
	{
		this->ptr = (const uint8_t *)ptr;
		DemuxHelper(&this->ptr[len]);
	}
}

/**
 * @brief Get media timescale value
 * @return Media timescale in units per second
 * 
 * Returns the timescale used for media timing calculations.
 * Used to convert media time units to seconds for PTS/DTS values.
 */
uint32_t Mp4Demux::getTimeScale() const
{
	return timescale;
}

/**
 * @brief Get codec information
 * @return Const reference to parsed codec information
 * 
 * Returns comprehensive codec information including format,
 * media type, and codec-specific parameters extracted from
 * the MP4 sample description.
 */
const AampCodecInfo& Mp4Demux::getCodecInfo() const
{
	return mCodecInfo;
}

/**
 * @brief Get DRM protection system data
 * @return Const reference to protection data vector
 * 
 * Returns all PSSH (Protection System Specific Header) data
 * extracted from the MP4 container for DRM license acquisition.
 */
const std::vector<AampPsshData>& Mp4Demux::getProtectionEvents() const
{
	return protectionData;
}

/**
 * @brief Get parsed media samples
 * @return Reference to vector of media samples
 * 
 * Returns all media samples extracted from the current MP4 fragment,
 * including sample data, timing information, and encryption metadata.
 */
std::vector<AampMediaSample>& Mp4Demux::getSamples()
{
	return samples;
}