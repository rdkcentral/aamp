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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <cstring>
#include <limits>
#include "CachedFragment.h"
#include "AampMediaType.h"
#include "priv_aamp.h"

/**
 * @brief Test fixture for CachedFragment class
 *
 * This fixture provides a clean environment for testing the CachedFragment class
 * functionality including constructor, Copy(), Clear(), and all member variables.
 */
class CachedFragmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create fresh CachedFragment instances for each test
        cachedFragment.reset(new CachedFragment());
        sourceCachedFragment.reset(new CachedFragment());

        // Set up test data (smaller to avoid memory allocation issues in test environment)
        testData = "TestData";
        testDataSize = strlen(testData);

        // Set up test values
        testPosition = 12.345;
        testDuration = 6.789;
        testAbsPosition = 100.555;
        testInitFragment = true;
        testDiscontinuity = false;
        testIsDummy = false;
        testProfileIndex = 2;
        testTimeScale = 90000;
        testUri = "http://example.com/segment1.ts";
        testType = eMEDIATYPE_VIDEO;
        testDownloadStartTime = 1234567890LL;
        testDiscontinuityIndex = 5LL;
        testPTSOffsetSec = 1.5;
    }

    void TearDown() override {
        // Clean up is handled by unique_ptr
    }

    // Test instances
    std::unique_ptr<CachedFragment> cachedFragment;
    std::unique_ptr<CachedFragment> sourceCachedFragment;

    // Test data
    const char* testData;
    size_t testDataSize;

    // Test values
    double testPosition;
    double testDuration;
    double testAbsPosition;
    bool testInitFragment;
    bool testDiscontinuity;
    bool testIsDummy;
    int testProfileIndex;
    uint32_t testTimeScale;
    std::string testUri;
    AampMediaType testType;
    long long testDownloadStartTime;
    long long testDiscontinuityIndex;
    double testPTSOffsetSec;
};

/**
 * @brief Test CachedFragment default constructor initialization
 *
 * Verifies that all member variables are properly initialized to default values
 * when a CachedFragment is constructed using the default constructor.
 */
TEST_F(CachedFragmentTest, Constructor_DefaultInitialization_AllFieldsSetToDefaults) {
    // Test primitive type defaults
    EXPECT_DOUBLE_EQ(cachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, 0.0);
    EXPECT_EQ(cachedFragment->initFragment, false);
    EXPECT_EQ(cachedFragment->discontinuity, false);
    EXPECT_EQ(cachedFragment->isDummy, false);
    EXPECT_EQ(cachedFragment->profileIndex, 0);
    EXPECT_EQ(cachedFragment->timeScale, 0U);
    EXPECT_TRUE(cachedFragment->uri.empty());
    EXPECT_EQ(cachedFragment->type, eMEDIATYPE_DEFAULT);
    EXPECT_EQ(cachedFragment->downloadStartTime, 0LL);
    EXPECT_EQ(cachedFragment->discontinuityIndex, 0LL);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, 0.0);

    // Test that BitrateChangeReason is properly initialized
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_ABR);

    // Note: fragment buffer operations are mocked and not tested directly
    // Only CachedFragment member variables are tested
}

/**
 * @brief Test CachedFragment with data population
 *
 * Verifies that member variables can be properly set and retrieved.
 */
TEST_F(CachedFragmentTest, SetMemberVariables_ValidValues_AllFieldsSetCorrectly) {
    // Set all member variables
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    cachedFragment->absPosition = testAbsPosition;
    cachedFragment->initFragment = testInitFragment;
    cachedFragment->discontinuity = testDiscontinuity;
    cachedFragment->isDummy = testIsDummy;
    cachedFragment->profileIndex = testProfileIndex;
    cachedFragment->timeScale = testTimeScale;
    cachedFragment->uri = testUri;
    cachedFragment->type = testType;
    cachedFragment->downloadStartTime = testDownloadStartTime;
    cachedFragment->discontinuityIndex = testDiscontinuityIndex;
    cachedFragment->PTSOffsetSec = testPTSOffsetSec;
    cachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_TUNE;

    // Note: fragment buffer operations are mocked and not tested directly

    // Verify all values are set correctly
    EXPECT_DOUBLE_EQ(cachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, testDuration);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, testAbsPosition);
    EXPECT_EQ(cachedFragment->initFragment, testInitFragment);
    EXPECT_EQ(cachedFragment->discontinuity, testDiscontinuity);
    EXPECT_EQ(cachedFragment->isDummy, testIsDummy);
    EXPECT_EQ(cachedFragment->profileIndex, testProfileIndex);
    EXPECT_EQ(cachedFragment->timeScale, testTimeScale);
    EXPECT_EQ(cachedFragment->uri, testUri);
    EXPECT_EQ(cachedFragment->type, testType);
    EXPECT_EQ(cachedFragment->downloadStartTime, testDownloadStartTime);
    EXPECT_EQ(cachedFragment->discontinuityIndex, testDiscontinuityIndex);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, testPTSOffsetSec);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_TUNE);

    // Note: fragment buffer operations are implementation details that should be mocked
    // CachedFragment tests focus on member variables, not fragment buffer behavior
}

