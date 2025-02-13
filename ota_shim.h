/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file ota_shim.h
 * @brief shim for dispatching UVE OTA ATSC playback
 */

#ifndef OTA_SHIM_H_
#define OTA_SHIM_H_

#include "StreamAbstractionAAMP.h"
#include <string>
#include <stdint.h>
#include "PlayerThunderInterface.h"
using namespace std;

/**
 * @struct ATSCSettings
 * @brief Structure to save the ATSC settings
 */
typedef struct ATSCSettings
{
	std::string preferredLanguages;
	std::string preferredRendition;

	ATSCSettings(): preferredLanguages(), preferredRendition() { };
}ATSCGlobalSettings;

/**
 * @class StreamAbstractionAAMP_OTA
 * @brief Fragment collector for OTA
 */
class StreamAbstractionAAMP_OTA : public StreamAbstractionAAMP
{
public:
    /**
     * @fn StreamAbstractionAAMP_OTA
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seek_pos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @fn ~StreamAbstractionAAMP_OTA
     */
    ~StreamAbstractionAAMP_OTA();
    /**
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_OTA(const StreamAbstractionAAMP_OTA&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_OTA& operator=(const StreamAbstractionAAMP_OTA&) = delete;

    /*Event Handler*/
    void onPlayerStatusHandler(PlayerStatusData data);

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
     *   @fn GetFirstPTS
     *
     *   @retval PTS of first sample
     */
    double GetFirstPTS() override;
    /**
     * @fn IsInitialCachingSupported
     */
    bool IsInitialCachingSupported() override;
    /**
     * @fn GetMaxBitrate
     * @return long MAX video bitrates
     */
    BitsPerSecond GetMaxBitrate(void) override;
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
//private:
protected:
    PlayerThunderInterface thunderAccessObj;
    std::string prevState;
    std::string prevBlockedReason;
    bool tuned;

	/* Additional data from ATSC playback  */
	std::string mPCRating; 		/**< Parental control rating json string object  */
	int mSsi;  			/**<  Signal strength indicator 0-100 where 100 is a perfect signal. -1 indicates data not available  */
	/* Video info   */
	long mVideoBitrate;
	float mFrameRate;		/**< FrameRate */
	VideoScanType mVideoScanType;   /**< Video Scan Type progressive/interlaced */
	int mAspectRatioWidth;		/**< Aspect Ratio Width*/
	int mAspectRatioHeight;		/**< Aspect Ratio Height*/
	std::string mVideoCodec;	/**<  VideoCodec - E.g MPEG2.*/
	std::string mHdrType; 		/**<  type of HDR being played, in example "DOLBY_VISION" */
	int miVideoWidth;       	/**<  Video Width  */
	int miVideoHeight;       	/**<  Video Height  */

	int miPrevmiVideoWidth;
	int miPrevmiVideoHeight;

	/* Audio Info   */
	long mAudioBitrate; 		/**<  int - Rate of the Audio stream in bps. Calculated based on transport stream rate. So will have some fluctuation. */
	std::string mAudioCodec; 	/**< AudioCodec E.g AC3.*/
	std::string mAudioMixType; 	/**<  AudioMixType(- E.g STEREO. */
	bool  mIsAtmos;  	 	/**<  Is Atmos : 1 - True if audio playing is Dolby Atmos, 0 false ,  -1 indicates data not available */

	/**
	 *   @fn PopulateMetaData
	 */
	bool PopulateMetaData(PlayerStatusData data); /**< reads metadata properties from player status object and return true if any of data is changed  */
	void SendMediaMetadataEvent();

    /**
     * @fn GetAudioTracks
     * @return void
     */
    void GetAudioTracks();

    /**
     * @fn NotifyAudioTrackChange
     *
     * @param[in] tracks - updated audio track info
     */
    void NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks);
    /**
     * @fn GetTextTracks
     * @return voi @return void
     */
    void GetTextTracks();
};

#endif //OTA_SHIM_H_
/**
 * @}
 */
 


