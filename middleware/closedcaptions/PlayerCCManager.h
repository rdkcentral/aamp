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
 * @file PlayerCCManager.h
 *
 * @brief Integration layer of ClosedCaption in MIDDLEWARE layer
 *
 */

#ifndef __PLAYER_CC_MANAGER_H__
#define __PLAYER_CC_MANAGER_H__

#include <string>
#include <vector>
#include "CCTrackInfo.h"
#include "PlayerLogManager.h"

/**
 * @enum CCFormat
 * @brief Different CC formats
 */
enum CCFormat
{
	eCLOSEDCAPTION_FORMAT_608 = 0,
	eCLOSEDCAPTION_FORMAT_708,
	eCLOSEDCAPTION_FORMAT_DEFAULT
};

/**
 * @class PlayerCCManagerBase
 * @brief Handles closed caption operations
 */

class PlayerCCManagerBase
{
public:
	/**
	 * @fn Init
	 *
	 * @param[in] handle - decoder handle
	 * @return int - 0 on success, -1 on failure
	 */
	int Init(void *handle);

	/**
	 * @brief Gets Handle or ID, Every client using subtec must call GetId  in the beginning , save id, which is required for Release function.
	 * @return int -  unique ID
	 */
	virtual int GetId() { return 0; };

	/**
	 * @brief Release CC resources
	 * @param[in] id -  returned from GetId function
	 */
	virtual void Release(int iID) = 0;

	/**
	 * @fn SetStatus
	 *
	 * @param[in] enable - true to enable CC rendering
	 * @return int - 0 on success, -1 on failure
	 */
	virtual int SetStatus(bool enable);

	/**
	 * @brief Get CC rendering status
	 *
	 * @return bool - true if enabled, false otherwise
	 */
	bool GetStatus() { return mEnabled; };

	/**
	 * @brief Get current CC track
	 *
	 * @return std::string - current CC track
	 */
	const std::string &GetTrack() { return mTrack; }

	/**
	 * @fn SetTrack
	 *
	 * @param[in] track - CC track to be selected
	 * @param[in] format - force track to 608/708 or default
	 * @return int - 0 on success, -1 on failure
	 */
	virtual int SetTrack(const std::string &track, const CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT);

	/**
	 * @fn SetStyle
	 *
	 * @param[in] options - rendering style options
	 * @return int - 0 on success, -1 on failure
	 */
	virtual int SetStyle(const std::string &options);

	/**
	 * @brief Get current CC styles
	 *
	 * @return std::string - current CC options
	 */
	//TODO: Default values can't be queried
	const std::string &GetStyle() { return mOptions; }

	/**
	 * @fn SetTrickplayStatus
	 *
	 * @param[in] enable - true when trickplay starts, false otherwise
	 * @return void
	 */
	virtual void SetTrickplayStatus(bool enable);

	/**
	 * @fn SetParentalControlStatus
	 *
	 * @param[in] locked - true when parental control lock enabled, false otherwise
	 * @return void
	 */
	virtual void SetParentalControlStatus(bool locked);

	/**
	 * @fn RestoreCC 
	 *
	 * @return void
	 */
	void RestoreCC();

	virtual ~PlayerCCManagerBase(){ };

	/**
	 * @brief update stored list of text tracks 
	 *
	 * @param[in] newTextTracks - list of text track to store
	 * @return void
	 */
	virtual void updateLastTextTracks(const std::vector<CCTrackInfo>& newTextTracks) {  mLastTextTracks = newTextTracks; }
	/**
	 * @brief Get list of text track Ids
	 *
	 * @return const std::vector<CCTrackInfo>& - list of text tracks data
	 */
	const std::vector<CCTrackInfo>& getLastTextTracks() const { return mLastTextTracks;}

	/**
	 * @brief To check whether Out of Band Closed caption/Subtile rendering supported or not. 
	 *
	 * @return bool, True if Out of Band Closed caption/subtitle rendering supported
	 */
	virtual bool IsOOBCCRenderingSupported();

protected:
	/**
	 * @brief To start CC rendering
	 *
	 * @return void
	 */
	virtual void StartRendering() = 0;

