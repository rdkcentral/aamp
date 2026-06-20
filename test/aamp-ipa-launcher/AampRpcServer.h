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
 * @file AampRpcServer.h
 * @brief WebSocket JSON-RPC 2.0 server that maps JSON-RPC methods to
 *        PlayerInstanceAAMP C++ calls and forwards AAMP events as
 *        JSON-RPC notifications to all connected clients.
 *
 * Threading model
 * ───────────────
 *  • Service thread  – runs the libwebsockets event loop.
 *    All lws API calls (lws_write, lws_callback_on_writable) are made
 *    exclusively from this thread.
 *  • Any thread      – may call BroadcastNotification().
 *    It pushes to a mutex-protected queue and wakes the service thread
 *    with lws_cancel_service(), which is the only lws function safe to
 *    call from an external thread.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <json/json.h>

namespace rpcserver { class IAbstractRpcServer; }
class PlayerInstanceAAMP;

/**
 * @class AampRpcServer
 * @brief Thin WebSocket / JSON-RPC 2.0 server layer over PlayerInstanceAAMP.
 */
class AampRpcServer
{
public:
	/**
	 * @brief Handler function signature.
	 *
	 * Each handler receives the AAMP player instance, the JSON-RPC
	 * "params" object, and a writable result to fill.
	 * Throw RpcException on error.
	 */
	using HandlerFn = std::function<
		void(PlayerInstanceAAMP *, const Json::Value &, Json::Value &)>;

	/**
	 * @brief Structured exception thrown by handlers to produce a
	 *        JSON-RPC error response.
	 */
	struct RpcException
	{
		int         code;     /**< JSON-RPC error code (use standard -326xx values) */
		std::string message;  /**< Human-readable error description */
	};

	/* Standard JSON-RPC 2.0 error codes */
	static constexpr int ERR_PARSE_ERROR      = -32700;
	static constexpr int ERR_INVALID_REQUEST  = -32600;
	static constexpr int ERR_METHOD_NOT_FOUND = -32601;
	static constexpr int ERR_INVALID_PARAMS   = -32602;
	static constexpr int ERR_INTERNAL_ERROR   = -32603;

	explicit AampRpcServer(PlayerInstanceAAMP *player);
	~AampRpcServer();

	AampRpcServer(const AampRpcServer &)            = delete;
	AampRpcServer &operator=(const AampRpcServer &) = delete;

	/**
	 * @brief Build the rpcserver instance and start listening.
	 * @param port TCP port (default 9999).
	 * @return true on success.
	 */
	bool Start(int port = 9999);

	/**
	 * @brief Stop the server.
	 */
	void Stop();

	/**
	 * @brief Push an event notification to all subscribed clients.
	 *        Thread-safe (rpcserver handles its own locking).
	 * @param method  Event name (e.g. "Player.onProgress").
	 * @param params  Event payload.
	 */
	void Notify(const std::string &method, const Json::Value &params);

private:
	/** Bind all Player.* methods to mRpcServer. */
	void RegisterMethods();

	PlayerInstanceAAMP                             *mPlayer{nullptr};
	std::unique_ptr<rpcserver::IAbstractRpcServer>  mRpcServer;
};
