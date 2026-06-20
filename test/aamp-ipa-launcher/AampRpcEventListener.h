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
 * @file AampRpcEventListener.h
 * @brief Bridges AAMP C++ player events to JSON-RPC 2.0 notifications
 *        sent to all connected WebSocket clients.
 *
 * AAMP event → JSON-RPC notification mapping
 * ───────────────────────────────────────────
 *  AAMP_EVENT_TUNED              → Player.onTuned         {}
 *  AAMP_EVENT_TUNE_FAILED        → Player.onTuneFailed    {description, code, shouldRetry}
 *  AAMP_EVENT_STATE_CHANGED      → Player.onStateChanged  {state}
 *  AAMP_EVENT_PROGRESS           → Player.onProgress      {position, duration, speed,
 *                                                           start, end, videoBuffered,
 *                                                           audioBuffered, liveLatency}
 *  AAMP_EVENT_EOS                → Player.onEOS           {}
 *  AAMP_EVENT_SPEED_CHANGED      → Player.onSpeedChanged  {speed}
 *  AAMP_EVENT_BUFFERING_CHANGED  → Player.onBufferingChanged {buffering}
 *  AAMP_EVENT_SEEKED             → Player.onSeeked        {positionMs}
 *  AAMP_EVENT_BITRATE_CHANGED    → Player.onBitrateChanged {bitrate, width, height,
 *                                                            description}
 *  AAMP_EVENT_MEDIA_METADATA     → Player.onMediaMetadata {duration, width, height,
 *                                                           hasDrm, isLive, drmType,
 *                                                           languages, bitrates,
 *                                                           speeds}
 *  AAMP_EVENT_AUDIO_TRACKS_CHANGED → Player.onAudioTracksChanged {}
 *  AAMP_EVENT_TEXT_TRACKS_CHANGED  → Player.onTextTracksChanged  {}
 *  AAMP_EVENT_ENTERING_LIVE        → Player.onEnteringLive        {}
 *  AAMP_EVENT_DURATION_CHANGED     → Player.onDurationChanged     {duration}
 */

#pragma once

#include "AampEventListener.h"
#include "AampRpcServer.h"

/**
 * @class AampRpcEventListener
 * @brief Receives every AAMP event, serialises it to JSON, and broadcasts
 *        it as a JSON-RPC 2.0 notification over WebSocket.
 */
class AampRpcEventListener : public AAMPEventObjectListener
{
public:
	/**
	 * @param server Reference to the running AampRpcServer. The server
	 *               must outlive this listener.
	 */
	explicit AampRpcEventListener(AampRpcServer &server);
	~AampRpcEventListener() override = default;

	AampRpcEventListener(const AampRpcEventListener &)            = delete;
	AampRpcEventListener &operator=(const AampRpcEventListener &) = delete;

	/**
	 * @brief Called by AAMP for every subscribed event.
	 *        Serialises the event payload to JSON and broadcasts it.
	 */
	void Event(const AAMPEventPtr &event) override;

private:
	AampRpcServer &mServer;
};
