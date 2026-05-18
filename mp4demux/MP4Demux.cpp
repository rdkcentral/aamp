/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include <utility>

// Each subsample entry is 6 bytes (2 bytes for clear + 4 bytes for encrypted)
#define MP4_SUBSAMPLE_ENTRY_SIZE 6

// Track Fragment Header (TFHD) box flags
#define TFHD_BASE_DATA_OFFSET_PRESENT          0x00001
#define TFHD_SAMPLE_DESCRIPTION_INDEX_PRESENT  0x00002
#define TFHD_DEFAULT_SAMPLE_DURATION_PRESENT   0x00008
#define TFHD_DEFAULT_SAMPLE_SIZE_PRESENT       0x00010
#define TFHD_DEFAULT_SAMPLE_FLAGS_PRESENT      0x00020

// Track Run (TRUN) box flags
#define TRUN_DATA_OFFSET_PRESENT                     0x0001
#define TRUN_FIRST_SAMPLE_FLAGS_PRESENT              0x0004
#define TRUN_SAMPLE_DURATION_PRESENT                 0x0100
#define TRUN_SAMPLE_SIZE_PRESENT                     0x0200
#define TRUN_SAMPLE_FLAGS_PRESENT                    0x0400
#define TRUN_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT  0x0800

// Sample Auxiliary Information (SAIZ/SAIO) box flags
#define SAIZ_SAIO_AUX_INFO_TYPE_PRESENT 0x1

// Sample Encryption (SENC) box flags
#define SENC_SUBSAMPLE_ENCRYPTION_PRESENT 0x2

// Video sample entry padding marker
#define VIDEO_PREDEFINED_PADDING_MARKER 0xffff

// Elementary Stream Descriptor (ESDS) tag values
#define ESDS_TAG_ES_DESCRIPTOR              0x03
#define ESDS_TAG_DECODER_CONFIG_DESCRIPTOR  0x04
#define ESDS_TAG_DECODER_SPECIFIC_INFO      0x05
#define ESDS_TAG_SL_CONFIG_DESCRIPTOR       0x06

#define TENC_BOX_KEY_ID_SIZE 16
#define PSSH_SYSTEM_ID_SIZE  16
#define FORMATTED_SYSTEM_ID_LENGTH 36

// Metrics logging interval in seconds (10 minutes).
#define MP4DEMUX_METRICS_LOG_INTERVAL_SECONDS 600

/**
 * @brief Mapping structure for FourCC to format conversion
 */
struct CodecMapping
{
	uint32_t fourCC;
	GstStreamOutputFormat format;
};

/**
 * @brief Mapping structure for FourCC to cipher type conversion
 */
struct CipherMapping
{
	uint32_t fourCC;
	CipherType cipher;
};

/**
 * @brief Mapping of FourCC codes to GstStreamOutputFormat
 */
constexpr CodecMapping gCodecMappings[] = {
	{ MultiChar_Constant("avcC"), GST_FORMAT_VIDEO_ES_H264 },
	{ MultiChar_Constant("hvcC"), GST_FORMAT_VIDEO_ES_HEVC },
	{ MultiChar_Constant("esds"), GST_FORMAT_AUDIO_ES_AAC_RAW },
	{ MultiChar_Constant("dec3"), GST_FORMAT_AUDIO_ES_EC3 },
	{ MultiChar_Constant("dac4"), GST_FORMAT_AUDIO_ES_AC4 } // AC-4 decoder config box
};

/**
 * @brief Mapping of FourCC codes to CipherType
 */
constexpr CipherMapping gCipherMappings[] = {
	{ MultiChar_Constant("cenc"), CIPHER_TYPE_CENC },
	{ MultiChar_Constant("cbcs"), CIPHER_TYPE_CBCS }
};

/**
 * @brief Convert FourCC code to stream output format
 * - avcC: H.264 video
 * - hvcC: HEVC video
 * - esds: AAC audio (raw)
 * - dec3: Enhanced AC3 audio
 *
 * @param fourCC Four character code from MP4 container
 * @return StreamOutputFormat corresponding to the codec type
 */
GstStreamOutputFormat GetGstStreamOutputFormatFromFourCC(const uint32_t fourCC)
{
	for (const auto& mapping : gCodecMappings)
	{
		if (mapping.fourCC == fourCC)
		{
			return mapping.format;
		}
	}
	return GST_FORMAT_UNKNOWN;
}

/**
 * @brief Convert FourCC code to cipher type
 * Maps MP4 encryption scheme FourCC codes to CipherType:
 * - cenc: AES-CTR encryption
 * - cbcs: AES-CBC encryption with pattern
 *
 * @param fourCC Four character code from MP4 container
 * @return CipherType corresponding to the encryption scheme
 */
CipherType GetCipherTypeFromFourCC(const uint32_t fourCC)
{
	for (const auto& mapping : gCipherMappings)
	{
		if (mapping.fourCC == fourCC)
		{
			return mapping.cipher;
		}
	}
	return CIPHER_TYPE_NONE;
}

/**
 * @brief Constructor for Mp4Demux
 * Initializes all member variables to their default values and sets up
 * the demuxer for MP4 parsing operations.
 */

