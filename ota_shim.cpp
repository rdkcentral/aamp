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
 * @file ota_shim.cpp
 * @brief shim for dispatching UVE OTA ATSC playback
 */

#include "AampUtils.h"
#include "ota_shim.h"
#include "priv_aamp.h"
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

using namespace std;

ATSCGlobalSettings gATSCSettings;

void StreamAbstractionAAMP_OTA::onPlayerStatusHandler(PlayerStatusData data) {
	// /* For detailed event data, we can print or use details like
	//    playerData["locator"].String(), playerData["length"].String(), playerData["position"].String() */

	// //Following used for blocked status processing
	bool blockedReasonChanged = false;
	std::string currentLocator;

	if(0 == data.currState.compare("BLOCKED"))
	{
		// Check if event is for current aamp instance,
		// sometimes blocked events are delayed and in fast channel change
		// senario , it is delivered late.
		currentLocator =  aamp->GetManifestUrl();

		if( 0 != currentLocator.compare(data.eventUrl))
		{
			AAMPLOG_WARN( "[OTA_SHIM] Ignoring BLOCKED event as tune url %s playerStatus event url %s is not same ",currentLocator.c_str(),data.eventUrl.c_str());

			//Ignore the event
			data.currState = prevState;
		}
		else
		{
			if(0 != data.reasonString.compare(prevBlockedReason))
			{
				blockedReasonChanged = true;
			}
		}
	}

	if(0 != prevState.compare(data.currState) || blockedReasonChanged)
	{
		AAMPPlayerState state = eSTATE_IDLE;
		prevBlockedReason.clear();
		AAMPLOG_WARN( "[OTA_SHIM] State changed from %s to %s ",  prevState.c_str(), data.currState.c_str());
		prevState = data.currState;
		if(0 == data.currState.compare("PENDING"))
		{
			state = eSTATE_PREPARING;
		}else if((0 == data.currState.compare("BLOCKED")) && (0 != data.reasonString.compare("NOT_BLOCKED")))
		{
			AAMPLOG_WARN( "[OTA_SHIM] Received BLOCKED event from player with REASON: %s Current Ratings: %s",  data.reasonString.c_str(), data.ratingString.c_str());

			aamp->SendAnomalyEvent(ANOMALY_WARNING,"BLOCKED REASON:%s", data.reasonString.c_str());
			aamp->SendBlockedEvent(data.reasonString, currentLocator);
			state = eSTATE_BLOCKED;
			prevBlockedReason = data.reasonString;
		}else if(0 == data.currState.compare("PLAYING"))
		{
			if(!tuned){
				aamp->SendTunedEvent(false);
				/* For consistency, during first tune, first move to
				 PREPARED state to match normal IPTV flow sequence */
				aamp->SetState(eSTATE_PREPARED);
				tuned = true;
				aamp->LogFirstFrame();
				aamp->LogTuneComplete();
			}
			AAMPLOG_WARN( "[OTA_SHIM] PLAYING STATE Current Ratings : %s", data.ratingString.c_str());
			state = eSTATE_PLAYING;
		}else if(0 == data.currState.compare("DONE"))
		{
			if(tuned){
				tuned = false;
			}
			state = eSTATE_COMPLETE;
		}else
		{
			if(0 == data.currState.compare("IDLE"))
			{
				aamp->SendAnomalyEvent(ANOMALY_WARNING, "ATSC Tuner Idle");
			}else{
				/* Currently plugin lists only "IDLE","ERROR","PROCESSING","PLAYING"&"DONE" */
				AAMPLOG_INFO( "[OTA_SHIM] Unsupported state change!");
			}
			/* Need not set a new state hence returning */
			return;
		}
		aamp->SetState(state);
	}

	if((0 == data.currState.compare("PLAYING")) || (0 == data.currState.compare("BLOCKED")) &&  0 == data.reasonString.compare("SERVICE_PIN_LOCKED"))
	{
		if(PopulateMetaData(data))
		{
			SendMediaMetadataEvent();

			// generate notify bitrate event if video w/h is changed
			// this is lagacy event used by factory test app to get video info
			if( (miPrevmiVideoWidth != miVideoWidth) ||  (miPrevmiVideoHeight != miVideoHeight) )
			{
				miPrevmiVideoWidth = miVideoWidth;
				miPrevmiVideoHeight = miVideoHeight;
				aamp->NotifyBitRateChangeEvent(mVideoBitrate, eAAMP_BITRATE_CHANGE_BY_OTA, miVideoWidth, miVideoHeight, mFrameRate, 0, false, mVideoScanType, mAspectRatioWidth, mAspectRatioHeight);
			}
		}
	}
}