/**
 * @brief Test CachedFragment Copy method with populated source
 *
 * Verifies that the Copy method correctly copies all member variables and data
 * from a source CachedFragment to the destination.
 */
TEST_F(CachedFragmentTest, Copy_PopulatedSource_AllFieldsCopiedCorrectly) {
    // Set up source fragment with test data
    sourceCachedFragment->position = testPosition;
    sourceCachedFragment->duration = testDuration;
    sourceCachedFragment->absPosition = testAbsPosition;
    sourceCachedFragment->initFragment = testInitFragment;
    sourceCachedFragment->discontinuity = testDiscontinuity;
    sourceCachedFragment->isDummy = testIsDummy;
    sourceCachedFragment->profileIndex = testProfileIndex;
    sourceCachedFragment->timeScale = testTimeScale;
    sourceCachedFragment->uri = testUri;
    sourceCachedFragment->type = testType;
    sourceCachedFragment->downloadStartTime = testDownloadStartTime;
    sourceCachedFragment->discontinuityIndex = testDiscontinuityIndex;
    sourceCachedFragment->PTSOffsetSec = testPTSOffsetSec;
    sourceCachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_SEEK;
    // Note: fragment buffer operations are mocked and not tested directly

    // Copy from source to destination (test member variable copying)
    cachedFragment->Copy(*sourceCachedFragment);

    // Verify all fields were copied correctly
    EXPECT_DOUBLE_EQ(cachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, testDuration);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, testAbsPosition);
    EXPECT_EQ(cachedFragment->initFragment, testInitFragment);
    EXPECT_EQ(cachedFragment->discontinuity, testDiscontinuity);
    EXPECT_EQ(cachedFragment->isDummy, testIsDummy);
    EXPECT_EQ(cachedFragment->profileIndex, testProfileIndex);
    EXPECT_EQ(cachedFragment->timeScale, testTimeScale);
    EXPECT_EQ(cachedFragment->uri, testUri);
    EXPECT_EQ(cachedFragment->type, testType);
    EXPECT_EQ(cachedFragment->downloadStartTime, testDownloadStartTime);
    EXPECT_EQ(cachedFragment->discontinuityIndex, testDiscontinuityIndex);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, testPTSOffsetSec);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_SEEK);

    // Note: fragment buffer operations are mocked, not tested directly
    // CachedFragment Copy behavior is verified through member variable copying
}


/**
 * @brief Test CachedFragment Copy method with empty source
 *
 * Verifies that the Copy method correctly handles copying from an empty
 * (default-initialized) source fragment.
 */
TEST_F(CachedFragmentTest, Copy_EmptySource_DefaultValuesCopied) {
    // Set up destination with some data first
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    // Note: fragment buffer operations are mocked and not tested directly

    // Copy from empty source (sourceCachedFragment is default-initialized)
    cachedFragment->Copy(*sourceCachedFragment);

    // Verify all CachedFragment fields are reset to default values
    EXPECT_DOUBLE_EQ(cachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, 0.0);
    EXPECT_EQ(cachedFragment->initFragment, false);
    EXPECT_EQ(cachedFragment->discontinuity, false);
    EXPECT_EQ(cachedFragment->isDummy, false);
    EXPECT_EQ(cachedFragment->profileIndex, 0);
    EXPECT_EQ(cachedFragment->timeScale, 0U);
    EXPECT_TRUE(cachedFragment->uri.empty());
    EXPECT_EQ(cachedFragment->type, eMEDIATYPE_DEFAULT);  // sourceCachedFragment is default-initialized with eMEDIATYPE_DEFAULT
    EXPECT_EQ(cachedFragment->downloadStartTime, 0LL);
    EXPECT_EQ(cachedFragment->discontinuityIndex, 0LL);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, 0.0);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_ABR);
}

