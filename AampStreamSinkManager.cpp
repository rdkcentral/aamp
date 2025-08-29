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
 * @brief manages stream sink of gstreamer
 */

#include "AampStreamSinkManager.h"
#include "priv_aamp.h"

AampStreamSinkManager::AampStreamSinkManager() :
	mGstPlayer(nullptr),
	mClientStreamSinkMap(),
	mActiveGstPlayersMap(),
	mInactiveGstPlayersMap(),
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
	for (auto it = mInactiveGstPlayersMap.begin(); it != mInactiveGstPlayersMap.end();)
	{
		delete(it->second);
		it = mInactiveGstPlayersMap.erase(it);
	}
	if (mActiveGstPlayersMap.size())
	{
		for (auto it = mActiveGstPlayersMap.begin(); it != mActiveGstPlayersMap.end();)
		{
			delete(it->second);
			it = mActiveGstPlayersMap.erase(it);
		}
		mGstPlayer = nullptr;
	}
	else
	{
		if (mGstPlayer)
		{
			delete (mGstPlayer);
			mGstPlayer = nullptr;
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

			// Retain matching GstPlayer player, remove others
			for (auto it = mActiveGstPlayersMap.begin(); it != mActiveGstPlayersMap.end();)
			{
				if (aamp == it->first)
				{
					AAMPLOG_WARN("AampStreamSinkManager(%p) Retaining GstPlayer created for PLAYER[%d]", this, it->first->mPlayerId);
					mGstPlayer = it->second;
					it++;
				}
				else
				{
					AAMPLOG_WARN("AampStreamSinkManager(%p) Deleting GstPlayer created for PLAYER[%d]", this, it->first->mPlayerId);
					delete(it->second);
					it = mActiveGstPlayersMap.erase(it);
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

void AampStreamSinkManager::CreateStreamSink(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	AampStreamSinkInactive *inactiveSink = new AampStreamSinkInactive(id3HandlerCallback);  /* For every instance of aamp, there should be an AampStreamSinkInactive object*/
	mInactiveGstPlayersMap.insert({aamp,inactiveSink});

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mGstPlayer == nullptr)
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, creating GstPlayer for PLAYER[%d]", this, aamp->mPlayerId);
				mGstPlayer = new AAMPGstPlayer(aamp, id3HandlerCallback, exportFrames);
				mActiveGstPlayersMap.insert({aamp, mGstPlayer});
			}
			else
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, not creating GstPlayer for PLAYER[%d]", this, aamp->mPlayerId);
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_WARN("AampStreamSinkManager(%p) %s Pipeline mode, creating GstPlayer for PLAYER[%d]", this,
						 mPipelineMode == ePIPELINEMODE_UNDEFINED ? "Undefined" : "Multi", aamp->mPlayerId);
			AAMPGstPlayer *gstPlayer = new AAMPGstPlayer(aamp, id3HandlerCallback, exportFrames);
			mActiveGstPlayersMap.insert({aamp, gstPlayer});
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
			if (mActiveGstPlayersMap.size() &&
				(aamp == mActiveGstPlayersMap.begin()->first))
			{
				/* Erase the map of active player*/
				mActiveGstPlayersMap.erase(aamp);
				AAMPLOG_WARN("AampStreamSinkManager(%p) No active players present", this );
			}

			if (mInactiveGstPlayersMap.count(aamp))
			{
				AampStreamSinkInactive* sink = mInactiveGstPlayersMap[aamp];
				mInactiveGstPlayersMap.erase(aamp);
				delete sink;
			}

			if (mInactiveGstPlayersMap.size())
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) %zu Inactive players present", this, mInactiveGstPlayersMap.size());

				// check the sink was not attached to the player that is being deleted
				if (mGstPlayer->IsAssociatedAamp(aamp))
				{
					if (mActiveGstPlayersMap.size() == 0)
					{
						// attach it to one of the existing inactive players
						AAMPLOG_WARN("AampStreamSinkManager(%p) Deleting player associated with sink! Attaching sink to default inactive PLAYER[%d]", this, mInactiveGstPlayersMap.begin()->first->mPlayerId);
						mGstPlayer->ChangeAamp(mInactiveGstPlayersMap.begin()->first,
												mInactiveGstPlayersMap.begin()->second->GetID3MetadataHandler());
					}
				}
			}
			else
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) No inactive players present, deleting GStreamer Pipeline PLAYER[%d]", this, aamp->mPlayerId);
				delete(mGstPlayer);
				mGstPlayer = nullptr;
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
			if (mInactiveGstPlayersMap.count(aamp))
			{
				AampStreamSinkInactive* sink = mInactiveGstPlayersMap[aamp];
				mInactiveGstPlayersMap.erase(aamp);
				delete(sink);
			}

			if (mActiveGstPlayersMap.count(aamp))
			{
				AAMPGstPlayer* sink = mActiveGstPlayersMap[aamp];
				mActiveGstPlayersMap.erase(aamp);
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
			if (!mEncryptedHeaders.empty())
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p) Encrypted headers have already been set PLAYER[%d]", this, aamp->mPlayerId);
			}
			else if (mGstPlayer != nullptr)
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p) Set encrypted player to PLAYER[%d]", this, aamp->mPlayerId);
				mGstPlayer->SetEncryptedAamp(aamp);
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
			if (mActiveGstPlayersMap.size() == 0)
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, no current active PLAYER[%d]", this, aamp->mPlayerId);
			}
			else if (mActiveGstPlayersMap.begin()->first == aamp)
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
				mActiveGstPlayersMap.erase(aamp);
			}
			else
			{
				// Can happen when Stop is called after Detach has already been called
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, asked to deactivate PLAYER[%d] when current active PLAYER[%d]", this, aamp->mPlayerId, mActiveGstPlayersMap.begin()->first->mPlayerId);
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

	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	
	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActiveGstPlayersMap.size() == 0)
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, no current active player", this );
			}
			else if (mActiveGstPlayersMap.begin()->first == aamp)
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, already active PLAYER[%d]", this, aamp->mPlayerId);
			}
			else
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, resetting current active PLAYER[%d]", this, mActiveGstPlayersMap.begin()->first->mPlayerId);
				mActiveGstPlayersMap.clear();
			}

			if (mActiveGstPlayersMap.size() == 0)
			{
				if (mGstPlayer != nullptr)
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p) Single Pipeline mode, setting active PLAYER[%d]", this, aamp->mPlayerId);

					mActiveGstPlayersMap.insert({aamp, mGstPlayer});
					SetActive(aamp, position);
				}
				else
				{
					AAMPLOG_ERR("AampStreamSinkManager(%p) Single Pipeline mode, mGstPlayer is null, can't set active PLAYER[%d]", this, aamp->mPlayerId);
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
	AAMPLOG_INFO("AampStreamSinkManager(%p) Setting PLAYER[%d] active, position(%f)", this, aamp->mPlayerId, position);

	mGstPlayer->ChangeAamp(aamp, mInactiveGstPlayersMap[aamp]->GetID3MetadataHandler());
	aamp->mIsFlushOperationInProgress = true;
	mGstPlayer->Flush(position, aamp->rate, true);
	aamp->mIsFlushOperationInProgress = false;
	mGstPlayer->SetSubtitleMute(aamp->subtitles_muted);
	if(!aamp->IsTuneCompleted() && aamp->IsPlayEnabled() && (mPipelineMode == ePIPELINEMODE_SINGLE))
	{
		mGstPlayer->ResetFirstFrame();
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
			else if (mActiveGstPlayersMap.count(aamp))
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning matching Stream Sink", this );
				sink_ptr = mActiveGstPlayersMap[aamp];
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p) Stream Sink not found", this );
			}
		}
		break;
		case ePIPELINEMODE_SINGLE:
		{
			if (!mActiveGstPlayersMap.empty())
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning active Stream Sink found", this );
				sink_ptr = mActiveGstPlayersMap.begin()->second;
			}
			else if (mGstPlayer != nullptr)
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p) No active Stream Sink found, returning mGstPlayer", this );
				sink_ptr = mGstPlayer;
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
	else if (mActiveGstPlayersMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning active Stream Sink found for PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = mActiveGstPlayersMap[aamp];
	}
	else if (mInactiveGstPlayersMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p) Returning inactive Stream Sink found or PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = mInactiveGstPlayersMap[aamp];
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

	if ((mPipelineMode == ePIPELINEMODE_SINGLE) && mActiveGstPlayersMap.empty())
	{
		AAMPLOG_WARN("AampStreamSinkManager(%p) No active player, returning single-pipeline sink for PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = mGstPlayer;
	}
	else
	{
		AAMPLOG_INFO("AampStreamSinkManager(%p) Getting stream sink for PLAYER[%d]", this, aamp->mPlayerId);
		sink_ptr = GetStreamSinkNoLock(aamp);
	}

	return sink_ptr;
}

void AampStreamSinkManager::UpdateTuningPlayer(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::mutex> lock(mStreamSinkMutex);
	switch (mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActiveGstPlayersMap.empty())
			{
				if (mGstPlayer == nullptr)
				{
					AAMPLOG_ERR(
						"AampStreamSinkManager(%p) No single pipeline stream sink PLAYER[%d]",
						this, aamp->mPlayerId);
				}
				else if (mInactiveGstPlayersMap.count(aamp) == 0)
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

					mGstPlayer->ChangeAamp(aamp,
										   mInactiveGstPlayersMap[aamp]->GetID3MetadataHandler());
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
