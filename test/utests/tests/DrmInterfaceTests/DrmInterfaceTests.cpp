#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include "DrmInterface.h"
#include "MockAampLicManager.h"
#include "MockPrivateInstanceAAMP.h"
#include "priv_aamp.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// Required by the real Aes.cpp linked in for the AesDec type; never called by these tests.
void ResolveURL(std::string& dst, std::string base, const char *uri, bool bPropagateUriParams)
{
	dst = uri ? uri : "";
}

class DrmInterfaceTests : public ::testing::Test
{
protected:
	std::unique_ptr<PrivateInstanceAAMP> mAamp;
	std::unique_ptr<AampDRMLicenseManager> mLicenseManager;
	std::unique_ptr<DrmInterface> mDrmInterface;

	void SetUp() override
	{
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		g_mockAampLicenseManager = std::make_shared<NiceMock<MockAampLicenseManager>>();
		mAamp = std::make_unique<PrivateInstanceAAMP>();
		mLicenseManager = std::make_unique<AampDRMLicenseManager>(3, mAamp.get());
		mAamp->mDRMLicenseManager = mLicenseManager.get();
		mDrmInterface = std::make_unique<DrmInterface>(mAamp.get());
	}

	void TearDown() override
	{
		mDrmInterface.reset();
		mAamp->mDRMLicenseManager = nullptr;
		mLicenseManager.reset();
		mAamp.reset();
		g_mockAampLicenseManager.reset();
		g_mockPrivateInstanceAAMP.reset();
	}
};

/**
 * @brief The active sessions are flagged before downloads are disabled and the error is reported.
 */
TEST_F(DrmInterfaceTests, DecryptFailureFlagsSessionsBeforeReportingError)
{
	ON_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillByDefault(Return(true));

	testing::Sequence sequence;
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).InSequence(sequence);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DisableDownloads()).InSequence(sequence);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_DRM_DECRYPT_FAILED, _, _, _, _, _, _)).InSequence(sequence);

	mDrmInterface->NotifyDrmError(AAMP_TUNE_DRM_DECRYPT_FAILED);
}

/**
 * @brief The sessions are flagged even when downloads were already disabled (error already reported).
 */
TEST_F(DrmInterfaceTests, FailureFlagsSessionsWhenDownloadsAlreadyDisabled)
{
	ON_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillByDefault(Return(false));
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DisableDownloads()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);

	mDrmInterface->NotifyDrmError(AAMP_TUNE_DRM_DECRYPT_FAILED);
}

/**
 * @brief Regression: other DRM failure codes keep reporting exactly as before.
 */
TEST_F(DrmInterfaceTests, NonDecryptFailureStillFlagsAndReports)
{
	ON_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillByDefault(Return(true));
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DisableDownloads());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_INVALID_DRM_KEY, nullptr, _, _, _, _, _));

	mDrmInterface->NotifyDrmError(AAMP_TUNE_INVALID_DRM_KEY);
}

/**
 * @brief Regression: untracked DRM errors still carry the description.
 */
TEST_F(DrmInterfaceTests, UntrackedErrorSendsDescription)
{
	ON_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillByDefault(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DisableDownloads());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_UNTRACKED_DRM_ERROR, testing::StrEq("AAMP: DRM Failure"), _, _, _, _, _));

	mDrmInterface->NotifyDrmError(AAMP_TUNE_UNTRACKED_DRM_ERROR);
}

/**
 * @brief A missing license manager must not prevent the failure from being reported.
 */
TEST_F(DrmInterfaceTests, NoLicenseManagerStillReportsError)
{
	mAamp->mDRMLicenseManager = nullptr;
	ON_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillByDefault(Return(true));
	EXPECT_CALL(*g_mockAampLicenseManager, markActiveSessionsFailed()).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DisableDownloads());
	EXPECT_CALL(*g_mockPrivateInstanceAAMP,
		SendErrorEvent(AAMP_TUNE_DRM_DECRYPT_FAILED, _, _, _, _, _, _));

	mDrmInterface->NotifyDrmError(AAMP_TUNE_DRM_DECRYPT_FAILED);
}