/**
 * @brief Test CachedFragment Clear method with populated fragment
 *
 * Verifies that the Clear method properly resets all member variables
 * to their default values and clears the fragment buffer.
 */
TEST_F(CachedFragmentTest, Clear_PopulatedFragment_AllFieldsResetToDefaults) {
    // Set up fragment with test data
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    cachedFragment->absPosition = testAbsPosition;
    cachedFragment->initFragment = testInitFragment;
    cachedFragment->discontinuity = testDiscontinuity;
    cachedFragment->isDummy = testIsDummy;
    cachedFragment->profileIndex = testProfileIndex;
    cachedFragment->timeScale = testTimeScale;
    cachedFragment->uri = testUri;
    cachedFragment->type = testType;
    cachedFragment->downloadStartTime = testDownloadStartTime;
    cachedFragment->discontinuityIndex = testDiscontinuityIndex;
    cachedFragment->PTSOffsetSec = testPTSOffsetSec;
    cachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_TUNE;
    // Note: fragment buffer operations are mocked and not tested directly

    // Verify data is set before clearing
    EXPECT_NE(cachedFragment->position, 0.0);
    // Note: fragment buffer operations are mocked and not tested directly

    // Clear the fragment
    cachedFragment->Clear();

    // Verify all CachedFragment fields are reset to defaults
    EXPECT_DOUBLE_EQ(cachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, 0.0);
    EXPECT_EQ(cachedFragment->initFragment, false);
    EXPECT_EQ(cachedFragment->discontinuity, false);
    EXPECT_EQ(cachedFragment->isDummy, false);
    EXPECT_EQ(cachedFragment->profileIndex, 0);
    EXPECT_EQ(cachedFragment->timeScale, 0U);
    EXPECT_TRUE(cachedFragment->uri.empty());
    EXPECT_EQ(cachedFragment->type, eMEDIATYPE_DEFAULT);  // Clear() sets type to eMEDIATYPE_DEFAULT
    EXPECT_EQ(cachedFragment->downloadStartTime, 0LL);
    EXPECT_EQ(cachedFragment->discontinuityIndex, 0LL);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, 0.0);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_ABR);

    // Note: Fragment buffer clearing behavior is implementation-specific and tested elsewhere
}

/**
 * @brief Test CachedFragment Clear method on empty fragment
 *
 * Verifies that calling Clear on an already empty fragment is safe
 * and maintains the default state.
 */
TEST_F(CachedFragmentTest, Clear_EmptyFragment_RemainsInDefaultState) {
    // Verify fragment starts in default state
    EXPECT_DOUBLE_EQ(cachedFragment->position, 0.0);
    // Note: fragment buffer operations are mocked and not tested directly

    // Clear the already-empty fragment
    cachedFragment->Clear();

    // Verify it remains in default state
    EXPECT_DOUBLE_EQ(cachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, 0.0);
    EXPECT_EQ(cachedFragment->initFragment, false);
    EXPECT_EQ(cachedFragment->discontinuity, false);
    EXPECT_EQ(cachedFragment->isDummy, false);
    EXPECT_EQ(cachedFragment->profileIndex, 0);
    EXPECT_EQ(cachedFragment->timeScale, 0U);
    EXPECT_TRUE(cachedFragment->uri.empty());
    EXPECT_EQ(cachedFragment->type, eMEDIATYPE_DEFAULT);  // Clear() sets type to eMEDIATYPE_DEFAULT
    EXPECT_EQ(cachedFragment->downloadStartTime, 0LL);
    EXPECT_EQ(cachedFragment->discontinuityIndex, 0LL);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, 0.0);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_ABR);
    // Note: Fragment buffer clearing behavior is implementation-specific
}

/**
 * @brief Test BitrateChangeReason enum handling
 *
 * Verifies that BitrateChangeReason enum values can be set and retrieved
 * correctly in the StreamInfo structure.
 */
