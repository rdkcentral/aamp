/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file CurlStoreTests.cpp
 *
 * Regression tests for VPAAMP-139 / follow-up optimisation VPAAMP-558.
 *
 * VPAAMP-139 root cause: old code heap-allocated a CurlDataShareLock per host
 * and passed its address as CURLSHOPT_USERDATA.  Cleanup freed the object while
 * libcurl could still invoke the lock callbacks with the stale pointer.
 * VPAAMP-139 fix: a single static CurlStore::mSharedCurlLock with process
 * lifetime replaced per-host heap allocations — eliminating the UAF but
 * serialising DNS/SSL cache operations for every CDN hostname on one mutex.
 *
 * VPAAMP-558 optimisation: replace the single static lock with a per-host
 * CurlDataShareLock embedded directly in curlstorestruct (mShareLock).
 * Lifetime safety is preserved because every cleanup path calls
 * curl_share_cleanup before SAFE_DELETE(CurlSock), so the embedded lock is
 * always alive when libcurl needs it.
 * The non-store path (CurlInit when the curl store is disabled) now passes
 * &gCurlShLock (file-scope static) as CURLSHOPT_USERDATA instead of NULL.
 *
 * Test strategy:
 *   T1 - Two different hostname entries must receive DISTINCT CURLSHOPT_USERDATA
 *        pointers.  If they share the same pointer, DNS/SSL cache operations
 *        for different CDNs serialise (the VPAAMP-139 state).
 *
 *   T2 - Lock/unlock callbacks work correctly while the store entry is alive
 *        (mCurlStoreUserCount > 0).  The embedded lock is valid; no crash.
 *
 *   T3 - When a store entry is evicted, curl_share_cleanup is called for its
 *        CURLSH handle before the struct is deleted.  This verifies the
 *        cleanup order that guarantees the embedded lock is alive during
 *        curl_share_cleanup.
 *
 *   T4 - Lock/unlock callbacks handle all three CURL_LOCK_DATA_* cases
 *        (DNS, SSL, default/generic) without crashing.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampCurlStore.h"
#include "AampConfig.h"
#include "priv_aamp.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockCurl.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

