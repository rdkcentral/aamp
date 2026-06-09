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
 * @file AampRialtoMediaSource.cpp
 * @brief Implementation of AampRialtoMediaSource — common pacing, DRM,
 *        and injection logic shared by all media source types.
 */

#include "AampRialtoMediaSource.h"
#include "AampLogManager.h"
#include "mp4demux/MP4Demux.h"
#include <algorithm>
#include <cinttypes>

// ---------------------------------------------------------------------------
// Anonymous helpers
// ---------------------------------------------------------------------------

namespace {

/// Map AAMP cipher type to the Rialto CipherMode enum.
firebolt::rialto::CipherMode cipherTypeToRialto(CipherType cipher)
{
	switch (cipher)
	{
		case CIPHER_TYPE_CENC: return firebolt::rialto::CipherMode::CENC;
		case CIPHER_TYPE_CBCS: return firebolt::rialto::CipherMode::CBCS;
		case CIPHER_TYPE_CBC1: return firebolt::rialto::CipherMode::CBC1;
		case CIPHER_TYPE_CENS: return firebolt::rialto::CipherMode::CENS;
		default:               return firebolt::rialto::CipherMode::UNKNOWN;
	}
}

} // namespace

// ---------------------------------------------------------------------------
// Destructor (out-of-line for unique_ptr<Mp4Demux> with forward decl)
// ---------------------------------------------------------------------------

AampRialtoMediaSource::~AampRialtoMediaSource() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::reset()
{
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		++m_state.generation;
		m_state.hasPending          = false;
		m_state.segmentsAddedInBatch = 0;
		m_state.eos                 = false;
		m_state.injectionGated      = false;
		m_state.injectorActive      = false;
		m_state.attachPending   = false;
		m_state.cv.notify_all();
	}
	m_sourceId       = -1;
	m_mksId          = -1;
	m_pendingCodecData = nullptr;
}

void AampRialtoMediaSource::invalidateGeneration()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	++m_state.generation;
	m_state.hasPending          = false;
	m_state.segmentsAddedInBatch = 0;
	m_state.injectionGated      = true;
	m_state.cv.notify_all();
}

bool AampRialtoMediaSource::waitForAttach()
{
	if (isAttached())
	{
		return true;  // fast path — already attached
	}
	std::unique_lock<std::mutex> lock(m_state.mu);
	if (!m_state.attachPending)
	{
		// Not attached and no deferred attachment in flight.
		return false;
	}
	const uint64_t waitGen = m_state.generation;
	AAMPLOG_INFO(
		"Inject blocked — source awaiting deferred attach mediaType=%d",
		static_cast<int>(mediaType()));
	// Do NOT include m_state.injectionGated in the predicate: the
	// pipeline transitions through PAUSED during startup before the
	// video IPC call returns.  Including injectionGated would
	// immediately wake this thread, find isAttached()==false, and
	// discard the frame.  The generation change (set by
	// invalidateGeneration inside Flush/Stop) is the correct abort.
	m_state.cv.wait(lock, [&]{
		return isAttached() || m_state.generation != waitGen;
	});
	if (!isAttached())
	{
		AAMPLOG_INFO(
			"Attach did not complete (abort) — discarding mediaType=%d",
			static_cast<int>(mediaType()));
		return false;
	}
	return true;
}

uint64_t AampRialtoMediaSource::captureGeneration()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	return m_state.generation;
}

// ---------------------------------------------------------------------------
// Protection
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::setProtection(ProtectionParams params)
{
	m_protection = std::move(params);
}

void AampRialtoMediaSource::clearProtection()
{
	m_mksId = -1;
	m_protection = std::nullopt;
}

// ---------------------------------------------------------------------------
// Codec data
// ---------------------------------------------------------------------------

std::shared_ptr<firebolt::rialto::CodecData>
AampRialtoMediaSource::takePendingCodecData()
{
	return std::exchange(m_pendingCodecData, nullptr);
}

// ---------------------------------------------------------------------------
// attachOrUpdate — Template Method
// ---------------------------------------------------------------------------

