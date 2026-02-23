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

#ifndef __TSB_FS__
#define __TSB_FS__

#include <filesystem>
#include <thread>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <cstdlib>
#include <cstring>

namespace TSB
{

namespace FS
{
using std::ofstream;
using std::ifstream;

using std::filesystem::create_directory;
using std::filesystem::directory_entry;
using std::filesystem::directory_iterator;
using std::filesystem::exists;
using std::filesystem::file_size;
using std::filesystem::path;
using std::filesystem::permissions;
using std::filesystem::perms;
using std::filesystem::remove;
using std::filesystem::remove_all;
using std::filesystem::rename;
using std::filesystem::space;
using std::filesystem::space_info;

using std::this_thread::sleep_for;

using ::open;
using ::close;
using ::flock;
using ::write;
using ::read;

/**
 * @brief Sector size for O_DIRECT alignment requirements
 */
constexpr std::size_t kSectorSize = 4096;

/**
 * @brief Align a size up to the nearest multiple of sector size
 *
 * @param[in] size - size to align
 * @return aligned size rounded up to the nearest sector boundary
 */
inline std::size_t AlignToSector(std::size_t size)
{
	return ((size + kSectorSize - 1) / kSectorSize) * kSectorSize;
}

/**
 * @brief Allocate a sector-aligned buffer suitable for O_DIRECT I/O
 *
 * @param[in] size - minimum buffer size (will be rounded up to sector alignment)
 * @return pointer to aligned buffer, or nullptr on failure. Caller must free() the result.
 */
inline void* AllocAlignedBuffer(std::size_t size)
{
	std::size_t alignedSize = AlignToSector(size);
	void* buf = nullptr;
	if (posix_memalign(&buf, kSectorSize, alignedSize) != 0)
	{
		buf = nullptr;
	}
	return buf;
}

} // namespace FS

} // namespace TSB

#endif // __TSB_FS__
