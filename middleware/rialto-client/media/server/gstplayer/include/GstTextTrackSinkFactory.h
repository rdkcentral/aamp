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

#ifndef FIREBOLT_RIALTO_SERVER_GST_TEXT_TRACK_SINK_FACTORY_H_
#define FIREBOLT_RIALTO_SERVER_GST_TEXT_TRACK_SINK_FACTORY_H_

#include "IGstTextTrackSinkFactory.h"
#include <memory>

namespace firebolt::rialto::server
{
/**
 * @brief IGstTextTrackSink factory class definition.
 */
class GstTextTrackSinkFactory : public IGstTextTrackSinkFactory
{
public:
    GstTextTrackSinkFactory() = default;
    ~GstTextTrackSinkFactory() override = default;

    GstElement *createGstTextTrackSink() const override;
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_TEXT_TRACK_SINK_FACTORY_H_
