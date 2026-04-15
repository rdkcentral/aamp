/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file OpenCDMProvider.cpp
 * @brief Concrete IOpenCDM / IOpenCDMSession implementation for the real OCDM
 *        C library.
 *
 * All opencdm_* C function calls are isolated to this file.
 * USE_THUNDER_OCDM_API_0_2 variant selection also lives here exclusively.
 */

#include "OpenCDMProvider.h"
#include "PlayerLogManager.h"

#include <gst/gst.h>
#include <cstring>

// ---------------------------------------------------------------------------
// OpenCDMSessionProvider
// ---------------------------------------------------------------------------

OpenCDMSessionProvider::OpenCDMSessionProvider(OpenCDMSession* handle,
                                              OpenCDMSessionCallbackSet* callbacks)
	: m_handle(handle)
	, m_callbacks(callbacks)
	, m_gstDecrypt(nullptr)
{
	// Resolve the preferred GStreamer session decrypt function at runtime.
	// This mirrors the existing dlsym logic previously in OCDMGSTSessionAdapter.
	m_gstDecrypt = reinterpret_cast<OpenCDMError(*)(OpenCDMSession*, GstBuffer*, GstCaps*)>(
		dlsym(RTLD_DEFAULT, "opencdm_gstreamer_session_decrypt_buffer"));

	if (m_gstDecrypt)
	{
		MW_LOG_WARN("[DRM_FLOW] OpenCDMSessionProvider: resolved opencdm_gstreamer_session_decrypt_buffer");
	}
	else
	{
		MW_LOG_WARN("[DRM_FLOW] OpenCDMSessionProvider: opencdm_gstreamer_session_decrypt_buffer not found, will use legacy fallback");
	}
}

KeyStatus OpenCDMSessionProvider::getStatus(const uint8_t* keyId, uint8_t keyIdSize)
{
	return opencdm_session_status(m_handle, keyId, keyIdSize);
}

OpenCDMError OpenCDMSessionProvider::update(const uint8_t* keyMessage, uint16_t keyMessageLength)
{
	return opencdm_session_update(m_handle, keyMessage, keyMessageLength);
}

OpenCDMError OpenCDMSessionProvider::close()
{
	return opencdm_session_close(m_handle);
}

OpenCDMError OpenCDMSessionProvider::destruct()
{
	OpenCDMError err = opencdm_destruct_session(m_handle);
	m_handle = nullptr;
	delete m_callbacks;
	m_callbacks = nullptr;
	return err;
}

int32_t OpenCDMSessionProvider::getMediaKeySessionId()
{
	// The real OCDM library does not expose a Rialto media key session ID.
	// The Rialto session ID is only meaningful on the Rialto Direct path
	// (RialtoOpenCDMSession), where it is returned without any OCDM API call.
	return -1;
}

bool OpenCDMSessionProvider::hasGstDecryptBuffer()
{
	return m_gstDecrypt != nullptr;
}

OpenCDMError OpenCDMSessionProvider::decrypt(uint8_t* data,
                                              uint32_t dataSize,
                                              EncryptionScheme encScheme,
                                              EncryptionPattern pattern,
                                              const uint8_t* iv,
                                              uint16_t ivSize,
                                              const uint8_t* keyId,
                                              uint16_t keyIdSize,
                                              uint32_t initWithLast15)
{
	return opencdm_session_decrypt(m_handle,
	                               data, dataSize,
	                               encScheme, pattern,
	                               iv, ivSize,
	                               keyId, keyIdSize,
	                               initWithLast15);
}

OpenCDMError OpenCDMSessionProvider::decryptGst(GstBuffer* buffer, GstCaps* caps)
{
	if (m_gstDecrypt)
	{
		return m_gstDecrypt(m_handle, buffer, caps);
	}
	// Should not be called without confirming m_gstDecrypt is available.
	return ERROR_FAIL;
}

OpenCDMError OpenCDMSessionProvider::decryptGstLegacy(GstBuffer* buffer,
                                                       GstBuffer* subSamples,
                                                       uint32_t subSampleCount,
                                                       GstBuffer* iv,
                                                       GstBuffer* keyId,
                                                       uint32_t initWithLast15)
{
	return opencdm_gstreamer_session_decrypt(m_handle,
	                                         buffer,
	                                         subSamples,
	                                         subSampleCount,
	                                         iv,
	                                         keyId,
	                                         initWithLast15);
}