// Defined in AampCurlStoreTest.cpp (test main)
extern AampConfig *gpGlobalConfig;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class CurlStoreTests : public ::testing::Test
{
protected:
    PrivateInstanceAAMP *mAamp{nullptr};

    // Stable fake addresses — non-null sentinels, never dereferenced by mock
    CURLSH *const kFakeShHandle   = reinterpret_cast<CURLSH *>(0x5001);
    CURL   *const kFakeCurlHandle = reinterpret_cast<CURL *>(0x5002);

    void SetUp() override
    {
        if (gpGlobalConfig == nullptr)
        {
            gpGlobalConfig = new AampConfig();
        }
        mAamp = new PrivateInstanceAAMP(gpGlobalConfig);

        g_mockAampConfig = std::make_shared<NiceMock<MockAampConfig>>();
        g_mockAampUtils  = std::make_shared<NiceMock<MockAampUtils>>();
        g_mockCurl       = std::make_shared<NiceMock<MockCurl>>();

        // EnableCurlStore = true so GetCurlHandle/SaveCurlHandle take the store
        // path.  All other bool config flags default to false (NiceMock default).
        ON_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore))
            .WillByDefault(Return(true));

        // curl_share_init — return a non-null fake handle
        ON_CALL(*g_mockCurl, curl_share_init())
            .WillByDefault(Return(kFakeShHandle));

        // curl_share_setopt_* — allow all variants, return success
        ON_CALL(*g_mockCurl, curl_share_setopt_ptr(_, _, _))
            .WillByDefault(Return(CURLSHE_OK));
        ON_CALL(*g_mockCurl, curl_share_setopt_func_lock(_, _, _))
            .WillByDefault(Return(CURLSHE_OK));
        ON_CALL(*g_mockCurl, curl_share_setopt_func_unlock(_, _, _))
            .WillByDefault(Return(CURLSHE_OK));
        ON_CALL(*g_mockCurl, curl_share_setopt_long(_, _, _))
            .WillByDefault(Return(CURLSHE_OK));

        // curl_share_cleanup — called during eviction, always succeeds
        ON_CALL(*g_mockCurl, curl_share_cleanup(_))
            .WillByDefault(Return(CURLSHE_OK));

        // curl_easy_init — return a non-null fake handle
        ON_CALL(*g_mockCurl, curl_easy_init())
            .WillByDefault(Return(kFakeCurlHandle));

        // aamp_getHostFromURL — default returns empty string (overridden per test)
        ON_CALL(*g_mockAampUtils, getHostFromURL(_))
            .WillByDefault(Return(std::string("")));

        // aamp_IsLocalHost — default false so store path is taken
        ON_CALL(*g_mockAampUtils, isLocalHost(_))
            .WillByDefault(Return(false));
    }

    void TearDown() override
    {
        delete mAamp;
        mAamp = nullptr;
        g_mockAampConfig.reset();
        g_mockAampUtils.reset();
        g_mockCurl.reset();
    }

    // Convenience: drive CreateCurlStore for `hostname` via GetCurlHandle.
    // Returns the fake CURL* that GetFromCurlStore produced.
    CURL *GetHandleForHost(const std::string &hostname)
    {
        ON_CALL(*g_mockAampUtils, getHostFromURL(_))
            .WillByDefault(Return(hostname));
        return CurlStore::GetCurlStoreInstance(mAamp)
                   .GetCurlHandle(mAamp,
                                  "https://" + hostname + "/segment.ts",
                                  eCURLINSTANCE_VIDEO);
    }

    // Convenience: return `curl` to the store for `hostname` (decrements
    // mCurlStoreUserCount, enabling eviction by RemoveCurlSock).
    void ReturnHandleForHost(const std::string &hostname, CURL *curl)
    {
        ON_CALL(*g_mockAampUtils, getHostFromURL(_))
            .WillByDefault(Return(hostname));
        CurlStore::GetCurlStoreInstance(mAamp)
            .SaveCurlHandle(mAamp,
                            "https://" + hostname + "/segment.ts",
                            eCURLINSTANCE_VIDEO,
                            curl);
    }
};

// ---------------------------------------------------------------------------
// T1: Two different hostname entries must receive DISTINCT CURLSHOPT_USERDATA pointers.
//
// VPAAMP-558: each curlstorestruct embeds its own CurlDataShareLock (mShareLock)
// so DNS/SSL cache operations for different CDN hosts use independent mutexes.
//
// Regression: if all hosts share one lock (VPAAMP-139 state), the addresses
// are equal and this test would FAIL.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, CreateCurlStore_UserDataDiffersAcrossHosts)
{
    void *userDataA = nullptr;
    void *userDataB = nullptr;

    // Capture the CURLSHOPT_USERDATA for host A then host B
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_ptr(_, CURLSHOPT_USERDATA, _))
        .WillOnce(DoAll(SaveArg<2>(&userDataA), Return(CURLSHE_OK)))
        .WillOnce(DoAll(SaveArg<2>(&userDataB), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));

    CURL *t1HandleA = GetHandleForHost("t1-host-a.example.com");
    CURL *t1HandleB = GetHandleForHost("t1-host-b.example.com");

    ASSERT_NE(userDataA, nullptr);
    ASSERT_NE(userDataB, nullptr);
    EXPECT_NE(userDataA, userDataB)
        << "Each host must have its own per-host lock (VPAAMP-558 regression)";

    // Return handles so mCurlStoreUserCount drops to 0; prevents cross-test
    // coupling via the process-lifetime singleton CurlStore.
    ReturnHandleForHost("t1-host-a.example.com", t1HandleA);
    ReturnHandleForHost("t1-host-b.example.com", t1HandleB);
}

