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
#include <memory>
#include <iostream>
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

using namespace firebolt::rialto;

/**
 * @class AampRialtoMediaPipelineClient
 * @brief IMediaPipelineClient implementation that forwards need-data
 *        and cancel-need-data events via installed callbacks.
 */
class AampRialtoMediaPipelineClient : public IMediaPipelineClient
{
public:
	/// Callback invoked when Rialto requests media data.
	/// Parameters: sourceId, frameCount, needDataRequestId
	using NeedDataCallback =
		std::function<void(int32_t, size_t, uint32_t)>;

	/// Callback invoked when Rialto cancels a pending need-data request.
	/// Parameter: sourceId
	using CancelNeedDataCallback = std::function<void(int32_t)>;

	AampRialtoMediaPipelineClient();
	~AampRialtoMediaPipelineClient() override;

	bool init();

	/// @brief Install callback for notifyNeedMediaData events.
	void SetNeedDataCallback(NeedDataCallback cb)
	{
		m_needDataCallback = std::move(cb);
	}

	/// @brief Install callback for notifyCancelNeedMediaData events.
	void SetCancelNeedDataCallback(CancelNeedDataCallback cb)
	{
		m_cancelNeedDataCallback = std::move(cb);
	}

	// IMediaPipelineClient Implementation (All required pure virtuals)
	void notifyNetworkState(NetworkState state) override;
	void notifyPlaybackState(PlaybackState state) override;
	void notifyPosition(int64_t position) override;
	void notifyNeedMediaData(
		int32_t sourceId,
		size_t frameCount,
		uint32_t needDataRequestId,
		const std::shared_ptr<MediaPlayerShmInfo>
			&mediaPlayerShmInfo) override;
	void notifyQos(
		int32_t sourceId, const QosInfo &qosInfo) override;
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
		PlaybackError error) override;
	void notifySourceFlushed(int32_t sourceId) override;
	void notifyPlaybackInfo(
		const PlaybackInfo &playbackInfo) override;

private:
	NeedDataCallback m_needDataCallback;
	CancelNeedDataCallback m_cancelNeedDataCallback;
};

#endif // AAMP_RIALTO_MEDIA_PIPELINE_CLIENT_H