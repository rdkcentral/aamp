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
		m_state.batchHasFirstPts    = false;
		m_state.batchFirstPtsSec    = 0.0;
		m_state.batchDurationSecSum = 0.0;
		m_state.eos                 = false;
		// gateMode is intentionally left untouched here.  reset() runs
		// mid-Configure(), which can be followed by a second Flush() (e.g.
		// trickplay's Flush(pos=0) -> Configure() -> Flush(correctPos) ->
		// Stream()) before playback is meant to resume.  Clearing the gate
		// now would let an early needData slip through with stale-position
		// data.  The gate is cleared only via gateInjection(false), called
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

void AampRialtoMediaSource::unblockInjection(
	firebolt::rialto::IMediaPipeline *pipeline, const char *reason,
	std::optional<bool> newEosState)
{
	bool     fireNoAvailableSamples = false;
	uint32_t reqId                  = 0;
	GateMode previousMode           = GateMode::NONE;
	BatchSummary batch;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		previousMode = m_state.gateMode;
		++m_state.generation;
		m_state.gateMode = GateMode::DROPPED;
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
			fireNoAvailableSamples = claimPendingRequestLocked(reqId, batch);
		}
		// If an injector is active, it owns the request and will close
		// it out itself when it observes the generation change (see
		// injectOneSample()).
		m_state.cv.notify_all();
	}
	AAMPLOG_INFO("gateMode set to DROPPED (was %d) sourceId=%d mediaType=%d newEosState=%d reason=%s",
		static_cast<int>(previousMode), m_sourceId, static_cast<int>(mediaType()), newEosState.has_value() ? static_cast<int>(*newEosState) : -1, reason);
	if (fireNoAvailableSamples)
	{
		respondAbandonedRequest(pipeline, reqId, batch);
	}
	// Reset segment-start so the next injection establishes a fresh
	// baseline after the seek.  Written outside the lock (atomic).
	m_firstPtsMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
}