Mp4Demux::Mp4Demux() :
	streamFormat(),
	mMediaTypeName("unknown"),
	ivSize(),
	cryptByteBlock(), skipByteBlock(),
	constantIvSize(), constantIv(), timeScale(),
	samples(), defaultKid(), gotAuxiliaryInformationOffset(),
	auxiliaryInformationOffset(), schemeType(CIPHER_TYPE_NONE),
	originalMediaType(), cencAuxInfoSizes(), protectionData(),
	moofPtr(),
	ptr(), endPtr(nullptr),
	version(), flags(), baseMediaDecodeTime(),
	trackId(), baseDataOffset(),
	mdatStart(nullptr), mdatEnd(nullptr),
	defaultSampleDescriptionIndex(), defaultSampleDuration(), defaultSampleSize(),
	defaultSampleFlags(),
	duration(),
	sampleOffset(), sencPresent(false),
	handledEncryptedSamples(false),
	mSampleInfo(),
	codecInfo(GST_FORMAT_INVALID), parseError(MP4_PARSE_OK),
	mLastLogTime(std::chrono::steady_clock::now()),
	mLogIntervalSeconds(MP4DEMUX_METRICS_LOG_INTERVAL_SECONDS),
	mDemuxTimeMs(),
	mFramesPerSecond()
{
	MP4_LOG_WARN("MP4Demux created and metrics will be logged every %d seconds",
		 static_cast<int>(mLogIntervalSeconds.count()));
}

/**
 * @brief Destructor for Mp4Demux
 * Cleans up resources and performs any necessary cleanup operations.
 */
Mp4Demux::~Mp4Demux()
{
	// Log final metrics at shutdown
	LogMetrics();
}

void Mp4Demux::setParseError( Mp4ParseError err, const char* what )
{
	parseError = err;
	const char *text = nullptr;
	switch( err )
	{
		default:
			text = "unknown parse error";
			break;
		case MP4_PARSE_OK:
			text = "unexpected setParseError(PARSE_OK)";
			break;
		case MP4_PARSE_ERROR_INVALID_BOX:
			text = "INVALID_BOX";
			break;
		case MP4_PARSE_ERROR_INVALID_IV_SIZE:
			text = "INVALID_IV_SIZE";
			break;
		case MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH:
			text = "SAMPLE_COUNT_MISMATCH";
			break;
		case MP4_PARSE_ERROR_UNSUPPORTED_ENCRYPTION_SCHEME:
			text = "UNSUPPORTED_ENCRYPTION_SCHEME";
			break;
		case MP4_PARSE_ERROR_INVALID_PADDING:
			text = "INVALID_PADDING";
			break;
		case MP4_PARSE_ERROR_UNSUPPORTED_SAMPLE_ENTRY_COUNT:
			text = "UNSUPPORTED_SAMPLE_ENTRY_COUNT";
			break;
		case MP4_PARSE_ERROR_UNSUPPORTED_STREAM_FORMAT:
			text = "UNSUPPORTED_STREAM_FORMAT";
			break;
		case MP4_PARSE_ERROR_INVALID_ESDS_TAG:
			text = "INVALID_ESDS_TAG";
			break;
		case MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH:
			text = "DATA_BOUNDARY_MISMATCH";
			break;
		case MP4_PARSE_ERROR_INVALID_INPUT:
			text = "INVALID_INPUT";
			break;
		case MP4_PARSE_ERROR_INVALID_KID:
			text = "INVALID_KID";
			break;
		case MP4_PARSE_ERROR_INVALID_ENTRY_COUNT:
			text = "INVALID_ENTRY_COUNT";
			break;
		case MP4_PARSE_ERROR_VARIABLE_LENGTH_OVERFLOW:
			text = "VARIABLE_LENGTH_OVERFLOW";
			break;
		case MP4_PARSE_ERROR_UNEXPECTED_IS_ENCRYPTED_FIELD:
			text = "UNEXPECTED_IS_ENCRYPTED_FIELD";
			break;
	}
	if (what && what[0] != '\0')
	{
		MP4_LOG_ERR( "%s: %s", text, what );
	}
	else
	{
		MP4_LOG_ERR( "%s", text );
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
	if (n <= 0 || n > 8 || !ptr || !endPtr || ptr + n > endPtr) {
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "ReadBytes: out of bounds");
	}
	uint64_t rc = 0;
	for (int i = 0; i < n; ++i) rc = (rc << 8) + (*ptr++);
	return rc;
}

/**
 * @brief Read 16-bit unsigned integer in big-endian format
 *
 * @return 16-bit unsigned integer value
 */
uint16_t Mp4Demux::ReadU16()
{
	return static_cast<uint16_t>(ReadBytes(2));
}

/**
 * @brief Read 32-bit unsigned integer in big-endian format
 *
 * @return 32-bit unsigned integer value
 */
uint32_t Mp4Demux::ReadU32()
{
	return static_cast<uint32_t>(ReadBytes(4));
}

/**
 * @brief Read 32-bit signed integer in big-endian format
 *
 * @return 32-bit signed integer value
 */
int32_t Mp4Demux::ReadI32()
{
	return static_cast<int32_t>(ReadBytes(4));
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
	if (!ptr || !endPtr || ptr + 1 > endPtr)
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "ReadHeader: out of bounds");
	version = *ptr++;
	flags = static_cast<uint32_t>(ReadBytes(3));
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
	if (!ptr || !endPtr || ptr + len > endPtr) {
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "SkipBytes: out of bounds");
	}
	ptr += len;
}

// === Parsing functions (representative ones simplified to rely on throws) ===

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
 */
