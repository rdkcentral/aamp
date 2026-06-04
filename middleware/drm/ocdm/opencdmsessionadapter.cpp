/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file opencdmsessionadapter.cpp
 * @brief Handles operation with OCDM session to handle DRM License data.
 *
 * All OCDM C library calls are routed through m_ocdm (IOpenCDM) and
 * m_session (IOpenCDMSession).  The concrete implementations
 * (OpenCDMProvider / RialtoMediaKeysProvider) contain the library-specific
 * code.
 */
#include "opencdmsessionadapter.h"

#include "DrmHelper.h"
#include "PlayerUtils.h"

#include "ProcessHandler.h"
#include "PlayerExternalsInterface.h"
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <errno.h>
#include <string.h>
#include <vector>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "PlayerLogManager.h"

#include <sys/time.h>
#include <set>
#define LICENSE_RENEWAL_MESSAGE_TYPE "1"

/**
 * @fn OCDMSessionAdapter
 * @brief OCDMSessionAdapter constructor
 */
OCDMSessionAdapter::OCDMSessionAdapter(DrmHelperPtr drmHelper,
                                       std::unique_ptr<IOpenCDM> ocdm,
                                       DrmCallbacks *callbacks)
	: DrmSession(drmHelper->ocdmSystemId())
	, m_eKeyState(KEY_INIT)
	, m_ocdm(std::move(ocdm))
	, m_session(nullptr)
	, m_pOutputProtection(nullptr)
	, decryptMutex()
	, m_sessionID()
	, m_challenge()
	, timeBeforeCallback(0)
	, m_challengeReady()
	, m_challengeSize(0)
	, m_keyStatus(InternalError)
	, m_keyStateIndeterminate(false)
	, m_keyStatusReady()
	, m_destUrl()
	, m_drmHelper(drmHelper)
	, m_drmCallbacks(callbacks)
	, m_keyStatusWait()
	, m_keyId()
	, m_keyStored()
	, m_usableKeys()
	, m_usableKeysMutex()
{
	MW_LOG_WARN("OCDMSessionAdapter :: enter");
	MW_LOG_WARN("OCDMSessionAdapter :: key process timeout is %d", drmHelper->keyProcessTimeout());

	// Get output protection pointer
	m_pOutputProtection = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();
	MW_LOG_WARN("OCDMSessionAdapter :: exit");
}


OCDMSessionAdapter::~OCDMSessionAdapter()
{
	MW_LOG_WARN("[HHH]OCDMSessionAdapter destructor called! keySystem %s", m_keySystem.c_str());
	clearDecryptContext();
	// m_ocdm is cleaned up by its unique_ptr destructor.
}


void OCDMSessionAdapter::generateDRMSession(const uint8_t *f_pbInitData,
		uint32_t f_cbInitData, std::string &customData)
{
	MW_LOG_INFO("at %p", this);

	std::lock_guard<std::mutex> guard(decryptMutex);
	if (!m_ocdm)
	{
		MW_LOG_WARN("OCDMSessionAdapter::generateDRMSession: no IOpenCDM provider");
		m_eKeyState = KEY_ERROR;
		return;
	}

	timeBeforeCallback = GetCurrentTimeMS();

	// Wire callbacks using std::function captures — no void* userData required.
	OpenCDMSessionCallbackSet callbacks;

	callbacks.onChallenge = [this](const char* destUrl,
	                               const uint8_t* challenge,
	                               uint16_t challengeSize)
	{
		timeBeforeCallback = GetCurrentTimeMS() - timeBeforeCallback;
		MW_LOG_WARN("Duration for process_challenge_callback %lld", timeBeforeCallback);
		processOCDMChallenge(destUrl, challenge, challengeSize);
	};

	callbacks.onKeyUpdate = [this](const uint8_t* key, uint8_t keySize)
	{
		keyUpdateOCDM(key, keySize);
	};

	callbacks.onKeysUpdated = [this]()
	{
		keysUpdatedOCDM();
	};

	// Route Rialto license renewals through the same DRM callback path used by
	// the OCDM stack's processOCDMChallenge when messageType == "1".
	callbacks.onLicenseRenewal = [this](const uint8_t* /*message*/, size_t /*size*/)
	{
		if (m_drmCallbacks)
		{
			m_drmCallbacks->LicenseRenewal(m_drmHelper, static_cast<DrmSession*>(this));
		}
	};

	const uint8_t* customDataPtr = customData.empty()
		? nullptr
		: reinterpret_cast<const uint8_t*>(customData.c_str());
	const uint16_t customDataLen = static_cast<uint16_t>(customData.length());

	MW_LOG_INFO("customData length: %d", customDataLen);

	m_session = m_ocdm->constructSession(
		m_keySystem,
		LicenseType::Temporary,
		"cenc",
		f_pbInitData, f_cbInitData,
		customDataPtr, customDataLen,
		callbacks);

	if (!m_session)
	{
		MW_LOG_ERR("OCDMSessionAdapter::generateDRMSession: constructSession failed");
		m_eKeyState = KEY_ERROR_SESSION_CREATE_FAILED;
	}
}


