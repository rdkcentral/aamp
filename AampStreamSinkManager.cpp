/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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
 * @file aampstreamsinkmanager.cpp
 * @brief manages stream sink players
 */

#include "AampStreamSinkManager.h"
#include "priv_aamp.h"
#include "StreamAbstractionAAMP.h"
#include "AampConfig.h"

AampStreamSinkManager::AampStreamSinkManager() :
	mStreamPlayer(nullptr),
	mClientStreamSinkMap(),
	mActivePlayersMap(),
	mInactivePlayersMap(),
	mEncryptedHeaders(),
	mMediaHeaders(AAMP_TRACK_COUNT, nullptr),
	mPipelineMode(ePIPELINEMODE_UNDEFINED),
	mStreamSinkMutex(),
	mEncryptedAamp(nullptr),
	mEncryptedHeadersInjected(false)
{
}

AampStreamSinkManager::~AampStreamSinkManager()
{
	Clear();
}

void AampStreamSinkManager::Clear(void)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	for (auto it = mClientStreamSinkMap.begin(); it != mClientStreamSinkMap.end();)
	{
		// Don't delete the StreamSink as client owned
		it = mClientStreamSinkMap.erase(it);
	}
	for (auto it = mInactivePlayersMap.begin(); it != mInactivePlayersMap.end();)
	{
		delete(it->second);
		it = mInactivePlayersMap.erase(it);
	}
	if (mActivePlayersMap.size())
	{
		for (auto it = mActivePlayersMap.begin(); it != mActivePlayersMap.end();)
		{
			delete(it->second);
			it = mActivePlayersMap.erase(it);
		}
		mStreamPlayer = nullptr;
	}
	else
	{
		if (mStreamPlayer)
		{
			delete (mStreamPlayer);
			mStreamPlayer = nullptr;
		}
	}
	mPipelineMode = ePIPELINEMODE_UNDEFINED;
	mEncryptedHeaders.clear();
	mEncryptedHeadersInjected = false;
	for (auto& header : mMediaHeaders)
	{
		header.reset();
		AAMPLOG_MIL("cleared mMediaHeaders");
	}
}

void AampStreamSinkManager::SetSinglePipelineMode(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p) Pipeline mode set to Single", this );
			mPipelineMode = ePIPELINEMODE_SINGLE;

			if (!mEncryptedHeaders.empty())
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p) Encrypted headers already been set", this );
			}

			// Retain matching stream player, remove others
			for (auto it = mActivePlayersMap.begin(); it != mActivePlayersMap.end();)
			{
				if (aamp == it->first)
				{
					AAMPLOG_WARN("AampStreamSinkManager(%p) Retaining stream player created for PLAYER[%d]", this, it->first->mPlayerId);
					mStreamPlayer = it->second;
					it++;
				}
				else
				{
					AAMPLOG_WARN("AampStreamSinkManager(%p) Deleting stream player created for PLAYER[%d]", this, it->first->mPlayerId);
					delete(it->second);
					it = mActivePlayersMap.erase(it);
				}
			}
		}
		break;

		case ePIPELINEMODE_SINGLE:
		{
			AAMPLOG_TRACE("AampStreamSinkManager(%p) Pipeline mode already set as Single", this );
		}
		break;

		case ePIPELINEMODE_MULTI:
		{
			AAMPLOG_ERR("AampStreamSinkManager(%p) Pipeline mode already set Multi", this );
		}
		break;
	}
}

StreamSink* AampStreamSinkManager::CreateSinkInstance(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function<void(const unsigned char *, int, int, int)> exportFrames)
{
	StreamSink *sink = nullptr;
	// DirectRialto is not yet supported, so we will always create the GstPlayer for now.
	// The config is in place for when DirectRialto support is added.
	if (ISCONFIGSET(eAAMPConfig_useDirectRialto))
	{
		AAMPLOG_ERR("Creating AampRialtoPlayer not yet supported");
	}
	//else
	{
		AAMPLOG_MIL("Creating stream player");
		sink = new AAMPGstPlayer(aamp, std::move(id3HandlerCallback), std::move(exportFrames));
	}
	return sink;
}