/**
 *  @brief  reads metadata properties from player status object and return true if any of data is changed
 */
bool StreamAbstractionAAMP_OTA::PopulateMetaData(PlayerStatusData data)
{
	bool isDataChanged = false;
	
	if( mPCRating != data.ratingString )
	{
		AAMPLOG_INFO( "[OTA_SHIM]ratings changed : old:%s new:%s ", mPCRating.c_str(), data.ratingString.c_str());
		mPCRating = data.ratingString;
		isDataChanged = true;
	}

	if(data.ssi != mSsi)
	{
		AAMPLOG_INFO( "[OTA_SHIM]SSI changed : old:%d new:%d ", mSsi, data.ssi);
		mSsi = data.ssi;
		isDataChanged = true;
	}

	VideoScanType tempScanType = (data.vid_progressive ? eVIDEOSCAN_PROGRESSIVE : eVIDEOSCAN_INTERLACED);
	if(mVideoScanType != tempScanType)
	{
		AAMPLOG_INFO( "[OTA_SHIM]Scan type changed : old:%d new:%d ", mVideoScanType, tempScanType);
		isDataChanged = true;
		mVideoScanType = tempScanType;
	}

	float tempframeRate = 0.0;
	
	if((0 != data.vid_frameRateN) && (0 != data.vid_frameRateD))
	{
		tempframeRate = data.vid_frameRateN / data.vid_frameRateD;

		if( mFrameRate != tempframeRate)
		{
			AAMPLOG_INFO( "[OTA_SHIM] mFrameRate changed : old:%f new:%f ", mFrameRate, tempframeRate);
			isDataChanged = true;
			mFrameRate = tempframeRate;
		}
	}

	if( data.vid_aspectRatioWidth != mAspectRatioWidth)
	{
		isDataChanged = true;
		AAMPLOG_INFO( "[OTA_SHIM] mAspectRatioWidth changed : old:%d new:%d ", mAspectRatioWidth, data.vid_aspectRatioWidth);
		mAspectRatioWidth = data.vid_aspectRatioWidth;
	}

	if( mAspectRatioHeight != data.vid_aspectRatioHeight)
	{
		AAMPLOG_INFO( "[OTA_SHIM] tempAspectRatioHeight  : old:%d new:%d ", mAspectRatioHeight, data.vid_aspectRatioHeight);
		isDataChanged = true;
		mAspectRatioHeight = data.vid_aspectRatioHeight;
	}

	if( miVideoWidth != data.vid_width)
	{
		AAMPLOG_INFO( "[OTA_SHIM] miVideoWidth  : old:%d new:%d ",  miVideoWidth, data.vid_width);
		miVideoWidth = data.vid_width;
		isDataChanged = true;
	}

	if( miVideoHeight != data.vid_height)
	{
		AAMPLOG_INFO( "[OTA_SHIM] miVideoHeight  : old:%d new:%d ",  miVideoHeight, data.vid_height);
		miVideoHeight = data.vid_height;
		isDataChanged = true;
	}

	if(0 != mVideoCodec.compare(data.vid_codec))
	{
		AAMPLOG_INFO( "[OTA_SHIM] mVideoCodec : old:%s new:%s ",  mVideoCodec.c_str(), data.vid_codec.c_str());
		mVideoCodec = data.vid_codec;
		isDataChanged = true;
	}

	mHdrType = data.vid_hdrType;

	if(0 != mAudioCodec.compare(data.aud_codec))
	{
		AAMPLOG_INFO( "[OTA_SHIM] tempAudioCodec : old:%s new:%s ",  mAudioCodec.c_str(), data.aud_codec.c_str());
		mAudioCodec = data.aud_codec;
		isDataChanged = true;
	}

	if(0 != mAudioMixType.compare(data.aud_mixType))
	{
		AAMPLOG_INFO( "[OTA_SHIM] tempAudioMixType : old:%s new:%s ",  mAudioMixType.c_str(), data.aud_mixType.c_str());
		mAudioMixType = data.aud_mixType;
		isDataChanged = true;
	}

	if( mIsAtmos != data.aud_isAtmos)
	{
		AAMPLOG_INFO( "[OTA_SHIM] -- mIsAtmos  : old:%d new:%d ",  mIsAtmos, data.aud_isAtmos);
		mIsAtmos = data.aud_isAtmos;
		isDataChanged = true;
	}

	if( isDataChanged )
	{
		mVideoBitrate = data.vid_bitrate;
		mAudioBitrate = data.aud_bitrate;
	}

	return isDataChanged;
}