void Mp4Demux::ParseSchemeManagementBox()
{
	ReadHeader();
	ParseProtectionSchemeInfo();
	SkipBytes(4); // scheme_type_parameter
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
	SkipBytes(1); // skip reserved
	uint8_t pattern = static_cast<uint8_t>(ReadBytes(1));
	if (schemeType == CIPHER_TYPE_CBCS)
	{
		cryptByteBlock = (pattern >> 4) & 0xf;
		skipByteBlock = pattern & 0xf;
	}
	
	switch( ReadBytes(1) )
	{
		case 0:
			codecInfo.mIsEncrypted = false;
			break;
		case 1:
			codecInfo.mIsEncrypted = true;
			break;
		default:
			throw Mp4ParseException(MP4_PARSE_ERROR_UNEXPECTED_IS_ENCRYPTED_FIELD, "invalid isEncrypted value (neither 0 or 1)");
			break;
	}
	
	// This is used to ensure encrypted caps are persisted even if its clear samples
	handledEncryptedSamples = true;
	ivSize = static_cast<uint8_t>(ReadBytes(1));
	
	if (ptr + TENC_BOX_KEY_ID_SIZE > endPtr) {
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "tenc: missing KID");
	}
	defaultKid.assign(ptr, ptr + TENC_BOX_KEY_ID_SIZE);
	ptr += TENC_BOX_KEY_ID_SIZE;
	
	if (version == 1)
	{ // Version 1 adds constant IV
		constantIvSize = static_cast<uint8_t>(ReadBytes(1));
		if (constantIvSize != 8 && constantIvSize != 16)
		{
			throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_IV_SIZE, "tenc: invalid IV size");
		}
		
		if (ptr + constantIvSize > endPtr) {
			throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "tenc[v1]: constant IV OOB");
		}
		constantIv.assign(ptr, ptr + constantIvSize);
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
	// Must have at least systemID (16)
	if (ptr + PSSH_SYSTEM_ID_SIZE > next)
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "pssh: short systemID");
	// Add new entry into protection data vector
	protectionData.emplace_back();
	MediaProtectionInfo &psshData = protectionData.back();
	// Format UUID (systemID)
	char systemIdBuffer[FORMATTED_SYSTEM_ID_LENGTH + 1]; // 36 chars + null terminator
	snprintf(systemIdBuffer, sizeof(systemIdBuffer),
			 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			 ptr[0x0], ptr[0x1], ptr[0x2], ptr[0x3], ptr[0x4], ptr[0x5], ptr[0x6], ptr[0x7],
			 ptr[0x8], ptr[0x9], ptr[0xa], ptr[0xb], ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf]);
	psshData.systemID.assign(systemIdBuffer, FORMATTED_SYSTEM_ID_LENGTH); // Copy without null terminator
	ptr += PSSH_SYSTEM_ID_SIZE;
	// Version-aware parsing:
	if (version == 1) {
		if (ptr + 4 > next) throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "pssh[v1]: short kidCount");
		uint32_t kidCount = ReadU32();
#if SIZE_MAX <= 0xffffffff
		// if size_t is 32-bit or smaller, perform overflow check
		if( kidCount > SIZE_MAX / 16 ) {
			throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_KID, "pssh[v1]: KID count overflow");
		}