void AampStreamSinkManager::CreateStreamSink(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	AampStreamSinkInactive *inactiveSink = new AampStreamSinkInactive(id3HandlerCallback);  /* For every instance of aamp, there should be an AampStreamSinkInactive object*/
	mInactivePlayersMap.insert({aamp,inactiveSink});

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mStreamPlayer == nullptr)
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_MIL("AampStreamSinkManager(%p) Single Pipeline mode, creating stream player for PLAYER[%d]", this, aamp->mPlayerId);
				mStreamPlayer = CreateSinkInstance(aamp, std::move(id3HandlerCallback), std::move(exportFrames));
				mActivePlayersMap.insert({aamp, mStreamPlayer});
			}
			else
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, not creating stream player for PLAYER[%d]", this, aamp->mPlayerId);
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_MIL("AampStreamSinkManager(%p) %s Pipeline mode, creating stream player for PLAYER[%d]", this,
						 mPipelineMode == ePIPELINEMODE_UNDEFINED ? "Undefined" : "Multi", aamp->mPlayerId);
			StreamSink *streamPlayer = CreateSinkInstance(aamp, std::move(id3HandlerCallback), std::move(exportFrames));
			mActivePlayersMap.insert({aamp, streamPlayer});
		}
		break;
	}
}

void AampStreamSinkManager::SetStreamSink(PrivateInstanceAAMP *aamp, StreamSink *clientSink)
{

	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	AAMPLOG_WARN("AampStreamSinkManager(%p) SetStreamSink for PLAYER[%d] clientSink %p", this, aamp->mPlayerId, clientSink);
	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			AAMPLOG_ERR("AampStreamSinkManager(%p) Single Pipeline mode, when setting client StreamSink", this );
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p) Undefined Pipeline, forcing to Multi Pipeline PLAYER[%d]", this, aamp->mPlayerId);
			mPipelineMode = ePIPELINEMODE_MULTI;
		}
		break;

		case ePIPELINEMODE_MULTI:
		{
			// Do nothing
		}
		break;
	}

	mClientStreamSinkMap.insert({aamp, clientSink});
}

void AampStreamSinkManager::DeleteStreamSink(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	//Do not edit or remove this log - it is used in L2 test
	AAMPLOG_WARN("AampStreamSinkManager(%p) DeleteStreamSink for PLAYER[%d]", this, aamp->mPlayerId);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActivePlayersMap.size() &&
				(aamp == mActivePlayersMap.begin()->first))
			{
				/* Erase the map of active player*/
				mActivePlayersMap.erase(aamp);
				AAMPLOG_WARN("AampStreamSinkManager(%p) No active players present", this );
			}

			if (mInactivePlayersMap.count(aamp))
			{
				AampStreamSinkInactive* sink = mInactivePlayersMap[aamp];
				mInactivePlayersMap.erase(aamp);
				delete sink;
			}

			if (mInactivePlayersMap.size())
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) %zu Inactive players present", this, mInactivePlayersMap.size());

				// check the sink was not attached to the player that is being deleted
				if (mStreamPlayer->IsAssociatedAamp(aamp))
				{
					if (mActivePlayersMap.size() == 0)
					{
						// attach it to one of the existing inactive players
						AAMPLOG_WARN("AampStreamSinkManager(%p) Deleting player associated with sink! Attaching sink to default inactive PLAYER[%d]", this, mInactivePlayersMap.begin()->first->mPlayerId);
						mStreamPlayer->ChangeAamp(mInactivePlayersMap.begin()->first,
												mInactivePlayersMap.begin()->second->GetID3MetadataHandler());
					}
				}
			}
			else
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) No inactive players present, deleting stream player pipeline PLAYER[%d]", this, aamp->mPlayerId);
				delete(mStreamPlayer);
				mStreamPlayer = nullptr;
				mPipelineMode = ePIPELINEMODE_UNDEFINED;
				mEncryptedHeadersInjected = false;
				for (auto& header : mMediaHeaders)
				{
					header.reset();
					AAMPLOG_MIL("cleared mMediaHeaders");
				}
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			if (mInactivePlayersMap.count(aamp))
			{
				AampStreamSinkInactive* sink = mInactivePlayersMap[aamp];
				mInactivePlayersMap.erase(aamp);
				delete(sink);
			}

			if (mActivePlayersMap.count(aamp))
			{
				StreamSink* sink = mActivePlayersMap[aamp];
				mActivePlayersMap.erase(aamp);
				delete(sink);
			}

			// If client supplied StreamSink just remove from map, don't delete
			if (mClientStreamSinkMap.count(aamp))
			{
				mClientStreamSinkMap.erase(aamp);
			}
		}
		break;
	}
}