void StreamAbstractionAAMP_OTA::SendMediaMetadataEvent()
{
	if(aamp->IsEventListenerAvailable(AAMP_EVENT_MEDIA_METADATA))
	{
		MediaMetadataEventPtr event = std::make_shared<MediaMetadataEvent>(-1/*duration*/, miVideoWidth, miVideoHeight, false/*hasDrm*/,true/*isLive*/, ""/*drmtype*/, -1/*programStartTime*/,0/*tsbdepth*/, std::string{});

		// This is video bitrate
		event->addBitrate(mVideoBitrate);
		event->addSupportedSpeed(1);
		event->SetVideoMetaData(mFrameRate,mVideoScanType,mAspectRatioWidth,mAspectRatioHeight, mVideoCodec,  mHdrType, mPCRating,mSsi);
		event->SetAudioMetaData(mAudioCodec,mAudioMixType,mIsAtmos);
		event->addAudioBitrate(mAudioBitrate);
		aamp->SendEvent(event,AAMP_EVENT_ASYNC_MODE);
	}
}

/**
 *  @brief  Initialize a newly created object.
 */
AAMPStatusType StreamAbstractionAAMP_OTA::Init(TuneType tuneType)
{
	AAMPStatusType retval = eAAMPSTATUS_OK;
	if(thunderAccessObj.IsThunderAccess())
	{
		AAMPLOG_INFO("[OTA_SHIM]Inside" );
		prevState = "IDLE";
		
		//initialize few variables, it will invalidate mediametadata/Notifybitrate events
		miVideoWidth = 0;
		miVideoHeight = 0;
		miPrevmiVideoWidth  = 0;
		miPrevmiVideoHeight  = 0;

		prevBlockedReason = "";
		tuned = false;

		aamp->SetContentType("OTA");

		thunderAccessObj.ActivatePlugin();
		std::function<void(PlayerStatusData)> actualMethod = std::bind(&StreamAbstractionAAMP_OTA::onPlayerStatusHandler, this, std::placeholders::_1);
		thunderAccessObj.RegisterOnPlayerStatusOta(actualMethod);
		AAMPStatusType retval = eAAMPSTATUS_OK;

		//activate RDK Shell - not required as this plugin is already activated
		// thunderRDKShellObj.ActivatePlugin();

	}
    return retval;
}

/**
 *  @brief StreamAbstractionAAMP_OTA Constructor
 */
StreamAbstractionAAMP_OTA::StreamAbstractionAAMP_OTA(class PrivateInstanceAAMP *aamp,double seek_pos, float rate)
                          : StreamAbstractionAAMP(aamp)
                            , tuned(false),
                            thunderAccessObj(PlayerThunderAccessPlugin::MEDIAPLAYER),
                            mPCRating(),mSsi(-1),mFrameRate(0),mVideoScanType(eVIDEOSCAN_UNKNOWN),mAspectRatioWidth(0),mAspectRatioHeight(0),
                            mVideoCodec(),mHdrType(),mAudioBitrate(0),mAudioCodec(),mAudioMixType(),mIsAtmos(false),
                            miVideoWidth(0),miVideoHeight(0),miPrevmiVideoWidth(0),miPrevmiVideoHeight(0),
                            mVideoBitrate(0),prevBlockedReason(),prevState()
{ // STUB
}