#endif
		size_t kidBytes = 16*static_cast<size_t>(kidCount);
		if (ptr + kidBytes > next) {
			throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "pssh[v1]: KIDs OOB");
		}
		// Optional: store KIDs in psshData if your MediaProtectionInfo supports it
		ptr += kidBytes;
	}
	// data_size (u32) + data (blob)
	if (ptr + 4 > next) throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "pssh: short dataSize");
	uint32_t dataSize = ReadU32();
	if (ptr + dataSize > next)
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "pssh: data OOB");
	psshData.pssh.assign(ptr, ptr + dataSize);
	SkipBytes(dataSize);
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
	size_t sampleCount = cencAuxInfoSizes.size();
	if (sampleCount && gotAuxiliaryInformationOffset)
	{
		ptr = moofPtr + auxiliaryInformationOffset;
		// Ensure the auxiliary information offset does not point past the end of the buffer
		if (ptr >= endPtr)
		{
			throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "aux: auxiliaryInformationOffset exceeds buffer");
		}
		uint64_t maxSampleCount = sampleOffset + sampleCount;
		if (samples.size() != maxSampleCount)
		{
			throw Mp4ParseException(MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH, "aux: sampleCount mismatch");
		}
		for (auto i = sampleOffset; i < maxSampleCount; i++)
		{
			samples[i].mDrmMetadata.mIsEncrypted = true;
			samples[i].mDrmMetadata.mKeyId = defaultKid;
			// TODO: Original media type is skipped for now
			if (schemeType == CIPHER_TYPE_CBCS)
			{
				samples[i].mDrmMetadata.mCipher = CIPHER_TYPE_CBCS;
				samples[i].mDrmMetadata.mCryptByteBlock = cryptByteBlock;
				samples[i].mDrmMetadata.mSkipByteBlock = skipByteBlock;
			}
			else
			{
				samples[i].mDrmMetadata.mCipher = CIPHER_TYPE_CENC;
			}
			// Skip IV data if present (comes before subsample data in auxiliary info)
			if (ivSize)
			{
				if (ivSize > static_cast<size_t>(endPtr - ptr))
				{
					throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "aux: IV data exceeds buffer");
				}
				// Read IV if not already present from senc box
				if (samples[i].mDrmMetadata.mIV.empty())
				{
					samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + ivSize);
				}
				ptr += ivSize;
			}
			else if (schemeType == CIPHER_TYPE_CBCS && !constantIv.empty())
			{
				samples[i].mDrmMetadata.mIV = std::vector<uint8_t>(constantIv.begin(), constantIv.end());
			}
			if (cencAuxInfoSizes[i - sampleOffset] > ivSize)
			{
				// Sub-sample encryption info present
				uint16_t numSubSamples = ReadU16();
				size_t remaining = static_cast<size_t>(endPtr - ptr);
				if (numSubSamples > remaining / MP4_SUBSAMPLE_ENTRY_SIZE)
				{
					throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "aux: subsample data OOB");
				}
				size_t subSamplesSize = static_cast<size_t>(numSubSamples) * MP4_SUBSAMPLE_ENTRY_SIZE;
				samples[i].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subSamplesSize);
				samples[i].mDrmMetadata.mNumSubSamples = numSubSamples;
				ptr += subSamplesSize;
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
	if (flags & SAIZ_SAIO_AUX_INFO_TYPE_PRESENT)
	{
		ParseProtectionSchemeInfo();
		SkipBytes(4); // aux_info_type_parameter
	}
	uint8_t defaultInfoSize = *ptr++;
	uint32_t sampleCount = ReadU32();
	if (defaultInfoSize)
	{
		for (auto i = 0u; i < sampleCount; i++ )
		{
			cencAuxInfoSizes.push_back(defaultInfoSize);
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
 * @brief Parse protection scheme information
 * Extracts the encryption scheme type from the box
 * Supports 'cenc' (AES-CTR) and 'cbcs' (AES-CBC with pattern).
 */
void Mp4Demux::ParseProtectionSchemeInfo()
{
	uint32_t type = ReadU32();
	auto cipher = GetCipherTypeFromFourCC(type);
	if (cipher == CIPHER_TYPE_NONE)
	{
		throw Mp4ParseException(MP4_PARSE_ERROR_UNSUPPORTED_ENCRYPTION_SCHEME, "schm: unsupported cipher");
	}
	schemeType = cipher;
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
	ReadHeader();
	if (flags & SAIZ_SAIO_AUX_INFO_TYPE_PRESENT)
	{
		ParseProtectionSchemeInfo();
		SkipBytes(4); // aux_info_type_parameter
	}
	uint32_t entryCount = ReadU32();
	if (entryCount == 0)
	{
		throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_ENTRY_COUNT, "saio: zero entryCount");
	}
	// Read the first offset; if multiple, warn and consume others
	if (version == 0)
	{
		auxiliaryInformationOffset = ReadU32();
		for (uint32_t i = 1; i < entryCount; ++i) { (void)ReadU32(); }
		gotAuxiliaryInformationOffset = true;
	}
	else
	{
		auxiliaryInformationOffset = ReadU64();
		for (uint32_t i = 1; i < entryCount; ++i) { (void)ReadU64(); }
		gotAuxiliaryInformationOffset = true;
	}
}

/**
 * @brief Parse sample encryption box (SENC)
 * Processes encryption metadata directly embedded in the SENC box.
 * For each encrypted sample, extracts:
 * - Initialization vector (IV)
 * - Subsample encryption information (clear/encrypted byte pairs)
 * - Cipher mode and pattern encryption settings
 *
 * @param next Pointer to next box
 */
void Mp4Demux::ParseSampleEncryption(const uint8_t *next)
{
	ReadHeader();
	uint32_t sampleCount = ReadU32();
	uint64_t maxSampleCount = sampleOffset + sampleCount;
	MP4_LOG_DEBUG("senc: sampleCount=%" PRIu32 " ivSize=%u flags=0x%x subSamplePresent=%d boxRemaining=%zu",
		sampleCount, ivSize, flags,
		(flags & SENC_SUBSAMPLE_ENCRYPTION_PRESENT) ? 1 : 0,
		static_cast<size_t>(next - ptr));

	if (samples.size() != maxSampleCount)
	{
		throw Mp4ParseException(MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH, "senc: sampleCount mismatch");
	}
	for (auto iSample = sampleOffset; iSample < maxSampleCount; iSample++)
	{
		samples[iSample].mDrmMetadata.mIsEncrypted = true;
		samples[iSample].mDrmMetadata.mKeyId = defaultKid;
		samples[iSample].mDrmMetadata.mCipher = schemeType;
		if (schemeType == CIPHER_TYPE_CBCS)
		{
			samples[iSample].mDrmMetadata.mCryptByteBlock = cryptByteBlock;
			samples[iSample].mDrmMetadata.mSkipByteBlock = skipByteBlock;
		}
		if (ivSize)
		{
			if (ivSize > static_cast<size_t>(endPtr - ptr))
			{
				throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "senc: IV data exceeds buffer");
			}
			samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(ptr, ptr + ivSize);
			ptr += ivSize;
		}
		else if (schemeType == CIPHER_TYPE_CBCS && !constantIv.empty())
		{
			samples[iSample].mDrmMetadata.mIV = std::vector<uint8_t>(constantIv.begin(), constantIv.end());
		}
		if (flags & SENC_SUBSAMPLE_ENCRYPTION_PRESENT)
		{ // sub sample encryption
			uint16_t numSubSamples = ReadU16();
			size_t subSamplesSize = numSubSamples * MP4_SUBSAMPLE_ENTRY_SIZE;
			if (subSamplesSize > static_cast<size_t>(endPtr - ptr))
			{
				throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "senc: subsample data OOB");
			}
			samples[iSample].mDrmMetadata.mSubSamples = std::vector<uint8_t>(ptr, ptr + subSamplesSize);
			samples[iSample].mDrmMetadata.mNumSubSamples = numSubSamples;
			ptr += subSamplesSize;
		}
	}
	sencPresent = true;
}

/**
 * @brief Parse track run box (TRUN)
 * Extracts sample metadata from the track run including:
 * - Sample durations and composition time offsets
 * - Sample flags and media timing information
 * Actual sample data copy and boundary validation are deferred into mSampleInfo 
 * This handles both normal and LLD streams where mdat follows moof.
 */
