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

#ifndef __TSB_MOCK_FILESYSTEM__
#define __TSB_MOCK_FILESYSTEM__

#include <gmock/gmock.h>
#include <filesystem>
#include <atomic>

#include "TsbSem.h"

class TsbMockFilesystem
{
public:
	MOCK_METHOD(bool, create_directory, (const std::filesystem::path&, std::error_code&));
	MOCK_METHOD(bool, exists, (const std::filesystem::path&));
	MOCK_METHOD(uintmax_t, file_size, (const std::filesystem::path&, std::error_code&), (const));
	MOCK_METHOD(void, permissions, (const std::filesystem::path &__p, std::filesystem::perms __prms, std::error_code &__ec));
	MOCK_METHOD(bool, remove, (const std::filesystem::path&, std::error_code&));
	MOCK_METHOD(uintmax_t, remove_all, (const std::filesystem::path&, std::error_code&));
	MOCK_METHOD(void, rename,
				(const std::filesystem::path&, const std::filesystem::path&, std::error_code&));
	MOCK_METHOD(std::filesystem::space_info, space,
				(const std::filesystem::path&, std::error_code&));

	TSB::Sem *mockRemoveAllSem{nullptr};
	std::atomic_bool mockRemoveAllCompleted{false};
};

extern TsbMockFilesystem* g_mockFilesystem;

#endif // __TSB_MOCK_FILESYSTEM__
