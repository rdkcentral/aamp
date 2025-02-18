/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include <filesystem>
#include <ios>
#include <memory>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "TsbApi.h"

#include "TsbMockFilesystem.h"
#include "TsbMockOfstream.h"
#include "TsbMockIfstream.h"
#include "TsbMockBasicFilebuf.h"
#include "TsbMockDirectoryIterator.h"
#include "TsbMockLibc.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Expectation;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::SetArrayArgument;
using ::testing::SetErrnoAndReturn;
using ::testing::Assign;
using ::testing::ReturnPointee;
using ::testing::StrEq;

namespace fs = std::filesystem;
using namespace std::chrono_literals;

#define TSB_TEST_BYTES_IN_MIB   (1024 * 1024)

const uintmax_t kCapacity{100}; // Using 100 makes percentage calculations easier
const uint32_t kMinFreePercent{5};
const uint32_t kMaxCapacity{1000}; // 1GB
const auto kFlushWaitTime{10ms};
const std::string kTsbLocation{"/tmp/tsb_location_MFT"}; // MFT = Mocked Filesystem Test
const std::string kFlushDir{kTsbLocation + "/0"};
const std::string kActiveDir{kTsbLocation + "/1"};
const std::string kFile{"file.mp4"};
const std::string kDir{"lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t"};
const std::string kUrl{"https://" + kDir + "/" + kFile};
const std::string kFileIncPath{kActiveDir + "/" + kDir + "/" + kFile};
const std::string kDirIncPath{kActiveDir + "/" + kDir};
const char kFileContent[]{"content of the file"};
const int kTsbLocationFd{1};

void Logger(std::string&& tsbMessage)
{
	std::cout << "[MFT]" << std::move(tsbMessage) << std::endl;
}

class TsbStoreTests : public ::testing::Test
{
protected:
	TsbMockBasicFileBuf* mMockBasicFileBuf{nullptr};

	void SetUp() override
	{
		g_mockFilesystem = new NiceMock<TsbMockFilesystem>();
		g_mockOfstream = new NiceMock<TsbMockOfstream>();
		g_mockIfstream = new NiceMock<TsbMockIfstream>();
		mMockBasicFileBuf = new NiceMock<TsbMockBasicFileBuf>();
		g_mockDirectoryIterator = new NiceMock<TsbMockDirectorIterator>();
		g_mockLibc = new NiceMock<TsbMockLibc>();
	}

	void TearDown() override
	{
		delete g_mockLibc;
		g_mockLibc = nullptr;

		delete g_mockDirectoryIterator;
		g_mockDirectoryIterator = nullptr;

		delete mMockBasicFileBuf;
		mMockBasicFileBuf = nullptr;

		delete g_mockIfstream;
		g_mockIfstream = nullptr;

		delete g_mockOfstream;
		g_mockOfstream = nullptr;

		delete g_mockFilesystem;
		g_mockFilesystem = nullptr;
	}

	TSB::Status performWrite(TSB::Store& store, const std::string& pathToWrite,
							 const std::pair<const void*, std::size_t>& buffer =
								 std::make_pair(kFileContent, sizeof(kFileContent)))
	{
		// Always need this on Write to avoid a null pointer segfault
		EXPECT_CALL(*g_mockOfstream, rdbuf()).WillRepeatedly(Return(mMockBasicFileBuf));

		return store.Write(pathToWrite, buffer.first, buffer.second);
	}

	TSB::Status performWriteInvalidUrl(TSB::Store& store, const std::string& pathToWrite,
									   const std::pair<const void*, std::size_t>& buffer =
										   std::make_pair(kFileContent, sizeof(kFileContent)))
	{
		EXPECT_CALL(*g_mockFilesystem, exists(_)).Times(0);
		EXPECT_CALL(*g_mockFilesystem, create_directory(_, _)).Times(0);
		EXPECT_CALL(*mMockBasicFileBuf, pubsetbuf(_, _)).Times(0);
		EXPECT_CALL(*g_mockOfstream, open(_, _)).Times(0);

		return store.Write(pathToWrite, buffer.first, buffer.second);
	}

	void waitForFlushCompletion()
	{
		// Wait for the call to remove_all to complete as this will be the flush
		while (g_mockFilesystem->mockRemoveAllCompleted.load() == false)
		{
			std::this_thread::sleep_for(kFlushWaitTime);
		}

		// remove_all completed, reset the sem and flag for any subsequent flushes
		g_mockFilesystem->mockRemoveAllSem = nullptr;
		g_mockFilesystem->mockRemoveAllCompleted.store(false);
	}

