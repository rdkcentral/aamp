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
 * @file aampgstplayer.cpp
 * @brief Gstreamer based player impl for AAMP
 */

#include "AampMemoryUtils.h"
#include "aampgstplayer.h"
#include "isobmffbuffer.h"
#include "AampUtils.h"
#include "TextStyleAttributes.h"
#include "AampStreamSinkManager.h"
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "priv_aamp.h"
#include <atomic>
#include <algorithm>

#include "InterfacePlayerRDK.h"
#include "ID3Metadata.hpp"
#include "AampSegmentInfo.hpp"
#include "AampBufferControl.h"
#include "AampDefine.h"
#include <functional>

#define PIPELINE_NAME "AAMPGstPlayerPipeline"

#define AAMP_MIN_DECODE_ERROR_INTERVAL 10000                     /**< Minimum time interval in milliseconds between two decoder error CB to send anomaly error */
#define INVALID_RATE -9999

/**
 * @struct AAMPGstPlayerPriv
 * @brief Holds private variables of AAMPGstPlayer
 */
struct AAMPGstPlayerPriv
{
	AAMPGstPlayerPriv(const AAMPGstPlayerPriv&) = delete;
	AAMPGstPlayerPriv& operator=(const AAMPGstPlayerPriv&) = delete;
	AampBufferControl::BufferControlMaster mBufferControl[AAMP_TRACK_COUNT];
	AAMPGstPlayerPriv() :mBufferControl()
	{

	}
};

/**
 * @brief Maps and sets configuration parameters from the AAMPGstPlayer instance
 *        to the InterfacePlayerRDK instance for GStreamer playback configuration.
 */
static void InitializePlayerConfigs(AAMPGstPlayer *_this, void *playerInstance)
{
	auto interfacePlayer = static_cast<InterfacePlayerRDK*>(playerInstance);
	auto& config = _this->aamp->mConfig;
	interfacePlayer->m_gstConfigParam->media = _this->aamp->GetMediaFormatTypeEnum();
	interfacePlayer->m_gstConfigParam->networkProxy =_this->aamp->GetNetworkProxy();
	interfacePlayer->m_gstConfigParam->tcpServerSink = config->IsConfigSet(eAAMPConfig_useTCPServerSink);
	interfacePlayer->m_gstConfigParam->tcpPort = config->GetConfigValue(eAAMPConfig_TCPServerSinkPort);
	interfacePlayer->m_gstConfigParam->appSrcForProgressivePlayback = config->IsConfigSet(eAAMPConfig_UseAppSrcForProgressivePlayback);
	interfacePlayer->m_gstConfigParam->enablePTSReStamp = config->IsConfigSet(eAAMPConfig_EnablePTSReStamp);
	interfacePlayer->m_gstConfigParam->seamlessAudioSwitch = config->IsConfigSet(eAAMPConfig_SeamlessAudioSwitch);
	interfacePlayer->m_gstConfigParam->videoBufBytes = config->GetConfigValue(eAAMPConfig_GstVideoBufBytes);
	interfacePlayer->m_gstConfigParam->enableDisconnectSignals = config->IsConfigSet(eAAMPConfig_enableDisconnectSignals);
	interfacePlayer->m_gstConfigParam->eosInjectionMode = config->GetConfigValue(eAAMPConfig_EOSInjectionMode);
	interfacePlayer->m_gstConfigParam->vodTrickModeFPS =  config->GetConfigValue(eAAMPConfig_VODTrickPlayFPS);
	interfacePlayer->m_gstConfigParam->enableGstPosQuery =  config->IsConfigSet(eAAMPConfig_EnableGstPositionQuery);
	interfacePlayer->m_gstConfigParam->audioBufBytes = config->GetConfigValue(eAAMPConfig_GstAudioBufBytes);
	interfacePlayer->m_gstConfigParam->progressTimer = config->GetConfigValue(eAAMPConfig_ReportProgressInterval);
	interfacePlayer->m_gstConfigParam->gstreamerBufferingBeforePlay = config->IsConfigSet(eAAMPConfig_GStreamerBufferingBeforePlay);
	interfacePlayer->m_gstConfigParam->seiTimeCode = config->IsConfigSet(eAAMPConfig_SEITimeCode);
	interfacePlayer->m_gstConfigParam->gstLogging = config->IsConfigSet(eAAMPConfig_GSTLogging);
	interfacePlayer->m_gstConfigParam->progressLogging =  config->IsConfigSet(eAAMPConfig_ProgressLogging);
	interfacePlayer->m_gstConfigParam->useWesterosSink = config->IsConfigSet(eAAMPConfig_UseWesterosSink);
	interfacePlayer->m_gstConfigParam->enableRectPropertyCfg = config->IsConfigSet(eAAMPConfig_EnableRectPropertyCfg);
	interfacePlayer->m_gstConfigParam->useRialtoSink = config->IsConfigSet(eAAMPConfig_useRialtoSink);
	interfacePlayer->m_gstConfigParam->monitorAV = config->IsConfigSet(eAAMPConfig_MonitorAV);
	interfacePlayer->m_gstConfigParam->disableUnderflow = config->IsConfigSet(eAAMPConfig_DisableUnderflow);
	interfacePlayer->m_gstConfigParam->monitorAvsyncThresholdMs = config->GetConfigValue(eAAMPConfig_MonitorAVSyncThreshold);
	interfacePlayer->m_gstConfigParam->monitorJumpThresholdMs =  config->GetConfigValue(eAAMPConfig_MonitorAVJumpThreshold);
	interfacePlayer->m_gstConfigParam->audioDecoderStreamSync = _this->aamp->mAudioDecoderStreamSync;
	interfacePlayer->m_gstConfigParam->audioOnlyMode = _this->aamp->mAudioOnlyPb;
	interfacePlayer->m_gstConfigParam->gstreamerSubsEnabled = _this->aamp->IsGstreamerSubsEnabled();
	interfacePlayer->m_gstConfigParam->media = _this->aamp->GetMediaFormatTypeEnum();
}

