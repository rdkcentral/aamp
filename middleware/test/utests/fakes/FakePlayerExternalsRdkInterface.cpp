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

// Global state variables for fake implementation
static int m_displayWidth = 0;
static int m_displayHeight = 0;
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

void GetDisplayResolution(int &width, int &height)
{
	width = m_displayWidth;
	height = m_displayHeight;
}

void SetResolution(int width, int height)
{
	m_displayWidth = width;
	m_displayHeight = height;
}

void SetHDMIStatus()
{
	// Fake implementation - no external device calls
	// In real implementation, this would call device::Manager APIs
	// For testing, we just handle gracefully
}

void OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent)
{
	const char *hdmihotplug = (displayEvent == dsDISPLAY_EVENT_CONNECTED) ? "connected" : "disconnected";
	
	SetHDMIStatus();
}

void OnHDCPStatusChange(dsHdcpStatus_t hdcpStatus)
{
	const char *hdcpStatusStr = (hdcpStatus == dsHDCP_STATUS_AUTHENTICATED) ? "authenticated" : "authentication failure";
	
	SetHDMIStatus();
}

void OnResolutionPostChange(int width, int height)
{
	SetResolution(width, height);
}

void OnResolutionPreChange(int width, int height)
{
	// No implementation needed for pre-change event in tests
}

char * GetTR181Config(const char * paramName, size_t & iConfigLen)
{
	iConfigLen = 0;
	return nullptr;
}

bool GetActiveInterface()
{
	return false;
}

void SetActiveInterface(bool isWifi)
{
	// Fake implementation
}

std::shared_ptr<DeviceInterfaceBase> GetDeviceInterface()
{
	return m_pDeviceInterfaceBase;
}

void setHdcpProtocol(dsHdcpProtocolVersion_t t_protocol)
{
	m_hdcpCurrentProtocol = t_protocol;
}

void SetUseFireBoltSDK(bool t_use_firebolt_sdk)
{
	m_use_firebolt_sdk = t_use_firebolt_sdk;
}

void SetPowerEvent(bool powerEvt)
{
	mPowerEvt = powerEvt;
}

bool GetPowerEvent()
{
	return mPowerEvt;
}

void SetDoFakeTuneCallBack(const std::function<void()>& t_doFakeTuneCallback)
{
	m_doFakeTuneCallback = t_doFakeTuneCallback;
}

std::function<void()> GetDoFakeTuneCallBack()
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

	void OnResolutionPostChange(int width, int height)
	{
		::OnResolutionPostChange(width, height);
	}

	void OnResolutionPreChange(int width, int height)
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
};
