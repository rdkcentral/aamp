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
 * @file rmf_shim.cpp
 * @brief shim for dispatching UVE RMF playback
 */

#include "AampUtils.h"
#include "rmf_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

using namespace std;

#define APP_ID "MainPlayer"

RMFGlobalSettings gRMFSettings;

void StreamAbstractionAAMP_RMF::onPlayerStatusHandler(std::string title) {

	if(0 == title.compare("report first video frame"))
	{
		if(!tuned){
			aamp->SendTunedEvent(false);
			tuned = true;
			aamp->LogFirstFrame();
			aamp->LogTuneComplete();
		}
		aamp->SetState(eSTATE_PLAYING);
	}
}

void StreamAbstractionAAMP_RMF::onPlayerErrorHandler(std::string error_message) {
	
	aamp->SendAnomalyEvent(ANOMALY_WARNING, error_message.c_str());
	aamp->SetState(eSTATE_ERROR);
}


/**
 *  @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_RMF::Init(TuneType tuneType)
{
	AAMPLOG_INFO("[RMF_SHIM]Inside" );
	AAMPStatusType retval = eAAMPSTATUS_OK;

	tuned = false;

	aamp->SetContentType("RMF");

	if(false == thunderAccessObj.InitRmf()) //Note: do not terminate unless we're desperate for resources. deinit is sluggish.
	{
		AAMPLOG_ERR("Failed to initialize RMF plugin");
		retval = eAAMPSTATUS_GENERIC_ERROR;
	}
	return retval;
}

/**
 *  @brief StreamAbstractionAAMP_RMF Constructor
 */
StreamAbstractionAAMP_RMF::StreamAbstractionAAMP_RMF(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
	: StreamAbstractionAAMP(aamp)
	  , tuned(false),
	  thunderAccessObj(PlayerThunderAccessPlugin::RMF)
{ // STUB
}

/**
 * @brief StreamAbstractionAAMP_RMF Distructor
 */
StreamAbstractionAAMP_RMF::~StreamAbstractionAAMP_RMF()
{
	AAMPLOG_INFO("[RMF_SHIM]StreamAbstractionAAMP_RMF Destructor called !! ");
}

/**
 *  @brief  Starts streaming.
 */
void StreamAbstractionAAMP_RMF::Start(void)
{
	std::string url = aamp->GetManifestUrl();

	AAMPLOG_INFO( "[RMF_SHIM] url : %s ", url.c_str());

	//SetPreferredAudioLanguages(); //TODO

	std::function<void(std::string)> eventHandler = std::bind(&StreamAbstractionAAMP_RMF::onPlayerStatusHandler, this, std::placeholders::_1);
	std::function<void(std::string)> errorHandler = std::bind(&StreamAbstractionAAMP_RMF::onPlayerErrorHandler, this, std::placeholders::_1);

	if(!thunderAccessObj.StartRmf(url, eventHandler, errorHandler))
	{
		AAMPLOG_ERR("Failed to play RMF URL %s", url.c_str());
	}
}

/**
 *  @brief  Stops streaming.
 */
void StreamAbstractionAAMP_RMF::Stop(bool clearChannelData)
{
	/*StreamAbstractionAAMP::Stop is being called twice
	  PrivateInstanceAAMP::Stop calls Stop with clearChannelData set to true
	  PrivateInstanceAAMP::TeardownStream calls Stop with clearChannelData set to false
	  Hence avoiding the Stop with clearChannelData set to false*/
	if(!clearChannelData)
		return;

	thunderAccessObj.StopRmf();
	aamp->SetState(eSTATE_STOPPED);
}

/**
 * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 */
void StreamAbstractionAAMP_RMF::SetVideoRectangle(int x, int y, int w, int h)
{
	std::string videoInputType = "";
	if(true != thunderAccessObj.SetVideoRectangle(x, y, w, h, videoInputType, PlayerThunderAccessShim::RMF_SHIM))
	{
		AAMPLOG_ERR("Failed to set video rectangle for URL: %s", aamp->GetManifestUrl().c_str());
	}
}

/**
 *  @brief Get the list of available audio tracks
 */
std::vector<AudioTrackInfo> & StreamAbstractionAAMP_RMF::GetAvailableAudioTracks(bool allTrack)
{
	if (mAudioTrackIndex.empty())
		GetAudioTracks();

	return mAudioTracks;
}

/**
 *  @brief Get current audio track
 */
int StreamAbstractionAAMP_RMF::GetAudioTrack()
{
	int index = -1;
	if (mAudioTrackIndex.empty())
		GetAudioTracks();

	if (!mAudioTrackIndex.empty())
	{
		for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
		{
			if (it->index == mAudioTrackIndex)
			{
				index = std::distance(mAudioTracks.begin(), it);
			}
		}
	}
	return index;
}

/**
 *  @brief Get current audio track
 */
bool StreamAbstractionAAMP_RMF::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
	int index = -1;
	bool bFound = false;
	if (mAudioTrackIndex.empty())
		GetAudioTracks();

	if (!mAudioTrackIndex.empty())
	{
		for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
		{
			if (it->index == mAudioTrackIndex)
			{
				audioTrack = *it;
				bFound = true;
			}
		}
	}
	return bFound;
}