	void createDirectoriesExpectations(fs::path path = fs::path{kFlushDir},
									   fs::path existingPath = fs::path{kTsbLocation})
	{
		fs::path currentPath;

		for (auto it = path.begin(); it != path.end(); ++it)
		{
			currentPath /= *it;
			if ((currentPath.string().find(existingPath.string()) == 0) &&
				(currentPath.string().length() <= existingPath.string().length()))
			{
				EXPECT_CALL(*g_mockFilesystem, exists(currentPath)).WillOnce(Return(true));
			}
			else
			{
				EXPECT_CALL(*g_mockFilesystem, exists(currentPath)).WillOnce(Return(false));
				EXPECT_CALL(*g_mockFilesystem, create_directory(currentPath, _))
					.WillOnce(Return(true));
				EXPECT_CALL(*g_mockFilesystem, permissions(currentPath, std::filesystem::perms::all, _))
					.Times(1);
			}
		}
	}

	// Having separate construct methods allows the tests to set extra expectations
	// before construction, when needed - for example for construction or Flush tests.
	std::unique_ptr<TSB::Store> createStore(const std::string& location, uint32_t minFreePercent,
											uint32_t maxCapacity)
	{
		TSB::Store::Config tsbConfig = {location, minFreePercent, maxCapacity};
		auto store = std::make_unique<TSB::Store>(tsbConfig, Logger, TSB::LogLevel::TRACE);

		waitForFlushCompletion();
		return store;
	}

	std::unique_ptr<TSB::Store> createStoreDefault()
	{
		EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
			.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
							Return(fs::space_info{kCapacity, 0, 0})));

		return createStore(kTsbLocation, kMinFreePercent, kMaxCapacity);
	}
};

TEST_F(TsbStoreTests, Clean_Create_Destroy_TrailingSlash)
{
	createDirectoriesExpectations();

	createStore(kTsbLocation + "/", kMinFreePercent, kMaxCapacity);
}

TEST_F(TsbStoreTests, CreateDestroySuccess)
{
	createDirectoriesExpectations();
	EXPECT_CALL(*g_mockLibc, open(StrEq(kTsbLocation.c_str()), O_RDONLY | O_DIRECTORY))
		.WillOnce(Return(kTsbLocationFd));
	EXPECT_CALL(*g_mockLibc, flock(kTsbLocationFd, LOCK_EX | LOCK_NB))
		.WillOnce(Return(0));

	// Pretend an old Flush and Active directory still exist,
	// so the directory iterator must iterate twice before reaching the end
	EXPECT_CALL(*g_mockDirectoryIterator, NotEq(_))
		.WillOnce(Return(true))
		.WillOnce(Return(true))
		.WillOnce(Return(false));
	EXPECT_CALL(*g_mockDirectoryIterator, DeRef())
		.WillOnce(Return(fs::directory_entry("1")))
		.WillOnce(Return(fs::directory_entry("2")));
	EXPECT_CALL(*g_mockFilesystem, rename(fs::path("1"), fs::path(kFlushDir + "/1"), _));
	EXPECT_CALL(*g_mockFilesystem, rename(fs::path("2"), fs::path(kFlushDir + "/2"), _));

	Expectation constructFlush =
		EXPECT_CALL(*g_mockFilesystem, remove_all(fs::path(kFlushDir), _)).WillOnce(Return(1));

	std::unique_ptr<TSB::Store> store = createStoreDefault();

	Expectation destructFlush = EXPECT_CALL(*g_mockFilesystem, remove_all(fs::path(kActiveDir), _))
									.After(constructFlush)
									.WillOnce(Return(1));
	EXPECT_CALL(*g_mockLibc, close(kTsbLocationFd)).WillOnce(Return(0));
}

TEST_F(TsbStoreTests, WriteSuccess)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	createDirectoriesExpectations(fs::path{kDirIncPath}, fs::path{kTsbLocation});
	EXPECT_CALL(*mMockBasicFileBuf, pubsetbuf(nullptr, 0));
	EXPECT_CALL(*g_mockOfstream, open(kFileIncPath, static_cast<std::ios_base::openmode>(1)));
	std::filesystem::perms permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
										 std::filesystem::perms::group_read | std::filesystem::perms::group_write |
										 std::filesystem::perms::others_read | std::filesystem::perms::others_write;
	EXPECT_CALL(*g_mockFilesystem, permissions(fs::path(kFileIncPath), permissions, _)).Times(1);
	EXPECT_CALL(*g_mockOfstream, write(kFileContent, sizeof(kFileContent)));
	EXPECT_CALL(*g_mockOfstream, close());

	ASSERT_THAT(performWrite(*store, kUrl), TSB::Status::OK);
}