// ---------------------------------------------------------------------------
// T2: Lock/unlock callbacks work correctly while a store entry is alive.
//
// The per-host embedded lock (mShareLock) is valid for the lifetime of its
// curlstorestruct.  When mCurlStoreUserCount > 0 the entry is live and
// invoking the captured lock/unlock callbacks must not crash.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, LockCallback_WorksWhileEntryIsAlive)
{
    void                *capturedUserData  = nullptr;
    curl_lock_function   capturedLockFn    = nullptr;
    curl_unlock_function capturedUnlockFn  = nullptr;

    EXPECT_CALL(*g_mockCurl, curl_share_setopt_ptr(_, CURLSHOPT_USERDATA, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedUserData), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_func_lock(_, CURLSHOPT_LOCKFUNC, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedLockFn), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_func_unlock(_, CURLSHOPT_UNLOCKFUNC, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedUnlockFn), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));

    // Create entry; mCurlStoreUserCount = 1 (still alive)
    CURL *t2Handle = GetHandleForHost("t2-host.example.com");

    ASSERT_NE(capturedUserData,  nullptr) << "USERDATA must be non-null";
    ASSERT_NE(capturedLockFn,   nullptr)  << "lock callback must be non-null";
    ASSERT_NE(capturedUnlockFn, nullptr)  << "unlock callback must be non-null";

    // Invoke the production callbacks while the entry is alive — the embedded
    // lock is valid so this must not crash.
    capturedLockFn(nullptr, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SHARED, capturedUserData);
    capturedUnlockFn(nullptr, CURL_LOCK_DATA_DNS, capturedUserData);

    // Return the handle so mCurlStoreUserCount drops to 0; prevents cross-test
    // coupling via the process-lifetime singleton CurlStore.
    ReturnHandleForHost("t2-host.example.com", t2Handle);
}

// ---------------------------------------------------------------------------
// T3: curl_share_cleanup is called for the evicted entry's CURLSH handle.
//
// VPAAMP-558 safety invariant: in every cleanup path the order is
//   (1) curl_share_cleanup(mCurlShared)   — share teardown, lock may fire
//   (2) SAFE_DELETE(CurlSock)             — destroys embedded mShareLock
// This test verifies step (1) actually happens (via mock expectation) so that
// the embedded lock is guaranteed alive whenever libcurl needs it.
//
// Mechanism: RemoveCurlSock fires when CreateCurlStore finds the map at or
// above MaxCurlSockStore (== 0 in tests).  It evicts the entry with the
// oldest timestamp and mCurlStoreUserCount == 0.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, ShareCleanup_CalledOnEviction)
{
    // Give distinct CURLSH handles to each curl_share_init call so we can
    // identify which entry is cleaned up.
    uintptr_t shareSeq = 0x6001;
    EXPECT_CALL(*g_mockCurl, curl_share_init())
        .WillOnce(Return(reinterpret_cast<CURLSH *>(0x6001))) // t3-host
        .WillOnce(Return(reinterpret_cast<CURLSH *>(0x6002))) // t3-trigger
        .WillRepeatedly(Return(reinterpret_cast<CURLSH *>(0x6003)));
    (void)shareSeq;

    CURL *hdl = GetHandleForHost("t3-host.example.com");
    // mCurlStoreUserCount = 1 after GetHandleForHost; drop to 0 so evictable
    ReturnHandleForHost("t3-host.example.com", hdl);

    // When GetHandleForHost("t3-trigger") calls CreateCurlStore, RemoveCurlSock
    // must call curl_share_cleanup on t3-host's CURLSH handle (0x6001) before
    // deleting the struct.  Exactly one call is expected.
    EXPECT_CALL(*g_mockCurl, curl_share_cleanup(reinterpret_cast<CURLSH *>(0x6001)))
        .Times(1);

    CURL *t3TriggerHandle = GetHandleForHost("t3-trigger.example.com");

    // Return the trigger handle so mCurlStoreUserCount drops to 0; prevents
    // cross-test coupling via the process-lifetime singleton CurlStore.
    // Note: t3-host was already evicted by RemoveCurlSock above, so no return
    // is needed for it.
    ReturnHandleForHost("t3-trigger.example.com", t3TriggerHandle);
}

