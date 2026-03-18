/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file FakeRMF.cpp
 * @brief Fake RMF shim
 */

#include "rmf_shim.h"



void StreamAbstractionAAMP_RMF::onPlayerStatusHandler(std::string title)
{
}

void StreamAbstractionAAMP_RMF::onPlayerErrorHandler(std::string error_message)
{
}


/**
 *  @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_RMF::Init(TuneType tuneType)
{
	return eAAMPSTATUS_OK;
}

/**
 *  @brief StreamAbstractionAAMP_RMF Constructor
 */
StreamAbstractionAAMP_RMF::StreamAbstractionAAMP_RMF(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
	: StreamAbstractionAAMP(aamp)
	  , tuned(false),
	  thunderAccessObj(PlayerThunderAccessPlugin::RMF)
{ 
}

/**
 * @brief StreamAbstractionAAMP_RMF Distructor
 */
StreamAbstractionAAMP_RMF::~StreamAbstractionAAMP_RMF()
{
}

/**
 *  @brief  Starts streaming.
 */
void StreamAbstractionAAMP_RMF::Start(void)
{
}

/**
 *  @brief  Stops streaming.
 */
void StreamAbstractionAAMP_RMF::Stop(bool clearChannelData)
{
}

/**
 * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 */
void StreamAbstractionAAMP_RMF::SetVideoRectangle(int x, int y, int w, int h)
{
}

/**
 *  @brief Get the list of available audio tracks
 */
std::vector<AudioTrackInfo> & StreamAbstractionAAMP_RMF::GetAvailableAudioTracks(bool allTrack)
{
	return mAudioTracks;
}

/**
 *  @brief Get current audio track
 */
int StreamAbstractionAAMP_RMF::GetAudioTrack()
{
	return -1;
}

/**
 *  @brief Get current audio track
 */
bool StreamAbstractionAAMP_RMF::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
	return false;
}

/**
 * @brief SetPreferredAudioLanguages set the preferred audio language list
 */
void StreamAbstractionAAMP_RMF::SetPreferredAudioLanguages()
{
}

/**
 *  @brief SetAudioTrackByLanguage set the audio language
 */
void StreamAbstractionAAMP_RMF::SetAudioTrackByLanguage(const char* lang)
{
}

/**
 *  @brief GetAudioTracks get the available audio tracks for the selected service / media
 */
void StreamAbstractionAAMP_RMF::GetAudioTracks()
{
}


/**
 *  @brief GetAudioTrackInternal get the primary key for the selected audio
 */
int StreamAbstractionAAMP_RMF::GetAudioTrackInternal()
{
	return 0;
}

/**
 *  @brief SetAudioTrack sets a specific audio track
 */
void StreamAbstractionAAMP_RMF::SetAudioTrack(int trackId)
{
}

/**
 *   @brief Get the list of available text tracks
 */
std::vector<TextTrackInfo> & StreamAbstractionAAMP_RMF::GetAvailableTextTracks(bool all)
{
	return mTextTracks;
}

/**
 * @brief GetTextTracks get the available text tracks for the selected service / media
 */
void StreamAbstractionAAMP_RMF::GetTextTracks()
{
}

/**
 *  @brief Disable Restrictions (unlock) till seconds mentioned
 */
void StreamAbstractionAAMP_RMF::DisableContentRestrictions(long grace, long time, bool eventChange)
{
}

/**
 *  @brief Enable Content Restriction (lock)
 */
void StreamAbstractionAAMP_RMF::EnableContentRestrictions()
{
}


/**
 * @brief Get output format of stream.
 */
void StreamAbstractionAAMP_RMF::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
}

/**
 *   @brief Return MediaTrack of requested type
 */
MediaTrack* StreamAbstractionAAMP_RMF::GetMediaTrack(TrackType type)
{
	return NULL;
}

/**
 * @brief Get current stream position.
 */
double StreamAbstractionAAMP_RMF::GetStreamPosition()
{
	return 0.0;
}

/**
 *   @brief Get stream information of a profile from subclass.
 */
StreamInfo* StreamAbstractionAAMP_RMF::GetStreamInfo(int idx)
{
	return NULL;
}

/**
 *   @brief  Get PTS of first sample.
 */
double StreamAbstractionAAMP_RMF::GetFirstPTS()
{
	return 0.0;
}

/**
 *   @brief  Get Start time PTS of first sample.
 */
double StreamAbstractionAAMP_RMF::GetStartTimeOfFirstPTS()
{
	return 0.0;
}



