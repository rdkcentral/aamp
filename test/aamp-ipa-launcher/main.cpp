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
 * @file main.cpp
 * @brief aamp-ipa-launcher entry point.
 *
 * What this does
 * ──────────────
 *  1. Parses command-line arguments (--port, --app-name).
 *  2. Creates a GLib main loop on a background thread (required by
 *     GStreamer which PlayerInstanceAAMP uses internally).
 *  3. Creates a PlayerInstanceAAMP instance (loads libaamp at link time).
 *  4. Registers an AampRpcEventListener so every AAMP event is forwarded
 *     as a JSON-RPC 2.0 notification to subscribed rpcserver clients.
 *  5. Starts the AampRpcServer (backed by the rpcserver library /
 *     websocketpp + libjsonrpccpp).
 *  6. Blocks until SIGINT or SIGTERM, then shuts down cleanly.
 *
 * Usage
 * ─────
 *  aamp-ipa-launcher [--port <N>] [--app-name <name>]
 *
 *  Default port: 9999
 *
 * Test with wscat (npm install -g wscat):
 *  wscat -c ws://localhost:9999
 *  > {"jsonrpc":"2.0","id":1,"method":"Player.Tune","params":{"url":"https://...mpd"}}
 *  > {"jsonrpc":"2.0","id":2,"method":"Player.subscribe","params":{"event":"Player.onProgress"}}
 */

#include "AampRpcServer.h"
#include "AampRpcEventListener.h"
#include "main_aamp.h"

#include <glib.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <string>
#include <thread>

/* ─────────────────────────────────────────────────────────────────────────── */
/* Signal handling                                                             */
/* ─────────────────────────────────────────────────────────────────────────── */

static GMainLoop          *gGMainLoop   = nullptr;
static std::atomic<bool>   gShutdown{false};

static void OnSignal(int /*sig*/)
{
	gShutdown = true;
	if (gGMainLoop)
		g_main_loop_quit(gGMainLoop);
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* GLib main loop thread (GStreamer requires a running GLib main loop)        */
/* ─────────────────────────────────────────────────────────────────────────── */

static gpointer GLibMainLoopThread(gpointer /*arg*/)
{
	gGMainLoop = g_main_loop_new(nullptr, FALSE);
	g_main_loop_run(gGMainLoop);
	g_main_loop_unref(gGMainLoop);
	gGMainLoop = nullptr;
	return nullptr;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* Argument parsing                                                            */
/* ─────────────────────────────────────────────────────────────────────────── */

struct Config
{
	int         port{9999};
	std::string appName{"aamp-ipa-launcher"};
};

static Config ParseArgs(int argc, char *argv[])
{
	Config cfg;
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
		{
			cfg.port = std::atoi(argv[++i]);
		}
		else if (std::strcmp(argv[i], "--app-name") == 0 && i + 1 < argc)
		{
			cfg.appName = argv[++i];
		}
		else if (std::strcmp(argv[i], "--help") == 0 ||
		         std::strcmp(argv[i], "-h") == 0)
		{
			std::fprintf(stdout,
				"Usage: aamp-ipa-launcher [options]\n"
				"  --port <N>       JSON-RPC WebSocket port (default: 9999)\n"
				"  --app-name <s>   Application name reported to AAMP\n"
				"  --help           Show this help\n"
				"\n"
				"JSON-RPC endpoint: ws://0.0.0.0:<port>\n"
				"Subscribe to events: Player.subscribe {\"event\":\"Player.onProgress\"}\n");
			std::exit(0);
		}
	}
	return cfg;
}

/* ─────────────────────────────────────────────────────────────────────────── */
/* main                                                                        */
/* ─────────────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	const Config cfg = ParseArgs(argc, argv);

	/* ── Install signal handlers ─────────────────────────────────────── */
	std::signal(SIGINT,  OnSignal);
	std::signal(SIGTERM, OnSignal);

	/* ── Start GLib main loop (GStreamer requirement) ─────────────────── */
	GThread *glibThread = g_thread_new("glib-main", GLibMainLoopThread, nullptr);
	if (!glibThread)
	{
		std::fprintf(stderr, "[aamp-ipa-launcher] Failed to start GLib loop\n");
		return 1;
	}

	/* ── Create AAMP player instance ─────────────────────────────────── */
	/*
	 * Pass nullptr for streamSink to use AAMP's default GStreamer sink.
	 * On headless / embedded targets, you may provide a custom StreamSink.
	 */
	PlayerInstanceAAMP player;
	player.SetAppName(cfg.appName);

	std::fprintf(stdout,
		"[aamp-ipa-launcher] PlayerInstanceAAMP created (app: %s)\n",
		cfg.appName.c_str());

	/* ── Create JSON-RPC server ───────────────────────────────────────── */
	AampRpcServer server(&player);

	/* ── Create event listener and subscribe to ALL events ───────────── */
	AampRpcEventListener listener(server);
	player.RegisterEvents(&listener);

	/* ── Start WebSocket server ───────────────────────────────────────── */
	if (!server.Start(cfg.port))
	{
		std::fprintf(stderr,
			"[aamp-ipa-launcher] Failed to start server on port %d\n",
			cfg.port);
		player.UnRegisterEvents(&listener);
		return 1;
	}

	std::fprintf(stdout,
		"[aamp-ipa-launcher] Ready. Connect with:\n"
		"  wscat -c ws://localhost:%d\n"
		"  then: Player.subscribe {\"event\":\"Player.onProgress\"}\n"
		"  Press Ctrl+C to stop.\n",
		cfg.port);

	/* ── Block until shutdown signal ─────────────────────────────────── */
	while (!gShutdown)
	{
		/* Sleep in 100 ms intervals so the signal handler can set gShutdown. */
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	/* ── Graceful shutdown ────────────────────────────────────────────── */
	std::fprintf(stdout, "\n[aamp-ipa-launcher] Shutting down...\n");

	player.UnRegisterEvents(&listener);
	player.Stop();

	server.Stop();

	/* Stop GLib loop (may already have been stopped by the signal). */
	if (gGMainLoop)
		g_main_loop_quit(gGMainLoop);
	g_thread_join(glibThread);

	std::fprintf(stdout, "[aamp-ipa-launcher] Done.\n");
	return 0;
}