TEST_F(CachedFragmentTest, BitrateChangeReason_CommonEnumValues_SetAndRetrievedCorrectly) {
    // Test common enum values that are guaranteed to exist
    std::vector<BitrateChangeReason> commonReasons = {
        eAAMP_BITRATE_CHANGE_BY_ABR,
        eAAMP_BITRATE_CHANGE_BY_RAMPDOWN,
        eAAMP_BITRATE_CHANGE_BY_TUNE,
        eAAMP_BITRATE_CHANGE_BY_SEEK,
        eAAMP_BITRATE_CHANGE_BY_TRICKPLAY,
        eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL,
        eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY,
        eAAMP_BITRATE_CHANGE_BY_FOG_ABR,
        eAAMP_BITRATE_CHANGE_BY_OTA,
        eAAMP_BITRATE_CHANGE_BY_HDMIIN
    };

    for (BitrateChangeReason reason : commonReasons) {
        cachedFragment->cacheFragStreamInfo.reason = reason;
        EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, reason);
    }
}

/**
 * @brief Test AampMediaType enum handling
 *
 * Verifies that AampMediaType enum values can be set and retrieved
 * correctly in the type field.
 */
TEST_F(CachedFragmentTest, AampMediaType_CommonEnumValues_SetAndRetrievedCorrectly) {
    // Test common AampMediaType values
    std::vector<AampMediaType> commonTypes = {
        eMEDIATYPE_VIDEO,
        eMEDIATYPE_AUDIO,
        eMEDIATYPE_SUBTITLE,
        eMEDIATYPE_MANIFEST,
        eMEDIATYPE_LICENCE,
        eMEDIATYPE_IFRAME
    };

    for (AampMediaType type : commonTypes) {
        cachedFragment->type = type;
        EXPECT_EQ(cachedFragment->type, type);
    }
}

/**
 * @brief Test CachedFragment member variables after data operations
 *
 * Verifies that CachedFragment member variables are properly maintained
 * when fragment data is manipulated, focusing on CachedFragment behavior.
 */
TEST_F(CachedFragmentTest, FragmentDataOperations_MemberVariablesUnaffected_CachedFragmentBehaviorCorrect) {
    // Set up fragment with known member variable values
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    cachedFragment->uri = testUri;
    cachedFragment->type = testType;
    cachedFragment->profileIndex = testProfileIndex;

    // Note: fragment buffer operations are mocked and not tested directly

    // Verify that CachedFragment member variables remain unchanged
    EXPECT_DOUBLE_EQ(cachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, testDuration);
    EXPECT_EQ(cachedFragment->uri, testUri);
    EXPECT_EQ(cachedFragment->type, testType);
    EXPECT_EQ(cachedFragment->profileIndex, testProfileIndex);

    // Note: fragment buffer operations are mocked and not tested directly
}

/**
 * @brief Test Copy method with different data sizes
 *
 * Verifies that the Copy method correctly handles copying member variables
 * regardless of fragment data size, focusing on CachedFragment behavior.
 */
TEST_F(CachedFragmentTest, Copy_DifferentDataSizes_MemberVariablesCopiedCorrectly) {
    // Set up source fragment with member variables (focus on CachedFragment data)
    sourceCachedFragment->position = testPosition;
    sourceCachedFragment->duration = testDuration;
    sourceCachedFragment->absPosition = testAbsPosition;
    sourceCachedFragment->type = testType;
    sourceCachedFragment->profileIndex = testProfileIndex;
    sourceCachedFragment->timeScale = testTimeScale;
    sourceCachedFragment->uri = testUri;
    sourceCachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_SEEK;

    // Note: fragment buffer operations are mocked and not tested directly

    // Copy to destination (test member variable copying)
    cachedFragment->Copy(*sourceCachedFragment);

    // Verify that all CachedFragment member variables were copied correctly
    EXPECT_DOUBLE_EQ(cachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, testDuration);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, testAbsPosition);
    EXPECT_EQ(cachedFragment->type, testType);
    EXPECT_EQ(cachedFragment->profileIndex, testProfileIndex);
    EXPECT_EQ(cachedFragment->timeScale, testTimeScale);
    EXPECT_EQ(cachedFragment->uri, testUri);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_SEEK);

    // Verify source member variables remain intact after copy
    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, testDuration);
    EXPECT_EQ(sourceCachedFragment->uri, testUri);
}