AampRialtoMediaSource::AttachResult AampRialtoMediaSource::attachOrUpdate(
	firebolt::rialto::IMediaPipeline &pipeline,
	MediaCodecInfo &codecInfo,
	IDrmBridge *drmBridge,
	int64_t flushPosNs)
{
	// 1. Validate codec
	std::string mimeType;
	firebolt::rialto::StreamFormat streamFormat =
		firebolt::rialto::StreamFormat::UNDEFINED;
	if (!mapCodecToMime(codecInfo.mCodecFormat, mimeType, streamFormat))
	{
		AAMPLOG_ERR("Unknown codec format=%d for mediaType=%d",
			static_cast<int>(codecInfo.mCodecFormat),
			static_cast<int>(mediaType()));
		return AttachResult::FAILED;
	}

	// 2. Build codec data
	std::shared_ptr<firebolt::rialto::CodecData> codecData;
	if (!codecInfo.mCodecData.empty())
	{
		codecData = std::make_shared<firebolt::rialto::CodecData>();
		codecData->data = codecInfo.mCodecData;
	}

	// 3. Update type-specific cached metadata (virtual)
	updateCachedMetadata(codecInfo);

	// 4. Stage pending codec data for the injection path
	m_pendingCodecData = codecData;

	// 5. If already attached → update only
	if (m_sourceId >= 0)
	{
		AAMPLOG_INFO("source already attached (id=%d) for mediaType=%d, "
			"staged new codec data",
			m_sourceId, static_cast<int>(mediaType()));
		return AttachResult::UPDATED;
	}

	// 6. First attach — create DRM session if protection params are present
	if (m_protection.has_value() && !drmBridge)
	{
		AAMPLOG_ERR("Protection params present but drmBridge is null for mediaType=%d"
			" — DRM session will not be created",
			static_cast<int>(mediaType()));
	}
	if (m_protection.has_value() && drmBridge)
	{
		const auto &prot = *m_protection;
		m_mksId = drmBridge->createSession(
			prot.systemId.c_str(),
			prot.initData.data(),
			prot.initData.size(),
			prot.type);
		if (m_mksId < 0)
		{
			AAMPLOG_WARN("createSession failed for mediaType=%d",
				static_cast<int>(mediaType()));
		}
		else
		{
			AAMPLOG_INFO("createSession returned mksId=%d for mediaType=%d",
				m_mksId, static_cast<int>(mediaType()));
		}
	}

	// 7. Create the Rialto source object (virtual — video/audio differ)
	auto source = createRialtoSource(
		mimeType, m_mksId >= 0, codecInfo, streamFormat, codecData);
	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
		sourceBase = std::move(source);
	if (!pipeline.attachSource(sourceBase))
	{
		AAMPLOG_ERR("attachSource failed for mediaType=%d",
			static_cast<int>(mediaType()));
		return AttachResult::FAILED;
	}

	m_sourceId = sourceBase->getId();
	AAMPLOG_INFO("Attached source id=%d mime=%s mediaType=%d",
		m_sourceId, mimeType.c_str(), static_cast<int>(mediaType()));

	// 8. Set initial segment position if a flush was staged
	if (flushPosNs >= 0)
	{
		if (!pipeline.setSourcePosition(
				m_sourceId, flushPosNs, /*resetTime=*/true))
		{
			AAMPLOG_WARN("setSourcePosition(%" PRId64 ") failed for "
				"mediaType=%d", flushPosNs,
				static_cast<int>(mediaType()));
		}
		else
		{
			AAMPLOG_INFO("setSourcePosition(%" PRId64 ") ok for "
				"mediaType=%d", flushPosNs,
				static_cast<int>(mediaType()));
		}
	}

	return AttachResult::NEWLY_ATTACHED;
}

// ---------------------------------------------------------------------------
// injectOneSample
// ---------------------------------------------------------------------------