/*
 * @brief Handles GStreamer buffer underflow events.

 * @param mediaType Type of media affected by the underflow.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleOnGstBufferUnderflowCb(int mediaType, AAMPGstPlayer * _this);

/**
 * @brief Handles buffering timeout events.

 * @param isBufferingTimeoutConditionMet Indicates if the buffering timeout condition is met.
 * @param isRateCorrectionDefaultOnPlaying Indicates if rate correction is enabled.
 * @param isPlayerReady used to notify subtitle parser for direct subtec integration
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleBufferingTimeoutCb(bool isBufferingTimeoutConditionMet, bool isRateCorrectionDefaultOnPlaying, bool isPlayerReady, AAMPGstPlayer * _this);

/**
 * @brief Registers GStreamer bus callbacks.

 * Registers the necessary callbacks with the GStreamer pipeline to handle  events.

 * @param _this Pointer to the AAMPGstPlayer instance.
 * @param playerInstance Pointer to the player instance.
 */
static void RegisterBusCb(AAMPGstPlayer *_this, void *playerInstance);

/**
 * @brief Handles red button callback events.

 * @param data Pointer to the data containing the SEI timecode.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleRedButtonCallback(const char *data, AAMPGstPlayer * _this);

/**
 * @brief Handles GStreamer bus messages.

 * @param busEvent GStreamer bus event data.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleBusMessage(const BusEventData busEvent, AAMPGstPlayer * _this);

/**
 * @brief Handles GStreamer decode error events.

 * @param decodeErrorCBCount Count of decode errors.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleOnGstDecodeErrorCb(int decodeErrorCBCount, AAMPGstPlayer * _this);

/**
 * @brief Handles GStreamer PTS error events.

 * @param isVideo Indicates if the error is related to video.
 * @param isAudioSink Indicates if the error is related to the audio sink.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleOnGstPtsErrorCb(bool isVideo, bool isAudioSink, AAMPGstPlayer * _this);

/**
 * @brief Handles the need_data in the aamp buffer.
 *
 * @param mediaType Type of media.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void NeedData(int mediaType, AAMPGstPlayer * _this);

/**
 * @brief Handles the enough_data in the aamp buffer.
 *
 * @param mediaType Type of media.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void EnoughData(int mediaType, AAMPGstPlayer * _this);

/**
 * @brief Registers GStreamer bus callbacks.

 * Registers the necessary callbacks with the GStreamer pipeline to handle  events.

 * @param _this Pointer to the AAMPGstPlayer instance.
 * @param playerInstance Pointer to the player instance.
 */
static void RegisterBusCb(AAMPGstPlayer *_this, void *playerInstance)
{
	auto Instance = static_cast<InterfacePlayerRDK*>(playerInstance);

	Instance->RegisterBufferUnderflowCb([_this](int mediaType){
		HandleOnGstBufferUnderflowCb(mediaType, _this);
	});

	Instance->RegisterBusEvent([_this](const BusEventData& event) {
		HandleBusMessage(event, _this);
	});

	Instance->RegisterGstDecodeErrorCb([_this](int decodeErrorCBCount) {
		HandleOnGstDecodeErrorCb(decodeErrorCBCount, _this);
	});

	Instance->RegisterGstPtsErrorCb([_this](bool isVideo, bool isAudioSink) {
		HandleOnGstPtsErrorCb(isVideo, isAudioSink, _this);
	});

	Instance->RegisterBufferingTimeoutCb([_this](bool isBufferingTimeoutConditionMet, bool isRateCorrectionDefaultOnPlaying, bool isPlayerReady){
		HandleBufferingTimeoutCb(isBufferingTimeoutConditionMet, isRateCorrectionDefaultOnPlaying, isPlayerReady, _this);
	});

	Instance->RegisterHandleRedButtonCallback([_this](const char *data){
		HandleRedButtonCallback(data, _this);
	});

	Instance->RegisterNeedDataCb([_this](int media){
		NeedData(media, _this);
	});

	Instance->RegisterEnoughDataCb([_this](int media){
		EnoughData(media, _this);
	});
}

/**
 * @brief Handles the need_data in the aamp buffer.
 *
 * @param mediaType Type of media.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void NeedData(int mediaType, AAMPGstPlayer * _this)
{
	UsingPlayerId playerId( _this->aamp->mPlayerId );
	AampMediaType media = static_cast<AampMediaType>(mediaType);
	_this->privateContext->mBufferControl[media].needData(_this, media);
}

/**
 * @brief Handles the enough data in the aamp buffer.
 *
 * @param mediaType Type of media.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void EnoughData(int mediaType, AAMPGstPlayer * _this)
{
	UsingPlayerId playerId( _this->aamp->mPlayerId );
	AampMediaType media = static_cast<AampMediaType>(mediaType);
	_this->privateContext->mBufferControl[media].enoughData(_this, media);
}

/**
 * @brief Registers callbacks for the first frame event.
 */
