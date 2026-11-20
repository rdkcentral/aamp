#pragma once

#include "media/api/MediaPipeline.h"

namespace nmqe::media {

std::unique_ptr<IMediaPipeline> createNullMediaPipeline(std::weak_ptr<IMediaPipelineClient> client, uint64_t maxQueuedBytes);

} // namespace nmqe::media
