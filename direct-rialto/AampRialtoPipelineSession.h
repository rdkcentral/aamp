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
 * @file AampRialtoPipelineSession.h
 * @brief Owner of the Rialto objects behind one AampRialtoPlayer: the
 *        pipeline, its client, the factories, the capabilities and the
 *        control backend.
 */

#ifndef AAMP_RIALTO_PIPELINE_SESSION_H
#define AAMP_RIALTO_PIPELINE_SESSION_H

#include "AampRialtoMediaPipelineClient.h"
#include "IMediaPipeline.h"
#include "IMediaPipelineCapabilities.h"
#include "IClientLogHandler.h"
#include "IRialtoControlBackend.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

/**
 * @class AampRialtoPipelineSession
 * @brief Creates, owns and hands out the Rialto pipeline of one player.
 *
 * Only the pipeline pointer is shared between threads; GetPipeline() and
 * SetPipeline() are safe from any thread.  Everything else is touched from
 * the AAMP control thread (Configure(), Stop(), the destructor), as before it
 * was moved out of AampRialtoPlayer.
 *
 * GetPipeline() returns a snapshot.  Keep it only for the duration of one
 * call: Stop() must release the pipeline synchronously, so a snapshot kept in
 * a member or a long-lived lambda would keep the Rialto session alive.
 */
class AampRialtoPipelineSession
{
public:
	/// Handlers the player installs on the Rialto client before load().
	struct Callbacks
	{
		AampRialtoMediaPipelineClient::NeedDataCallback needData;
		AampRialtoMediaPipelineClient::CancelNeedDataCallback cancelNeedData;
		AampRialtoMediaPipelineClient::PlaybackStateCallback playbackState;
		AampRialtoMediaPipelineClient::PositionCallback position;
		AampRialtoMediaPipelineClient::DurationCallback duration;
		AampRialtoMediaPipelineClient::BufferUnderflowCallback bufferUnderflow;
		AampRialtoMediaPipelineClient::PlaybackErrorCallback playbackError;
	};

	/// Outcome of CreateAndLoad().
	enum class CreateResult
	{
		FactoryUnavailable, ///< No IMediaPipelineFactory (Rialto server missing).
		CreateFailed,       ///< createMediaPipeline() returned nullptr.
		LoadFailed,         ///< Created and kept, but load() failed.
		Loaded              ///< Created and loaded.
	};

	/// @param controlBackend  Optional; Rialto's IControl wrapper.  When null,
	///                        the first CreateAndLoad() creates the default.
	explicit AampRialtoPipelineSession(
		std::unique_ptr<IRialtoControlBackend> controlBackend = nullptr);

	/// Releases the pipeline and clears the client's callbacks, in that order.
	~AampRialtoPipelineSession();

	AampRialtoPipelineSession(const AampRialtoPipelineSession &) = delete;
	AampRialtoPipelineSession &operator=(const AampRialtoPipelineSession &) = delete;

	/// Snapshot of the current pipeline, or null.  Thread-safe.
	std::shared_ptr<firebolt::rialto::IMediaPipeline> GetPipeline() const;

	/// Replace the pipeline.  The previous one is released outside the lock.
	void SetPipeline(std::shared_ptr<firebolt::rialto::IMediaPipeline> pipeline);

	/// Create the Rialto client if it does not exist yet.
	void EnsureClient();

	/// Create the log bridge, factories, capabilities and control backend
	/// that are not there yet, wait for Rialto to report RUNNING, create the
	/// pipeline, install @p callbacks on the client and load().  The pipeline
	/// is kept (and GetPipeline() returns it) even when load() fails.
	CreateResult CreateAndLoad(const Callbacks &callbacks);

	/// Remove every handler from the client.  Only safe once the pipeline is
	/// released.
	void ClearClientCallbacks();

	/// Deregister from Rialto's IControl; the next CreateAndLoad() makes a new
	/// backend.
	void ReleaseControlBackend();

	/// Rialto's capabilities object, or null until CreateAndLoad() has run.
	firebolt::rialto::IMediaPipelineCapabilities *Capabilities() const
	{
		return m_capabilities.get();
	}

private:
	/// Bridges Rialto client log messages into AAMP's logging system.
	class LogHandler : public firebolt::rialto::IClientLogHandler
	{
	public:
		void log(
			Level level,
			const std::string &file,
			int line,
			const std::string &function,
			const std::string &message) override;
	};

	void RegisterLogHandler();
	void EnsureFactories();

	std::shared_ptr<LogHandler> m_logHandler;
	/// Null until CreateAndLoad() runs.
	std::shared_ptr<firebolt::rialto::IMediaPipelineFactory> m_factory;
	/// Created once so every user shares a single instance.
	std::unique_ptr<firebolt::rialto::IMediaPipelineCapabilities> m_capabilities;
	/// Waits for ApplicationState::RUNNING before the pipeline is created.
	std::unique_ptr<IRialtoControlBackend> m_controlBackend;
	std::shared_ptr<AampRialtoMediaPipelineClient> m_client;

	/// Guarded by m_pipelineMutex; read it only through GetPipeline().
	std::shared_ptr<firebolt::rialto::IMediaPipeline> m_pipeline;
	/// Leaf lock: held only to copy or swap m_pipeline, never across a call.
	mutable std::mutex m_pipelineMutex;
};

#endif // AAMP_RIALTO_PIPELINE_SESSION_H
