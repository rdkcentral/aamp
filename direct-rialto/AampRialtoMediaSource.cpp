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
#include <cmath>
#include <limits>
#include <utility>

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

/// Bytes of DRM metadata (key id + IV + subsample table) a sample
/// contributes to its segment, for batch byte-accounting/logging only.
/// Mirrors the encrypted-segment condition used by annotateEncryption().
size_t drmMetadataBytes(const AampMediaSample &sample, int32_t mksId)
{
	if (!sample.mDrmMetadata.mIsEncrypted || mksId < 0)
	{
		return 0;
	}
	return sample.mDrmMetadata.mKeyId.size() +
	       sample.mDrmMetadata.mIV.size() +
	       sample.mDrmMetadata.mSubSamples.size();
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
	m_handshake.Reset();
	m_sourceId.store(-1, std::memory_order_release);
	m_mksId.store(-1, std::memory_order_release);
	m_activeProtection.reset();
	{
		std::lock_guard<std::mutex> lock(m_codecDataMutex);
		m_pendingCodecData = nullptr;
	}

	m_firstPtsMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
	m_lastAcceptedEndMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
	m_acceptedFrames.store(0, std::memory_order_relaxed);
	m_droppedSampleCount.store(0, std::memory_order_relaxed);
	m_needDataCount.store(0, std::memory_order_relaxed);
}

void AampRialtoMediaSource::unblockInjection(
	firebolt::rialto::IMediaPipeline *pipeline, const char *reason,
	std::optional<bool> newEosState)
{
	GateMode previousMode = GateMode::NONE;
	const auto abandoned = m_handshake.Unblock(newEosState, previousMode);
	AAMPLOG_INFO("gateMode set to DROPPED (was %d) sourceId=%d mediaType=%d newEosState=%d reason=%s",
		static_cast<int>(previousMode), sourceId(), static_cast<int>(mediaType()), newEosState.has_value() ? static_cast<int>(*newEosState) : -1, reason);
	if (abandoned)
	{
		respondAbandonedRequest(pipeline, abandoned->requestId, abandoned->batch);
	}
	// Reset segment-start so the next injection establishes a fresh
	// baseline after the seek.
	m_firstPtsMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
	m_lastAcceptedEndMs.store(kFirstPtsNotSet, std::memory_order_relaxed);
	m_acceptedFrames.store(0, std::memory_order_relaxed);
	m_needDataCount.store(0, std::memory_order_relaxed);
}

void AampRialtoMediaSource::gateInjection(
	firebolt::rialto::IMediaPipeline *pipeline, bool gate, const char *reason)
{
	GateMode previousMode = GateMode::NONE;
	GateMode newMode      = GateMode::NONE;
	const auto deferredEos = m_handshake.SetGate(gate, previousMode, newMode);
	if (previousMode != newMode)
	{
		AAMPLOG_INFO("gateMode changed (%d->%d) sourceId=%d mediaType=%d reason=%s",
			static_cast<int>(previousMode), static_cast<int>(newMode),
			sourceId(), static_cast<int>(mediaType()), reason);
	}
	if (deferredEos)
	{
		if (!pipeline)
		{
			AAMPLOG_ERR("pipeline is null — cannot send deferred EOS for "
				"sourceId=%d", sourceId());
		}
		else if (!sendHaveData(*pipeline,
				firebolt::rialto::MediaSourceStatus::EOS,
				deferredEos->requestId, deferredEos->batch))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u",
				deferredEos->requestId);
		}
	}
}

void AampRialtoMediaSource::setEos(bool eos, const char *reason)
{
	if (m_handshake.SetEos(eos))
	{
		AAMPLOG_INFO("eos (%s->%s) sourceId=%d mediaType=%d reason=%s",
			eos ? "false" : "true", eos ? "true" : "false",
			sourceId(), static_cast<int>(mediaType()), reason);
	}
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
			"for sourceId=%d", reqId, sourceId());
	}
	else if (!sendHaveData(*pipeline,
			firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES,
			reqId, batch))
	{
		AAMPLOG_WARN("haveData(NO_AVAILABLE_SAMPLES) failed "
			"requestId=%u for sourceId=%d", reqId, sourceId());
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
			"frameCount=%zu firstPtsSec=%.3f durationSecSum=%.3f "
			"mediaBytes=%zu metadataBytes=%zu",
			sourceId(), static_cast<int>(mediaType()),
			static_cast<int>(status), requestId,
			batch.frameCount, batch.firstPtsSec, batch.durationSecSum,
			batch.mediaBytes, batch.metadataBytes);
	}
	else
	{
		AAMPLOG_TRACE("sourceId=%d mediaType=%d status=%d requestId=%u "
			"frameCount=%zu mediaBytes=%zu metadataBytes=%zu",
			sourceId(), static_cast<int>(mediaType()),
			static_cast<int>(status), requestId, batch.frameCount,
			batch.mediaBytes, batch.metadataBytes);
	}
	return pipeline.haveData(status, requestId);
}