void AAMPGstPlayer::RegisterFirstFrameCallbacks()
{
	playerInstance->callbackMap[InterfaceCB::firstVideoFrameDisplayed] = [this]()
	{
		aamp->NotifyFirstVideoFrameDisplayed();
	};
	playerInstance->callbackMap[InterfaceCB::idleCb] = [this]()
	{
		UsingPlayerId playerId( aamp->mPlayerId );
		aamp->ReportProgress();

	};
	playerInstance->callbackMap[InterfaceCB::progressCb] = [this]()
	{
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			privateContext->mBufferControl[i].update(this, static_cast<AampMediaType>(i));
		}
		aamp->ReportProgress();
	};
	playerInstance->callbackMap[InterfaceCB::firstVideoFrameReceived] = [this]()
	{
		aamp->NotifyFirstFrameReceived(this->playerInstance->GetCCDecoderHandle());
	};
	playerInstance->callbackMap[InterfaceCB::notifyEOS] = [this]()
	{
		aamp->NotifyEOSReached();
	};
	playerInstance->FirstFrameCallback([this](int mediatype, bool notifyFirstBuffer, bool initCC, bool& requireFirstVideoFrameDisplay, bool &audioOnly) {
		this->NotifyFirstFrame((AampMediaType)mediatype, notifyFirstBuffer, initCC, requireFirstVideoFrameDisplay, audioOnly);
	});
	playerInstance->setupStreamCallbackMap[InterfaceCB::startNewSubtitleStream] = [this](int streamId)
	{
		if (eGST_MEDIATYPE_SUBTITLE == streamId)
		{
			if(this->aamp->IsGstreamerSubsEnabled())
			{
				this->aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);                                    /* Stop any ongoing downloads before setting up a new subtitle stream */
			}
		}

	};

	playerInstance->StopCallback([this](bool status)
								 {
		this->Stop(status);
	});
	playerInstance->TearDownCallback([this](bool status, int mediaType)
									 {
		if(status)
		{
			privateContext->mBufferControl[(AampMediaType)mediaType].teardownStart();
		}
		else
		{
			privateContext->mBufferControl[(AampMediaType)mediaType].teardownEnd();
		}
	});
}

void AAMPGstPlayer::UnregisterBusCb()
{
	playerInstance->RegisterBufferUnderflowCb(nullptr);
	playerInstance->RegisterBusEvent(nullptr);
	playerInstance->RegisterGstDecodeErrorCb(nullptr);
	playerInstance->RegisterGstPtsErrorCb(nullptr);
	playerInstance->RegisterBufferingTimeoutCb(nullptr);
	playerInstance->RegisterHandleRedButtonCallback(nullptr);
	playerInstance->RegisterNeedDataCb(nullptr);
	playerInstance->RegisterEnoughDataCb(nullptr);
}

void AAMPGstPlayer::UnregisterFirstFrameCallbacks()
{
	playerInstance->callbackMap[InterfaceCB::firstVideoFrameDisplayed] = nullptr;
	playerInstance->callbackMap[InterfaceCB::idleCb] = nullptr;
	playerInstance->callbackMap[InterfaceCB::progressCb] = nullptr;
	playerInstance->callbackMap[InterfaceCB::firstVideoFrameReceived] = nullptr;
	playerInstance->callbackMap[InterfaceCB::notifyEOS] = nullptr;
	playerInstance->FirstFrameCallback(nullptr);
	playerInstance->setupStreamCallbackMap[InterfaceCB::startNewSubtitleStream] = nullptr;
	playerInstance->StopCallback(nullptr);
	playerInstance->TearDownCallback(nullptr);
}

void AAMPGstPlayer::NotifyFirstFrame(int mediatype, bool notifyFirstBuffer, bool initCC, bool &requireFirstVideoFrameDisplay, bool &audioOnly)
{
	bool firstBufferNotified=false;
	AampMediaType type = static_cast<AampMediaType>(mediatype);
	// LogTuneComplete will be noticed after getting video first frame.
	// incase of audio or video only playback NumberofTracks =1, so in that case also LogTuneCompleted needs to captured when either audio/video frame received.
	if(notifyFirstBuffer && !aamp->mIsFlushOperationInProgress)
	{
		aamp->LogFirstFrame();
		aamp->LogTuneComplete();
		aamp->NotifyFirstBufferProcessed(GetVideoRectangle());
		firstBufferNotified=true;
	}
	audioOnly = aamp->mAudioOnlyPb;
	if (eMEDIATYPE_VIDEO == type)
	{
		if((aamp->mTelemetryInterval > 0) && aamp->mDiscontinuityFound)
		{
			aamp->SetDiscontinuityParam();
		}

		// No additional checks added here, since the NotifyFirstFrame will be invoked only once
		// in westerossink disabled case until specific platform fixes it. Also aware of NotifyFirstBufferProcessed called
		// twice in this function, since it updates timestamp for calculating time elapsed, its trivial
		if (!firstBufferNotified && !aamp->mIsFlushOperationInProgress)
		{
			aamp->NotifyFirstBufferProcessed(GetVideoRectangle());
		}

		if (initCC)
		{
			//If pipeline is set to ready forcefully due to change in track_id, then re-initialize CC
			aamp->InitializeCC(playerInstance->GetCCDecoderHandle());
		}

		requireFirstVideoFrameDisplay = aamp->IsFirstVideoFrameDisplayedRequired();
	}
};

/*AAMPGstPlayer constructor*/

AAMPGstPlayer::AAMPGstPlayer(PrivateInstanceAAMP *aamp, id3_callback_t id3HandlerCallback, std::function<void(const unsigned char *, int, int, int) > exportFrames) : aamp(NULL), mEncryptedAamp(NULL), privateContext(NULL), mBufferingLock(), trickTeardown(false), m_ID3MetadataHandler{id3HandlerCallback}, cbExportYUVFrame(NULL)

