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
#include <stdexcept>
#include <string>
#include <chrono>
#include "AampLogManager.h"
#include "AampDemuxDataTypes.h" // for AampMediaSample
#include "DemuxDataTypes.h"     // for MediaCodecInfo, MediaProtectionInfo

/**
 * @brief Convert multi-character constants like 'cenc' to equivalent 32 bit integer
 * @param TEXT Four character string to convert
 */
#define MultiChar_Constant(TEXT) ( \
 (static_cast<uint32_t>(TEXT[0]) << 0x18) \
 + (static_cast<uint32_t>(TEXT[1]) << 0x10) \
 + (static_cast<uint32_t>(TEXT[2]) << 0x08) \
 + (static_cast<uint32_t>(TEXT[3]) << 0x00) )
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
	MP4_LOG_LEVEL_NONE = 0, /**< No logging */
	MP4_LOG_LEVEL_ERROR = 1, /**< Error level */
	MP4_LOG_LEVEL_WARNING = 2, /**< Warning level */
	MP4_LOG_LEVEL_INFO = 3, /**< Info level */
	MP4_LOG_LEVEL_DEBUG = 4, /**< Debug level */
	MP4_LOG_LEVEL_VERBOSE = 5 /**< Verbose level */
};
/**
 * @brief MP4 logger macro
 */
#define MP4_LOG_WARN(...)  AAMPLOG_WARN(__VA_ARGS__)
#define MP4_LOG_ERR(...)   AAMPLOG_ERR(__VA_ARGS__)
#define MP4_LOG_INFO(...)  AAMPLOG_INFO(__VA_ARGS__)
#define MP4_LOG_DEBUG(...) AAMPLOG_DEBUG(__VA_ARGS__)

/**
 * Enum for MP4 parsing errors
 */
enum Mp4ParseError
{
	MP4_PARSE_OK = 0, /**< No error */
	MP4_PARSE_ERROR_INVALID_BOX, /**< Invalid box encountered */
	MP4_PARSE_ERROR_INVALID_IV_SIZE, /**< Invalid IV size (expected 8 or 16) */
	MP4_PARSE_ERROR_SAMPLE_COUNT_MISMATCH, /**< Explicit sample count doesn't match implicit sample count */
	MP4_PARSE_ERROR_UNSUPPORTED_ENCRYPTION_SCHEME, /**< Expected cenc or cbcs */
	MP4_PARSE_ERROR_INVALID_PADDING, /**< Unexpected Video Padding field (should be 0xffff) */
	MP4_PARSE_ERROR_UNSUPPORTED_SAMPLE_ENTRY_COUNT,/**< Zero sample entry count */
	MP4_PARSE_ERROR_UNSUPPORTED_STREAM_FORMAT, /**< Unsupported stream format */
	MP4_PARSE_ERROR_INVALID_ESDS_TAG, /**< Invalid ESDS tag */
	MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH, /**< Data boundary mismatch - referencing invalid memory */
	MP4_PARSE_ERROR_INVALID_INPUT, /**< Invalid input to parse function; nullptr or zero length */
	MP4_PARSE_ERROR_INVALID_KID, /**< Invalid (huge) kidCount */
	MP4_PARSE_ERROR_INVALID_ENTRY_COUNT, /**< Entry count is zero */
	MP4_PARSE_ERROR_VARIABLE_LENGTH_OVERFLOW, /**< Value encoded using octets exceed 32 bits */
	MP4_PARSE_ERROR_UNEXPECTED_IS_ENCRYPTED_FIELD /**< is_encrypted not 0 or 1 */
};

/**
 * @brief Domain exception for MP4 parsing errors.
 *
 * Thrown by low-level helpers (ReadBytes/SkipBytes/ReadLen/ReadHeader) and
 * caught at the public Parse(...) boundary to preserve a non-throwing facade.
 */
class Mp4ParseException : public std::runtime_error
{
public:
	explicit Mp4ParseException(Mp4ParseError code, const char* what)
	  : std::runtime_error(what), code_(code) {}
	Mp4ParseError code() const noexcept { return code_; }
private:
	Mp4ParseError code_;
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
	/**
	 * @struct MetricStats
	 * @brief Statistics accumulator for demux performance metrics
	 */
	struct MetricStats
	{
		uint64_t count;  /**< Number of Observations */
		double sum;      /**< Sum of all values */
		double min;      /**< Minimum value */
		double max;      /**< Maximum value */