void OCDMSessionAdapter::processOCDMChallenge(const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize) {

	MW_LOG_INFO("at %p", this);

	const std::string challengeData(reinterpret_cast<const char *>(challenge), challengeSize);
	const std::set<std::string> individualisationTypes = {"individualization-request", "3"};
	const std::string delimiter(":Type:");
	const size_t delimiterPos = challengeData.find(delimiter);
	const std::string messageType = challengeData.substr(0, delimiterPos);

	// Check if this message should be forwarded using a DRM callback.
	// Example message: individualization-request:Type:(payload)
	if ((delimiterPos != std::string::npos) && (individualisationTypes.count(messageType) > 0))
	{
		MW_LOG_WARN("processOCDMChallenge received message with type=%s", messageType.c_str());

		if (m_drmCallbacks)
		{
			m_drmCallbacks->Individualization(challengeData.substr(delimiterPos + delimiter.length()));
		}
	}
	else
	{
		// Assuming this is a standard challenge callback
		m_challenge = challengeData;
		MW_LOG_WARN("processOCDMChallenge challenge = %s", m_challenge.c_str());

		m_destUrl.assign(destUrl);
		MW_LOG_WARN("processOCDMChallenge destUrl = %s (default value used as drm server)", m_destUrl.c_str());

		m_challengeReady.signal();
	}

	if(messageType == LICENSE_RENEWAL_MESSAGE_TYPE)
	{
		if (m_drmCallbacks)
			m_drmCallbacks->LicenseRenewal(m_drmHelper,static_cast<DrmSession*> (this));
	}
}

void OCDMSessionAdapter::keyUpdateOCDM(const uint8_t key[], const uint8_t keySize) {
	MW_LOG_INFO("at %p", this);
	// Validate input parameters
	if (key != nullptr && keySize > 0)
	{
		// Convert key to keyId - common for both branches
		std::vector<uint8_t> keyData = RawKeyToKeyId(key, keySize);

		if (m_session)
		{
			m_keyStatus = m_session->getStatus(key, keySize);
			m_keyStateIndeterminate = false;
		}
		else
		{
			m_keyStored.clear();
			m_keyStored.assign(key, key + keySize);
			m_keyStateIndeterminate = true;
		}

		// Update usable keys list (common for both branches)
		{
			std::lock_guard<std::mutex> lock(m_usableKeysMutex);
			// Check if this key already exists to avoid duplicates
			if (std::find(m_usableKeys.begin(), m_usableKeys.end(), keyData) == m_usableKeys.end())
			{
				m_usableKeys.push_back(keyData);
			}
		}
	}
}

void OCDMSessionAdapter::keysUpdatedOCDM() {
	MW_LOG_INFO("at %p", this);
	m_keyStatusReady.signal();
}


DrmData * OCDMSessionAdapter::generateKeyRequest(string& destinationURL, uint32_t timeout)
{
	MW_LOG_INFO("at %p", this);
	DrmData * result = NULL;

	m_eKeyState = KEY_ERROR;

	if (m_challengeReady.wait(timeout) == true) {
		if (m_challenge.empty() != true) {
			std::string delimiter (":Type:");
			std::string requestType (m_challenge.substr(0, m_challenge.find(delimiter)));
			if ( (requestType.size() != 0) && (requestType.size() !=  m_challenge.size()) ) {
				(void) m_challenge.erase(0, m_challenge.find(delimiter) + delimiter.length());
			}

			result = new DrmData(m_challenge.c_str(), m_challenge.length());
			destinationURL.assign((m_destUrl.c_str()));
			MW_LOG_WARN("destinationURL is %s (default value used as drm server)", destinationURL.c_str());
			m_eKeyState = KEY_PENDING;
		}
		else {
			MW_LOG_WARN("Empty keyRequest");
		}
	} else {
		MW_LOG_WARN("Timed out waiting for keyRequest");
	}
	return result;
}