// ---------------------------------------------------------------------------
// Helper: get a handle from the store for a specific curlId (slot).
// Wraps GetCurlHandle so tests can use any AampCurlInstance slot, not just
// eCURLINSTANCE_VIDEO which GetHandleForHost hard-codes.
// ---------------------------------------------------------------------------
static CURL *GetHandleForHostSlot(
    CurlStoreTests * /*unused*/,
    PrivateInstanceAAMP *aamp,
    const std::string &hostname,
    AampCurlInstance slot)
{
    ON_CALL(*g_mockAampUtils, getHostFromURL(_))
        .WillByDefault(Return(hostname));
    return CurlStore::GetCurlStoreInstance(aamp).GetCurlHandle(
        aamp, "https://" + hostname + "/segment.ts", slot);
}

static void ReturnHandleForHostSlot(
    CurlStoreTests * /*unused*/,
    PrivateInstanceAAMP *aamp,
    const std::string &hostname,
    AampCurlInstance slot,
    CURL *curl)
{
    ON_CALL(*g_mockAampUtils, getHostFromURL(_))
        .WillByDefault(Return(hostname));
    CurlStore::GetCurlStoreInstance(aamp).SaveCurlHandle(
        aamp, "https://" + hostname + "/segment.ts", slot, curl);
}

// ---------------------------------------------------------------------------
// T4: The lock/unlock callbacks handle all CURL_LOCK_DATA_* selector values
// correctly (DNS → mDnsCurlShareMutex, SSL → mSslCurlShareMutex,
// other/generic → mCurlSharedlock).
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, LockCallback_HandlesAllLockDataTypes)
{
    void               *capturedUserData  = nullptr;
    curl_lock_function  capturedLockFn    = nullptr;
    curl_unlock_function capturedUnlockFn = nullptr;

    EXPECT_CALL(*g_mockCurl, curl_share_setopt_ptr(_, CURLSHOPT_USERDATA, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedUserData), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_func_lock(_, CURLSHOPT_LOCKFUNC, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedLockFn), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_func_unlock(_, CURLSHOPT_UNLOCKFUNC, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedUnlockFn), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));

    CURL *t4Handle = GetHandleForHost("t4-host.example.com");

    ASSERT_NE(capturedUserData,  nullptr);
    ASSERT_NE(capturedLockFn,   nullptr);
    ASSERT_NE(capturedUnlockFn, nullptr);

    // Exercise all three lock data selectors — must not crash
    const curl_lock_data kDataTypes[] = {
        CURL_LOCK_DATA_DNS,
        CURL_LOCK_DATA_SSL_SESSION,
        CURL_LOCK_DATA_CONNECT  // falls through to default (mCurlSharedlock)
    };

    for (curl_lock_data data : kDataTypes)
    {
        capturedLockFn(nullptr, data, CURL_LOCK_ACCESS_SHARED, capturedUserData);
        capturedUnlockFn(nullptr, data, capturedUserData);
    }

    // Return the handle so mCurlStoreUserCount drops to 0; prevents cross-test
    // coupling via the process-lifetime singleton CurlStore.
    ReturnHandleForHost("t4-host.example.com", t4Handle);
}

