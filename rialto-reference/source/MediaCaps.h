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
 * @file MediaCaps.h
 * @brief Rialto source/segment description to GStreamer caps.
 *
 * Mirrors the caps the Rialto server builds (CapsBuilder / GstMimeMapping),
 * driven by one MIME table so supporting a new codec is a table entry, given
 * that a decoder for it is installed.
 */

#ifndef RIALTO_REFERENCE_MEDIA_CAPS_H
#define RIALTO_REFERENCE_MEDIA_CAPS_H

#include "IMediaPipeline.h"

#include <gst/gst.h>

namespace rialto_reference
{

/**
 * @brief Caps for a newly attached source.
 *
 * @retval new caps (caller owns), or nullptr if the MIME type is not in the table.
 */
GstCaps *buildSourceCaps(const firebolt::rialto::IMediaPipeline::MediaSource &source);

/**
 * @brief Applies per-segment changes (codec data, video size, audio rate and
 *        channels) to the current caps, as the Rialto server does.
 *
 * @retval new caps (caller owns) if anything changed, otherwise nullptr.
 */
GstCaps *updateCapsForSegment(const GstCaps *current, const firebolt::rialto::IMediaPipeline::MediaSegment &segment);

} // namespace rialto_reference

#endif // RIALTO_REFERENCE_MEDIA_CAPS_H