{
	privateContext = new AAMPGstPlayerPriv();
	playerInstance = new InterfacePlayerRDK();                                       // for time being to use across class and non-class members when progressive testing
	RegisterBusCb(this, playerInstance);
	if(privateContext)
	{
		this->aamp = aamp;
		// Initially set to this instance, can be changed by SetEncryptedAamp
		this->mEncryptedAamp = aamp;

		this->cbExportYUVFrame = exportFrames;
		playerInstance->gstCbExportYUVFrame = exportFrames;
		std::string debugLevel = GETCONFIGVALUE(eAAMPConfig_GstDebugLevel);
		if(!debugLevel.empty())
		{
			playerInstance->EnableGstDebugLogging(debugLevel);
		}
		InitializePlayerConfigs(this,playerInstance);
		playerInstance->SetLoggerInfo(AampLogManager::disableLogRedirection, AampLogManager::enableEthanLogRedirection, AampLogManager::aampLoglevel, AampLogManager::locked);
		playerInstance->SetPlayerName(PLAYER_NAME);
		playerInstance->setEncryption((void*)aamp);
		RegisterFirstFrameCallbacks();
	}
	else
	{
		AAMPLOG_WARN("privateContext  is null");  //CID:85372 - Null Returns
	}
}


/**
 * @brief AAMPGstPlayer Destructor
 */

AAMPGstPlayer::~AAMPGstPlayer()
{
	UnregisterBusCb();
	UnregisterFirstFrameCallbacks();
	playerInstance->DestroyPipeline();
	SAFE_DELETE(privateContext);
}

/**
 * @brief Handles buffering timeout events.

 * @param isBufferingTimeoutConditionMet Indicates if the buffering timeout condition is met.
 * @param isRateCorrectionDefaultOnPlaying Indicates if rate correction is enabled.
 * @param isPlayerReady used to notify subtitle parser for direct subtec integration
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleBufferingTimeoutCb(bool isBufferingTimeoutConditionMet, bool isRateCorrectionDefaultOnPlaying, bool isPlayerReady, AAMPGstPlayer * _this)
{
	auto aamp = _this->aamp;
	if( aamp )
	{
		UsingPlayerId playerId( aamp->mPlayerId );
		if(isBufferingTimeoutConditionMet)
		{
			AAMPLOG_WARN("Schedule retune.");
			aamp->ScheduleRetune(eGST_ERROR_VIDEO_BUFFERING, eMEDIATYPE_VIDEO);
		}
		else if(isPlayerReady)
		{
			if(isRateCorrectionDefaultOnPlaying)
			{
				// Setting first fractional rate as DEFAULT_INITIAL_RATE_CORRECTION_SPEED right away on PLAYING to avoid audio drop
				if (aamp->mConfig->IsConfigSet(eAAMPConfig_EnableLiveLatencyCorrection) && aamp->IsLive())
				{
					AAMPLOG_WARN("Setting first fractional rate %.6f right after moving to PLAYING", DEFAULT_INITIAL_RATE_CORRECTION_SPEED);
					_this->SetPlayBackRate(DEFAULT_INITIAL_RATE_CORRECTION_SPEED);
				}
			}
			if(!aamp->IsGstreamerSubsEnabled())
			{
				aamp->UpdateSubtitleTimestamp();
			}
		}
	}
}

/**
 * @brief Handles GStreamer decode error events.

 * @param decodeErrorCBCount Count of decode errors.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleOnGstDecodeErrorCb(int decodeErrorCBCount, AAMPGstPlayer * _this)
{
	_this->aamp->SendAnomalyEvent(ANOMALY_WARNING, "Decode Error Message Callback=%d time=%d",decodeErrorCBCount, AAMP_MIN_DECODE_ERROR_INTERVAL);
	AAMPLOG_ERR("## APP[%s] Got Decode Error message",_this->aamp->GetAppName().c_str());
}

/**
 * @brief Handles GStreamer PTS error events.

 * @param isVideo Indicates if the error is related to video.
 * @param isAudioSink Indicates if the error is related to the audio sink.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleOnGstPtsErrorCb(bool isVideo, bool isAudioSink, AAMPGstPlayer * _this)
{
	AAMPLOG_ERR("## APP[%s] Got PTS error message", _this->aamp->GetAppName().c_str());
	if(isVideo)
	{
		_this->aamp->ScheduleRetune(eGST_ERROR_PTS, eMEDIATYPE_VIDEO);
	}
	else if(isAudioSink)
	{
		_this->aamp->ScheduleRetune(eGST_ERROR_PTS, eMEDIATYPE_AUDIO);
	}
}

/**
 * @brief Handles GStreamer buffer underflow events.

 * @param mediaType Type of media affected by the underflow.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleOnGstBufferUnderflowCb(int mediaType, AAMPGstPlayer * _this)
{
	AampMediaType type = static_cast<AampMediaType>(mediaType);

	bool isBufferFull = _this->privateContext->mBufferControl[type].isBufferFull(type);
	_this->privateContext->mBufferControl[type].underflow(_this, type);
	_this->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, type, isBufferFull);		/* Schedule a retune */
}

/**
 * @brief Handles red button callback events.

 * @param data Pointer to the data containing the SEI timecode.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleRedButtonCallback(const char *data, AAMPGstPlayer * _this)
{
	if (_this)
	{
		_this->aamp->seiTimecode.assign(data);
	}
}

/**
 * @brief Handles GStreamer bus messages.
 * @param busEvent GStreamer bus event data.
 * @param _this Pointer to the AAMPGstPlayer instance.
 */
