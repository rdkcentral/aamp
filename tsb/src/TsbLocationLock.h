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

#ifndef __TSB_LOCATION_LOCK__
#define __TSB_LOCATION_LOCK__

#include "TsbFs.h"
#include "TsbApi.h"

namespace TSB
{

/**
 * @brief A simple RAII wrapper class for file-locking TSB Store locations,
 *        to ensure that the location remains unique to the Store instance.
 */
class LocationLock
{
public:
	/**
	 *  @fn LocationLock (constructor)
	 *
	 *  @brief Creates and initialises a new LocationLock for a given TSB Store filesystem location.
	 *         The LocationLock is constructed in unlocked state.
	 *
	 *  @param[in] location - directory location, specified as an absolute filesystem path
	 *
	 *  @throw std::invalid_argument if the directory location cannot be opened
	 */
	explicit LocationLock(const FS::path& location);

	/**
	 *  @fn Lock
	 *
	 *  @brief Locks the directory location, so that other TSB Store instances can't use it.
	 *
	 *  @retval Status::OK on success.
	 *  @retval Status::FAILED if the directory could not be locked. This indicates that another
	 *          TSB Store instance on the system is currently using the directory location.
	 */
	Status Lock();

	/**
	 *  @fn ~LocationLock (destructor)
	 *
	 *  @brief Destroys the LocationLock instance.
	 *         If the directory location is currently locked, it will become unlocked.
	 */
	~LocationLock();

private:
	int mLocationFd{-1};
};

} // namespace TSB

#endif // __TSB_LOCATION_LOCK__
