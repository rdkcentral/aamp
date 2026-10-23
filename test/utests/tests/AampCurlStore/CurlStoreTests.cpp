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
