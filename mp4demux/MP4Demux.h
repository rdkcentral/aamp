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
 * @file MP4Demux.h
 * @brief MP4 Demultiplexer for AAMP
 * 
 * This file contains the MP4 demuxer class that provides functionality to parse
 * MP4 containers, extract media samples, and handle DRM-protected content.
 */

#ifndef __MP4_DEMUX_H__
#define __MP4_DEMUX_H__

#include <cstdint>
#include <stddef.h>
#include <vector>
#include <string>
#include "AampLogManager.h"
#include "AampDemuxDataTypes.h" // for AampMediaSample
#include "DemuxDataTypes.h" // for MediaCodecInfo, MediaProtectionInfo

/**
 * @brief Convert multi-character constants like 'cenc' to equivalent 32 bit integer
 * @param TEXT Four character string to convert
 */
#define MultiChar_Constant(TEXT) ( \
(static_cast<uint32_t>(TEXT[0]) << 0x18) | \
(static_cast<uint32_t>(TEXT[1]) << 0x10) | \
(static_cast<uint32_t>(TEXT[2]) << 0x08) | \
(static_cast<uint32_t>(TEXT[3]) << 0x00) )

#define FourCCToString(FOURCC) ( \
	std::string( \
		{ static_cast<char>((FOURCC >> 24) & 0xFF), \
		  static_cast<char>((FOURCC >> 16) & 0xFF), \
		  static_cast<char>((FOURCC >> 8) & 0xFF), \
		  static_cast<char>(FOURCC & 0xFF) } ) )

/**
 * @brief MP4 logging levels
 */
enum mp4LogLevel
{
	MP4_LOG_LEVEL_NONE = 0,      /**< No logging */
	MP4_LOG_LEVEL_ERROR = 1,     /**< Error level */
	MP4_LOG_LEVEL_WARNING = 2,   /**< Warning level */
	MP4_LOG_LEVEL_INFO = 3,      /**< Info level */
	MP4_LOG_LEVEL_DEBUG = 4,     /**< Debug level */
	MP4_LOG_LEVEL_VERBOSE = 5    /**< Verbose level */
};

/**
 * @brief MP4 logger macro
 */
#define MP4_LOG_WARN(...) AAMPLOG_WARN(__VA_ARGS__)
#define MP4_LOG_ERR(...) AAMPLOG_ERR(__VA_ARGS__)
#define MP4_LOG_INFO(...) AAMPLOG_INFO(__VA_ARGS__)
#define MP4_LOG_DEBUG(...) AAMPLOG_DEBUG(__VA_ARGS__)

/**
 * Enum for MP4 parsing errors
 */
enum Mp4ParseError
{
	MP4_PARSE_OK = 0,                              /**< No error */
	MP4_PARSE_ERROR_INVALID_BOX,                   /**< Invalid box encountered */
	MP4_PARSE_ERROR_INVALID_CONSTANT_IV_SIZE,      /**< Invalid constant IV size */
	MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH,         /**< Sample count mismatch */
	MP4_PARSE_ERROR_UNSUPPORTED_ENCRYPTION_SCHEME, /**< Invalid auxiliary info type */
	MP4_PARSE_ERROR_MISSING_DATA_OFFSET,           /**< Missing data offset in TRUN */
	MP4_PARSE_ERROR_INVALID_PADDING,               /**< Invalid padding value */
	MP4_PARSE_ERROR_UNSUPPORTED_SAMPLE_ENTRY_COUNT,/**< Unsupported sample entry count */
	MP4_PARSE_ERROR_UNSUPPORTED_STREAM_FORMAT,     /**< Unsupported stream format */
	MP4_PARSE_ERROR_INVALID_ESDS_TAG,              /**< Invalid ESDS tag */
	MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH,        /**< Data boundary mismatch */
	MP4_PARSE_ERROR_INVALID_INPUT                 /**< Invalid input to parse function */
};

/**
 * @brief Utility function to get stream output format from FourCC code
 * @param fourCC Four character code identifier
 * @return GstStreamOutputFormat corresponding to the FourCC
 */
GstStreamOutputFormat GetGstStreamOutputFormatFromFourCC(const uint32_t fourCC);

/**
 * @brief Utility function to get cipher type from FourCC code
 * @param fourCC Four character code identifier
 * @return CipherType corresponding to the FourCC
 */
CipherType GetCipherTypeFromFourCC(const uint32_t fourCC);

/**
 * @brief MP4 Demultiplexer class
 * 
 * This class provides functionality to parse MP4 containers, extract media samples,
 * handle encryption metadata, and support various codec types including H.264, HEVC,
 * AAC, AC3, EC3, and AC4.
 */
