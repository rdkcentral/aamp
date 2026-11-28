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
 * @file GstReferencePipeline.h
 * @brief GStreamer reference implementation of firebolt::rialto::IMediaPipeline.
 *
 * An in-process implementation of the Rialto media pipeline API that decodes
 * and renders with a local GStreamer pipeline instead of talking to a Rialto
 * server. It follows Rialto server semantics (need-data handshake, flushing
 * seek with SEEKING/SEEK_DONE, per-source segments, EOS accounting) so that a
 * Rialto client such as AAMP's Direct Rialto player, or any other consumer
 * written against the Rialto API, can play real A/V where no Rialto server
 * exists, and so that it can be A/B tested against real Rialto.
 *
 * Only the public Rialto client headers are needed; no Rialto library is
 * linked.
 */

#ifndef RIALTO_REFERENCE_GST_REFERENCE_PIPELINE_H
#define RIALTO_REFERENCE_GST_REFERENCE_PIPELINE_H

#include "IMediaPipeline.h"

#include <memory>

namespace rialto_reference
{

/**
 * @brief Creates a GStreamer-backed media pipeline.
 *
 * Initialises GStreamer if the host process has not already done so.
 *
 * Environment overrides (all optional):
 *  - RIALTO_REF_VIDEO_SINK / RIALTO_REF_AUDIO_SINK: a gst-launch style
 *    description of the sink, e.g. "fakesink sync=true" for headless runs.
 *  - RIALTO_REF_VERBOSE=1: per-request logging.
 *
 * @retval the pipeline, or nullptr if GStreamer could not be initialised.
 */
std::unique_ptr<firebolt::rialto::IMediaPipeline>
createGstMediaPipeline(std::weak_ptr<firebolt::rialto::IMediaPipelineClient> client,
                       const firebolt::rialto::VideoRequirements &videoRequirements);

/**
 * @brief Creates a factory producing GStreamer-backed media pipelines.
 */
std::shared_ptr<firebolt::rialto::IMediaPipelineFactory> createGstMediaPipelineFactory();

} // namespace rialto_reference

#endif // RIALTO_REFERENCE_GST_REFERENCE_PIPELINE_H