bool AampRialtoMediaSource::injectOneSample(
	firebolt::rialto::IMediaPipeline &pipeline,
	uint64_t capturedGen,
	AampMediaSample &&sample,
	std::shared_ptr<firebolt::rialto::CodecData> codecData)
{
	bool injected = false;

	// If EOS was already fully handled, nothing to do.
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		if (m_state.eos && !m_state.hasPending)
		{
			return false;
		}
		m_state.injectorActive = true;
	}

	bool done = false;
	while (!done)
	{
		uint32_t reqId = 0;
		{
			std::unique_lock<std::mutex> lock(m_state.mu);
			m_state.cv.wait(lock, [&]{
				return m_state.generation != capturedGen ||
				       m_state.injectionGated ||
				       m_state.hasPending;
			});
			if (m_state.generation != capturedGen || m_state.injectionGated)
			{
				m_state.injectorActive = false;
				done = true;
				continue;
			}
			reqId = m_state.pendingRequestId;
		}

		// Build the segment outside the lock (polymorphic).
		auto segment = createSegment(sample);
		if (!segment)
		{
			AAMPLOG_WARN("createSegment returned null for sourceId=%d",
				m_sourceId);
			// If EOS is pending, send haveData(EOS) so the request
			// doesn't hang.
			{
				bool fireEos = false;
				uint32_t eosReqId = 0;
				{
					std::lock_guard<std::mutex> lock(m_state.mu);
					if (m_state.eos && m_state.hasPending &&
					    m_state.pendingRequestId == reqId)
					{
						eosReqId = reqId;
						m_state.hasPending          = false;
						m_state.segmentsAddedInBatch = 0;
						fireEos = true;
					}
					m_state.injectorActive = false;
				}
				if (fireEos)
				{
					pipeline.haveData(
						firebolt::rialto::MediaSourceStatus::EOS,
						eosReqId);
				}
			}
			done = true;
			continue;
		}

		if (codecData)
		{
			segment->setCodecData(codecData);
		}

		// Annotate DRM metadata when encrypted.
		if (sample.mDrmMetadata.mIsEncrypted && m_mksId < 0)
		{
			AAMPLOG_WARN("Encrypted sample for sourceId=%d but no DRM session (mksId=%d)"
				" — segment will be injected without encryption metadata",
				m_sourceId, m_mksId);
		}
		if (sample.mDrmMetadata.mIsEncrypted && m_mksId >= 0)
		{
			segment->setEncrypted(true);
			segment->setMediaKeySessionId(m_mksId);
			segment->setKeyId(sample.mDrmMetadata.mKeyId);
			segment->setInitVector(sample.mDrmMetadata.mIV);
			segment->setCipherMode(
				cipherTypeToRialto(sample.mDrmMetadata.mCipher));
			if (sample.mDrmMetadata.mCipher == CIPHER_TYPE_CBCS)
			{
				segment->setEncryptionPattern(
					sample.mDrmMetadata.mCryptByteBlock,
					sample.mDrmMetadata.mSkipByteBlock);
			}

			if (sample.mDrmMetadata.mNumSubSamples > 0)
			{
				const size_t kEntrySize = 6;
				const auto  &raw        = sample.mDrmMetadata.mSubSamples;
				for (uint32_t s = 0;
				     s < sample.mDrmMetadata.mNumSubSamples; ++s)
				{
					const size_t offset = s * kEntrySize;
					if (offset + kEntrySize > raw.size())
					{
						break;
					}
					const uint16_t clearBytes =
						(static_cast<uint16_t>(raw[offset])     << 8) |
						 static_cast<uint16_t>(raw[offset + 1]);
					const uint32_t encBytes =
						(static_cast<uint32_t>(raw[offset + 2]) << 24) |
						(static_cast<uint32_t>(raw[offset + 3]) << 16) |
						(static_cast<uint32_t>(raw[offset + 4]) <<  8) |
						 static_cast<uint32_t>(raw[offset + 5]);
					segment->addSubSample(clearBytes, encBytes);
				}
			}
			else
			{
				segment->addSubSample(
					/*numClearBytes=*/0,
					static_cast<uint32_t>(sample.mDataSize));
			}
		}

		segment->setData(
			static_cast<uint32_t>(sample.mDataSize),
			sample.mData.get());

		auto addStatus = pipeline.addSegment(reqId, segment);
		if (addStatus == firebolt::rialto::AddSegmentStatus::NO_SPACE)
		{
			size_t addedSoFar = 0;
			bool   sendHaveData = false;
			{
				std::lock_guard<std::mutex> lock(m_state.mu);
				if (m_state.generation == capturedGen &&
				    m_state.hasPending &&
				    m_state.pendingRequestId == reqId)
				{
					addedSoFar                  = m_state.segmentsAddedInBatch;
					m_state.hasPending          = false;
					m_state.segmentsAddedInBatch = 0;
					sendHaveData                = true;
				}
			}
			if (sendHaveData)
			{
				const auto status = addedSoFar > 0
					? firebolt::rialto::MediaSourceStatus::OK
					: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
				if (!pipeline.haveData(status, reqId))
				{
					AAMPLOG_WARN("haveData failed requestId=%u", reqId);
				}
			}
			AAMPLOG_INFO("addSegment NO_SPACE sourceId=%d requestId=%u "
				"— waiting for next needData",
				m_sourceId, reqId);
		}
		else
		{
			if (addStatus != firebolt::rialto::AddSegmentStatus::OK)
			{
				AAMPLOG_WARN("addSegment failed sourceId=%d "
					"requestId=%u status=%d",
					m_sourceId, reqId, static_cast<int>(addStatus));
			}

			bool sendHaveData = false;
			bool sendEos      = false;
			{
				std::lock_guard<std::mutex> lock(m_state.mu);
				if (m_state.generation == capturedGen &&
				    m_state.hasPending &&
				    m_state.pendingRequestId == reqId)
				{
					++m_state.segmentsAddedInBatch;
					if (m_state.eos)
					{
						// Last sample — signal EOS to Rialto.
						m_state.hasPending          = false;
						m_state.segmentsAddedInBatch = 0;
						sendHaveData                = true;
						sendEos                     = true;
					}
					else if (m_state.segmentsAddedInBatch >=
					         m_state.pendingFrameCount)
					{
						m_state.hasPending          = false;
						m_state.segmentsAddedInBatch = 0;
						sendHaveData                = true;
					}
				}
				m_state.injectorActive = false;
			}
			if (sendHaveData)
			{
				auto status = sendEos
					? firebolt::rialto::MediaSourceStatus::EOS
					: firebolt::rialto::MediaSourceStatus::OK;
				if (!pipeline.haveData(status, reqId))
				{
					AAMPLOG_WARN("haveData failed requestId=%u",
						reqId);
				}
			}
			injected = true;
			done     = true;
		}
	}

	return injected;
}