// ---------------------------------------------------------------------------
// T5: LIFO recycling within a slot — the most-recently-returned handle is
// recycled first by the new per-slot deque (pop_back).
//
// The old single-deque mFreeQ always popped from the FRONT (pop_front / FIFO),
// reusing the oldest pooled handle first.  The new mFreeSlots[slot] pops from
// the BACK (pop_back / LIFO), so the most-recently-returned — and therefore
// most likely still-warm — TCP connection is reused first.
//
// Regression: with the old mFreeQ, after storing kHdlFirst then kHdlSecond
// for the same slot, GetCurlHandleFromFreeQ returns kHdlFirst (front/FIFO).
// The new GetCurlHandleFromSlot returns kHdlSecond (back/LIFO).  The
// EXPECT_EQ assertions below therefore FAIL against the old implementation.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, PerSlotRetrieval_LIFORecyclingWithinSlot)
{
    const std::string hostname = "t5-host.example.com";

    // Two distinct sentinel handles allocated for the same VIDEO slot.
    CURL *const kHdlFirst  = reinterpret_cast<CURL *>(0xA001);
    CURL *const kHdlSecond = reinterpret_cast<CURL *>(0xA002);
    EXPECT_CALL(*g_mockCurl, curl_easy_init())
        .WillOnce(Return(kHdlFirst))    // 1st miss → VIDEO slot, 1st session
        .WillOnce(Return(kHdlSecond))   // 2nd miss → VIDEO slot, 2nd session
        .WillRepeatedly(Return(kFakeCurlHandle));

    // Two concurrent cache misses on the same slot (two sessions, same host).
    // Pool[VIDEO] is empty for both calls, so new handles are allocated.
    CURL *h0 = GetHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO);
    CURL *h1 = GetHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO);
    ASSERT_EQ(h0, kHdlFirst);
    ASSERT_EQ(h1, kHdlSecond);

    // Return h0 first, then h1.
    // Pool[VIDEO] deque becomes [kHdlFirst, kHdlSecond]: kHdlFirst oldest (front),
    // kHdlSecond newest (back).
    ReturnHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO, h0);
    ReturnHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO, h1);

    // LIFO first get: must recycle kHdlSecond (newest entry, at the back).
    // Old mFreeQ (pop_front / FIFO) would return kHdlFirst (at front) here,
    // causing this assertion to FAIL on the old implementation.
    CURL *recycledFirst = GetHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO);
    EXPECT_EQ(recycledFirst, kHdlSecond);

    // Pool is now [kHdlFirst] only.  Second consecutive get returns kHdlFirst.
    CURL *recycledSecond = GetHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO);
    EXPECT_EQ(recycledSecond, kHdlFirst);

    // Restore balance (userCount → 0) so the entry is evictable.
    ReturnHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO, recycledFirst);
    ReturnHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO, recycledSecond);
}

