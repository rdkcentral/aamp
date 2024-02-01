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
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

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
}

void AampStreamSinkManager::SetSinglePipelineMode(void)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Pipeline mode set to Single", this, __FUNCTION__);
			mPipelineMode = ePIPELINEMODE_SINGLE;

			if (!mEncryptedHeaders.empty())
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Encrypted headers already been set", this, __FUNCTION__);
			}

			// Retain first GstPlayer player, remove others
			if (!mActiveGstPlayersMap.empty())
			{
				auto it = mActiveGstPlayersMap.begin();

				mGstPlayer = it->second;
				it++;

				for (; it != mActiveGstPlayersMap.end();)
				{
					AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Deleting GstPlayer created for PLAYER[%d]", this, __FUNCTION__, it->first->mPlayerId);
					delete(it->second);
					it = mActiveGstPlayersMap.erase(it);
				}
			}
		}
		break;

		case ePIPELINEMODE_SINGLE:
		{
			AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Pipeline mode already set as Single", this, __FUNCTION__);
		}
		break;

		case ePIPELINEMODE_MULTI:
		{
			AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Pipeline mode already set Multi", this, __FUNCTION__);
		}
		break;
	}
}

void AampStreamSinkManager::CreateStreamSink(AampLogManager *logObj, PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
					, std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
#endif
				)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

    AampStreamSinkInactive *inactiveSink = new AampStreamSinkInactive(logObj, id3HandlerCallback);  /* For every instance of aamp, there should be an AampStreamSinkInactive object*/
	mInactiveGstPlayersMap.insert({aamp,inactiveSink});

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mGstPlayer == nullptr)
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, creating GstPlayer for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
				mGstPlayer = new AAMPGstPlayer(logObj, aamp, id3HandlerCallback, exportFrames);
#else
				mGstPlayer = new AAMPGstPlayer(logObj, aamp, id3HandlerCallback);
#endif
				mActiveGstPlayersMap.insert({aamp, mGstPlayer});
			}
			else
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, not creating GstPlayer for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_WARN("AampStreamSinkManager(%p)::%s %s Pipeline mode, creating GstPlayer for PLAYER[%d]", this, __FUNCTION__,
						 mPipelineMode == ePIPELINEMODE_UNDEFINED ? "Undefined" : "Multi", aamp->mPlayerId);
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
			AAMPGstPlayer *gstPlayer = new AAMPGstPlayer(logObj, aamp, id3HandlerCallback, exportFrames);
#else
			AAMPGstPlayer *gstPlayer = new AAMPGstPlayer(logObj, aamp, id3HandlerCallback);
#endif
			mActiveGstPlayersMap.insert({aamp, gstPlayer});
		}
		break;
	}
}

void AampStreamSinkManager::SetStreamSink(PrivateInstanceAAMP *aamp, StreamSink *clientSink)
{
	AAMPLOG_WARN("AampStreamSinkManager(%p)::%s SetStreamSink for PLAYER[%d] clientSink %p", this, __FUNCTION__, aamp->mPlayerId, clientSink);

	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Single Pipeline mode, when setting client StreamSink", this, __FUNCTION__);
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Undefined Pipeline, forcing to Multi Pipeline PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
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
	AAMPLOG_WARN("AampStreamSinkManager(%p)::%s DeleteStreamSink for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);

	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActiveGstPlayersMap.size() &&
				(aamp == mActiveGstPlayersMap.begin()->first))
			{
				/* Erase the map of active player*/
				mActiveGstPlayersMap.erase(aamp);
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s No active players present", this, __FUNCTION__);
			}

			if (mInactiveGstPlayersMap.count(aamp))
			{
				AampStreamSinkInactive* sink = mInactiveGstPlayersMap[aamp];
				mInactiveGstPlayersMap.erase(aamp);
				delete sink;
			}

			if (mInactiveGstPlayersMap.size())
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s %ld Inactive players present", this, __FUNCTION__, mInactiveGstPlayersMap.size());
			}
			else
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s No inactive players present, deleting GStreamer Pipeline PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
				delete(mGstPlayer);
				mGstPlayer = nullptr;
				mPipelineMode = ePIPELINEMODE_UNDEFINED;
				mEncryptedHeadersInjected = false;
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
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Ignore set encrypted headers", this, __FUNCTION__);
		}
		break;
		case ePIPELINEMODE_SINGLE:
		{
			if (!mEncryptedHeaders.empty())
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p)::%s Encrypted headers have already been set PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
			}
			else if (mGstPlayer != nullptr)
			{
				AAMPLOG_INFO("AampStreamSinkManager(%p)::%s Set encrypted player to PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
				mGstPlayer->SetEncryptedAamp(aamp);
				mEncryptedHeaders = mappedHeaders;
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p)::%s No active StreamSink PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
			}
		}
		break;
	}
}