// ---------------------------------------------------------------------------
// signalEos
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::signalEos(
	firebolt::rialto::IMediaPipeline *pipeline)
{
	bool     fireEos = false;
	uint32_t reqId   = 0;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		m_state.eos = true;
		if (m_state.hasPending && !m_state.injectorActive)
		{
			// No injector is active — respond immediately with EOS.
			// Any segments already buffered in Rialto for this request
			// will be delivered before EOS is propagated downstream.
			reqId = m_state.pendingRequestId;
			m_state.hasPending = false;
			fireEos = true;
		}
		// If injectorActive, the injector will send haveData(EOS) after
		// delivering its sample.
	}
	if (fireEos)
	{
		if (!pipeline)
		{
			AAMPLOG_ERR("pipeline is null — cannot send EOS for sourceId=%d",
				m_sourceId);
		}
		else if (!pipeline->haveData(
				firebolt::rialto::MediaSourceStatus::EOS, reqId))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u", reqId);
		}
	}
	m_state.cv.notify_all();
}

// ---------------------------------------------------------------------------
// handleNeedData / handleCancelNeedData
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::handleNeedData(
	size_t frameCount,
	uint32_t requestId,
	firebolt::rialto::IMediaPipeline *pipeline)
{
	AAMPLOG_INFO("sourceId=%d frameCount=%zu requestId=%u",
		m_sourceId, frameCount, requestId);

	// Inband CC sources have no data to inject — the Rialto server
	// extracts CC from the video bitstream internally.  Acknowledge
	// immediately with NO_AVAILABLE_SAMPLES so the server does not
	// stall waiting for data that will never arrive from AAMP.
	if (isInbandCC())
	{
		AAMPLOG_INFO("Inband CC source: responding with "
			"NO_AVAILABLE_SAMPLES requestId=%u", requestId);
		if (pipeline &&
		    !pipeline->haveData(
			    firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES,
			    requestId))
		{
			AAMPLOG_WARN("haveData(NO_AVAILABLE_SAMPLES) failed "
				"requestId=%u", requestId);
		}
		return;
	}

	bool fireEos = false;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		if (m_state.eos && !m_state.injectorActive)
		{
			// No injector waiting — respond with EOS directly.
			fireEos = true;
		}
		else
		{
			// Either not EOS, or an injector is active and needs the
			// slot to deliver its sample (it will send haveData(EOS)
			// after injection).
		m_state.hasPending          = true;
		m_state.pendingRequestId    = requestId;
		const size_t batchOverride  = needDataBatchSize();
		m_state.pendingFrameCount   = (batchOverride > 0)
			? batchOverride
			: std::max<size_t>(frameCount, 1);
		m_state.segmentsAddedInBatch = 0;
		m_state.injectionGated      = false;
		}
	}
	if (fireEos)
	{
		if (pipeline &&
		    !pipeline->haveData(
			    firebolt::rialto::MediaSourceStatus::EOS, requestId))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u", requestId);
		}
	}
	else
	{
		m_state.cv.notify_all();
	}
}

