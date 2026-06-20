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
 * @file AampRpcHandlers.cpp
 * @brief Implements every JSON-RPC handler by forwarding to
 *        PlayerInstanceAAMP. Uses jsoncpp (Json::Value) for all
 *        parameter access and result construction.
 */

#include "AampRpcHandlers.h"
#include "main_aamp.h"

#include <json/json.h>
#include <string>
#include <vector>

namespace AampRpcHandlers
{

/* â”€â”€ Internal helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

static void ok(Json::Value &result)
{
	result["success"] = true;
}

static void require(const Json::Value &params, const std::string &key)
{
	if (!params.isMember(key))
		throw AampRpcServer::RpcException{
			AampRpcServer::ERR_INVALID_PARAMS,
			"Missing required parameter: " + key
		};
}

static const char *stateToString(AAMPPlayerState s)
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

/* Helper: parse a JSON string returned by AAMP APIs into a Json::Value. */
static Json::Value parseJsonString(const std::string &s)
{
	Json::Value out;
	Json::CharReaderBuilder builder;
	std::string errs;
	std::istringstream iss(s);
	if (!Json::parseFromStream(builder, iss, &out, &errs))
		out = Json::Value(s); /* fall back to raw string on parse error */
	return out;
}

/* â”€â”€ Playback control â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleTune(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result)
{
	require(params, "url");

	const std::string url         = params["url"].asString();
	const bool        autoPlay    = params.get("autoPlay",    true).asBool();
	const std::string contentType = params.get("contentType", "").asString();
	const std::string traceUUID   = params.get("traceUUID",   "").asString();

	p->Tune(url.c_str(),
	        autoPlay,
	        contentType.empty() ? nullptr : contentType.c_str(),
	        /*bFirstAttempt=*/ true,
	        /*bFinalAttempt=*/ false,
	        traceUUID.empty()   ? nullptr : traceUUID.c_str());
	ok(result);
}

void HandleStop(PlayerInstanceAAMP *p, const Json::Value & /*params*/, Json::Value &result)
{
	p->Stop();
	ok(result);
}

void HandleSeek(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result)
{
	require(params, "position");
	const double pos        = params["position"].asDouble();
	const bool   keepPaused = params.get("keepPaused", false).asBool();
	p->Seek(pos, keepPaused);
	ok(result);
}

void HandleSeekToLive(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result)
{
	const bool keepPaused = params.get("keepPaused", false).asBool();
	p->SeekToLive(keepPaused);
	ok(result);
}

void HandleSetRate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result)
{
	require(params, "rate");
	const float rate               = params["rate"].asFloat();
	const int   overshootCorrection = params.get("overshootCorrection", 0).asInt();
	p->SetRate(rate, overshootCorrection);
	ok(result);
}

void HandleSetPlaybackSpeed(PlayerInstanceAAMP *p, const Json::Value &params,
                             Json::Value &result)
{
	require(params, "speed");
	p->SetPlaybackSpeed(params["speed"].asFloat());
	ok(result);
}

void HandlePauseAt(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result)
{
	require(params, "position");
	p->PauseAt(params["position"].asDouble());
	ok(result);
}

void HandleSetRateAndSeek(PlayerInstanceAAMP *p, const Json::Value &params,
                           Json::Value &result)
{
	require(params, "rate");
	require(params, "position");
	p->SetRateAndSeek(params["rate"].asInt(), params["position"].asDouble());
	ok(result);
}

