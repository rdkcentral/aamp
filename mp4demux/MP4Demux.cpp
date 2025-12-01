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
#include "AampDefine.h"
#include <inttypes.h>
#include <cstdio>
#include <cstring>
#include <memory>

/**
 * @brief Constructor for Mp4Demux
 * Initializes all member variables to their default values and sets up
 * the demuxer for MP4 parsing operations.
 */
Mp4Demux::Mp4Demux() :
	streamFormat(),
	dataReferenceIndex(),
	ivSize(),
	cryptByteBlock(), skipByteBlock(),
	constantIvSize(), constantIv(), timeScale(),
	samples(), defaultKid(), gotAuxiliaryInformationOffset(),
	auxiliaryInformationOffset(), schemeType(), schemeVersion(),
	originalMediaType(), cencAuxInfoSizes(), protectionData(),
	moofPtr(), ptr(),
	version(), flags(), baseMediaDecodeTime(),
	fragmentDuration(), trackId(), baseDataOffset(),
	defaultSampleDescriptionIndex(), defaultSampleDuration(), defaultSampleSize(),
	defaultSampleFlags(), creationTime(), modificationTime(),
	duration(), rate(), volume(),
	matrix{}, layer(), alternateGroup(),
	widthFixed(), heightFixed(), language(),
	sampleOffset(), sencPresent(false),
	handledEncryptedSamples(false),
	codecInfo(FORMAT_INVALID), parseError(MP4_PARSE_OK)
{
}

/**
 * @brief Destructor for Mp4Demux
 * Cleans up resources and performs any necessary cleanup operations.
 */
Mp4Demux::~Mp4Demux()
{
}

/**
 * @brief Convert FourCC code to stream output format
 * Maps MP4 codec FourCC codes to AAMP stream output formats:
 * - avcC: H.264 video
 * - hvcC: HEVC video
 * - esds: AAC audio (raw)
 * - dec3: Enhanced AC3 audio
 * 
 * @param fourCC Four character code from MP4 container
 * @return StreamOutputFormat corresponding to the codec type
 */
StreamOutputFormat Mp4Demux::GetStreamOutputFormatFromFourCC(const uint32_t fourCC)
{
	switch (fourCC)
	{
		case MultiChar_Constant("avcC"):
			return FORMAT_VIDEO_ES_H264;
		case MultiChar_Constant("hvcC"):
			return FORMAT_VIDEO_ES_HEVC;
		case MultiChar_Constant("esds"):
			return FORMAT_AUDIO_ES_AAC_RAW;
		case MultiChar_Constant("dec3"):
			return FORMAT_AUDIO_ES_EC3;
		default:
			return FORMAT_UNKNOWN;
	}
}

/**
 * @brief Convert stream output format to media type
 * Categorizes stream formats into their respective media types:
 * - Video formats (H.264, HEVC, MPEG2) -> VIDEO
 * - Audio formats (AAC, AC3, EC3, etc.) -> AUDIO  
 * - Subtitle formats (WebVTT, TTML) -> SUBTITLE
 * 
 * @param format Stream output format identifier
 * @return AampMediaType for the given format
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
 * Reads bytes from the current parser position and converts from
 * big-endian (network byte order) to host byte order. Advances
 * the parser position by n bytes.
 * 
 * @param n Number of bytes to read (1-8)
 * @return Value read as uint64_t in host byte order
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
 * 
 * @return 16-bit unsigned integer value
 */
uint16_t Mp4Demux::ReadU16()
{
	return (uint16_t)ReadBytes(2);
}

/**
 * @brief Read 32-bit unsigned integer in big-endian format
 * 
 * @return 32-bit unsigned integer value
 */
uint32_t Mp4Demux::ReadU32()
{
	return (uint32_t)ReadBytes(4);
}

/**
 * @brief Read 32-bit signed integer in big-endian format
 * 
 * @return 32-bit signed integer value
 */
int32_t Mp4Demux::ReadI32()
{
	return (int32_t)ReadBytes(4);
}

/**
 * @brief Read 64-bit unsigned integer in big-endian format
 * 
 * @return 64-bit unsigned integer value
 */
uint64_t Mp4Demux::ReadU64()
{
	return ReadBytes(8);
}

