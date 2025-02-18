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
#include <chrono>

#include "TsbFakeOfstream.h"
#include "TsbFakeIfstream.h"
#include "TsbFakeDirectoryIterator.h"
#include "TsbFakeLibc.h"


namespace TSB
{

namespace FS
{

/*
 * The tests probably don't need to mock these types, which are either based on character
 * strings or are simple structs and don't actually make filesystem calls.  For example, path only
 * handles syntax - the pathname may represent a non-existing path or even one that is not allowed
 * to exist on the current filesystem or OS.
 */
using std::filesystem::directory_entry;
using std::filesystem::path;
using std::filesystem::perms;
using std::filesystem::space_info;

bool create_directory(const path &, std::error_code &);
bool exists(const path &);
uintmax_t file_size(const path &, std::error_code &);
void permissions(const path &, perms, std::error_code &);
bool remove(const path &, std::error_code &);
uintmax_t remove_all(const path &, std::error_code &);
void rename(const path &, const path &, std::error_code &);
space_info space(const path &, std::error_code &);

template <typename _Rep, typename _Period>
void sleep_for(const std::chrono::duration<_Rep, _Period> &)
{
	// Do nothing
}

} // namespace FS

} // namespace TSB

#endif // __TSB_FS__