void AampStreamSinkManager::SetEncryptedHeaders(PrivateInstanceAAMP *aamp, std::map<int, std::string>& mappedHeaders)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p) Ignore set encrypted headers", this );
		}
		break;
		case ePIPELINEMODE_SINGLE:
		{
			if ((mStreamPlayer != nullptr) && !mEncryptedHeaders.empty())
			{
				// If encryption info is already set, check that it has not been set from a different player
				int encryptedPlayerId = mStreamPlayer->GetEncryptedAampId();
				if (encryptedPlayerId != aamp->mPlayerId)
				{
					AAMPLOG_ERR("AampStreamSinkManager(%p) encrypted player (%d) does not match current player (%d)", this, encryptedPlayerId, aamp->mPlayerId);
					mEncryptedHeaders.clear();
					mEncryptedHeadersInjected = false;
				}
			}

			if (!mEncryptedHeaders.empty())
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p) Encrypted headers have already been set PLAYER[%d]", this, aamp->mPlayerId);
			}
			else if (mStreamPlayer != nullptr)
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p) Set encrypted player to PLAYER[%d]", this, aamp->mPlayerId);
				mStreamPlayer->SetEncryptedAamp(aamp);
				mEncryptedHeaders = mappedHeaders;
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p) No active StreamSink PLAYER[%d]", this, aamp->mPlayerId);
			}
		}
		break;
	}
}

void AampStreamSinkManager::ReinjectEncryptedHeaders()
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	mEncryptedHeadersInjected = false;
}

void AampStreamSinkManager::GetEncryptedHeaders(std::map<int, std::string>& mappedHeaders)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	if (!mEncryptedHeadersInjected)
	{
		mappedHeaders = mEncryptedHeaders;
		mEncryptedHeadersInjected = true;
	}
	else
	{
		AAMPLOG_INFO("AampStreamSinkManager(%p) Encrypted headers already injected", this );
		mappedHeaders.clear();
	}
}

void AampStreamSinkManager::DeactivatePlayer(PrivateInstanceAAMP *aamp, bool stop)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		break;

		case ePIPELINEMODE_SINGLE:
		{
			if (mActivePlayersMap.size() == 0)
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, no current active PLAYER[%d]", this, aamp->mPlayerId);
			}
			else if (mActivePlayersMap.begin()->first == aamp)
			{
				if (stop)
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, deactivating and stopping active PLAYER[%d]", this, aamp->mPlayerId);
					mEncryptedHeadersInjected = false;
					mEncryptedHeaders.clear();
					for (auto& header : mMediaHeaders)
					{
						header.reset();
						AAMPLOG_MIL("cleared mMediaHeaders");
					}
				}
				else
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, deactivating active PLAYER[%d]", this, aamp->mPlayerId);
				}
				mActivePlayersMap.erase(aamp);
			}
			else
			{
				// Can happen when Stop is called after Detach has already been called
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, asked to deactivate PLAYER[%d] when current active PLAYER[%d]", this, aamp->mPlayerId, mActivePlayersMap.begin()->first->mPlayerId);
			}
		}
		break;
	}
}

