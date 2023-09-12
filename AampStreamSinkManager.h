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

class PrivateInstanceAAMP;

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
	 */
	virtual void SetSinglePipelineMode(void);
	/**
	 *  @fn CreateStreamSink
	 *  @brief Creates the StreamSink that will be associated with the instance of PrivateInstanceAAMP passed
	 *  @param[in] logObj - the log manager associated with the instance of PrivateInstanceAAMP
	 *  @param[in] aamp - the instance of PrivateInstanceAAMP
	 *  @param[in] id3HandlerCallback - the id3 handler callback associated with the instance of PrivateInstanceAAMP
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
	 *  @param[in] exportFrames -
#endif
	 */
	virtual void CreateStreamSink(AampLogManager *logObj, PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback
#ifdef RENDER_FRAMES_IN_APP_CONTEXT
					  , std::function< void(uint8_t *, int, int, int) > exportFrames = nullptr
#endif
					);

	/**
	 *  @fn SetStreamSink
	 *  @brief Sets a client supplied StreamSink and associates it with the PrivateInstanceAAMP passed, also sets pipeline mode to multi
	 *  @param[in] aamp - the instance of PrivateInstanceAAMP
	 *  @param[in] clientSink - the client supplied StreamSink
	 */
	virtual void SetStreamSink(PrivateInstanceAAMP *aamp, StreamSink *clientSink);

	/**
	 *  @fn DeleteStreamSink
	 *  @brief Deletes the StreamSink associated with the instance of PrivateInstanceAAMP passed
	 *  @param[in] aamp - the instance of PrivateInstanceAAMP
	 */
	virtual void DeleteStreamSink(PrivateInstanceAAMP *aamp);
	/**
	 *  @fn SetEncryptedHeaders
	 *  @brief Store the mpd init headers collected from the encrypted asset
	 *  @param[in] aamp - the PrivateInstanceAAMP that has the encrypted init headers
	 *  @param[in] mappedHeaders - the encrypted headers, Mediatype mapped to url
	 */
	virtual void SetEncryptedHeaders(PrivateInstanceAAMP *aamp, std::map<int, std::string>& mappedHeaders);
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
	 *  @param[in] aamp - the PrivateInstanceAAMP, that is to be removed from active players map
	 */
	virtual void DeactivatePlayer(PrivateInstanceAAMP *aamp, bool stop);
	/**
	 *  @fn ActivatePlayer
	 *  @brief Performs action to activate an instance of PrivateInstanceAAMP
	 *  @param[in] aamp - the PrivateInstanceAAMP, that is to be made active
	 */
	virtual void ActivatePlayer(PrivateInstanceAAMP *aamp);
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
	 *  @fn GetStreamSink
	 *  @brief Gets the active StreamSink pointer; for single pipeline mode this is the main StreamSink pointer,
	 * 	for multipipeline this is the StreamSink that matches the passed PrivateInstanceAAMP. If no Sink found, nullptr is returned.
	 *  @param[in] aamp - the PrivateInstanceAAMP, the active stream sink of which is required (for multipipeline)
	 *  @param[out] - return Streamsink from active map if present, nullptr if not
	 */
	virtual StreamSink* GetActiveStreamSink(PrivateInstanceAAMP *aamp);
	/**
	 *  @fn GetStreamSink
	 *  @brief Gets a StreamSink pointer for the matching PrivateInstanceAAMP. If no Sink found, nullptr is returned.
	 *  @param[in] aamp - the PrivateInstanceAAMP, the stream sink of which is required
	 *  @param[out] - return Streamsink from active map if present, otherwise from the map of inactive sink, otherwsie nullptr
	 */
	virtual StreamSink* GetStreamSink(PrivateInstanceAAMP *aamp);

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
	 *  @brief Makes an instance of PrivateInstanceAAMP as the active i.e. its data fed into Gstreamer pipeline
	 *  @param[in] aamp - the PrivateInstanceAAMP, data of which will be fed into Gstreamer pipeline
	 */
	void SetActive(PrivateInstanceAAMP *aamp);

	AAMPGstPlayer *mGstPlayer;

	std::map<PrivateInstanceAAMP*, StreamSink*> mClientStreamSinkMap;						/**< To maintain information on client supplied StreamSink for PrivateInstanceAAMP */
	std::map<PrivateInstanceAAMP*, AAMPGstPlayer*> mActiveGstPlayersMap;					/**< To maintain information on currently active PrivateInstanceAAMP */
	std::map<PrivateInstanceAAMP*, AampStreamSinkInactive*> mInactiveGstPlayersMap;			/**< To maintain information on currently inactive PrivateInstanceAAMP*/
	std::map<int, std::string> mEncryptedHeaders;

	PipelineMode mPipelineMode;

	std::recursive_mutex mStreamSinkMutex;

	PrivateInstanceAAMP *mEncryptedAamp;
	bool mEncryptedHeadersInjected;
};

#endif /* AAMPSTREAMSINKMANAGER_H */