/**
 * @brief StreamAbstractionAAMP_OTA Destructor
 */
StreamAbstractionAAMP_OTA::~StreamAbstractionAAMP_OTA()
{
	/*
	Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.release", "params":{ "id":"MainPlayer", "tag" : "MyApp"} }
	Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
	*/
        
	if(thunderAccessObj.IsThunderAccess())
	{
        thunderAccessObj.ReleaseOta();
	}
	else
	{
		std::string id = "3";
        std:: string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.release", "{\"id\":\"MainPlayer\",\"tag\" : \"MyApp\"}");
        AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
	}
}

/**
 *  @brief  Starts streaming.
 */
void StreamAbstractionAAMP_OTA::Start(void)
{
	std::string id = "3";
        std::string response;
	const char *display = getenv("WAYLAND_DISPLAY");
	std::string waylandDisplay;
	if( display )
	{
		waylandDisplay = display;
		AAMPLOG_WARN( "WAYLAND_DISPLAY: '%s'", display );
	}
	else
	{
		AAMPLOG_WARN( "WAYLAND_DISPLAY: NULL!" );
	}
	if(aamp)
	{
		std::string url = aamp->GetManifestUrl();
		if(!thunderAccessObj.IsThunderAccess())
		{
				AAMPLOG_WARN( "[OTA_SHIM]Inside CURL ACCESS");
			/*
			Request : {"jsonrpc": "2.0","id": 4,"method": "Controller.1.activate", "params": { "callsign": "org.rdk.MediaPlayer" }}
			Response : {"jsonrpc": "2.0","id": 4,"result": null}
			*/
			response = aamp_PostJsonRPC(id, "Controller.1.activate", "{\"callsign\":\"org.rdk.MediaPlayer\"}" );
				AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
				response.clear();
			/*
			Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.create", "params":{ "id" : "MainPlayer", "tag" : "MyApp"} }
			Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
			*/
			response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.create", "{\"id\":\"MainPlayer\",\"tag\":\"MyApp\"}");
				AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
				response.clear();
			// inform (MediaRite) player instance on which wayland display it should draw the video. This MUST be set before load/play is called.
			/*
			Request : {"jsonrpc":"2.0", "id":3, "method":"org.rdk.MediaPlayer.1.setWaylandDisplay", "params":{"id" : "MainPlayer","display" : "westeros-123"} }
			Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true} }
			*/
			response = aamp_PostJsonRPC( id, "org.rdk.MediaPlayer.1.setWaylandDisplay", "{\"id\":\"MainPlayer\",\"display\":\"" + waylandDisplay + "\"}" );
				AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
				response.clear();
			/*
			Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.load", "params":{ "id":"MainPlayer", "url":"live://...", "autoplay": true} }
			Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
			*/
			response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.load","{\"id\":\"MainPlayer\",\"url\":\""+url+"\",\"autoplay\":true}" );
				AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
				response.clear();
			/*
			Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.play", "params":{ "id":"MainPlayer"} }
			Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
			*/
	
			// below play request harmless, but not needed, given use of autoplay above
			// response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.play", "{\"id\":\"MainPlayer\"}");
				// AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());

		}
		else
		{
			
			thunderAccessObj.StartOta(url, waylandDisplay, aamp->preferredLanguagesString, gATSCSettings.preferredLanguages, aamp->preferredRenditionString, gATSCSettings.preferredRendition);

		}
	}
}

/**
 *  @brief  Stops streaming.
 */