int64_t AampRialtoMediaSource::firstPtsMs() const
{
	return m_firstPtsMs.load(std::memory_order_relaxed);
}

int64_t AampRialtoMediaSource::injectedSpanMs() const
{
	const int64_t first = m_firstPtsMs.load(std::memory_order_relaxed);
	const int64_t end = m_lastAcceptedEndMs.load(std::memory_order_relaxed);
	int64_t span = 0;
	if ((first != kFirstPtsNotSet) && (end != kFirstPtsNotSet) && (end > first))
	{
		span = end - first;
	}
	return span;
}

uint32_t AampRialtoMediaSource::acceptedFrames() const
{
	return m_acceptedFrames.load(std::memory_order_relaxed);
}

bool AampRialtoMediaSource::bufferFull() const
{
	return m_handshake.BufferFull();
}

bool AampRialtoMediaSource::waitForAttach()
{
	if (isAttached())
	{
		return true;  // fast path — already attached
	}
	const auto result = m_handshake.WaitForAttach(
		[this]{ return isAttached(); },
		[this]{
			AAMPLOG_INFO(
				"Inject blocked — source awaiting deferred attach mediaType=%d",
				static_cast<int>(mediaType()));
		});
	if (result == AampRialtoNeedDataHandshake::AttachWait::Aborted)
	{
		AAMPLOG_INFO(
			"Attach did not complete (abort) — discarding mediaType=%d",
			static_cast<int>(mediaType()));
	}
	return result == AampRialtoNeedDataHandshake::AttachWait::Attached;
}

bool AampRialtoMediaSource::isEos()
{
	return m_handshake.IsEos();
}

uint64_t AampRialtoMediaSource::captureGeneration()
{
	return m_handshake.Generation();
}

// ---------------------------------------------------------------------------
// Codec data
// ---------------------------------------------------------------------------