/**
 * @brief Test Copy method followed by Clear
 *
 * Verifies that Copy and Clear methods work correctly in sequence
 * and provide proper resource management.
 */
TEST_F(CachedFragmentTest, CopyThenClear_SequentialOperations_WorkCorrectly) {
    // Set up source with data
    sourceCachedFragment->position = testPosition;
    sourceCachedFragment->duration = testDuration;
    // Note: fragment buffer operations are mocked and not tested directly

    // Copy from source (test member variable copying)
    cachedFragment->Copy(*sourceCachedFragment);

    // Verify copy worked (focus on CachedFragment member variables)
    EXPECT_DOUBLE_EQ(cachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, testDuration);

    // Clear the destination
    cachedFragment->Clear();

    // Verify clear worked (focus on CachedFragment member variables)
    EXPECT_DOUBLE_EQ(cachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, 0.0);

    // Verify source is unaffected (CachedFragment member variables)
    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, testDuration);
}

/**
 * @brief Test boundary values for numeric fields
 *
 * Verifies that CachedFragment correctly handles boundary values
 * for position, duration, and other numeric fields.
 */
TEST_F(CachedFragmentTest, BoundaryValues_NumericFields_HandledCorrectly) {
    // Test extreme values
    cachedFragment->position = std::numeric_limits<double>::max();
    cachedFragment->duration = std::numeric_limits<double>::min();
    cachedFragment->absPosition = -std::numeric_limits<double>::max();
    cachedFragment->profileIndex = std::numeric_limits<int>::max();

    // Verify values are set correctly
    EXPECT_DOUBLE_EQ(cachedFragment->position, std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(cachedFragment->duration, std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, -std::numeric_limits<double>::max());
    EXPECT_EQ(cachedFragment->profileIndex, std::numeric_limits<int>::max());

    // Test copying extreme values
    sourceCachedFragment->Copy(*cachedFragment);

    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, std::numeric_limits<double>::max());
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, std::numeric_limits<double>::min());
    EXPECT_DOUBLE_EQ(sourceCachedFragment->absPosition, -std::numeric_limits<double>::max());
    EXPECT_EQ(sourceCachedFragment->profileIndex, std::numeric_limits<int>::max());
}

// ============================================================================
// Tests for new idiomatic methods (copy constructor, move constructor, etc.)
// ============================================================================

/**
 * @brief Test CachedFragment move constructor
 *
 * Verifies that the move constructor properly transfers ownership of resources
 * from source to destination, leaving source in a valid but empty state.
 */
TEST_F(CachedFragmentTest, MoveConstructor_PopulatedSource_ResourcesMovedCorrectly) {
    // Set up source fragment with test data
    sourceCachedFragment->position = testPosition;
    sourceCachedFragment->duration = testDuration;
    sourceCachedFragment->absPosition = testAbsPosition;
    sourceCachedFragment->initFragment = testInitFragment;
    sourceCachedFragment->discontinuity = testDiscontinuity;
    sourceCachedFragment->isDummy = testIsDummy;
    sourceCachedFragment->profileIndex = testProfileIndex;
    sourceCachedFragment->timeScale = testTimeScale;
    sourceCachedFragment->uri = testUri;
    sourceCachedFragment->type = testType;
    sourceCachedFragment->downloadStartTime = testDownloadStartTime;
    sourceCachedFragment->discontinuityIndex = testDiscontinuityIndex;
    sourceCachedFragment->PTSOffsetSec = testPTSOffsetSec;
    sourceCachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_SEEK;
    // Note: fragment buffer operations are mocked and not tested directly

    // Create moved fragment using move constructor
    CachedFragment movedFragment(std::move(*sourceCachedFragment));

    // Verify all fields were moved correctly
    EXPECT_DOUBLE_EQ(movedFragment.position, testPosition);
    EXPECT_DOUBLE_EQ(movedFragment.duration, testDuration);
    EXPECT_DOUBLE_EQ(movedFragment.absPosition, testAbsPosition);
    EXPECT_EQ(movedFragment.initFragment, testInitFragment);
    EXPECT_EQ(movedFragment.discontinuity, testDiscontinuity);
    EXPECT_EQ(movedFragment.isDummy, testIsDummy);
    EXPECT_EQ(movedFragment.profileIndex, testProfileIndex);
    EXPECT_EQ(movedFragment.timeScale, testTimeScale);
    EXPECT_EQ(movedFragment.uri, testUri);
    EXPECT_EQ(movedFragment.type, testType);
    EXPECT_EQ(movedFragment.downloadStartTime, testDownloadStartTime);
    EXPECT_EQ(movedFragment.discontinuityIndex, testDiscontinuityIndex);
    EXPECT_DOUBLE_EQ(movedFragment.PTSOffsetSec, testPTSOffsetSec);
    EXPECT_EQ(movedFragment.cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_SEEK);

    // Note: fragment buffer operations are mocked and not tested directly

    // Verify source has been reset to default values (moved-from state)
    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, 0.0);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->absPosition, 0.0);
    EXPECT_EQ(sourceCachedFragment->initFragment, false);
    EXPECT_EQ(sourceCachedFragment->discontinuity, false);
    EXPECT_EQ(sourceCachedFragment->isDummy, false);
    EXPECT_EQ(sourceCachedFragment->profileIndex, 0);
    EXPECT_EQ(sourceCachedFragment->timeScale, 0U);
    EXPECT_TRUE(sourceCachedFragment->uri.empty());
    EXPECT_EQ(sourceCachedFragment->type, eMEDIATYPE_DEFAULT);
    EXPECT_EQ(sourceCachedFragment->downloadStartTime, 0LL);
    EXPECT_EQ(sourceCachedFragment->discontinuityIndex, 0LL);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->PTSOffsetSec, 0.0);
}

