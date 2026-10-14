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

#ifndef AAMP_RIALTO_MEDIA_PIPELINE_CLIENT_H
#define AAMP_RIALTO_MEDIA_PIPELINE_CLIENT_H

#include "IMediaPipeline.h"
#include <functional>
#include <mutex>
#include <memory>
#include <utility>
#include <vector>

/**
 * @brief Payload delivered by a notifyNeedMediaData callback.
 */
struct NeedDataRequestEvent
{
	int32_t sourceId;
	uint32_t requestId;
	size_t frameCount;
};

/**
 * @class AampRialtoMediaPipelineClient
 * @brief IMediaPipelineClient implementation that forwards need-data
 *        and cancel-need-data events via installed callbacks.
 */
class AampRialtoMediaPipelineClient
	: public firebolt::rialto::IMediaPipelineClient
{
public:
	/// Callback invoked when Rialto requests media data.
	/// Parameters: sourceId, frameCount, needDataRequestId
	using NeedDataCallback =
		std::function<void(int32_t, size_t, uint32_t)>;

	/// Callback invoked when Rialto cancels a pending need-data request.
	/// Parameter: sourceId
	using CancelNeedDataCallback = std::function<void(int32_t)>;

	/// Callback invoked when the Rialto server transitions to a new state.
	/// Parameter: PlaybackState
	using PlaybackStateCallback =
		std::function<void(firebolt::rialto::PlaybackState)>;

	/// Callback invoked when Rialto reports current position.
	/// Parameter: position in nanoseconds
	using PositionCallback = std::function<void(int64_t)>;

	/// Callback invoked when Rialto reports stream duration.
	/// Parameter: duration in nanoseconds
	using DurationCallback = std::function<void(int64_t)>;

	/// Callback invoked when Rialto reports that a media source has
	/// exhausted its internal buffer.
	/// Parameter: sourceId
	using BufferUnderflowCallback = std::function<void(int32_t)>;

	/// Callback invoked when Rialto confirms a source flush is complete.
	/// Parameter: sourceId
	using SourceFlushedCallback = std::function<void(int32_t)>;

	/// Callback invoked when Rialto reports a non-fatal playback error.
	/// Parameters: sourceId, PlaybackError
	using PlaybackErrorCallback =
		std::function<void(int32_t, firebolt::rialto::PlaybackError)>;

	AampRialtoMediaPipelineClient();
	~AampRialtoMediaPipelineClient() override;

	/// @brief Install callback for notifyNeedMediaData events.
	void SetNeedDataCallback(NeedDataCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_needDataCallback = std::move(cb);
	}

	/// @brief Install callback for notifyCancelNeedMediaData events.
	void SetCancelNeedDataCallback(CancelNeedDataCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_cancelNeedDataCallback = std::move(cb);
	}

	/// @brief Install callback for notifyPlaybackState events.
	void SetPlaybackStateCallback(PlaybackStateCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_playbackStateCallback = std::move(cb);
	}

	/// @brief Install callback for notifyPosition events.
	void SetPositionCallback(PositionCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_positionCallback = std::move(cb);
	}

	/// @brief Install callback for notifyDuration events.
	void SetDurationCallback(DurationCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_durationCallback = std::move(cb);
	}

	/// @brief Install callback for notifyBufferUnderflow events.
	void SetBufferUnderflowCallback(BufferUnderflowCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_bufferUnderflowCallback = std::move(cb);
	}

	/// @brief Install callback for notifySourceFlushed events.
	void SetSourceFlushedCallback(SourceFlushedCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_sourceFlushedCallback = std::move(cb);
	}

	/// @brief Install callback for notifyPlaybackError events.
	void SetPlaybackErrorCallback(PlaybackErrorCallback cb)
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_playbackErrorCallback = std::move(cb);
	}

	/// @brief Remove every installed callback.
	void ClearCallbacks()
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		m_needDataCallback = nullptr;
		m_cancelNeedDataCallback = nullptr;
		m_playbackStateCallback = nullptr;
		m_positionCallback = nullptr;
		m_durationCallback = nullptr;
		m_bufferUnderflowCallback = nullptr;
		m_sourceFlushedCallback = nullptr;
		m_playbackErrorCallback = nullptr;
	}

	// IMediaPipelineClient Implementation (All required pure virtuals)
	void notifyNetworkState(firebolt::rialto::NetworkState state) override;
	void notifyPlaybackState(firebolt::rialto::PlaybackState state) override;
	void notifyPosition(int64_t position) override;
	void notifyNeedMediaData(
		int32_t sourceId,
		size_t frameCount,
		uint32_t needDataRequestId,
		const std::shared_ptr<firebolt::rialto::MediaPlayerShmInfo>
			&mediaPlayerShmInfo) override;
	void notifyQos(
		int32_t sourceId,
		const firebolt::rialto::QosInfo &qosInfo) override;
	void notifyBufferUnderflow(int32_t sourceId) override;
	void notifyDuration(int64_t duration) override;
	void notifyNativeSize(
		uint32_t width, uint32_t height,
		double aspect = 1.0) override;
	void notifyVideoData(bool hasData) override;
	void notifyAudioData(bool hasData) override;
	void notifyCancelNeedMediaData(int32_t sourceId) override;
	void notifyPlaybackError(
		int32_t sourceId,
		firebolt::rialto::PlaybackError error) override;
	void notifySourceFlushed(int32_t sourceId) override;
	void notifyPlaybackInfo(
		const firebolt::rialto::PlaybackInfo &playbackInfo) override;
	void notifyFirstFrameReceived(int32_t sourceId) override;

private:
	/// Copy @p cb under the lock so it can be invoked without holding it: the
	/// setters run on the control thread while Rialto delivers events on its
	/// own thread, and a callback may itself call a setter.
	template <typename Callback>
	Callback Snapshot(const Callback &cb) const
	{
		std::lock_guard<std::mutex> lock(m_callbackMutex);
		return cb;
	}

	mutable std::mutex m_callbackMutex;
	NeedDataCallback m_needDataCallback;
	CancelNeedDataCallback m_cancelNeedDataCallback;
	PlaybackStateCallback m_playbackStateCallback;
	PositionCallback m_positionCallback;
	DurationCallback m_durationCallback;
	BufferUnderflowCallback m_bufferUnderflowCallback;
	SourceFlushedCallback m_sourceFlushedCallback;
	PlaybackErrorCallback m_playbackErrorCallback;
};

#endif // AAMP_RIALTO_MEDIA_PIPELINE_CLIENT_H