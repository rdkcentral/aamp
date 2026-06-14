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
 * Regression tests for VPAAMP-139: use-after-free in CurlStore lock callbacks.
 *
 * Root cause: old code heap-allocated a CurlDataShareLock per host entry and
 * passed its address as CURLSHOPT_USERDATA.  On store cleanup the object was
 * deleted while libcurl could still invoke the lock/unlock callbacks with the
 * stale pointer, causing a use-after-free.
 *
 * Fix: a single static CurlDataShareLock (CurlStore::mSharedCurlLock) with
 * process lifetime is now used for all CURLSH handles.
 *
 * Test strategy:
 *   T1 - Two different hostnames must receive the SAME CURLSHOPT_USERDATA
 *        pointer.  On old code they each got a distinct heap allocation so
 *        the pointers differed; on fixed code both equal &mSharedCurlLock.
 *
 *   T2 - After a store entry is evicted and the same hostname is re-added,
 *        the new CURLSHOPT_USERDATA must be identical to the original.  On old
 *        code a fresh heap allocation produced a different address.
 *
 *   T3 - (ASAN regression) After a store entry is evicted — the path where
 *        old code freed the per-entry lock — invoking the captured lock
 *        callback with the captured USERDATA must not crash or trigger an
 *        AddressSanitizer use-after-free report.  On fixed code USERDATA
 *        points to the static object which is never freed.
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
// T1: Two different hostnames must receive the SAME CURLSHOPT_USERDATA pointer.
//
// Regression: old code did `new curldatasharelock()` per host, so two hosts
// got different heap addresses and this test would FAIL.
// Fixed code passes &mSharedCurlLock for every host → addresses are equal.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, CreateCurlStore_UserDataIsSameStaticPointerAcrossHosts)
{
    void *userDataA = nullptr;
    void *userDataB = nullptr;

    // Capture the first CURLSHOPT_USERDATA call (host A) then the second (host B)
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_ptr(_, CURLSHOPT_USERDATA, _))
        .WillOnce(DoAll(SaveArg<2>(&userDataA), Return(CURLSHE_OK)))
        .WillOnce(DoAll(SaveArg<2>(&userDataB), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK)); // allow further calls from prior-test evictions

    GetHandleForHost("t1-host-a.example.com");
    GetHandleForHost("t1-host-b.example.com");

    ASSERT_NE(userDataA, nullptr);
    ASSERT_NE(userDataB, nullptr);
    EXPECT_EQ(userDataA, userDataB)
        << "CURLSHOPT_USERDATA must be the same static lock for all hosts";
}

// ---------------------------------------------------------------------------
// T2: After a store entry is evicted and its hostname re-added, the new
// CURLSHOPT_USERDATA must equal the original one.
//
// Regression: old code allocated a fresh lock on each CreateCurlStore call,
// so the second allocation produced a different address → test FAIL.
// Fixed code always uses &mSharedCurlLock → addresses are equal.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, CreateCurlStore_UserDataSameAfterEntryRecreation)
{
    void *userDataFirst  = nullptr;
    void *userDataSecond = nullptr;

    // We expect three CreateCurlStore calls: t2-orig, t2-trigger (causes
    // eviction of t2-orig), t2-orig again.  Capture 1st and 3rd USERDATA.
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_ptr(_, CURLSHOPT_USERDATA, _))
        .WillOnce(DoAll(SaveArg<2>(&userDataFirst),  Return(CURLSHE_OK))) // t2-orig created
        .WillOnce(Return(CURLSHE_OK))                                       // t2-trigger created
        .WillOnce(DoAll(SaveArg<2>(&userDataSecond), Return(CURLSHE_OK))) // t2-orig re-created
        .WillRepeatedly(Return(CURLSHE_OK));

    CURL *handleOrig = GetHandleForHost("t2-orig.example.com");
    // Return handle to store so mCurlStoreUserCount = 0, enabling eviction
    ReturnHandleForHost("t2-orig.example.com", handleOrig);
    // Adding a new host triggers RemoveCurlSock which evicts t2-orig
    GetHandleForHost("t2-trigger.example.com");
    // Re-add t2-orig: should call CreateCurlStore again with the same USERDATA
    GetHandleForHost("t2-orig.example.com");

    ASSERT_NE(userDataFirst,  nullptr);
    ASSERT_NE(userDataSecond, nullptr);
    EXPECT_EQ(userDataFirst, userDataSecond)
        << "CURLSHOPT_USERDATA must be the same static lock after re-creation";
}

// ---------------------------------------------------------------------------
// T3 (ASAN regression): Invoke the captured lock callback with the captured
// USERDATA *after* the owning store entry has been evicted.
//
// Old code: cleanup called SAFE_DELETE(pstShareLocks) — the heap object pointed
// to by USERDATA was freed.  Invoking the callback after that is use-after-free;
// AddressSanitizer would report it.
//
// Fixed code: USERDATA = &mSharedCurlLock (static, never freed).  Invoking the
// callback after eviction is safe.  This test passes cleanly under ASAN.
// ---------------------------------------------------------------------------
TEST_F(CurlStoreTests, LockCallback_SafeToCallAfterStoreEviction)
{
    void               *capturedUserData  = nullptr;
    curl_lock_function  capturedLockFn    = nullptr;
    curl_unlock_function capturedUnlockFn = nullptr;

    // Capture the lock-related setopt arguments from CreateCurlStore("t3-evict")
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_ptr(_, CURLSHOPT_USERDATA, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedUserData), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_func_lock(_, CURLSHOPT_LOCKFUNC, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedLockFn), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));
    EXPECT_CALL(*g_mockCurl, curl_share_setopt_func_unlock(_, CURLSHOPT_UNLOCKFUNC, _))
        .WillOnce(DoAll(SaveArg<2>(&capturedUnlockFn), Return(CURLSHE_OK)))
        .WillRepeatedly(Return(CURLSHE_OK));

    // Step 1: create store entry for t3-evict, capture its USERDATA and callbacks
    CURL *handleEvict = GetHandleForHost("t3-evict.example.com");

    ASSERT_NE(capturedUserData,  nullptr) << "USERDATA must be non-null";
    ASSERT_NE(capturedLockFn,   nullptr) << "lock callback must be non-null";
    ASSERT_NE(capturedUnlockFn, nullptr) << "unlock callback must be non-null";

    // Step 2: return the handle so mCurlStoreUserCount drops to 0
    ReturnHandleForHost("t3-evict.example.com", handleEvict);

    // Step 3: adding a new host triggers RemoveCurlSock which evicts t3-evict.
    // On old code this would have deleted the heap lock (USERDATA now dangling).
    // On fixed code the static lock is untouched.
    GetHandleForHost("t3-trigger.example.com");

    // Step 4: invoke the production lock callback with the captured USERDATA.
    // With ASAN: use-after-free is detected here on old code; passes on fixed code.
    capturedLockFn(nullptr, CURL_LOCK_DATA_DNS, CURL_LOCK_ACCESS_SHARED, capturedUserData);
    capturedUnlockFn(nullptr, CURL_LOCK_DATA_DNS, capturedUserData);
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

    GetHandleForHost("t4-host.example.com");

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
}
