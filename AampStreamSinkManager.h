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

#include <vector>
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

	/**
	 * @class MediaHeader
	 * @brief  Class containing MediaHeader data to be cached from the main asset
	 */
	class MediaHeader {
	public:
		std::string url;           /**< url of the media */
		std::string mimeType;      /**< mime type of the media */
		bool injected;             /**< indicates if the media header has been injected */

		MediaHeader() = default;
		MediaHeader(const std::string& url_, const std::string& mimeType_)
			: url(url_), mimeType(mimeType_), injected(false) {}
	};

	virtual ~AampStreamSinkManager();
	/**
	 *  @fn SetSinglePipelineMode
	 *  @brief Sets the GStreamer pipeline mode to single.
	 *  @param[in] aamp - the PrivateInstanceAAMP, the player that is activating single pipeline
	 */
	virtual void SetSinglePipelineMode(PrivateInstanceAAMP *aamp);
	/**
	 *  @fn CreateStreamSink
	 *  @brief Creates the StreamSink that will be associated with the instance of PrivateInstanceAAMP passed
	 *  @param[in] aamp - the instance of PrivateInstanceAAMP
	 *  @param[in] id3HandlerCallback - the id3 handler callback associated with the instance of PrivateInstanceAAMP
	 *  @param[in] exportFrames -
	 */
	virtual void CreateStreamSink(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function< void(const unsigned char *, int, int, int) > exportFrames = nullptr);

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
	 *  @fn GetActiveStreamSink
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
	 *  @param[out] - return Streamsink from active map if present, otherwise from the map of inactive sink, otherwise nullptr
	 */
	virtual StreamSink* GetStreamSink(PrivateInstanceAAMP *aamp);
	/**
	 *  @fn GetStoppingStreamSink
	 *  @brief Gets the stream sink to stop for the given PrivateInstanceAAMP. In single-pipeline mode,
	 * 		   if there are no active stream sinks, then the single pipeline stream sink will be returned.
	 *  @param[in] aamp - the PrivateInstanceAAMP that represents the player being stopped
	 *  @param[out] - return the stream sink to stop - either the single pipeline stream sink,
	 * 				  or the stream sink associated with the given player (may be nullptr if couldn't be found)
	 */
	virtual StreamSink* GetStoppingStreamSink(PrivateInstanceAAMP *aamp);
	/**
	 *  @fn UpdateTuningPlayer
	 *  @brief Updates the player associated with the single pipeline stream sink, if there are
	 * 		   currently no active players already using the single pipeline.
	 *         Has no effect if not in single pipeline mode, or if there is already a player active
	 *         (which will be the case if the client is pre-loading an asset for smooth ad transition).
	 *  @param[in] aamp - the PrivateInstanceAAMP that represents the player being tuned
	 */
	virtual void UpdateTuningPlayer(PrivateInstanceAAMP *aamp);
	/**
	 *  @fn AddMediaHeader
	 *  @brief Store the media init headers collected from the main VOD asset
	 *  @param[in] track - the media(subtitle,video or audio) for which the headers to be saved
	 *  @param[in] header - contains the init url and mimeType of the media
	 */
	virtual void AddMediaHeader(unsigned track, std::shared_ptr<MediaHeader> header);
	/**
	 *  @fn RemoveMediaHeader
	 *  @brief Removes the media init headers collected from the main VOD asset
	 *  @param[in] track - the media(subtitle,video or audio) for which the headers to be removed
	 */
	virtual void RemoveMediaHeader(unsigned track);
	/**
	 *  @fn GetMediaHeader
	 *  @brief Returns the media init headers collected from the main VOD asset
	 *  @param[in] track - the media(subtitle,video or audio) for which the headers to be retrieved
	 */
	virtual std::shared_ptr<MediaHeader> GetMediaHeader(unsigned track);

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
	 *  @param[in] position - the current playback position for the player being activated
	 */
	void SetActive(PrivateInstanceAAMP *aamp, double position);
	/**
	 *  @fn GetStreamSinkNoLock
	 *  @brief Gets a StreamSink pointer for the matching PrivateInstanceAAMP,
	 *         but without locking the StreamSink mutex. \ref GetStreamSink for details.
	 */
	StreamSink* GetStreamSinkNoLock(PrivateInstanceAAMP *aamp);

	AAMPGstPlayer *mGstPlayer;

	std::map<PrivateInstanceAAMP*, StreamSink*> mClientStreamSinkMap;						/**< To maintain information on client supplied StreamSink for PrivateInstanceAAMP */
	std::map<PrivateInstanceAAMP*, AAMPGstPlayer*> mActiveGstPlayersMap;					/**< To maintain information on currently active PrivateInstanceAAMP */
	std::map<PrivateInstanceAAMP*, AampStreamSinkInactive*> mInactiveGstPlayersMap;			/**< To maintain information on currently inactive PrivateInstanceAAMP*/
	std::map<int, std::string> mEncryptedHeaders;

	std::vector<std::shared_ptr<MediaHeader> > mMediaHeaders;

	PipelineMode mPipelineMode;

	std::mutex mStreamSinkMutex;

	PrivateInstanceAAMP *mEncryptedAamp;
	bool mEncryptedHeadersInjected;

};

#endif /* AAMPSTREAMSINKMANAGER_H */
