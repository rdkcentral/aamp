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
#include <thread>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>

#include "TsbApi.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// RFT = Real Filesystem Test
#define TSB_BASE_LOCATION		"/tmp/tsb_location_RFT"
#define TSB_LOCATION_TEMPLATE	TSB_BASE_LOCATION"/baseXXXXXX"

const uint32_t kMinFreePercent{5};
const uint32_t kMaxCapacity{UINT32_MAX};
const std::string kUrl{"https://lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/file.mp4"};
const std::string kFile{"lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/file.mp4"};
const char kFileContent[] {"content of the file"};

void Logger(std::string&& tsbMessage)
{
	std::cout << "[RFT]" << std::move(tsbMessage) << std::endl;
}

TEST(TsbStoreCreateDestroyTests, Negative_Unwriteable_Clean_Create_Destroy)
{
	// Set a readonly path.
	TSB::Store::Config tsbConfig{"/", kMinFreePercent, kMaxCapacity};

	EXPECT_THROW(auto store = std::make_unique<TSB::Store>(tsbConfig, Logger, TSB::LogLevel::TRACE),
				 std::invalid_argument);
}

TEST(TsbStoreCreateDestroyTests, Negative_Relative_Clean_Create_Destroy)
{
	// Set a Relative path.
	TSB::Store::Config tsbConfig{"../", kMinFreePercent, kMaxCapacity};

	EXPECT_THROW(auto store = std::make_unique<TSB::Store>(tsbConfig, Logger, TSB::LogLevel::TRACE),
				 std::invalid_argument);
}

class TsbStoreTests : public ::testing::Test
{

protected:

	void SetUp() override
	{
		// Create the base directory if it doesn't exist
		std::filesystem::create_directories(TSB_BASE_LOCATION);
		// Create a different directory for each test, so they can run in parallel
		char templatebuf[] = TSB_LOCATION_TEMPLATE;
		mTsbLocation = mkdtemp(templatebuf);

		const std::string kFlushDir{mTsbLocation + "/0"};

		TSB::Store::Config tsbConfig{mTsbLocation, kMinFreePercent, kMaxCapacity};
		mTsbStore = new TSB::Store(tsbConfig, Logger, TSB::LogLevel::TRACE);

		// Wait for the initial flush to complete, so that the Store is in a consistent state
		// before the test runs
		EXPECT_TRUE(WaitForFlush(kFlushDir));
	}

	void TearDown() override
	{
		delete mTsbStore;
		mTsbStore = nullptr;

		// If these tests are running in parallel, other tests may be using the base directory,
		// so don't delete that - just delete the TSB location subdirectory.
		fs::remove_all(mTsbLocation);
	}

	bool WaitForFlush(const std::string& dir, std::chrono::milliseconds timeout = 20ms) const
	{
		auto timeWaited{0ms};
		auto timeToSleep{1ms};

		do
		{
			std::this_thread::sleep_for(timeToSleep);
			timeWaited += timeToSleep;
		} while ((timeWaited <= timeout) && fs::exists(dir));

		return (timeWaited <= timeout);
	}

	TSB::Store *mTsbStore{nullptr};

	std::string mTsbLocation{};

	TSB::Status writeOperation(const std::string& pathToWrite )
	{
		std::size_t size {sizeof(kFileContent)};
		return mTsbStore->Write(pathToWrite, kFileContent, size);
	}
};

TEST_F(TsbStoreTests, CheckPermissions)
{
	const std::filesystem::perms dirExpectedPermissions {std::filesystem::perms::all};
	const std::filesystem::perms fileExpectedPermissions {std::filesystem::perms::owner_read |
														  std::filesystem::perms::owner_write |
														  std::filesystem::perms::group_read |
														  std::filesystem::perms::group_write |
														  std::filesystem::perms::others_read |
														  std::filesystem::perms::others_write};
	const std::string kFlushDir{mTsbLocation + "/0"};
	const std::string kActiveDir{mTsbLocation + "/1"};

	// Check that the permissions for the Flush directory are set correctly
	std::filesystem::perms permissions {std::filesystem::status(kFlushDir).permissions()};
	EXPECT_TRUE((permissions & std::filesystem::perms::all) == dirExpectedPermissions);

	// Set the umask to S_IRWXU (700) to check that permissions are still set correctly
	mode_t oldUmask = umask(S_IRWXU);

	EXPECT_EQ(writeOperation(kUrl), TSB::Status::OK);

	// Check that the permissions for the Active directory are set correctly
	permissions = std::filesystem::status(kActiveDir).permissions();
	EXPECT_TRUE((permissions & std::filesystem::perms::all) == dirExpectedPermissions);

	// Check that the permissions for the file created are set correctly
	permissions = std::filesystem::status(kActiveDir + "/" + kFile).permissions();
	EXPECT_TRUE((permissions & std::filesystem::perms::all) == fileExpectedPermissions);

	// Restore the umask
	umask(oldUmask);
}

