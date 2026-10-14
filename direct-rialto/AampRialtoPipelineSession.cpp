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
 * @file AampRialtoPipelineSession.cpp
 * @brief Implementation of AampRialtoPipelineSession.
 */

#include "AampRialtoPipelineSession.h"
#include "AampRialtoControlBackend.h"
#include "AampLogManager.h"
#include "IClientLogControl.h"

#include <utility>

namespace
{
	/// Upper bound for the wait on Rialto's application state transitioning
	/// to RUNNING.  In practice the transition completes in a few milliseconds;
	/// the timeout exists only to avoid a permanent hang if the Rialto server
	/// never reports RUNNING.
	constexpr int kRialtoRunningTimeoutMs = 2000;

	/// Rialto compares these only with its primary-video minimum, to choose
	/// the primary decoder; a smaller value would select the secondary one.
	constexpr std::uint32_t kMaxVideoWidth  = 3840;
	constexpr std::uint32_t kMaxVideoHeight = 2160;

	/// Rialto ignores the URL for MSE playback.
	constexpr const char *kMseUrl = "mse://1";

	/// Left false: true makes the server enable decoder rate correction, on top
	/// of AAMP's own live latency control.
	constexpr bool kPipelineIsLive = false;
}

// ---------------------------------------------------------------------------
// Rialto -> AAMP log bridge
// ---------------------------------------------------------------------------

