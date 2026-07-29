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
		// injectionGated is intentionally left untouched here.  reset() runs
		// mid-Configure(), which can be followed by a second Flush() (e.g.
		// trickplay's Flush(pos=0) -> Configure() -> Flush(correctPos) ->
		// Stream()) before playback is meant to resume.  Clearing the gate
		// now would let an early needData slip through with stale-position
		// data.  The gate is cleared only via clearInjectionGate(), called
		// from AampRialtoPlayer at the points that actually issue play().
		m_state.injectorActive      = false;
		m_state.attachPending   = false;
		m_state.cv.notify_all();
	}
	m_sourceId       = -1;
	m_mksId          = -1;
	m_pendingCodecData = nullptr;

	m_firstPtsMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
}

void AampRialtoMediaSource::invalidateGeneration(
	firebolt::rialto::IMediaPipeline *pipeline, const char *reason,
	std::optional<bool> newEosState)
{
	bool     fireNoAvailableSamples = false;
	uint32_t reqId                  = 0;
	bool     wasGated                = false;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		wasGated = m_state.injectionGated;
		++m_state.generation;
		m_state.injectionGated = true;
		if (newEosState.has_value())
		{
			// Applied atomically with the gate/generation change above so a
			// concurrent needData/signalEos can never observe the gate set
			// but the pre-invalidation eos value (or vice versa).
			m_state.eos = *newEosState;
		}
		if (!m_state.injectorActive)
		{
			// No injector is active to answer this request on our
			// behalf — close it out now so it is never left dangling
			// (e.g. a needData arrived just before Flush()/Stop() and
			// nothing would otherwise ever respond to it).
			fireNoAvailableSamples = claimPendingRequestLocked(reqId);
		}
		// If an injector is active, it owns the request and will close
		// it out itself when it observes the generation change (see
		// injectOneSample()).
		m_state.cv.notify_all();
	}
	AAMPLOG_INFO("injectionGated SET (%s->true) sourceId=%d mediaType=%d newEosState=%d reason=%s",
		wasGated ? "true" : "false", m_sourceId, static_cast<int>(mediaType()), newEosState.has_value() ? newEosState : 0, reason);
	if (fireNoAvailableSamples)
	{
		respondAbandonedRequest(pipeline, reqId);
	}
	// Reset segment-start so the next injection establishes a fresh
	// baseline after the seek.  Written outside the lock (atomic).
	m_firstPtsMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
}

void AampRialtoMediaSource::clearInjectionGate(
	firebolt::rialto::IMediaPipeline *pipeline, const char *reason)
{
	bool     wasGated = false;
	bool     fireEos  = false;
	uint32_t reqId    = 0;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		wasGated = m_state.injectionGated;
		m_state.injectionGated = false;
		// Replay any EOS resolution that was deliberately deferred while
		// the gate was set — see tryClaimEosLocked().
		fireEos = tryClaimEosLocked(reqId);
		m_state.cv.notify_all();
	}
	if (wasGated)
	{
		AAMPLOG_INFO("injectionGated CLEARED sourceId=%d mediaType=%d reason=%s",
			m_sourceId, static_cast<int>(mediaType()), reason);
	}
	if (fireEos)
	{
		if (!pipeline)
		{
			AAMPLOG_ERR("pipeline is null — cannot send deferred EOS for "
				"sourceId=%d", m_sourceId);
		}
		else if (!pipeline->haveData(
				firebolt::rialto::MediaSourceStatus::EOS, reqId))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u", reqId);
		}
	}
}

void AampRialtoMediaSource::setEos(bool eos, const char *reason)
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	if (m_state.eos != eos)
	{
		AAMPLOG_INFO("eos (%s->%s) sourceId=%d mediaType=%d reason=%s",
			m_state.eos ? "true" : "false", eos ? "true" : "false",
			m_sourceId, static_cast<int>(mediaType()), reason);
		m_state.eos = eos;
		m_state.cv.notify_all();
	}
}

bool AampRialtoMediaSource::claimPendingRequestLocked(uint32_t &outReqId)
{
	bool claimed = false;
	if (m_state.hasPending)
	{
		outReqId                    = m_state.pendingRequestId;
		m_state.hasPending          = false;
		m_state.segmentsAddedInBatch = 0;
		claimed                     = true;
	}
	return claimed;
}

bool AampRialtoMediaSource::tryClaimEosLocked(uint32_t &outReqId)
{
	if (m_state.eos && m_state.hasPending &&
	    !m_state.injectorActive && !m_state.injectionGated)
	{
		return claimPendingRequestLocked(outReqId);
	}
	return false;
}