class Mp4Demux
{
private:
	// Stream format and configuration
	uint32_t streamFormat;                         /**< Stream format identifier */
	
	// Encryption parameters
	uint8_t ivSize;                               /**< Initialization vector size */
	uint8_t cryptByteBlock;                       /**< Encrypted byte block count */
	uint8_t skipByteBlock;                        /**< Skipped byte block count */
	uint8_t constantIvSize;                       /**< Constant IV size */
	std::vector<uint8_t> constantIv;              /**< Constant initialization vector */
	
	// Media timing and samples
	uint32_t timeScale;                           /**< Media timescale */
	std::vector<AampMediaSample> samples;         /**< Parsed media samples */
	
	// Encryption-specific data
	std::vector<uint8_t> defaultKid;              /**< Default key identifier */
	bool gotAuxiliaryInformationOffset;           /**< Flag for auxiliary info offset */
	uint64_t auxiliaryInformationOffset;          /**< Auxiliary information offset */
	CipherType schemeType;                        /**< Encryption scheme ('cenc' or 'cbcs') */
	uint32_t originalMediaType;                   /**< Original media type before encryption */
	std::vector<uint8_t> cencAuxInfoSizes;        /**< CENC auxiliary info sizes */
	std::vector<MediaProtectionInfo> protectionData;     /**< DRM protection system data */

	// Parser state
	const uint8_t *moofPtr;                       /**< Base address for sample data */
	const uint8_t *ptr;                           /**< Current parser position */
	
	// Box header fields
	uint8_t version;                              /**< Box version */
	uint32_t flags;                               /**< Box flags */
	
	// Track fragment fields
	uint64_t baseMediaDecodeTime;                 /**< Base media decode time */
	uint32_t trackId;                             /**< Track identifier */
	uint64_t baseDataOffset;                      /**< Base data offset */
	uint32_t defaultSampleDescriptionIndex;       /**< Default sample description index */
	uint32_t defaultSampleDuration;               /**< Default sample duration */
	uint32_t defaultSampleSize;                   /**< Default sample size */
	uint32_t defaultSampleFlags;                  /**< Default sample flags */
	
	// Track header fields
	uint64_t duration;                            /**< Track duration */
	
	// Sample processing
	uint64_t sampleOffset;                        /**< Current sample offset */
	bool sencPresent;                             /**< SENC box present flag */
	bool handledEncryptedSamples;                 /**< Flag indicating encrypted samples have been handled */
	MediaCodecInfo codecInfo;                     /**< Codec information */
	Mp4ParseError parseError;                     /**< Current parse error state */

	/**
	 * @brief Read n bytes from current position in big-endian format
	 * @param n Number of bytes to read
	 * @return Value read as uint64_t
	 */
	uint64_t ReadBytes(int n);
	
	/**
	 * @brief Read 16-bit unsigned integer in big-endian format
	 * @return 16-bit unsigned integer value
	 */
	uint16_t ReadU16();
	
	/**
	 * @brief Read 32-bit unsigned integer in big-endian format
	 * @return 32-bit unsigned integer value
	 */
	uint32_t ReadU32();
	
	/**
	 * @brief Read 32-bit signed integer in big-endian format
	 * @return 32-bit signed integer value
	 */
	int32_t ReadI32();
	
	/**
	 * @brief Read 64-bit unsigned integer in big-endian format
	 * @return 64-bit unsigned integer value
	 */
	uint64_t ReadU64();
	
	/**
	 * @brief Read box header (version and flags)
	 */
	void ReadHeader();
	
	/**
	 * @brief Skip specified number of bytes
	 * @param len Number of bytes to skip
	 */
	void SkipBytes(size_t len);
	
	/**
	 * @brief Parse original format box for encrypted media
	 */
	void ParseOriginalFormat();
	
	/**
	 * @brief Parse scheme management box for DRM information
	 */
	void ParseSchemeManagementBox();
	
	/**
	 * @brief Parse track encryption box
	 */
	void ParseTrackEncryptionBox();
	
	/**
	 * @brief Parse protection system specific header box (PSSH)
	 * @param next Pointer to next box
	 */
	void ParseProtectionSystemSpecificHeaderBox(const uint8_t *next);
	
	/**
	 * @brief Process auxiliary information for encrypted samples
	 */
	void ProcessAuxiliaryInformation();
	
	/**
	 * @brief Parse sample auxiliary information sizes box
	 */
	void ParseSampleAuxiliaryInformationSizes();
	
	/**
	 * @brief Parse protection scheme information
	 */
	void ParseProtectionSchemeInfo();
	
