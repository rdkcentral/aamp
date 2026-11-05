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
 * @file AampRialtoNeedDataHandshake.h
 * @brief State and decisions of one source's needData/haveData handshake.
 */

#ifndef AAMP_RIALTO_NEED_DATA_HANDSHAKE_H
#define AAMP_RIALTO_NEED_DATA_HANDSHAKE_H

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>

/**
 * @class AampRialtoNeedDataHandshake
 * @brief Pending request, gate, generation, EOS and active-injector state of
 *        one media source, and every decision taken under its lock.
 *
 * It never calls Rialto.  Each operation that closes out a request returns a
 * Claim; the caller sends the matching haveData() after the call returns, so
 * no Rialto call is made with the lock held.
 *
 * Threads: the injector thread (BeginInject() .. Complete()), the Rialto
 * event thread (OnNeedData(), OnCancelNeedData()) and the control thread
 * (Reset(), Unblock(), SetGate(), SetEos(), SignalEos()).  All state is
 * guarded by State::mu.
 */
class AampRialtoNeedDataHandshake
{
public:
	/**
	 * @brief Injection gating mode for the needData/haveData handshake.
	 */
	enum class GateMode
	{
		NONE,     ///< Normal operation — samples flow through as needData arrives.
		BLOCKED,  ///< New samples block (waiting) until SetGate(false) or a newer generation.
		DROPPED   ///< New samples are discarded immediately — never blocks the caller thread.
	};

	/**
	 * @brief Snapshot of the segments staged for a needData batch, taken
	 *        at the moment the batch is claimed/abandoned so the eventual
	 *        haveData() response can be logged alongside what data (if
	 *        any) it is answering for.
	 */
	struct BatchSummary
	{
		size_t frameCount{0};       ///< Segments added to this batch.
		bool   hasFirstPts{false};  ///< True once at least one segment was added.
		double firstPtsSec{0.0};    ///< PTS (seconds) of the first segment added.
		double durationSecSum{0.0}; ///< Sum of durations (seconds) added.
		size_t mediaBytes{0};       ///< Sum of media payload bytes added.
		size_t metadataBytes{0};    ///< Sum of DRM metadata bytes added.
	};

	/// A request taken over by the caller, who must answer it.
	struct Claim
	{
		uint32_t requestId{0};
		BatchSummary batch;
	};

	/**
	 * @brief The handshake state.  Exposed for tests and read-only checks;
	 *        lock mu before reading it.
	 */
	struct State
	{
		/// Protects all fields in this struct.
		std::mutex              mu;
		/// Notified whenever hasPending, generation, gateMode,
		/// attachPending, or eos changes.
		std::condition_variable cv;

