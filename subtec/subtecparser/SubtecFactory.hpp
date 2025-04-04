/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include <string>
#include "PlayerLogManager.h"
#include "WebvttSubtecDevParser.hpp"
#include "WebVttSubtecParser.hpp"
#include "TtmlSubtecParser.hpp"
#include "subtitleParser.h" //required for gpGlobalConfig also

namespace
{
    template<typename T, typename ...Args>
    std::unique_ptr<T> subtec_make_unique(Args&& ...args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
}

class SubtecFactory
{
public:
    static std::unique_ptr<SubtitleParser> createSubtitleParser(std::string mimeType, int width, int height, bool webVTTCueListenersRegistered, bool isWebVTTNativeConfigSet, bool& playerResumeDownload)
    {
        SubtitleMimeType type = eSUB_TYPE_UNKNOWN;

        AAMPLOG_INFO("createSubtitleParser: mimeType %s", mimeType.c_str());

        if (!mimeType.compare("text/vtt"))
            type = eSUB_TYPE_WEBVTT;
        else if (!mimeType.compare("application/ttml+xml") ||
                !mimeType.compare("application/mp4"))
            type = eSUB_TYPE_TTML;

        return createSubtitleParser(type, width, height, webVTTCueListenersRegistered, isWebVTTNativeConfigSet, playerResumeDownload);
    }

    static std::unique_ptr<SubtitleParser> createSubtitleParser(SubtitleMimeType mimeType, int width, int height, bool webVTTCueListenersRegistered, bool isWebVTTNativeConfigSet, bool& playerResumeDownload)
    {
        AAMPLOG_INFO("createSubtitleParser: mimeType: %d", mimeType);
        std::unique_ptr<SubtitleParser> empty;
        
        try {
            switch (mimeType)
            {
                case eSUB_TYPE_WEBVTT:
                    // If JavaScript cue listeners have been registered use WebVTTParser,
                    // otherwise use subtec
                    if (!webVTTCueListenersRegistered)
			            if (isWebVTTNativeConfigSet)
                            return subtec_make_unique<WebVTTSubtecParser>(mimeType, width, height);
                        else
                            return subtec_make_unique<WebVTTSubtecDevParser>(mimeType, width, height);
                    else
                        return subtec_make_unique<WebVTTParser>(mimeType, width, height);
                case eSUB_TYPE_TTML:
                {
                    playerResumeDownload = true;
                    return subtec_make_unique<TtmlSubtecParser>(mimeType, width, height);
                }
                default:
                    AAMPLOG_WARN("Unknown subtitle parser type %d, returning empty", mimeType);
                    break;
            }
        } catch (const std::runtime_error &e) {
            AAMPLOG_WARN("%s", e.what());
            AAMPLOG_WARN(" Failed on SubtitleParser construction - returning empty");
        }

        return empty;
    }
};