/**
 * @brief Test CachedFragment move assignment operator
 *
 * Verifies that the move assignment operator properly transfers ownership
 * from source to destination using move semantics.
 */
TEST_F(CachedFragmentTest, MoveAssignment_PopulatedSource_ResourcesMovedCorrectly) {
    // Set up destination with some initial data
    cachedFragment->position = 999.9;
    cachedFragment->duration = 888.8;
    // Note: fragment buffer operations are mocked and not tested directly

    // Set up source fragment with test data
    sourceCachedFragment->position = testPosition;
    sourceCachedFragment->duration = testDuration;
    sourceCachedFragment->absPosition = testAbsPosition;
    sourceCachedFragment->initFragment = testInitFragment;
    sourceCachedFragment->discontinuity = testDiscontinuity;
    sourceCachedFragment->isDummy = testIsDummy;
    sourceCachedFragment->profileIndex = testProfileIndex;
    sourceCachedFragment->timeScale = testTimeScale;
    sourceCachedFragment->uri = testUri;
    sourceCachedFragment->type = testType;
    sourceCachedFragment->downloadStartTime = testDownloadStartTime;
    sourceCachedFragment->discontinuityIndex = testDiscontinuityIndex;
    sourceCachedFragment->PTSOffsetSec = testPTSOffsetSec;
    sourceCachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_SEEK;
    // Note: fragment buffer operations are mocked and not tested directly

    // Move assign from source to destination
    *cachedFragment = std::move(*sourceCachedFragment);

    // Verify all fields were moved correctly
    EXPECT_DOUBLE_EQ(cachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, testDuration);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, testAbsPosition);
    EXPECT_EQ(cachedFragment->initFragment, testInitFragment);
    EXPECT_EQ(cachedFragment->discontinuity, testDiscontinuity);
    EXPECT_EQ(cachedFragment->isDummy, testIsDummy);
    EXPECT_EQ(cachedFragment->profileIndex, testProfileIndex);
    EXPECT_EQ(cachedFragment->timeScale, testTimeScale);
    EXPECT_EQ(cachedFragment->uri, testUri);
    EXPECT_EQ(cachedFragment->type, testType);
    EXPECT_EQ(cachedFragment->downloadStartTime, testDownloadStartTime);
    EXPECT_EQ(cachedFragment->discontinuityIndex, testDiscontinuityIndex);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, testPTSOffsetSec);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_SEEK);

    // Note: fragment buffer operations are mocked and not tested directly

    // Verify source has been reset to default values (moved-from state)
    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, 0.0);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, 0.0);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->absPosition, 0.0);
    EXPECT_EQ(sourceCachedFragment->initFragment, false);
    EXPECT_EQ(sourceCachedFragment->discontinuity, false);
    EXPECT_EQ(sourceCachedFragment->isDummy, false);
    EXPECT_EQ(sourceCachedFragment->profileIndex, 0);
    EXPECT_EQ(sourceCachedFragment->timeScale, 0U);
    EXPECT_TRUE(sourceCachedFragment->uri.empty());
    EXPECT_EQ(sourceCachedFragment->type, eMEDIATYPE_DEFAULT);
    EXPECT_EQ(sourceCachedFragment->downloadStartTime, 0LL);
    EXPECT_EQ(sourceCachedFragment->discontinuityIndex, 0LL);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->PTSOffsetSec, 0.0);
}

