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
 * @file FakePlayerExternalsRdkInterface.cpp
 * @brief Fake implementation of PlayerExternalsRdkInterface with no external dependencies
 * Used for unit testing in isolation without platform-specific HAL implementations
 */

#include <memory>
#include <functional>

// Enum and type definitions for standalone fake implementation
enum dsDisplayEvent_t {
	dsDISPLAY_EVENT_CONNECTED = 0,
	dsDISPLAY_EVENT_DISCONNECTED = 1
};

enum dsHdcpStatus_t {
	dsHDCP_STATUS_AUTHENTICATED = 0,
	dsHDCP_STATUS_UNAUTHENTICATED = 1,
	dsHDCP_STATUS_AUTHENTICATION_FAILURE = 2,
	dsHDCP_STATUS_UNKNOWN = 3
};

enum dsHdcpProtocolVersion_t {
	dsHDCP_VERSION_1X = 0,
	dsHDCP_VERSION_2X = 1
};

enum InitState {
	NOT_INITIALIZED = 0,
	IARM = 1
};

class DeviceInterfaceBase {
public:
	virtual ~DeviceInterfaceBase() {}
};

// Validation constraints for resolution
static constexpr int MIN_RESOLUTION = 320;      // Minimum valid resolution (e.g., 320x240)
static constexpr int MAX_RESOLUTION = 7680;     // Maximum valid resolution (8K: 7680x4320)

// Global state variables for fake implementation
static int m_displayWidth = 0;
static int m_displayHeight = 0;
static bool m_lastResolutionValid = true;       // Track if last resolution was valid
static bool m_isHDCPEnabled = false;
static dsHdcpProtocolVersion_t m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
static int m_sourceWidth = 0;
static int m_sourceHeight = 0;
static void* m_gstElement = nullptr;
static std::shared_ptr<DeviceInterfaceBase> m_pDeviceInterfaceBase = nullptr;
static bool m_use_firebolt_sdk = false;
static InitState m_initialized = NOT_INITIALIZED;
static bool mPowerEvt = false;
static std::function<void()> m_doFakeTuneCallback = nullptr;

inline void GetDisplayResolution(int &width, int &height)
{
	width = m_displayWidth;
	height = m_displayHeight;
}

inline void SetResolution(int width, int height)
{
	// Validate input: reject zero, negative, or out-of-range values
	if (width <= 0 || height <= 0 || 
	    width < MIN_RESOLUTION || width > MAX_RESOLUTION ||
	    height < MIN_RESOLUTION || height > MAX_RESOLUTION)
	{
		// Mark as invalid but don't update state
		m_lastResolutionValid = false;
		return;
	}
	
	// Update only if validation passed
	m_displayWidth = width;
	m_displayHeight = height;
	m_lastResolutionValid = true;
}

inline void SetHDMIStatus()
{
	// Fake implementation - no external device calls
	// In real implementation, this would call device::Manager APIs
	// For testing, we just handle gracefully
}

inline void OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent)
{
	SetHDMIStatus();
}

inline void OnHDCPStatusChange(dsHdcpStatus_t hdcpStatus)
{
    SetHDMIStatus();
}

inline void OnResolutionPostChange(const int width, const int height)
{
	SetResolution(width, height);
}

inline void OnResolutionPreChange(const int width, const int height)
{
	// No implementation needed for pre-change event in tests
}

inline char * GetTR181Config(const char * paramName, size_t & iConfigLen)
{
	iConfigLen = 0;
	return nullptr;
}

inline bool GetActiveInterface()
{
	return false;
}

inline void SetActiveInterface(bool isWifi)
{
	// Fake implementation
}

inline std::shared_ptr<DeviceInterfaceBase> GetDeviceInterface()
{
	return m_pDeviceInterfaceBase;
}

inline void setHdcpProtocol(dsHdcpProtocolVersion_t t_protocol)
{
	m_hdcpCurrentProtocol = t_protocol;
}

inline void SetUseFireBoltSDK(bool t_use_firebolt_sdk)
{
	m_use_firebolt_sdk = t_use_firebolt_sdk;
}

inline void SetPowerEvent(bool powerEvt)
{
	mPowerEvt = powerEvt;
}

inline bool GetPowerEvent()
{
	return mPowerEvt;
}

