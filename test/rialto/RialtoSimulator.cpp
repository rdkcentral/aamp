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
 * @file RialtoSimulator.cpp
 * @brief In-process Rialto server simulator for L2 integration tests.
 *
 * Implements IMediaPipeline, IMediaPipelineFactory, IControl, IControlFactory,
 * and IClientLogControl factories with enough behaviour for AAMP DirectRialto
 * code to progress through its state machine and report playback.
 *
 * When built without the "rialto" option, this library is linked into AAMP
 * instead of the real libRialtoClient.so.  It does not decode or render A/V
 * but it drives the IMediaPipelineClient callbacks (notifyPlaybackState,
 * notifyNeedMediaData, notifyPosition) so that AAMP behaves as if connected
 * to a real Rialto server.
 *
 * Log lines are printed with "[RialtoSim]" prefix so L2 tests can match them.
 */

#include "IMediaPipeline.h"
#include "IClientLogControl.h"
#include "IControl.h"
#include "IMediaKeys.h"
#include "IMediaPipelineCapabilities.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace firebolt::rialto
{

// ===========================================================================
// Logging helper
// ===========================================================================

#define RIALTO_SIM_LOG(fmt, ...) \
	fprintf(stderr, "[RialtoSim] " fmt "\n", ##__VA_ARGS__)

// ===========================================================================
// SimMediaPipeline - simulates the Rialto media pipeline
// ===========================================================================

class SimMediaPipeline : public IMediaPipeline
{
public:
	explicit SimMediaPipeline(
		std::weak_ptr<IMediaPipelineClient> client,
		const VideoRequirements &reqs)
		: m_client(client)
		, m_nextSourceId(1)
		, m_loaded(false)
		, m_allSourcesAttached(false)
		, m_playing(false)
		, m_basePositionNs(0)
		, m_basePositionSet(false)
		, m_needDataRequestId(1)
		, m_stopRequested(false)
	{
		RIALTO_SIM_LOG("SimMediaPipeline: created (width=%u height=%u)",
			reqs.maxWidth, reqs.maxHeight);
	}

	~SimMediaPipeline() override
	{
		stopThreads();
		RIALTO_SIM_LOG("SimMediaPipeline: destroyed");
	}

	// -- IMediaPipeline interface --

	std::weak_ptr<IMediaPipelineClient> getClient() override
	{
		return m_client;
	}

	bool load(MediaType type, const std::string &mimeType,
		const std::string &url, bool isLive) override
	{
		RIALTO_SIM_LOG("load: type=%d mime=%s url=%s isLive=%d",
			static_cast<int>(type), mimeType.c_str(), url.c_str(), isLive);
		m_loaded = true;
		return true;
	}

	bool attachSource(
		const std::unique_ptr<MediaSource> &source) override
	{
		int32_t id = m_nextSourceId++;
		const_cast<MediaSource &>(*source).setId(id);
		m_attachedSources.push_back(id);
		RIALTO_SIM_LOG("attachSource: assigned sourceId=%d type=%d",
			id, static_cast<int>(source->getType()));
		return true;
	}

	bool removeSource(int32_t id) override
	{
		RIALTO_SIM_LOG("removeSource: sourceId=%d", id);
		return true;
	}

	bool allSourcesAttached() override
	{
		RIALTO_SIM_LOG("allSourcesAttached: %zu sources",
			m_attachedSources.size());
		m_allSourcesAttached = true;
		startNeedDataPump();
		return true;
	}

	bool play(bool &async) override
	{
		RIALTO_SIM_LOG("play");
		async = false;
		m_playing = true;
		m_playStartTime = std::chrono::steady_clock::now();

		if (auto client = m_client.lock())
		{
			client->notifyPlaybackState(PlaybackState::PLAYING);
		}

		startPositionThread();
		return true;
	}

	bool pause() override
	{
		RIALTO_SIM_LOG("pause");
		m_playing = false;
		if (auto client = m_client.lock())
		{
			client->notifyPlaybackState(PlaybackState::PAUSED);
		}
		return true;
	}

	bool stop() override
	{
		RIALTO_SIM_LOG("stop");
		m_playing = false;
		stopThreads();
		if (auto client = m_client.lock())
		{
			client->notifyPlaybackState(PlaybackState::STOPPED);
		}
		return true;
	}

	bool setPlaybackRate(double rate) override
	{
		RIALTO_SIM_LOG("setPlaybackRate: rate=%f", rate);
		return true;
	}

	bool setPosition(int64_t position) override
	{
		RIALTO_SIM_LOG("setPosition: position=%ld", static_cast<long>(position));
		m_basePositionNs.store(position, std::memory_order_relaxed);
		m_basePositionSet.store(true, std::memory_order_relaxed);
		m_playStartTime = std::chrono::steady_clock::now();
		return true;
	}

	bool getPosition(int64_t &position) override
	{
		position = getCurrentPositionNs();
		return true;
	}

	bool getStats(int32_t, uint64_t &renderedFrames,
		uint64_t &droppedFrames) override
	{
		renderedFrames = 100;
		droppedFrames = 0;
		return true;
	}

	bool setImmediateOutput(int32_t, bool) override { return true; }
//	bool setReportDecodeErrors(int32_t, bool) override { return true; }
//	bool getQueuedFrames(int32_t, uint32_t &qf) override { qf = 0; return true; }
	bool getImmediateOutput(int32_t, bool &io) override { io = false; return true; }

	bool setVideoWindow(uint32_t x, uint32_t y,
		uint32_t width, uint32_t height) override
	{
		RIALTO_SIM_LOG("setVideoWindow: x=%u y=%u w=%u h=%u",
			x, y, width, height);
		return true;
	}

	bool haveData(MediaSourceStatus status,
		uint32_t needDataRequestId) override
	{
		RIALTO_SIM_LOG("haveData: status=%d requestId=%u",
			static_cast<int>(status), needDataRequestId);

		if (m_playing && status == MediaSourceStatus::OK)
		{
			scheduleNextNeedData();
		}
		return true;
	}

	AddSegmentStatus addSegment(uint32_t needDataRequestId,
		const std::unique_ptr<MediaSegment> &mediaSegment) override
	{
		// Capture the first segment PTS as the base position for
		// simulated playback.  Subsequent segments do NOT override —
		// position advances by elapsed wall-clock time from this base.
		if (mediaSegment && !m_basePositionSet.load(std::memory_order_relaxed))
		{
			int64_t pts = mediaSegment->getTimeStamp();
			if (pts > 0)
			{
				m_basePositionNs.store(pts, std::memory_order_relaxed);
				m_basePositionSet.store(true, std::memory_order_relaxed);
				m_playStartTime = std::chrono::steady_clock::now();
			}
		}
		return AddSegmentStatus::OK;
	}

	bool renderFrame() override
	{
		RIALTO_SIM_LOG("renderFrame");
		return true;
	}

	bool setVolume(double targetVolume, uint32_t volumeDuration,
		EaseType easeType) override
	{
		RIALTO_SIM_LOG("setVolume: vol=%f dur=%u", targetVolume, volumeDuration);
		return true;
	}

	bool getVolume(double &currentVolume) override
	{
		currentVolume = 1.0;
		return true;
	}

	bool setMute(int32_t sourceId, bool mute) override
	{
		RIALTO_SIM_LOG("setMute: sourceId=%d mute=%d", sourceId, mute);
		return true;
	}

	bool getMute(int32_t, bool &mute) override { mute = false; return true; }
	bool setTextTrackIdentifier(const std::string &) override { return true; }
	bool getTextTrackIdentifier(std::string &) override { return true; }
	bool setLowLatency(bool) override { return true; }
	bool setSync(bool) override { return true; }
	bool getSync(bool &sync) override { sync = true; return true; }
	bool setSyncOff(bool) override { return true; }
	bool setStreamSyncMode(int32_t, int32_t) override { return true; }
	bool getStreamSyncMode(int32_t &mode) override { mode = 0; return true; }

	bool flush(int32_t sourceId, bool resetTime, bool &async) override
	{
		RIALTO_SIM_LOG("flush: sourceId=%d resetTime=%d", sourceId, resetTime);
		async = false;
		// Reset position tracking for the next segment
		m_basePositionSet.store(false, std::memory_order_relaxed);
		return true;
	}

	bool setSourcePosition(int32_t sourceId, int64_t position,
		bool resetTime, double appliedRate,
		uint64_t stopPosition) override
	{
		RIALTO_SIM_LOG("setSourcePosition: sourceId=%d position=%ld resetTime=%d",
			sourceId, static_cast<long>(position), resetTime);
		if (position > 0)
		{
			m_basePositionNs.store(position, std::memory_order_relaxed);
			m_basePositionSet.store(true, std::memory_order_relaxed);
			m_playStartTime = std::chrono::steady_clock::now();
		}
		return true;
	}

	bool setSubtitleOffset(int32_t, int64_t) override { return true; }
	bool processAudioGap(int64_t, uint32_t, int64_t, bool) override { return true; }
	bool setBufferingLimit(uint32_t) override { return true; }
	bool getBufferingLimit(uint32_t &lim) override { lim = kInvalidLimitBuffering; return true; }
	bool setUseBuffering(bool) override { return true; }
	bool getUseBuffering(bool &ub) override { ub = false; return true; }

	bool switchSource(
		const std::unique_ptr<MediaSource> &source) override
	{
		RIALTO_SIM_LOG("switchSource: type=%d",
			static_cast<int>(source->getType()));
		return true;
	}

    bool getDuration(int64_t &duration) override { return 0; }

private:
	int64_t getCurrentPositionNs() const
	{
		int64_t base = m_basePositionNs.load(std::memory_order_relaxed);
		if (!m_playing)
		{
			return base;
		}
		auto elapsed = std::chrono::steady_clock::now() - m_playStartTime;
		auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
		return base + elapsedNs;
	}

	void startNeedDataPump()
	{
		if (auto client = m_client.lock())
		{
			for (int32_t sourceId : m_attachedSources)
			{
				uint32_t reqId = m_needDataRequestId++;
				RIALTO_SIM_LOG("notifyNeedMediaData: sourceId=%d requestId=%u",
					sourceId, reqId);
				client->notifyNeedMediaData(sourceId, 24, reqId, nullptr);
			}
		}
	}

	void scheduleNextNeedData()
	{
		std::thread([this]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			if (m_stopRequested.load(std::memory_order_relaxed))
			{
				return;
			}
			if (auto client = m_client.lock())
			{
				for (int32_t sourceId : m_attachedSources)
				{
					if (m_stopRequested.load(std::memory_order_relaxed))
					{
						break;
					}
					uint32_t reqId = m_needDataRequestId++;
					client->notifyNeedMediaData(sourceId, 24, reqId, nullptr);
				}
			}
		}).detach();
	}

	void startPositionThread()
	{
		if (m_positionThread.joinable())
		{
			return;
		}

		m_stopRequested.store(false, std::memory_order_relaxed);
		m_positionThread = std::thread([this]() {
			while (!m_stopRequested.load(std::memory_order_relaxed))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				if (m_stopRequested.load(std::memory_order_relaxed))
				{
					break;
				}
				if (!m_playing)
				{
					continue;
				}

				int64_t pos = getCurrentPositionNs();
				if (auto client = m_client.lock())
				{
					client->notifyPosition(pos);
				}
			}
		});
	}

	void stopThreads()
	{
		m_stopRequested.store(true, std::memory_order_relaxed);
		if (m_positionThread.joinable())
		{
			m_positionThread.join();
		}
	}

	std::weak_ptr<IMediaPipelineClient> m_client;
	std::atomic<int32_t> m_nextSourceId;
	std::vector<int32_t> m_attachedSources;
	bool m_loaded;
	bool m_allSourcesAttached;
	std::atomic<bool> m_playing;
	std::atomic<int64_t> m_basePositionNs;
	std::atomic<bool> m_basePositionSet;
	std::chrono::steady_clock::time_point m_playStartTime;
	std::atomic<uint32_t> m_needDataRequestId;
	std::atomic<bool> m_stopRequested;
	std::thread m_positionThread;
};