// ---------------------------------------------------------------------------
// T6: mPendingFlush — FlushCurlSockForHost with live users (userCount > 0)
// must set the pending-flush flag, then dispose handles returned by in-flight
// callers instead of re-pooling them, and complete the deferred CURLSH cleanup
// automatically when the last user calls KeepInCurlStore.
//
// Scenario:
//   1. Two sessions check out handles (count=2).
//   2. Session-1 returns its handle via CurlTerm (isFlushFds=true):
//        - KeepInCurlStoreBulk pushes handle to pool (count→1, flush not yet set).
//        - FlushCurlSockForHost clears the pool (curl_easy_cleanup on pooled hdl),
//          sets mPendingFlush=true because count=1 > 0.
//        - curl_share_cleanup NOT called yet.
//   3. Session-2 returns its handle via SaveCurlHandle:
//        - mPendingFlush=true → handle disposed, not re-pooled.
//        - count→0 → deferred cleanup completes: curl_share_cleanup called.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, PendingFlush_DisposesReturnedHandlesAndDefersCurlShare)
{
    const std::string hostname = "t6-host.example.com";
    CURLSH *const kShareHdl  = reinterpret_cast<CURLSH *>(0xB001);
    CURL   *const kHdlVid    = reinterpret_cast<CURL *>(0xB002);
    CURL   *const kHdlAud    = reinterpret_cast<CURL *>(0xB003);

    // Catch-all for curl_easy_cleanup: singleton entries from earlier tests are
    // evicted (and their handles cleaned up) inside KeepInCurlStoreBulk's
    // RemoveCurlSock path during this test.  Without this catch-all, NiceMock
    // would treat those eviction calls as "unexpected" (an EXPECT_CALL exists
    // but no matcher matches) rather than "uninteresting" (no EXPECT_CALL).
    // GMock processes expectations in LIFO order, so specific EXPECT_CALLs set
    // *after* this catch-all will still be tried first and take priority.
    EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(_)).Times(AnyNumber());

    // kHdlVid must be disposed exactly once by FlushCurlSockForHost when the
    // pool is cleared.  Set after the catch-all so GMock (LIFO) tries this
    // specific matcher first.
    EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(kHdlVid)).Times(1);

    ON_CALL(*g_mockAampUtils, getHostFromURL(_)).WillByDefault(Return(hostname));
    ON_CALL(*g_mockCurl, curl_share_init()).WillByDefault(Return(kShareHdl));
    EXPECT_CALL(*g_mockCurl, curl_easy_init())
        .WillOnce(Return(kHdlVid))
        .WillOnce(Return(kHdlAud))
        .WillRepeatedly(Return(kFakeCurlHandle));

    // Session-1 gets VIDEO handle (count=1, cache miss).
    CURL *h_vid = GetHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_VIDEO);
    ASSERT_EQ(h_vid, kHdlVid);

    // Session-2 gets AUDIO handle (count=2, cache miss).
    CURL *h_aud = GetHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_AUDIO);
    ASSERT_EQ(h_aud, kHdlAud);

    // Session-1 returns VIDEO handle and triggers flush (isFlushFds=true):
    //   KeepInCurlStoreBulk: count→1, kHdlVid pushed to pool[VIDEO].
    //   FlushCurlSockForHost: pool cleared (kHdlVid disposed via the explicit
    //   EXPECT_CALL above), mPendingFlush=true.
    //   curl_share_cleanup NOT called yet (count still 1 > 0).
    // Any share_cleanup from evictions (e.g., t1-host-a) during CurlTerm is
    // NiceMock-uninteresting (no matching EXPECT_CALL active at that point).
    mAamp->curl[eCURLINSTANCE_VIDEO] = h_vid;
    mAamp->mOrigManifestUrl.hostname     = hostname;
    mAamp->mOrigManifestUrl.isRemotehost = true;
    CurlStore::GetCurlStoreInstance(mAamp).CurlTerm(
        mAamp, eCURLINSTANCE_VIDEO, 1, /*isFlushFds=*/true);
    // After CurlTerm: aamp->curl[VIDEO]=nullptr, mPendingFlush=true, count=1.

    // Session-2 returns AUDIO handle.
    // mPendingFlush=true → kHdlAud disposed (not re-pooled), count→0 → share_cleanup.
    // Set these AFTER CurlTerm so eviction share_cleanup calls (0x5001 etc.) remain
    // uninteresting (no matching EXPECT_CALL active during CurlTerm).
    EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(kHdlAud)).Times(1);
    EXPECT_CALL(*g_mockCurl, curl_share_cleanup(kShareHdl)).Times(1);

    ReturnHandleForHostSlot(this, mAamp, hostname, eCURLINSTANCE_AUDIO, h_aud);
    // Entry is now fully removed from the store.
}

