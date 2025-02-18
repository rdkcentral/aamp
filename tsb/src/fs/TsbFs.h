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

} // namespace FS

} // namespace TSB

#endif // __TSB_FS__