/**
 * @brief Read MP4 box header (version and flags)
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
 * Advances the parser position by len bytes without reading the data.
 * Used to skip over unused or reserved fields in MP4 boxes.
 * 
 * @param len Number of bytes to skip
 */
void Mp4Demux::SkipBytes(size_t len)
{
	ptr += len;
}

/**
 * @brief Parse original format box for encrypted media
 * Extracts the original media format from encrypted content (encv/enca).
 * The original format is stored before encryption was applied and is
 * used to determine the actual codec type for encrypted streams.
 */
void Mp4Demux::ParseOriginalFormat()
{
	originalMediaType = ReadU32();
}

/**
 * @brief Parse scheme management box for DRM information
 * Extracts DRM scheme information including:
 * - schemeType: 'cenc' (AES-CTR) or 'cbcs' (AES-CBC with pattern)
 * - schemeVersion: Version of the encryption scheme
 */
void Mp4Demux::ParseSchemeManagementBox()
{
	ReadHeader();
	schemeType = ReadU32(); // 'cenc' or 'cbcs'
	schemeVersion = ReadU32();
}

/**
 * @brief Parse track encryption box
 * Extracts encryption parameters for the track:
 * - Pattern encryption settings for CBCS
 * - Encryption flag and IV size
 * - Default key identifier (KID)
 * - Constant IV for CBCS scheme
 */
void Mp4Demux::ParseTrackEncryptionBox()
{
	ReadHeader();

	ptr++; // skip
	uint8_t pattern = *ptr++;
	if (schemeType == MultiChar_Constant("cbcs"))
	{
		cryptByteBlock = (pattern >> 4) & 0xf;
		skipByteBlock = pattern & 0xf;
	}
	codecInfo.mIsEncrypted = *ptr++;
	// This is used to ensure encrypted caps are persisted even if its clear samples
	handledEncryptedSamples = true;
	ivSize = *ptr++;

	defaultKid = std::string(reinterpret_cast<const char*>(ptr), 16);
	ptr += 16;
	if (schemeType == MultiChar_Constant("cbcs"))
	{
		constantIvSize = *ptr++;
		if (constantIvSize != 8 && constantIvSize != 16)
		{
			parseError = MP4_PARSE_ERROR_INVALID_CONSTANT_IV_SIZE;
			MP4_LOG_ERR("Invalid constant IV size: %u, expected 8 or 16", constantIvSize);
			return;
		}
		constantIv = std::vector<uint8_t>(ptr, ptr + constantIvSize);
		ptr += constantIvSize;
	}
}

/**
 * @brief Parse protection system specific header box (PSSH)
 * Extracts DRM protection system data including:
 * - System ID (formatted as UUID string)
 * - PSSH data blob for DRM license acquisition
 * The parsed data is stored for later DRM initialization.
 * 
 * @param next Pointer to next box boundary
 */
void Mp4Demux::ParseProtectionSystemSpecificHeaderBox(const uint8_t *next)
{
	ReadHeader();
	char system_id[37]; // 32 hex chars + 4 hyphens + 1 null terminator
	snprintf(system_id, sizeof(system_id), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			ptr[0x0], ptr[0x1], ptr[0x2], ptr[0x3], ptr[0x4], ptr[0x5], ptr[0x6], ptr[0x7],
			ptr[0x8], ptr[0x9], ptr[0xa], ptr[0xb], ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf]);
	ptr += 16;
	size_t pssh_size = next - ptr;

	protectionData.emplace_back();
	AampPsshData &psshData = protectionData.back();
	psshData.systemID = std::string(system_id);
	psshData.pssh = std::vector<uint8_t>(ptr, ptr + pssh_size);
	// Update ptr to next box
	SkipBytes(pssh_size);
}

/**
 * @brief Process auxiliary information for encrypted samples
 * Reads encryption metadata from auxiliary information when no SENC box
 * is present. Processes initialization vectors and subsample encryption
 * data for each sample, applying the appropriate cipher mode (CENC/CBCS).
 */