		/// True when Rialto has issued a needData request that has not
		/// yet been answered with a haveData response.
		bool     hasPending{false};
		/// Request ID from the current outstanding needData event; used
		/// to correlate the haveData response.
		uint32_t pendingRequestId{0};
		/// Number of frames requested in the current needData event;
		/// determines when the batch is considered complete.
		size_t   pendingFrameCount{0};
		/// Count of segments successfully delivered via addSegment for
		/// the current request batch; triggers haveData(OK) when it
		/// reaches pendingFrameCount.
		size_t   segmentsAddedInBatch{0};
		/// True once the first segment of the current batch has set
		/// batchFirstPtsSec.  Distinguishes "first frame had PTS 0" from
		/// "no frame added yet".
		bool     batchHasFirstPts{false};
		/// PTS (seconds) of the first segment added in the current batch.
		/// Only meaningful when batchHasFirstPts is true.
		double   batchFirstPtsSec{0.0};
		/// Running sum of durations (seconds) of segments added in the
		/// current batch.
		double   batchDurationSecSum{0.0};
		/// Running sum of raw media payload bytes (AampMediaSample::mDataSize)
		/// of segments added in the current batch.
		size_t   batchMediaBytesSum{0};
		/// Running sum of DRM metadata bytes (key id + IV + subsample table)
		/// attached to segments added in the current batch.
		size_t   batchMetadataBytesSum{0};
		/// Set once SignalEos() has been called.  Causes the next batch
		/// completion (or an idle needData) to fire haveData(EOS) instead
		/// of haveData(OK).
		bool     eos{false};
		/// Monotonically-increasing abort token.  Bumped by Reset() and
		/// Unblock(); injectors capture it at entry and abort if it
		/// changes while they are blocked.
		uint64_t generation{0};
		/// Injection gating mode — see GateMode.  Set to DROPPED by
		/// Unblock() (flush/stop/pause-notify: abort now, never block a
		/// caller thread again) and to BLOCKED or NONE by SetGate(gate),
		/// called via AampRialtoPlayer::UngateAllSources() (gate=false) at
		/// each site that issues pipeline play() or resumes injection —
		/// IssuePlay() (reached from Stream(), CheckAllSourcesAttached() and
		/// the SEEK_DONE play branch through the playback controller once no
		/// hold remains), StartPreRoll(), Pause(false), StopBuffering() and
		/// NotifyInjectorToResume(); the PLAYING playback-state handler does
		/// not ungate — or (gate=true) by Configure() and Flush()'s flushable
		/// path, which expect genuinely-wanted data to arrive shortly and
		/// must block it rather than drop it.
		/// Deliberately NOT touched by Reset(): clearing it there would
		/// reopen the gate before a multi-step Configure()/Flush() sequence
		/// (e.g. trickplay's Flush(pos=0) -> Configure() -> Flush(correctPos)
		/// -> Stream()) has finished, letting stale-position data slip
		/// through on an early needData.  NOT related to the pipeline PAUSED
		/// state — see WaitForAttach() for the important distinction.
		/// While BLOCKED, WaitForGate() blocks newly-submitted samples until
		/// the gate clears or a newer generation supersedes them.  While
		/// DROPPED, newly-submitted samples are discarded immediately
		/// without ever blocking the caller thread.
		GateMode gateMode{GateMode::NONE};
		/// True while an injector thread is between BeginInject() and the
		/// end of its sample.  Prevents SignalEos() and OnNeedData() from
		/// firing haveData(EOS) immediately when the active injector will
		/// send it after delivering its sample.
		bool     injectorActive{false};
		/// True while this source's Rialto attachment has been deferred
		/// (e.g. waiting for the video source to attach first).  Inject
		/// threads block on the cv when this is set so that no frames are
		/// silently discarded before the source is ready.
		bool     attachPending{false};
	};

	State &state() { return m_state; }

	// -----------------------------------------------------------------
	// Control thread
	// -----------------------------------------------------------------

	/// New session: bump the generation and clear the request, batch, EOS,
	/// injector and attach state.  gateMode is left as it is (see State).
	void Reset();

	/**
	 * @brief Bump the generation, switch to DROPPED and optionally set EOS,
	 *        all atomically.
	 * @param newEos    New EOS value, or nullopt to keep it.
	 * @param previous  Set to the gate mode before the call.
	 * @return The pending request if no injector is active to answer it;
	 *         the caller must answer it with NO_AVAILABLE_SAMPLES.
	 */
	std::optional<Claim> Unblock(std::optional<bool> newEos,
		GateMode &previous);

	/**
	 * @brief Enter BLOCKED (gate=true) or clear to NONE (gate=false).
	 * @return When clearing, the pending request if EOS resolution was
	 *         deferred while BLOCKED; the caller must answer it with EOS.
	 */
	std::optional<Claim> SetGate(bool gate, GateMode &previous,
		GateMode &current);

	/// @return true if @p eos differs from the current value (and was set).
	bool SetEos(bool eos);

	/**
	 * @brief Latch EOS.
	 * @return The pending request if it can be answered with EOS now (no
	 *         active injector, gate not BLOCKED).  Call WakeWaiters() after
	 *         answering it.
	 */
	std::optional<Claim> SignalEos();

	// -----------------------------------------------------------------
	// Rialto event thread
	// -----------------------------------------------------------------

	struct NeedDataResult
	{
		/// An older request that was never answered; answer it with
		/// NO_AVAILABLE_SAMPLES.
		std::optional<Claim> superseded;
		/// The new request arrived while gated; answer it with
		/// NO_AVAILABLE_SAMPLES (empty batch).
		bool rejected{false};
		/// The new request can be answered with EOS right away.
		std::optional<Claim> eos;
	};

	/// Stage a needData request.  Call WakeWaiters() after sending the
	/// replies, so the injector does not add segments before them.
	NeedDataResult OnNeedData(size_t frameCount, uint32_t requestId);