/* â”€â”€ Playback state queries â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleGetState(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                     Json::Value &result)
{
	result["state"] = stateToString(p->GetState());
}

void HandleGetPlaybackPosition(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                                Json::Value &result)
{
	result["position"] = p->GetPlaybackPosition();
}

void HandleGetPlaybackDuration(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                                Json::Value &result)
{
	result["duration"] = p->GetPlaybackDuration();
}

void HandleGetPlaybackRate(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                            Json::Value &result)
{
	result["rate"] = p->GetPlaybackRate();
}

void HandleIsLive(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                   Json::Value &result)
{
	result["isLive"] = p->IsLive();
}

/* â”€â”€ Video â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleSetVideoRectangle(PlayerInstanceAAMP *p, const Json::Value &params,
                              Json::Value &result)
{
	require(params, "x");
	require(params, "y");
	require(params, "w");
	require(params, "h");
	p->SetVideoRectangle(params["x"].asInt(), params["y"].asInt(),
	                     params["w"].asInt(), params["h"].asInt());
	ok(result);
}

void HandleSetVideoMute(PlayerInstanceAAMP *p, const Json::Value &params,
                         Json::Value &result)
{
	require(params, "muted");
	p->SetVideoMute(params["muted"].asBool());
	ok(result);
}

void HandleGetVideoMute(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                         Json::Value &result)
{
	result["muted"] = p->GetVideoMute();
}

void HandleSetVideoZoom(PlayerInstanceAAMP *p, const Json::Value &params,
                         Json::Value &result)
{
	require(params, "zoom");
	p->SetVideoZoom(static_cast<VideoZoomMode>(params["zoom"].asInt()));
	ok(result);
}

/* â”€â”€ Audio â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleSetAudioVolume(PlayerInstanceAAMP *p, const Json::Value &params,
                           Json::Value &result)
{
	require(params, "volume");
	p->SetAudioVolume(params["volume"].asInt());
	ok(result);
}

void HandleGetAudioVolume(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                           Json::Value &result)
{
	result["volume"] = p->GetAudioVolume();
}

void HandleSetLanguage(PlayerInstanceAAMP *p, const Json::Value &params,
                        Json::Value &result)
{
	require(params, "language");
	const std::string lang = params["language"].asString();
	p->SetLanguage(lang.c_str());
	ok(result);
}

void HandleGetAudioLanguage(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                             Json::Value &result)
{
	result["language"] = p->GetAudioLanguage();
}

void HandleGetAvailableAudioTracks(PlayerInstanceAAMP *p, const Json::Value &params,
                                    Json::Value &result)
{
	const bool allTracks = params.get("allTracks", false).asBool();
	result["tracks"] = parseJsonString(p->GetAvailableAudioTracks(allTracks));
}

void HandleSetAudioTrack(PlayerInstanceAAMP *p, const Json::Value &params,
                          Json::Value &result)
{
	require(params, "trackId");
	p->SetAudioTrack(params["trackId"].asInt());
	ok(result);
}

void HandleGetAudioTrack(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                          Json::Value &result)
{
	result["trackId"] = p->GetAudioTrack();
}

void HandleGetAudioTrackInfo(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                              Json::Value &result)
{
	result["trackInfo"] = parseJsonString(p->GetAudioTrackInfo());
}

/* â”€â”€ Subtitles / Text tracks â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleSetSubtitleMute(PlayerInstanceAAMP *p, const Json::Value &params,
                            Json::Value &result)
{
	require(params, "muted");
	p->SetSubtitleMute(params["muted"].asBool());
	ok(result);
}

void HandleGetAvailableTextTracks(PlayerInstanceAAMP *p, const Json::Value &params,
                                   Json::Value &result)
{
	const bool allTracks = params.get("allTracks", false).asBool();
	result["tracks"] = parseJsonString(p->GetAvailableTextTracks(allTracks));
}

void HandleSetTextTrack(PlayerInstanceAAMP *p, const Json::Value &params,
                         Json::Value &result)
{
	require(params, "trackId");
	p->SetTextTrack(params["trackId"].asInt());
	ok(result);
}

void HandleGetTextTrack(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                         Json::Value &result)
{
	result["trackId"] = p->GetTextTrack();
}

/* â”€â”€ Bitrate / ABR â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleGetVideoBitrate(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                            Json::Value &result)
{
	result["bitrate"] = static_cast<Json::Int64>(p->GetVideoBitrate());
}

void HandleSetVideoBitrate(PlayerInstanceAAMP *p, const Json::Value &params,
                            Json::Value &result)
{
	require(params, "bitrate");
	p->SetVideoBitrate(static_cast<BitsPerSecond>(params["bitrate"].asInt64()));
	ok(result);
}

void HandleGetVideoBitrates(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                             Json::Value &result)
{
	std::vector<BitsPerSecond> brs = p->GetVideoBitrates();
	Json::Value arr(Json::arrayValue);
	for (auto b : brs)
		arr.append(static_cast<Json::Int64>(b));
	result["bitrates"] = arr;
}

void HandleSetInitialBitrate(PlayerInstanceAAMP *p, const Json::Value &params,
                              Json::Value &result)
{
	require(params, "bitrate");
	p->SetInitialBitrate(static_cast<BitsPerSecond>(params["bitrate"].asInt64()));
	ok(result);
}

void HandleGetInitialBitrate(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                              Json::Value &result)
{
	result["bitrate"] = static_cast<Json::Int64>(p->GetInitialBitrate());
}

void HandleSetMinimumBitrate(PlayerInstanceAAMP *p, const Json::Value &params,
                              Json::Value &result)
{
	require(params, "bitrate");
	p->SetMinimumBitrate(static_cast<BitsPerSecond>(params["bitrate"].asInt64()));
	ok(result);
}

void HandleGetMinimumBitrate(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                              Json::Value &result)
{
	result["bitrate"] = static_cast<Json::Int64>(p->GetMinimumBitrate());
}

void HandleSetMaximumBitrate(PlayerInstanceAAMP *p, const Json::Value &params,
                              Json::Value &result)
{
	require(params, "bitrate");
	p->SetMaximumBitrate(static_cast<BitsPerSecond>(params["bitrate"].asInt64()));
	ok(result);
}

void HandleGetMaximumBitrate(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                              Json::Value &result)
{
	result["bitrate"] = static_cast<Json::Int64>(p->GetMaximumBitrate());
}

/* â”€â”€ DRM â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleSetLicenseServerURL(PlayerInstanceAAMP *p, const Json::Value &params,
                                Json::Value &result)
{
	require(params, "url");
	const std::string url = params["url"].asString();
	p->SetLicenseServerURL(url.c_str());
	ok(result);
}

void HandleGetDRM(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                   Json::Value &result)
{
	result["drm"] = p->GetDRM();
}

void HandleSetPreferredDRM(PlayerInstanceAAMP *p, const Json::Value &params,
                            Json::Value &result)
{
	require(params, "drmType");
	const std::string drmStr = params["drmType"].asString();
	DRMSystems drm = eDRM_MAX_DRMSystems;
	if      (drmStr == "widevine")  drm = eDRM_WideVine;
	else if (drmStr == "playready") drm = eDRM_PlayReady;
	else if (drmStr == "clearkey")  drm = eDRM_ClearKey;
	else
	{
		throw AampRpcServer::RpcException{
			AampRpcServer::ERR_INVALID_PARAMS,
			"Unknown drmType: " + drmStr +
			". Valid values: widevine, playready, clearkey"
		};
	}
	p->SetPreferredDRM(drm);
	ok(result);
}

/* â”€â”€ Configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void HandleInitAAMPConfig(PlayerInstanceAAMP *p, const Json::Value &params,
                           Json::Value &result)
{
	require(params, "config");
	std::string configStr;
	if (params["config"].isString())
		configStr = params["config"].asString();
	else
	{
		Json::FastWriter writer;
		configStr = writer.write(params["config"]);
	}

	if (!p->InitAAMPConfig(configStr.c_str()))
	{
		throw AampRpcServer::RpcException{
			AampRpcServer::ERR_INTERNAL_ERROR,
			"InitAAMPConfig rejected the provided configuration"
		};
	}
	ok(result);
}

void HandleGetAAMPConfig(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                          Json::Value &result)
{
	result["config"] = parseJsonString(p->GetAAMPConfig());
}

void HandleSetAppName(PlayerInstanceAAMP *p, const Json::Value &params,
                       Json::Value &result)
{
	require(params, "name");
	p->SetAppName(params["name"].asString());
	ok(result);
}

void HandleSetPreferredLanguages(PlayerInstanceAAMP *p, const Json::Value &params,
                                  Json::Value &result)
{
	const std::string langList  = params.get("languageList",  "").asString();
	const std::string rendition = params.get("rendition",     "").asString();
	const std::string type      = params.get("type",          "").asString();
	const std::string codecList = params.get("codecList",     "").asString();
	const std::string labelList = params.get("labelList",     "").asString();

	p->SetPreferredLanguages(
		langList.empty()  ? nullptr : langList.c_str(),
		rendition.empty() ? nullptr : rendition.c_str(),
		type.empty()      ? nullptr : type.c_str(),
		codecList.empty() ? nullptr : codecList.c_str(),
		labelList.empty() ? nullptr : labelList.c_str()
	);
	ok(result);
}

void HandleGetPreferredLanguages(PlayerInstanceAAMP *p, const Json::Value & /*params*/,
                                  Json::Value &result)
{
	result["languageList"] = p->GetPreferredLanguages();
}

} // namespace AampRpcHandlers