// ---------------------------------------------------------------------------
// OpenCDMProvider
// ---------------------------------------------------------------------------

OpenCDMProvider::OpenCDMProvider(const std::string& keySystem)
	: m_system(nullptr)
{
	MW_LOG_WARN("OpenCDMProvider: creating system for keySystem=%s", keySystem.c_str());
#ifdef USE_THUNDER_OCDM_API_0_2
	m_system = opencdm_create_system(keySystem.c_str());
#else
	m_system = opencdm_create_system();
#endif
	if (!m_system)
	{
		MW_LOG_ERR("OpenCDMProvider: opencdm_create_system() FAILED");
	}
}

OpenCDMProvider::~OpenCDMProvider()
{
	if (m_system)
	{
#ifdef USE_THUNDER_OCDM_API_0_2
		opencdm_destruct_system(m_system);
#endif
		m_system = nullptr;
	}
}

std::unique_ptr<IOpenCDMSession> OpenCDMProvider::constructSession(
	const std::string&              keySystem,
	LicenseType                     licenseType,
	const std::string&              initDataType,
	const uint8_t*                  initData,
	uint32_t                        initDataSize,
	const uint8_t*                  customData,
	uint16_t                        customDataSize,
	const OpenCDMSessionCallbackSet& callbacks)
{
	if (!m_system)
	{
		MW_LOG_WARN("OpenCDMProvider::constructSession: system not available");
		return nullptr;
	}

	// Build C-style callback struct, bridging to the std::function set.
	// The OCDMSessionAdapter instance is captured via userData.
	OpenCDMSessionCallbacks cCallbacks{};

	// We need a heap-allocated copy of the callback set so the C lambdas
	// can capture it via the userData pointer.  The copy lives for the
	// lifetime of the session.
	auto* cbCopy = new OpenCDMSessionCallbackSet(callbacks);

	cCallbacks.process_challenge_callback = [](OpenCDMSession*, void* userData,
	                                           const char destUrl[],
	                                           const uint8_t challenge[],
	                                           const uint16_t challengeSize)
	{
		auto* cb = static_cast<OpenCDMSessionCallbackSet*>(userData);
		if (cb->onChallenge)
		{
			cb->onChallenge(destUrl, challenge, challengeSize);
		}
	};

	cCallbacks.key_update_callback = [](OpenCDMSession*, void* userData,
	                                    const uint8_t key[],
	                                    const uint8_t keySize)
	{
		auto* cb = static_cast<OpenCDMSessionCallbackSet*>(userData);
		if (cb->onKeyUpdate)
		{
			cb->onKeyUpdate(key, keySize);
		}
	};

	cCallbacks.error_message_callback = [](OpenCDMSession*, void*, const char[]) {};

	cCallbacks.keys_updated_callback = [](const OpenCDMSession*, void* userData)
	{
		auto* cb = static_cast<OpenCDMSessionCallbackSet*>(userData);
		if (cb->onKeysUpdated)
		{
			cb->onKeysUpdated();
		}
	};

	OpenCDMSession* rawSession = nullptr;

#ifdef USE_THUNDER_OCDM_API_0_2
	OpenCDMError err = opencdm_construct_session(
		m_system,
		licenseType,
		initDataType.c_str(),
		const_cast<uint8_t*>(initData), initDataSize,
		customData, customDataSize,
		&cCallbacks,
		static_cast<void*>(cbCopy),
		&rawSession);
#else
	OpenCDMError err = opencdm_construct_session(
		m_system,
		keySystem.c_str(),
		licenseType,
		initDataType.c_str(),
		const_cast<uint8_t*>(initData), initDataSize,
		customData, customDataSize,
		&cCallbacks,
		static_cast<void*>(cbCopy),
		&rawSession);
#endif

	if (err != ERROR_NONE || !rawSession)
	{
		MW_LOG_ERR("[DRM_FLOW] OpenCDMProvider::constructSession: opencdm_construct_session FAILED for %s, err=0x%x", keySystem.c_str(), err);
		delete cbCopy;
		return nullptr;
	}

	MW_LOG_WARN("[DRM_FLOW] OpenCDMProvider::constructSession: OK for %s", keySystem.c_str());
	return std::make_unique<OpenCDMSessionProvider>(rawSession, cbCopy);
}
