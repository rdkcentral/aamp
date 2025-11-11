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

#ifndef FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITER_FACTORY_H_
#define FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITER_FACTORY_H_

#include "IMediaFrameWriter.h"
#include <memory>

namespace firebolt::rialto::common
{
/**
 * @brief IMediaFrameWriter factory class definition.
 */
class MediaFrameWriterFactory : public IMediaFrameWriterFactory
{
public:
    MediaFrameWriterFactory();
    std::unique_ptr<IMediaFrameWriter> createFrameWriter(uint8_t *shmBuffer,
                                                         const std::shared_ptr<MediaPlayerShmInfo> &shminfo) override;

private:
    int m_metadataVersion;
};

} // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_MEDIA_FRAME_WRITER_FACTORY_H_
