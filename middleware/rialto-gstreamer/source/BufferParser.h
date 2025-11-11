/*
 * Copyright (C) 2022 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef BUFFERPARSER_H
#define BUFFERPARSER_H

#include "GStreamerUtils.h"
#include <IMediaPipeline.h>
#include <gst/gst.h>

class BufferParser
{
    enum class EncryptionFormat
    {
        CLEAR,
        CENC,
        WEBM
    };

public:
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
    parseBuffer(const GstRefSample &sample, GstBuffer *buffer, GstMapInfo map, int streamId);

private:
    virtual std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
    parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure, int64_t timeStamp,
                              int64_t duration) = 0;

    void addProtectionMetadataToSegment(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &segment,
                                        GstBuffer *buffer, const GstMapInfo &map, GstStructure *structure);
    void addCodecDataToSegment(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &segment,
                               GstStructure *structure);
    void addDisplayOffsetToSegment(std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &segment,
                                   guint64 displayOffset);
};

class AudioBufferParser : public BufferParser
{
private:
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
    parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure, int64_t timeStamp,
                              int64_t duration) override;
};

class VideoBufferParser : public BufferParser
{
private:
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
    parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure, int64_t timeStamp,
                              int64_t duration) override;
};

class SubtitleBufferParser : public BufferParser
{
private:
    std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment>
    parseSpecificPartOfBuffer(GstBuffer *buffer, int streamId, GstStructure *structure, int64_t timeStamp,
                              int64_t duration) override;
};

#endif // BUFFERPARSER_H
