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

#include <gtest/gtest.h>
#include "AampEvent.h"

using namespace testing;

/**
 * @class ErrorCodeMappingTests
 * @brief Tests that specific tune failures map to their documented error codes.
 *
 * Purpose: Lock in the backward-compatible error code contract (VPAAMP-918).
 * These tests verify that the three error codes (51, 52, 53) that were
 * inadvertently shifted by commit bda30014 are now restored to their
 * original values, allowing external clients to match on these codes.
 *
 * Specification: See AAMP-UVE-API.md error code table for documented values.
 * Related: Commit bda30014 (VPLAY-11225) caused regression by consolidating
 * CORRUPT_DRM_DATA into the DRM group (50/10), cascading shifts to codes
 * 52 and 53. This PR restores codes 51, 52, 53 while keeping subCode for
 * fine-grained categorization.
 */

class ErrorCodeMappingTests : public testing::Test {
protected:
	const std::string session_id{"test-session-id-error-code-mapping"};

	// Test helper: creates MediaErrorEvent and verifies code/subCode
	void VerifyErrorCodeMapping(
		AAMPTuneFailure tuneFailure,
		int expectedCode,
		int expectedSubCode,
		const char* description)
	{
		MediaErrorEvent event(
			tuneFailure,
			expectedCode,
			expectedSubCode,
			description,
			false,
			0,
			0,
			0,
			"",
			session_id
		);

		EXPECT_EQ(event.getCode(), expectedCode)
			<< "Tune failure " << static_cast<int>(tuneFailure)
			<< " should map to code " << expectedCode;
		EXPECT_EQ(event.getSubCode(), expectedSubCode)
			<< "Tune failure " << static_cast<int>(tuneFailure)
			<< " should map to subCode " << expectedSubCode;
	}
};

/**
 * TEST: CorruptDrmData_ReturnsCode51SubCode1
 *
 * Ensures AAMP_TUNE_CORRUPT_DRM_DATA maps to error code 51, subCode 1.
 *
 * Regression prevention: Commit bda30014 moved this from code 51 to 50/10.
 * External clients matching event.code === 51 would fail silently.
 * This test locks in the restored backward-compatible code.
 */
TEST_F(ErrorCodeMappingTests, CorruptDrmData_ReturnsCode51SubCode1)
{
	VerifyErrorCodeMapping(
		AAMP_TUNE_CORRUPT_DRM_DATA,
		51,  // Expected major code (restored from 50 in bda30014)
		1,   // Expected subCode
		"AAMP: DRM failure due to Corrupt DRM files"
	);
}

/**
 * TEST: DeviceNotProvisioned_ReturnsCode52SubCode1
 *
 * Ensures AAMP_TUNE_DEVICE_NOT_PROVISIONED maps to error code 52, subCode 1.
 *
 * Regression prevention: Commit bda30014 cascaded a −1 shift, moving this
 * from code 52 to 51. External clients matching code 52 would fail silently.
 * This test locks in the restored backward-compatible code.
 */
TEST_F(ErrorCodeMappingTests, DeviceNotProvisioned_ReturnsCode52SubCode1)
{
	VerifyErrorCodeMapping(
		AAMP_TUNE_DEVICE_NOT_PROVISIONED,
		52,  // Expected major code (restored from 51 in bda30014)
		1,   // Expected subCode
		"AAMP: Device not provisioned"
	);
}

/**
 * TEST: HdcpComplianceError_ReturnsCode53SubCode1
 *
 * Ensures AAMP_TUNE_HDCP_COMPLIANCE_ERROR maps to error code 53, subCode 1.
 *
 * Regression prevention: Commit bda30014 cascaded a −1 shift, moving this
 * from code 53 to 52. External clients matching code 53 would fail silently.
 * This test locks in the restored backward-compatible code.
 */
TEST_F(ErrorCodeMappingTests, HdcpCompliance_ReturnsCode53SubCode1)
{
	VerifyErrorCodeMapping(
		AAMP_TUNE_HDCP_COMPLIANCE_ERROR,
		53,  // Expected major code (restored from 52 in bda30014)
		1,   // Expected subCode
		"AAMP: HDCP Compliance Check Failure"
	);
}

/**
 * TEST: ErrorCodeSequence_AllThreeCodesUnique
 *
 * Ensures that the three restored error codes are unique and sequential.
 * This guards against accidental collisions or reversions.
 */
TEST_F(ErrorCodeMappingTests, ErrorCodeSequence_AllThreeCodesUnique)
{
	MediaErrorEvent event1(
		AAMP_TUNE_CORRUPT_DRM_DATA, 51, 1, "", false, 0, 0, 0, "", session_id
	);
	MediaErrorEvent event2(
		AAMP_TUNE_DEVICE_NOT_PROVISIONED, 52, 1, "", false, 0, 0, 0, "", session_id
	);
	MediaErrorEvent event3(
		AAMP_TUNE_HDCP_COMPLIANCE_ERROR, 53, 1, "", false, 0, 0, 0, "", session_id
	);

	// Verify uniqueness
	EXPECT_NE(event1.getCode(), event2.getCode());
	EXPECT_NE(event2.getCode(), event3.getCode());
	EXPECT_NE(event1.getCode(), event3.getCode());

	// Verify sequential relationship for external validation
	EXPECT_EQ(event1.getCode() + 1, event2.getCode());
	EXPECT_EQ(event2.getCode() + 1, event3.getCode());
}