TEST_F(TsbStoreTests, WriteAnotherFileExistingDirectory)
{
	const std::string urlAnotherFileSameDirectory{"https://lin017-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/enc/cenc/t/file2.mp4"};

	EXPECT_EQ(writeOperation(kUrl), TSB::Status::OK);
	EXPECT_EQ(writeOperation(urlAnotherFileSameDirectory), TSB::Status::OK);
}

TEST_F(TsbStoreTests, FlushNonExistentFlushDir)
{
	const std::string kFlushDir{mTsbLocation + "/0"};
	const std::string kActiveDir{mTsbLocation + "/1"};

	// Verify the Flush API with no flush dir
	mTsbStore->Flush();
	std::this_thread::sleep_for(1ms);      // Wait until asynchronous flushing completed
	EXPECT_FALSE(fs::exists(kFlushDir));
	EXPECT_FALSE(fs::exists(kActiveDir));
	EXPECT_EQ(fs::remove_all(mTsbLocation), static_cast<uintmax_t>(1));
}

TEST_F(TsbStoreTests, FlushConcurrent)
{
	std::string dir0{mTsbLocation + "/0"};
	std::string dir1{mTsbLocation + "/1"};     // this is the current active directory
	std::string dir2{mTsbLocation + "/2"};
	std::string dir3{mTsbLocation + "/3"};
	std::string dir4{mTsbLocation + "/4"};

	mTsbStore->Flush();

	// Create the new active directory
	fs::create_directories(dir2);

	// Call Flush possibly while the previous flush is in progress
	mTsbStore->Flush();

	// Create the new active directory
	fs::create_directories(dir3);

	// Call Flush possibly while the previous flush is in progress
	mTsbStore->Flush();

	// Create the new active directory
	fs::create_directories(dir4);

	EXPECT_TRUE(WaitForFlush(dir3));

	// Check that all the directories, except the active one, have been deleted
	EXPECT_FALSE(fs::exists(dir0));
	EXPECT_FALSE(fs::exists(dir1));
	EXPECT_FALSE(fs::exists(dir2));
	EXPECT_FALSE(fs::exists(dir3));
	EXPECT_TRUE(fs::exists(dir4));
}

TEST_F(TsbStoreTests, SecondConcurrentInstanceSameLocationFailure)
{
	TSB::Store::Config tsbConfig{mTsbLocation, kMinFreePercent, kMaxCapacity};

	// Create a second concurrent store instance for the *same* TSB Location
	EXPECT_THROW(auto store = std::make_unique<TSB::Store>(tsbConfig, Logger, TSB::LogLevel::TRACE),
				 std::invalid_argument);
}

TEST_F(TsbStoreTests, SecondConcurrentInstanceDifferentLocationSuccess)
{
	char templatebuf[] = TSB_LOCATION_TEMPLATE;
	std::string differentLocation = mkdtemp(templatebuf);
	TSB::Store::Config tsbConfig{differentLocation, kMinFreePercent, kMaxCapacity};

	// Create a second concurrent store instance for a *different* TSB Location
	TSB::Store* secondStore = nullptr;
	EXPECT_NO_THROW(secondStore = new TSB::Store(tsbConfig, Logger, TSB::LogLevel::TRACE));

	// Second Store must be destroyed before its temporary location directory is removed
	delete secondStore;

	fs::remove_all(differentLocation);
}

TEST_F(TsbStoreTests, SecondSequentialInstanceSameLocationSuccess)
{
	delete mTsbStore;
	mTsbStore = nullptr;

	TSB::Store::Config tsbConfig{mTsbLocation, kMinFreePercent, kMaxCapacity};

	// Create a second sequential store instance for the *same* TSB Location. This verifies
	// that the Store location was unlocked when the first instance was destructed, above.
	EXPECT_NO_THROW(mTsbStore = new TSB::Store(tsbConfig, Logger, TSB::LogLevel::TRACE));
}