static void HandleBusMessage(const BusEventData busEvent, AAMPGstPlayer * _this)
{
	switch(busEvent.msgType)
	{
		case MESSAGE_ERROR:
		{
			std::string errorDesc = "GstPipeline Error:" + busEvent.msg;
			if (busEvent.msg.find("video decode error") != std::string::npos)
			{
				_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, errorDesc.c_str(), false);
			}
			else if (busEvent.msg.find("HDCP Compliance Check Failure") != std::string::npos)
			{
				_this->aamp->SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR, errorDesc.c_str(), false);
			}
			else if ((busEvent.msg.find("Internal data stream error") != std::string::npos) && _this->aamp->mConfig->IsConfigSet(eAAMPConfig_RetuneForGSTError))
			{
				AAMPLOG_ERR("Schedule retune for GstPipeline Error");
				_this->aamp->ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO);
			}
			else if (busEvent.msg.find("Error parsing H.264 stream") != std::string::npos)
			{ // note: surfacing this intermittent error can cause freeze on partner apps.
				AAMPLOG_WARN("%s", errorDesc.c_str());
			}
			else if (busEvent.msg.find("This file is corrupt and cannot be played") != std::string::npos)
			{ // fatal error; disable retry flag to avoid failure loop
				AAMPLOG_ERR("%s", errorDesc.c_str());
				_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR,errorDesc.c_str(), false);
			}
			else
			{
				_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, errorDesc.c_str());
			}
		}
			break;

		case MESSAGE_WARNING:
			if (_this->aamp->mConfig->IsConfigSet(eAAMPConfig_DecoderUnavailableStrict)  && busEvent.msg.find("No decoder available") != std::string::npos)
			{
				std::string warnDesc = "GstPipeline Error:" + busEvent.msg;
				_this->aamp->SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, warnDesc.c_str(), false);
			}
			break;

		case MESSAGE_STATE_CHANGE:

		{
			if(busEvent.firstBufferProcessed)
			{
				_this->aamp->NotifyFirstBufferProcessed(_this->GetVideoRectangle());
			}
			if(busEvent.receivedFirstFrame)
			{
				_this->aamp->LogFirstFrame();
				_this->aamp->LogTuneComplete();
			}

			if(_this->aamp->mSetPlayerRateAfterFirstframe || (busEvent.setPlaybackRate && ((AAMP_SLOWMOTION_RATE == _this->aamp->playerrate) && (_this->aamp->rate == AAMP_NORMAL_PLAY_RATE))))
			{
				StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(_this->aamp);
				if (sink)
				{
					if(_this->aamp->mSetPlayerRateAfterFirstframe)
					{
						_this->aamp->mSetPlayerRateAfterFirstframe=false;

						if(false != sink->SetPlayBackRate(_this->aamp->playerrate))
						{
							_this->aamp->rate=_this->aamp->playerrate;
							_this->aamp->SetAudioVolume(0);
						}
					}
					else if (busEvent.setPlaybackRate)
					{
						if(false != sink->SetPlayBackRate(_this->aamp->rate))
						{
							_this->aamp->playerrate=_this->aamp->rate;
						}
					}
				}
			}
		}
			break;

		case MESSAGE_EOS:
			break;

		case MESSAGE_APPLICATION:
			if (busEvent.msg.find("HDCPProtectionFailure") != std::string::npos)
			{
				AAMPLOG_ERR("Received HDCPProtectionFailure event.Schedule Retune ");
				_this->Flush(0, AAMP_NORMAL_PLAY_RATE, true);
				_this->aamp->ScheduleRetune(eGST_ERROR_OUTPUT_PROTECTION_ERROR,eMEDIATYPE_VIDEO);
			}
			break;
		default:
			AAMPLOG_WARN("message not supported");
	}
}


/**
 *  @brief Generate a protection event
 */
void AAMPGstPlayer::QueueProtectionEvent(const char *protSystemId, const void *initData, size_t initDataSize, AampMediaType type)
{
	std::string formatType = this->aamp->IsDashAsset() ? "dash/mpd" : "hls/m3u8";
	playerInstance->QueueProtectionEvent(formatType, protSystemId, initData, initDataSize, type);
}

/**
 *  @brief Cleanup generated protection event
 */
void AAMPGstPlayer::ClearProtectionEvent()
{
	playerInstance->ClearProtectionEvent();
}

/**
 *@brief notify injector to pause pushing the buffer
 */
void AAMPGstPlayer::NotifyInjectorToPause()
{
	playerInstance->PauseInjector();
}

/**
 *@brief notify injector to resume pushing the buffer
 */
void AAMPGstPlayer::NotifyInjectorToResume()
{
	playerInstance->ResumeInjector();
}

/**
 *  @brief Inject stream buffer to gstreamer pipeline
 */
bool AAMPGstPlayer::SendHelper(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration, bool copy, double fragmentPTSoffset, bool initFragment, bool discontinuity)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		if (eMEDIATYPE_VIDEO == mediaType)
		{
			bool isFirstVideoBuffer = playerInstance->HandleVideoBufferSent();
			if(isFirstVideoBuffer)
			{ // required in order for subtitle harvesting/processing to work
				aamp->UpdateSubtitleTimestamp();
				// required in order to fetch more than eAAMPConfig_PrePlayBufferCount video segments see WaitForFreeFragmentAvailable()
				aamp->NotifyFirstFrameReceived(playerInstance->GetCCDecoderHandle());
			}
		}
		return false;
	}

	bool sendNewSegmentEvent = false;
	bool notifyFirstBufferProcessed = false;
	bool resetTrickUTC = false;
	bool firstBufferPushed = false;

	// This block checks if the data contain a valid ID3 header and if it is the case
	// calls the callback function.
	{
		namespace aih = aamp::id3_metadata::helpers;

		if (aih::IsValidMediaType(mediaType) &&
			aih::IsValidHeader(static_cast<const uint8_t*>(ptr), len))
		{
			m_ID3MetadataHandler(mediaType, static_cast<const uint8_t*>(ptr), len,
								 {fpts, fdts, fDuration}, nullptr);
		}
	}

	// Ignore eMEDIATYPE_DSM_CC packets
	if(mediaType == eMEDIATYPE_DSM_CC)
	{
		return false;
	}
	if (!aamp->mbNewSegmentEvtSent[mediaType] || (mediaType == eMEDIATYPE_VIDEO && aamp->rate != AAMP_NORMAL_PLAY_RATE))
	{
		sendNewSegmentEvent = true;
	}
	bool bPushBuffer = playerInstance->SendHelper(mediaType, ptr, len, fpts, fdts, fDuration, fragmentPTSoffset, copy, initFragment, discontinuity, notifyFirstBufferProcessed, sendNewSegmentEvent, resetTrickUTC, firstBufferPushed);
	if(sendNewSegmentEvent)
	{
		aamp->mbNewSegmentEvtSent[mediaType] = true;
	}
	if(firstBufferPushed)
	{
		this->aamp->profiler.ProfilePerformed(PROFILE_BUCKET_FIRST_BUFFER);
	}
	if(bPushBuffer)
	{
		privateContext->mBufferControl[mediaType].notifyFragmentInject(this, mediaType, fpts, fdts, fDuration, discontinuity);
	}
	if (eMEDIATYPE_VIDEO == mediaType)
	{
		// For westerossink, it will send first-video-frame-callback signal after each flush
		// So we can move NotifyFirstBufferProcessed to the more accurate signal callback
		if (notifyFirstBufferProcessed)
		{
			aamp->NotifyFirstBufferProcessed(GetVideoRectangle());
			if((aamp->mTelemetryInterval > 0) && aamp->mDiscontinuityFound)
			{
				aamp->SetDiscontinuityParam();
			}
		}
		if(resetTrickUTC)                               //PlatformNeeds TrickStartUTC Time
		{
			aamp->ResetTrickStartUTCTime();
		}

		StopBuffering(false);
	}
	return bPushBuffer;
}