void StreamAbstractionAAMP_OTA::Stop(bool clearChannelData)
{
	/*StreamAbstractionAAMP::Stop is being called twice
	  PrivateInstanceAAMP::Stop calls Stop with clearChannelData set to true
	  PrivateInstanceAAMP::TeardownStream calls Stop with clearChannelData set to false
	  Hence avoiding the Stop with clearChannelData set to false*/
	if(!clearChannelData)
		return;

	if(!thunderAccessObj.IsThunderAccess())
	{
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.stop", "params":{ "id":"MainPlayer"} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std::string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.stop", "{\"id\":\"MainPlayer\"}");
        AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
	}
	else
	{
        thunderAccessObj.StopOta();
	}
}

/**
 * @brief SetVideoRectangle sets the position coordinates (x,y) & size (w,h)
 */
void StreamAbstractionAAMP_OTA::SetVideoRectangle(int x, int y, int w, int h)
{
	if(!thunderAccessObj.IsThunderAccess())
	{
        /*
        Request : {"jsonrpc":"2.0", "id":3, "method": "org.rdk.MediaPlayer.1.setVideoRectangle", "params":{ "id":"MainPlayer", "x":0, "y":0, "w":1280, "h":720} }
        Response: { "jsonrpc":"2.0", "id":3, "result": { "success": true } }
        */
        std::string id = "3";
        std::string response = aamp_PostJsonRPC(id, "org.rdk.MediaPlayer.1.setVideoRectangle", "{\"id\":\"MainPlayer\", \"x\":" + to_string(x) + ", \"y\":" + to_string(y) + ", \"w\":" + to_string(w) + ", \"h\":" + std::to_string(h) + "}");
        AAMPLOG_WARN( "StreamAbstractionAAMP_OTA: response '%s'", response.c_str());
	}
	else
	{
		std::string videoInputType = "";
		thunderAccessObj.SetVideoRectangle(x, y, w, h, videoInputType, PlayerThunderAccessShim::OTA_SHIM);
	}
}

/**
 *  @brief NotifyAudioTrackChange To notify audio track change.Currently not used
 *        as mediaplayer does not have support yet.
 */
void StreamAbstractionAAMP_OTA::NotifyAudioTrackChange(const std::vector<AudioTrackInfo> &tracks)
{
    if ((0 != mAudioTracks.size()) && (tracks.size() != mAudioTracks.size()))
    {
        aamp->NotifyAudioTracksChanged();
    }
    return;
}

/**
 *  @brief Get the list of available audio tracks
 */
std::vector<AudioTrackInfo> & StreamAbstractionAAMP_OTA::GetAvailableAudioTracks(bool allTrack)
{
    if (mAudioTrackIndex.empty())
        GetAudioTracks();

    return mAudioTracks;
}

/**
 *  @brief Get current audio track
 */
int StreamAbstractionAAMP_OTA::GetAudioTrack()
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
                index = (int)std::distance(mAudioTracks.begin(), it);
            }
        }
    }
    return index;
}

/**
 *  @brief Get current audio track
 */
bool StreamAbstractionAAMP_OTA::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
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
void StreamAbstractionAAMP_OTA::SetPreferredAudioLanguages()
{
	if(thunderAccessObj.IsThunderAccess())
	{
		bool modifiedLang = false;
		bool modifiedRend = false;
		AAMPLOG_WARN( "[OTA_SHIM]aamp->preferredLanguagesString : %s, gATSCSettings.preferredLanguages : %s aamp->preferredRenditionString : %s gATSCSettings.preferredRendition : %s",  aamp->preferredLanguagesString.c_str(),gATSCSettings.preferredLanguages.c_str(), aamp->preferredRenditionString.c_str(), gATSCSettings.preferredRendition.c_str());fflush(stdout);

		PlayerPreferredAudioData data;
		data.preferredRenditionString = aamp->preferredLanguagesString;
		data.pluginPreferredLanguagesString = gATSCSettings.preferredLanguages;
		data.preferredRenditionString = aamp->preferredRenditionString;
		data.pluginPreferredRenditionString = gATSCSettings.preferredRendition;
		thunderAccessObj.SetPreferredAudioLanguages(data, PlayerThunderAccessShim::OTA_SHIM);

		if((0 != aamp->preferredLanguagesString.length()) && (aamp->preferredLanguagesString != gATSCSettings.preferredLanguages)){
			modifiedLang = true;
		}
		if((0 != aamp->preferredRenditionString.length()) && (aamp->preferredRenditionString != gATSCSettings.preferredRendition)){

			if(0 == aamp->preferredRenditionString.compare("VISUALLY_IMPAIRED")){
				modifiedRend = true;
			}else if(0 == aamp->preferredRenditionString.compare("NORMAL")){
				modifiedRend = true;
			}else{
				/*No rendition settings to MediaSettings*/
			}
		}
		if(modifiedLang){
			gATSCSettings.preferredLanguages = aamp->preferredLanguagesString;
		}
		if(modifiedRend){
			gATSCSettings.preferredRendition = aamp->preferredRenditionString;
		}
	}
}
		
	