TEST_F(TsbStoreTests, WriteFailExists)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	ASSERT_THAT(store->Write(kUrl, kFileContent, sizeof(kFileContent)), TSB::Status::ALREADY_EXISTS);
}

TEST_F(TsbStoreTests, WriteFailNoSpace)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	createDirectoriesExpectations(fs::path{kDirIncPath}, fs::path{kTsbLocation});
	// Always need this on Write to avoid a null pointer segfault
	EXPECT_CALL(*g_mockOfstream, rdbuf()).WillOnce(Return(mMockBasicFileBuf));
	EXPECT_CALL(*mMockBasicFileBuf, pubsetbuf(nullptr, 0));
	EXPECT_CALL(*g_mockOfstream, open(kFileIncPath, static_cast<std::ios_base::openmode>(1)));
	std::filesystem::perms permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
										 std::filesystem::perms::group_read | std::filesystem::perms::group_write |
										 std::filesystem::perms::others_read | std::filesystem::perms::others_write;
	EXPECT_CALL(*g_mockFilesystem, permissions(fs::path(kFileIncPath), permissions, _)).Times(1);
	EXPECT_CALL(*g_mockOfstream, write(kFileContent, sizeof(kFileContent)));
	EXPECT_CALL(*g_mockOfstream, fail())
		.WillOnce(Return(false))                       // Called after open
		.WillOnce(Return(true));                       // Called after write
	EXPECT_CALL(*g_mockOfstream, bad())
		.WillOnce(SetErrnoAndReturn(ENOSPC, true));    // Called after write, fails with no space
	EXPECT_CALL(*g_mockOfstream, close());
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(kFileIncPath), _)).WillOnce(Return(true));

	ASSERT_THAT(store->Write(kUrl, kFileContent, sizeof(kFileContent)), TSB::Status::NO_SPACE);
}

TEST_F(TsbStoreTests, WriteFailFault)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	createDirectoriesExpectations(fs::path{kDirIncPath}, fs::path{kTsbLocation});
	// Always need this on Write to avoid a null pointer segfault
	EXPECT_CALL(*g_mockOfstream, rdbuf()).WillOnce(Return(mMockBasicFileBuf));
	EXPECT_CALL(*mMockBasicFileBuf, pubsetbuf(nullptr, 0));
	EXPECT_CALL(*g_mockOfstream, open(kFileIncPath, static_cast<std::ios_base::openmode>(1)));
	std::filesystem::perms permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
										 std::filesystem::perms::group_read | std::filesystem::perms::group_write |
										 std::filesystem::perms::others_read | std::filesystem::perms::others_write;
	EXPECT_CALL(*g_mockFilesystem, permissions(fs::path(kFileIncPath), permissions, _)).Times(1);
	EXPECT_CALL(*g_mockOfstream, write(kFileContent, sizeof(kFileContent)));
	EXPECT_CALL(*g_mockOfstream, fail())
		.WillOnce(Return(false))                       // Called after open
		.WillOnce(SetErrnoAndReturn(EFAULT, true));    // Called after write, fails with no space
	EXPECT_CALL(*g_mockOfstream, close());
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(kFileIncPath), _)).WillOnce(Return(true));

	ASSERT_THAT(store->Write(kUrl, kFileContent, sizeof(kFileContent)), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, ReadSuccess)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	char readBuffer[sizeof(kFileContent)] = {};

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIfstream, open(kFileIncPath, static_cast<std::ios_base::openmode>(1)));
	EXPECT_CALL(*g_mockIfstream, read(NotNull(), sizeof(readBuffer)))
		.WillOnce(SetArrayArgument<0>(kFileContent, kFileContent + sizeof(kFileContent)));
	EXPECT_CALL(*g_mockIfstream, close());

	ASSERT_THAT(store->Read(kUrl, readBuffer, sizeof(readBuffer)), TSB::Status::OK);
	ASSERT_THAT(std::memcmp(readBuffer, kFileContent, sizeof(readBuffer)), 0);
}