/**
 *  @brief inject HLS/ts elementary stream buffer to gstreamer pipeline
 */
bool AAMPGstPlayer::SendCopy(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration)
{
	return SendHelper( mediaType, ptr, len, fpts, fdts, fDuration, true /*copy*/, 0.0 );
}

/**
 *  @brief inject mp4 segment to gstreamer pipeline
 */
bool AAMPGstPlayer::SendTransfer(AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double fDuration, double fragmentPTSoffset, bool initFragment, bool discontinuity)
{
	return SendHelper( mediaType, ptr, len, fpts, fdts, fDuration, false /*transfer*/, fragmentPTSoffset,  initFragment, discontinuity );
}

/**
 * @brief To start playback
 */
void AAMPGstPlayer::Stream()
{
}


/**
 * @brief Configure pipeline based on A/V formats
 */
void AAMPGstPlayer::Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool setReadyAfterPipelineCreation)
{
	bool isSubEnable = aamp->IsGstreamerSubsEnabled();
	int32_t trackId = aamp->GetCurrentAudioTrackId();
	int PipelinePriority;
	gint rate = INVALID_RATE;

	AAMPLOG_MIL("videoFormat %d audioFormat %d auxFormat %d subFormat %d",format, audioFormat, auxFormat, subFormat);

	playerInstance->SetPreferredDRM(GetDrmSystemID(aamp->GetPreferredDRM())); // pass the preferred DRM to Interface
	InitializePlayerConfigs(this, playerInstance);
	/*set the run time configs for pipeline configuration*/

	const char *envVal = getenv("AAMP_AV_PIPELINE_PRIORITY");
	PipelinePriority = envVal ? atoi(envVal) : -1;
#ifdef AAMP_STOP_SINK_ON_SEEK
	rate = aamp->rate;
#endif
	bool FirstFrameFlag = aamp->IsFirstVideoFrameDisplayedRequired();
	/*Configure and create the pipeline*/
	playerInstance->ConfigurePipeline(static_cast<int>(format),static_cast<int>(audioFormat),static_cast<int>(auxFormat),static_cast<int>(subFormat),
									  bESChangeStatus,forwardAudioToAux,setReadyAfterPipelineCreation,
									  isSubEnable, trackId, rate, PIPELINE_NAME, PipelinePriority, FirstFrameFlag, aamp->GetManifestUrl().c_str());
#ifdef TRACE
	AAMPLOG_MIL("exiting AAMPGstPlayer");
#endif
}

/**
 *  @brief Checks to see if the pipeline is configured for specified media type
 */
bool AAMPGstPlayer::PipelineConfiguredForMedia(AampMediaType type)
{
	bool pipelineConfigured = true;

	if( type != eMEDIATYPE_SUBTITLE || aamp->IsGstreamerSubsEnabled() )
	{
		pipelineConfigured = playerInstance->PipelineConfiguredForMedia((int) type);
	}
	return pipelineConfigured;
}

/**
 *  @brief Starts processing EOS for a particular stream type
 */
void AAMPGstPlayer::EndOfStreamReached(AampMediaType type)
{
	bool shouldHaltBuffering = false;
	playerInstance->EndOfStreamReached(type, shouldHaltBuffering);
	if(shouldHaltBuffering)
	{
		StopBuffering(true);
	}
}

/**
 *  @brief Stop playback and any idle handlers active at the time
 */
void AAMPGstPlayer::Stop(bool keepLastFrame)
{
	AAMPLOG_MIL("entering AAMPGstPlayer_Stop keepLastFrame %d", keepLastFrame);

	playerInstance->Stop(keepLastFrame);

	aamp->seiTimecode.assign("");
	AAMPLOG_MIL("exiting AAMPGstPlayer_Stop");
}


/**
 * @brief Set the instance of PrivateInstanceAAMP that has encrypted content, used in the context of
 * single pipeline.
 * @param[in] aamp - Pointer to the instance of PrivateInstanceAAMP that has the encrypted content
 */
void AAMPGstPlayer::SetEncryptedAamp(PrivateInstanceAAMP *aamp)
{
	mEncryptedAamp = aamp;
	playerInstance->setEncryption((void*)mEncryptedAamp);

}

bool AAMPGstPlayer::IsAssociatedAamp(PrivateInstanceAAMP *aampInstance)
{
	return aamp == aampInstance;
}