// ---------------------------------------------------------------------------
// T7: mCurlStoreUserCount never goes negative — an extra Keep (without a
// matching Get) must clamp the counter to 0 and log a warning rather than
// silently wrapping to 2^32-1 (the unsigned-int overflow the old code had).
//
// With a wrapped count the entry is permanently un-evictable.  After clamping
// the entry's count is 0 so RemoveCurlSock can evict it normally.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, UserCount_DoesNotGoBelowZero)
{
    const std::string hostname = "t7-host.example.com";

    // Give t7-host a unique CURLSH sentinel so we can assert its specific
    // share handle is cleaned up when the entry is evicted (not just any
    // share handle from a different host).
    CURLSH *const kT7ShareHdl = reinterpret_cast<CURLSH *>(0xC002);
    EXPECT_CALL(*g_mockCurl, curl_share_init())
        .WillOnce(Return(kT7ShareHdl))           // t7-host
        .WillRepeatedly(Return(kFakeShHandle));   // t7-trigger and any other host

    ON_CALL(*g_mockAampUtils, getHostFromURL(_)).WillByDefault(Return(hostname));

    // One Get → count=1; one matching Keep → count=0 (balanced).
    CURL *h = GetHandleForHost(hostname);
    ReturnHandleForHost(hostname, h);

    // Extra Keep with no matching Get — would underflow unsigned int to ~4e9.
    // The signed-int + clamp fix must keep count at 0.
    // Use a fresh fake handle that the store doesn't know about.
    CURL *const kExtraHdl = reinterpret_cast<CURL *>(0xC001);
    // NiceMock: curl_easy_cleanup is allowed (store may dispose the extra handle).
    ReturnHandleForHost(hostname, kExtraHdl);

    // Verify t7-host's entry is evictable: RemoveCurlSock must call
    // curl_share_cleanup on kT7ShareHdl exactly once, proving the entry's
    // userCount stayed at 0 (was not wrapped to ~4e9 by the underflow bug).
    EXPECT_CALL(*g_mockCurl, curl_share_cleanup(kT7ShareHdl)).Times(1);

    // Creating a new host entry triggers RemoveCurlSock (MaxCurlSockStore=0),
    // which must successfully evict t7-host (proving userCount==0 after clamp).
    CURL *triggerHdl = GetHandleForHost("t7-trigger.example.com");
    ReturnHandleForHost("t7-trigger.example.com", triggerHdl);
}

// ---------------------------------------------------------------------------
// T8: CURLOPT_SHARE re-applied on recycle — when a handle is retrieved from
// the pool (GetCurlHandleFromSlot), the implementation must call
// curl_easy_setopt(handle, CURLOPT_SHARE, mCurlShared) to ensure the recycled
// handle is bound to the current live share, not a potentially stale one.
//
// Regression: the old GetCurlHandleFromFreeQ did NOT re-apply CURLOPT_SHARE;
// if the share pointer ever changed (e.g., flush-and-recreate) the recycled
// handle would use a stale or freed CURLSH.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, RecycledHandle_CurlOptShareReapplied)
{
    const std::string hostname = "t8-host.example.com";
    CURLSH *const kShareHdl = reinterpret_cast<CURLSH *>(0xD001);
    CURL   *const kHdl      = reinterpret_cast<CURL *>(0xD002);

    ON_CALL(*g_mockAampUtils, getHostFromURL(_)).WillByDefault(Return(hostname));
    ON_CALL(*g_mockCurl, curl_share_init()).WillByDefault(Return(kShareHdl));
    // Use EXPECT_CALL (not ON_CALL) for sequential returns from curl_easy_init.
    EXPECT_CALL(*g_mockCurl, curl_easy_init())
        .WillOnce(Return(kHdl))
        .WillRepeatedly(Return(kFakeCurlHandle));

    // First Get: cache miss → kHdl created fresh.
    CURL *h = GetHandleForHost(hostname);
    ASSERT_EQ(h, kHdl);

    // Return kHdl to the pool.
    ReturnHandleForHost(hostname, h);

    // Second Get: cache HIT — kHdl should be recycled from slot VIDEO.
    // GetCurlHandleFromSlot must call curl_easy_setopt(kHdl, CURLOPT_SHARE, kShareHdl).
    // FakeCurl routes CURLOPT_SHARE through curl_easy_setopt_ptr (D4 fix).
    EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(kHdl, CURLOPT_SHARE, kShareHdl))
        .Times(1);  // Must be called exactly once on the recycled handle

    CURL *recycled = GetHandleForHost(hostname);
    EXPECT_EQ(recycled, kHdl);  // Must have retrieved the pooled kHdl

    // Restore balance.
    ReturnHandleForHost(hostname, recycled);
}
