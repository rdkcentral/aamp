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
 * @brief Handles operation with OCDM session to handle DRM License data
 */
#include "opencdmsessionadapter.h"
#include "DrmHelper.h"
#include "PlayerUtils.h"

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
OCDMSessionAdapter::OCDMSessionAdapter(DrmHelperPtr drmHelper, DrmCallbacks *callbacks) :
		DrmSession(drmHelper->ocdmSystemId()),
		m_eKeyState(KEY_INIT),
		m_pOpenCDMSystem(NULL),
		m_pOpenCDMSession(NULL),
		m_pOutputProtection(NULL),
		decryptMutex(),
		m_sessionID(),
		m_challenge(),
		timeBeforeCallback(0),
		m_challengeReady(),
		m_challengeSize(0),
		m_keyStatus(InternalError),
		m_keyStateIndeterminate(false),
		m_keyStatusReady(),
		m_OCDMSessionCallbacks(),
		m_destUrl(),
		m_drmHelper(drmHelper),
		m_drmCallbacks(callbacks),
		m_keyStatusWait(),
		m_keyId(),
		m_keyStored()
{
	MW_LOG_WARN("OCDMSessionAdapter :: enter ");
	MW_LOG_WARN("OCDMSessionAdapter :: key process timeout is %d", drmHelper->keyProcessTimeout());

	initDRMSystem();

	// Get output protection pointer
	m_pOutputProtection = PlayerExternalsInterface::GetPlayerExternalsInterfaceInstance();
	MW_LOG_WARN("OCDMSessionAdapter :: exit ");
}


void OCDMSessionAdapter::initDRMSystem()
{
	std::lock_guard<std::mutex> guard(decryptMutex);
	MW_LOG_WARN("initDRMSystem :: enter ");
	if (m_pOpenCDMSystem == nullptr) {
#ifdef USE_THUNDER_OCDM_API_0_2
		m_pOpenCDMSystem = opencdm_create_system(m_keySystem.c_str());
#else
		m_pOpenCDMSystem = opencdm_create_system();
#endif
		if (m_pOpenCDMSystem == nullptr) {
			MW_LOG_ERR("opencdm_create_system() FAILED");
		}
	}
	MW_LOG_WARN("initDRMSystem :: exit ");
}


OCDMSessionAdapter::~OCDMSessionAdapter()
{
	MW_LOG_WARN("[HHH]OCDMSessionAdapter destructor called! keySystem %s", m_keySystem.c_str());
	clearDecryptContext();

	if (m_pOpenCDMSystem) {
#ifdef USE_THUNDER_OCDM_API_0_2
		opencdm_destruct_system(m_pOpenCDMSystem);
#endif
		m_pOpenCDMSystem = NULL;
	}

}


