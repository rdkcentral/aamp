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
 * @file AampRpcServer.cpp
 * @brief Implementation of AampRpcServer using the rpcserver library
 *        (websocketpp + libjsonrpccpp) as the WebSocket/JSON-RPC transport.
 */

#include "AampRpcServer.h"
#include "AampRpcHandlers.h"
#include "main_aamp.h"

#include <rpcserver/WsRpcServerBuilder.h>
#include <rpcserver/IAbstractRpcServer.h>

#include <cstdio>
#include <stdexcept>

#define RPC_LOG(fmt, ...) \
	fprintf(stdout, "[aamp-ipa-launcher] " fmt "\n", ##__VA_ARGS__)
#define RPC_ERR(fmt, ...) \
	fprintf(stderr, "[aamp-ipa-launcher][ERR] " fmt "\n", ##__VA_ARGS__)

/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
/* Construction / Destruction                                                  */
/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

AampRpcServer::AampRpcServer(PlayerInstanceAAMP *player)
	: mPlayer(player)
{
}

AampRpcServer::~AampRpcServer()
{
	Stop();
}

/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
/* Public API                                                                  */
/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

bool AampRpcServer::Start(int port)
{
	mRpcServer.reset(
		rpcserver::WsRpcServerBuilder(static_cast<uint16_t>(port))
			.enableServerEvents(
				"Player.subscribe",
				"Player.unsubscribe",
				"Player.getSubscriptions")
			.build()
	);

	RegisterMethods();

	if (!mRpcServer->StartListening())
	{
		RPC_ERR("StartListening failed on port %d", port);
		mRpcServer.reset();
		return false;
	}

	RPC_LOG("JSON-RPC WebSocket server listening on ws://0.0.0.0:%d", port);
	RPC_LOG("Subscribe to events with: Player.subscribe {\"event\":\"Player.onProgress\"}");
	return true;
}

void AampRpcServer::Stop()
{
	if (mRpcServer)
	{
		mRpcServer->StopListening();
		mRpcServer.reset();
		RPC_LOG("Server stopped.");
	}
}

void AampRpcServer::Notify(const std::string &method, const Json::Value &params)
{
	if (mRpcServer)
		mRpcServer->onEvent(method, params);
}

/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
/* Method registration                                                         */
/* â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

void AampRpcServer::RegisterMethods()
{
	using namespace AampRpcHandlers;

	/* Helper: wrap a HandlerFn (which takes PlayerInstanceAAMP*) into the
	 * rpcserver bindMethod signature (which takes only params + result). */
	auto bind = [&](const std::string &name, HandlerFn fn)
	{
		mRpcServer->bindMethod(name,
			[this, fn](const Json::Value &params, Json::Value &result)
			{
				fn(mPlayer, params, result);
			});
	};

	/* Playback control */
	bind("Player.Tune",             HandleTune);
	bind("Player.Stop",             HandleStop);
	bind("Player.Seek",             HandleSeek);
	bind("Player.SeekToLive",       HandleSeekToLive);
	bind("Player.SetRate",          HandleSetRate);
	bind("Player.SetPlaybackSpeed", HandleSetPlaybackSpeed);
	bind("Player.PauseAt",          HandlePauseAt);
	bind("Player.SetRateAndSeek",   HandleSetRateAndSeek);

	/* Playback state queries */
	bind("Player.GetState",             HandleGetState);
	bind("Player.GetPlaybackPosition",  HandleGetPlaybackPosition);
	bind("Player.GetPlaybackDuration",  HandleGetPlaybackDuration);
	bind("Player.GetPlaybackRate",      HandleGetPlaybackRate);
	bind("Player.IsLive",               HandleIsLive);

	/* Video */
	bind("Player.SetVideoRectangle", HandleSetVideoRectangle);
	bind("Player.SetVideoMute",      HandleSetVideoMute);
	bind("Player.GetVideoMute",      HandleGetVideoMute);
	bind("Player.SetVideoZoom",      HandleSetVideoZoom);

	/* Audio */
	bind("Player.SetAudioVolume",          HandleSetAudioVolume);
	bind("Player.GetAudioVolume",          HandleGetAudioVolume);
	bind("Player.SetLanguage",             HandleSetLanguage);
	bind("Player.GetAudioLanguage",        HandleGetAudioLanguage);
	bind("Player.GetAvailableAudioTracks", HandleGetAvailableAudioTracks);
	bind("Player.SetAudioTrack",           HandleSetAudioTrack);
	bind("Player.GetAudioTrack",           HandleGetAudioTrack);
	bind("Player.GetAudioTrackInfo",       HandleGetAudioTrackInfo);

	/* Subtitles / Text tracks */
	bind("Player.SetSubtitleMute",         HandleSetSubtitleMute);
	bind("Player.GetAvailableTextTracks",  HandleGetAvailableTextTracks);
	bind("Player.SetTextTrack",            HandleSetTextTrack);
	bind("Player.GetTextTrack",            HandleGetTextTrack);

	/* Bitrate / ABR */
	bind("Player.GetVideoBitrate",    HandleGetVideoBitrate);
	bind("Player.SetVideoBitrate",    HandleSetVideoBitrate);
	bind("Player.GetVideoBitrates",   HandleGetVideoBitrates);
	bind("Player.SetInitialBitrate",  HandleSetInitialBitrate);
	bind("Player.GetInitialBitrate",  HandleGetInitialBitrate);
	bind("Player.SetMinimumBitrate",  HandleSetMinimumBitrate);
	bind("Player.GetMinimumBitrate",  HandleGetMinimumBitrate);
	bind("Player.SetMaximumBitrate",  HandleSetMaximumBitrate);
	bind("Player.GetMaximumBitrate",  HandleGetMaximumBitrate);

	/* DRM */
	bind("Player.SetLicenseServerURL", HandleSetLicenseServerURL);
	bind("Player.GetDRM",              HandleGetDRM);
	bind("Player.SetPreferredDRM",     HandleSetPreferredDRM);

	/* Configuration */
	bind("Player.InitAAMPConfig",         HandleInitAAMPConfig);
	bind("Player.GetAAMPConfig",          HandleGetAAMPConfig);
	bind("Player.SetAppName",             HandleSetAppName);
	bind("Player.SetPreferredLanguages",  HandleSetPreferredLanguages);
	bind("Player.GetPreferredLanguages",  HandleGetPreferredLanguages);
}