TEST_F(TsbStoreTests, ReadFailNotExists)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	char readBuffer[1024];

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	ASSERT_THAT(store->Read(kUrl, readBuffer, sizeof(readBuffer)), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, ReadFailBadRead)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	char readBuffer[1024];

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIfstream, fail()).WillOnce(Return(true));
	ASSERT_THAT(store->Read(kUrl, readBuffer, sizeof(readBuffer)), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, ReadFaillNotEnoughData)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	char readBuffer[1024];

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIfstream, fail()).WillOnce(Return(false)).WillOnce(Return(true));
	ASSERT_THAT(store->Read(kUrl, readBuffer, sizeof(readBuffer)), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, ReadNullBuffer)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	char readBuffer[1024];

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIfstream, open(_, _)).Times(0);

	ASSERT_THAT(store->Read(kUrl, nullptr, sizeof(readBuffer)), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, ReadZeroSize)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	char readBuffer[1024];

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIfstream, open(_, _)).Times(0);

	ASSERT_THAT(store->Read(kUrl, readBuffer, 0), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, DeleteSuccess)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, file_size(fs::path(kFileIncPath), _))
		.WillOnce(Return(sizeof(kFileContent)));
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(kFileIncPath), _)).WillOnce(Return(true));

	store->Delete(kUrl);
}

TEST_F(TsbStoreTests, DeleteFailNoFile)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	EXPECT_CALL(*g_mockFilesystem, file_size(fs::path(kFileIncPath), _))
			.Times(0);
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(kFileIncPath), _)).Times(0);
	store->Delete(kUrl);
}

TEST_F(TsbStoreTests, DeleteFailOnRemove)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(kFileIncPath), _)).WillOnce(Return(false));
	store->Delete(kUrl);
}

TEST_F(TsbStoreTests, FlushSuccess)
{
	// Ignore flushes on construction and destruction, as these are tested in CreateDestroySuccess
	EXPECT_CALL(*g_mockFilesystem, remove_all(_, _)).Times(AnyNumber());

	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockFilesystem, remove_all(fs::path(kActiveDir), _)).WillOnce(Return(1));

	store->Flush();
	waitForFlushCompletion();
}

TEST_F(TsbStoreTests, WriteNoSpace)
{
	uint32_t fsCapacity = 200;
	uint32_t minFreePercent = 10;
	uint32_t availableCapacity = (fsCapacity * (100 - minFreePercent)) / 100;
	// Test precondition: available capacity is exactly divisible by 2
	ASSERT_TRUE(availableCapacity % 2 == 0);
	uint32_t segmentSize = availableCapacity / 2;
	const std::string url2 = kUrl + "_url2";
	const std::string url3 = kUrl + "_url3";
	std::pair<const void*, std::size_t> buffer = std::make_pair(kFileContent, segmentSize);

	// Exists calls will return false, except on Delete
	EXPECT_CALL(*g_mockFilesystem, exists(_)).WillRepeatedly(Return(false));

	// Make the filesystem mock return the test capacity
	EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
						Return(fs::space_info{fsCapacity, 0, 0})));

	Logger("Create Store");
	std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, minFreePercent, kMaxCapacity);

	Logger("Verify that Write checks free space");
	ASSERT_THAT(performWrite(*store, kUrl, buffer), TSB::Status::OK);
	ASSERT_THAT(performWrite(*store, url2, buffer), TSB::Status::OK);
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);

	Logger("Delete single segment (failure)");
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, file_size(_, _)).WillOnce(Return(segmentSize));
	EXPECT_CALL(*g_mockFilesystem, remove(_, _)).WillOnce(Return(false));
	store->Delete(kUrl);

	Logger("Verify that failed Delete does not free space");
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);

	Logger("Delete single segment (success)");
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, file_size(_, _)).WillOnce(Return(segmentSize));
	EXPECT_CALL(*g_mockFilesystem, remove(_, _)).WillOnce(Return(true));
	store->Delete(kUrl);

	Logger("Verify that successful Delete frees single segment space, only");
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	ASSERT_THAT(performWrite(*store, kUrl, buffer), TSB::Status::OK);
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);

	Logger("Flush all segments");
	store->Flush();
	waitForFlushCompletion();

	Logger("Verify that Flush frees all space");
	ASSERT_THAT(performWrite(*store, kUrl, buffer), TSB::Status::OK);
	ASSERT_THAT(performWrite(*store, url2, buffer), TSB::Status::OK);
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);
}