/**
 *  @brief SetAudioTrackByLanguage set the audio language
 */
void StreamAbstractionAAMP_OTA::SetAudioTrackByLanguage(const char* lang)
{
	if(thunderAccessObj.IsThunderAccess())
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
	}
}

/**
 *  @brief GetAudioTracks get the available audio tracks for the selected service / media
 */
void StreamAbstractionAAMP_OTA::GetAudioTracks()
{
	if(thunderAccessObj.IsThunderAccess())
	{
		std::vector<AudioTrackInfo> aTracks;
		std::vector<PlayerAudioData> plyrAudData;
		std::string aTrackIdx = "";
		std::string index = "";
		
		aTrackIdx = thunderAccessObj.GetAudioTracksOta(plyrAudData);

		for(int i = 0; i < plyrAudData.size(); i++)
		{
			index = to_string(i);

			std::string languageCode;
			languageCode = Getiso639map_NormalizeLanguageCode(plyrAudData[i].language, aamp->GetLangCodePreference());
			aTracks.push_back(AudioTrackInfo(index, /*idx*/ languageCode, /*lang*/ plyrAudData[i].contentType, /*rend*/ plyrAudData[i].name, /*name*/ plyrAudData[i].type, /*codecStr*/ plyrAudData[i].pk, /*primaryKey*/ plyrAudData[i].contentType, /*contentType*/ plyrAudData[i].mixType /*mixType*/));
		}

		mAudioTracks = aTracks;
		mAudioTrackIndex = aTrackIdx;
    }
}

/**
 *  @brief SetAudioTrack sets a specific audio track
 */
void StreamAbstractionAAMP_OTA::SetAudioTrack(int trackId)
{
	if(thunderAccessObj.IsThunderAccess())
	{
		std::string tempStr = "";
		tempStr = thunderAccessObj.SetAudioTrackOta(trackId, mAudioTracks[trackId].primaryKey);
		if(!tempStr.empty())
		{
			mAudioTrackIndex = tempStr;
		}
	}
}

/**
 *   @brief Get the list of available text tracks
 */
std::vector<TextTrackInfo> & StreamAbstractionAAMP_OTA::GetAvailableTextTracks(bool all)
{
	AAMPLOG_TRACE("[OTA_SHIM]");
	if (mTextTracks.empty())
		GetTextTracks();

	return mTextTracks;
}

/**
 * @brief GetTextTracks get the available text tracks for the selected service / media
 */