void Mp4Demux::ParseTrackRun()
{
	ReadHeader();
	uint32_t sampleCount = ReadU32();
	const uint8_t *dataPtr = moofPtr;
	if (flags & TRUN_DATA_OFFSET_PRESENT)
	{ // offset from start of Moof box field
		int32_t dataOffset = ReadI32();
		dataPtr += dataOffset;
	}
	// ISO 14496-12: TRUN_FIRST_SAMPLE_FLAGS_PRESENT overrides the first sample only;
	// TRUN_SAMPLE_FLAGS_PRESENT provides per-sample flags; otherwise use defaultSampleFlags.
	uint32_t firstSampleFlags = 0;
	if (flags & TRUN_FIRST_SAMPLE_FLAGS_PRESENT)
	{
		firstSampleFlags = ReadU32();
	}
	uint64_t dts = baseMediaDecodeTime;
	for (auto i = 0u; i < sampleCount; i++)
	{
		samples.emplace_back();
		uint32_t sampleLen = defaultSampleSize;
		uint32_t sampleDuration = defaultSampleDuration;
		if (flags & TRUN_SAMPLE_DURATION_PRESENT)
		{
			sampleDuration = ReadU32();
		}
		if (flags & TRUN_SAMPLE_SIZE_PRESENT)
		{
			sampleLen = ReadU32();
		}
		uint32_t effectiveSampleFlags = defaultSampleFlags;
		if (flags & TRUN_SAMPLE_FLAGS_PRESENT)
		{ // per-sample flags present in TRUN
			effectiveSampleFlags = ReadU32();
		}
		else if (i == 0 && (flags & TRUN_FIRST_SAMPLE_FLAGS_PRESENT))
		{ // first-sample-only override (mutually exclusive with TRUN_SAMPLE_FLAGS_PRESENT)
			effectiveSampleFlags = firstSampleFlags;
		}
		// ISO 14496-12 sample_flags bit 16: sample_is_non_sync_sample
		// 0 = sync/key frame (I-frame), 1 = non-sync sample
		bool isKeyFrame = (effectiveSampleFlags & 0x00010000) == 0;
		int32_t sampleCompositionTimeOffset = 0;
		if (flags & TRUN_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)
		{ // for samples where pts and dts differ (overriding 'trex')
			sampleCompositionTimeOffset = ReadI32();
		}
		// mdat follows the moof in the bitstream
		// so boundary checks and data copy must wait until mdatStart/mdatEnd are established.
		PendingSamplePayload pendingSample;
		pendingSample.dataPtr  = dataPtr;
		pendingSample.sampleLen = sampleLen;
		pendingSample.sampleIdx = samples.size() - 1;
		pendingSample.mDts      = dts / (double)timeScale;
		pendingSample.mPts      = (dts + sampleCompositionTimeOffset) / (double)timeScale;
		pendingSample.mDuration = sampleDuration / (double)timeScale;
		pendingSample.mIsKeyFrame = isKeyFrame;
		mSampleInfo.emplace_back(pendingSample);
		dataPtr += sampleLen;
		dts += sampleDuration;
	}
}

/**
 * @brief Assign sample payload data after mdat is parsed
 *
 * Called immediately after mdatStart/mdatEnd are set from an mdat box.
 * Iterates over mSampleInfo, validates each sample's data
 * pointer against the mdat extent, and copies the payload into the corresponding AampMediaSample.
 * Clears mSampleInfo on exit.
 */
void Mp4Demux::ProcessSamples()
{
	if (!mdatStart || !mdatEnd)
	{
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "mdat: bounds not established");
	}
	for (const auto& pending : mSampleInfo)
	{
		const uint8_t* dataPtr = pending.dataPtr;
		uint32_t sampleLen = pending.sampleLen;
		// Validate data pointer is within current mdat bounds
		if (dataPtr < mdatStart || dataPtr >= mdatEnd)
		{
			throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "trun: dataPtr outside mdat");
		}
		// Guard: sample payload must not overrun mdat
		if (dataPtr + sampleLen > mdatEnd)
		{
			throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "trun: sample payload OOB");
		}
		AampMediaSample& s = samples[pending.sampleIdx];
		// Aliasing constructor: mData shares mCurrentSegment's refcount but
		// points directly at the sample payload within that buffer.
		s.mData       = std::shared_ptr<const uint8_t>(mCurrentSegment, dataPtr);
		s.mDataSize   = sampleLen;
		s.mDts        = pending.mDts;
		s.mPts        = pending.mPts;
		s.mDuration   = pending.mDuration;
		s.mIsKeyFrame = pending.mIsKeyFrame;
	}
	mSampleInfo.clear();
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
	if (flags & TFHD_BASE_DATA_OFFSET_PRESENT)
	{
		baseDataOffset = ReadU64();
	}
	if (flags & TFHD_SAMPLE_DESCRIPTION_INDEX_PRESENT)
	{
		defaultSampleDescriptionIndex = ReadU32();
	}
	if (flags & TFHD_DEFAULT_SAMPLE_DURATION_PRESENT)
	{
		defaultSampleDuration = ReadU32();
	}
	if (flags & TFHD_DEFAULT_SAMPLE_SIZE_PRESENT)
	{
		defaultSampleSize = ReadU32();
	}
	if (flags & TFHD_DEFAULT_SAMPLE_FLAGS_PRESENT)
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
	// Skip: reserved[6] (4) + data_reference_index (4) + reserved/pre_defined fields (16)
	SkipBytes(24);
	codecInfo.mInfo.video.mWidth  = ReadU16();
	codecInfo.mInfo.video.mHeight = ReadU16();
	// Skip: horizontal_resolution (4) + vertical_resolution (4) + reserved (4) + frame_count (2) + compressor_name (32) + depth (2)
	SkipBytes(48);
	int pad = ReadU16();
	if (pad != VIDEO_PREDEFINED_PADDING_MARKER)
	{
		throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_PADDING, "video: invalid padding marker");
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
	// Skip: reserved[6] (4) + data_reference_index (4) + reserved[2] (8)
	SkipBytes(16);
	codecInfo.mInfo.audio.mChannelCount = ReadU16();
	// Skip: sample_size (2) + pre_defined/reserved (4)
	SkipBytes(6);
	codecInfo.mInfo.audio.mSampleRate = ReadU16(); // Upper 16 bits of 32-bit fixed-point sample_rate
	SkipBytes(2); // Lower 16 bits of sample_rate (typically 0)
}