int OCDMSessionAdapter::processDRMKey(DrmData* key, uint32_t timeout)
{
	MW_LOG_INFO("at %p", this);
	int retValue = -1;

	OpenCDMError status = OpenCDMError::ERROR_NONE;

	if (key)
	{
		const uint8_t* keyMessage   = reinterpret_cast<const uint8_t*>(key->getData().c_str());
		const uint16_t keyMsgLength = static_cast<uint16_t>(key->getDataLength());

		MW_LOG_INFO("Calling session update, key length=%u", keyMsgLength);
		status = m_session->update(keyMessage, keyMsgLength);
	}
	else
	{
		// If no key data has been provided then this suggests the key acquisition
		// will be performed by the DRM implementation itself. Hence there is no
		// need to call session update
		MW_LOG_INFO("NULL key data provided, assuming external key acquisition");
	}

	if (status == OpenCDMError::ERROR_NONE) {
		if (m_keyStatusReady.wait(timeout) == true) {
			MW_LOG_WARN("Key Status updated");
		}
		// The key could be signalled ready before the session is even created, so we need to check we didn't miss it
		if (m_keyStateIndeterminate) {
			m_keyStatus = m_session->getStatus(m_keyStored.data(),
			                                   static_cast<uint8_t>(m_keyStored.size()));
			m_keyStateIndeterminate = false;
			MW_LOG_WARN("Key arrived early, new state is %d", m_keyStatus);
		}
		if (m_keyStatus == KeyStatus::Usable) {
			MW_LOG_WARN("processKey: Key Usable!");
			m_eKeyState = KEY_READY;
			retValue = 0;
		}
		else if(m_keyStatus == KeyStatus::HWError)
		{
			//  SAGE Hang .. Need to restart the wpecdmi process and then self kill player to recover
			MW_LOG_WARN("processKey: Update() returned HWError.Restarting process...");
			ProcessHandler processHandler;
			// In Release another process handles opencdm which needs to be restarts .In Sprint this process is not available.
			// So check if process exists before killing it .
			if (processHandler.KillProcess("WPEFramework")) /** Current OCDM process **/
			{
				MW_LOG_WARN("OCDM HWError reported.. Killed the process WPEFramework for recovery..");
			}
			else 
			{
				if(processHandler.KillProcess("WPEcdmi")) /** Backword compatibility **/
				{
					MW_LOG_WARN("OCDM HWError reported.. Killed the process WPEcdmi for recovery..");
				}
			} 

			// wait for 5sec for all the logs to be flushed
			sleep(5);
			// Now kill self
			processHandler.SelfKill();
		}
		else {
			if(m_keyStatus == KeyStatus::OutputRestricted)
			{
				MW_LOG_WARN("processKey: Update() Output restricted keystatus: %d", (int) m_keyStatus);
				retValue = HDCP_OUTPUT_PROTECTION_FAILURE;
			}
			else if(m_keyStatus == KeyStatus::OutputRestrictedHDCP22)
			{
				MW_LOG_WARN("processKey: Update() Output Compliance error keystatus: %d\n", (int) m_keyStatus);
				retValue = HDCP_COMPLIANCE_CHECK_FAILURE;
			}
			else
			{
				MW_LOG_WARN("processKey: Update() returned keystatus: %d\n", (int) m_keyStatus);
				retValue = (int) m_keyStatus;
			}
			m_eKeyState = KEY_ERROR;
		}
	}
	m_keyStatusWait.signal();
	return retValue;
}


bool OCDMSessionAdapter::waitForState(KeyState state, const uint32_t timeout)
{
	if (m_eKeyState == state) {
		return true;
	}
	if (!m_keyStatusWait.wait(timeout)) {
		return false;
	}
	return m_eKeyState == state;
}


KeyState OCDMSessionAdapter::getState()
{
	return m_eKeyState;
}


void OCDMSessionAdapter:: clearDecryptContext()
{
	MW_LOG_WARN("[HHH] clearDecryptContext.");

	std::lock_guard<std::mutex> guard(decryptMutex);

	if (m_session) {
		m_session->close();
		m_session->destruct();
		m_session.reset();
	}

	// Clear usable keys when clearing the session
	{
		std::lock_guard<std::mutex> keyLock(m_usableKeysMutex);
		m_usableKeys.clear();
	}

	m_eKeyState = KEY_INIT;
}

void OCDMSessionAdapter::setKeyId(const std::vector<uint8_t>& keyId)
{
	m_keyId = keyId;
}

int32_t OCDMSessionAdapter::getMediaKeySessionId() const
{
	if (m_session)
		return m_session->getMediaKeySessionId();
	return -1;
}

bool OCDMSessionAdapter::verifyOutputProtection()
{
	if (m_drmHelper->isHdcp22Required() && m_pOutputProtection->IsSourceUHD())
	{
		// Source material is UHD
		if (!m_pOutputProtection->isHDCPConnection2_2())
		{
			// UHD and not HDCP 2.2
			MW_LOG_WARN("UHD source but not HDCP 2.2. FAILING decrypt");
			return false;
		}
	}

	return true;
}

/**
 * @fn getUsableKeys
 * @brief Get the list of usable key IDs from the DRM session
 * @retval Reference to vector of usable key IDs
 */
const std::vector<std::vector<uint8_t>>& OCDMSessionAdapter::getUsableKeys() const
{
	std::lock_guard<std::mutex> lock(m_usableKeysMutex);
	return m_usableKeys;
}
