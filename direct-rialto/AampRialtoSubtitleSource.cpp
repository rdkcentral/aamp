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
 * @file AampRialtoSubtitleSource.cpp
 * @brief Subtitle-specific Rialto media source skeleton.
 */

#include "AampRialtoSubtitleSource.h"
#include "AampLogManager.h"

// ---------------------------------------------------------------------------
// mapCodecToMime — not yet implemented for subtitles
// ---------------------------------------------------------------------------

bool AampRialtoSubtitleSource::mapCodecToMime(
	GstStreamOutputFormat /*codecFormat*/,
	std::string & /*mimeType*/,
	firebolt::rialto::StreamFormat & /*streamFormat*/) const
{
	// Subtitle injection via Rialto is not yet supported.
	return false;
}

// ---------------------------------------------------------------------------
// createRialtoSource — not yet implemented for subtitles
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
AampRialtoSubtitleSource::createRialtoSource(
	const std::string & /*mimeType*/,
	bool /*hasDrm*/,
	const MediaCodecInfo & /*codecInfo*/,
	firebolt::rialto::StreamFormat /*streamFormat*/,
	std::shared_ptr<firebolt::rialto::CodecData> /*codecData*/) const
{
	return nullptr;
}

// ---------------------------------------------------------------------------
// updateCachedMetadata — no-op for subtitle skeleton
// ---------------------------------------------------------------------------

void AampRialtoSubtitleSource::updateCachedMetadata(
	const MediaCodecInfo & /*codecInfo*/)
{
}

// ---------------------------------------------------------------------------
// createSegment — not yet implemented for subtitles
// ---------------------------------------------------------------------------

std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
AampRialtoSubtitleSource::createSegment(
	int64_t /*ptsNs*/, int64_t /*durationNs*/) const
{
	return nullptr;
}