void Mp4Demux::ProcessAuxiliaryInformation()
{
	//Backup the ptr value
	const uint8_t* bptr = ptr;
	size_t sample_count = cencAuxInfoSizes.size();
	if (sample_count && gotAuxiliaryInformationOffset)
	{
		ptr = moofPtr + auxiliaryInformationOffset;
		uint64_t maxSampleCount = sampleOffset + sample_count;
		if (samples.size() != maxSampleCount)
		{
			parseError = MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH;
			MP4_LOG_ERR("Sample count mismatch: expected %" PRIu64 ", got %zu", maxSampleCount, samples.size());
			return;
		}
		for (auto i = sampleOffset; i < maxSampleCount; i++)
		{
			samples[i].mDrmMetadata.mIsEncrypted = true;
			samples[i].mDrmMetadata.mKeyId = defaultKid;
			// TODO: Original media type is skipped for now
			if (schemeType == MultiChar_Constant("cbcs"))
			{
				samples[i].mDrmMetadata.mCipher = "cbcs";
				samples[i].mDrmMetadata.mCryptByteBlock = cryptByteBlock;
				samples[i].mDrmMetadata.mSkipByteBlock = skipByteBlock;
			}
			else
			{
				samples[i].mDrmMetadata.mCipher = "cenc";
			}
			// Skip IV data if present (comes before subsample data in auxiliary info)
			if (ivSize)
			{
				// Read IV if not already present from senc box
				if (samples[i].mDrmMetadata.mIV.empty())
				{
					samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + ivSize);
				}
				ptr += ivSize;
			}
			else if (schemeType == MultiChar_Constant("cbcs") && !constantIv.empty())
			{
				samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(constantIv.begin(), constantIv.end());
			}

			if (cencAuxInfoSizes[i - sampleOffset] > ivSize)
			{
				// Sub-sample encryption info present
				uint16_t n_subsamples = ReadU16();
				size_t subsamples_size = n_subsamples * MP4_SUBSAMPLE_ENTRY_SIZE;
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
 * Reads the sizes of auxiliary information entries for encrypted samples.
 * Each entry corresponds to the size of encryption metadata (IV + subsample info)
 * for one sample. Supports both default size and individual size modes.
 */
void Mp4Demux::ParseSampleAuxiliaryInformationSizes()
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
		ParseAuxInfo();
	}
	uint8_t default_info_size = *ptr++;
	uint32_t sampleCount = ReadU32();
	if (default_info_size)
	{
		for (auto i = 0u; i < sampleCount; i++ )
		{
			cencAuxInfoSizes.push_back(default_info_size);
		}
	}
	else
	{
		for (auto i = 0u; i < sampleCount; i++ )
		{
			cencAuxInfoSizes.push_back(ptr[i]);
		}
		ptr += sampleCount;
	}
}

/**
 * @brief Parse auxiliary information type parameters
 * Reads optional auxiliary information type and parameters when present.
 * Used in conjunction with SAIZ and SAIO boxes to specify encryption
 * auxiliary information format.
 */
void Mp4Demux::ParseAuxInfo()
{
	uint32_t schemeType = ReadU32(); // cenc or cbcs
	if (schemeType != MultiChar_Constant("cenc") && schemeType != MultiChar_Constant("cbcs"))
	{
		parseError = MP4_PARSE_ERROR_UNSUPPORTED_ENCRYPTION_SCHEME;
		MP4_LOG_ERR("Unsupported encryption scheme type: 0x%08x, expected 'cenc' or 'cbcs'", schemeType);
		return;
	}

	uint32_t auxInfoTypeParameter = ReadU32();
	(void)auxInfoTypeParameter;
}

/**
 * @brief Parse sample auxiliary information offsets box (SAIO)
 * Reads the offset to auxiliary information data within the movie fragment.
 * This offset points to where encryption metadata (IVs, subsample info) is
 * stored for encrypted samples. Supports both 32-bit and 64-bit offsets.
 */
void Mp4Demux::ParseSampleAuxiliaryInformationOffsets()
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
		ParseAuxInfo();
	}
	uint32_t entry_count = ReadU32();
	(void)entry_count	;
	if( version == 0 )
	{
		auxiliaryInformationOffset = ReadU32();
	}
	else
	{
		auxiliaryInformationOffset = ReadU64();
	}
	gotAuxiliaryInformationOffset = true;
}

/**
 * @brief Parse sample encryption box (SENC)
 * Processes encryption metadata directly embedded in the SENC box.
 * For each encrypted sample, extracts:
 * - Initialization vector (IV)
 * - Subsample encryption information (clear/encrypted byte pairs)
 * - Cipher mode and pattern encryption settings
 */