TEST_F(TsbStoreTests, WriteNoSpace_MaxCapacity)
{
	const uint32_t fsCapacity = 20000000;                                      // in bytes
	const uint32_t minFreePercent = 10;
	const uint32_t availableCapacity = (fsCapacity * (100 - minFreePercent)) / 100;
	const uint32_t maxCapacityBytes = 10485760;                                // in bytes (1024 * 1024)
	const uint32_t maxCapacityMB = maxCapacityBytes / TSB_TEST_BYTES_IN_MIB;   // in MiB
	// Test precondition: maximum capacity in MB is greater than 0
	ASSERT_GT(maxCapacityBytes, 0);
	// Test precondition: maximum capacity set is exactly divisible by 2
	ASSERT_TRUE(maxCapacityBytes % 2 == 0);
	// Test precondition: maximum capacity set is lower than the available partition capacity
	ASSERT_LT(maxCapacityBytes, availableCapacity);
	const uint32_t segmentSize = maxCapacityBytes / 2;
	const std::string url2 = kUrl + "_url2";
	const std::string url3 = kUrl + "_url3";
	std::pair<const void*, std::size_t> buffer = std::make_pair(kFileContent, segmentSize);

	// Exists calls will return false, except on Delete
	EXPECT_CALL(*g_mockFilesystem, exists(_)).WillRepeatedly(Return(false));

	// Make the filesystem mock return the test capacity
	EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
						Return(fs::space_info{fsCapacity, 0, 0})));

	Logger("Create Store");
	std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, minFreePercent, maxCapacityMB);

	Logger("Verify that Write checks free space");
	ASSERT_THAT(performWrite(*store, kUrl, buffer), TSB::Status::OK);
	ASSERT_THAT(performWrite(*store, url2, buffer), TSB::Status::OK);
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);

	Logger("Delete single segment (failure)");
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, file_size(_, _)).WillOnce(Return(segmentSize));
	EXPECT_CALL(*g_mockFilesystem, remove(_, _)).WillOnce(Return(false));
	store->Delete(kUrl);

	Logger("Verify that failed Delete does not free space");
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);

	Logger("Delete single segment (success)");
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, file_size(_, _)).WillOnce(Return(segmentSize));
	EXPECT_CALL(*g_mockFilesystem, remove(_, _)).WillOnce(Return(true));
	store->Delete(kUrl);

	Logger("Verify that successful Delete frees single segment space, only");
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));
	ASSERT_THAT(performWrite(*store, kUrl, buffer), TSB::Status::OK);
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);

	Logger("Flush all segments");
	store->Flush();
	waitForFlushCompletion();

	Logger("Verify that Flush frees all space");
	ASSERT_THAT(performWrite(*store, kUrl, buffer), TSB::Status::OK);
	ASSERT_THAT(performWrite(*store, url2, buffer), TSB::Status::OK);
	ASSERT_THAT(store->Write(url3, kFileContent, 1), TSB::Status::NO_SPACE);
}

TEST_F(TsbStoreTests, WriteNullBuffer)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockOfstream, open(_, _)).Times(0);

	ASSERT_THAT(store->Write(kUrl, nullptr, sizeof(kFileContent)), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, WriteZeroSize)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	EXPECT_CALL(*g_mockOfstream, open(_, _)).Times(0);

	ASSERT_THAT(store->Write(kUrl, kFileContent, 0), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, CreateLargeStore)
{
	// Make the filesystem mock return a "large" capacity a bit over 4GB (UINT32_MAX)
	auto largeCapacity = static_cast<uintmax_t>(UINT32_MAX) + 1;
	EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
						Return(fs::space_info{largeCapacity, 0, 0})));

	std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, 0, UINT32_MAX);

	// This Write should succeed if the Store supports capacities larger than 32-bit numbers
	ASSERT_THAT(performWrite(*store, kUrl), TSB::Status::OK);
}

