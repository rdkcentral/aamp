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
 * @file AampDrmBridge.cpp
 * @brief Production concrete implementation of IDrmBridge.
 */

#include "AampDrmBridge.h"
#include "priv_aamp.h"
#include "AampDRMLicManager.h"
#include "DrmSessionManager.h"
#include "DrmMediaFormat.h"
#include "AampLogManager.h"

AampDrmBridge::AampDrmBridge(PrivateInstanceAAMP *aamp)
	: m_aamp(aamp)
{
}

int32_t AampDrmBridge::createSession(
	const char    *systemId,
	const void    *initData,
	size_t         len,
	AampMediaType  type)
{
	if (!m_aamp || !systemId || !initData || len == 0)
	{
		AAMPLOG_WARN("AampDrmBridge::createSession: invalid arguments");
		return -1;
	}

	AampDRMLicenseManager *licMgr = m_aamp->mDRMLicenseManager;
	if (!licMgr || !licMgr->mDrmSessionManager)
	{
		AAMPLOG_WARN("AampDrmBridge::createSession: DRM license manager not available");
		return -1;
	}

	DrmSessionManager *dsm = licMgr->mDrmSessionManager;

	int responseCode = 0;
	int err          = 0;

	// streamType must be cast-compatible with GstMediaType:
	//   eGST_MEDIATYPE_VIDEO (0) == eMEDIATYPE_VIDEO
	//   eGST_MEDIATYPE_AUDIO (1) == eMEDIATYPE_AUDIO
	const int streamType = static_cast<int>(type);

	AAMPLOG_INFO("AampDrmBridge::createSession systemId=%s len=%zu type=%d",
		systemId, len, streamType);

	DrmSession *session = dsm->createDrmSession(
		responseCode,
		err,
		systemId,
		eMEDIAFORMAT_DASH,
		static_cast<const unsigned char *>(initData),
		static_cast<uint16_t>(len),
		streamType,
		m_aamp,      // PrivateInstanceAAMP implements DrmCallbacks
		nullptr);    // content metadata — not available at this call site

	if (!session)
	{
		AAMPLOG_ERR("AampDrmBridge::createSession: createDrmSession failed "
			"responseCode=%d err=%d", responseCode, err);
		return -1;
	}

	const int32_t mksId = session->getMediaKeySessionId();
	AAMPLOG_INFO("AampDrmBridge::createSession: mksId=%d", mksId);
	return mksId;
}

void AampDrmBridge::clearSessions()
{
	AAMPLOG_INFO("AampDrmBridge::clearSessions");
	if (!m_aamp)
		return;

	AampDRMLicenseManager *licMgr = m_aamp->mDRMLicenseManager;
	if (licMgr && licMgr->mDrmSessionManager)
	{
		licMgr->mDrmSessionManager->clearDrmSession(/*forceClearSession=*/true);
	}
}