void Mp4Demux::ParseSampleEncryption()
{
	ReadHeader();
	uint32_t sampleCount = ReadU32();
	uint64_t maxSampleCount = sampleOffset + sampleCount;
	if (samples.size() != maxSampleCount)
	{
		parseError = MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH;
		MP4_LOG_ERR("Sample count mismatch in SENC: expected %" PRIu64 ", got %zu", maxSampleCount, samples.size());
		return;
	}
	for (auto iSample = sampleOffset; iSample < maxSampleCount; iSample++)
	{
		samples[iSample].mDrmMetadata.mIsEncrypted = true;
		samples[iSample].mDrmMetadata.mKeyId = defaultKid;
		if (schemeType == MultiChar_Constant("cbcs"))
		{
			samples[iSample].mDrmMetadata.mCipher = "cbcs";
			samples[iSample].mDrmMetadata.mCryptByteBlock = cryptByteBlock;
			samples[iSample].mDrmMetadata.mSkipByteBlock = skipByteBlock;
		}
		else
		{
			samples[iSample].mDrmMetadata.mCipher = "cenc";
		}
		if (ivSize)
		{
			samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + ivSize);
			ptr += ivSize;
		}
		else if (schemeType == MultiChar_Constant("cbcs") && !constantIv.empty())
		{
			samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(constantIv.begin(), constantIv.end());
		}

		if (flags & 2)
		{ // sub sample encryption
			uint16_t n_subsamples = ReadU16();
			size_t subsamples_size = n_subsamples * MP4_SUBSAMPLE_ENTRY_SIZE;
			samples[iSample].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subsamples_size);
			ptr += subsamples_size;
		}
	}
	sencPresent = true;
}

/**
 * @brief Parse track run box (TRUN)
 * Extracts sample information from the track run including:
 * - Sample data offsets and sizes
 * - Sample durations and composition time offsets
 * - Sample flags and media timing information
 * Creates AampMediaSample objects with proper PTS/DTS timing.
 */
void Mp4Demux::ParseTrackRun()
{
	ReadHeader();
	uint32_t sample_count = ReadU32();
	const unsigned char *data_ptr = moofPtr;
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
		parseError = MP4_PARSE_ERROR_MISSING_DATA_OFFSET;
		MP4_LOG_ERR("Missing data offset in TRUN box");
		return;
	}
	uint32_t sample_flags = 0;
	if (flags & 0x0004)
	{
		sample_flags = ReadU32();
	}
	uint64_t dts = baseMediaDecodeTime;
	for (auto i = 0u; i < sample_count; i++)
	{
		samples.emplace_back();
		// Get reference to newly added sample
		AampMediaSample& newSample = samples.back();
		uint32_t sample_len = defaultSampleSize;
		uint32_t sample_duration = defaultSampleDuration;
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
		newSample.mDts = dts / (double)timeScale;
		newSample.mPts = (dts + sample_composition_time_offset) / (double)timeScale;
		newSample.mDuration = sample_duration / (double)timeScale;
		dts += sample_duration;
	}
}

/**
 * @brief Parse track fragment header box (TFHD)
 * Extracts track fragment header information including:
 * - Track ID
 * - Base data offset for media data
 * - Default sample description index
 * - Default sample duration and size
 * - Default sample flags
 */
void Mp4Demux::ParseTrackFragmentHeader()
{
	ReadHeader();
	trackId = ReadU32();
	if (flags & 0x00001)
	{
		baseDataOffset = ReadU64();
	}
	if (flags & 0x00002)
	{
		defaultSampleDescriptionIndex = ReadU32();
	}
	if (flags & 0x00008)
	{
		defaultSampleDuration = ReadU32();
	}
	if (flags & 0x00010)
	{
		defaultSampleSize = ReadU32();
	}
	if (flags & 0x00020)
	{
		defaultSampleFlags = ReadU32();
	}
}

/**
 * @brief Parse track fragment decode time box (TFDT)
 * Extracts the base media decode time for the track fragment.
 * This value is used to calculate sample DTS values within the fragment.
 */
void Mp4Demux::ParseTrackFragmentDecodeTime()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	baseMediaDecodeTime = ReadBytes(sz);
}

/**
 * @brief Parse video information from sample entry
 * Extracts video-specific properties from the sample description:
 * - Video dimensions (width, height)
 * - Display resolution information
 * - Frame count and color depth
 * - Data reference index for media data location
 */