// ===========================================================================
// SimMediaPipelineFactory
// ===========================================================================

class SimMediaPipelineFactory : public IMediaPipelineFactory
{
public:
	std::unique_ptr<IMediaPipeline> createMediaPipeline(
		std::weak_ptr<IMediaPipelineClient> client,
		const VideoRequirements &videoRequirements) const override
	{
		RIALTO_SIM_LOG("createMediaPipeline");
		return std::make_unique<SimMediaPipeline>(client, videoRequirements);
	}
};

// ===========================================================================
// SimControl - immediately reports RUNNING to the registered client
// ===========================================================================

class SimControl : public IControl
{
public:
	bool registerClient(std::weak_ptr<IControlClient> client,
		ApplicationState &appState) override
	{
		appState = ApplicationState::RUNNING;
		RIALTO_SIM_LOG("IControl::registerClient: state=RUNNING");
		return true;
	}
};

class SimControlFactory : public IControlFactory
{
public:
	std::shared_ptr<IControl> createControl() const override
	{
		RIALTO_SIM_LOG("createControl");
		return std::make_shared<SimControl>();
	}
};

// ===========================================================================
// SimClientLogControl - no-op log control
// ===========================================================================

class SimClientLogControl : public IClientLogControl
{
public:
	bool registerLogHandler(const std::shared_ptr<IClientLogHandler> &,
		bool) override
	{
		return true;
	}
};

