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

#include "TsbLocationLock.h"

using namespace TSB;

LocationLock::LocationLock(const FS::path& location)
{
	mLocationFd = FS::open(location.string().c_str(), O_RDONLY | O_DIRECTORY);
	if (mLocationFd == -1)
	{
		throw std::invalid_argument("Failed to open the configured TSB Store location directory: " +
									location.string());
	}
}

Status LocationLock::Lock()
{
	// Take an exclusive, non-blocking lock on the location directory.
	// If a TSB Store is instantiated for the *same* TSB location, then the lock will fail
	// (this includes TSB Store instances in other processes)
	// If the current process crashes, the file lock is automatically released.
	return (FS::flock(mLocationFd, LOCK_EX | LOCK_NB) == -1) ? Status::FAILED : Status::OK;
}

LocationLock::~LocationLock()
{
	// This will also release the file lock
	(void)FS::close(mLocationFd);
}