/**
 * @brief Parse movie header box (MVHD)
 * Extracts global movie properties including:
 * - Time scale and duration
 */
void Mp4Demux::ParseMovieHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	SkipBytes(sz); // creation_time
	SkipBytes(sz); // modification_time
	timeScale = ReadU32();
	duration = ReadBytes(sz);
	// Skip: rate (fixed-point 16.16) (4) + volume (fixed-point 8.8) (2) + reserved (10)
	SkipBytes(16);
	// Skip: matrix (36) + pre_defined[6] (24) + next_track_ID (4)
	SkipBytes(64);
}

/**
 * @brief Parse track header box (TKHD)
 * Extracts track-specific properties including:
 * - Track ID and duration
 */
void Mp4Demux::ParseTrackHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	SkipBytes(sz); // creation_time
	SkipBytes(sz); // modification_time
	trackId = ReadU32();
	//Skip: reserved (4) + duration (sz) + reserved[2] (8) + layer (2) + alternate_group (2) + volume (2) + reserved (2)
	SkipBytes(20 + sz);
	//Skip: matrix (36) + width_fixed (4) + height (4)
	SkipBytes(44);
}

/**
 * @brief Parse media header box (MDHD)
 * Extracts media-specific properties including:
 * - Time scale and duration
 */
void Mp4Demux::ParseMediaHeader()
{
	ReadHeader();
	int sz = (version == 1) ? 8 : 4;
	SkipBytes(sz); // creation_time
	SkipBytes(sz); // modification_time
	timeScale = ReadU32();
	duration = ReadBytes(sz);
	// Skip: 1 bit reserved + 3 x 5 bits language code (2) + pre_defined (2)
	SkipBytes(4);
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
	if (count != 1) {
		throw Mp4ParseException(MP4_PARSE_ERROR_UNSUPPORTED_SAMPLE_ENTRY_COUNT, "stsd: count != 1");
	}
	// Parse contained sample entries/config boxes
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
	streamFormat = type;
	switch (streamFormat)
	{
		case MultiChar_Constant("hev1"):
		case MultiChar_Constant("avc1"):
		case MultiChar_Constant("hvc1"):
		case MultiChar_Constant("encv"):
			mMediaTypeName = "video";
			ParseVideoInformation();
			break;
		case MultiChar_Constant("mp4a"):
		case MultiChar_Constant("ec-3"):
		case MultiChar_Constant("ac-4"):
		case MultiChar_Constant("enca"):
			mMediaTypeName = "audio";
			ParseAudioInformation();
			break;
		default:
			mMediaTypeName = "unknown";
			throw Mp4ParseException(MP4_PARSE_ERROR_UNSUPPORTED_STREAM_FORMAT, "stsd entry: unsupported format");
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
uint32_t Mp4Demux::ReadLen()
{
	if (!ptr || !endPtr) {
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "ReadLen: null bounds");
	}
	uint32_t rc = 0;
	int bits = 0;
	while (ptr < endPtr) {
		unsigned char octet = *ptr++;
		bits += 7;
		if (bits > 32) {
			throw Mp4ParseException(MP4_PARSE_ERROR_VARIABLE_LENGTH_OVERFLOW, "ReadLen: overflow >32 bits");
		}
		rc = (rc << 7) + (octet & 0x7f);
		if ((octet & 0x80) == 0) {
			return rc;
		}
	}
	throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "ReadLen: unterminated var int");
}

/**
 * @brief Parse ESDS codec configuration helper
 * Iteratively parses Elementary Stream Descriptor structure for AAC audio:
 * - Tag 0x03: ES descriptor (container)
 * - Tag 0x04: Decoder config descriptor (object type, stream type, bitrates)
 * - Tag 0x05: Decoder specific info (actual codec configuration data)
 * - Tag 0x06: SL config descriptor
 *
 * @param next Pointer to end of data
 */