void Mp4Demux::ParseVideoInformation()
{
	SkipBytes(4); // always zero?
	dataReferenceIndex = ReadU32();
	SkipBytes(16); // always zero?
	codecInfo.mInfo.video.mWidth = ReadU16();
	codecInfo.mInfo.video.mHeight = ReadU16();
	codecInfo.mInfo.video.mHorizontalResolution = ReadU32();
	codecInfo.mInfo.video.mVerticalResolution = ReadU32();
	SkipBytes(4);
	codecInfo.mInfo.video.mFrameCount = ReadU16();
	SkipBytes(32); // compressor_name
	codecInfo.mInfo.video.mDepth = ReadU16();
	int pad = ReadU16();
	if (pad != 0xffff)
	{
		// TODO: Is it a critical error?
		parseError = MP4_PARSE_ERROR_INVALID_PADDING;
		MP4_LOG_ERR("Invalid padding value: 0x%04x, expected 0xffff", pad);
		return;
	}
}

/**
 * @brief Parse audio information from sample entry
 * Extracts audio-specific properties from the sample description:
 * - Channel count and sample size
 * - Sample rate information
 * - Data reference index for media data location
 */
void Mp4Demux::ParseAudioInformation()
{
	SkipBytes(4); // zero
	dataReferenceIndex = ReadU32();
	SkipBytes(8); // zero
	codecInfo.mInfo.audio.mChannelCount = ReadU16();
	codecInfo.mInfo.audio.mSampleSize = ReadU16();
	SkipBytes(4); // zero
	codecInfo.mInfo.audio.mSampleRate = ReadU16();
	SkipBytes(2); // zero
}

/**
 * @brief Parse movie header box (MVHD)
 * Extracts global movie properties including:
 * - Creation and modification times
 * - Time scale and duration
 * - Playback rate and volume
 * - Transformation matrix for video rendering
 */
void Mp4Demux::ParseMovieHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	creationTime = ReadBytes(sz);
	modificationTime = ReadBytes(sz);
	timeScale = ReadU32();
	duration = ReadBytes(sz);
	rate = ReadU32();
	volume = ReadU32(); // fixed point
	ptr += 8; // reserved
	for (int i = 0; i < 9; i++)
	{
		matrix[i] = ReadI32();
	}
	// skip pre_defined
	SkipBytes(24);
	// skip next trackID
	SkipBytes(4);
}

/**
 * @brief Parse track header box (TKHD)
 * Extracts track-specific properties including:
 * - Creation and modification times
 * - Track ID and duration
 * - Layer, alternate group, and volume
 * - Transformation matrix and video dimensions
 */
void Mp4Demux::ParseTrackHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	creationTime = ReadBytes(sz);
	modificationTime = ReadBytes(sz);
	trackId = ReadU32();
	ptr += 20 + sz; // duration, layer, alternateGroup, volume
	for (int i = 0; i < 9; i++)
	{
		matrix[i] = ReadI32();
	}
	widthFixed = ReadU32();
	heightFixed = ReadU32();
}

/**
 * @brief Parse media header box (MDHD)
 * Extracts media-specific properties including:
 * - Creation and modification times
 * - Time scale and duration
 * - Language code
 */
void Mp4Demux::ParseMediaHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	creationTime = ReadBytes(sz);
	modificationTime = ReadBytes(sz);
	timeScale = ReadU32();
	duration = ReadBytes(sz);
	language = ReadU16();
	// skip pre_defined
	SkipBytes(2);
}

/**
 * @brief Parse handler reference box (HDLR)
 * Extracts handler type and name for the media track.
 */
void Mp4Demux::ParseHandlerReference()
{
	ReadHeader();
	SkipBytes(4); // pre_defined
	uint32_t handler_type = ReadU32();
	(void)handler_type;
	SkipBytes(12); // reserved
	// handler name is null-terminated string (remaining bytes)
}

/**
 * @brief Parse movie fragment header box (MFHD)
 * Extracts the sequence number for the movie fragment.
 * This number indicates the order of fragments within the movie.
 */
void Mp4Demux::ParseMovieFragmentHeaderBox()
{
	ReadHeader();
	uint32_t sequence_number = ReadU32();
	(void)sequence_number;
}