void AampRialtoMediaSource::gateInjection(
	firebolt::rialto::IMediaPipeline *pipeline, bool gate, const char *reason)
{
	GateMode previousMode = GateMode::NONE;
	GateMode newMode      = GateMode::NONE;
	bool     fireEos      = false;
	uint32_t reqId        = 0;
	BatchSummary batch;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		previousMode     = m_state.gateMode;
		m_state.gateMode = gate ? GateMode::BLOCKED : GateMode::NONE;
		newMode          = m_state.gateMode;
		if (!gate)
		{
			// Replay any EOS resolution that was deliberately deferred
			// while BLOCKED — see tryClaimEosLocked().
			fireEos = tryClaimEosLocked(reqId, batch);
		}
		m_state.cv.notify_all();
	}
	if (previousMode != newMode)
	{
		AAMPLOG_INFO("gateMode changed (%d->%d) sourceId=%d mediaType=%d reason=%s",
			static_cast<int>(previousMode), static_cast<int>(newMode),
			m_sourceId, static_cast<int>(mediaType()), reason);
	}
	if (fireEos)
	{
		if (!pipeline)
		{
			AAMPLOG_ERR("pipeline is null — cannot send deferred EOS for "
				"sourceId=%d", m_sourceId);
		}
		else if (!sendHaveData(*pipeline,
				firebolt::rialto::MediaSourceStatus::EOS, reqId, batch))
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

AampRialtoMediaSource::BatchSummary AampRialtoMediaSource::snapshotAndClearBatchLocked()
{
	BatchSummary summary;
	summary.frameCount     = m_state.segmentsAddedInBatch;
	summary.hasFirstPts    = m_state.batchHasFirstPts;
	summary.firstPtsSec    = m_state.batchFirstPtsSec;
	summary.durationSecSum = m_state.batchDurationSecSum;
	m_state.segmentsAddedInBatch = 0;
	m_state.batchHasFirstPts     = false;
	m_state.batchFirstPtsSec     = 0.0;
	m_state.batchDurationSecSum  = 0.0;
	return summary;
}

bool AampRialtoMediaSource::claimPendingRequestLocked(uint32_t &outReqId, BatchSummary &outBatch)
{
	bool claimed = false;
	if (m_state.hasPending)
	{
		outReqId           = m_state.pendingRequestId;
		outBatch           = snapshotAndClearBatchLocked();
		m_state.hasPending = false;
		claimed            = true;
	}
	return claimed;
}

bool AampRialtoMediaSource::tryClaimEosLocked(uint32_t &outReqId, BatchSummary &outBatch)
{
	bool claimed = false;
	if (m_state.eos && m_state.hasPending &&
	    !m_state.injectorActive && m_state.gateMode != GateMode::BLOCKED)
	{
		claimed = claimPendingRequestLocked(outReqId, outBatch);
	}
	return claimed;
}

void AampRialtoMediaSource::respondAbandonedRequest(
	firebolt::rialto::IMediaPipeline *pipeline, uint32_t reqId,
	const BatchSummary &batch)
{
	if (!pipeline)
	{
		// Pipeline already torn down by Stop() - nothing left to answer,
		// drop the stale request rather than treating this as an error.
		AAMPLOG_INFO("pipeline gone - dropping abandoned requestId=%u "
			"for sourceId=%d", reqId, m_sourceId);
	}
	else if (!sendHaveData(*pipeline,
			firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES,
			reqId, batch))
	{
		AAMPLOG_WARN("haveData(NO_AVAILABLE_SAMPLES) failed "
			"requestId=%u for sourceId=%d", reqId, m_sourceId);
	}
}

bool AampRialtoMediaSource::sendHaveData(
	firebolt::rialto::IMediaPipeline &pipeline,
	firebolt::rialto::MediaSourceStatus status,
	uint32_t requestId,
	const BatchSummary &batch)
{
	if (batch.hasFirstPts)
	{
		AAMPLOG_TRACE("sourceId=%d mediaType=%d status=%d requestId=%u "
			"frameCount=%zu firstPtsSec=%.3f durationSecSum=%.3f",
			m_sourceId, static_cast<int>(mediaType()),
			static_cast<int>(status), requestId,
			batch.frameCount, batch.firstPtsSec, batch.durationSecSum);
	}
	else
	{
		AAMPLOG_TRACE("sourceId=%d mediaType=%d status=%d requestId=%u "
			"frameCount=%zu",
			m_sourceId, static_cast<int>(mediaType()),
			static_cast<int>(status), requestId, batch.frameCount);
	}
	return pipeline.haveData(status, requestId);
}

int64_t AampRialtoMediaSource::firstPtsMs() const
{
	return m_firstPtsMs.load(std::memory_order_relaxed);
}

void AampRialtoMediaSource::setFirstPtsMs(int64_t ptsMs)
{
	AAMPLOG_INFO("firstPtsMs set to %" PRId64 " for sourceId=%d mediaType=%d",
		ptsMs, m_sourceId, static_cast<int>(mediaType()));
	m_firstPtsMs.store(ptsMs, std::memory_order_relaxed);
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
	// Do NOT include m_state.gateMode in the predicate: the
	// pipeline transitions through PAUSED during startup before the
	// video IPC call returns.  Including gateMode would
	// immediately wake this thread, find isAttached()==false, and
	// discard the frame.  The generation change (set by
	// unblockInjection() inside Flush/Stop) is the correct abort.
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
	if (!mapCodecToMime(codecInfo.mCodecFormat, codecInfo.mNaluLengthPrefixed,
		mimeType, streamFormat))
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

	while (!done)
	{
		bool     bailedOut    = false;
		bool     abortPending = false;
		uint32_t abortReqId   = 0;
		BatchSummary abortBatch;
		bool     regated      = false;

		// Stage 1: block here — rather than dropping the sample — while
		// gateMode is BLOCKED for the current generation (Stream() was
		// called while a flush was still in progress, so play() is
		// deferred and the gate has not been cleared yet: see
		// AampRialtoPlayer::Stream()).  This is genuinely-wanted playback
		// data, not stale data, so it must not be discarded merely because
		// it arrived a little early.  If gateMode is DROPPED
		// (NotifyInjectorToPause()/Stop()/destruction — see
		// unblockInjection()), the sample is discarded immediately without
		// ever blocking this thread, mirroring GStreamer's
		// InterfacePlayerRDK::mPauseInjector drop semantics.
		{
			std::unique_lock<std::mutex> lock(m_state.mu);
			m_state.cv.wait(lock, [&]{
				return m_state.generation != capturedGen ||
				       m_state.gateMode != GateMode::BLOCKED;
			});
			if (m_state.generation != capturedGen ||
			    m_state.gateMode == GateMode::DROPPED)
			{
				// Superseded by a newer flush, or injection has been
				// switched to DROPPED mode while waiting for the gate to
				// clear — discard the sample and never block on it.
				abortPending = claimPendingRequestLocked(abortReqId, abortBatch);
				m_state.injectorActive = false;
				bailedOut = true;
			}
		}
		if (bailedOut)
		{
			AAMPLOG_INFO("injector bailing out while waiting for ungate "
				"sourceId=%d - superseded by a newer flush or DROPPED mode, "
				"abandonedRequestId=%u",
				m_sourceId, abortPending ? abortReqId : 0);
			if (abortPending)
			{
				respondAbandonedRequest(&pipeline, abortReqId, abortBatch);
			}
			done = true;
			continue;
		}

		// Stage 2: wait for a needData request.  Also wakes if gateMode
		// is (re-)set to BLOCKED before one arrives (e.g. Configure()'s
		// gateInjection(true), which — unlike unblockInjection() — does
		// not bump generation) so this injector loops back to stage 1
		// instead of injecting into a session that is no longer ready.
		uint32_t reqId = 0;
		{
			std::unique_lock<std::mutex> lock(m_state.mu);
			m_state.cv.wait(lock, [&]{
				return m_state.generation != capturedGen ||
				       m_state.hasPending ||
				       m_state.gateMode == GateMode::BLOCKED;
			});
			if (m_state.generation != capturedGen)
			{
				// This injector is unconditionally abandoning its slot.
				// unblockInjection() deferred responsibility to us
				// (we were the active injector) — if a request is still
				// pending, no other path will ever close it out, so we
				// must respond ourselves once the lock is released.
				abortPending = claimPendingRequestLocked(abortReqId, abortBatch);
				m_state.injectorActive = false;
				bailedOut = true;
			}
			else if (m_state.hasPending)
			{
				reqId = m_state.pendingRequestId;
			}
			else
			{
				regated = true;
			}
		}

		if (bailedOut)
		{
			AAMPLOG_INFO("injector bailing out sourceId=%d generation changed - "
				"abandonedRequestId=%u",
				m_sourceId, abortPending ? abortReqId : 0);
			if (abortPending)
			{
				respondAbandonedRequest(&pipeline, abortReqId, abortBatch);
			}
			done = true;
			continue;
		}

		if (regated)
		{
			continue;
		}

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
				BatchSummary respBatch;
				{
					std::lock_guard<std::mutex> lock(m_state.mu);
					if (m_state.hasPending &&
					    m_state.pendingRequestId == reqId)
					{
						respReqId                     = reqId;
						wasEos                        = m_state.eos;
						m_state.hasPending            = false;
						respBatch                     = snapshotAndClearBatchLocked();
						sendResponse                   = true;
					}
					m_state.injectorActive = false;
				}
				if (sendResponse)
				{
					const auto status = wasEos
						? firebolt::rialto::MediaSourceStatus::EOS
						: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
					if (!sendHaveData(pipeline, status, respReqId, respBatch))
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
						capturedGen, reqId, morePending, sample.mPts,
						sample.mDuration);
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
	bool         sendHaveData = false;
	uint32_t     claimedReqId = 0;
	BatchSummary batch;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		if (m_state.generation == capturedGen &&
		    m_state.hasPending &&
		    m_state.pendingRequestId == reqId)
		{
			sendHaveData = claimPendingRequestLocked(claimedReqId, batch);
		}
	}
	if (sendHaveData)
	{
		const auto status = batch.frameCount > 0
			? firebolt::rialto::MediaSourceStatus::OK
			: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
		if (!this->sendHaveData(pipeline, status, reqId, batch))
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
	double samplePts,
	double sampleDurationSec)
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
	BatchSummary batch;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		if (m_state.generation == capturedGen &&
		    m_state.hasPending &&
		    m_state.pendingRequestId == reqId)
		{
			++m_state.segmentsAddedInBatch;
			if (!m_state.batchHasFirstPts)
			{
				m_state.batchHasFirstPts = true;
				m_state.batchFirstPtsSec = samplePts;
			}
			m_state.batchDurationSecSum += sampleDurationSec;
			if (m_state.eos)
			{
				// Last sample — signal EOS to Rialto.
				m_state.hasPending = false;
				batch              = snapshotAndClearBatchLocked();
				sendHaveData        = true;
				sendEos             = true;
			}
			else if (m_state.segmentsAddedInBatch >=
			         m_state.pendingFrameCount || !morePending)
			{
				// Send haveData when we've reached the requested frame count
				// OR when morePending is false (last sample in the batch)
				m_state.hasPending = false;
				batch              = snapshotAndClearBatchLocked();
				sendHaveData        = true;
			}
		}
		else if (m_state.hasPending && m_state.pendingRequestId == reqId)
		{
			// Generation changed (Flush/Stop/unblockInjection) while
			// addSegment() was in flight for this request, but nothing has
			// claimed it since — unblockInjection() left it for us (the
			// active injector at the time) to close out.  Answer it now
			// rather than leaking it forever.
			m_state.hasPending = false;
			batch              = snapshotAndClearBatchLocked();
			sendAbandoned       = true;
		}
		m_state.injectorActive = false;
	}
	if (sendHaveData)
	{
		auto status = sendEos
			? firebolt::rialto::MediaSourceStatus::EOS
			: firebolt::rialto::MediaSourceStatus::OK;
		if (!this->sendHaveData(pipeline, status, reqId, batch))
		{
			AAMPLOG_WARN("haveData failed requestId=%u",
				reqId);
		}
	}
	else if (sendAbandoned)
	{
		respondAbandonedRequest(&pipeline, reqId, batch);
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
	BatchSummary batch;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		m_state.eos = true;
		// Deliberately gate-aware: if gateMode is BLOCKED, resolution is
		// deferred until gateInjection(false) replays it, so EOS is never
		// answered ahead of samples still blocked behind the gate — see
		// tryClaimEosLocked().
		fireEos = tryClaimEosLocked(reqId, batch);
		// If injectorActive or gateMode==BLOCKED, the pending request is
		// left for the injector (on completion) or gateInjection(false)
		// (on ungate) to send haveData(EOS).
	}
	if (fireEos)
	{
		if (!pipeline)
		{
			AAMPLOG_ERR("pipeline is null — cannot send EOS for sourceId=%d",
				m_sourceId);
		}
		else if (!sendHaveData(*pipeline,
				firebolt::rialto::MediaSourceStatus::EOS, reqId, batch))
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
	BatchSummary eosBatch;
	bool     rejectGated       = false;
	bool     supersededPending = false;
	uint32_t supersededReqId   = 0;
	BatchSummary supersededBatch;
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		// If a previous needData was staged but never claimed, it must be
		// closed out here rather than silently overwritten.  Dropping it
		// silently would leave Rialto's own bookkeeping for that requestId
		// unanswered, which desyncs AAMP and Rialto once the newer request
		// is eventually served.
		if (m_state.hasPending)
		{
			supersededPending = true;
			supersededReqId   = m_state.pendingRequestId;
			supersededBatch   = snapshotAndClearBatchLocked();
		}
		rejectGated = m_state.gateMode != GateMode::NONE;
		if (rejectGated)
		{
			// Rialto treats any needData issued while a flush/seek is
			// still outstanding (i.e. before its own SEEK_DONE/play() is
			// confirmed) as stale on its side.  Attaching real sample data
			// to this requestId would therefore be silently discarded by
			// Rialto, permanently losing that sample.  Answer it
			// immediately with NO_AVAILABLE_SAMPLES instead of staging it
			// as pending; Rialto is expected to issue a fresh needData once
			// playback is genuinely ready to resume (i.e. once the gate is
			// cleared via gateInjection(false)).  gateMode is intentionally
			// NOT cleared here — see the gateMode field comment for why.
			m_state.hasPending = false;
		}
		else
		{
			m_state.hasPending           = true;
			m_state.pendingRequestId     = requestId;
			m_state.pendingFrameCount    = std::max<size_t>(frameCount, 1);

			// Attempt to resolve it immediately as EOS.
			fireEos = tryClaimEosLocked(eosReqId, eosBatch);
		}
	}
	if (supersededPending)
	{
		AAMPLOG_WARN("sourceId=%d requestId=%u superseded stale unclaimed "
			"requestId=%u before it was ever answered - closing it out "
			"with NO_AVAILABLE_SAMPLES",
			m_sourceId, requestId, supersededReqId);
		respondAbandonedRequest(pipeline, supersededReqId, supersededBatch);
	}
	if (rejectGated)
	{
		AAMPLOG_INFO("sourceId=%d requestId=%u received while "
			"gateMode!=NONE - answering NO_AVAILABLE_SAMPLES "
			"immediately (Rialto would treat data on this request as "
			"stale pre-SEEK_DONE)",
			m_sourceId, requestId);
		// This request was rejected in the same call that created it, so it
		// never had any segments added — always an empty batch.
		respondAbandonedRequest(pipeline, requestId, BatchSummary{});
	}
	else if (fireEos)
	{
		if (pipeline &&
		    !sendHaveData(*pipeline,
			    firebolt::rialto::MediaSourceStatus::EOS, eosReqId, eosBatch))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u", eosReqId);
		}
	}
	m_state.cv.notify_all();
}

void AampRialtoMediaSource::handleCancelNeedData()
{
	AAMPLOG_INFO("sourceId=%d", m_sourceId);
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		m_state.hasPending = false;
		snapshotAndClearBatchLocked();
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