void Mp4Demux::ParseEsdsCodecConfigHelper(const uint8_t *next)
{
	while (ptr < next)
	{
		uint32_t tag = *ptr++;
		uint32_t len = ReadLen();
		switch (tag)
		{
			case ESDS_TAG_ES_DESCRIPTOR:
				/**
				 * ES descriptor contains nested descriptors
				 * Skip ES_ID (2 bytes) and flags (1 byte)
				 */
				SkipBytes(3);
				break;
			case ESDS_TAG_DECODER_CONFIG_DESCRIPTOR:
				/**
				 * Decoder config descriptor contains nested descriptors
				 * objectTypeIndication (1 byte) - codec type (0x40 for AAC LC)
				 * streamType (1 byte packed) - stream type (6 bits) + upStream flag (1 bit) + reserved (1 bit)
				 * bufferSizeDB (3 bytes / 24 bits) - decoder buffer size
				 * maxBitrate (4 bytes / 32 bits) - maximum bitrate
				 * avgBitrate (4 bytes / 32 bits) - average bitrate
				 * Total: 1 + 1 + 3 + 4 + 4 = 13 bytes
				 */
				SkipBytes(13); // Skip entire decoder config descriptor header
				break;
			case ESDS_TAG_DECODER_SPECIFIC_INFO:
				// Leaf descriptor - contains actual codec configuration data
				if (ptr + len > next)
				{
					throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "esds: malformed config data");
				}
				codecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + len);
				ptr += len;
				break;
			case ESDS_TAG_SL_CONFIG_DESCRIPTOR:
				// Leaf descriptor - skip SL config
				SkipBytes(len);
				break;
			default:
				throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_ESDS_TAG, "esds: invalid tag");
		}
	}
	if (ptr != next)
	{
		throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "esds: parser did not land on next");
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
	codecInfo.mCodecFormat = GetGstStreamOutputFormatFromFourCC(type);
	if (type == MultiChar_Constant("esds"))
	{
		// Skip FullBox header: version (1 byte) + flags (3 bytes) = 4 bytes
		// The box size and type were already consumed by DemuxHelper
		SkipBytes(4);
		ParseEsdsCodecConfigHelper(next);
	}
	else
	{
		size_t codecDataLen = next - ptr;
		// No need to read this for dec3 box. Expand the filter later if needed.
		if (type != MultiChar_Constant("dec3"))
		{
			codecInfo.mCodecData = std::vector<uint8_t>(ptr, ptr + codecDataLen);
		}
		// Update ptr to next box
		SkipBytes(codecDataLen);
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
void Mp4Demux::ParseMovieExtendsHeader()
{
	// Currently not used
}

void Mp4Demux::DemuxHelper(const uint8_t *fin)
{
	while (ptr < fin)
	{
		uint64_t size = ReadU32();
		uint32_t type = ReadU32();
		const uint8_t *next = nullptr;
		if( size==0 )
		{ // box extends to end of buffer
			next = fin;
		}
		else
		{
			if( size==1 )
			{ // format: size(4)+type(4)+large_size(8)+payload
				size = ReadU64();
				if ( size < 16)
				{
					throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_BOX, "box: large_size < header");
				}
				size -= 16;
			}
			else if( size>=8 )
			{ // format: size(4)+type(4)+payload
				size -= 8;
			}
			else
			{
				throw Mp4ParseException(MP4_PARSE_ERROR_INVALID_BOX, "box: size < 8");
			}
			const uint64_t remaining = static_cast<uint64_t>(fin - ptr);
			if( size > remaining )
			{
				throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "box: payload > remaining");
			}
			next = ptr + size;
		}
		MP4_LOG_DEBUG("Box type: %s, size: %" PRIu64, FourCCToString(type).c_str(), size);
		switch (type)
		{
			case MultiChar_Constant("free"):
			case MultiChar_Constant("skip"):
				// free and skip are ISO BMFF padding boxes containing unused space
				ptr = next;
				break;
			case MultiChar_Constant("hev1"):
			case MultiChar_Constant("hvc1"):
			case MultiChar_Constant("avc1"):
			case MultiChar_Constant("mp4a"):
			case MultiChar_Constant("ec-3"):
			case MultiChar_Constant("ac-4"):
			case MultiChar_Constant("enca"):
			case MultiChar_Constant("encv"):
				ParseStreamFormatBox(type, next);
				break;
			case MultiChar_Constant("hvcC"):
			case MultiChar_Constant("dac4"): // AC-4 DecoderSpecific box
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
				ParseSampleEncryption(next);
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
			case MultiChar_Constant("schm"):
				ParseSchemeManagementBox();
				break;
			case MultiChar_Constant("frma"):
				ParseOriginalFormat();
				break;
			case MultiChar_Constant("tenc"):
				ParseTrackEncryptionBox();
				break;
			case MultiChar_Constant("moof"): // Movie Fragment
				// Update moofPtr to current moof box start
				moofPtr = ptr - 8;
				// For LLD streams, we may have multiple moof boxes
				// so we need to track sampleOffset to map samples to mdat
				sampleOffset = samples.size();
				// Reset encryption state for each moof
				gotAuxiliaryInformationOffset = false;
				cencAuxInfoSizes.clear();
				sencPresent = false;
				// Clear any leftover pending payloads from a prior fragment so Left over should be treated as error
				// they are not re-assigned when the next mdat is encountered.
				mSampleInfo.clear();
				DemuxHelper(next);
				MP4_LOG_DEBUG("Completed parsing 'moof' box, sampleOffset: %" PRIu64 " total samples: %zu", sampleOffset, samples.size());
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
				// Recursive parsing for container boxes
				DemuxHelper(next);
				break;
			case MultiChar_Constant("mehd"): // Movie Extends Header
			case MultiChar_Constant("mfhd"): // Movie Fragment Header
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
				ptr = next;
				break;
			case MultiChar_Constant("mdat"): // Movie Data (under file box)
				// Track mdat payload range for TRUN validation
				mdatStart = ptr; // start of payload (after header bytes)
				mdatEnd   = next; // end of payload
				// Now that mdat bounds are known, copy sample payloads that were deferred by ParseTrackRun() (handles both normal and LLD order).
				ProcessSamples();
				ptr = next; // skip payload
				break;
			default:
				// Unknown/unhandled box — skip payload and continue
				MP4_LOG_WARN("Skipping unknown box type: %s, size: %" PRIu64, FourCCToString(type).c_str(), size);
				ptr = next;
				break;
		}
		if (ptr != next)
		{
			throw Mp4ParseException(MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, "box: parser did not land on next");
		}
	}
}

/**
 * @brief Parse MP4 data segment with shared ownership of the backing buffer.
 *
 * Stores @p segment internally during parsing so that every extracted sample's
 * mData field (aliasing shared_ptr) keeps the buffer alive exactly as long as
 * the sample exists. Resets sample data from previous segments while preserving
 * metadata, then initiates recursive parsing of the MP4 container structure.
 * Handles both initialization segments and media fragments.
 * The internal mCurrentSegment hold is released before returning; only the
 * individual samples carry a shared reference thereafter.
 *
 * @param segment Shared ownership of the buffer to parse (must not be null/empty).
 * @return true if parsing succeeded, false on error
 */