/**
 * @brief Parse movie extends header box (MEHD)
 * Extracts the fragment duration for the movie fragment.
 */
void Mp4Demux::ParseMovieExtendsHeader()
{
	ReadHeader();
	fragmentDuration = ReadU32();
}

/**
 * @brief Parse track extends box (TREX)
 * Extracts default sample properties for the track:
 * - Track ID
 * - Default sample description index
 * - Default sample duration and size
 * - Default sample flags
 */
void Mp4Demux::ParseTrackExtendsBox()
{
	ReadHeader();
	trackId = ReadU32();
	defaultSampleDescriptionIndex = ReadU32();
	defaultSampleDuration = ReadU32();
	defaultSampleSize = ReadU32();
	defaultSampleFlags = ReadU32();
}

/**
 * @brief Parse sample description box (STSD)
 * Extracts the number of sample descriptions and processes them.
 * Currently only supports a single sample description.
 * 
 * @param next Pointer to next box
 */
void Mp4Demux::ParseSampleDescriptionBox(const uint8_t *next)
{
	ReadHeader();
	uint32_t count = ReadU32();
	if (count != 1)
	{
		parseError = MP4_PARSE_ERROR_UNSUPPORTED_SAMPLE_ENTRY_COUNT;
		MP4_LOG_ERR("Unsupported sample description count: %u, expected 1", count);
		return;
	}
	DemuxHelper(next);
}

/**
 * @brief Parse stream format box
 * Determines the codec type from the FourCC and
 * invokes the appropriate parsing function for
 * video or audio information extraction.
 * 
 * @param type FourCC type
 * @param next Pointer to next box
 */
void Mp4Demux::ParseStreamFormatBox(uint32_t type, const uint8_t *next)
{
	//codecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC(type);
	streamFormat = type;
	switch (streamFormat)
	{
		case MultiChar_Constant("hev1"):
		case MultiChar_Constant("avc1"):
		case MultiChar_Constant("hvc1"):
		case MultiChar_Constant("encv"):
			ParseVideoInformation();
			break;

		case MultiChar_Constant("mp4a"):
		case MultiChar_Constant("ec-3"):
		case MultiChar_Constant("enca"):
			ParseAudioInformation();
			break;

		default:
			parseError = MP4_PARSE_ERROR_UNSUPPORTED_STREAM_FORMAT;
			MP4_LOG_ERR("Unsupported stream format: 0x%08x", streamFormat);
			break;
	}
	// No need to continue if error occurred
	if (parseError != MP4_PARSE_OK)
	{
		return;
	}
	DemuxHelper(next);
}

/**
 * @brief Read length field with variable encoding
 * Reads a variable-length encoded integer used in Elementary Stream
 * Descriptor (ESDS) boxes. Each byte contains 7 bits of data and
 * 1 continuation bit. Used for AAC codec configuration parsing.
 * 
 * @return Length value decoded from variable-length encoding
 */
int Mp4Demux::ReadLen()
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
 * Recursively parses Elementary Stream Descriptor structure for AAC audio:
 * - Tag 0x03: ES descriptor
 * - Tag 0x04: Decoder config descriptor (object type, stream type, bitrates)
 * - Tag 0x05: Decoder specific info (actual codec configuration data)
 * - Tag 0x06: SL config descriptor
 * 
 * @param next Pointer to end of data
 */
void Mp4Demux::ParseCodecConfigHelper(const uint8_t *next)
{
	while (ptr < next)
	{
		uint32_t tag = *ptr++;
		uint32_t len = ReadLen();
		const uint8_t *end = ptr + len;
		switch (tag)
		{
			case 0x03:
				SkipBytes(3);
				ParseCodecConfigHelper(end);
				break;

			case 0x04:
				codecInfo.mInfo.audio.mObjectTypeId = *ptr++; // 5 = AAC LC
				codecInfo.mInfo.audio.mStreamType = *ptr++; // >>2
				codecInfo.mInfo.audio.mUpStream = *ptr++;
				codecInfo.mInfo.audio.mBufferSize = ReadU16();
				codecInfo.mInfo.audio.mMaxBitrate = ReadU32();
				codecInfo.mInfo.audio.mAvgBitrate = ReadU32();
				ParseCodecConfigHelper(end);
				break;

			case 0x05:
				codecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + len);
				ptr += len;
				break;

			case 0x06:
				SkipBytes(len);
				break;

			default:
				parseError = MP4_PARSE_ERROR_INVALID_ESDS_TAG;
				MP4_LOG_ERR("Invalid ESDS tag: 0x%02x", tag);
				return;
				break;
		}
		if (parseError != MP4_PARSE_OK)
		{
			return;
		}
		if (ptr != end)
		{
			parseError = MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH;
			MP4_LOG_ERR("Data boundary mismatch in codec config, ptr offset: %td", ptr - end);
		}
	}
}