TEST_F(TsbStoreTests, CreateCannotGetCapacity)
{
	// Make the filesystem mock capacity function fail
	EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
						Return(fs::space_info{static_cast<uintmax_t>(-1), 0, 0})));

	ASSERT_THROW(std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, 0, 0),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, CreateCannotCreateLocationDirectory)
{
	fs::path path{kTsbLocation};
	fs::path currentPath;

	for (auto it = path.begin(); it != path.end(); ++it)
	{
		currentPath /= *it;
		if (std::next(it) == path.end())
		{
			EXPECT_CALL(*g_mockFilesystem, exists(currentPath)).WillOnce(Return(false));
			EXPECT_CALL(*g_mockFilesystem, create_directory(currentPath, _))
				.WillOnce(DoAll(SetArgReferee<1>(std::make_error_code(std::errc::permission_denied)),
								Return(false)));
		}
		else
		{
			EXPECT_CALL(*g_mockFilesystem, exists(currentPath)).WillOnce(Return(true));
		}
	}

	ASSERT_THROW(std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, 0, 0),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, CreateCannotOpenLocationDirectory)
{
	EXPECT_CALL(*g_mockLibc, open(StrEq(kTsbLocation.c_str()), _)).WillOnce(Return(-1));

	ASSERT_THROW(std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, 0, 0),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, CreateCannotLockLocationDirectory)
{
	EXPECT_CALL(*g_mockLibc, open(StrEq(kTsbLocation.c_str()), _)).WillOnce(Return(kTsbLocationFd));
	EXPECT_CALL(*g_mockLibc, flock(kTsbLocationFd, _)).WillOnce(Return(-1)); // Fail locking
	EXPECT_CALL(*g_mockLibc, close(kTsbLocationFd)).WillOnce(Return(0));

	ASSERT_THROW(std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, 0, 0),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, CreateCannotCreateFlushDirectory)
{
	createDirectoriesExpectations(fs::path{kTsbLocation});
	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFlushDir))).WillOnce(Return(false));
	EXPECT_CALL(*g_mockFilesystem, create_directory(fs::path(kFlushDir), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::make_error_code(std::errc::permission_denied)),
						Return(false)));

	ASSERT_THROW(std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, 0, 0),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, CreateInvalidMinFreePercent)
{
	uint32_t invalidPercent = 101;
	ASSERT_THROW(std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, invalidPercent, 0),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, CreateMaximumMinFreePercent)
{
	EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
						Return(fs::space_info{kCapacity, 0, 0})));

	uint32_t maxPercent = 100;
	std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, maxPercent, kMaxCapacity);

	// This Write should fail as 100% of the storage must remain free
	ASSERT_THAT(store->Write(kUrl, kFileContent, 1), TSB::Status::NO_SPACE);
}

TEST_F(TsbStoreTests, CreateMinimumMinFreePercent)
{
	EXPECT_CALL(*g_mockFilesystem, space(fs::path(kTsbLocation), _))
		.WillOnce(DoAll(SetArgReferee<1>(std::error_code()),
						Return(fs::space_info{kCapacity, 0, 0})));

	uint32_t minPercent = 0;
	std::unique_ptr<TSB::Store> store = createStore(kTsbLocation, minPercent, kMaxCapacity);

	// This full capacity Write should succeed
	ASSERT_THAT(performWrite(*store, kUrl, std::make_pair(kFileContent, kCapacity)),
				TSB::Status::OK);
}

