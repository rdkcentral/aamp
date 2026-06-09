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
 * @file PlayerDirectRialtoCCManager.h
 * @brief PlayerCCManagerBase subclass for the direct-rialto backend.
 *
 * Replaces PlayerRialtoCCManager (which calls g_object_set on a GstElement*)
 * with a GStreamer-free implementation that forwards track selection and
 * mute/un-mute through the IDirectRialtoCC control interface implemented by
 * AampRialtoPlayer.
 *
 * Lifecycle:
 *  1. PlayerCCManager::GetInstance() creates this instance when
 *     mCCManagerType == DirectRialtoCCManager, which is set by calling
 *     PlayerCCManager::SetRialto(true, true) during player construction.
 *  2. When the first PLAYING playback-state arrives, AampRialtoPlayer passes
 *     itself (as IDirectRialtoCC*) via NotifyFirstFrameReceived(), which
 *     flows through priv_aamp::InitializeCC() → PlayerCCManagerBase::Init()
 *     → Initialize() here, storing the control pointer.
 *  3. Subsequent SetTrack() / StartRendering() / StopRendering() calls from
 *     priv_aamp drive track selection and muting through the stored pointer.
 */

#ifndef PLAYER_DIRECT_RIALTO_CC_MANAGER_H
#define PLAYER_DIRECT_RIALTO_CC_MANAGER_H

#include "PlayerCCManager.h"
#include "IDirectRialtoCC.h"

#include <mutex>
#include <set>

/**
 * @class PlayerDirectRialtoCCManager
 * @brief Closed-caption manager for the direct-rialto path.
 */
class PlayerDirectRialtoCCManager : public PlayerCCManagerBase
{
public:
	PlayerDirectRialtoCCManager() = default;
	~PlayerDirectRialtoCCManager() override = default;

	/// @copydoc PlayerCCManagerBase::GetId
	int GetId() override;

	/// @copydoc PlayerCCManagerBase::Release
	void Release(int iID) override;

	/// @copydoc PlayerCCManagerBase::SetTrack
	int SetTrack(const std::string &track,
	             CCFormat format = eCLOSEDCAPTION_FORMAT_DEFAULT) override;

protected:
	/// @copydoc PlayerCCManagerBase::Initialize
	int Initialize(void *handle) override;

	/// @copydoc PlayerCCManagerBase::StartRendering
	void StartRendering() override;

	/// @copydoc PlayerCCManagerBase::StopRendering
	void StopRendering() override;

	/// @copydoc PlayerCCManagerBase::SetDigitalChannel
	int SetDigitalChannel(unsigned int id) override { return 0; }

	/// @copydoc PlayerCCManagerBase::SetAnalogChannel
	int SetAnalogChannel(unsigned int id) override { return 0; }

	/// @copydoc PlayerCCManagerBase::ResetState
	void ResetState() override;

private:
	/**
	 * @brief Map a CC track string to the text-track-identifier expected by
	 *        the Rialto server, mirroring PlayerRialtoCCManager::SetTrack().
	 *
	 * If the track string starts with a digit, a prefix is prepended based
	 * on the format: "CC" for 608, "SERVICE" for 708.
	 */
	static std::string mapTrackIdentifier(const std::string &track,
	                                      CCFormat format);

	/// Non-owning pointer to the AampRialtoPlayer CC control interface.
	/// Null until Initialize() is called (first PLAYING state).
	IDirectRialtoCC *m_control{nullptr};

	/// Guards mId / mIdSet.
	std::mutex m_idLock;
	int        m_id{0};
	std::set<int> m_idSet;
};

#endif // PLAYER_DIRECT_RIALTO_CC_MANAGER_H