	/// Drop the pending request (Rialto cancelled it) and wake waiters.
	void OnCancelNeedData();

	/// Wake every thread waiting on the state.
	void WakeWaiters();

	// -----------------------------------------------------------------
	// Injector thread
	// -----------------------------------------------------------------

	enum class AttachWait
	{
		NotPending, ///< Not attached and no deferred attachment in flight.
		Attached,   ///< The deferred attachment completed.
		Aborted     ///< A generation change (Flush/Stop) ended the wait.
	};

	/**
	 * @brief Block while the source's attachment is deferred.
	 * @param attached    True once the source is attached; called under
	 *                    the lock.
	 * @param onBlocking  Called (under the lock) just before blocking.
	 */
	AttachWait WaitForAttach(const std::function<bool()> &attached,
		const std::function<void()> &onBlocking);

	/// Mark the injector active.  @return false if EOS was already
	/// answered, in which case there is nothing to inject.
	bool BeginInject();

	enum class Wait
	{
		Proceed,         ///< Gate open / request available.
		GateReasserted,  ///< Gate became BLOCKED before a request arrived.
		Abort            ///< Superseded or DROPPED; the injector is done.
	};

	struct WaitResult
	{
		Wait outcome{Wait::Proceed};
		/// The request to fill, when WaitForRequest() returns Proceed.
		uint32_t requestId{0};
		/// On Abort, a pending request the caller must answer with
		/// NO_AVAILABLE_SAMPLES.
		std::optional<Claim> abandoned;
	};

	/// Stage 1: wait while BLOCKED for generation @p gen.  Abort if the
	/// generation changed or the gate is DROPPED.
	WaitResult WaitForGate(uint64_t gen);

	/// Stage 2: wait for a needData request.  GateReasserted if the gate is
	/// set to BLOCKED first; Abort if the generation changes.
	WaitResult WaitForRequest(uint64_t gen);

	struct DropResult
	{
		/// The request, if still owned, to answer with EOS (when eos) or
		/// NO_AVAILABLE_SAMPLES.
		std::optional<Claim> claim;
		bool eos{false};
	};

	/// The sample cannot be injected.  Ends the injector's turn.
	DropResult DropSample(uint32_t requestId);

	/// addSegment() returned NO_SPACE.  Marks the buffer full if @p gen is
	/// current.  @return The request to answer (OK if it has frames,
	/// otherwise NO_AVAILABLE_SAMPLES); the injector stays active.
	std::optional<Claim> OnNoSpace(uint64_t gen, uint32_t requestId);

	/// A segment Rialto took (whatever its status).
	struct Segment
	{
		double ptsSec{0.0};
		double durationSec{0.0};
		size_t mediaBytes{0};
		size_t metadataBytes{0};
	};

	enum class Reply
	{
		None,       ///< Batch not complete yet.
		Ok,         ///< haveData(OK).
		Eos,        ///< haveData(EOS).
		Abandoned   ///< haveData(NO_AVAILABLE_SAMPLES): superseded meanwhile.
	};

	struct Completion
	{
		Reply reply{Reply::None};
		BatchSummary batch;
	};

	/// Count @p segment against request @p requestId and decide whether the
	/// batch is complete.  Ends the injector's turn.
	Completion Complete(uint64_t gen, uint32_t requestId, bool morePending,
		const Segment &segment);

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	bool IsEos();
	uint64_t Generation();

	/// True if addSegment() returned NO_SPACE since the last Reset() or
	/// Unblock().
	bool BufferFull() const { return m_bufferFull.load(std::memory_order_relaxed); }

private:
	/// Caller holds m_state.mu.
	BatchSummary SnapshotAndClearBatchLocked();
	/// Caller holds m_state.mu.
	std::optional<Claim> ClaimPendingLocked();
	/// Caller holds m_state.mu.  Claims only if EOS is set, a request is
	/// pending, no injector is active and the gate is not BLOCKED: EOS is
	/// never answered ahead of samples held behind the gate.  DROPPED does
	/// not defer it, as no samples are left to come.
	std::optional<Claim> TryClaimEosLocked();

	State m_state;
	std::atomic<bool> m_bufferFull{false};
};

#endif /* AAMP_RIALTO_NEED_DATA_HANDSHAKE_H */
