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
#include "AampDemuxDataTypes.h" // for AampCodecInfo, AampPsshData, AampMediaSample

/**
 * @brief Convert multi-character constants like 'cenc' to equivalent 32 bit integer
 * @param TEXT Four character string to convert
 */
#define MultiChar_Constant(TEXT) ( \
(static_cast<uint32_t>(TEXT[0]) << 0x18) | \
(static_cast<uint32_t>(TEXT[1]) << 0x10) | \
(static_cast<uint32_t>(TEXT[2]) << 0x08) | \
(static_cast<uint32_t>(TEXT[3]) << 0x00) )

/**
 * @brief MP4 logging levels
 */
enum mp4LogLevel
{
	MP4_LOG_NONE = 0,      /**< No logging */
	MP4_LOG_ERROR = 1,     /**< Error level */
	MP4_LOG_WARNING = 2,   /**< Warning level */
	MP4_LOG_INFO = 3,      /**< Info level */
	MP4_LOG_DEBUG = 4,     /**< Debug level */
	MP4_LOG_VERBOSE = 5    /**< Verbose level */
};

/**
 * @brief MP4 logger macro
 */
#define MP4_LOGGER(level, ...) AAMPLOG_WARN(__VA_ARGS__)

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
	uint32_t stream_format;                         /**< Stream format identifier */
	uint32_t data_reference_index;                  /**< Data reference index */
	
	// Encryption parameters
	uint8_t iv_size;                               /**< Initialization vector size */
	uint8_t crypt_byte_block;                      /**< Encrypted byte block count */
	uint8_t skip_byte_block;                       /**< Skipped byte block count */
	uint8_t constant_iv_size;                      /**< Constant IV size */
	std::vector<uint8_t> constant_iv;              /**< Constant initialization vector */
	
	// Media timing and samples
	uint32_t timescale;                            /**< Media timescale */
	std::vector<AampMediaSample> samples;          /**< Parsed media samples */
	
	// Encryption-specific data
	std::string default_kid;                       /**< Default key identifier */
	bool got_auxiliary_information_offset;         /**< Flag for auxiliary info offset */
	uint64_t auxiliary_information_offset;         /**< Auxiliary information offset */
	uint32_t scheme_type;                          /**< Encryption scheme ('cenc' or 'cbcs') */
	uint32_t scheme_version;                       /**< Encryption scheme version */
	uint32_t original_media_type;                  /**< Original media type before encryption */
	std::vector<uint8_t> cenc_aux_info_sizes;      /**< CENC auxiliary info sizes */
	std::vector<AampPsshData> protectionData;      /**< DRM protection system data */
	AampCodecInfo mCodecInfo;                      /**< Codec information */

	// Parser state
	const uint8_t *moof_ptr;                       /**< Base address for sample data */
	const uint8_t *ptr;                           /**< Current parser position */
	
	// Box header fields
	uint8_t version;                               /**< Box version */
	uint32_t flags;                               /**< Box flags */
	
	// Track fragment fields
	uint64_t baseMediaDecodeTime;                  /**< Base media decode time */
	uint32_t fragment_duration;                    /**< Fragment duration */
	uint32_t track_id;                            /**< Track identifier */
	uint64_t base_data_offset;                    /**< Base data offset */
	uint32_t default_sample_description_index;     /**< Default sample description index */
	uint32_t default_sample_duration;             /**< Default sample duration */
	uint32_t default_sample_size;                 /**< Default sample size */
	uint32_t default_sample_flags;                /**< Default sample flags */
	
	// Track header fields
	uint64_t creation_time;                       /**< Track creation time */
	uint64_t modification_time;                   /**< Track modification time */
	uint32_t duration;                           /**< Track duration */
	uint32_t rate;                              /**< Playback rate */
	uint32_t volume;                            /**< Audio volume */
	int32_t matrix[9];                          /**< Transformation matrix */
	uint16_t layer;                             /**< Visual layer */
	uint16_t alternate_group;                   /**< Alternate group */
	uint32_t width_fixed;                       /**< Video width (fixed point) */
	uint32_t height_fixed;                      /**< Video height (fixed point) */
	uint16_t language;                          /**< Language code */
	
	// Sample processing
	uint64_t sampleOffset;                       /**< Current sample offset */
	bool sencPresent;                           /**< SENC box present flag */
	bool verbose;                               /**< Verbose logging flag */

	/**
	 * @brief Get stream output format from FourCC code
	 * @param fourCC Four character code identifier
	 * @return StreamOutputFormat corresponding to the FourCC
	 */
	StreamOutputFormat GetStreamOutputFormatFromFourCC(const uint32_t fourCC);

	/**
	 * @brief Get media type for stream output format
	 * @param format Stream output format
	 * @return AampMediaType corresponding to the format
	 */
	AampMediaType GetMediaTypeForStreamOutputFormat(const StreamOutputFormat format);

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
	void parseOriginalFormat();
	
	/**
	 * @brief Parse scheme management box for DRM information
	 */
	void parseSchemeManagementBox();
	
	/**
	 * @brief Parse track encryption box
	 */
	void parseTrackEncryptionBox();
	
	/**
	 * @brief Parse protection system specific header box (PSSH)
	 * @param next Pointer to next box
	 */
	void parseProtectionSystemSpecificHeaderBox(const uint8_t *next);
	
	/**
	 * @brief Process auxiliary information for encrypted samples
	 */
	void process_auxiliary_information();
	
	/**
	 * @brief Parse sample auxiliary information sizes box
	 */
	void parseSampleAuxiliaryInformationSizes();
	
	/**
	 * @brief Parse auxiliary information box
	 */
	void parseAuxInfo();
	
	/**
	 * @brief Parse sample auxiliary information offsets box
	 */
	void parseSampleAuxiliaryInformationOffsets();
	
	/**
	 * @brief Parse sample encryption box (SENC)
	 */
	void parseSampleEncryption();
	
	/**
	 * @brief Parse track run box (TRUN)
	 */
	void parseTrackRun();
	
	/**
	 * @brief Parse track fragment header box (TFHD)
	 */
	void parseTrackFragmentHeader();
	
	/**
	 * @brief Parse track fragment decode time box (TFDT)
	 */
	void parseTrackFragmentDecodeTime();
	
	/**
	 * @brief Parse video sample entry box
	 */
	void parseVideoSampleEntry();
	
	/**
	 * @brief Parse audio sample entry box
	 */
	void parseAudioSampleEntry();
	
	/**
	 * @brief Parse video information from sample entry
	 */
	void parseVideoInformation();
	
	/**
	 * @brief Parse audio information from sample entry
	 */
	void parseAudioInformation();
	
	/**
	 * @brief Parse movie header box (MVHD)
	 */
	void parseMovieHeader();
	
	/**
	 * @brief Parse track header box (TKHD)
	 */
	void parseTrackHeader();
	
	/**
	 * @brief Parse media header box (MDHD)
	 */
	void parseMediaHeader();
	
	/**
	 * @brief Parse handler reference box (HDLR)
	 */
	void parseHandlerReference();
	
	/**
	 * @brief Parse stream format box
	 * @param type FourCC type
	 * @param next Pointer to next box
	 */
	void parseStreamFormatBox(uint32_t type, const uint8_t *next);
	
	/**
	 * @brief Read length field with variable encoding
	 * @return Length value
	 */
	int readLen();
	
	/**
	 * @brief Parse codec configuration helper
	 * @param next Pointer to end of data
	 */
	void parseCodecConfigHelper(const uint8_t *next);
	
	/**
	 * @brief Parse codec configuration box
	 * @param type FourCC type
	 * @param next Pointer to next box
	 */
	void parseCodecConfigurationBox(uint32_t type, const uint8_t *next);
	
	/**
	 * @brief Parse movie fragment header box
	 */
	void parseMovieFragmentHeaderBox();
	
	/**
	 * @brief Parse movie extends header box
	 */
	void parseMovieExtendsHeader();
	
	/**
	 * @brief Parse track extends box
	 */
	void parseTrackExtendsBox();
	
	/**
	 * @brief Parse sample description box
	 * @param next Pointer to next box
	 */
	void parseSampleDescriptionBox(const uint8_t *next);
	
	/**
	 * @brief Main demux helper function
	 * @param end Pointer to end of data
	 */
	void DemuxHelper(const uint8_t *end);

public:
	/**
	 * @brief Constructor
	 * @param verbose Enable verbose logging
	 */
	Mp4Demux(bool verbose = false);
	
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
	 */
	void Parse(const void *ptr, size_t len);
	
	/**
	 * @brief Get timescale value
	 * @return Media timescale
	 */
	uint32_t getTimeScale() const;

	/**
	 * @brief Get codec information
	 * @return Const reference to codec info
	 */
	const AampCodecInfo& getCodecInfo() const;

	/**
	 * @brief Get protection system data
	 * @return Const reference to protection data vector
	 */
	const std::vector<AampPsshData>& getProtectionEvents() const;

	/**
	 * @brief Get parsed media samples
	 * @return Reference to samples vector
	 */
	std::vector<AampMediaSample>& getSamples();
};

#endif /* __MP4_DEMUX_H__ */