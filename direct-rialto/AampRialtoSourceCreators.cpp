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
 * @file AampRialtoSourceCreators.cpp
 * @brief Default SourceCreator used by the production AampRialtoPlayer
 *        constructor.  Separated from AampRialtoPlayer.cpp so that test
 *        binaries can substitute a fake without linking the concrete
 *        Video/Audio/Subtitle source implementations.
 */

#include "AampRialtoPlayer.h"
#include "AampRialtoVideoSource.h"
#include "AampRialtoAudioSource.h"
#include "AampRialtoSubtitleSource.h"

SourceCreator makeDefaultSourceCreator()
{
	return [](AampMediaType type) -> std::unique_ptr<AampRialtoMediaSource>
	{
		switch (type)
		{
		case eMEDIATYPE_VIDEO:    return std::make_unique<AampRialtoVideoSource>();
		case eMEDIATYPE_AUDIO:    return std::make_unique<AampRialtoAudioSource>();
		case eMEDIATYPE_SUBTITLE: return std::make_unique<AampRialtoSubtitleSource>();
		default:                  return nullptr;
		}
	};
}
