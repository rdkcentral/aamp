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
* @file DrmSession.cpp
* @brief Source file for DrmSession.
*/

#include "DrmSession.h"
#include "PlayerLogManager.h"

/**
 * @brief Constructor for DrmSession.
 */
DrmSession::DrmSession(const string &keySystem) : m_keySystem(keySystem),m_OutputProtectionEnabled(false)
		, mPlayerSecManagerSession()
{
}

/**
 * @brief Destructor for DrmSession..
 */
DrmSession::~DrmSession()
{
}

/**
 * @brief Get the DRM System, ie, UUID for PlayReady WideVine etc..
 */
string DrmSession::getKeySystem()
{
	return m_keySystem;
}

/**
 * @brief Function to decrypt GStreamer stream  buffer.
 */
int DrmSession::decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps)
{
	MW_LOG_ERR("GST decrypt method not implemented");
	return -1;
}

/**
 * @brief Function to decrypt stream  buffer.
 */
int DrmSession::decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData)
{
	MW_LOG_ERR("Standard decrypt method not implemented");
	return -1;
}