void AampRialtoPipelineSession::LogHandler::log(
	Level level,
	const std::string &file,
	int line,
	const std::string &function,
	const std::string &message)
{
	const char *tag = "RialtoClient";
	switch (level)
	{
	case Level::Fatal:
	case Level::Error:
		AAMPLOG_ERR("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Warning:
		AAMPLOG_WARN("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Milestone:
		AAMPLOG_MIL("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Info:
		AAMPLOG_INFO("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	case Level::Debug:
	case Level::External:
	default:
		AAMPLOG_TRACE("%s [%s:%d %s] %s",
			tag, file.c_str(), line, function.c_str(), message.c_str());
		break;
	}
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

AampRialtoPipelineSession::AampRialtoPipelineSession(
	std::unique_ptr<IRialtoControlBackend> controlBackend)
	: m_controlBackend(std::move(controlBackend))
{
}

AampRialtoPipelineSession::~AampRialtoPipelineSession()
{
	SetPipeline(nullptr);
	ClearClientCallbacks();
}

// ---------------------------------------------------------------------------
// Pipeline access
// ---------------------------------------------------------------------------

std::shared_ptr<firebolt::rialto::IMediaPipeline>
AampRialtoPipelineSession::GetPipeline() const
{
	std::lock_guard<std::mutex> lock(m_pipelineMutex);
	return m_pipeline;
}

void AampRialtoPipelineSession::SetPipeline(
	std::shared_ptr<firebolt::rialto::IMediaPipeline> pipeline)
{
	std::shared_ptr<firebolt::rialto::IMediaPipeline> previous;
	{
		std::lock_guard<std::mutex> lock(m_pipelineMutex);
		previous = std::move(m_pipeline);
		m_pipeline = std::move(pipeline);
	}
}

// ---------------------------------------------------------------------------
// Client, backend and factories
// ---------------------------------------------------------------------------

void AampRialtoPipelineSession::EnsureClient()
{
	if (!m_client)
	{
		m_client = std::make_shared<AampRialtoMediaPipelineClient>();
	}
}

void AampRialtoPipelineSession::ClearClientCallbacks()
{
	if (m_client)
	{
		m_client->ClearCallbacks();
	}
}

void AampRialtoPipelineSession::ReleaseControlBackend()
{
	m_controlBackend.reset();
}

void AampRialtoPipelineSession::RegisterLogHandler()
{
	if (!m_logHandler)
	{
		m_logHandler = std::make_shared<LogHandler>();
		auto logControlFactory =
			firebolt::rialto::IClientLogControlFactory::createFactory();
		if (logControlFactory)
		{
			logControlFactory->createClientLogControl()
				.registerLogHandler(m_logHandler,
					/*ignoreLogLevels=*/true);
		}
		else
		{
			AAMPLOG_WARN("Failed to create IClientLogControlFactory - Rialto logs suppressed");
		}
	}
}

void AampRialtoPipelineSession::EnsureFactories()
{
	// Registering with Rialto's IControl is deferred to the first Configure()
	// call (mirrors rialto-gstreamer's PullModePlaybackDelegate, which creates
	// its ControlBackend on the NULL->READY state change) rather than at
	// construction time, so a player instance that is created but never
	// tuned does not register a client with the Rialto server.
	if (!m_controlBackend)
	{
		m_controlBackend = std::make_unique<AampRialtoControlBackend>();
	}
	if (!m_factory)
	{
		m_factory = firebolt::rialto::IMediaPipelineFactory::createFactory();
	}
	if (!m_capabilities)
	{
		auto capFactory =
			firebolt::rialto::IMediaPipelineCapabilitiesFactory::createFactory();
		if (capFactory)
		{
			m_capabilities = capFactory->createMediaPipelineCapabilities();
		}
		else
		{
			AAMPLOG_WARN("Failed to create IMediaPipelineCapabilitiesFactory");
		}
	}
}

// ---------------------------------------------------------------------------
// CreateAndLoad
// ---------------------------------------------------------------------------

AampRialtoPipelineSession::CreateResult
AampRialtoPipelineSession::CreateAndLoad(const Callbacks &callbacks)
{
	RegisterLogHandler();
	EnsureFactories();
	EnsureClient();

	if (!m_factory)
	{
		AAMPLOG_ERR("Failed to create IMediaPipelineFactory - is the Rialto server running?");
		return CreateResult::FactoryUnavailable;
	}

	// Wait for the Rialto server to report ApplicationState::RUNNING before
	// creating the media pipeline - ensures the proxy ctor sees RUNNING from
	// its internal registerClient() call, preventing NeedMediaData events
	// from being silently dropped.
	if (m_controlBackend && !m_controlBackend->waitForRunning(kRialtoRunningTimeoutMs))
	{
		AAMPLOG_WARN(
			"Proceeding to createMediaPipeline despite Rialto state not RUNNING");
	}

	firebolt::rialto::VideoRequirements kRequirements{kMaxVideoWidth, kMaxVideoHeight};
	std::shared_ptr<firebolt::rialto::IMediaPipeline> pipeline =
		m_factory->createMediaPipeline(
			std::weak_ptr<firebolt::rialto::IMediaPipelineClient>(m_client),
			kRequirements);
	SetPipeline(pipeline);
	if (!pipeline)
	{
		AAMPLOG_ERR("createMediaPipeline returned nullptr - check Rialto server logs (syslog) for details");
		return CreateResult::CreateFailed;
	}

	AAMPLOG_INFO("Created pipeline %p", pipeline.get());

	// Install before load(): events Rialto delivers while loading would
	// otherwise be dropped.
	m_client->SetNeedDataCallback(callbacks.needData);
	m_client->SetCancelNeedDataCallback(callbacks.cancelNeedData);
	m_client->SetPlaybackStateCallback(callbacks.playbackState);
	m_client->SetPositionCallback(callbacks.position);
	m_client->SetDurationCallback(callbacks.duration);
	m_client->SetBufferUnderflowCallback(callbacks.bufferUnderflow);
	m_client->SetPlaybackErrorCallback(callbacks.playbackError);

	if (!pipeline->load(
			firebolt::rialto::MediaType::MSE,
			"",
			kMseUrl,
			kPipelineIsLive))
	{
		AAMPLOG_ERR("load() failed - Rialto will reject attachSource calls");
		return CreateResult::LoadFailed;
	}

	AAMPLOG_INFO("load() succeeded");
	return CreateResult::Loaded;
}