void StreamAbstractionAAMP_OTA::GetTextTracks()
{
	AAMPLOG_TRACE("[OTA_SHIM]");

	std::vector<PlayerTextData> txtData;
	std::vector<TextTrackInfo> txtTracks;

	if(thunderAccessObj.GetTextTracksOta(txtData))
	{
		int ccIndex = 0;
		for(int i = 0; i < txtData.size(); i++)
		{
			std::string trackType;
			trackType = txtData[i].type;
			if(0 == trackType.compare("CC"))
			{

				std::string empty;
				std::string index = std::to_string(ccIndex++);
				std::string serviceNo;
				int ccServiceNumber = -1;
				std::string languageCode = Getiso639map_NormalizeLanguageCode(txtData[i].language, aamp->GetLangCodePreference());

				ccServiceNumber = txtData[i].ccServiceNumber;
				/*Plugin info : ccServiceNumber	int Set to 1-63 for 708 CC Subtitles and 1-4 for 608/TEXT*/
				if(txtData[i].ccType == std::string{"CC708"})
				{
					if((ccServiceNumber >= 1) && (ccServiceNumber <= 63))
					{
						/*708 CC*/
						serviceNo = "SERVICE";
						serviceNo.append(std::to_string(ccServiceNumber));
					}
					else
					{
						AAMPLOG_WARN( "[OTA_SHIM]:unexpected text track for 708 CC");
					}
				}
				else if(txtData[i].ccType == std::string{"CC608"})
				{
					if((ccServiceNumber >= 1) && (ccServiceNumber <= 4))
					{
						/*608 CC*/
						serviceNo = "CC";
						serviceNo.append(std::to_string(ccServiceNumber));
					}
					else
					{
						AAMPLOG_WARN( "[OTA_SHIM]:unexpected text track for 608 CC");
					}
				}
				else if(txtData[i].ccType == std::string{"TEXT"})
				{
					if((ccServiceNumber >= 1) && (ccServiceNumber <= 4))
					{
						/*TEXT CC*/
						serviceNo = "TXT";
						serviceNo.append(std::to_string(ccServiceNumber));
					}
					else
					{
						AAMPLOG_WARN( "[OTA_SHIM]:unexpected text track for TEXT CC");
					}
				}
				else
				{
					AAMPLOG_WARN( "[OTA_SHIM]:unexpected ccType: '%s'",  txtData[i].ccType.c_str());
				}

				txtTracks.push_back(TextTrackInfo(index, languageCode, true, empty, txtData[i].name, serviceNo, empty, txtData[i].pk));
				//values shared: index, language, isCC, rendition-empty, name, instreamId, characteristics-empty, primarykey
				AAMPLOG_WARN("[OTA_SHIM]:: Text Track - index:%s lang:%s, isCC:true, rendition:empty, name:%s, instreamID:%s, characteristics:empty, primarykey:%d",  index.c_str(), languageCode.c_str(), txtData[i].name.c_str(), serviceNo.c_str(), txtData[i].pk);
			}
		}
	}
	else
	{
		std::string empty;
		// Push dummy track , when not published,
		// it is observed that even if track is not published
		// CC1 is present
		txtTracks.push_back(TextTrackInfo("0", "und", true, empty, "Undetermined", "CC1", empty, 0 ));
	}

	mTextTracks = txtTracks;
}

/**
 *  @brief Disable Restrictions (unlock) till seconds mentioned
 */
void StreamAbstractionAAMP_OTA::DisableContentRestrictions(long grace, long time, bool eventChange)
{
	thunderAccessObj.DisableContentRestrictionsOta(grace, time, eventChange);
}

/**
 *  @brief Enable Content Restriction (lock)
 */
void StreamAbstractionAAMP_OTA::EnableContentRestrictions()
{
	thunderAccessObj.EnableContentRestrictionsOta();
}

/**
 * @brief Get output format of stream.
 */
void StreamAbstractionAAMP_OTA::GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat)
{
    primaryOutputFormat = FORMAT_INVALID;
    audioOutputFormat = FORMAT_INVALID;
	auxAudioOutputFormat = FORMAT_INVALID;
	subtitleOutputFormat = FORMAT_INVALID;
}

/**
 *   @brief  Get PTS of first sample.
 */
double StreamAbstractionAAMP_OTA::GetFirstPTS()
{ // STUB
    return 0.0;
}

/**
 *  @brief Check if initial Caching is supports
 */
bool StreamAbstractionAAMP_OTA::IsInitialCachingSupported()
{ // STUB
	return false;
}

/**
 *  @brief Gets Max Bitrate available for current playback.
 */
BitsPerSecond StreamAbstractionAAMP_OTA::GetMaxBitrate()
{ // STUB
    return 0;
}