/**
 * @brief Change the instance of PrivateInstanceAAMP that is using the gstreamer
 * pipeline, when it is being used as a single pipeline shared among multiple
 * instances of PrivateInstanceAAMP
 * @param[in] newAamp - pointer to new instance of PrivateInstanceAAMP
 * @param[in] id3HandlerCallback - the id3 callback handle associated with this instance of PrivateInstanceAAMP
 */
void AAMPGstPlayer::ChangeAamp(PrivateInstanceAAMP *newAamp, id3_callback_t id3HandlerCallback)
{
	aamp = newAamp;
	if(aamp->DownloadsAreEnabled())
	{
		playerInstance->ResumeInjector();
	}
	playerInstance->DisableDecoderHandleNotified();
	m_ID3MetadataHandler = id3HandlerCallback;
}
/**
 * @brief Flush the track playbin
 * @param[in] pos - position to seek to after flush
 */
void AAMPGstPlayer::FlushTrack(AampMediaType type,double pos)
{
	int mediaType = static_cast<int>(type);
	double audioDelta = aamp->mAudioDelta;
	double subDelta = aamp->mSubtitleDelta;
	double rate = playerInstance->FlushTrack(mediaType, pos, audioDelta, subDelta);

	if(aamp->mCorrectionRate != rate)
	{
		AAMPLOG_MIL("Reset Rate Correction to 1");
		aamp->mCorrectionRate = rate;
	}
}

/**
 *  @brief Get playback duration in MS
 */
long AAMPGstPlayer::GetDurationMilliseconds(void)
{
	long rc ;
	rc = playerInstance->GetDurationMilliseconds();
	return rc;
}

/**
 *  @brief Get playback position in MS
 */
long long AAMPGstPlayer::GetPositionMilliseconds(void)
{
	long long rc ;
	rc = playerInstance->GetPositionMilliseconds();
	return rc;
}

/**
 *  @brief To pause/play pipeline
 */
bool AAMPGstPlayer::Pause( bool pause, bool forceStopGstreamerPreBuffering )
{
	aamp->SyncBegin();					/* Obtains a mutex lock */

	AAMPLOG_MIL("entering AAMPGstPlayer_Pause - pause(%d) stop-pre-buffering(%d)", pause, forceStopGstreamerPreBuffering);

	bool res = this->playerInstance->Pause(pause, forceStopGstreamerPreBuffering);
	if(res)
	{
		if(!aamp->IsGstreamerSubsEnabled())
			aamp->PauseSubtitleParser(pause);
	}

	aamp->SyncEnd();					/* Releases the mutex */

	return res;
	//return retValue;
}

/**
 *  @brief Set video display rectangle co-ordinates
 */
void AAMPGstPlayer::SetVideoRectangle(int x, int y, int w, int h)
{
	playerInstance->SetVideoRectangle(x, y, w, h);
}

/**
 *  @brief Set video zoom
 */
void AAMPGstPlayer::SetVideoZoom(VideoZoomMode zoom)
{
	int zoom_mode = static_cast<int>(zoom);
	playerInstance->SetVideoZoom(zoom_mode);
}

void AAMPGstPlayer::SetSubtitlePtsOffset(std::uint64_t pts_offset)
{
	AAMPLOG_INFO("seek_pos_seconds %2f", aamp->seek_pos_seconds);
	playerInstance->SetSubtitlePtsOffset( pts_offset);
}

void AAMPGstPlayer::SetSubtitleMute(bool muted)
{
	playerInstance->SetSubtitleMute( muted);
}

/**
 * @brief Reset first frame
 */
void AAMPGstPlayer::ResetFirstFrame(void)
{
	playerInstance->ResetFirstFrame();
}

/**
 * @brief Set video mute
 */
void AAMPGstPlayer::SetVideoMute(bool muted)
{
	playerInstance->SetVideoMute(muted);
}

/**
 *  @brief Set audio volume
 */
void AAMPGstPlayer::SetAudioVolume(int volume)
{
	playerInstance->SetAudioVolume(volume);
	playerInstance->SetVolumeOrMuteUnMute();
}

/**
 *  @brief Flush cached GstBuffers and set seek position & rate
 */
void AAMPGstPlayer::Flush(double position, int rate, bool shouldTearDown)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		return;
	}
	AAMPPlayerState state = aamp->GetState();
	bool isAppSeek = false;
	if(state == eSTATE_SEEKING)
	{
		isAppSeek = true;
	}
	bool ret = playerInstance->Flush(position, rate, shouldTearDown, isAppSeek);
	if(ret)
	{
		for (int i = 0; i < AAMP_TRACK_COUNT; i++)
		{
			//reset buffer control states prior to gstreamer flush so that the first needs_data event is caught
			privateContext->mBufferControl[i].flush();
		}

		aamp->mCorrectionRate = (double)AAMP_NORMAL_PLAY_RATE;
	}
}

/**
 *  @brief Process discontinuity for a stream type
 */
bool AAMPGstPlayer::Discontinuity(AampMediaType type)
{
	bool ret = false;

	bool CompleteDiscontinuityDataDeliverForPTSRestamp =false;
	bool shouldHaltBuffering = false;
	ret = playerInstance->CheckDiscontinuity((int)type,(int)aamp->mVideoFormat, aamp->ReconfigureForCodecChange(), CompleteDiscontinuityDataDeliverForPTSRestamp, shouldHaltBuffering);

	if(CompleteDiscontinuityDataDeliverForPTSRestamp)
	{
		AAMPLOG_WARN("NO EOS: PTS-RESTAMP ENABLED and codec has not changed");
		aamp->CompleteDiscontinuityDataDeliverForPTSRestamp(type);
	}

	else if(shouldHaltBuffering)
	{
		StopBuffering(true);
	}
	return ret;
}

/**
 *  @brief Gets Video PTS
 */
long long AAMPGstPlayer::GetVideoPTS(void)
{
	long long rc;
	rc = playerInstance->GetVideoPTS();
	return rc;
}