		MetricStats() : count(0), sum(0.0), min(0.0), max(0.0) {}

		void Update(double value)
		{
			if (count == 0)
			{
				min = value;
				max = value;
			}
			else
			{
				if (value < min) min = value;
				if (value > max) max = value;
			}
			sum += value;
			++count;
		}

		double GetAverage() const
		{
			return (count > 0) ? (sum / count) : 0.0;
		}

		bool HasData() const
		{
			return count > 0;
		}

		void Reset()
		{
			count = 0;
			sum = 0.0;
			min = 0.0;
			max = 0.0;
		}
	};

	// Stream format and configuration
	uint32_t streamFormat; /**< Stream format identifier */
	const char* mMediaTypeName; /**< Media type name for logging */
	// Encryption parameters
	uint8_t ivSize; /**< Initialization vector size */
	uint8_t cryptByteBlock; /**< Encrypted byte block count */
	uint8_t skipByteBlock; /**< Skipped byte block count */
	uint8_t constantIvSize; /**< Constant IV size */
	std::vector<uint8_t> constantIv; /**< Constant initialization vector */
	// Media timing and samples
	uint32_t timeScale; /**< Media timescale */
	std::vector<AampMediaSample> samples; /**< Parsed media samples */
	// Encryption-specific data
	std::vector<uint8_t> defaultKid; /**< Default key identifier */
	bool gotAuxiliaryInformationOffset; /**< Flag for auxiliary info offset */
	uint64_t auxiliaryInformationOffset; /**< Auxiliary information offset */
	CipherType schemeType; /**< Encryption scheme ('cenc' or 'cbcs') */
	uint32_t originalMediaType; /**< Original media type before encryption */
	std::vector<uint8_t> cencAuxInfoSizes; /**< CENC auxiliary info sizes */
	std::vector<MediaProtectionInfo> protectionData; /**< DRM protection system data */
	// Parser state
	const uint8_t *moofPtr; /**< Base address for sample data */
	const uint8_t *ptr; /**< Current parser position */
	const uint8_t *endPtr; /**< Absolute end boundary of the current parse buffer */
	// MDAT range tracking (for sample data validation)
	const uint8_t *mdatStart; /**< Pointer to first byte of payload of current/most recent 'mdat' */
	const uint8_t *mdatEnd;   /**< Pointer to one past the last byte of payload of current/most recent 'mdat' */
	// Box header fields
	uint8_t version; /**< Box version */
	uint32_t flags; /**< Box flags */
	// Track fragment fields
	uint64_t baseMediaDecodeTime; /**< Base media decode time */
	uint32_t trackId; /**< Track identifier */
	uint64_t baseDataOffset; /**< Base data offset */
	uint32_t defaultSampleDescriptionIndex; /**< Default sample description index */
	uint32_t defaultSampleDuration; /**< Default sample duration */
	uint32_t defaultSampleSize; /**< Default sample size */
	uint32_t defaultSampleFlags; /**< Default sample flags */
	// Track header fields
	uint64_t duration; /**< Track duration */
	// Sample processing
	uint64_t sampleOffset; /**< Current sample offset */
	bool sencPresent; /**< SENC box present flag */
	bool handledEncryptedSamples; /**< Flag indicating encrypted samples have been handled */

	/**
	 * @brief Pending sample payload data.
	 */
	struct PendingSamplePayload
	{
		const uint8_t* dataPtr; /**< Pointer into the parse buffer where sample data starts */
		uint32_t sampleLen;     /**< Byte length of this sample's payload */
		size_t sampleIdx;       /**< Index into samples[] where data must be placed */
		double mDts;            /**< Decode timestamp in seconds */
		double mPts;            /**< Presentation timestamp in seconds */
		double mDuration;       /**< Sample duration in seconds */
		bool mIsKeyFrame{false}; /**< True if this is a sync/key frame (I-frame) */
	};
	std::vector<PendingSamplePayload> mSampleInfo; /**< sample payloads awaiting mdat bounds */
	MediaCodecInfo codecInfo; /**< Codec information */
	Mp4ParseError parseError; /**< Current parse error state */