TEST_F(TsbStoreTests, UrlToFileMapperEmptyUrl)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	ASSERT_THAT(performWriteInvalidUrl(*store, ""), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, UrlToFileMapperNoFile)
{
	const std::string urlNoFile{
		"https://lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/"};
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	ASSERT_THAT(performWriteInvalidUrl(*store, urlNoFile), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, UrlToFileMapperExtraDelimiter)
{
	const std::string urlExtraDelimiter{
		"https:///lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/file2.mp4"};
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	ASSERT_THAT(performWriteInvalidUrl(*store, urlExtraDelimiter), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, UrlToFileMapperTwoDirectoryDelimiter)
{
	const std::string urlTwoDirectoryDelimiter{
		"https://lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1//frag/bmff/enc/cenc/t/file.mp4"};
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	ASSERT_THAT(performWriteInvalidUrl(*store, urlTwoDirectoryDelimiter), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, UrlToFileMapperInvalidStorageAccess)
{
	const std::string urlWithInvalidStorageAccess{
		"https://..lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1//frag/bmff/enc/cenc/t/file.mp4"};
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	ASSERT_THAT(performWriteInvalidUrl(*store, urlWithInvalidStorageAccess), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, UrlToFileMapperAllChars)
{
    std::string stringWithAllChars;

    // Check that TSB APIs accept URLs with any 8 bit char except 0.
    // Loop through character values 1 to 255 and append them to the string
    for (int i = 1; i < 256; ++i) {
        char c = static_cast<char>(i);
        if (c != '/')
            stringWithAllChars += c;
        if (i % 50 == 0)
            stringWithAllChars += '/';
    }

    std::string urlWithAllChars = "https://" + stringWithAllChars;

	std::unique_ptr<TSB::Store> store = createStoreDefault();
    ASSERT_EQ(performWrite(*store, urlWithAllChars), TSB::Status::OK);
}

TEST_F(TsbStoreTests, UrlToFileMapperUftUrlSpecialChars)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	const std::string fileIncPath{kActiveDir + "/some/directories/mot%C3%B6rhead.mp4"};
	const std::string dirIncPath{kActiveDir + "/some/directories"};
	const std::string url{"http://some/directories/mot%C3%B6rhead.mp4"};

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(fileIncPath))).WillOnce(Return(false));
	createDirectoriesExpectations(fs::path{dirIncPath});
	EXPECT_CALL(*mMockBasicFileBuf, pubsetbuf(nullptr, 0));
	EXPECT_CALL(*g_mockOfstream, open(fileIncPath, static_cast<std::ios_base::openmode>(1)));
	std::filesystem::perms permissions = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
										 std::filesystem::perms::group_read | std::filesystem::perms::group_write |
										 std::filesystem::perms::others_read | std::filesystem::perms::others_write;
	EXPECT_CALL(*g_mockFilesystem, permissions(fs::path(fileIncPath), permissions, _)).Times(1);
	EXPECT_CALL(*g_mockOfstream, write(kFileContent, sizeof(kFileContent)));
	EXPECT_CALL(*g_mockOfstream, close());

	ASSERT_THAT(performWrite(*store, url), TSB::Status::OK);
}

TEST_F(TsbStoreTests, UrlToFileMapperContainsNullCharacters)
{
	std::string containsNullCharacters("a\0b", 3);
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	ASSERT_THAT(performWriteInvalidUrl(*store, containsNullCharacters), TSB::Status::FAILED);
}

TEST_F(TsbStoreTests, WriteDuringFlush_Success)
{
	TSB::Sem removeAllSem;
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	Logger("Start the Flush");
	g_mockFilesystem->mockRemoveAllSem = &removeAllSem;
	store->Flush();

	// Available space is just enough to write a segment
	uintmax_t available = kMinFreePercent + sizeof(kFileContent);
	ASSERT_LT(available, kCapacity);

	Logger("Verify that Write during Flush succeeds");
	ASSERT_THAT(performWrite(*store, kUrl), TSB::Status::OK);

	Logger("Complete the Flush");
	removeAllSem.Post();
	waitForFlushCompletion();
}

TEST_F(TsbStoreTests, WriteDuringFlush_SuccessOnRetry)
{
	const std::string activeDir{kTsbLocation + "/2"};
	const std::string fileIncPath{activeDir + "/" + kDir + "/" + kFile};

	uintmax_t available = kMinFreePercent + sizeof(kFileContent);
	ASSERT_LT(available, kCapacity);
	EXPECT_CALL(*g_mockOfstream, open(fileIncPath, _)).Times(2);

	// First write fails and we remove file
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(fileIncPath), _)).WillOnce(Return(true));
	// Writing the buffer fails once and succeeds the second time
	EXPECT_CALL(*g_mockOfstream, write(kFileContent, sizeof(kFileContent))).Times(2);
	EXPECT_CALL(*g_mockOfstream, fail())
		.WillOnce(Return(false))                       // Called after open
		.WillOnce(Return(true))                        // Called after write, the first time it will fail
		.WillOnce(Return(false))                       // Called after open 2nd time
		.WillOnce(Return(false));                      // Called after write, the second time it will succeed
	EXPECT_CALL(*g_mockOfstream, bad())
		.WillOnce(SetErrnoAndReturn(ENOSPC, true))     // Called after write, the first time it will fail
		.WillOnce(Return(false));                      // Called after write, the second time it will succeed
	EXPECT_CALL(*g_mockOfstream, close()).Times(2);

	Logger("Create STBStore");
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	Logger("Start the Flush");
	// Set semaphore so flusher thread will be blocked until it is posted.
	TSB::Sem removeAllSem;
	g_mockFilesystem->mockRemoveAllSem = &removeAllSem;
	store->Flush();

	Logger("Verify that Write during Flush succeeds on a 2nd attempt");
	ASSERT_THAT(performWrite(*store, kUrl), TSB::Status::OK);

	Logger("Complete the Flush");
	removeAllSem.Post();
	waitForFlushCompletion();
}

