# L1 Test Examples

Complete working examples from the AAMP test suite. Use these as templates.

---

## Example: AampLatencyMonitorTests

### Directory Layout

```
test/utests/tests/AampLatencyMonitorTests/
├── AampLatencyMonitorTests.cpp       (runner)
├── AampLatencyMonitorTestCases.cpp   (cases + fixture)
└── CMakeLists.txt
```

### CMakeLists.txt

```cmake
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include(GoogleTest)

set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
set(EXEC_NAME AampLatencyMonitorTests)

# Include common test directories
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)

set(TEST_SOURCES
	AampLatencyMonitorTests.cpp
	AampLatencyMonitorTestCases.cpp)

set(AAMP_SOURCES
	${AAMP_ROOT}/AampLatencyMonitor.cpp)

add_executable(${EXEC_NAME}
	${TEST_SOURCES}
	${AAMP_SOURCES})

set_target_properties(${EXEC_NAME} PROPERTIES FOLDER "utests")

if (CMAKE_XCODE_BUILD_SYSTEM)
  xcode_define_schema(${EXEC_NAME})
endif()

if (COVERAGE_ENABLED)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

target_link_libraries(${EXEC_NAME}
	fakes
	-pthread
	${GLIB_LINK_LIBRARIES}
	${OS_LD_FLAGS}
	${GMOCK_LINK_LIBRARIES}
	${GTEST_LINK_LIBRARIES})

aamp_utest_run_add(${EXEC_NAME})
```

### Test Runner (AampLatencyMonitorTests.cpp)

```cpp
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
 * @file AampLatencyMonitorTests.cpp
 * @brief Google Test runner for AampLatencyMonitor unit tests.
 */

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
```

### Test Cases (AampLatencyMonitorTestCases.cpp) — Key Patterns

#### Fixture with NiceMock and proper setup/teardown

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "AampLatencyMonitor.h"
#include "priv_aamp.h"
#include "AampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampStreamSinkManager.h"
#include "MockStreamSink.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

// Required by the fake AAMP infrastructure.
AampConfig *gpGlobalConfig{nullptr};

class AampLatencyMonitorTest : public ::testing::Test
{
protected:
	PrivateInstanceAAMP*                  mAamp{nullptr};
	NiceMock<MockPrivateInstanceAAMP>*    mMockAamp{nullptr};
	NiceMock<MockAampStreamSinkManager>*  mMockSinkMgr{nullptr};
	NiceMock<MockStreamSink>*             mMockSink{nullptr};
	AampConfig*                           mConfig{nullptr};
	AampLatencyMonitor*                   mMonitor{nullptr};

	void SetUp() override
	{
		mConfig      = new AampConfig();
		mAamp        = new PrivateInstanceAAMP(mConfig);
		mMockAamp    = new NiceMock<MockPrivateInstanceAAMP>();
		mMockSinkMgr = new NiceMock<MockAampStreamSinkManager>();
		mMockSink    = new NiceMock<MockStreamSink>();

		g_mockPrivateInstanceAAMP   = mMockAamp;
		g_mockAampStreamSinkManager = mMockSinkMgr;

		// Default safe stubs — overridden per test as required.
		ON_CALL(*mMockAamp, GetState()).WillByDefault(Return(eSTATE_PLAYING));
		ON_CALL(*mMockAamp, IsAdPlaying()).WillByDefault(Return(false));
		ON_CALL(*mMockSinkMgr, GetStreamSink(_)).WillByDefault(Return(mMockSink));
		ON_CALL(*mMockSink, SetPlayBackRate(_)).WillByDefault(Return(true));

		mMonitor = new AampLatencyMonitor(mAamp);
	}

	void TearDown() override
	{
		if (mMonitor)
		{
			mMonitor->Stop();
			delete mMonitor;
			mMonitor = nullptr;
		}

		g_mockPrivateInstanceAAMP   = nullptr;
		g_mockAampStreamSinkManager = nullptr;

		delete mMockSink;    mMockSink    = nullptr;
		delete mMockSinkMgr; mMockSinkMgr = nullptr;
		delete mMockAamp;    mMockAamp    = nullptr;
		delete mAamp;        mAamp        = nullptr;
		delete mConfig;      mConfig      = nullptr;
	}
};
```

#### Polling helper — no sleep() needed

```cpp
/// @brief Wait until GetCurrentRate() equals @p expected (up to @p maxWaitMs ms).
bool WaitForRate(double expected, int maxWaitMs = 500)
{
	auto deadline = std::chrono::steady_clock::now()
		+ std::chrono::milliseconds(maxWaitMs);
	while (mMonitor->GetCurrentRate() != expected)
	{
		if (std::chrono::steady_clock::now() >= deadline)
			return false;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return true;
}
```

#### Test naming: Class_Method_Behavior

```cpp
/**
 * @test State_InitiallyIdle
 * @brief A freshly constructed monitor must not be running.
 */
TEST_F(AampLatencyMonitorTest, State_InitiallyIdle)
{
	EXPECT_FALSE(mMonitor->IsRunning());
}

/**
 * @test State_StartTransitionsToRunning
 * @brief Start() must cause the worker thread to reach kRunning.
 */
TEST_F(AampLatencyMonitorTest, State_StartTransitionsToRunning)
{
	mMonitor->Start(MakeFastConfig());
	EXPECT_TRUE(WaitForRunning());
}

/**
 * @test State_StopTransitionsToIdle
 * @brief Stop() must return the monitor to kIdle.
 */
TEST_F(AampLatencyMonitorTest, State_StopTransitionsToIdle)
{
	mMonitor->Start(MakeFastConfig());
	ASSERT_TRUE(WaitForRunning());
	mMonitor->Stop();
	EXPECT_FALSE(mMonitor->IsRunning());
}
```

---

## Pattern Summary

| Aspect | Pattern |
|--------|---------|
| Mock type | `NiceMock<T>` by default |
| Global mock pointers | `g_mockPrivateInstanceAAMP`, `g_mockAampStreamSinkManager`, etc. |
| Default stubs | `ON_CALL(...).WillByDefault(Return(...))` in `SetUp()` |
| Per-test overrides | `EXPECT_CALL(...)` within the test body |
| Async waiting | Polling loop with deadline (2 ms interval) |
| Assertions | On component state/return values, never on fake internals |
| Test names | `Component_Method_ExpectedBehavior` |
| Doxygen | `@test` and `@brief` on every `TEST_F` |
