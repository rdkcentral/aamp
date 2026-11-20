#pragma once

// Factory only: no GStreamer type crosses this header.

#include "media/api/MediaPipeline.h"

namespace nmqe::media {

std::unique_ptr<IMediaPipeline> createGstMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, const PlatformSettings &settings);

} // namespace nmqe::media
