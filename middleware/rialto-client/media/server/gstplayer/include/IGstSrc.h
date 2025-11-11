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

#ifndef FIREBOLT_RIALTO_SERVER_I_GST_SRC_H_
#define FIREBOLT_RIALTO_SERVER_I_GST_SRC_H_

#include "IDecryptionService.h"
#include <MediaCommon.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <list>
#include <memory>
#include <stdint.h>
#include <unordered_map>

namespace firebolt::rialto::server
{
class IGstSrc;

/**
 * @brief IGstSrc factory class, for the IGstSrc singleton object.
 */
class IGstSrcFactory
{
public:
    IGstSrcFactory() = default;
    virtual ~IGstSrcFactory() = default;

    /**
     * @brief Gets the IGstSrcFactory instance.
     *
     * @retval the factory instance or null on error.
     */
    static std::shared_ptr<IGstSrcFactory> getFactory();

    /**
     * @brief Gets a IGstSrc singleton object.
     *
     * @retval the src instance or null on error.
     */
    virtual std::shared_ptr<IGstSrc> getGstSrc() = 0;
};

/**
 * @brief Structure used for stream info
 */
struct StreamInfo
{
    explicit StreamInfo(GstElement *appSrc_ = nullptr, bool hasDrm_ = true) : appSrc(appSrc_), hasDrm(hasDrm_) {}
    bool operator==(const StreamInfo &other) const
    {
        return appSrc == other.appSrc && hasDrm == other.hasDrm && isDataNeeded == other.isDataNeeded &&
               isNeedDataPending == other.isNeedDataPending && isDataPushed == other.isDataPushed;
    }
    GstElement *appSrc;
    bool hasDrm;
    bool isDataNeeded{false};
    bool isNeedDataPending{false};
    bool isDataPushed{false};
    std::list<GstBuffer *> buffers{};
    bool underflowOccured{false};
};
/**
 * @brief Definition of a stream info map.
 */
using StreamInfoMap = std::unordered_map<MediaSourceType, StreamInfo>;

class IGstSrc
{
public:
    IGstSrc() = default;
    virtual ~IGstSrc() = default;

    IGstSrc(const IGstSrc &) = delete;
    IGstSrc &operator=(const IGstSrc &) = delete;
    IGstSrc(IGstSrc &&) = delete;
    IGstSrc &operator=(IGstSrc &&) = delete;

    /**
     * @brief Initalise rialto src.
     */
    virtual void initSrc() = 0;

    /**
     * @brief Sets up the app source.
     *
     * @param[in] source    : The source element.
     * @param[in] appsrc    : The app source to attach.
     * @param[in] callbacks : Callbacks to be set on the app source.
     * @param[in] userData  : Data to be passed to the callbacks.
     * @param[in] type      : The media type of the source.
     */
    virtual void setupAndAddAppSrc(IDecryptionService *decryptionService, GstElement *source, StreamInfo &streamInfo,
                                   GstAppSrcCallbacks *callbacks, gpointer userData,
                                   firebolt::rialto::MediaSourceType type) = 0;

    /**
     * @brief Notify rialto source that all app sources have been added.
     *
     * @param[in] element   : The source element.
     */
    virtual void allAppSrcsAdded(GstElement *element) = 0;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_GST_SRC_H_
