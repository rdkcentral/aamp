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
#include <mutex>
#include <memory>
#include <vector>

/**
 * @brief Pipeline events the player handles.  Delivered on Rialto's event
 *        thread, without the client's lock held.
 */
class IAampRialtoPipelineEvents
{
public:
	virtual ~IAampRialtoPipelineEvents() = default;

	/// Rialto wants @p frameCount frames for @p sourceId.
	virtual void OnNeedMediaData(int32_t sourceId, size_t frameCount,
		uint32_t requestId) = 0;
	/// Rialto cancelled the pending need-data request for @p sourceId.
	virtual void OnCancelNeedMediaData(int32_t sourceId) = 0;
	/// The Rialto server changed state.
	virtual void OnPlaybackState(firebolt::rialto::PlaybackState state) = 0;
	/// Current position, in nanoseconds.
	virtual void OnPosition(int64_t positionNs) = 0;
	/// Stream duration, in nanoseconds.
	virtual void OnDuration(int64_t durationNs) = 0;
	/// @p sourceId has exhausted its buffer.
	virtual void OnBufferUnderflow(int32_t sourceId) = 0;
	/// Non-fatal playback error on @p sourceId.
	virtual void OnPlaybackError(int32_t sourceId,
		firebolt::rialto::PlaybackError error) = 0;
};

/**
 * @class AampRialtoMediaPipelineClient
 * @brief IMediaPipelineClient implementation that forwards Rialto events to
 *        the installed IAampRialtoPipelineEvents sink.
 */
class AampRialtoMediaPipelineClient
	: public firebolt::rialto::IMediaPipelineClient
{
public:
	AampRialtoMediaPipelineClient();
	~AampRialtoMediaPipelineClient() override;

	/// @brief Install @p sink (nullptr to remove).  Not owned: it must
	///        outlive its installation and any event already being delivered
	///        to it.  Safe from any thread, including from inside an event.
	void SetEventSink(IAampRialtoPipelineEvents *sink)
	{
		std::lock_guard<std::mutex> lock(m_sinkMutex);
		m_sink = sink;
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
	/// The installed sink.  Read under the lock and called without it, so an
	/// event may call SetEventSink().
	IAampRialtoPipelineEvents *Sink() const
	{
		std::lock_guard<std::mutex> lock(m_sinkMutex);
		return m_sink;
	}

	mutable std::mutex m_sinkMutex;
	IAampRialtoPipelineEvents *m_sink{nullptr};
};

#endif // AAMP_RIALTO_MEDIA_PIPELINE_CLIENT_H