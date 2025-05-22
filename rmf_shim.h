/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
 * @file rmf_shim.h
 * @brief shim for dispatching UVE RMF playback
 */

#ifndef RMF_SHIM_H_
#define RMF_SHIM_H_

#include "StreamAbstractionAAMP.h"
#include <string>
#include <stdint.h>
#include "PlayerThunderInterface.h"
using namespace std;

/**
 * @struct RMFSettings
 * @brief Structure to save the ATSC settings
 */
typedef struct RMFSettings
{
	std::string preferredLanguages;
	RMFSettings(): preferredLanguages() { };
}RMFGlobalSettings;

/**
 * @class StreamAbstractionAAMP_RMF
 * @brief Fragment collector for RMF
 */
class StreamAbstractionAAMP_RMF : public StreamAbstractionAAMP
{
	public:
	/**
	 * @fn StreamAbstractionAAMP_RMF
	 * @param aamp pointer to PrivateInstanceAAMP object associated with player
	 * @param seek_pos Seek position
	 * @param rate playback rate
	 */
	StreamAbstractionAAMP_RMF(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
	/**
	 * @fn ~StreamAbstractionAAMP_RMF
	 */
	~StreamAbstractionAAMP_RMF();
	/**
	 * @brief Copy constructor disabled
	 *
	 */
	StreamAbstractionAAMP_RMF(const StreamAbstractionAAMP_RMF&) = delete;
	/**
	 * @brief assignment operator disabled
	 *
	 */
	StreamAbstractionAAMP_RMF& operator=(const StreamAbstractionAAMP_RMF&) = delete;

	/*Event Handlers*/
	void onPlayerStatusHandler(std::string title);
	void onPlayerErrorHandler(std::string err_msg);
	/**
	 *   @fn Start
	 */
	void Start() override;
	/**
	 *   @fn Stop
	 */
	void Stop(bool clearChannelData) override;
	/**
	 *   @fn Init
	 *   @note   To be implemented by sub classes
	 *   @param  tuneType to set type of object.
	 *   @retval true on success
	 *   @retval false on failure
	 */
	AAMPStatusType Init(TuneType tuneType) override;
	/**
	 * @fn GetStreamFormat
	 * @param[out]  primaryOutputFormat - format of primary track
	 * @param[out]  audioOutputFormat - format of audio track
	 * @param[out]  auxOutputFormat - format of aux audio track
	 * @param[out]  subtitleOutputFormat - format of subtitle track
	 */
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subtitleOutputFormat) override;
	/**
	 *   @fn GetStreamPosition
	 *
	 *   @retval current position of stream.
	 */
	double GetStreamPosition() override;
	/**
	 *   @fn GetMediaTrack
	 *
	 *   @param[in]  type - track type
	 *   @retval MediaTrack pointer.
	 */
	MediaTrack* GetMediaTrack(TrackType type) override;
	/**
	 *   @fn GetFirstPTS
	 *
	 *   @retval PTS of first sample
	 */
	double GetFirstPTS() override;
	/**
	 *   @fn GetStartTimeOfFirstPTS
	 *
	 *   @retval start time of first sample
	 */
	double GetStartTimeOfFirstPTS() override;
	/**
	 * @fn SetVideoRectangle
	 *
	 * @param[in] x,y - position coordinates of video rectangle
	 * @param[in] wxh - width & height of video rectangle
	 */
	void SetVideoRectangle(int x, int y, int w, int h) override;
	/**
	 * @fn SetAudioTrack
	 *
	 * @param[in] Index of the audio track.
	 */
	void SetAudioTrack(int index) override;
	/**
	 * @fn SetAudioTrackByLanguage
	 *
	 * @param[in] lang : Audio Language to be set
	 */
	void SetAudioTrackByLanguage(const char* lang) override;
	/**
	 *   @fn GetAvailableAudioTracks
	 *
	 *   @return std::vector<AudioTrackInfo> List of available audio tracks
	 */
	std::vector<AudioTrackInfo> &GetAvailableAudioTracks(bool allTrack=false) override;
	/**
	 *   @fn GetAudioTrack
	 *
	 *   @return int - index of current audio track
	 */
	int GetAudioTrack() override;
	/**
	 *   @fn GetCurrentAudioTrack
	 *   @return int - index of current audio track
	 */
	bool GetCurrentAudioTrack(AudioTrackInfo &audioTrack) override;
	/**
	 *   @fn GetAvailableTextTracks
	 *   @return std::vector<TextTrackInfo> List of available text tracks
	 */
	std::vector<TextTrackInfo> &GetAvailableTextTracks(bool all=false) override;
	/**
	 * @fn SetPreferredAudioLanguages
	 *
	 */
	void SetPreferredAudioLanguages() override;
	/**
	 * @fn DisableContentRestrictions
	 *
	 * @param[in] grace - seconds from current time, grace period, grace = -1 will allow an unlimited grace period
	 * @param[in] time - seconds from current time,time till which the channel need to be kept unlocked
	 * @param[in] eventChange - disable restriction handling till next program event boundary
	 */
	void DisableContentRestrictions(long grace, long time, bool eventChange) override;
	/**
	 * @fn EnableContentRestrictions
	 *
	 */
	void EnableContentRestrictions() override;

	/**
	 * @fn UpdateFailedDRMStatus
	 * @brief Function to update the failed DRM status to mark the adaptation sets to be omitted
	 * @param[in] object  - Prefetch object instance which failed
	 */
	void UpdateFailedDRMStatus(LicensePreFetchObject *object) override { }
	private:
	PlayerThunderInterface thunderAccessObj;
	bool tuned;

	/**
	 * @fn GetAudioTracks
	 * @return void
	 */
	void GetAudioTracks();
	/**
	 * @fn GetAudioTrackInternal
	 *
	 */
	int GetAudioTrackInternal();

	/**
	 * @fn GetTextTracks
	 * @return voi @return void
	 */
	void GetTextTracks();

	protected:
	/**
	 *   @fn GetStreamInfo
	 *
	 *   @param[in]  idx - profile index.
	 *   @retval stream information corresponding to index.
	 */
	StreamInfo* GetStreamInfo(int idx) override;
};
#endif //RMF_SHIM_H_
/**
 * @}
 */



