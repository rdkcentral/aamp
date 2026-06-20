/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file AampRpcEventListener.cpp
 * @brief Serialises every AAMP event to a JSON-RPC notification (jsoncpp)
 *        and hands it to AampRpcServer::Notify() for delivery to
 *        subscribed rpcserver clients.
 */

#include "AampRpcEventListener.h"
#include "AampEvent.h"

#include <json/json.h>
#include <memory>

static const char *stateStr(AAMPPlayerState s)
{
	switch (s)
	{
	case eSTATE_IDLE:         return "idle";
	case eSTATE_INITIALIZING: return "initializing";
	case eSTATE_INITIALIZED:  return "initialized";
	case eSTATE_PREPARING:    return "preparing";
	case eSTATE_PREPARED:     return "prepared";
	case eSTATE_BUFFERING:    return "buffering";
	case eSTATE_PAUSED:       return "paused";
	case eSTATE_SEEKING:      return "seeking";
	case eSTATE_PLAYING:      return "playing";
	case eSTATE_STOPPING:     return "stopping";
	case eSTATE_STOPPED:      return "stopped";
	case eSTATE_COMPLETE:     return "complete";
	case eSTATE_ERROR:        return "error";
	case eSTATE_RELEASED:     return "released";
	case eSTATE_BLOCKED:      return "blocked";
	default:                   return "unknown";
	}
}

AampRpcEventListener::AampRpcEventListener(AampRpcServer &server)
	: mServer(server)
{
}

void AampRpcEventListener::Event(const AAMPEventPtr &event)
{
	if (!event) return;

	Json::Value params(Json::objectValue);

	switch (event->getType())
	{
	case AAMP_EVENT_TUNED:
		mServer.Notify("Player.onTuned", params);
		break;

	case AAMP_EVENT_TUNE_FAILED:
	{
		auto e = std::dynamic_pointer_cast<MediaErrorEvent>(event);
		if (!e) break;
		params["description"] = e->getDescription();
		params["code"]        = e->getCode();
		params["shouldRetry"] = e->shouldRetry();
		mServer.Notify("Player.onTuneFailed", params);
		break;
	}

	case AAMP_EVENT_STATE_CHANGED:
	{
		auto e = std::dynamic_pointer_cast<StateChangedEvent>(event);
		if (!e) break;
		params["state"] = stateStr(e->getState());
		mServer.Notify("Player.onStateChanged", params);
		break;
	}

	case AAMP_EVENT_PROGRESS:
	{
		auto e = std::dynamic_pointer_cast<ProgressEvent>(event);
		if (!e) break;
		params["positionMs"]      = e->getPosition();
		params["durationMs"]      = e->getDuration();
		params["speed"]           = e->getSpeed();
		params["startMs"]         = e->getStart();
		params["endMs"]           = e->getEnd();
		params["videoBufferedMs"] = e->getVideoBufferedDuration();
		params["audioBufferedMs"] = e->getAudioBufferedDuration();
		params["liveLatencyMs"]   = e->getLiveLatency();
		params["profileBitrate"]  = static_cast<Json::Int64>(e->getProfileBandwidth());
		params["networkBitrate"]  = static_cast<Json::Int64>(e->getNetworkBandwidth());
		mServer.Notify("Player.onProgress", params);
		break;
	}

	case AAMP_EVENT_EOS:
		mServer.Notify("Player.onEOS", params);
		break;

	case AAMP_EVENT_SPEED_CHANGED:
	{
		auto e = std::dynamic_pointer_cast<SpeedChangedEvent>(event);
		if (!e) break;
		params["speed"] = e->getRate();
		mServer.Notify("Player.onSpeedChanged", params);
		break;
	}

	case AAMP_EVENT_BUFFERING_CHANGED:
	{
		auto e = std::dynamic_pointer_cast<BufferingChangedEvent>(event);
		if (!e) break;
		params["buffering"] = e->buffering();
		mServer.Notify("Player.onBufferingChanged", params);
		break;
	}

	case AAMP_EVENT_SEEKED:
	{
		auto e = std::dynamic_pointer_cast<SeekedEvent>(event);
		if (!e) break;
		params["positionMs"] = e->getPosition();
		mServer.Notify("Player.onSeeked", params);
		break;
	}

	case AAMP_EVENT_BITRATE_CHANGED:
	{
		auto e = std::dynamic_pointer_cast<BitrateChangeEvent>(event);
		if (!e) break;
		params["bitrate"]     = static_cast<Json::Int64>(e->getBitrate());
		params["width"]       = e->getWidth();
		params["height"]      = e->getHeight();
		params["description"] = e->getDescription();
		mServer.Notify("Player.onBitrateChanged", params);
		break;
	}

	case AAMP_EVENT_MEDIA_METADATA:
	{
		auto e = std::dynamic_pointer_cast<MediaMetadataEvent>(event);
		if (!e) break;

		Json::Value languages(Json::arrayValue);
		for (const auto &lang : e->getLanguages())
			languages.append(lang);

		Json::Value bitrates(Json::arrayValue);
		for (auto b : e->getBitrates())
			bitrates.append(static_cast<Json::Int64>(b));

		Json::Value speeds(Json::arrayValue);
		for (auto s : e->getSupportedSpeeds())
			speeds.append(s);

		params["durationMs"]  = static_cast<Json::Int64>(e->getDuration());
		params["width"]       = e->getWidth();
		params["height"]      = e->getHeight();
		params["hasDrm"]      = e->hasDrm();
		params["isLive"]      = e->isLive();
		params["drmType"]     = e->getDrmType();
		params["languages"]   = languages;
		params["bitrates"]    = bitrates;
		params["speeds"]      = speeds;
		mServer.Notify("Player.onMediaMetadata", params);
		break;
	}

	case AAMP_EVENT_AUDIO_TRACKS_CHANGED:
		mServer.Notify("Player.onAudioTracksChanged", params);
		break;

	case AAMP_EVENT_TEXT_TRACKS_CHANGED:
		mServer.Notify("Player.onTextTracksChanged", params);
		break;

	case AAMP_EVENT_ENTERING_LIVE:
		mServer.Notify("Player.onEnteringLive", params);
		break;

	case AAMP_EVENT_DURATION_CHANGED:
		mServer.Notify("Player.onDurationChanged", params);
		break;

	case AAMP_EVENT_DRM_METADATA:
	{
		auto e = std::dynamic_pointer_cast<DrmMetaDataEvent>(event);
		if (!e) break;
		params["accessStatus"] = e->getAccessStatus();
		mServer.Notify("Player.onDrmMetadata", params);
		break;
	}

	case AAMP_EVENT_AD_STARTED:
		mServer.Notify("Player.onAdStarted", params);
		break;

	case AAMP_EVENT_AD_COMPLETED:
		mServer.Notify("Player.onAdCompleted", params);
		break;

	case AAMP_EVENT_AD_RESERVATION_START:
		mServer.Notify("Player.onAdReservationStart", params);
		break;

	case AAMP_EVENT_AD_RESERVATION_END:
		mServer.Notify("Player.onAdReservationEnd", params);
		break;

	case AAMP_EVENT_REPORT_ANOMALY:
	{
		auto e = std::dynamic_pointer_cast<AnomalyReportEvent>(event);
		if (!e) break;
		params["severity"] = e->getSeverity();
		params["message"]  = e->getMessage();
		mServer.Notify("Player.onAnomaly", params);
		break;
	}

	default:
		break;
	}
}