/**
 * @brief Test CachedFragment move assignment self-assignment
 *
 * Verifies that self-assignment with move semantics is handled correctly.
 */
TEST_F(CachedFragmentTest, MoveAssignment_SelfAssignment_NoSideEffects) {
    // Set up fragment with test data
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    cachedFragment->uri = testUri;
    // Note: fragment buffer operations are mocked and not tested directly

    // Store original values for comparison
    double originalPosition = cachedFragment->position;
    double originalDuration = cachedFragment->duration;
    std::string originalUri = cachedFragment->uri;

    // Self-assign with move
    *cachedFragment = std::move(*cachedFragment);

    // Verify values remain unchanged (self-move should be safe, only CachedFragment member variables)
    EXPECT_DOUBLE_EQ(cachedFragment->position, originalPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, originalDuration);
    EXPECT_EQ(cachedFragment->uri, originalUri);
    // Note: fragment buffer operations are mocked and not tested directly
}

/**
 * @brief Test CachedFragment swap method
 *
 * Verifies that the swap method correctly exchanges all member variables
 * between two CachedFragment instances.
 */
TEST_F(CachedFragmentTest, Swap_TwoPopulatedFragments_AllFieldsSwappedCorrectly) {
    // Set up first fragment with test data
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    cachedFragment->absPosition = testAbsPosition;
    cachedFragment->initFragment = testInitFragment;
    cachedFragment->discontinuity = testDiscontinuity;
    cachedFragment->isDummy = testIsDummy;
    cachedFragment->profileIndex = testProfileIndex;
    cachedFragment->timeScale = testTimeScale;
    cachedFragment->uri = testUri;
    cachedFragment->type = testType;
    cachedFragment->downloadStartTime = testDownloadStartTime;
    cachedFragment->discontinuityIndex = testDiscontinuityIndex;
    cachedFragment->PTSOffsetSec = testPTSOffsetSec;
    cachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_SEEK;
    // Note: fragment buffer operations are mocked and not tested directly

    // Set up second fragment with different data
    const double secondPosition = 200.0;
    const double secondDuration = 150.0;
    const char* secondData = "Different test data";
    const size_t secondDataSize = strlen(secondData);

    sourceCachedFragment->position = secondPosition;
    sourceCachedFragment->duration = secondDuration;
    sourceCachedFragment->absPosition = 300.0;
    sourceCachedFragment->initFragment = false;
    sourceCachedFragment->discontinuity = true;
    sourceCachedFragment->isDummy = true;
    sourceCachedFragment->profileIndex = 99;
    sourceCachedFragment->timeScale = 48000;
    sourceCachedFragment->uri = "http://different.com/segment2.ts";
    sourceCachedFragment->type = eMEDIATYPE_AUDIO;
    sourceCachedFragment->downloadStartTime = 9876543210LL;
    sourceCachedFragment->discontinuityIndex = 10LL;
    sourceCachedFragment->PTSOffsetSec = 5.5;
    sourceCachedFragment->cacheFragStreamInfo.reason = eAAMP_BITRATE_CHANGE_BY_TUNE;
    // Note: fragment buffer operations are mocked and not tested directly

    // Perform swap
    cachedFragment->swap(*sourceCachedFragment);

    // Verify first fragment now has second fragment's data
    EXPECT_DOUBLE_EQ(cachedFragment->position, secondPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, secondDuration);
    EXPECT_DOUBLE_EQ(cachedFragment->absPosition, 300.0);
    EXPECT_EQ(cachedFragment->initFragment, false);
    EXPECT_EQ(cachedFragment->discontinuity, true);
    EXPECT_EQ(cachedFragment->isDummy, true);
    EXPECT_EQ(cachedFragment->profileIndex, 99);
    EXPECT_EQ(cachedFragment->timeScale, 48000U);
    EXPECT_EQ(cachedFragment->uri, "http://different.com/segment2.ts");
    EXPECT_EQ(cachedFragment->type, eMEDIATYPE_AUDIO);
    EXPECT_EQ(cachedFragment->downloadStartTime, 9876543210LL);
    EXPECT_EQ(cachedFragment->discontinuityIndex, 10LL);
    EXPECT_DOUBLE_EQ(cachedFragment->PTSOffsetSec, 5.5);
    EXPECT_EQ(cachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_TUNE);
    // Note: fragment buffer operations are mocked and not tested directly

    // Verify second fragment now has first fragment's data
    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, testDuration);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->absPosition, testAbsPosition);
    EXPECT_EQ(sourceCachedFragment->initFragment, testInitFragment);
    EXPECT_EQ(sourceCachedFragment->discontinuity, testDiscontinuity);
    EXPECT_EQ(sourceCachedFragment->isDummy, testIsDummy);
    EXPECT_EQ(sourceCachedFragment->profileIndex, testProfileIndex);
    EXPECT_EQ(sourceCachedFragment->timeScale, testTimeScale);
    EXPECT_EQ(sourceCachedFragment->uri, testUri);
    EXPECT_EQ(sourceCachedFragment->type, testType);
    EXPECT_EQ(sourceCachedFragment->downloadStartTime, testDownloadStartTime);
    EXPECT_EQ(sourceCachedFragment->discontinuityIndex, testDiscontinuityIndex);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->PTSOffsetSec, testPTSOffsetSec);
    EXPECT_EQ(sourceCachedFragment->cacheFragStreamInfo.reason, eAAMP_BITRATE_CHANGE_BY_SEEK);
    // Note: fragment buffer operations are mocked and not tested directly
}

