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

#ifndef OpenCDMSessionAdapter_h
#define OpenCDMSessionAdapter_h

/**
 * @file opencdmsessionadapter.h
 * @brief Handles operation with OCDM session to handle DRM License data
 */

#include "PlayerExternalsInterface.h"
#include "DrmSession.h"
#include "DrmHelper.h"

#include "open_cdm.h"
#include "open_cdm_adapter.h"
#include <condition_variable>
#include "DrmCallbacks.h"

using namespace std;

/**
 * @class Event
 * @brief class to DRM Event handle
 */

class Event {
private:
	bool signalled; //TODO: added to handle the events fired before calling wait, need to recheck
	std::mutex lock;
	std::condition_variable condition;
public:
	Event() : signalled(false), lock(), condition() {
	}
	virtual ~Event() {
	}

	inline bool wait(const uint32_t waitTime)
	{
		bool ret = true;
		std::unique_lock<std::mutex> _lock(lock);
		if (!signalled) {
			if (waitTime == 0) {
				condition.wait(_lock);
			} else {
				if( std::cv_status::timeout == condition.wait_for(_lock,std::chrono::milliseconds(waitTime)) )
				{
					ret = false;
				}
			}
		}
		signalled = false;
		return ret;
	}

	inline void signal()
	{
		std::unique_lock<std::mutex> _lock(lock);
		signalled = true;
		condition.notify_all();
	}
};

/**
 * @class OCDMSessionAdapter
 * @brief Open CDM DRM session
 */
class OCDMSessionAdapter : public DrmSession
{
protected:
	std::mutex decryptMutex;

	KeyState m_eKeyState;

	OpenCDMSession* m_pOpenCDMSession;
#ifdef USE_THUNDER_OCDM_API_0_2
	struct OpenCDMSystem* m_pOpenCDMSystem;
#else
	struct OpenCDMAccessor* m_pOpenCDMSystem;
#endif
	OpenCDMSessionCallbacks m_OCDMSessionCallbacks;
	std::shared_ptr<PlayerExternalsInterface> m_pOutputProtection;

	std::string m_challenge;
	uint16_t m_challengeSize;

	std::string m_destUrl;
	KeyStatus m_keyStatus;
	bool m_keyStateIndeterminate;
	std::vector<uint8_t> m_keyStored;

	Event m_challengeReady;
	Event m_keyStatusReady;
	Event m_keyStatusWait;
	string m_sessionID;

	std::vector<uint8_t> m_keyId;

	DrmHelperPtr m_drmHelper;
	DrmCallbacks *m_drmCallbacks;

	bool verifyOutputProtection();
public:
	void processOCDMChallenge(const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize);
	void keysUpdatedOCDM();
	void keyUpdateOCDM(const uint8_t key[], const uint8_t keySize);
	long long timeBeforeCallback;

private:
	void initDRMSystem();

public:
    	OCDMSessionAdapter(DrmHelperPtr drmHelper, DrmCallbacks *callbacks = nullptr);
	~OCDMSessionAdapter();
    	OCDMSessionAdapter(const OCDMSessionAdapter&) = delete;
	OCDMSessionAdapter& operator=(const OCDMSessionAdapter&) = delete;
	void generateDRMSession(const uint8_t *f_pbInitData,
		uint32_t f_cbInitData, std::string &customData) override; 
	DrmData * generateKeyRequest(string& destinationURL, uint32_t timeout) override;
	int processDRMKey(DrmData* key, uint32_t timeout) override;
	KeyState getState() override;
	void clearDecryptContext() override;
#if defined(USE_OPENCDM_ADAPTER)
	void setKeyId(const std::vector<uint8_t>& keyId) override;
#endif
	bool waitForState(KeyState state, const uint32_t timeout) override;
};

#endif
