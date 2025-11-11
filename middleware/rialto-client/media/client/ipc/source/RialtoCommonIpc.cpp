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

#include "RialtoCommonIpc.h"

namespace firebolt::rialto::client
{
firebolt::rialto::ProtoMediaSourceType convertProtoMediaSourceType(MediaSourceType mediaSourceType)
{
    firebolt::rialto::ProtoMediaSourceType protoMediaSourceType = firebolt::rialto::ProtoMediaSourceType::UNKNOWN;
    switch (mediaSourceType)
    {
    case MediaSourceType::AUDIO:
        protoMediaSourceType = firebolt::rialto::ProtoMediaSourceType::AUDIO;
        break;
    case MediaSourceType::VIDEO:
        protoMediaSourceType = firebolt::rialto::ProtoMediaSourceType::VIDEO;
        break;
    case MediaSourceType::SUBTITLE:
        protoMediaSourceType = firebolt::rialto::ProtoMediaSourceType::SUBTITLE;
        break;
    default:
        break;
    }

    return protoMediaSourceType;
}
}; // namespace firebolt::rialto::client