class SimClientLogControlFactory : public IClientLogControlFactory
{
public:
	IClientLogControl &createClientLogControl() override
	{
		static SimClientLogControl instance;
		return instance;
	}
};

// ===========================================================================
// Factory singletons
// ===========================================================================

std::shared_ptr<IMediaPipelineFactory> IMediaPipelineFactory::createFactory()
{
	RIALTO_SIM_LOG("IMediaPipelineFactory::createFactory");
	static auto factory = std::make_shared<SimMediaPipelineFactory>();
	return factory;
}

std::shared_ptr<IClientLogControlFactory> IClientLogControlFactory::createFactory()
{
	static auto factory = std::make_shared<SimClientLogControlFactory>();
	return factory;
}

std::shared_ptr<IControlFactory> IControlFactory::createFactory()
{
	RIALTO_SIM_LOG("IControlFactory::createFactory");
	static auto factory = std::make_shared<SimControlFactory>();
	return factory;
}

// ===========================================================================
// MediaSegment::copy - non-inline members declared in IMediaPipeline.h
// ===========================================================================

void IMediaPipeline::MediaSegment::copy(const MediaSegment &other)
{
	m_sourceId = other.m_sourceId;
	m_type = other.m_type;
	m_data = other.m_data;
	m_dataLength = other.m_dataLength;
	m_timeStamp = other.m_timeStamp;
	m_duration = other.m_duration;
	m_codecData = other.m_codecData;
	m_extraData = other.m_extraData;
	m_encrypted = other.m_encrypted;
	m_mediaKeySessionId = other.m_mediaKeySessionId;
	m_keyId = other.m_keyId;
	m_initVector = other.m_initVector;
	m_subSamples = other.m_subSamples;
	m_initWithLast15 = other.m_initWithLast15;
	m_alignment = other.m_alignment;
	m_cipherMode = other.m_cipherMode;
	m_crypt = other.m_crypt;
	m_skip = other.m_skip;
	m_encryptionPatternSet = other.m_encryptionPatternSet;
	m_displayOffset = other.m_displayOffset;
}