void AampRialtoMediaSource::handleCancelNeedData()
{
	AAMPLOG_INFO("sourceId=%d", m_sourceId);
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		m_state.hasPending          = false;
		m_state.segmentsAddedInBatch = 0;
	}
	m_state.cv.notify_all();
}

// ---------------------------------------------------------------------------
// flushSource
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::flushSource(
	firebolt::rialto::IMediaPipeline &pipeline,
	int64_t positionNs)
{
	if (m_sourceId >= 0)
	{
		bool async = false;
		if (!pipeline.flush(m_sourceId, /*resetTime=*/true, async))
		{
			AAMPLOG_WARN("flush failed for sourceId=%d", m_sourceId);
		}
		if (!pipeline.setSourcePosition(
				m_sourceId, positionNs, /*resetTime=*/true))
		{
			AAMPLOG_WARN("setSourcePosition failed for sourceId=%d",
				m_sourceId);
		}
	}
}

// ---------------------------------------------------------------------------
// processInitFragment
// ---------------------------------------------------------------------------

std::optional<MediaCodecInfo> AampRialtoMediaSource::processInitFragment(
	std::shared_ptr<std::vector<uint8_t>> buffer)
{
	if (!m_demuxer)
	{
		m_demuxer = std::make_unique<Mp4Demux>();
	}
	if (!m_demuxer->Parse(std::move(buffer)))
	{
		AAMPLOG_ERR(
			"processInitFragment: Parse failed mediaType=%d err=%d",
			static_cast<int>(mediaType()),
			static_cast<int>(m_demuxer->GetLastError()));
		return std::nullopt;
	}
	return m_demuxer->GetCodecInfo();
}

// ---------------------------------------------------------------------------
// processDataFragment
// ---------------------------------------------------------------------------

bool AampRialtoMediaSource::processDataFragment(
	firebolt::rialto::IMediaPipeline &pipeline,
	std::shared_ptr<std::vector<uint8_t>> buffer,
	double /*fpts*/,
	double /*fdts*/,
	double /*fDuration*/,
	double /*fragmentPTSoffset*/)
{
	if (!m_demuxer)
	{
		AAMPLOG_WARN("processDataFragment: no demuxer for mediaType=%d",
			static_cast<int>(mediaType()));
		return false;
	}
	if (!m_demuxer->Parse(std::move(buffer)))
	{
		AAMPLOG_ERR(
			"processDataFragment: Parse failed mediaType=%d err=%d",
			static_cast<int>(mediaType()),
			static_cast<int>(m_demuxer->GetLastError()));
		return false;
	}
	auto samples = m_demuxer->GetSamples();
	if (samples.empty())
	{
		return true;
	}
	if (!waitForAttach())
	{
		return true;
	}
	uint64_t capturedGen = captureGeneration();
	auto pendingCodecData = takePendingCodecData();
	bool firstSample = true;
	for (auto &s : samples)
	{
		std::shared_ptr<firebolt::rialto::CodecData> codecData;
		if (firstSample)
		{
			codecData = pendingCodecData;
		}
		firstSample = false;
		if (!injectOneSample(pipeline, capturedGen, std::move(s), codecData))
		{
			AAMPLOG_INFO(
				"processDataFragment: aborted mid-batch mediaType=%d",
				static_cast<int>(mediaType()));
			break;
		}
	}
	AAMPLOG_INFO("Processed %zu samples for mediaType=%d",
		samples.size(), static_cast<int>(mediaType()));
	return true;
}

// ---------------------------------------------------------------------------
// injectSingleSample
// ---------------------------------------------------------------------------

bool AampRialtoMediaSource::injectSingleSample(
	firebolt::rialto::IMediaPipeline &pipeline,
	AampMediaSample &&sample)
{
	if (!waitForAttach())
	{
		return false;
	}
	uint64_t capturedGen = captureGeneration();
	auto pendingCodecData = takePendingCodecData();
	return injectOneSample(
		pipeline, capturedGen, std::move(sample), pendingCodecData);
}
