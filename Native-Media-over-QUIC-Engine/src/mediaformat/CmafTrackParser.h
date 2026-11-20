#pragma once

#include "common/ReceivedObject.h"
#include "media/api/MediaPipeline.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Mp4Demux;

namespace nmqe {

/** Producer Reference Time box ('prft', ISO/IEC 14496-12 8.16.5). */
struct ProducerReferenceTime
{
	uint32_t referenceTrackId = 0;
	uint64_t ntpTimestamp = 0;      // 32.32 fixed point seconds since 1900
	uint64_t mediaTime = 0;         // in the reference track's timescale
	uint32_t flags = 0;
	int64_t wallMs() const;         // ntpTimestamp as Unix epoch milliseconds
};

/** Timing and access units of one CMAF fragment (one or more moof+mdat). */
struct CmafFragment
{
	std::vector<media::MediaSegment> segments;
	uint64_t baseMediaDecodeTime = 0;  // tfdt, track timescale
	int64_t startPtsNs = 0;            // earliest presentation time
	int64_t endPtsNs = 0;              // latest presentation time + duration
	bool startsWithKeyFrame = false;
	size_t keyFrames = 0;
	bool hadInit = false;              // the Object also carried ftyp/moov
	std::optional<ProducerReferenceTime> prft;
};

/**
 * Parses one track's CMAF header and fragments with AAMP's Mp4Demux, and
 * produces Media Pipeline configuration and access units. Times are derived
 * from integer track ticks (tfdt + sample durations, composition offsets).
 */
class CmafTrackParser
{
public:
	CmafTrackParser(std::string trackName, media::MediaSourceType type);
	~CmafTrackParser();
	CmafTrackParser(const CmafTrackParser &) = delete;
	CmafTrackParser &operator=(const CmafTrackParser &) = delete;

	/** Parses a CMAF header (ftyp+moov). */
	bool setInit(const std::vector<uint8_t> &init, std::string &error);
	bool hasInit() const { return mHasInit; }

	/** The source configuration derived from the header. */
	const media::MediaSource &source() const { return mSource; }
	/** Records the pipeline source ID that access units are tagged with. */
	void setSourceId(int32_t id) { mSource.id = id; }
	uint32_t timescale() const { return mTimescale; }
	/** A stable string describing the codec configuration, to detect changes. */
	const std::string &codecSignature() const { return mCodecSignature; }

	/** Parses one Object's payload (moof+mdat, optionally preceded by ftyp+moov). */
	bool parseFragment(const std::shared_ptr<const ByteBuffer> &payload, CmafFragment &out, std::string &error);

	/** Top-level box types of a payload, for diagnostics (e.g. "moof,mdat"). */
	static std::string boxSummary(const uint8_t *data, size_t size);
	/** Finds the first top-level 'prft' box. */
	static std::optional<ProducerReferenceTime> findPrft(const uint8_t *data, size_t size);
	/** Whether a payload carries a CMAF header ('ftyp' or 'moov' at top level). */
	static bool hasHeader(const uint8_t *data, size_t size);

private:
	void updateSource();
	int64_t ticksToNs(int64_t ticks) const;

	std::string mTrack;
	media::MediaSourceType mType;
	std::unique_ptr<Mp4Demux> mDemux;
	media::MediaSource mSource;
	uint32_t mTimescale = 0;
	bool mHasInit = false;
	std::string mCodecSignature;
};

} // namespace nmqe