void AampRialtoMediaSource::respondAbandonedRequest(
	firebolt::rialto::IMediaPipeline *pipeline, uint32_t reqId)
{
	if (!pipeline)
	{
		AAMPLOG_ERR("pipeline is null — cannot close abandoned "
			"requestId=%u for sourceId=%d", reqId, m_sourceId);
	}
	else if (!pipeline->haveData(
			firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES,
			reqId))
	{
		AAMPLOG_WARN("haveData(NO_AVAILABLE_SAMPLES) failed "
			"requestId=%u for sourceId=%d", reqId, m_sourceId);
	}
}

int64_t AampRialtoMediaSource::firstPtsMs() const
{
	return m_firstPtsMs.load(std::memory_order_relaxed);
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
	int64_t flushPosNs,
	const std::optional<ProtectionParams> &protection,
	double appliedRate)
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
	if (protection.has_value() && !drmBridge)
	{
		AAMPLOG_ERR("Protection params present but drmBridge is null for mediaType=%d"
			" — DRM session will not be created",
			static_cast<int>(mediaType()));
	}
	if (protection.has_value() && drmBridge)
	{
		const auto &prot = *protection;
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
				m_sourceId, flushPosNs, /*resetTime=*/true, appliedRate))
		{
			AAMPLOG_WARN("setSourcePosition(%" PRId64 ") appliedRate=%f failed "
				"for mediaType=%d", flushPosNs, appliedRate,
				static_cast<int>(mediaType()));
		}
		else
		{
			AAMPLOG_INFO("setSourcePosition(%" PRId64 ") appliedRate=%f ok for "
				"mediaType=%d", flushPosNs, appliedRate,
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
	std::shared_ptr<firebolt::rialto::CodecData> codecData,
	bool morePending)
{
	bool injected     = false;
	bool alreadyAtEos = false;

	// If EOS was already fully handled, nothing to do.
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		alreadyAtEos = m_state.eos && !m_state.hasPending;
		if (!alreadyAtEos)
		{
			m_state.injectorActive = true;
		}
	}

	bool done = alreadyAtEos;

	if (!done)
	{
		// Block here — rather than dropping the sample — if it was
		// captured while injectionGated was already set for the current
		// generation (Stream() was called while a flush was still in
		// progress, so play() is deferred and the gate has not been
		// cleared yet: see AampRialtoPlayer::Stream()).  This is
		// genuinely-wanted playback data, not stale data, so it must not
		// be discarded merely because it arrived a little early.
		//
		// injectionGated can only transition false->true together with a
		// generation bump (see invalidateGeneration()); clearInjectionGate()
		// never touches generation.  So once we observe
		// generation == capturedGen && !injectionGated below, the gate
		// cannot be re-set again without also invalidating capturedGen —
		// meaning the generation check in the main loop below is already
		// sufficient to catch any newer flush that supersedes this sample.
		bool     bailedOut    = false;
		bool     abortPending = false;
		uint32_t abortReqId   = 0;
		{
			std::unique_lock<std::mutex> lock(m_state.mu);
			m_state.cv.wait(lock, [&]{
				return m_state.generation != capturedGen ||
				       !m_state.injectionGated;
			});
			if (m_state.generation != capturedGen)
			{
				// Superseded by a newer flush while waiting for the gate
				// to clear — now genuinely stale, discard it.
				abortPending = claimPendingRequestLocked(abortReqId);
				m_state.injectorActive = false;
				bailedOut = true;
			}
		}
		if (bailedOut)
		{
			AAMPLOG_INFO("injector bailing out while waiting for ungate "
				"sourceId=%d - superseded by a newer flush, "
				"abandonedRequestId=%u",
				m_sourceId, abortPending ? abortReqId : 0);
			if (abortPending)
			{
				respondAbandonedRequest(&pipeline, abortReqId);
			}
			done = true;
		}
	}

	while (!done)
	{
		uint32_t reqId        = 0;
		bool     bailedOut    = false;
		bool     abortPending = false;
		uint32_t abortReqId   = 0;
		{
			std::unique_lock<std::mutex> lock(m_state.mu);
			m_state.cv.wait(lock, [&]{
				return m_state.generation != capturedGen ||
				       m_state.hasPending;
			});
			if (m_state.generation != capturedGen)
			{
				// This injector is unconditionally abandoning its slot.
				// invalidateGeneration() deferred responsibility to us
				// (we were the active injector) — if a request is still
				// pending, no other path will ever close it out, so we
				// must respond ourselves once the lock is released.
				abortPending = claimPendingRequestLocked(abortReqId);
				m_state.injectorActive = false;
				bailedOut = true;
			}
			else
			{
				reqId = m_state.pendingRequestId;
			}
		}

		if (bailedOut)
		{
			AAMPLOG_INFO("injector bailing out sourceId=%d generation changed - "
				"abandonedRequestId=%u",
				m_sourceId, abortPending ? abortReqId : 0);
		}

		if (abortPending)
		{
			respondAbandonedRequest(&pipeline, abortReqId);
		}

		if (bailedOut)
		{
			done = true;
		}
		else
		{
			// Build the segment outside the lock (polymorphic).
			auto segment = createSegment(sample);
			if (!segment)
			{
				AAMPLOG_WARN("createSegment returned null for sourceId=%d",
					m_sourceId);
				// Always respond if we still own this request — dropping it
				// silently (as a non-EOS createSegment() failure previously
				// did) would leak it forever, since nothing else will ever
				// answer this requestId.
				bool     sendResponse = false;
				bool     wasEos       = false;
				uint32_t respReqId    = 0;
				{
					std::lock_guard<std::mutex> lock(m_state.mu);
					if (m_state.hasPending &&
					    m_state.pendingRequestId == reqId)
					{
						respReqId                     = reqId;
						wasEos                        = m_state.eos;
						m_state.hasPending            = false;
						m_state.segmentsAddedInBatch  = 0;
						sendResponse                   = true;
					}
					m_state.injectorActive = false;
				}
				if (sendResponse)
				{
					const auto status = wasEos
						? firebolt::rialto::MediaSourceStatus::EOS
						: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
					if (!pipeline.haveData(status, respReqId))
					{
						AAMPLOG_WARN("haveData failed requestId=%u", respReqId);
					}
				}
				done = true;
			}
			else
			{
				if (codecData)
				{
					segment->setCodecData(codecData);
				}
				annotateEncryption(sample, *segment);
				segment->setData(
					static_cast<uint32_t>(sample.mDataSize),
					sample.mData.get());

				auto addStatus = pipeline.addSegment(reqId, segment);
				if (addStatus == firebolt::rialto::AddSegmentStatus::NO_SPACE)
				{
					handleAddSegmentNoSpace(pipeline, capturedGen, reqId);
				}
				else
				{
					handleAddSegmentCompletion(pipeline, addStatus,
						capturedGen, reqId, morePending, sample.mPts);
					injected = true;
					done     = true;
				}
			}
		}
	}

	return injected;
}