	/**
	 * @brief Parse sample auxiliary information offsets box
	 */
	void ParseSampleAuxiliaryInformationOffsets();
	
	/**
	 * @brief Parse sample encryption box (SENC)
	 */
	void ParseSampleEncryption();
	
	/**
	 * @brief Parse track run box (TRUN)
	 */
	void ParseTrackRun();
	
	/**
	 * @brief Parse track fragment header box (TFHD)
	 */
	void ParseTrackFragmentHeader();
	
	/**
	 * @brief Parse track fragment decode time box (TFDT)
	 */
	void ParseTrackFragmentDecodeTime();
	
	/**
	 * @brief Parse video sample entry box
	 */
	void ParseVideoSampleEntry();
	
	/**
	 * @brief Parse audio sample entry box
	 */
	void ParseAudioSampleEntry();
	
	/**
	 * @brief Parse video information from sample entry
	 */
	void ParseVideoInformation();
	
	/**
	 * @brief Parse audio information from sample entry
	 */
	void ParseAudioInformation();
	
	/**
	 * @brief Parse movie header box (MVHD)
	 */
	void ParseMovieHeader();
	
	/**
	 * @brief Parse track header box (TKHD)
	 */
	void ParseTrackHeader();
	
	/**
	 * @brief Parse media header box (MDHD)
	 */
	void ParseMediaHeader();
	
	/**
	 * @brief Parse handler reference box (HDLR)
	 */
	void ParseHandlerReference();
	
	/**
	 * @brief Parse stream format box
	 * @param type FourCC type
	 * @param next Pointer to next box
	 */
	void ParseStreamFormatBox(uint32_t type, const uint8_t *next);
	
	/**
	 * @brief Read length field with variable encoding
	 * @return Length value
	 */
	int ReadLen();
	
	/**
	 * @brief Parse codec configuration helper
	 * @param next Pointer to end of data
	 */
	void ParseEsdsCodecConfigHelper(const uint8_t *next);
	
	/**
	 * @brief Parse codec configuration box
	 * @param type FourCC type
	 * @param next Pointer to next box
	 */
	void ParseCodecConfigurationBox(uint32_t type, const uint8_t *next);
	
	/**
	 * @brief Parse movie extends header box
	 */
	void ParseMovieExtendsHeader();
	
	/**
	 * @brief Parse track extends box
	 */
	void ParseTrackExtendsBox();
	
	/**
	 * @brief Parse sample description box
	 * @param next Pointer to next box
	 */
	void ParseSampleDescriptionBox(const uint8_t *next);
	
	/**
	 * @brief Main demux helper function
	 * @param end Pointer to end of data
	 */
	void DemuxHelper(const uint8_t *end);

public:
	/**
	 * @brief Constructor
	 */
	Mp4Demux();
	
	/**
	 * @brief Destructor
	 */
	~Mp4Demux();
	
	/**
	 * @brief Copy constructor (deleted)
	 */
	Mp4Demux(const Mp4Demux & other) = delete;
	
	/**
	 * @brief Assignment operator (deleted)
	 */
	Mp4Demux& operator=(const Mp4Demux & other) = delete;

	/**
	 * @brief Parse MP4 data
	 * @param ptr Pointer to MP4 data
	 * @param len Length of data
	 * @return true if parsing succeeded, false on error
	 */
	bool Parse(const void *ptr, size_t len);

	/**
	 * @brief Get last parser error
	 * @return Mp4ParseError indicating the last error that occurred
	 */
	Mp4ParseError GetLastError() const;
	
	/**
	 * @brief Get timescale value
	 * @return Media timescale
	 */
	uint32_t GetTimeScale() const;

	/**
	 * @brief Get codec information
	 * Returns comprehensive codec information including format,
	 * media type, and codec-specific parameters extracted from
	 * the MP4 sample description.
	 * 
	 * @return Codec information with ownership transferred to caller
	 */
	MediaCodecInfo GetCodecInfo();

	/**
	 * @brief Get protection system data
	 * Returns all PSSH (Protection System Specific Header) data
	 * extracted from the MP4 container for DRM license acquisition.
	 * 
	 * @return Protection data vector with ownership transferred to caller
	 */
	std::vector<MediaProtectionInfo> GetProtectionEvents();

	/**
	 * @brief Get parsed media samples
	 * Returns all media samples extracted from the current MP4 fragment,
	 * including sample data, timing information, and encryption metadata.
	 * 
	 * @return Media samples vector with ownership transferred to caller
	 */
	std::vector<AampMediaSample> GetSamples();
};

#endif /* __MP4_DEMUX_H__ */