void AampStreamSinkManager::ActivatePlayer(PrivateInstanceAAMP *aamp)
{
	// N.B. GetPositionMs() must be called before locking the StreamSink mutex, to avoid deadlock.
	// This is because PrivateInstanceAAMP::GetPositionRelativeToSeekMilliseconds() calls
	// GetStreamSink, which also locks mStreamSinkMutex.
	double position = aamp->GetPositionMs() / 1000.00;
	// Initialize flushPosition with current playback position,
	// as a fallback mechanism when streamAbstraction is null
	double flushPosition = position;

	// Access mpStreamAbstractionAAMP under the PrivateInstanceAAMP stream lock
	{
		std::lock_guard<std::recursive_mutex> streamLock(aamp->GetStreamLock());
		StreamAbstractionAAMP *streamAbstraction = aamp->mpStreamAbstractionAAMP;
		if (streamAbstraction != nullptr)
		{
			//Update flushPosition from aamp->mpStreamAbstractionAAMP
			if (aamp->mMediaFormat == eMEDIAFORMAT_PROGRESSIVE)
			{
				flushPosition = streamAbstraction->GetStreamPosition();
			}
			else
			{
				flushPosition = streamAbstraction->GetFirstPTS();
			}
		}
	}

	AAMPLOG_INFO("flushPosition:%lf, position:%lf", flushPosition, position);

	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActivePlayersMap.size() == 0)
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, no current active player", this );
			}
			else if (mActivePlayersMap.begin()->first == aamp)
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, already active PLAYER[%d]", this, aamp->mPlayerId);
			}
			else
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, resetting current active PLAYER[%d]", this, mActivePlayersMap.begin()->first->mPlayerId);
				mActivePlayersMap.clear();
			}

			if (mActivePlayersMap.size() == 0)
			{
				if (mStreamPlayer != nullptr)
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, setting active PLAYER[%d]", this, aamp->mPlayerId);

					mActivePlayersMap.insert({aamp, mStreamPlayer});
					SetActive(aamp, flushPosition);
				}
				else
				{
					AAMPLOG_ERR("AampStreamSinkManager(%p) Single Pipeline mode, mStreamPlayer is null, can't set active PLAYER[%d]", this, aamp->mPlayerId);
				}
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_WARN("AampStreamSinkManager(%p) Undefined Pipeline, forcing to Multi Pipeline PLAYER[%d]", this, aamp->mPlayerId);
			mPipelineMode = ePIPELINEMODE_MULTI;
		}
		break;

		case ePIPELINEMODE_MULTI:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_INFO("AampStreamSinkManager(%p) Multi Pipeline mode, do nothing PLAYER[%d]", this, aamp->mPlayerId);
		}
		break;
	}
}

void AampStreamSinkManager::SetActive(PrivateInstanceAAMP *aamp, double position)
{
	AAMPLOG_INFO("AampStreamSinkManager(%p) Setting PLAYER[%d] active, position(%f) subtitles_muted=%d", this, aamp->mPlayerId, position, aamp->subtitles_muted.load());

	mStreamPlayer->ChangeAamp(aamp, mInactivePlayersMap[aamp]->GetID3MetadataHandler());
	aamp->mIsFlushOperationInProgress = true;
	mStreamPlayer->Flush(position, aamp->rate, true);
	aamp->mIsFlushOperationInProgress = false;
	mStreamPlayer->SetSubtitleMute(aamp->subtitles_muted);
	if(!aamp->IsTuneCompleted() && aamp->IsPlayEnabled() && (mPipelineMode == ePIPELINEMODE_SINGLE))
	{
		mStreamPlayer->ResetFirstFrame();
	}
}

/**
 * @brief Creates a singleton instance of AampStreamSinkManager
 */
AampStreamSinkManager& AampStreamSinkManager::GetInstance()
{
	static AampStreamSinkManager instance;
	return instance;
}