void OCDMSessionAdapter::generateDRMSession(const uint8_t *f_pbInitData,
		uint32_t f_cbInitData, std::string &customData)
{
	MW_LOG_INFO("at %p, with %p, %p", this , m_pOpenCDMSystem, m_pOpenCDMSession);

	std::lock_guard<std::mutex> guard(decryptMutex);
	if (m_pOpenCDMSystem == nullptr)
	{
		MW_LOG_WARN("OpenCDM system not present, unable to generate DRM session");
		m_eKeyState = KEY_ERROR;
	}
	else
	{
		memset(&m_OCDMSessionCallbacks, 0, sizeof(m_OCDMSessionCallbacks));
		timeBeforeCallback = GetCurrentTimeMS();
		m_OCDMSessionCallbacks.process_challenge_callback = [](OpenCDMSession* session, void* userData, const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize) {
			OCDMSessionAdapter* userSession = reinterpret_cast<OCDMSessionAdapter*>(userData);
			userSession->timeBeforeCallback = ((GetCurrentTimeMS())-(userSession->timeBeforeCallback));
			MW_LOG_WARN( "Duration for process_challenge_callback %lld",(userSession->timeBeforeCallback));
			userSession->processOCDMChallenge(destUrl, challenge, challengeSize);
		};

		m_OCDMSessionCallbacks.key_update_callback = [](OpenCDMSession* session, void* userData, const uint8_t key[], const uint8_t keySize) {
			OCDMSessionAdapter* userSession = reinterpret_cast<OCDMSessionAdapter*>(userData);
			userSession->keyUpdateOCDM(key, keySize);
		};

		m_OCDMSessionCallbacks.error_message_callback = [](OpenCDMSession* session, void* userData, const char message[]) {
		};

		m_OCDMSessionCallbacks.keys_updated_callback = [](const OpenCDMSession* session, void* userData) {
			OCDMSessionAdapter* userSession = reinterpret_cast<OCDMSessionAdapter*>(userData);
			userSession->keysUpdatedOCDM();
		};
		const unsigned char *customDataMessage = customData.empty() ? nullptr:reinterpret_cast<const unsigned char *>(customData.c_str()) ;
		const uint16_t customDataMessageLength = customData.length();
		MW_LOG_INFO("data length : %d: ", customDataMessageLength);
#ifdef USE_THUNDER_OCDM_API_0_2
	OpenCDMError ocdmRet = opencdm_construct_session(m_pOpenCDMSystem, LicenseType::Temporary, "cenc",
#else
    OpenCDMError ocdmRet = opencdm_construct_session(m_pOpenCDMSystem, m_keySystem.c_str(), LicenseType::Temporary, "cenc",
#endif
				  const_cast<unsigned char*>(f_pbInitData), f_cbInitData,
				  customDataMessage, customDataMessageLength,
				  &m_OCDMSessionCallbacks,
				  static_cast<void*>(this),
				  &m_pOpenCDMSession);
		if (ocdmRet != ERROR_NONE)
		{
			MW_LOG_ERR("Error constructing OCDM session. OCDM err=0x%x", ocdmRet);
			m_eKeyState = KEY_ERROR;
		}
	}
}


void OCDMSessionAdapter::processOCDMChallenge(const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize) {

	MW_LOG_INFO("at %p, with %p, %p", this , m_pOpenCDMSystem, m_pOpenCDMSession);

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
	MW_LOG_INFO("at %p, with %p, %p", this , m_pOpenCDMSystem, m_pOpenCDMSession);
	if (m_pOpenCDMSession) {
		m_keyStatus = opencdm_session_status(m_pOpenCDMSession, key, keySize);
		m_keyStateIndeterminate = false;
	} 
	else {
		m_keyStored.clear();
		m_keyStored.assign(key, key+keySize);
		m_keyStateIndeterminate = true;
	}
  
}

void OCDMSessionAdapter::keysUpdatedOCDM() {
	MW_LOG_INFO("at %p, with %p, %p", this , m_pOpenCDMSystem, m_pOpenCDMSession);
	m_keyStatusReady.signal();
}


DrmData * OCDMSessionAdapter::generateKeyRequest(string& destinationURL, uint32_t timeout)
{
	MW_LOG_INFO("at %p, with %p, %p", this , m_pOpenCDMSystem, m_pOpenCDMSession);
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
	MW_LOG_INFO("at %p, with %p, %p", this , m_pOpenCDMSystem, m_pOpenCDMSession);
	int retValue = -1;
	const uint8_t* keyMessage = NULL;
	uint16_t keyMessageLength = 0;

	OpenCDMError status = OpenCDMError::ERROR_NONE;

	if (key)
	{
		keyMessage = (const uint8_t *)key->getData().c_str();
		keyMessageLength = key->getDataLength();
	}

	if (keyMessage)
	{
		MW_LOG_INFO("Calling opencdm_session_update, key length=%u", keyMessageLength);
		status = opencdm_session_update(m_pOpenCDMSession, keyMessage, keyMessageLength);
	}
	else
	{
		// If no key data has been provided then this suggests the key acquisition
		// will be performed by the DRM implementation itself. Hence there is no
		// need to call opencdm_session_update
		MW_LOG_INFO("NULL key data provided, assuming external key acquisition");
	}

	if (status == OpenCDMError::ERROR_NONE) {
		if (m_keyStatusReady.wait(timeout) == true) {
			MW_LOG_WARN("Key Status updated");
		}
		// The key could be signalled ready before the session is even created, so we need to check we didn't miss it
		if (m_keyStateIndeterminate) {
			m_keyStatus = opencdm_session_status(m_pOpenCDMSession, m_keyStored.data(), m_keyStored.size());
			m_keyStateIndeterminate = false;
			MW_LOG_WARN("Key arrived early, new state is %d", m_keyStatus);
		}
#ifdef USE_THUNDER_OCDM_API_0_2
		if (m_keyStatus == Usable) {
#else
		if (m_keyStatus == KeyStatus::Usable) {
#endif
			MW_LOG_WARN("processKey: Key Usable!");
			m_eKeyState = KEY_READY;
			retValue = 0;
		}
#ifdef USE_THUNDER_OCDM_API_0_2
		else if(m_keyStatus == HWError)
#else
		else if(m_keyStatus == KeyStatus::HWError)
#endif
		{
			//  SAGE Hang .. Need to restart the wpecdmi process and then self kill player to recover
			MW_LOG_WARN("processKey: Update() returned HWError.Restarting process...");
			int systemResult = -1;
			// In Release another process handles opencdm which needs to be restarts .In Sprint this process is not available.
			// So check if process exists before killing it .
			systemResult = system("pgrep WPEcdmi");
			if(systemResult == 0)
			{
				systemResult = system("pkill -9 WPEcdmi");
				if(systemResult != 0)
				{
					MW_LOG_WARN("Unable to shutdown WPEcdmi process.%d", systemResult);
				}
			}
			else
			{
				// check for WPEFramework process
				systemResult = system("pgrep WPEFramework");
				if(systemResult == 0)
				{
					systemResult = system("pkill -9 WPEFramework");
					if(systemResult != 0)
					{
						MW_LOG_WARN("Unable to shutdown WPEFramework process.%d", systemResult);
					}
				}
			}

			// wait for 5sec for all the logs to be flushed
			sleep(5);
			// Now kill self
			if(kill(getpid(), SIGKILL) < 0)
			{
				MW_LOG_WARN("Kill Failed = %d", errno);  //CID:88415 - checked return
			}
		}
		else {
#ifdef USE_THUNDER_OCDM_API_0_2
			if(m_keyStatus == OutputRestricted)
#else
			if(m_keyStatus == KeyStatus::OutputRestricted)
#endif
			{
				MW_LOG_WARN("processKey: Update() Output restricted keystatus: %d", (int) m_keyStatus);
				retValue = HDCP_OUTPUT_PROTECTION_FAILURE;
			}
#ifdef USE_THUNDER_OCDM_API_0_2
			else if(m_keyStatus == OutputRestrictedHDCP22)
#else
			else if(m_keyStatus == KeyStatus::OutputRestrictedHDCP22)
#endif
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

	if (m_pOpenCDMSession) {
		opencdm_session_close(m_pOpenCDMSession);
		opencdm_destruct_session(m_pOpenCDMSession);
		m_pOpenCDMSession = NULL;
	}
	m_eKeyState = KEY_INIT;
}

void OCDMSessionAdapter::setKeyId(const std::vector<uint8_t>& keyId)
{
	m_keyId = keyId;
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
