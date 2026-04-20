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
 * @file AampNetworkPersona.h
 * @brief Simulated network latency injection controlled by a persona JSON file.
 *
 * Purpose
 * -------
 * Loads a "network persona" JSON (same format as the abrsim
 * NetworkCharacteristics struct) and introduces realistic TTFB and
 * transfer-time delays around curl downloads so that AAMP's ABR heuristics
 * can be exercised under controlled, repeatable network conditions without
 * requiring an actual congested network.
 *
 * How throttling works (option 2 – wall-clock padding)
 * ----------------------------------------------------
 *  1. Before curl_easy_perform: sleep for a sampled TTFB.
 *  2. After curl_easy_perform:  compute how long curl actually took and sleep
 *     for any remaining time to reach the persona's predicted transfer time,
 *     so that total wall-clock ≈ TTFB + predicted_transfer.
 *
 * The ABR bandwidth estimator then observes download times that match the
 * persona's throughput distribution, giving the same adaptive behaviour as
 * the standalone abrsim simulator.
 *
 * Configuration
 * -------------
 * Set eAAMPConfig_NetworkPersonaFile to a path; leave empty (the default)
 * to disable entirely with zero overhead.
 *
 * Thread-safety
 * -------------
 * All public methods are thread-safe.  LoadFromFile() is idempotent — the
 * first call with a non-empty path loads the persona; subsequent calls
 * (from any thread) return immediately.
 */

#pragma once

#include <cstddef>
#include <mutex>
#include <random>
#include <string>

/**
 * @class AampNetworkPersona
 * @brief Process-level singleton that models per-request network latency
 *        according to the loaded persona parameters.
 */
class AampNetworkPersona
{
public:
    // ── Singleton access ────────────────────────────────────────────────────
    static AampNetworkPersona& Instance();

    // ── Lifecycle ───────────────────────────────────────────────────────────

    /**
     * @brief Load persona parameters from a JSON file.
     *
     * Idempotent: once a persona is loaded this call is a no-op regardless
     * of the path supplied.  Returns true if the persona is (or becomes)
     * loaded after the call.
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Returns true when a persona file has been successfully loaded.
     */
    bool IsLoaded() const;

    // ── Sampling ─────────────────────────────────────────────────────────────

    /**
     * @brief Sample the simulated TTFB for one HTTP request.
     *
     * Models: Gaussian RTT jitter + optional spike + optional new-connection
     * TCP-handshake penalty.
     *
     * @param assumeNewConnection  When true the new-connection setup penalty is
     *        always applied; when false it is applied with probability
     *        (1 - p_conn_reuse) from the persona.
     * @return Simulated time-to-first-byte in milliseconds (>= 0).
     */
    double SampleTtfbMs(bool assumeNewConnection = false);

    /**
     * @brief Sample the simulated payload transfer time for a body of
     *        @p bytes bytes.
     *
     * Models: per-download independent lognormal throughput + per-burst TCP
     * flush jitter + occasional stall (packet-loss / retransmit event).
     *
     * Per-download independence means no AR(1) correlation between successive
     * calls — each call samples from the marginal steady-state distribution.
     *
     * @param bytes  Number of bytes in the response body.  Returns 0 for
     *               empty bodies.
     * @return Simulated transfer time in milliseconds (>= 0).
     */
    double SampleTransferMs(std::size_t bytes);

private:
    AampNetworkPersona();
    ~AampNetworkPersona()                               = default;
    AampNetworkPersona(const AampNetworkPersona&)       = delete;
    AampNetworkPersona& operator=(const AampNetworkPersona&) = delete;

    // ── Persona parameters (loaded once from JSON) ───────────────────────────
    double mBaseRttMs        = 175.0;
    double mRttJitterMs      =  20.0;
    double mTtfbSpikeP       =   0.01;   ///< probability of TTFB spike
    double mTtfbSpikeMs      = 120.0;
    double mMeanThrLn        =   0.0;    ///< ln(mean throughput in bytes/s)
    double mThrSigmaLn       =   0.80;   ///< log-normal std-dev
    int    mBurstsPerSegment =    8;
    double mFlushJitterMs    =   6.0;
    double mLateChunkP       =   0.01;   ///< probability of a stall event
    double mLateChunkExtraMs = 120.0;
    double mPConnReuse       =   0.95;
    double mNewConnPenaltyMs = 170.0;

    // ── State ────────────────────────────────────────────────────────────────
    mutable std::mutex      mMutex;       ///< guards mLoaded and mRng
    bool                    mLoaded = false;
    mutable std::mt19937_64 mRng;
};
