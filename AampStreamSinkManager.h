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
 * @file AampStreamSinkmanager.h
 * @brief manages stream sink of gstreamer
 */

#ifndef AAMPSTREAMSINKMANAGER_H
#define AAMPSTREAMSINKMANAGER_H

#include <stddef.h>
#include "aampgstplayer.h"
#include "AampStreamSinkInactive.h"

class PlayerInstanceAAMP;

/**
 * @class AampStreamSinkManager
 * @brief Class declaration that manages stream sink of gstreamer
 */
class AampStreamSinkManager
{
public:

	virtual ~AampStreamSinkManager();
	/**
	 *  @fn SetSinglePipelineMode
	 *  @brief Sets the GStreamer pipeline mode to single.
	 *  @param[in] aamp - the PlayerInstanceAAMP, the player that is activating single pipeline
	 */
	virtual void SetSinglePipelineMode(PlayerInstanceAAMP *aamp);
	/**
	 *  @fn CreateStreamSink
	 *  @brief Creates the StreamSink that will be associated with the instance of PlayerInstanceAAMP passed
	 *  @param[in] aamp - the instance of PlayerInstanceAAMP
	 *  @param[in] id3HandlerCallback - the id3 handler callback associated with the instance of PlayerInstanceAAMP
	 *  @param[in] exportFrames -
	 */
	virtual void CreateStreamSink(PlayerInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames = nullptr);

	/**
	 *  @fn SetStreamSink
	 *  @brief Sets a client supplied StreamSink and associates it with the PlayerInstanceAAMP passed, also sets pipeline mode to multi
	 *  @param[in] aamp - the instance of PlayerInstanceAAMP
	 *  @param[in] clientSink - the client supplied StreamSink
	 */
	virtual void SetStreamSink(PlayerInstanceAAMP *aamp, StreamSink *clientSink);

	/**
	 *  @fn DeleteStreamSink
	 *  @brief Deletes the StreamSink associated with the instance of PlayerInstanceAAMP passed
	 *  @param[in] aamp - the instance of PlayerInstanceAAMP
	 */
	virtual void DeleteStreamSink(PlayerInstanceAAMP *aamp);
	/**
	 *  @fn SetEncryptedHeaders
	 *  @brief Store the mpd init headers collected from the encrypted asset
	 *  @param[in] aamp - the PlayerInstanceAAMP that has the encrypted init headers
	 *  @param[in] mappedHeaders - the encrypted headers, Mediatype mapped to url
	 */
	virtual void SetEncryptedHeaders(PlayerInstanceAAMP *aamp, std::map<int, std::string>& mappedHeaders);
	/**
	 *  @fn GetEncryptedHeaders
	 *  @brief Gets the mpd init headers collected from the encrypted asset and sets the mEncryptedHeadersInjected flag.
	 * 			Further calls will not get the init headers until mEncryptedHeadersInjected flag is cleared
	 *  @param[in] mappedHeaders - the encrypted headers, Mediatype mapped to url
	 */
	virtual void GetEncryptedHeaders(std::map<int, std::string>& mappedHeaders);
	/**
	 *  @fn ReinjectEncryptedHeaders
	 *  @brief Clears the mEncryptedHeadersInjected flag so that GetEncryptedHeaders returns the headers on next call
	 */
	virtual void ReinjectEncryptedHeaders();
	/**
	 *  @fn DeactivatePlayer
	 *  @brief Removes the entry from active players map
	 *  @param[in] aamp - the PlayerInstanceAAMP, that is to be removed from active players map
	 */
	virtual void DeactivatePlayer(PlayerInstanceAAMP *aamp, bool stop);
	/**
	 *  @fn ActivatePlayer
	 *  @brief Performs action to activate an instance of PlayerInstanceAAMP
	 *  @param[in] aamp - the PlayerInstanceAAMP, that is to be made active
	 */
	virtual void ActivatePlayer(PlayerInstanceAAMP *aamp);
	/**
	 * @brief Creates a singleton instance of AampStreamSinkManager
	 */
	static AampStreamSinkManager& GetInstance();
	/**
	 *  @fn Clear
	 *  @brief Clear the StreamSinkManager instance of all created StreamSink
	 */
	void Clear();
	/**
	 *  @fn GetActiveStreamSink
	 *  @brief Gets the active StreamSink pointer; for single pipeline mode this is the main StreamSink pointer,
	 * 	for multipipeline this is the StreamSink that matches the passed PlayerInstanceAAMP. If no Sink found, nullptr is returned.
	 *  @param[in] aamp - the PlayerInstanceAAMP, the active stream sink of which is required (for multipipeline)
	 *  @param[out] - return Streamsink from active map if present, nullptr if not
	 */
	virtual StreamSink* GetActiveStreamSink(PlayerInstanceAAMP *aamp);
	/**
	 *  @fn GetStreamSink
	 *  @brief Gets a StreamSink pointer for the matching PlayerInstanceAAMP. If no Sink found, nullptr is returned.
	 *  @param[in] aamp - the PlayerInstanceAAMP, the stream sink of which is required
	 *  @param[out] - return Streamsink from active map if present, otherwise from the map of inactive sink, otherwise nullptr
	 */
	virtual StreamSink* GetStreamSink(PlayerInstanceAAMP *aamp);
	/**
	 *  @fn GetStoppingStreamSink
	 *  @brief Gets the stream sink to stop for the given PlayerInstanceAAMP. In single-pipeline mode,
	 * 		   if there are no active stream sinks, then the single pipeline stream sink will be returned.
	 *  @param[in] aamp - the PlayerInstanceAAMP that represents the player being stopped
	 *  @param[out] - return the stream sink to stop - either the single pipeline stream sink,
	 * 				  or the stream sink associated with the given player (may be nullptr if couldn't be found)
	 */
	virtual StreamSink* GetStoppingStreamSink(PlayerInstanceAAMP *aamp);
	/**
	 *  @fn UpdateTuningPlayer
	 *  @brief Updates the player associated with the single pipeline stream sink, if there are
	 * 		   currently no active players already using the single pipeline.
	 *         Has no effect if not in single pipeline mode, or if there is already a player active
	 *         (which will be the case if the client is pre-loading an asset for smooth ad transition).
	 *  @param[in] aamp - the PlayerInstanceAAMP that represents the player being tuned
	 */
	virtual void UpdateTuningPlayer(PlayerInstanceAAMP *aamp);

protected:

