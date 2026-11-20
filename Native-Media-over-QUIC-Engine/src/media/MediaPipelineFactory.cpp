#include "media/api/MediaPipeline.h"

#include "media/gst/GstMediaPipeline.h"
#include "media/null/NullMediaPipeline.h"

namespace nmqe::media {

std::unique_ptr<IMediaPipeline> createMediaPipeline(const std::string &kind, std::weak_ptr<IMediaPipelineClient> client,
                                                    const PlatformSettings &settings)
{
	if (kind == "gst") return createGstMediaPipeline(std::move(client), settings);
	if (kind == "null") return createNullMediaPipeline(std::move(client), settings.appsrcMaxBytes);
	return nullptr;
}

} // namespace nmqe::media