StreamSink* AampStreamSinkManager::GetActiveStreamSink(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	StreamSink *sink_ptr = nullptr;

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			if (mClientStreamSinkMap.count(aamp))
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning matching client Stream Sink", this );
				sink_ptr = mClientStreamSinkMap[aamp];
			}
			else if (mActivePlayersMap.count(aamp))
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning matching Stream Sink", this );
				sink_ptr = mActivePlayersMap[aamp];
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p) Stream Sink not found", this );
			}
		}
		break;
		case ePIPELINEMODE_SINGLE:
		{
			if (!mActivePlayersMap.empty())
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning active Stream Sink found", this );
				sink_ptr = mActivePlayersMap.begin()->second;
			}
			else if (mStreamPlayer != nullptr)
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) No active Stream Sink found, returning mStreamPlayer", this );
				sink_ptr = mStreamPlayer;
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p) Active Stream Sink not found", this );
			}
		}
		break;
	}

	return sink_ptr;
}

StreamSink* AampStreamSinkManager::GetStreamSink(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	return GetStreamSinkNoLock(aamp);
}

StreamSink* AampStreamSinkManager::GetStreamSinkNoLock(PrivateInstanceAAMP *aamp)
{
	StreamSink *sink_ptr = nullptr;

	if (mClientStreamSinkMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning client Stream Sink found for PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = mClientStreamSinkMap[aamp];
	}
	else if (mActivePlayersMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning active Stream Sink found for PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = mActivePlayersMap[aamp];
	}
	else if (mInactivePlayersMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning inactive Stream Sink found or PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = mInactivePlayersMap[aamp];
	}
	else
	{
		// If not found, best not to dereference the pointer in case invalid
		AAMPLOG_ERR("AampStreamSinkManager(%p) Stream Sink for aamp(%p) not found", this, aamp);
	}

	return sink_ptr;
}

StreamSink *AampStreamSinkManager::GetStoppingStreamSink(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	StreamSink *sink_ptr = nullptr;

	if ((mPipelineMode == ePIPELINEMODE_SINGLE) && mActivePlayersMap.empty())
	{
		// Check if there is any inactive player that has been tuned (excluding the calling aamp)
		bool hasTunedInactivePlayer = false;
		for (const auto& inactivePlayer : mInactivePlayersMap)
		{
			if (inactivePlayer.first != aamp)
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p) Inactive PLAYER[%d], tuned=%s",
						this, inactivePlayer.first->mPlayerId, inactivePlayer.second->IsTuned() ? "true" : "false");
				if (inactivePlayer.second->IsTuned())
				{
					hasTunedInactivePlayer = true;
					break;
				}
			}
		}

		if (!hasTunedInactivePlayer)
		{
			AAMPLOG_INFO("AampStreamSinkManager(%p) No active player and no tuned inactive players, returning single-pipeline sink for PLAYER[%d]",
					this, aamp->mPlayerId);
			sink_ptr = mStreamPlayer;
		}
		else
		{
			AAMPLOG_INFO("AampStreamSinkManager(%p) Has tuned inactive players, getting stream sink for PLAYER[%d]",
					this, aamp->mPlayerId);
			sink_ptr = GetStreamSinkNoLock(aamp);
		}
	}
	else
	{
		AAMPLOG_INFO("AampStreamSinkManager(%p) Getting stream sink for PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = GetStreamSinkNoLock(aamp);
	}

	return sink_ptr;
}

void AampStreamSinkManager::SetTuned(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	auto it = mInactivePlayersMap.find(aamp);
	if (it != mInactivePlayersMap.end())
	{
		it->second->SetTuned(true);
		AAMPLOG_INFO("AampStreamSinkManager(%p) Set tuned flag for PLAYER[%d]", this, aamp->mPlayerId);
	}
	else
	{
		AAMPLOG_WARN("AampStreamSinkManager(%p) Could not find inactive stream sink for PLAYER[%d]", this, aamp->mPlayerId);
	}
}