TEST_F(TsbStoreTests, WriteDuringFlush_TimeoutFailure)
{
	const std::string activeDir{kTsbLocation + "/2"};
	const std::string fileIncPath{activeDir + "/" + kDir + "/" + kFile};
	auto expectedTimeout = 5000ms;
	auto expectedSleepTime = 2ms;
	bool failReturn = false;

	EXPECT_CALL(*g_mockOfstream, rdbuf()).WillRepeatedly(Return(mMockBasicFileBuf));
	EXPECT_CALL(*mMockBasicFileBuf, pubsetbuf(nullptr, 0)).Times(AnyNumber());

	EXPECT_CALL(*g_mockOfstream, open(fileIncPath, _))
	.Times((expectedTimeout / expectedSleepTime) + 1)
	.WillRepeatedly(Assign(&failReturn, false));


	EXPECT_CALL(*g_mockOfstream, write(kFileContent, sizeof(kFileContent)))
	.Times((expectedTimeout / expectedSleepTime) + 1)
	.WillRepeatedly(Assign(&failReturn, true));

	EXPECT_CALL(*g_mockOfstream, fail())
	.WillRepeatedly(ReturnPointee(&failReturn));   //called after open,called after write

	EXPECT_CALL(*g_mockOfstream, close()).Times((expectedTimeout / expectedSleepTime) + 1);
	EXPECT_CALL(*g_mockFilesystem, remove(fs::path(fileIncPath), _))
	.Times((expectedTimeout / expectedSleepTime) + 1)
	.WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockOfstream, bad())
		.WillRepeatedly(SetErrnoAndReturn(ENOSPC, true));  // Called after write, until it times out

	Logger("Create STBStore");
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	Logger("Start the Flush");
	// Set semaphore so flusher thread will be blocked until it is posted.
	TSB::Sem removeAllSem;
	g_mockFilesystem->mockRemoveAllSem = &removeAllSem;
	store->Flush();

	Logger("Verify that Write during Flush fails if no space becomes available within timeout");
	ASSERT_THAT(store->Write(kUrl, kFileContent, sizeof(kFileContent)), TSB::Status::NO_SPACE);

	Logger("Complete the Flush");
	removeAllSem.Post();
	waitForFlushCompletion();
}

TEST_F(TsbStoreTests, WriteDuringFlush_OpenFileFailure)
{
	const std::string activeDir{kTsbLocation + "/2"};
	const std::string fileIncPath{activeDir + "/" + kDir + "/" + kFile};

	// Avoid segfault due to NULL pointer
	EXPECT_CALL(*g_mockOfstream, rdbuf()).WillOnce(Return(mMockBasicFileBuf));
	// No other file stream failures
	EXPECT_CALL(*g_mockOfstream, fail()).WillRepeatedly(Return(false));

	// Opening the file fails
	Expectation openFile =
		EXPECT_CALL(*g_mockOfstream, open(fileIncPath, _));
	EXPECT_CALL(*g_mockOfstream, fail()).After(openFile).WillOnce(Return(true));

	// Write should not be called
	EXPECT_CALL(*g_mockOfstream, write(_, _)).Times(0);

	Logger("Create STBStore");
	std::unique_ptr<TSB::Store> store = createStoreDefault();

	Logger("Start the Flush");
	// Set semaphore so flusher thread will be blocked until it is posted.
	TSB::Sem removeAllSem;
	g_mockFilesystem->mockRemoveAllSem = &removeAllSem;
	store->Flush();

	Logger("Verify that Write during Flush fails when writing the buffer");
	ASSERT_THAT(store->Write(kUrl, kFileContent, sizeof(kFileContent)), TSB::Status::FAILED);

	Logger("Complete the Flush");
	removeAllSem.Post();
	waitForFlushCompletion();
}

TEST_F(TsbStoreTests, GetSize)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	std::error_code ec;
	const std::uintmax_t expected_size = 20;

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(true));
	EXPECT_CALL(*g_mockFilesystem, file_size(fs::path(kFileIncPath), ec)).WillOnce(Return(expected_size));

	EXPECT_EQ(store->GetSize(kUrl), expected_size);
}

TEST_F(TsbStoreTests, GetSize_Nofile)
{
	std::unique_ptr<TSB::Store> store = createStoreDefault();
	const std::uintmax_t expected_size = 0;

	EXPECT_CALL(*g_mockFilesystem, exists(fs::path(kFileIncPath))).WillOnce(Return(false));

	EXPECT_EQ(store->GetSize(kUrl), expected_size);
}