	AampStreamSinkManager();

private:


	enum PipelineMode
	{
		ePIPELINEMODE_UNDEFINED,
		ePIPELINEMODE_SINGLE,
		ePIPELINEMODE_MULTI,
	};

	/**
	 *  @fn SetActive
	 *  @brief Makes an instance of PlayerInstanceAAMP as the active i.e. its data fed into Gstreamer pipeline
	 *  @param[in] aamp - the PlayerInstanceAAMP, data of which will be fed into Gstreamer pipeline
	 *  @param[in] position - the current playback position for the player being activated
	 */
	void SetActive(PlayerInstanceAAMP *aamp, double position);
	/**
	 *  @fn GetStreamSinkNoLock
	 *  @brief Gets a StreamSink pointer for the matching PlayerInstanceAAMP,
	 *         but without locking the StreamSink mutex. \ref GetStreamSink for details.
	 */
	StreamSink* GetStreamSinkNoLock(PlayerInstanceAAMP *aamp);

	AAMPGstPlayer *mGstPlayer;

	std::map<PlayerInstanceAAMP*, StreamSink*> mClientStreamSinkMap;						/**< To maintain information on client supplied StreamSink for PlayerInstanceAAMP */
	std::map<PlayerInstanceAAMP*, AAMPGstPlayer*> mActiveGstPlayersMap;					/**< To maintain information on currently active PlayerInstanceAAMP */
	std::map<PlayerInstanceAAMP*, AampStreamSinkInactive*> mInactiveGstPlayersMap;			/**< To maintain information on currently inactive PlayerInstanceAAMP*/
	std::map<int, std::string> mEncryptedHeaders;

	PipelineMode mPipelineMode;

	std::mutex mStreamSinkMutex;

	PlayerInstanceAAMP *mEncryptedAamp;
	bool mEncryptedHeadersInjected;
};

#endif /* AAMPSTREAMSINKMANAGER_H */