PlaybackQualityStruct* AAMPGstPlayer::GetVideoPlaybackQuality(void)
{
	return ((PlaybackQualityStruct*)playerInstance->GetVideoPlaybackQuality());
}
/**
 *  @brief Reset EOS SignalledFlag
 */
void AAMPGstPlayer::ResetEOSSignalledFlag()
{
	playerInstance->ResetEOSSignalledFlag();
}

/**
 *  @brief Check if cache empty for a media type
 */
bool AAMPGstPlayer::IsCacheEmpty(AampMediaType mediaType)
{
	bool ret = true;
	int MediaType = (int)mediaType;
	ret = playerInstance->IsCacheEmpty(MediaType);

	return ret;
}

/**
 *  @brief Set pipeline to PLAYING state once fragment caching is complete
 */
void AAMPGstPlayer::NotifyFragmentCachingComplete()
{
	playerInstance->NotifyFragmentCachingComplete();
}

/**
 *  @brief Set pipeline to PAUSED state to wait on NotifyFragmentCachingComplete()
 */
void AAMPGstPlayer::NotifyFragmentCachingOngoing()
{
	if(!playerInstance->gstPrivateContext->paused)
	{
		Pause(true, true);
	}
	playerInstance->gstPrivateContext->pendingPlayState = true;
}

/**
 *  @brief Get video display's width and height
 */
void AAMPGstPlayer::GetVideoSize(int &width, int &height)
{
	playerInstance->GetVideoSize( width, height);
}

/***
 * @fn  IsCodecSupported
 *
 * @brief Check whether Gstreamer platform has support of the given codec or not.
 *        codec to component mapping done in gstreamer side.
 * @param codecName - Name of codec to be checked
 * @return True if platform has the support else false
 */

bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	return InterfacePlayerRDK::IsCodecSupported(codecName);
}


/**
 *  @brief Increase the rank of AAMP decryptor plugins
 */
void AAMPGstPlayer::InitializeAAMPGstreamerPlugins()
{
	InterfacePlayerRDK::InitializePlayerGstreamerPlugins();
}

/**
 *  @brief Signal trick mode discontinuity to gstreamer pipeline
 */
void AAMPGstPlayer::SignalTrickModeDiscontinuity()
{
	playerInstance->SignalTrickModeDiscontinuity();
}

/**
 *  @brief Flush the data in case of a new tune pipeline
 */
void AAMPGstPlayer::SeekStreamSink(double position, double rate)
{
	// shouldTearDown is set to false, because in case of a new tune pipeline
	// might not be in a playing/paused state which causes Flush() to destroy
	// pipeline. This has to be avoided.
	Flush(position, rate, false);

}

/**
 *  @brief Get the video rectangle co-ordinates
 */
std::string AAMPGstPlayer::GetVideoRectangle()
{
	return std::string(playerInstance->GetVideoRectangle());
}

/**
 *  @brief Un-pause pipeline and notify buffer end event to player
 */
void AAMPGstPlayer::StopBuffering(bool forceStop)
{
	std::lock_guard<std::mutex> guard(mBufferingLock);
	//Check if we are in buffering
	if (ISCONFIGSET(eAAMPConfig_ReportBufferEvent) && aamp->GetBufUnderFlowStatus())
	{
		bool isPlaying = false;
		bool sendEndEvent = playerInstance->StopBuffering(forceStop, isPlaying);
		if(!sendEndEvent && isPlaying)
		{
			sendEndEvent = aamp->PausePipeline(false, false);
			aamp->UpdateSubtitleTimestamp();
		}

		if( !sendEndEvent )
		{
			AAMPLOG_ERR("Failed to un-pause pipeline for stop buffering!");
		}
		else
		{
			aamp->SendBufferChangeEvent();          /* To indicate buffer availability */
		}
	}
}

/**
 * @brief  Set playback rate to audio/video sinks
 */
bool AAMPGstPlayer::SetPlayBackRate ( double rate )
{
	AAMPLOG_WARN("setting playback rate: %f",rate );
	bool ret = playerInstance->SetPlayBackRate(rate);
	return ret;
}

/**
 * @brief Set the text style of the subtitle to the options passed
 */
bool AAMPGstPlayer::SetTextStyle(const std::string &options)
{
	bool ret = playerInstance->SetTextStyle(options);
	return ret;
}

/**
 * @fn SignalSubtitleClock
 * @brief Signal the new clock to subtitle module
 * @return - true indicating successful operation in sending the clock update
 */
bool AAMPGstPlayer::SignalSubtitleClock( void )
{
	bool signalSent=false;
	signalSent = playerInstance->SignalSubtitleClock(GetVideoPTS(), aamp->GetBufUnderFlowStatus());
	return signalSent;
}

void AAMPGstPlayer::GetBufferControlData(AampMediaType mediaType, BufferControlData &data) const
{
	int type = (int) mediaType;
	data.StreamReady = playerInstance->IsStreamReady(type);
	if (data.StreamReady)
	{
		data.ElapsedSeconds = std::abs(aamp->GetPositionRelativeToSeekSeconds());

		data.GstWaitingForData = playerInstance->GetBufferControlData(mediaType);
	}
	else
	{
		data.ElapsedSeconds = 0;
		data.GstWaitingForData = false;
	}
}
void AAMPGstPlayer::SetPauseOnStartPlayback(bool enable)
{
	playerInstance->SetPauseOnStartPlayback(enable);
}

/**
 *  @brief Check if PTS is changing
 *  @retval true if PTS changed from lastKnown PTS or timeout hasn't expired, will optimistically return true if video-pts attribute is not available from decoder
 */
bool AAMPGstPlayer::CheckForPTSChangeWithTimeout(long timeout)
{
	return playerInstance->CheckForPTSChangeWithTimeout(timeout);
}