/**
 * @brief SetPreferredAudioLanguages set the preferred audio language list
 */
void StreamAbstractionAAMP_RMF::SetPreferredAudioLanguages()
{
	bool modifiedLang = false;
	bool modifiedRend = false;

	PlayerPreferredAudioData data;
	data.preferredLanguagesString = aamp->preferredLanguagesString;
	data.pluginPreferredLanguagesString = gRMFSettings.preferredLanguages;
	thunderAccessObj.SetPreferredAudioLanguages(data, PlayerThunderAccessShim::RMF_SHIM);

	if((0 != aamp->preferredLanguagesString.length()) && (aamp->preferredLanguagesString != gRMFSettings.preferredLanguages)){
		modifiedLang = true;
	}
	if(modifiedLang || modifiedRend)
	{
		bool rpcResult = false;
		//TODO: Pass preferred audio language to MediaEngineRMF. Not currently supported.

	}
}

/**
 *  @brief SetAudioTrackByLanguage set the audio language
 */
void StreamAbstractionAAMP_RMF::SetAudioTrackByLanguage(const char* lang)
{
	int index = -1;

	if(NULL != lang)
	{
		if(mAudioTrackIndex.empty())
			GetAudioTracks();

		std::vector<AudioTrackInfo>::iterator itr;
		for(itr = mAudioTracks.begin(); itr != mAudioTracks.end(); itr++)
		{
			if(0 == strcmp(lang, itr->language.c_str()))
			{
				index = std::distance(mAudioTracks.begin(), itr);
				break;
			}
		}
	}
	if(-1 != index)
	{
		SetAudioTrack(index);
	}
	return;
}

/**
 *  @brief GetAudioTracks get the available audio tracks for the selected service / media
 */
void StreamAbstractionAAMP_RMF::GetAudioTracks()
{
	//TODO: coming soon...
	return;
}


/**
 *  @brief GetAudioTrackInternal get the primary key for the selected audio
 */
int StreamAbstractionAAMP_RMF::GetAudioTrackInternal()
{
	//TODO: audio track selection support will follow later.
	return 0;
}

/**
 *  @brief SetAudioTrack sets a specific audio track
 */
void StreamAbstractionAAMP_RMF::SetAudioTrack(int trackId)
{
	//TODO: audio track selection will follow later.
}

/**
 *   @brief Get the list of available text tracks
 */
std::vector<TextTrackInfo> & StreamAbstractionAAMP_RMF::GetAvailableTextTracks(bool all)
{
	AAMPLOG_TRACE("[RMF_SHIM]");
	return mTextTracks;
}

/**
 * @brief GetTextTracks get the available text tracks for the selected service / media
 */
void StreamAbstractionAAMP_RMF::GetTextTracks()
{
	//TODO: this is a placeholder. Actual CC support will follow later.
	return;
}

/**
 *  @brief Disable Restrictions (unlock) till seconds mentioned
 */
void StreamAbstractionAAMP_RMF::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	//Not supported.
}

/**
 *  @brief Enable Content Restriction (lock)
 */
void StreamAbstractionAAMP_RMF::EnableContentRestrictions()
{
	//Not supported.
}


/**
 * @brief Get output format of stream.
 */
void StreamAbstractionAAMP_RMF::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
	primaryOutputFormat = FORMAT_INVALID;
	audioOutputFormat = FORMAT_INVALID;
	auxAudioOutputFormat = FORMAT_INVALID;
	subtitleOutputFormat = FORMAT_INVALID;
}

/**
 *   @brief Return MediaTrack of requested type
 */
MediaTrack* StreamAbstractionAAMP_RMF::GetMediaTrack(TrackType type)
{ // STUB
	return NULL;
}

/**
 * @brief Get current stream position.
 */
double StreamAbstractionAAMP_RMF::GetStreamPosition()
{ // STUB
	return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 */
StreamInfo* StreamAbstractionAAMP_RMF::GetStreamInfo(int idx)
{ // STUB
	return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 */
double StreamAbstractionAAMP_RMF::GetFirstPTS()
{ // STUB
	return 0.0;
}

/**
 *   @brief  Get Start time PTS of first sample.
 */
double StreamAbstractionAAMP_RMF::GetStartTimeOfFirstPTS()
{ // STUB
	return 0.0;
}

