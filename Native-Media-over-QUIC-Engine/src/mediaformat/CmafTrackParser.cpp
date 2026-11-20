#include "CmafTrackParser.h"

#include "MP4Demux.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nmqe {

namespace {

uint32_t readU32(const uint8_t *p)
{
	return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
	       (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

uint64_t readU64(const uint8_t *p)
{
	return (static_cast<uint64_t>(readU32(p)) << 32) | readU32(p + 4);
}

/** Calls `visit(type, body, bodySize)` for each top-level box; stops at a malformed box. */
template <typename Visit>
void forEachBox(const uint8_t *data, size_t size, Visit visit)
{
	size_t offset = 0;
	while (offset + 8 <= size)
	{
		uint64_t boxSize = readU32(data + offset);
		size_t header = 8;
		if (boxSize == 1)
		{
			if (offset + 16 > size) return;
			boxSize = readU64(data + offset + 8);
			header = 16;
		}
		else if (boxSize == 0)
		{
			boxSize = size - offset;
		}
		if (boxSize < header || boxSize > size - offset) return;
		const uint32_t type = readU32(data + offset + 4);
		if (!visit(type, data + offset + header, static_cast<size_t>(boxSize - header))) return;
		offset += static_cast<size_t>(boxSize);
	}
}

constexpr uint32_t fourcc(const char (&text)[5])
{
	return (static_cast<uint32_t>(text[0]) << 24) | (static_cast<uint32_t>(text[1]) << 16) |
	       (static_cast<uint32_t>(text[2]) << 8) | static_cast<uint32_t>(text[3]);
}

int64_t secondsToTicks(double seconds, uint32_t timescale)
{
	return std::llround(seconds * static_cast<double>(timescale));
}

std::string hex(const std::vector<uint8_t> &bytes)
{
	static const char kDigits[] = "0123456789abcdef";
	std::string out;
	out.reserve(bytes.size() * 2);
	for (uint8_t b : bytes)
	{
		out += kDigits[b >> 4];
		out += kDigits[b & 0xf];
	}
	return out;
}

} // namespace

int64_t ProducerReferenceTime::wallMs() const
{
	// NTP era 0 starts 1900-01-01; Unix epoch is 2208988800 s later.
	constexpr uint64_t kNtpToUnixSeconds = 2208988800ULL;
	const uint64_t seconds = ntpTimestamp >> 32;
	const uint64_t fraction = ntpTimestamp & 0xffffffffULL;
	if (seconds < kNtpToUnixSeconds) return 0;
	return static_cast<int64_t>((seconds - kNtpToUnixSeconds) * 1000 + ((fraction * 1000) >> 32));
}

CmafTrackParser::CmafTrackParser(std::string trackName, media::MediaSourceType type)
	: mTrack(std::move(trackName)), mType(type), mDemux(std::make_unique<Mp4Demux>())
{
	mSource.type = type;
}

CmafTrackParser::~CmafTrackParser() = default;

std::string CmafTrackParser::boxSummary(const uint8_t *data, size_t size)
{
	std::string out;
	forEachBox(data, size, [&](uint32_t type, const uint8_t *, size_t) {
		if (!out.empty()) out += ',';
		out += FourCCToString(type);
		return true;
	});
	return out;
}

bool CmafTrackParser::hasHeader(const uint8_t *data, size_t size)
{
	bool found = false;
	forEachBox(data, size, [&](uint32_t type, const uint8_t *, size_t) {
		found = type == fourcc("ftyp") || type == fourcc("moov");
		return !found;
	});
	return found;
}

std::optional<ProducerReferenceTime> CmafTrackParser::findPrft(const uint8_t *data, size_t size)
{
	std::optional<ProducerReferenceTime> result;
	forEachBox(data, size, [&](uint32_t type, const uint8_t *body, size_t bodySize) {
		if (type != fourcc("prft")) return true;
		if (bodySize < 4 + 4 + 8 + 4) return false;
		const uint8_t version = body[0];
		ProducerReferenceTime prft;
		prft.flags = readU32(body) & 0xffffff;
		prft.referenceTrackId = readU32(body + 4);
		prft.ntpTimestamp = readU64(body + 8);
		if (version == 1)
		{
			if (bodySize < 4 + 4 + 8 + 8) return false;
			prft.mediaTime = readU64(body + 16);
		}
		else
		{
			prft.mediaTime = readU32(body + 16);
		}
		result = prft;
		return false;
	});
	return result;
}

bool CmafTrackParser::setInit(const std::vector<uint8_t> &init, std::string &error)
{
	if (init.empty())
	{
		error = "empty CMAF header";
		return false;
	}
	if (!mDemux->Parse(std::make_shared<std::vector<uint8_t>>(init)))
	{
		error = "CMAF header parse failed (Mp4ParseError " + std::to_string(mDemux->GetLastError()) + ")";
		return false;
	}
	mTimescale = mDemux->GetTimeScale();
	if (mTimescale == 0)
	{
		error = "CMAF header has no timescale";
		return false;
	}
	// Fragments carry no mdhd; keep the header's timescale for them.
	mDemux->SetFallbackTimeScale(mTimescale);
	updateSource();
	if (mSource.mimeType.empty())
	{
		error = "unsupported codec in CMAF header";
		return false;
	}
	mHasInit = true;
	return true;
}

void CmafTrackParser::updateSource()
{
	MediaCodecInfo info = mDemux->GetCodecInfo();
	media::MediaSource source;
	source.type = mType;
	source.codecData = info.mCodecData;
	source.alignment = media::SegmentAlignment::AU;
	switch (info.mCodecFormat)
	{
		case GST_FORMAT_VIDEO_ES_H264:
			source.mimeType = "video/h264";
			source.streamFormat = info.mNaluLengthPrefixed ? media::StreamFormat::AVC : media::StreamFormat::BYTE_STREAM;
			break;
		case GST_FORMAT_VIDEO_ES_HEVC:
			source.mimeType = "video/h265";
			source.streamFormat = info.mNaluLengthPrefixed ? media::StreamFormat::HVC1 : media::StreamFormat::BYTE_STREAM;
			break;
		case GST_FORMAT_AUDIO_ES_AAC:
		case GST_FORMAT_AUDIO_ES_AAC_RAW:
			source.mimeType = "audio/mp4";
			source.streamFormat = media::StreamFormat::RAW;
			break;
		default:
			break;
	}
	if (mType == media::MediaSourceType::VIDEO)
	{
		source.width = info.mInfo.video.mWidth;
		source.height = info.mInfo.video.mHeight;
	}
	else
	{
		source.numberOfChannels = info.mInfo.audio.mChannelCount;
		source.sampleRate = info.mInfo.audio.mSampleRate;
	}
	source.id = mSource.id;
	mCodecSignature = source.mimeType + ":" + std::to_string(mTimescale) + ":" + hex(source.codecData);
	mSource = std::move(source);
}

int64_t CmafTrackParser::ticksToNs(int64_t ticks) const
{
	// Split to avoid overflow: ticks may be ~1e14 on an epoch-anchored timeline.
	const int64_t seconds = ticks / mTimescale;
	const int64_t remainder = ticks % mTimescale;
	return seconds * 1000000000LL + remainder * 1000000000LL / mTimescale;
}

bool CmafTrackParser::parseFragment(const std::shared_ptr<const ByteBuffer> &payload, CmafFragment &out, std::string &error)
{
	out = CmafFragment();
	if (!payload || payload->empty())
	{
		error = "empty Object payload";
		return false;
	}
	out.hadInit = hasHeader(payload->data(), payload->size());
	out.prft = findPrft(payload->data(), payload->size());
	if (!mHasInit && !out.hadInit)
	{
		error = "fragment before CMAF header";
		return false;
	}

	// Mp4Demux only reads the buffer; the samples alias it, which keeps the Object payload
	// alive for exactly as long as an access unit refers to it.
	auto segment = std::const_pointer_cast<ByteBuffer>(payload);
	if (!mDemux->Parse(std::move(segment)))
	{
		error = "fragment parse failed (Mp4ParseError " + std::to_string(mDemux->GetLastError()) + ")";
		return false;
	}
	if (out.hadInit)
	{
		mTimescale = mDemux->GetTimeScale();
		if (mTimescale == 0)
		{
			error = "in-band CMAF header has no timescale";
			return false;
		}
		mDemux->SetFallbackTimeScale(mTimescale);
		updateSource();
		mHasInit = true;
	}

	std::vector<AampMediaSample> samples = mDemux->GetSamples();
	if (samples.empty())
	{
		return true;
	}
	int64_t earliest = std::numeric_limits<int64_t>::max();
	int64_t latest = std::numeric_limits<int64_t>::min();
	out.segments.reserve(samples.size());
	for (auto &sample : samples)
	{
		const int64_t ptsTicks = secondsToTicks(sample.mPts, mTimescale);
		const int64_t dtsTicks = secondsToTicks(sample.mDts, mTimescale);
		const int64_t durationTicks = secondsToTicks(sample.mDuration, mTimescale);
		media::MediaSegment au;
		au.sourceId = mSource.id;
		au.type = mType;
		au.timeStampNs = ticksToNs(ptsTicks);
		au.decodeTimeStampNs = ticksToNs(dtsTicks);
		au.durationNs = ticksToNs(durationTicks);
		au.keyFrame = sample.mIsKeyFrame;
		au.size = sample.mDataSize;
		au.data = std::move(sample.mData);
		earliest = std::min(earliest, au.timeStampNs);
		latest = std::max(latest, au.timeStampNs + au.durationNs);
		if (au.keyFrame) ++out.keyFrames;
		out.segments.push_back(std::move(au));
	}
	out.baseMediaDecodeTime = static_cast<uint64_t>(secondsToTicks(samples.front().mDts, mTimescale));
	out.startPtsNs = earliest;
	out.endPtsNs = latest;
	out.startsWithKeyFrame = out.segments.front().keyFrame;
	return true;
}

} // namespace nmqe