void IMediaPipeline::MediaSegmentAudio::copy(const MediaSegmentAudio &other)
{
	MediaSegment::copy(other);
	m_sampleRate = other.m_sampleRate;
	m_numberOfChannels = other.m_numberOfChannels;
	m_clippingStart = other.m_clippingStart;
	m_clippingEnd = other.m_clippingEnd;
}

void IMediaPipeline::MediaSegmentVideo::copy(const MediaSegmentVideo &other)
{
	MediaSegment::copy(other);
	m_width = other.m_width;
	m_height = other.m_height;
	m_frameRate = other.m_frameRate;
}

} // namespace firebolt::rialto

namespace firebolt::rialto
{

// Stub IMediaKeysFactory for builds that link RialtoSimulator instead of
// the real libRialtoClient.  The Rialto DRM path is not exercised in these
// builds, so returning nullptr is sufficient to satisfy the linker.
std::shared_ptr<IMediaKeysFactory> IMediaKeysFactory::createFactory()
{
    return nullptr;
}

} // namespace firebolt::rialto

namespace firebolt::rialto
{

// Stub IMediaPipelineCapabilitiesFactory for builds that link RialtoSimulator
// instead of the real libRialtoClient.  Returns a capabilities object whose
// isVideoMaster() reports false (audio-master), which is the safe default for
// simulator/test builds.
class SimMediaPipelineCapabilities : public IMediaPipelineCapabilities
{
public:
	std::vector<std::string> getSupportedMimeTypes(MediaSourceType) override
	{
		return {};
	}
	bool isMimeTypeSupported(const std::string &) override { return true; }
	std::vector<std::string> getSupportedProperties(
		MediaSourceType, const std::vector<std::string> &) override
	{
		return {};
	}
	bool isVideoMaster(bool &videoMaster) override
	{
		videoMaster = false;
		return true;
	}
};

class SimMediaPipelineCapabilitiesFactory
	: public IMediaPipelineCapabilitiesFactory
{
public:
	std::unique_ptr<IMediaPipelineCapabilities>
	createMediaPipelineCapabilities() const override
	{
		return std::make_unique<SimMediaPipelineCapabilities>();
	}
};

std::shared_ptr<IMediaPipelineCapabilitiesFactory>
IMediaPipelineCapabilitiesFactory::createFactory()
{
	static auto factory =
		std::make_shared<SimMediaPipelineCapabilitiesFactory>();
	return factory;
}

} // namespace firebolt::rialto