void AampStreamSinkManager::ReinjectEncryptedHeaders()
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	mEncryptedHeadersInjected = false;
}

void AampStreamSinkManager::GetEncryptedHeaders(std::map<int, std::string>& mappedHeaders)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	if (!mEncryptedHeadersInjected)
	{
		mappedHeaders = mEncryptedHeaders;
		mEncryptedHeadersInjected = true;
	}
	else
	{
		AAMPLOG_INFO("AampStreamSinkManager(%p)::%s Encrypted headers already injected", this, __FUNCTION__);
		mappedHeaders.clear();
	}
}

void AampStreamSinkManager::DeactivatePlayer(PrivateInstanceAAMP *aamp, bool stop)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		break;

		case ePIPELINEMODE_SINGLE:
		{
			if (mActiveGstPlayersMap.size() == 0)
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, no current active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
			}
			else if (mActiveGstPlayersMap.begin()->first == aamp)
			{
				if (stop)
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, deactivating and stopping active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
					mEncryptedHeadersInjected = false;
					mEncryptedHeaders.clear();
				}
				else
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, deactivating active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
				}
				mActiveGstPlayersMap.erase(aamp);
			}
			else
			{
				// Can happen when Stop is called after Detach has already been called
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, asked to deactivate PLAYER[%d] when current active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId, mActiveGstPlayersMap.begin()->first->mPlayerId);
			}
		}
		break;
	}
}

void AampStreamSinkManager::ActivatePlayer(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActiveGstPlayersMap.size() == 0)
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, no current active player", this, __FUNCTION__);
			}
			else if (mActiveGstPlayersMap.begin()->first == aamp)
			{
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, already active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
			}
			else
			{
				//Do not edit or remove this log - it is used in L2 test
				AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, resetting current active PLAYER[%d]", this, __FUNCTION__, mActiveGstPlayersMap.begin()->first->mPlayerId);
				mActiveGstPlayersMap.clear();
			}

			if (mActiveGstPlayersMap.size() == 0)
			{
				if (mGstPlayer != nullptr)
				{
					//Do not edit or remove this log - it is used in L2 test
					AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Single Pipeline mode, setting active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);

					mActiveGstPlayersMap.insert({aamp, mGstPlayer});
					SetActive(aamp);
				}
				else
				{
					AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Single Pipeline mode, mGstPlayer is null, can't set active PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
				}
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_WARN("AampStreamSinkManager(%p)::%s Undefined Pipeline, forcing to Multi Pipeline PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
			mPipelineMode = ePIPELINEMODE_MULTI;
		}
		break;

		case ePIPELINEMODE_MULTI:
		{
			//Do not edit or remove this log - it is used in L2 test
			AAMPLOG_INFO("AampStreamSinkManager(%p)::%s Multi Pipeline mode, do nothing PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
		}
		break;
	}
}

void AampStreamSinkManager::SetActive(PrivateInstanceAAMP *aamp)
{
	double position = aamp->GetPositionMs() / 1000.00;

	AAMPLOG_INFO("AampStreamSinkManager(%p)::%s Setting PLAYER[%d] active, position(%f)", this, __FUNCTION__, aamp->mPlayerId, position);

	mGstPlayer->ChangeAamp(aamp, mInactiveGstPlayersMap[aamp]->GetLogManager(), mInactiveGstPlayersMap[aamp]->GetID3MetadataHandler());
	mGstPlayer->Flush(position, aamp->rate, true);
	mGstPlayer->SetSubtitleMute(aamp->subtitles_muted);
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
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	StreamSink *sink_ptr = nullptr;

	switch(mPipelineMode)
	{
		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			if (mClientStreamSinkMap.count(aamp))
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Returning matching client Stream Sink", this, __FUNCTION__);
				sink_ptr = mClientStreamSinkMap[aamp];
			}
			else if (mActiveGstPlayersMap.count(aamp))
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Returning matching Stream Sink", this, __FUNCTION__);
				sink_ptr = mActiveGstPlayersMap[aamp];
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Stream Sink not found", this, __FUNCTION__);
			}
		}
		break;
		case ePIPELINEMODE_SINGLE:
		{
			if (!mActiveGstPlayersMap.empty())
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Returning active Stream Sink found", this, __FUNCTION__);
				sink_ptr = mActiveGstPlayersMap.begin()->second;
			}
			else if (mGstPlayer != nullptr)
			{
				AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s No active Stream Sink found, returning mGstPlayer", this, __FUNCTION__);
				sink_ptr = mGstPlayer;
			}
			else
			{
				AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Active Stream Sink not found", this, __FUNCTION__);
			}
		}
		break;
	}

	return sink_ptr;
}