// ---------------------------------------------------------------------------
// injectOneSample helpers
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::annotateEncryption(
	const AampMediaSample &sample,
	firebolt::rialto::IMediaPipeline::MediaSegment &segment) const
{
	if (sample.mDrmMetadata.mIsEncrypted && m_mksId < 0)
	{
		AAMPLOG_WARN("Encrypted sample for sourceId=%d but no DRM session (mksId=%d)"
			" — segment will be injected without encryption metadata",
			m_sourceId, m_mksId);
	}
	if (sample.mDrmMetadata.mIsEncrypted && m_mksId >= 0)
	{
		segment.setEncrypted(true);
		segment.setMediaKeySessionId(m_mksId);
		segment.setKeyId(sample.mDrmMetadata.mKeyId);
		segment.setInitVector(sample.mDrmMetadata.mIV);
		segment.setCipherMode(
			cipherTypeToRialto(sample.mDrmMetadata.mCipher));
		if (sample.mDrmMetadata.mCipher == CIPHER_TYPE_CBCS)
		{
			segment.setEncryptionPattern(
				sample.mDrmMetadata.mCryptByteBlock,
				sample.mDrmMetadata.mSkipByteBlock);
		}

		AAMPLOG_TRACE("Encrypted segment: sourceId=%d mksId=%d cipher=%d "
			"keyIdSize=%zu ivSize=%zu numSubSamples=%u",
			m_sourceId, m_mksId,
			static_cast<int>(sample.mDrmMetadata.mCipher),
			sample.mDrmMetadata.mKeyId.size(),
			sample.mDrmMetadata.mIV.size(),
			sample.mDrmMetadata.mNumSubSamples);
		if (AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE))
		{
			AAMPLOG_TRACE("  keyId:");
			DumpBlob(sample.mDrmMetadata.mKeyId.data(),
				sample.mDrmMetadata.mKeyId.size());
			AAMPLOG_TRACE("  IV:");
			DumpBlob(sample.mDrmMetadata.mIV.data(),
				sample.mDrmMetadata.mIV.size());
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
				segment.addSubSample(clearBytes, encBytes);
				AAMPLOG_TRACE("  sub-sample[%u] clear=%u enc=%u",
					s, clearBytes, encBytes);
			}
		}
		else
		{
			segment.addSubSample(
				/*numClearBytes=*/0,
				static_cast<uint32_t>(sample.mDataSize));
			AAMPLOG_TRACE("  single sub-sample: clear=0 enc=%zu",
				sample.mDataSize);
		}
	}
	if (!sample.mDrmMetadata.mIsEncrypted && m_mksId >= 0)
	{
		AAMPLOG_TRACE("Unencrypted sample for sourceId=%d with active DRM "
			"session (mksId=%d) — segment sent without encryption metadata",
			m_sourceId, m_mksId);
	}
}

