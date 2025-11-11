/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#ifndef FIREBOLT_RIALTO_SERVER_IPC_MEDIA_PIPELINE_CLIENT_H_
#define FIREBOLT_RIALTO_SERVER_IPC_MEDIA_PIPELINE_CLIENT_H_

#include "IIpcServer.h"
#include "IMediaPipelineClient.h"
#include <memory>
#include <mutex>

namespace firebolt::rialto::server::ipc
{
class MediaPipelineClient : public IMediaPipelineClient
{
public:
    MediaPipelineClient(int sessionId, const std::shared_ptr<::firebolt::rialto::ipc::IClient> &ipcClient);
    ~MediaPipelineClient() override;

    void notifyDuration(int64_t duration) override;
    void notifyPosition(int64_t position) override;
    void notifyNativeSize(uint32_t width, uint32_t height, double aspect) override;
    void notifyNetworkState(NetworkState state) override;
    void notifyPlaybackState(PlaybackState state) override;
    void notifyVideoData(bool hasData) override;
    void notifyAudioData(bool hasData) override;
    void notifyNeedMediaData(int32_t sourceId, size_t frameCount, uint32_t needDataRequestId,
                             const std::shared_ptr<MediaPlayerShmInfo> &shmInfo) override;
    void notifyCancelNeedMediaData(int32_t sourceId) override;
    void notifyQos(int32_t sourceId, const QosInfo &qosInfo) override;
    void notifyBufferUnderflow(int32_t sourceId) override;
    void notifyPlaybackError(int32_t sourceId, PlaybackError error) override;
    void notifySourceFlushed(int32_t sourceId) override;

private:
    int m_sessionId;
    std::shared_ptr<::firebolt::rialto::ipc::IClient> m_ipcClient;

    // It is possible for a needData to be sent while a source is been attached,
    // this causes an issue in client side as they recieve a needData from a source
    // id they are unaware of.
    std::mutex m_needDataMutex;
};
} // namespace firebolt::rialto::server::ipc

#endif // FIREBOLT_RIALTO_SERVER_IPC_MEDIA_PIPELINE_CLIENT_H_