void AampStreamSinkManager::UpdateTuningPlayer(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	switch (mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActivePlayersMap.empty())
			{
				if (mStreamPlayer == nullptr)
				{
					AAMPLOG_ERR(
						"AampStreamSinkManager(%p) No single pipeline stream sink PLAYER[%d]",
						this, aamp->mPlayerId);
				}
				else if (mInactivePlayersMap.count(aamp) == 0)
				{
					AAMPLOG_ERR(
						"AampStreamSinkManager(%p) No inactive stream sink for PLAYER[%d]",
						this, aamp->mPlayerId);
				}
				else
				{
					AAMPLOG_WARN(
						"AampStreamSinkManager(%p) Single pipeline stream sink with no active players, update player to PLAYER[%d]",
						this, aamp->mPlayerId);

					mStreamPlayer->ChangeAamp(aamp,
										   mInactivePlayersMap[aamp]->GetID3MetadataHandler());
				}
			}
			else
			{
				AAMPLOG_INFO(
					"AampStreamSinkManager(%p) Active stream sink exists, do not update PLAYER[%d]",
					this, aamp->mPlayerId);
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			AAMPLOG_INFO("AampStreamSinkManager(%p) %s Pipeline mode, do not update PLAYER[%d]",
						 this,
						 mPipelineMode == ePIPELINEMODE_UNDEFINED ? "Undefined" : "Multi",
						 aamp->mPlayerId);
		}
		break;
	}
}

void AampStreamSinkManager::AddMediaHeader(unsigned track, std::shared_ptr<AampStreamSinkManager::MediaHeader> header)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	if(track < AAMP_TRACK_COUNT)
	{
		if (mMediaHeaders[track])
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p) media header for track[%u] have already been set; url[%s] mimeType[%s] injected[%d]",
				this, track, mMediaHeaders[track]->url.c_str(), mMediaHeaders[track]->mimeType.c_str(), mMediaHeaders[track]->injected);
		}
		else
		{
			mMediaHeaders[track] = std::move(header);
			AAMPLOG_INFO("AampStreamSinkManager(%p) Added header for track[%u] url[%s] mimeType[%s] injected[%d]",
				this, track, mMediaHeaders[track]->url.c_str(), mMediaHeaders[track]->mimeType.c_str(), mMediaHeaders[track]->injected);
		}
	}
	else
	{
		AAMPLOG_WARN("AampStreamSinkManager(%p) Unable to add media header. track[%u] should be within %d", this, track, AAMP_TRACK_COUNT);
	}
}

void AampStreamSinkManager::RemoveMediaHeader(unsigned track)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);

	if(track < AAMP_TRACK_COUNT)
	{
		mMediaHeaders[track].reset();
		AAMPLOG_INFO("AampStreamSinkManager(%p) Removed header for track[%u]", this, track);
	}
	else
	{
		AAMPLOG_WARN("AampStreamSinkManager(%p)  Unable to remove header! track[%u] should be within %d", this, track, AAMP_TRACK_COUNT);
	}
}

std::shared_ptr<AampStreamSinkManager::MediaHeader> AampStreamSinkManager::GetMediaHeader(unsigned track)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	std::shared_ptr<AampStreamSinkManager::MediaHeader> header = {};

	if(track < AAMP_TRACK_COUNT)
	{
		if (mMediaHeaders[track])
		{
			header = mMediaHeaders[track];
			AAMPLOG_INFO("AampStreamSinkManager(%p) track[%u] url[%s] mimeType[%s] injected[%d]",
				this, track, mMediaHeaders[track]->url.c_str(), mMediaHeaders[track]->mimeType.c_str(), mMediaHeaders[track]->injected);
		}
		else
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p) unable to find MediaHeader for track[%u]", this, track);
		}
	}
	else
	{
		AAMPLOG_WARN("AampStreamSinkManager(%p) track[%u] should be within %d", this, track, AAMP_TRACK_COUNT);
	}

	return header;
}