std::shared_ptr<firebolt::rialto::CodecData>
AampRialtoMediaSource::takePendingCodecData()
{
	std::lock_guard<std::mutex> lock(m_codecDataMutex);
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
	{
		std::lock_guard<std::mutex> lock(m_codecDataMutex);
		m_pendingCodecData = codecData;
	}

	// 5. If already attached → update only.  A new period's init segment
	//    can carry genuinely new protection params (different DRM system or
	//    PSSH/init data), which must be applied to the existing session —
	//    otherwise every subsequent sample keeps stamping the stale mksId.
	//    Compare against the params last used to create a session so
	//    unchanged/repeated params (re-queued on every period regardless of
	//    whether they changed) do not trigger a redundant OCDM license
	//    round-trip.
	if (sourceId() >= 0)
	{
		if (protection.has_value() && !(m_activeProtection == protection))
		{
			if (!drmBridge)
			{
				AAMPLOG_ERR("Protection params changed but drmBridge is null for"
					" mediaType=%d — DRM session will not be updated",
					static_cast<int>(mediaType()));
			}
			else
			{
				const auto &prot = *protection;
				const int32_t newMksId = drmBridge->createSession(
					prot.systemId.c_str(),
					prot.initData.data(),
					prot.initData.size(),
					prot.type);
				if (newMksId < 0)
				{
					AAMPLOG_WARN("createSession failed while updating mediaType=%d"
						" — keeping previous mksId=%d",
						static_cast<int>(mediaType()), mksId());
				}
				else
				{
					AAMPLOG_INFO("createSession returned new mksId=%d (was %d) for"
						" mediaType=%d", newMksId, mksId(),
						static_cast<int>(mediaType()));
					m_mksId.store(newMksId, std::memory_order_release);
					m_activeProtection = protection;
				}
			}
		}
		AAMPLOG_INFO("source already attached (id=%d) for mediaType=%d, "
			"staged new codec data",
			sourceId(), static_cast<int>(mediaType()));
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
		m_mksId.store(drmBridge->createSession(
			prot.systemId.c_str(),
			prot.initData.data(),
			prot.initData.size(),
			prot.type), std::memory_order_release);
		if (mksId() < 0)
		{
			AAMPLOG_WARN("createSession failed for mediaType=%d",
				static_cast<int>(mediaType()));
		}
		else
		{
			AAMPLOG_INFO("createSession returned mksId=%d for mediaType=%d",
				mksId(), static_cast<int>(mediaType()));
			m_activeProtection = protection;
		}
	}

	// 7. Create the Rialto source object (virtual — video/audio differ)
	auto source = createRialtoSource(
		mimeType, mksId() >= 0, codecInfo, streamFormat, codecData);
	std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource>
		sourceBase = std::move(source);
	if (!pipeline.attachSource(sourceBase))
	{
		AAMPLOG_ERR("attachSource failed for mediaType=%d",
			static_cast<int>(mediaType()));
		return AttachResult::FAILED;
	}

	m_sourceId.store(sourceBase->getId(), std::memory_order_release);
	AAMPLOG_INFO("Attached source id=%d mime=%s mediaType=%d",
		sourceId(), mimeType.c_str(), static_cast<int>(mediaType()));

	// 8. Set initial segment position if a flush was staged
	if (flushPosNs >= 0)
	{
		if (!pipeline.setSourcePosition(
				sourceId(), flushPosNs, /*resetTime=*/true, appliedRate))
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
	using Wait = AampRialtoNeedDataHandshake::Wait;
	bool injected = false;
	bool done = !m_handshake.BeginInject();

	while (!done)
	{
		// Stage 1: hold — rather than drop — genuinely-wanted data while the
		// gate is BLOCKED (Stream() called while a flush is still in
		// progress, see AampRialtoPlayer::Stream()).  In DROPPED mode
		// (NotifyInjectorToPause()/Stop()/destruction) the sample is
		// discarded without blocking, mirroring GStreamer's
		// InterfacePlayerRDK::mPauseInjector.
		const auto gate = m_handshake.WaitForGate(capturedGen);
		if (gate.outcome == Wait::Abort)
		{
			AAMPLOG_INFO("injector bailing out while waiting for ungate "
				"sourceId=%d - superseded by a newer flush or DROPPED mode, "
				"abandonedRequestId=%u",
				sourceId(), gate.abandoned ? gate.abandoned->requestId : 0);
			if (gate.abandoned)
			{
				respondAbandonedRequest(&pipeline, gate.abandoned->requestId,
					gate.abandoned->batch);
			}
			done = true;
			continue;
		}

		// Stage 2: wait for a needData request.  A gate re-set to BLOCKED
		// first (Configure()'s gateInjection(true), which does not bump the
		// generation) sends this injector back to stage 1.
		const auto request = m_handshake.WaitForRequest(capturedGen);
		if (request.outcome == Wait::Abort)
		{
			AAMPLOG_INFO("injector bailing out sourceId=%d generation changed - "
				"abandonedRequestId=%u",
				sourceId(), request.abandoned ? request.abandoned->requestId : 0);
			if (request.abandoned)
			{
				respondAbandonedRequest(&pipeline,
					request.abandoned->requestId, request.abandoned->batch);
			}
			done = true;
			continue;
		}
		if (request.outcome == Wait::GateReasserted)
		{
			continue;
		}
		const uint32_t reqId = request.requestId;

		// Build the segment outside the lock (polymorphic).
		auto segment = createSegment(sample);
		// A sample that cannot be injected correctly is dropped the same
		// way as one whose segment could not be created.
		if (segment &&
		    sample.mDataSize > std::numeric_limits<uint32_t>::max())
		{
			AAMPLOG_ERR("Dropping sample for sourceId=%d: %zu bytes do not "
				"fit the 32-bit segment size", sourceId(), sample.mDataSize);
			segment.reset();
		}
		else if (segment && !annotateEncryption(sample, *segment))
		{
			segment.reset();
		}
		if (!segment)
		{
			AAMPLOG_WARN("no segment for sourceId=%d (creation failed or "
				"sample rejected)", sourceId());
			// Answer the request if we still own it: nothing else will.
			const auto drop = m_handshake.DropSample(reqId);
			if (drop.claim)
			{
				const auto status = drop.eos
					? firebolt::rialto::MediaSourceStatus::EOS
					: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
				if (!sendHaveData(pipeline, status, drop.claim->requestId,
						drop.claim->batch))
				{
					AAMPLOG_WARN("haveData failed requestId=%u",
						drop.claim->requestId);
				}
			}
			done = true;
			continue;
		}

		if (codecData)
		{
			segment->setCodecData(codecData);
		}
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
				sample.mDuration, sample.mDataSize,
				drmMetadataBytes(sample, mksId()));
			injected = true;
			done     = true;
		}
	}

	return injected;
}