/**
 * @brief Parse codec configuration box
 * Handles codec-specific configuration data:
 * - ESDS: AAC Elementary Stream Descriptor (complex structure)
 * - avcC: H.264 configuration (SPS/PPS data)
 * - hvcC: HEVC configuration (VPS/SPS/PPS data)
 * - dec3: Enhanced AC3 configuration (skipped for now)
 * 
 * @param type FourCC type identifier
 * @param next Pointer to next box boundary
 */
void Mp4Demux::ParseCodecConfigurationBox(uint32_t type, const uint8_t *next)
{
	codecInfo.mCodecFormat = GetStreamOutputFormatFromFourCC(type);
	if (type == MultiChar_Constant("esds"))
	{
		SkipBytes(4);
		ParseCodecConfigHelper(next);
	}
	else
	{
		size_t codec_data_len = next - ptr;
		//No need to read this for dec3 box. Filter this against other types if any.
		if (type != MultiChar_Constant("dec3"))
		{
			codecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + codec_data_len);
		}
		// Update ptr to next box
		SkipBytes(codec_data_len);
	}
}

/**
 * @brief Main demux helper function
 * Core MP4 parsing engine that recursively processes MP4 boxes.
 * Handles box size calculation, type identification, and dispatches
 * to appropriate parsing functions based on box type (FourCC).
 * Supports both standard and encrypted MP4 container formats.
 * 
 * @param fin Pointer to end of data to parse
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
				ParseStreamFormatBox(type, next);
				break;

			case MultiChar_Constant("hvcC"):
			case MultiChar_Constant("dec3"):
			case MultiChar_Constant("avcC"):
			case MultiChar_Constant("esds"): // Elementary Stream Descriptor
				ParseCodecConfigurationBox(type, next);
				break;

			case MultiChar_Constant("pssh"):
				ParseProtectionSystemSpecificHeaderBox(next);
				break;

			case MultiChar_Constant("saio"): // points to first IV in senc box
				ParseSampleAuxiliaryInformationOffsets();
				break;

			case MultiChar_Constant("saiz"): // defines size of senc entries
				ParseSampleAuxiliaryInformationSizes();
				break;

			case MultiChar_Constant("senc"): // modern, optional
				ParseSampleEncryption();
				break;

			case MultiChar_Constant("mfhd"):
				ParseMovieFragmentHeaderBox();
				break;

			case MultiChar_Constant("tfhd"):
				ParseTrackFragmentHeader();
				break;

			case MultiChar_Constant("trun"):
				ParseTrackRun();
				break;

			case MultiChar_Constant("tfdt"):
				ParseTrackFragmentDecodeTime();
				break;

			case MultiChar_Constant("mvhd"):
				ParseMovieHeader();
				break;

			case MultiChar_Constant("mehd"):
				ParseMovieExtendsHeader();
				break;

			case MultiChar_Constant("trex"):
				ParseTrackExtendsBox();
				break;

			case MultiChar_Constant("tkhd"):
				ParseTrackHeader();
				break;

			case MultiChar_Constant("mdhd"):
				ParseMediaHeader();
				break;

			case MultiChar_Constant("stsd"): // Sample Description
				ParseSampleDescriptionBox(next);
				break;

			case MultiChar_Constant("ftyp"): // FileType (major_brand, minor_version, compatible_brands)
			case MultiChar_Constant("hdlr"): // Handler Reference (handler, name)
			case MultiChar_Constant("vmhd"): // Video Media Header (graphics_mode, op_color)
			case MultiChar_Constant("smhd"): // Sound Media Header (balance)
			case MultiChar_Constant("dref"): // Data Reference (url) (under dinf box)
			case MultiChar_Constant("stts"): // Decoding Time To Sample (under stbl box)
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
				// Skip these boxes for now
				ptr = next;
				break;

			case MultiChar_Constant("schm"):
				ParseSchemeManagementBox();
				break;

			case MultiChar_Constant("frma"):
				ParseOriginalFormat();
				break;

			case MultiChar_Constant("tenc"):
				ParseTrackEncryptionBox();
				break;

			case MultiChar_Constant("moof"):  // Movie Fragment
				moofPtr = ptr - 8;
				// For LLD streams, we may have multiple moof boxes
				// so we need to track sampleOffset to map samples to mdat
				sampleOffset = samples.size();
				// Reset encryption state for each moof
				gotAuxiliaryInformationOffset = false;
				cencAuxInfoSizes.clear();
				sencPresent = false;

				DemuxHelper(next);

				if (!sencPresent && gotAuxiliaryInformationOffset)
				{
					// If no 'senc' box, we need to get IVs and subsample data from auxiliary info
					ProcessAuxiliaryInformation();
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
		if (parseError != MP4_PARSE_OK)
		{
			return;
		}
		if (ptr != next)
		{
			parseError = MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH;
			MP4_LOG_ERR("Box type %s data boundary mismatch, ptr offset: %td", FourCCToString(type).c_str(), ptr - next);
			return;
		}
	}
}

/**
 * @brief Parse MP4 data segment
 * Main entry point for MP4 parsing. Resets sample data from previous
 * segments while preserving metadata, then initiates recursive parsing
 * of the MP4 container structure. Handles both initialization segments
 * and media fragments.
 * 
 * @param ptr Pointer to MP4 data buffer
 * @param len Length of data buffer in bytes
 * @return true if parsing succeeded, false on error
 */