StreamSink* AampStreamSinkManager::GetStreamSink(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	StreamSink *sink_ptr = nullptr;

	if (mClientStreamSinkMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Returning client Stream Sink found for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
		sink_ptr = mClientStreamSinkMap[aamp];
	}
	else if (mActiveGstPlayersMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Returning active Stream Sink found for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
		sink_ptr = mActiveGstPlayersMap[aamp];
	}
	else if (mInactiveGstPlayersMap.count(aamp) != 0)
	{
		AAMPLOG_TRACE("AampStreamSinkManager(%p)::%s Returning inactive Stream Sink found or PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
		sink_ptr = mInactiveGstPlayersMap[aamp];
	}
	else
	{
		// If not found, best not to dereference the pointer in case invalid
		AAMPLOG_ERR("AampStreamSinkManager(%p)::%s Stream Sink for aamp(%p) not found", this, __FUNCTION__, aamp);
	}

	return sink_ptr;
}

StreamSink *AampStreamSinkManager::GetStoppingStreamSink(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	StreamSink *sink_ptr = nullptr;

	if ((mPipelineMode == ePIPELINEMODE_SINGLE) && mActiveGstPlayersMap.empty())
	{
		AAMPLOG_WARN("AampStreamSinkManager(%p)::%s No active player, returning single-pipeline sink for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
		sink_ptr = mGstPlayer;
	}
	else
	{
		AAMPLOG_INFO("AampStreamSinkManager(%p)::%s Getting stream sink for PLAYER[%d]", this, __FUNCTION__, aamp->mPlayerId);
		sink_ptr = GetStreamSink(aamp);
	}

	return sink_ptr;
}

void AampStreamSinkManager::UpdateTuningPlayer(PrivateInstanceAAMP *aamp)
{
	std::lock_guard<std::recursive_mutex> lock(mStreamSinkMutex);

	switch (mPipelineMode)
	{
		case ePIPELINEMODE_SINGLE:
		{
			if (mActiveGstPlayersMap.empty())
			{
				if (mGstPlayer == nullptr)
				{
					AAMPLOG_ERR(
						"AampStreamSinkManager(%p)::%s No single pipeline stream sink PLAYER[%d]",
						this, __FUNCTION__, aamp->mPlayerId);
				}
				else if (mInactiveGstPlayersMap.count(aamp) == 0)
				{
					AAMPLOG_ERR(
						"AampStreamSinkManager(%p)::%s No inactive stream sink for PLAYER[%d]",
						this, __FUNCTION__, aamp->mPlayerId);
				}
				else
				{
					AAMPLOG_WARN(
						"AampStreamSinkManager(%p)::%s Single pipeline stream sink with no active players, update player to PLAYER[%d]",
						this, __FUNCTION__, aamp->mPlayerId);

					mGstPlayer->ChangeAamp(aamp, mInactiveGstPlayersMap[aamp]->GetLogManager(),
										   mInactiveGstPlayersMap[aamp]->GetID3MetadataHandler());
				}
			}
			else
			{
				AAMPLOG_INFO(
					"AampStreamSinkManager(%p)::%s Active stream sink exists, do not update PLAYER[%d]",
					this, __FUNCTION__, aamp->mPlayerId);
			}
		}
		break;

		case ePIPELINEMODE_UNDEFINED:
		case ePIPELINEMODE_MULTI:
		{
			AAMPLOG_INFO("AampStreamSinkManager(%p)::%s %s Pipeline mode, do not update PLAYER[%d]",
						 this, __FUNCTION__,
						 mPipelineMode == ePIPELINEMODE_UNDEFINED ? "Undefined" : "Multi",
						 aamp->mPlayerId);
		}
		break;
	}
}
