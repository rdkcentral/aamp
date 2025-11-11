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

#include "RialtoCommonModule.h"

namespace firebolt::rialto::server::ipc
{
firebolt::rialto::MediaSourceType convertMediaSourceType(const firebolt::rialto::ProtoMediaSourceType &mediaSourceType)
{
    switch (mediaSourceType)
    {
    case firebolt::rialto::ProtoMediaSourceType::UNKNOWN:
    {
        return firebolt::rialto::MediaSourceType::UNKNOWN;
    }
    case firebolt::rialto::ProtoMediaSourceType::AUDIO:
    {
        return firebolt::rialto::MediaSourceType::AUDIO;
    }
    case firebolt::rialto::ProtoMediaSourceType::VIDEO:
    {
        return firebolt::rialto::MediaSourceType::VIDEO;
    }
    case firebolt::rialto::ProtoMediaSourceType::SUBTITLE:
    {
        return firebolt::rialto::MediaSourceType::SUBTITLE;
    }
    }
    return firebolt::rialto::MediaSourceType::UNKNOWN;
}

} // namespace firebolt::rialto::server::ipc
