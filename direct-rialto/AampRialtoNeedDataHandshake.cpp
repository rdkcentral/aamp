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
 * @file AampRialtoNeedDataHandshake.cpp
 * @brief needData/haveData handshake state and decisions.
 */

#include "AampRialtoNeedDataHandshake.h"

#include <algorithm>

using Handshake = AampRialtoNeedDataHandshake;

// ---------------------------------------------------------------------------
// Helpers (caller holds m_state.mu)
// ---------------------------------------------------------------------------

Handshake::BatchSummary Handshake::SnapshotAndClearBatchLocked()
{
	BatchSummary summary;
	summary.frameCount     = m_state.segmentsAddedInBatch;
	summary.hasFirstPts    = m_state.batchHasFirstPts;
	summary.firstPtsSec    = m_state.batchFirstPtsSec;
	summary.durationSecSum = m_state.batchDurationSecSum;
	summary.mediaBytes     = m_state.batchMediaBytesSum;
	summary.metadataBytes  = m_state.batchMetadataBytesSum;
	m_state.segmentsAddedInBatch  = 0;
	m_state.batchHasFirstPts      = false;
	m_state.batchFirstPtsSec      = 0.0;
	m_state.batchDurationSecSum   = 0.0;
	m_state.batchMediaBytesSum    = 0;
	m_state.batchMetadataBytesSum = 0;
	return summary;
}

std::optional<Handshake::Claim> Handshake::ClaimPendingLocked()
{
	std::optional<Claim> claim;
	if (m_state.hasPending)
	{
		claim = Claim{m_state.pendingRequestId, SnapshotAndClearBatchLocked()};
		m_state.hasPending = false;
	}
	return claim;
}

std::optional<Handshake::Claim> Handshake::TryClaimEosLocked()
{
	std::optional<Claim> claim;
	if (m_state.eos && m_state.hasPending &&
	    !m_state.injectorActive && m_state.gateMode != GateMode::BLOCKED)
	{
		claim = ClaimPendingLocked();
	}
	return claim;
}

// ---------------------------------------------------------------------------
// Control thread
// ---------------------------------------------------------------------------

void Handshake::Reset()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	++m_state.generation;
	m_state.hasPending = false;
	SnapshotAndClearBatchLocked();
	m_state.eos            = false;
	m_state.injectorActive = false;
	m_state.attachPending  = false;
	m_bufferFull.store(false, std::memory_order_relaxed);
	m_state.cv.notify_all();
}

std::optional<Handshake::Claim> Handshake::Unblock(
	std::optional<bool> newEos, GateMode &previous)
{
	std::optional<Claim> claim;
	std::lock_guard<std::mutex> lock(m_state.mu);
	previous = m_state.gateMode;
	++m_state.generation;
	m_state.gateMode = GateMode::DROPPED;
	if (newEos.has_value())
	{
		// With the generation bump, so a concurrent needData/SignalEos never
		// sees the new gate with the old EOS value (or the reverse).
		m_state.eos = *newEos;
	}
	if (!m_state.injectorActive)
	{
		// Nobody else will answer a request that arrived just before
		// Flush()/Stop().  An active injector answers its own once it sees
		// the generation change.
		claim = ClaimPendingLocked();
	}
	m_bufferFull.store(false, std::memory_order_relaxed);
	m_state.cv.notify_all();
	return claim;
}

std::optional<Handshake::Claim> Handshake::SetGate(bool gate,
	GateMode &previous, GateMode &current)
{
	std::optional<Claim> claim;
	std::lock_guard<std::mutex> lock(m_state.mu);
	previous = m_state.gateMode;
	m_state.gateMode = gate ? GateMode::BLOCKED : GateMode::NONE;
	current = m_state.gateMode;
	if (!gate)
	{
		claim = TryClaimEosLocked();
	}
	m_state.cv.notify_all();
	return claim;
}

bool Handshake::SetEos(bool eos)
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	const bool changed = (m_state.eos != eos);
	if (changed)
	{
		m_state.eos = eos;
		m_state.cv.notify_all();
	}
	return changed;
}

std::optional<Handshake::Claim> Handshake::SignalEos()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	m_state.eos = true;
	// If an injector is active or the gate is BLOCKED, the injector (on
	// completion) or SetGate(false) answers with EOS instead.
	return TryClaimEosLocked();
}

// ---------------------------------------------------------------------------
// Rialto event thread
// ---------------------------------------------------------------------------

Handshake::NeedDataResult Handshake::OnNeedData(size_t frameCount,
	uint32_t requestId)
{
	NeedDataResult result;
	std::lock_guard<std::mutex> lock(m_state.mu);
	// Overwriting an unanswered request would leave it open on Rialto's side
	// and desynchronise the two once the newer request is served.
	result.superseded = ClaimPendingLocked();
	result.rejected = (m_state.gateMode != GateMode::NONE);
	if (!result.rejected)
	{
		m_state.hasPending        = true;
		m_state.pendingRequestId  = requestId;
		m_state.pendingFrameCount = std::max<size_t>(frameCount, 1);
		result.eos = TryClaimEosLocked();
	}
	// When rejected: Rialto treats data on a request issued before its
	// SEEK_DONE as stale and discards it, so the caller answers it now and
	// Rialto asks again after the gate clears.  The gate is not cleared here.
	return result;
}