	// Performance metrics tracking
	MetricStats mDemuxTimeMs;        /**< Demux time statistics in milliseconds */
	MetricStats mFramesPerSecond;    /**< Frames per second statistics */
	std::chrono::steady_clock::time_point mLastLogTime; /**< Last metrics log timestamp */
	std::chrono::seconds mLogIntervalSeconds; /**< Logging interval in seconds */

	/** Temporary hold on the segment shared_ptr during Parse(shared_ptr); cleared before returning. */
	std::shared_ptr<std::vector<uint8_t>> mCurrentSegment{};

	/**
	 * @brief log human readable parse error and update state
	 * @param parseError one of Mp4ParseError
	 * @param what optional error detail string (e.g. from exception)
	 *
	 * Note: still used from the Parse(...) catch block to centralize logging.
	 */
	void setParseError( Mp4ParseError, const char* what = nullptr );

	/**
	 * @brief Read n bytes from current position in big-endian format
	 * @param n Number of bytes to read
	 * @return Value read as uint64_t (throws Mp4ParseException on error)
	 */
	uint64_t ReadBytes(int n);

	/**
	 * @brief Read 16-bit unsigned integer in big-endian format
	 * @return 16-bit unsigned integer value (throws on error)
	 */
	uint16_t ReadU16();

	/**
	 * @brief Read 32-bit unsigned integer in big-endian format
	 * @return 32-bit unsigned integer value (throws on error)
	 */
	uint32_t ReadU32();

	/**
	 * @brief Read 32-bit signed integer in big-endian format
	 * @return 32-bit signed integer value (throws on error)
	 */
	int32_t ReadI32();

	/**
	 * @brief Read 64-bit unsigned integer in big-endian format
	 * @return 64-bit unsigned integer value (throws on error)
	 */
	uint64_t ReadU64();

	/**
	 * @brief Read box header (version and flags)
	 * @throws Mp4ParseException on out-of-bounds
	 */
	void ReadHeader();

	/**
	 * @brief Skip specified number of bytes
	 * @param len Number of bytes to skip
	 * @throws Mp4ParseException on out-of-bounds
	 */
	void SkipBytes(size_t len);

	/** @brief Parse original format box for encrypted media */
	void ParseOriginalFormat();
	/** @brief Parse scheme management box for DRM information */
	void ParseSchemeManagementBox();
	/** @brief Parse track encryption box */
	void ParseTrackEncryptionBox();
	/** @brief Parse protection system specific header box (PSSH)
	 * @param next Pointer to next box
	 */
	void ParseProtectionSystemSpecificHeaderBox(const uint8_t *next);
	/** @brief Process auxiliary information for encrypted samples */
	void ProcessAuxiliaryInformation();
	/** @brief Parse sample auxiliary information sizes box */
	void ParseSampleAuxiliaryInformationSizes();
	/** @brief Parse protection scheme information */
	void ParseProtectionSchemeInfo();
	/** @brief Parse sample auxiliary information offsets box */
	void ParseSampleAuxiliaryInformationOffsets();
	/** @brief Parse sample encryption box (SENC)
	 * @param next Pointer to next box
	 */
	void ParseSampleEncryption(const uint8_t *next);
	/** @brief Parse track run box (TRUN) */
	void ParseTrackRun();
	/**
	 * @brief Assign deferred sample payload data after mdat is parsed
	 */
	void ProcessSamples();
	/** @brief Parse track fragment header box (TFHD) */
	void ParseTrackFragmentHeader();
	/** @brief Parse track fragment decode time box (TFDT) */
	void ParseTrackFragmentDecodeTime();
	/** @brief Parse video sample entry box */
	void ParseVideoSampleEntry();
	/** @brief Parse audio sample entry box */
	void ParseAudioSampleEntry();
	/** @brief Parse video information from sample entry */
	void ParseVideoInformation();
	/** @brief Parse audio information from sample entry */
	void ParseAudioInformation();
	/** @brief Parse movie header box (MVHD) */
	void ParseMovieHeader();
	/** @brief Parse track header box (TKHD) */
	void ParseTrackHeader();
	/** @brief Parse media header box (MDHD) */
	void ParseMediaHeader();
	/** @brief Parse handler reference box (HDLR) */
	void ParseHandlerReference();
	/** @brief Parse stream format box
	 * @param type FourCC type
	 * @param next Pointer to next box
	 */
	void ParseStreamFormatBox(uint32_t type, const uint8_t *next);
	/** @brief Read length field with variable encoding
	 * @return Length value
	 */
	uint32_t ReadLen();
	/** @brief Parse codec configuration helper
	 * @param next Pointer to end of data
	 */
	void ParseEsdsCodecConfigHelper(const uint8_t *next);
	/** @brief Parse codec configuration box
	 * @param type FourCC type
	 * @param next Pointer to next box
	 */
	void ParseCodecConfigurationBox(uint32_t type, const uint8_t *next);
	/** @brief Parse meta box (QTFF or ISO BMFF variant)
	 * @param next Pointer to end of box payload
	 */
	void ParseMetaBox(const uint8_t *next);
	/** @brief Parse sample group description box (SGPD)
	 * @param next Pointer to end of box payload
	 */
	void ParseSampleGroupDescription(const uint8_t *next);
	/** @brief Parse sample to group box (SBGP)
	 * @param next Pointer to end of box payload
	 */
	void ParseSampleToGroup(const uint8_t *next);
	/** @brief Parse movie extends header box */
	void ParseMovieExtendsHeader();
	/** @brief Parse track extends box */
	void ParseTrackExtendsBox();
	/** @brief Parse sample description box
	 * @param next Pointer to next box
	 */
	void ParseSampleDescriptionBox(const uint8_t *next);
	/** @brief Main demux helper function
	 * @param end Pointer to end of data
	 */
	void DemuxHelper(const uint8_t *end);
public:
	/**
	 * @brief Record metrics for a demux operation
	 * @param demuxTimeMs Time taken to demux in milliseconds
	 * @param fps Frames per second processed during demux
	 */
	void RecordDemuxMetrics(double demuxTimeMs, double fps);

