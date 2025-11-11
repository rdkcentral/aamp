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

#ifndef FIREBOLT_RIALTO_SERVER_GENERIC_PLAYER_CONTEXT_H_
#define FIREBOLT_RIALTO_SERVER_GENERIC_PLAYER_CONTEXT_H_

#include "IGstSrc.h"
#include "IRdkGstreamerUtilsWrapper.h"
#include "ITimer.h"
#include "MediaCommon.h"
#include <gst/gst.h>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace firebolt::rialto::server
{
constexpr double kNoPendingPlaybackRate{0.0};

enum class EosState
{
    PENDING,
    SET
};

/**
 * @brief Structure used for video geometry
 */
struct Rectangle
{
    int x, y, width, height;
    constexpr Rectangle() : x(0), y(0), width(0), height(0) {}
    constexpr Rectangle(int x_, int y_, int w_, int h_) : x(x_), y(y_), width(w_), height(h_) {}
    Rectangle(const Rectangle &rhs) = default;
    inline constexpr bool empty() { return (width == 0) || (height == 0); }
    inline void clear() { x = y = width = height = 0; }
};

/**
 * @brief Structure used for set source position
 */
struct SegmentData
{
    int64_t position;
    bool resetTime;
    double appliedRate;
    uint64_t stopPosition;
};

struct GenericPlayerContext
{
    /**
     * @brief The rialto src object.
     */
    std::shared_ptr<IGstSrc> gstSrc{nullptr};

    /**
     * @brief The gstreamer pipeline.
     */
    GstElement *pipeline{nullptr};

    /**
     * @brief The gstreamer source.
     */
    GstElement *source{nullptr};

    /**
     * @brief A map of streams attached to the source.
     */
    StreamInfoMap streamInfo{};

    /**
     * @brief Child sink of the autovideosink.
     */
    GstElement *autoVideoChildSink{nullptr};

    /**
     * @brief Child sink of the autoaudiosink.
     */
    GstElement *autoAudioChildSink{nullptr};

    /**
     * @brief The subtitle sink
     */
    GstElement *subtitleSink{nullptr};

    /**
     * @brief The video sink
     */
    GstElement *videoSink{nullptr};

    /**
     * @brief Flag used to check, if video decoder handle has been set.
     */
    bool isVideoHandleSet{false};

    /**
     * @brief Flag used to check, if BUFFERED notification has been sent.
     *
     * Flag can be used only in worker thread
     */
    bool bufferedNotificationSent{false};

    /**
     * @brief Flag used to check, if the playback is in the playing state
     *
     * Flag can be used only in worker thread
     */
    bool isPlaying{false};

    /**
     * @brief Flag used to check, if EOS has been notified to the client
     *
     * Flag can be used only in worker thread
     */
    bool eosNotified{false};

    /**
     * @brief Pending video geometry
     */
    Rectangle pendingGeometry;

    /**
     * @brief Current playback rate
     */
    double playbackRate{1.0};

    /**
     * @brief Pending playback rate
     */
    double pendingPlaybackRate{kNoPendingPlaybackRate};

    /**
     * @brief Pending immediate output for MediaSourceType::VIDEO
     */
    std::optional<bool> pendingImmediateOutputForVideo{};

    /**
     * @brief Pending low latency
     */
    std::optional<bool> pendingLowLatency{};

    /**
     * @brief Pending sync
     */
    std::optional<bool> pendingSync{};

    /**
     * @brief Pending sync off
     */
    std::optional<bool> pendingSyncOff{};

    /**
     * @brief Pending buffering limit
     */
    std::optional<uint32_t> pendingBufferingLimit{};

    /**
     * @brief Pending use buffering
     */
    std::optional<bool> pendingUseBuffering{};

    /**
     * @brief Pending stream sync mode
     */
    std::map<MediaSourceType, int32_t> pendingStreamSyncMode{};

    /**
     * @brief Pending render frame
     */
    bool pendingRenderFrame{false};

    /**
     * @brief Pending show video window
     */
    std::optional<bool> pendingShowVideoWindow{};

    /**
     * @brief Last audio sample timestamps
     * TODO(LLDEV-31012) Needed to detect audio stream underflow
     */
    int64_t lastAudioSampleTimestamps{0};

    /**
     * @brief The decryption service.
     */
    IDecryptionService *decryptionService{nullptr};

    /**
     * @brief Flag used to check, if audio source has been recently removed
     *
     * Flag can be used only in worker thread
     */
    bool audioSourceRemoved{false};

    /**
     * @brief Audio elements of gst pipeline.
     *
     * Attribute can be used only in worker thread
     */
    firebolt::rialto::wrappers::PlaybackGroupPrivate playbackGroup;

    /**
     * @brief A map of streams that have ended.
     */
    std::unordered_map<MediaSourceType, EosState> endOfStreamInfo{};

    /**
     * @brief Flag used to check if client already notified server that all sources were attached
     *
     * Attribute can be used only in worker thread
     */
    bool wereAllSourcesAttached{false};

    /**
     * @brief Flag used to check if FinishSetupSource is finished. It is needed to avoid need data overwriting.
     *
     * Attribute can be used only in worker thread
     */
    bool setupSourceFinished{false};

    /**
     * @brief Queued source positions. Used by SetSourcePosition task to request pushing new sample.
     *
     * Attribute can be used only in worker thread
     */
    std::map<GstElement *, std::vector<SegmentData>> initialPositions;

    /**
     * @brief Currently set position of a source. Used to check, if additional segment should be pushed.
     *
     * Attribute can be used only in worker thread
     */
    std::map<GstElement *, SegmentData> currentPosition;

    /**
     * @brief The mutex, which protects properties, which are read/written by main/worker thread.
     *        This mutex should be removed in future, when we find out better solution for
     *        property read-write.
     */
    std::mutex propertyMutex;

    /**
     * @brief Flag used to check if audio fade is enabled
     *
     * Attribute can be used only in worker thread
     */
    std::atomic_bool audioFadeEnabled{false};
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GENERIC_PLAYER_CONTEXT_H_