	/**
	 * @brief To stop CC rendering
	 *
	 * @return void
	 */
	virtual void StopRendering() = 0;

	/**
	 * @brief Impl specific initialization code called before each public interface call
	 * @return void
	 */
	virtual void EnsureInitialized(){};

	/**
	 * @brief Impl specific initialization code for HAL
	 * @return void
	 */
	virtual void EnsureHALInitialized(){};

	/**
	 * @brief Impl specific initialization code for Communication with renderer
	 * @return void
	 */
	virtual void EnsureRendererCommsInitialized(){};

	/**
	 * @brief Impl specific initialization code called once in Init() function
	 *
	 * @return 0 - success, -1 - failure
	 */
	virtual int Initialize(void *handle){return 0;}

	/**
	 * @brief set digital channel with specified id
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	virtual int SetDigitalChannel(unsigned int id) = 0;

	/**
	 * @brief set analog channel with specified id
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	virtual int SetAnalogChannel(unsigned int id) = 0;

	/**
	 * @brief validate mCCHandle
	 *
	 * @return bool
	 */
	virtual bool CheckCCHandle() const {return true;}

	/**
	 * @fn Start
	 *
	 * @return void
	 */
	void Start();

	/**
	 * @fn Stop
	 *
	 * @return void
	 */
	void Stop();

	
	std::string mOptions{};                /**< CC rendering styles */
	std::string mTrack{};                  /**< CC track */
	std::vector<CCTrackInfo> mLastTextTracks;
	bool mEnabled{false};                  /**< true if CC rendering enabled, false otherwise */
	bool mTrickplayStarted{false};         /**< If a trickplay is going on or not */
	bool mParentalCtrlLocked{false};       /**< If Parental Control lock enabled on not */
};

/**
 * @class PlayerCCManager
 * @brief Handle the CC manager instance
 */

class PlayerCCManager
{
public:
	/**
	 * @fn GetInstance
	 *
	 * @return PlayerCCManager - singleton instance
	 */
	static PlayerCCManagerBase * GetInstance();

	/**
	 * @fn DestroyInstance
	 *
	 * @return void
	 */
	static void DestroyInstance();

private:
	static PlayerCCManagerBase *mInstance; /**< Singleton instance */
};

class PlayerFakeCCManager : public PlayerCCManagerBase
{
public:

	void Release(int iID) override {};
	/**
	 * @fn PlayerFakeCCManager
	 */
	PlayerFakeCCManager() = default;

	/**
	 * @brief Destructor
	 */
	~PlayerFakeCCManager() = default;

	PlayerFakeCCManager(const PlayerFakeCCManager&) = delete;
	PlayerFakeCCManager& operator=(const PlayerFakeCCManager&) = delete;

private:
	/**
	 * @fn StartRendering
	 *
	 * @return void
	 */
	void StartRendering() override {};

	/**
	 * @fn StopRendering
	 *
	 * @return void
	 */
	void StopRendering() override {};

	/**
	 * @fn SetDigitalChannel
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetDigitalChannel(unsigned int id) override { return 0; };
	/**
	 * @fn SetAnalogChannel
	 *
	 * @return CC_VL_OS_API_RESULT
	 */
	int SetAnalogChannel(unsigned int id) override { return 0; };

	bool CheckCCHandle() const override{ return false; }
	
	void updateLastTextTracks(const std::vector<CCTrackInfo>& newTextTracks) override {};

	void SetParentalControlStatus(bool locked) override {};

	void SetTrickplayStatus(bool enable) override {};

	int SetStatus(bool enable) override { return 0; };

	int SetStyle(const std::string &options) override { return 0; };

	int SetTrack(const std::string &track, const CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT) override { return 0; };

	bool IsOOBCCRenderingSupported() override { return false; };
};

#endif /* __PLAYER_CC_MANAGER_H__ */