// ---------------------------------------------------------------------------
// injectOneSample helpers
// ---------------------------------------------------------------------------

bool AampRialtoMediaSource::annotateEncryption(
	const AampMediaSample &sample,
	firebolt::rialto::IMediaPipeline::MediaSegment &segment) const
{
	constexpr size_t kSubSampleEntrySize = 6;
	constexpr uint32_t kDropLogInterval = 100;

	const auto &raw = sample.mDrmMetadata.mSubSamples;
	const char *rejection = nullptr;
	if (sample.mDrmMetadata.mIsEncrypted && mksId() < 0)
	{
		rejection = "no DRM session";
	}
	else if (sample.mDrmMetadata.mIsEncrypted &&
	         raw.size() < sample.mDrmMetadata.mNumSubSamples * kSubSampleEntrySize)
	{
		rejection = "sub-sample table shorter than declared";
	}

	if (rejection != nullptr)
	{
		const uint32_t dropped =
			m_droppedSampleCount.fetch_add(1, std::memory_order_relaxed) + 1;
		if (dropped == 1 || (dropped % kDropLogInterval) == 0)
		{
			AAMPLOG_ERR("Dropping encrypted sample for sourceId=%d: %s "
				"(mksId=%d numSubSamples=%u tableBytes=%zu, %u dropped)",
				sourceId(), rejection, mksId(),
				static_cast<unsigned>(sample.mDrmMetadata.mNumSubSamples),
				raw.size(), dropped);
		}
		return false;
	}

	if (sample.mDrmMetadata.mIsEncrypted && mksId() >= 0)
	{
		segment.setEncrypted(true);
		segment.setMediaKeySessionId(mksId());
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
			sourceId(), mksId(),
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
			for (uint32_t s = 0;
			     s < sample.mDrmMetadata.mNumSubSamples; ++s)
			{
				const size_t offset = s * kSubSampleEntrySize;
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
	if (!sample.mDrmMetadata.mIsEncrypted && mksId() >= 0)
	{
		AAMPLOG_TRACE("Unencrypted sample for sourceId=%d with active DRM "
			"session (mksId=%d) — segment sent without encryption metadata",
			sourceId(), mksId());
	}
	return true;
}

void AampRialtoMediaSource::handleAddSegmentNoSpace(
	firebolt::rialto::IMediaPipeline &pipeline,
	uint64_t capturedGen,
	uint32_t reqId)
{
	const auto claim = m_handshake.OnNoSpace(capturedGen, reqId);
	AampRialtoNeedDataHandshake::BatchSummary batch;
	if (claim)
	{
		batch = claim->batch;
		const auto status = batch.frameCount > 0
			? firebolt::rialto::MediaSourceStatus::OK
			: firebolt::rialto::MediaSourceStatus::NO_AVAILABLE_SAMPLES;
		if (!sendHaveData(pipeline, status, reqId, batch))
		{
			AAMPLOG_WARN("haveData failed requestId=%u", reqId);
		}
	}
	AAMPLOG_INFO("addSegment NO_SPACE sourceId=%d mediaType=%d requestId=%u "
		"acceptedThisBatch=%zu mediaBytes=%zu acceptedFrames=%u "
		"— waiting for next needData",
		sourceId(), static_cast<int>(mediaType()), reqId,
		batch.frameCount, batch.mediaBytes, acceptedFrames());
}

void AampRialtoMediaSource::handleAddSegmentCompletion(
	firebolt::rialto::IMediaPipeline &pipeline,
	firebolt::rialto::AddSegmentStatus addStatus,
	uint64_t capturedGen,
	uint32_t reqId,
	bool morePending,
	double samplePts,
	double sampleDurationSec,
	size_t sampleMediaBytes,
	size_t sampleMetadataBytes)
{
	using Reply = AampRialtoNeedDataHandshake::Reply;
	if (addStatus == firebolt::rialto::AddSegmentStatus::OK)
	{
		const int64_t ptsMs = SecondsToMs(samplePts);
		int64_t expected = kFirstPtsNotSet;
		if (m_firstPtsMs.compare_exchange_strong(
				expected, ptsMs, std::memory_order_relaxed))
		{
			AAMPLOG_INFO("firstPtsMs set to %" PRId64 " for sourceId=%d",
				ptsMs, sourceId());
		}
		m_lastAcceptedEndMs.store(
			ptsMs + SecondsToMs(sampleDurationSec),
			std::memory_order_relaxed);
		m_acceptedFrames.fetch_add(1, std::memory_order_relaxed);
	}
	else
	{
		AAMPLOG_WARN("addSegment failed sourceId=%d "
			"requestId=%u status=%d",
			sourceId(), reqId, static_cast<int>(addStatus));
	}

	const auto completion = m_handshake.Complete(capturedGen, reqId,
		morePending, {samplePts, sampleDurationSec, sampleMediaBytes,
		sampleMetadataBytes});
	if (completion.reply == Reply::Ok || completion.reply == Reply::Eos)
	{
		const auto status = (completion.reply == Reply::Eos)
			? firebolt::rialto::MediaSourceStatus::EOS
			: firebolt::rialto::MediaSourceStatus::OK;
		if (!sendHaveData(pipeline, status, reqId, completion.batch))
		{
			AAMPLOG_WARN("haveData failed requestId=%u", reqId);
		}
	}
	else if (completion.reply == Reply::Abandoned)
	{
		respondAbandonedRequest(&pipeline, reqId, completion.batch);
	}
}

// ---------------------------------------------------------------------------
// signalEos
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::signalEos(
	firebolt::rialto::IMediaPipeline *pipeline)
{
	const auto claim = m_handshake.SignalEos();
	if (claim)
	{
		if (!pipeline)
		{
			AAMPLOG_ERR("pipeline is null — cannot send EOS for sourceId=%d",
				sourceId());
		}
		else if (!sendHaveData(*pipeline,
				firebolt::rialto::MediaSourceStatus::EOS,
				claim->requestId, claim->batch))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u", claim->requestId);
		}
	}
	m_handshake.WakeWaiters();
}

// ---------------------------------------------------------------------------
// handleNeedData / handleCancelNeedData
// ---------------------------------------------------------------------------

void AampRialtoMediaSource::handleNeedData(
	size_t frameCount,
	uint32_t requestId,
	firebolt::rialto::IMediaPipeline *pipeline)
{
	// Need-data arrives many times a second, so only the first request after a
	// reset or flush and a periodic summary are logged at INFO.
	const uint32_t requestCount =
		m_needDataCount.fetch_add(1, std::memory_order_relaxed) + 1;
	const bool logSummary =
		(requestCount == 1) || (requestCount % kNeedDataLogInterval == 0);
	if (logSummary)
	{
		AAMPLOG_INFO("needData #%u sourceId=%d mediaType=%d frameCount=%zu "
			"requestId=%u acceptedFrames=%u injectedSpanMs=%" PRId64
			" bufferFull=%d",
			requestCount, sourceId(), static_cast<int>(mediaType()),
			frameCount, requestId, acceptedFrames(), injectedSpanMs(),
			static_cast<int>(bufferFull()));
	}
	else
	{
		AAMPLOG_TRACE("sourceId=%d frameCount=%zu requestId=%u",
			sourceId(), frameCount, requestId);
	}

	const auto result = m_handshake.OnNeedData(frameCount, requestId);
	if (result.superseded)
	{
		AAMPLOG_WARN("sourceId=%d requestId=%u superseded stale unclaimed "
			"requestId=%u before it was ever answered - closing it out "
			"with NO_AVAILABLE_SAMPLES",
			sourceId(), requestId, result.superseded->requestId);
		respondAbandonedRequest(pipeline, result.superseded->requestId,
			result.superseded->batch);
	}
	if (result.rejected)
	{
		AAMPLOG_INFO("sourceId=%d requestId=%u received while "
			"gateMode!=NONE - answering NO_AVAILABLE_SAMPLES "
			"immediately (Rialto would treat data on this request as "
			"stale pre-SEEK_DONE)",
			sourceId(), requestId);
		// Rejected in the call that created it, so the batch is empty.
		respondAbandonedRequest(pipeline, requestId, BatchSummary{});
	}
	else if (result.eos)
	{
		if (pipeline &&
		    !sendHaveData(*pipeline,
			    firebolt::rialto::MediaSourceStatus::EOS,
			    result.eos->requestId, result.eos->batch))
		{
			AAMPLOG_WARN("haveData(EOS) failed requestId=%u",
				result.eos->requestId);
		}
	}
	m_handshake.WakeWaiters();
}

void AampRialtoMediaSource::handleCancelNeedData()
{
	AAMPLOG_INFO("sourceId=%d", sourceId());
	m_handshake.OnCancelNeedData();
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