void Handshake::OnCancelNeedData()
{
	{
		std::lock_guard<std::mutex> lock(m_state.mu);
		m_state.hasPending = false;
		SnapshotAndClearBatchLocked();
	}
	m_state.cv.notify_all();
}

void Handshake::WakeWaiters()
{
	m_state.cv.notify_all();
}

// ---------------------------------------------------------------------------
// Injector thread
// ---------------------------------------------------------------------------

Handshake::AttachWait Handshake::WaitForAttach(
	const std::function<bool()> &attached,
	const std::function<void()> &onBlocking)
{
	std::unique_lock<std::mutex> lock(m_state.mu);
	if (!m_state.attachPending)
	{
		return AttachWait::NotPending;
	}
	const uint64_t waitGen = m_state.generation;
	onBlocking();
	// gateMode is deliberately not in the predicate: the pipeline passes
	// through PAUSED during startup before the video attach returns, which
	// would wake this thread early and drop the frame.  Only a generation
	// change (Flush/Stop) aborts the wait.
	m_state.cv.wait(lock, [&]{
		return attached() || m_state.generation != waitGen;
	});
	return attached() ? AttachWait::Attached : AttachWait::Aborted;
}

bool Handshake::BeginInject()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	const bool alreadyAtEos = m_state.eos && !m_state.hasPending;
	if (!alreadyAtEos)
	{
		m_state.injectorActive = true;
	}
	return !alreadyAtEos;
}

Handshake::WaitResult Handshake::WaitForGate(uint64_t gen)
{
	WaitResult result;
	std::unique_lock<std::mutex> lock(m_state.mu);
	m_state.cv.wait(lock, [&]{
		return m_state.generation != gen ||
		       m_state.gateMode != GateMode::BLOCKED;
	});
	if (m_state.generation != gen || m_state.gateMode == GateMode::DROPPED)
	{
		result.outcome = Wait::Abort;
		result.abandoned = ClaimPendingLocked();
		m_state.injectorActive = false;
	}
	return result;
}

Handshake::WaitResult Handshake::WaitForRequest(uint64_t gen)
{
	WaitResult result;
	std::unique_lock<std::mutex> lock(m_state.mu);
	m_state.cv.wait(lock, [&]{
		return m_state.generation != gen ||
		       m_state.hasPending ||
		       m_state.gateMode == GateMode::BLOCKED;
	});
	if (m_state.generation != gen)
	{
		// Unblock() left any pending request to this (active) injector.
		result.outcome = Wait::Abort;
		result.abandoned = ClaimPendingLocked();
		m_state.injectorActive = false;
	}
	else if (m_state.hasPending)
	{
		result.requestId = m_state.pendingRequestId;
	}
	else
	{
		result.outcome = Wait::GateReasserted;
	}
	return result;
}

Handshake::DropResult Handshake::DropSample(uint32_t requestId)
{
	DropResult result;
	std::lock_guard<std::mutex> lock(m_state.mu);
	if (m_state.hasPending && m_state.pendingRequestId == requestId)
	{
		result.eos = m_state.eos;
		result.claim = ClaimPendingLocked();
	}
	m_state.injectorActive = false;
	return result;
}

std::optional<Handshake::Claim> Handshake::OnNoSpace(uint64_t gen,
	uint32_t requestId)
{
	std::optional<Claim> claim;
	std::lock_guard<std::mutex> lock(m_state.mu);
	if (m_state.generation == gen)
	{
		m_bufferFull.store(true, std::memory_order_relaxed);
		if (m_state.hasPending && m_state.pendingRequestId == requestId)
		{
			claim = ClaimPendingLocked();
		}
	}
	return claim;
}

Handshake::Completion Handshake::Complete(uint64_t gen, uint32_t requestId,
	bool morePending, const Segment &segment)
{
	Completion result;
	std::lock_guard<std::mutex> lock(m_state.mu);
	const bool ownsRequest =
		m_state.hasPending && m_state.pendingRequestId == requestId;
	if (ownsRequest && m_state.generation == gen)
	{
		++m_state.segmentsAddedInBatch;
		m_state.batchMediaBytesSum    += segment.mediaBytes;
		m_state.batchMetadataBytesSum += segment.metadataBytes;
		if (!m_state.batchHasFirstPts)
		{
			m_state.batchHasFirstPts = true;
			m_state.batchFirstPtsSec = segment.ptsSec;
		}
		m_state.batchDurationSecSum += segment.durationSec;
		if (m_state.eos)
		{
			result.reply = Reply::Eos;
		}
		else if (m_state.segmentsAddedInBatch >= m_state.pendingFrameCount ||
		         !morePending)
		{
			result.reply = Reply::Ok;
		}
	}
	else if (ownsRequest)
	{
		// The generation changed while addSegment() was in flight and
		// Unblock() left the request to this injector.
		result.reply = Reply::Abandoned;
	}
	if (result.reply != Reply::None)
	{
		m_state.hasPending = false;
		result.batch = SnapshotAndClearBatchLocked();
	}
	m_state.injectorActive = false;
	return result;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool Handshake::IsEos()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	return m_state.eos;
}

uint64_t Handshake::Generation()
{
	std::lock_guard<std::mutex> lock(m_state.mu);
	return m_state.generation;
}
