/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 
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

#ifndef FIREBOLT_RIALTO_SERVER_CAPS_BUILDER_H_
#define FIREBOLT_RIALTO_SERVER_CAPS_BUILDER_H_

#include "IGlibWrapper.h"
#include "IGstWrapper.h"
#include "IMediaPipeline.h"
#include <gst/gst.h>
#include <memory>

namespace firebolt::rialto::server
{
class MediaSourceCapsBuilder
{
public:
    MediaSourceCapsBuilder(std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                           std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                           const firebolt::rialto::IMediaPipeline::MediaSourceAV &source);
    virtual ~MediaSourceCapsBuilder() = default;
    virtual GstCaps *buildCaps();

protected:
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;
    const IMediaPipeline::MediaSourceAV &m_attachedSource;

    GstCaps *buildCommonCaps();
    void addAlignmentToCaps(GstCaps *caps) const;
    void addCodecDataToCaps(GstCaps *caps) const;
    void addStreamFormatToCaps(GstCaps *caps) const;
};

class MediaSourceAudioCapsBuilder : public MediaSourceCapsBuilder
{
public:
    MediaSourceAudioCapsBuilder(std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                const IMediaPipeline::MediaSourceAudio &source);
    ~MediaSourceAudioCapsBuilder() override = default;
    GstCaps *buildCaps() override;

protected:
    GstCaps *createOpusCaps();
    GstCaps *getAudioSpecificConfiguration() const;
    void addSampleRateAndChannelsToCaps(GstCaps *caps) const;
    void addMpegVersion4ToCaps(GstCaps *caps) const;
    void addMp3Caps(GstCaps *caps) const;
    void addRawAudioData(GstCaps *caps) const;
    void addFlacSpecificData(GstCaps *caps) const;

    const IMediaPipeline::MediaSourceAudio &m_attachedAudioSource;
};

class MediaSourceVideoCapsBuilder : public MediaSourceCapsBuilder
{
public:
    MediaSourceVideoCapsBuilder(std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                const IMediaPipeline::MediaSourceVideo &source);
    ~MediaSourceVideoCapsBuilder() override = default;
    GstCaps *buildCaps() override;

protected:
    const IMediaPipeline::MediaSourceVideo &m_attachedVideoSource;
};

class MediaSourceVideoDolbyVisionCapsBuilder : public MediaSourceVideoCapsBuilder
{
public:
    MediaSourceVideoDolbyVisionCapsBuilder(std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> gstWrapper,
                                           std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> glibWrapper,
                                           const IMediaPipeline::MediaSourceVideoDolbyVision &source);
    ~MediaSourceVideoDolbyVisionCapsBuilder() override = default;
    GstCaps *buildCaps() override;

protected:
    const IMediaPipeline::MediaSourceVideoDolbyVision &m_attachedDolbySource;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_CAPS_BUILDER_H_
