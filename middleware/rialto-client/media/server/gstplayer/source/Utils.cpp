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

#include "Utils.h"
#include "CapsBuilder.h"
#include "GstMimeMapping.h"
#include "IGlibWrapper.h"
#include "IGstWrapper.h"
#include "RialtoServerLogging.h"
#include <algorithm>
#include <string.h>

namespace
{
const char *underflowSignals[]{"buffer-underflow-callback", "vidsink-underflow-callback", "underrun-callback"};

bool isType(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element, GstElementFactoryListType type)
{
    if (!element)
    {
        return false;
    }

    GstElementFactory *factory{gstWrapper.gstElementGetFactory(element)};
    if (!factory)
    {
        return false;
    }
    return gstWrapper.gstElementFactoryListIsType(factory, type);
}
} // namespace

namespace firebolt::rialto::server
{

bool isVideoDecoder(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO);
}

bool isAudioDecoder(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO);
}

bool isVideoParser(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_PARSER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO);
}

bool isVideoSink(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_SINK | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO);
}

bool isAudioSink(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_SINK | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO);
}

bool isSink(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_SINK);
}

bool isDecoder(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_DECODER);
}

bool isAudio(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO);
}

bool isVideo(const firebolt::rialto::wrappers::IGstWrapper &gstWrapper, GstElement *element)
{
    return isType(gstWrapper, element, GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO);
}

std::optional<std::string> getUnderflowSignalName(const firebolt::rialto::wrappers::IGlibWrapper &glibWrapper,
                                                  GstElement *element)
{
    GType type = glibWrapper.gObjectType(element);
    guint nsignals{0};
    guint *signals = glibWrapper.gSignalListIds(type, &nsignals);

    for (guint i = 0; i < nsignals; i++)
    {
        GSignalQuery query;
        glibWrapper.gSignalQuery(signals[i], &query);
        const auto signalNameIt = std::find_if(std::begin(underflowSignals), std::end(underflowSignals),
                                               [&](const auto *signalName)
                                               { return strcmp(signalName, query.signal_name) == 0; });

        if (std::end(underflowSignals) != signalNameIt)
        {
            glibWrapper.gFree(signals);
            return std::string(*signalNameIt);
        }
    }
    glibWrapper.gFree(signals);

    return std::nullopt;
}

GstCaps *createCapsFromMediaSource(const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                                   const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper,
                                   const std::unique_ptr<IMediaPipeline::MediaSource> &source)
{
    std::unique_ptr<MediaSourceCapsBuilder> capsBuilder;

    firebolt::rialto::SourceConfigType configType = source->getConfigType();
    if (configType == firebolt::rialto::SourceConfigType::AUDIO)
    {
        const IMediaPipeline::MediaSourceAudio *kSource = dynamic_cast<IMediaPipeline::MediaSourceAudio *>(source.get());
        if (kSource)
        {
            capsBuilder = std::make_unique<MediaSourceAudioCapsBuilder>(gstWrapper, glibWrapper, *kSource);
        }
        else
        {
            RIALTO_SERVER_LOG_ERROR("Failed to cast to audio source");
            return nullptr;
        }
    }
    else if (configType == firebolt::rialto::SourceConfigType::VIDEO)
    {
        const IMediaPipeline::MediaSourceVideo *kSource = dynamic_cast<IMediaPipeline::MediaSourceVideo *>(source.get());
        if (kSource)
        {
            capsBuilder = std::make_unique<MediaSourceVideoCapsBuilder>(gstWrapper, glibWrapper, *kSource);
        }
        else
        {
            RIALTO_SERVER_LOG_ERROR("Failed to cast to video source");
            return nullptr;
        }
    }
    else if (configType == firebolt::rialto::SourceConfigType::VIDEO_DOLBY_VISION)
    {
        const IMediaPipeline::MediaSourceVideoDolbyVision *kSource =
            dynamic_cast<IMediaPipeline::MediaSourceVideoDolbyVision *>(source.get());
        if (kSource)
        {
            capsBuilder = std::make_unique<MediaSourceVideoDolbyVisionCapsBuilder>(gstWrapper, glibWrapper, *kSource);
        }
        else
        {
            RIALTO_SERVER_LOG_ERROR("Failed to cast to dolby vision source!");
            return nullptr;
        }
    }
    else if (configType == firebolt::rialto::SourceConfigType::SUBTITLE)
    {
        // subtitle caps is just a simple type, without any extra parameters
        return firebolt::rialto::server::createSimpleCapsFromMimeType(gstWrapper, *source.get());
    }
    else
    {
        RIALTO_SERVER_LOG_ERROR("Invalid config type %u", static_cast<uint32_t>(configType));
        return nullptr;
    }

    return capsBuilder->buildCaps();
}
} // namespace firebolt::rialto::server
