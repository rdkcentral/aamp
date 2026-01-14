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
 * @file PlayerSubtecCCManager.h
 *
 * @brief Integration layer of Subtec ClosedCaption in Middleware
 *
 */

#ifndef __PLAYER_SUBTEC_CC_MANAGER_H__
#define __PLAYER_SUBTEC_CC_MANAGER_H__

#include "PlayerCCManager.h"

#include <string>
#include <set>
#include <mutex>
#include "SubtecConnector.h"


/**
 * @class PlayerSubtecCCManager
 * @brief Handling Subtec CC operation
 */

class PlayerSubtecCCManager : public PlayerCCManagerBase
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
	virtual int GetId();

	/**
	 * @fn PlayerSubtecCCManager
	 */
	PlayerSubtecCCManager();

	/**
	 * @brief Destructor
	 */
	~PlayerSubtecCCManager() = default;

	PlayerSubtecCCManager(const PlayerSubtecCCManager&) = delete;
	PlayerSubtecCCManager& operator=(const PlayerSubtecCCManager&) = delete;

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
	 * @fn EnsureInitialized
	 * @return void
	 */
	void EnsureInitialized() override;

	/**
	 * @fn EnsureHALInitialized
	 * @return void
	 */
	void EnsureHALInitialized() override;

	/**
	 * @brief Impl specific initialization code called once in Init() function
	 *
	 * @return 0 - success, -1 - failure
	 */
	int Initialize(void *handle) override;

	/**
	 * @fn EnsureRendererCommsInitialized
	 * @return void
	 */
	void EnsureRendererCommsInitialized() override;

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
	 * @fn EnsureRendererStateConsistency
	 *
	 * @return void
	 */
	void EnsureRendererStateConsistency();

	void *mCCHandle{nullptr}; /**< Decoder handle for initializing CC resources */


private:
	bool mRendererInitialized{false};
	bool mHALInitialized{false};
	bool mHandleUpdated{false};
	std::mutex mIdLock{};
	int mId{0};
	std::set<int> mIdSet{};
};

#endif /* __PLAYER_SUBTEC_CC_MANAGER_H__ */