void AampRialtoMediaSource::handleAddSegmentNoSpace(
	firebolt::rialto::IMediaPipeline &pipeline,
	uint64_t capturedGen,
	uint32_t reqId)
{
	size_t   addedSoFar   = 0;
	bool     sendHaveData = false;
	uint32_t claimedReqId = 0;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		if (m_state.generation == capturedGen &&
		    m_state.hasPending &&
		    m_state.pendingRequestId == reqId)
		{
			addedSoFar   = m_state.segmentsAddedInBatch;
			sendHaveData = claimPendingRequestLocked(claimedReqId);
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

void AampRialtoMediaSource::handleAddSegmentCompletion(
	firebolt::rialto::IMediaPipeline &pipeline,
	firebolt::rialto::AddSegmentStatus addStatus,
	uint64_t capturedGen,
	uint32_t reqId,
	bool morePending,
	double samplePts)
{
	if (addStatus == firebolt::rialto::AddSegmentStatus::OK)
	{
		const int64_t ptsMs = static_cast<int64_t>(samplePts * 1000.0);
		int64_t expected = kFirstPtsNotSet;
		if (m_firstPtsMs.compare_exchange_strong(
				expected, ptsMs, std::memory_order_relaxed))
		{
			AAMPLOG_INFO("firstPtsMs set to %" PRId64 " for sourceId=%d",
				ptsMs, m_sourceId);
		}
	}
	else
	{
		AAMPLOG_WARN("addSegment failed sourceId=%d "
			"requestId=%u status=%d",
			m_sourceId, reqId, static_cast<int>(addStatus));
	}

	bool sendHaveData  = false;
	bool sendEos       = false;
	bool sendAbandoned = false;
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
				m_state.hasPending           = false;
				m_state.segmentsAddedInBatch = 0;
				sendHaveData                  = true;
				sendEos                       = true;
			}
			else if (m_state.segmentsAddedInBatch >=
			         m_state.pendingFrameCount || !morePending)
			{
				// Send haveData when we've reached the requested frame count
				// OR when morePending is false (last sample in the batch)
				m_state.hasPending           = false;
				m_state.segmentsAddedInBatch = 0;
				sendHaveData                  = true;
			}
		}
		else if (m_state.hasPending && m_state.pendingRequestId == reqId)
		{
			// Generation changed (Flush/Stop/invalidateGeneration) while
			// addSegment() was in flight for this request, but nothing has
			// claimed it since — invalidateGeneration() left it for us (the
			// active injector at the time) to close out.  Answer it now
			// rather than leaking it forever.
			m_state.hasPending           = false;
			m_state.segmentsAddedInBatch = 0;
			sendAbandoned                = true;
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
	else if (sendAbandoned)
	{
		respondAbandonedRequest(&pipeline, reqId);
	}
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
		// Deliberately gate-aware: if injectionGated is set, resolution is
		// deferred until clearInjectionGate() replays it, so EOS is never
		// answered ahead of samples still blocked behind the gate — see
		// tryClaimEosLocked().
		fireEos = tryClaimEosLocked(reqId);
		// If injectorActive or injectionGated, the pending request is left
		// for the injector (on completion) or clearInjectionGate() (on
		// ungate) to send haveData(EOS).
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

	bool     fireEos           = false;
	uint32_t eosReqId          = 0;
	bool     stagedGated       = false;
	bool     supersededPending = false;
	uint32_t supersededReqId   = 0;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		// Always stage the incoming request first, regardless of eos/gate/
		// injector state.  If a previous needData was staged but never
		// claimed (e.g. it was staged while injectionGated and a second
		// needData arrived before the gate cleared), it must be closed out
		// here rather than silently overwritten.  Dropping it silently would
		// leave Rialto's own bookkeeping for that requestId unanswered,
		// which desyncs AAMP and Rialto once the newer request is eventually
		// served.
		if (m_state.hasPending)
		{
			supersededPending = true;
			supersededReqId   = m_state.pendingRequestId;
		}
		m_state.hasPending           = true;
		m_state.pendingRequestId     = requestId;
		m_state.pendingFrameCount    = std::max<size_t>(frameCount, 1);
		m_state.segmentsAddedInBatch = 0;
		stagedGated = m_state.injectionGated;

		// Then attempt to resolve it immediately as EOS.  This intentionally
		// defers while injectionGated is set (see tryClaimEosLocked()) —
		// clearInjectionGate() replays the resolution once the gate clears.
		// injectionGated is intentionally NOT cleared here — see the
		// injectionGated field comment for why.
		fireEos = tryClaimEosLocked(eosReqId);
	}
	if (supersededPending)
	{
		AAMPLOG_WARN("sourceId=%d requestId=%u superseded stale unclaimed "
			"requestId=%u before it was ever answered - closing it out "
			"with NO_AVAILABLE_SAMPLES",
			m_sourceId, requestId, supersededReqId);
		respondAbandonedRequest(pipeline, supersededReqId);
	}
	if (fireEos)
	{
		if (pipeline &&
		    !pipeline->haveData(
			    firebolt::rialto::MediaSourceStatus::EOS, eosReqId))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u", eosReqId);
		}
	}
	else if (stagedGated)
	{
		AAMPLOG_INFO("sourceId=%d requestId=%u staged while injectionGated=true - "
			"will be held until the gate clears (or discarded if superseded "
			"by a newer flush)",
			m_sourceId, requestId);
	}
	m_state.cv.notify_all();
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
		// NOTE: setSourcePosition() is intentionally NOT called here.
		// It is called from AampRialtoPlayer::OnSourceFlushed() after
		// the server confirms the flush via SourceFlushedEvent.  Calling
		// it here (while the server is still flushing) risks the SEGMENT
		// event being discarded, leaving Rialto's EOS state un-cleared
		// and causing an immediate END_OF_STREAM on the next play().
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
	double fpts,
	double fdts,
	double fDuration,
	double /*fragmentPTSoffset*/)
{
	if (!m_demuxer)
	{
		// HLS-TS ES path: the TSProcessor has already demuxed the TS into
		// individual ES frames and calls SendCopy() once per frame with
		// correct PTS/DTS/duration.  Construct a single AampMediaSample
		// from the raw ES buffer and inject it directly, bypassing the
		// fMP4 demuxer path which requires a boxed MP4 fragment.
		if (!waitForAttach())
		{
			AAMPLOG_INFO("Source not attached; dropping ES sample mediaType=%d",
				static_cast<int>(mediaType()));
			return true;
		}
		AampMediaSample sample;
		sample.mDataSize = buffer->size();
		// Aliasing constructor: mData points into the buffer while buffer's
		// ref-count keeps the storage alive.
		sample.mData = std::shared_ptr<const uint8_t>(
			buffer, buffer->data());
		sample.mPts      = fpts;
		sample.mDts      = fdts;
		sample.mDuration = fDuration;
		uint64_t capturedGen     = captureGeneration();
		auto     pendingCodecData = takePendingCodecData();
		injectOneSample(pipeline, capturedGen,
			std::move(sample), pendingCodecData, /*morePending=*/false);
		return true;
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
	size_t sampleIndex = 0;
	const size_t totalSamples = samples.size();
	for (auto &s : samples)
	{
		std::shared_ptr<firebolt::rialto::CodecData> codecData;
		if (firstSample)
		{
			codecData = pendingCodecData;
		}
		firstSample = false;
		++sampleIndex;
		bool morePending = (sampleIndex < totalSamples);
		if (!injectOneSample(pipeline, capturedGen, std::move(s), codecData, morePending))
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
	AampMediaSample &&sample,
	bool morePending)
{
	if (!waitForAttach())
	{
		return false;
	}
	uint64_t capturedGen = captureGeneration();
	auto pendingCodecData = takePendingCodecData();
	return injectOneSample(
		pipeline, capturedGen, std::move(sample), pendingCodecData, morePending);
}