bool Mp4Demux::Parse(std::shared_ptr<std::vector<uint8_t>>&& segment)
{
	if (!segment || segment->empty())
	{
		setParseError(MP4_PARSE_ERROR_INVALID_INPUT);
		return false;
	}
	mCurrentSegment = std::move(segment);

	const void *data = mCurrentSegment->data();
	size_t len = mCurrentSegment->size();
	MP4_LOG_DEBUG("Parsing MP4 data segment, ptr:%p len=%zu", data, len);

	// Start timing for metrics
	auto startTime = std::chrono::steady_clock::now();

	// Reset error state
	parseError = MP4_PARSE_OK;
	// scrub sample data from previous segment, but leave other metadata intact
	samples.clear();
	cencAuxInfoSizes.clear();
	protectionData.clear();
	mSampleInfo.clear();
	gotAuxiliaryInformationOffset = false;
	moofPtr = nullptr;
	endPtr = &((const uint8_t*)data)[len];
	mdatStart = nullptr;
	mdatEnd = nullptr;
	this->ptr = (const uint8_t *)data;

	bool ret = false;
	try {
		DemuxHelper(&this->ptr[len]);
		ret = true;
		// Force encrypted flag if any encrypted samples were handled previously.
		// For GStreamer, renegotiation will fail if the caps change from
		// encrypted to clear, so we need to keep the encrypted flag set.
		if (handledEncryptedSamples && codecInfo.mIsEncrypted == false)
		{
			MP4_LOG_WARN("Forcing encrypted flag in codec info due to prior encrypted samples");
			codecInfo.mIsEncrypted = true;
		}

		// Record metrics if parsing succeeded and samples are extracted
		auto endTime = std::chrono::steady_clock::now();
		auto demuxDuration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(endTime - startTime);
		uint32_t frameCount = static_cast<uint32_t>(samples.size());

		if (frameCount > 0)
		{
			double fragmentDuration = 0.0;
			for (const auto& sample : samples)
			{
				fragmentDuration += sample.mDuration;
			}
			double fps = (fragmentDuration > 0.0) ? (static_cast<double>(frameCount) / fragmentDuration) : 0.0;
			RecordDemuxMetrics(demuxDuration.count(), fps);
			MP4_LOG_DEBUG("Demux metrics: %u frames in %.3f ms", frameCount, demuxDuration.count());
		}
	} catch (const Mp4ParseException& ex) {
		setParseError(ex.code(), ex.what());
		ret = false;
	} catch (const std::exception& /*ex*/) {
		// Map unknown std exceptions to a generic parse error
		setParseError(MP4_PARSE_ERROR_INVALID_BOX);
		ret = false;
	}

	mCurrentSegment = nullptr;
	return ret;
}

void Mp4Demux::ParseOrThrow(std::shared_ptr<std::vector<uint8_t>>&& segment)
{
	if (!Parse(std::move(segment)))
	{
		throw Mp4ParseException(GetLastError(), "Parse failed");
	}
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
MediaCodecInfo Mp4Demux::GetCodecInfo()
{
	// std::move is required here because codecInfo is a member variable.
	// NRVO (Named Return Value Optimization) only applies to local variables,
	// not class members. Without std::move, the compiler would attempt to copy
	// (which fails since MediaCodecInfo has deleted copy constructor).
	return std::move(codecInfo);
}

/**
 * @brief Get DRM protection system data
 * Returns all PSSH (Protection System Specific Header) data
 * extracted from the MP4 container for DRM license acquisition.
 *
 * @return Protection data vector with ownership transferred to caller
 */
std::vector<MediaProtectionInfo> Mp4Demux::GetProtectionEvents()
{
	// std::move is required here because codecInfo is a member variable.
	return std::move(protectionData);
}

/**
 * @brief Return parsed media samples with lifetime tracking.
 *
 * Returns all media samples extracted from the current MP4 fragment,
 * including sample data, timing information, and encryption metadata.
 * Each sample's mData is an aliasing shared_ptr<const uint8_t> that keeps
 * the backing segment buffer (previously passed to Parse()) alive for the
 * sample's lifetime, enabling zero-copy access to the parsed payload.
 *
 * @return Media samples vector with ownership transferred to caller.
 */
std::vector<AampMediaSample> Mp4Demux::GetSamples()
{
	return std::move(samples);
}

/**
 * @brief Record metrics for a demux operation
 * Updates statistics and logs periodic reports every 10 minutes
 *
 * @param demuxTimeMs Time taken to demux a fragment in milliseconds
 * @param fps Frames per second calculated from the fragment
 */

void Mp4Demux::RecordDemuxMetrics(double demuxTimeMs, double fps)
{
	mDemuxTimeMs.Update(demuxTimeMs);
	mFramesPerSecond.Update(fps);

	if (ShouldLogMetrics())
	{
		LogMetrics();
	}
}
/**
 * @brief Log accumulated metrics to AAMP logs
 * Outputs min/max/avg for demux time and frames per second,
 * then resets the accumulators and updates the last-log timestamp.
 */

void Mp4Demux::LogMetrics()
{
	if (!mDemuxTimeMs.HasData() || !mFramesPerSecond.HasData())
		return;

	MP4_LOG_WARN("MP4Demux Metrics: mediaType=%s dmt(demux time)ms min=%.3f max=%.3f avg=%.3f | fps min=%.3f max=%.3f avg=%.3f",
		mMediaTypeName,
		mDemuxTimeMs.min, mDemuxTimeMs.max, mDemuxTimeMs.GetAverage(),
		mFramesPerSecond.min, mFramesPerSecond.max, mFramesPerSecond.GetAverage());

	mLastLogTime = std::chrono::steady_clock::now();
	mDemuxTimeMs.Reset();
	mFramesPerSecond.Reset();
}
/**
 * @brief Check if reporting interval has elapsed
 * @return true if 10 minutes have passed since last report
 */
bool Mp4Demux::ShouldLogMetrics() const
{
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - mLastLogTime);
	return elapsed >= mLogIntervalSeconds;
}