	/**
	 * @brief Log accumulated metrics
	 */
	void LogMetrics();

	/**
	 * @brief Check if metrics should be logged based on time interval
	 * @return true if 10 minutes have elapsed
	 */
	bool ShouldLogMetrics() const;

public:
	/** @brief Constructor */
	Mp4Demux();
	/** @brief Destructor */
	~Mp4Demux();
	/** @brief Copy constructor (deleted) */
	Mp4Demux(const Mp4Demux & other) = delete;
	/** @brief Assignment operator (deleted) */
	Mp4Demux& operator=(const Mp4Demux & other) = delete;
	/**
	 * @brief Parse MP4 data with shared ownership of the backing buffer.
	 *
	 * Stores @p segment internally during parsing. Each extracted sample's
	 * mData field is set via an aliasing shared_ptr that shares the segment's
	 * reference count while pointing directly at the sample payload bytes.
	 * The internal reference is released before returning; only the individual
	 * samples hold a reference thereafter.  Callers do not need to track the
	 * segment after this call.
	 *
	 * @param segment Shared ownership of the buffer to parse (must not be null).
	 * @return true if parsing succeeded, false on error
	 */
	bool Parse(std::shared_ptr<std::vector<uint8_t>>&& segment);

	/**
	 * @brief Throwing variant with shared ownership of the backing buffer.
	 * Delegates to Parse(shared_ptr) and throws on failure.
	 * @param segment Shared ownership of the buffer to parse (must not be null).
	 * @throws Mp4ParseException on parse failure
	 */
	void ParseOrThrow(std::shared_ptr<std::vector<uint8_t>>&& segment);

	/** @brief Get last parser error
	 * @return Mp4ParseError indicating the last error that occurred
	 */
	Mp4ParseError GetLastError() const;
	/** @brief Get timescale value
	 * @return Media timescale
	 */
	uint32_t GetTimeScale() const;
	/** @brief Get codec information
	 * @return Codec information with ownership transferred to caller
	 */
	MediaCodecInfo GetCodecInfo();
	/** @brief Get protection system data
	 * @return Protection data vector with ownership transferred to caller
	 */
	std::vector<MediaProtectionInfo> GetProtectionEvents();

	/**
	 * @brief Return parsed media samples.
	 *
	 * Each sample's mData is an aliasing shared_ptr<const uint8_t> that keeps
	 * the backing segment buffer alive for the sample's lifetime, enabling
	 * zero-copy access to the parsed payload.
	 *
	 * @return Media samples vector with ownership transferred to caller
	 */
	std::vector<AampMediaSample> GetSamples();
};
#endif /* __MP4_DEMUX_H__ */