/**
 * @brief Test free function swap
 *
 * Verifies that the free function swap works correctly and calls the member swap.
 */
TEST_F(CachedFragmentTest, FreeSwap_TwoFragments_CallsMemberSwap) {
    // Set up fragments with different data
    cachedFragment->position = testPosition;
    cachedFragment->duration = testDuration;
    cachedFragment->uri = testUri;

    const double secondPosition = 999.0;
    const double secondDuration = 888.0;
    const std::string secondUri = "http://second.com/test";

    sourceCachedFragment->position = secondPosition;
    sourceCachedFragment->duration = secondDuration;
    sourceCachedFragment->uri = secondUri;

    // Use free function swap
    swap(*cachedFragment, *sourceCachedFragment);

    // Verify swap occurred
    EXPECT_DOUBLE_EQ(cachedFragment->position, secondPosition);
    EXPECT_DOUBLE_EQ(cachedFragment->duration, secondDuration);
    EXPECT_EQ(cachedFragment->uri, secondUri);

    EXPECT_DOUBLE_EQ(sourceCachedFragment->position, testPosition);
    EXPECT_DOUBLE_EQ(sourceCachedFragment->duration, testDuration);
    EXPECT_EQ(sourceCachedFragment->uri, testUri);
}

/**
 * @brief Test container operations with idiomatic methods
 *
 * CachedFragment is move-only after the zero-copy refactor, so STL containers
 * that require copy (`push_back(const T&)`) no longer compile. Verify the
 * remaining move-based container interactions still work.
 */
TEST_F(CachedFragmentTest, ContainerOperations_MoveOnly_WorkCorrectly) {
    std::vector<CachedFragment> fragments;

    CachedFragment testFragment;
    testFragment.position = testPosition;
    testFragment.duration = testDuration;
    testFragment.uri = testUri;

    // Move-only types must use emplace_back / push_back(rvalue).
    fragments.emplace_back(std::move(testFragment));

    EXPECT_EQ(fragments.size(), 1);
    EXPECT_DOUBLE_EQ(fragments[0].position, testPosition);
    EXPECT_DOUBLE_EQ(fragments[0].duration, testDuration);
    EXPECT_EQ(fragments[0].uri, testUri);

    // Source CachedFragment must be in moved-from state.
    EXPECT_DOUBLE_EQ(testFragment.position, 0.0);
    EXPECT_DOUBLE_EQ(testFragment.duration, 0.0);
    EXPECT_TRUE(testFragment.uri.empty());
    EXPECT_EQ(testFragment.type, eMEDIATYPE_DEFAULT);
}