inline void SetDoFakeTuneCallBack(const std::function<void()>& t_doFakeTuneCallback)
{
	m_doFakeTuneCallback = t_doFakeTuneCallback;
}

inline std::function<void()> GetDoFakeTuneCallBack()
{
	return m_doFakeTuneCallback;
}

// Wrapper class for test compatibility
class PlayerExternalsRdkInterface
{
public:
	static std::shared_ptr<PlayerExternalsRdkInterface> GetPlayerExternalsRdkInterfaceInstance()
	{
		static std::shared_ptr<PlayerExternalsRdkInterface> instance = std::make_shared<PlayerExternalsRdkInterface>();
		return instance;
	}

	// Rule of Five: Explicitly enforce singleton pattern
	// Default constructor needed for std::make_shared
	PlayerExternalsRdkInterface() = default;

	// Prevent copying to ensure only one instance exists
	PlayerExternalsRdkInterface(const PlayerExternalsRdkInterface&) = delete;
	PlayerExternalsRdkInterface& operator=(const PlayerExternalsRdkInterface&) = delete;

	// Allow move semantics for shared_ptr compatibility
	PlayerExternalsRdkInterface(PlayerExternalsRdkInterface&&) = default;
	PlayerExternalsRdkInterface& operator=(PlayerExternalsRdkInterface&&) = default;

	// Default destructor
	~PlayerExternalsRdkInterface() = default;
	void GetDisplayResolution(int &width, int &height)
	{
		::GetDisplayResolution(width, height);
	}

	void SetResolution(int width, int height)
	{
		::SetResolution(width, height);
	}

	void SetHDMIStatus()
	{
		::SetHDMIStatus();
	}

	void OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent)
	{
		::OnDisplayHDMIHotPlug(displayEvent);
	}

	void OnHDCPStatusChange(dsHdcpStatus_t hdcpStatus)
	{
		::OnHDCPStatusChange(hdcpStatus);
	}

	void OnResolutionPostChange(const int width, const int height)
	{
		::OnResolutionPostChange(width, height);
	}

	void OnResolutionPreChange(const int width, const int height)
	{
		::OnResolutionPreChange(width, height);
	}

	char * GetTR181Config(const char * paramName, size_t & iConfigLen)
	{
		return ::GetTR181Config(paramName, iConfigLen);
	}

	bool GetActiveInterface()
	{
		return ::GetActiveInterface();
	}

	void SetActiveInterface(bool isWifi)
	{
		::SetActiveInterface(isWifi);
	}

	std::shared_ptr<DeviceInterfaceBase> GetDeviceInterface()
	{
		return ::GetDeviceInterface();
	}

	void setHdcpProtocol(dsHdcpProtocolVersion_t t_protocol)
	{
		::setHdcpProtocol(t_protocol);
	}

	void SetUseFireBoltSDK(bool t_use_firebolt_sdk)
	{
		::SetUseFireBoltSDK(t_use_firebolt_sdk);
	}

	void SetPowerEvent(bool powerEvt)
	{
		::SetPowerEvent(powerEvt);
	}

	bool GetPowerEvent()
	{
		return ::GetPowerEvent();
	}

	void SetDoFakeTuneCallBack(const std::function<void()>& t_doFakeTuneCallback)
	{
		::SetDoFakeTuneCallBack(t_doFakeTuneCallback);
	}

	std::function<void()> GetDoFakeTuneCallBack()
	{
		return ::GetDoFakeTuneCallBack();
	}

	/**
	 * @brief Check if the last resolution update was valid
	 * @return true if the last SetResolution/OnResolutionPostChange was valid, false otherwise
	 */
	bool IsLastResolutionValid() const
	{
		return m_lastResolutionValid;
	}

	/**
	 * @brief Reset all internal state variables to initial values
	 * This should be called between tests to ensure proper test isolation
	 */
	void Reset()
	{
		m_displayWidth = 0;
		m_displayHeight = 0;
		m_isHDCPEnabled = false;
		m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;
		m_sourceWidth = 0;
		m_sourceHeight = 0;
		m_gstElement = nullptr;
		m_pDeviceInterfaceBase = nullptr;
		m_use_firebolt_sdk = false;
		m_initialized = NOT_INITIALIZED;
		mPowerEvt = false;
		m_doFakeTuneCallback = nullptr;
		m_lastResolutionValid = true;
	}
};
