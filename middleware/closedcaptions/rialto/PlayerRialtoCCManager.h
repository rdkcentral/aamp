/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/**
 * @file PlayerRialtoCCManager.h
 *
 * @brief Integration layer of Rialto ClosedCaption in Middleware
 *
 */

#ifndef __PLAYER_RIALTO_CC_MANAGER_H__
#define __PLAYER_RIALTO_CC_MANAGER_H__

#include "PlayerCCManager.h"

#include <string>
#include <set>
#include <mutex>

/**
 * @class PlayerRialtoCCManager
 * @brief Handling Rialto CC operation
 */

class PlayerRialtoCCManager : public PlayerCCManagerBase
{
public:

	/**
	 * @fn Release
	 * @param[in] id -  returned from GetId function
	 */
	void Release(int iID) override;

	/**
	 * @fn GetId
	 * @return int -  unique ID
	 */
	int GetId() override;

	/**
	 * @fn SetTrack
	 *
	 * @param[in] track - CC track to be selected
	 * @param[in] format - force track to 608/708 or default (not used)
	 * @return int - 0 on success, -1 on failure
	 */
	int SetTrack(const std::string &track, const CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT) override;

	/**
	 * @fn PlayerRialtoCCManager
	 */
	PlayerRialtoCCManager();

	/**
	 * @brief Destructor
	 */
	~PlayerRialtoCCManager() = default;

	PlayerRialtoCCManager(const PlayerRialtoCCManager&) = delete;
	PlayerRialtoCCManager& operator=(const PlayerRialtoCCManager&) = delete;

private:
	/**
	 * @fn StartRendering
	 *
	 * @return void
	 */
	void StartRendering() override;

	/**
	 * @fn StopRendering
	 *
	 * @return void
	 */
	void StopRendering() override;

	/**
	 * @brief Impl specific initialization code called once in Init() function
	 *
	 * @return 0 - success, -1 - failure
	 */
	int Initialize(void *handle) override;

	/**
	 * @fn SetDigitalChannel
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetDigitalChannel(unsigned int id) override;
	/**
	 * @fn SetAnalogChannel
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetAnalogChannel(unsigned int id) override;

	/**
	 * @fn ResetState
	 *
	 * @return void
	 */
	void ResetState() override;

private:
	void *mSubtitleControlHandle{nullptr};

	std::mutex mIdLock{};
	int mId{0};
	std::set<int> mIdSet{};
};

#endif /* __PLAYER_RIALTO_CC_MANAGER_H__ */