bool Mp4Demux::Parse(const void *ptr, size_t len)
{
	// Reset error state
	parseError = MP4_PARSE_OK;

	// scrub sample data from previous segment, but leave other metadata intact
	samples.clear();
	cencAuxInfoSizes.clear();
	protectionData.clear();
	gotAuxiliaryInformationOffset = false;
	moofPtr = NULL;
	if (ptr)
	{
		this->ptr = (const uint8_t *)ptr;
		DemuxHelper(&this->ptr[len]);
		if (parseError != MP4_PARSE_OK)
		{
			return false;
		}
	}
	// Force encrypted flag if any encrypted samples were handled previously
	// For GStreamer, renegotiation will fail if the caps change from
	// encrypted to clear, so we need to keep the encrypted flag set.
	if (handledEncryptedSamples && codecInfo.mIsEncrypted == false)
	{
		MP4_LOG(MP4_LOG_WARNING, "Forcing encrypted flag in codec info due to prior encrypted samples");
		codecInfo.mIsEncrypted = true;
	}
	return true;
}

/**
 * @brief Get last parser error
 * Returns the last error that occurred during parsing.
 * 
 * @return Mp4ParseError indicating the last error
 */
Mp4ParseError Mp4Demux::GetLastError() const
{
	return parseError;
}

/**
 * @brief Get media timescale value
 * Returns the timescale used for media timing calculations.
 * Used to convert media time units to seconds for PTS/DTS values.
 * 
 * @return Media timescale in units per second
 */
uint32_t Mp4Demux::GetTimeScale() const
{
	return timeScale;
}

/**
 * @brief Get codec information
 * Returns comprehensive codec information including format,
 * media type, and codec-specific parameters extracted from
 * the MP4 sample description.
 * 
 * @return Codec information with ownership transferred to caller
 */
AampCodecInfo Mp4Demux::GetCodecInfo()
{
	return std::move(codecInfo);
}

/**
 * @brief Get DRM protection system data
 * Returns all PSSH (Protection System Specific Header) data
 * extracted from the MP4 container for DRM license acquisition.
 * 
 * @return Protection data vector with ownership transferred to caller
 */
std::vector<AampPsshData> Mp4Demux::GetProtectionEvents()
{
	return std::move(protectionData);
}

/**
 * @brief Get parsed media samples
 * Returns all media samples extracted from the current MP4 fragment,
 * including sample data, timing information, and encryption metadata.
 * 
 * @return Media samples vector with ownership transferred to caller
 */
std::vector<AampMediaSample> Mp4Demux::GetSamples()
{
	return std::move(samples);
}