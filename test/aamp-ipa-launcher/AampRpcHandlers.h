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
 * @file AampRpcHandlers.h
 * @brief JSON-RPC method handler declarations.
 *
 * Each handler has the signature:
 *   nlohmann::json Handler(PlayerInstanceAAMP *player,
 *                          const nlohmann::json &params);
 *
 * On success it returns a JSON result object.
 * On error it throws AampRpcServer::RpcException.
 *
 * JSON-RPC ↔ PlayerInstanceAAMP mapping
 * ──────────────────────────────────────
 *  Playback Control
 *  ────────────────
 *  Player.Tune               → Tune(url, autoPlay, contentType)
 *  Player.Stop               → Stop()
 *  Player.Seek               → Seek(position, keepPaused)
 *  Player.SeekToLive         → SeekToLive(keepPaused)
 *  Player.SetRate            → SetRate(rate)
 *  Player.SetPlaybackSpeed   → SetPlaybackSpeed(speed)
 *  Player.PauseAt            → PauseAt(position)
 *  Player.SetRateAndSeek     → SetRateAndSeek(rate, position)
 *
 *  Playback State
 *  ──────────────
 *  Player.GetState             → GetState()
 *  Player.GetPlaybackPosition  → GetPlaybackPosition()
 *  Player.GetPlaybackDuration  → GetPlaybackDuration()
 *  Player.GetPlaybackRate      → GetPlaybackRate()
 *  Player.IsLive               → IsLive()
 *
 *  Video
 *  ─────
 *  Player.SetVideoRectangle  → SetVideoRectangle(x,y,w,h)
 *  Player.SetVideoMute       → SetVideoMute(muted)
 *  Player.GetVideoMute       → GetVideoMute()
 *  Player.SetVideoZoom       → SetVideoZoom(zoom)
 *
 *  Audio
 *  ─────
 *  Player.SetAudioVolume           → SetAudioVolume(volume)
 *  Player.GetAudioVolume           → GetAudioVolume()
 *  Player.SetLanguage              → SetLanguage(language)
 *  Player.GetAudioLanguage         → GetAudioLanguage()
 *  Player.GetAvailableAudioTracks  → GetAvailableAudioTracks()
 *  Player.SetAudioTrack            → SetAudioTrack(trackId)
 *  Player.GetAudioTrack            → GetAudioTrack()
 *  Player.GetAudioTrackInfo        → GetAudioTrackInfo()
 *
 *  Subtitles / Text Tracks
 *  ───────────────────────
 *  Player.SetSubtitleMute          → SetSubtitleMute(muted)
 *  Player.GetAvailableTextTracks   → GetAvailableTextTracks()
 *  Player.SetTextTrack             → SetTextTrack(trackId)
 *  Player.GetTextTrack             → GetTextTrack()
 *
 *  Bitrate / ABR
 *  ─────────────
 *  Player.GetVideoBitrate    → GetVideoBitrate()
 *  Player.SetVideoBitrate    → SetVideoBitrate(bitrate)
 *  Player.GetVideoBitrates   → GetVideoBitrates()
 *  Player.SetInitialBitrate  → SetInitialBitrate(bitrate)
 *  Player.GetInitialBitrate  → GetInitialBitrate()
 *  Player.SetMinimumBitrate  → SetMinimumBitrate(bitrate)
 *  Player.GetMinimumBitrate  → GetMinimumBitrate()
 *  Player.SetMaximumBitrate  → SetMaximumBitrate(bitrate)
 *  Player.GetMaximumBitrate  → GetMaximumBitrate()
 *
 *  DRM
 *  ───
 *  Player.SetLicenseServerURL  → SetLicenseServerURL(url)
 *  Player.GetDRM               → GetDRM()
 *  Player.SetPreferredDRM      → SetPreferredDRM(drmType)
 *
 *  Configuration
 *  ─────────────
 *  Player.InitAAMPConfig         → InitAAMPConfig(jsonStr)
 *  Player.GetAAMPConfig          → GetAAMPConfig()
 *  Player.SetAppName             → SetAppName(name)
 *  Player.SetPreferredLanguages  → SetPreferredLanguages(languageList, ...)
 *  Player.GetPreferredLanguages  → GetPreferredLanguages()
 */

#pragma once

#include "AampRpcServer.h"
#include <json/json.h>

class PlayerInstanceAAMP;

namespace AampRpcHandlers
{

/* Playback control */
void HandleTune(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleStop(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSeek(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSeekToLive(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetRate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetPlaybackSpeed(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandlePauseAt(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetRateAndSeek(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* State queries */
void HandleGetState(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetPlaybackPosition(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetPlaybackDuration(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetPlaybackRate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleIsLive(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* Video */
void HandleSetVideoRectangle(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetVideoMute(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetVideoMute(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetVideoZoom(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* Audio */
void HandleSetAudioVolume(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAudioVolume(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetLanguage(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAudioLanguage(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAvailableAudioTracks(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetAudioTrack(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAudioTrack(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAudioTrackInfo(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* Subtitles / Text tracks */
void HandleSetSubtitleMute(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAvailableTextTracks(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetTextTrack(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetTextTrack(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* Bitrate / ABR */
void HandleGetVideoBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetVideoBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetVideoBitrates(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetInitialBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetInitialBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetMinimumBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetMinimumBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetMaximumBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetMaximumBitrate(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* DRM */
void HandleSetLicenseServerURL(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetDRM(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetPreferredDRM(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

/* Configuration */
void HandleInitAAMPConfig(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetAAMPConfig(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetAppName(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleSetPreferredLanguages(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);
void HandleGetPreferredLanguages(PlayerInstanceAAMP *p, const Json::Value &params, Json::Value &result);

} // namespace AampRpcHandlers
