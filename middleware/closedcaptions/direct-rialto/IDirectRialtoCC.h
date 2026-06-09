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
 * @file IDirectRialtoCC.h
 * @brief Narrow control interface for closed-caption operations in the
 *        direct-rialto path.
 *
 * AampRialtoPlayer implements this interface and passes itself (cast to
 * IDirectRialtoCC*) to PlayerDirectRialtoCCManager::Initialize() so that
 * the CC manager can drive track selection and muting through the Rialto
 * IMediaPipeline API without depending on GStreamer.
 */

#ifndef IDIRECT_RIALTO_CC_H
#define IDIRECT_RIALTO_CC_H

#include <string>

/**
 * @interface IDirectRialtoCC
 * @brief Minimal CC-control interface implemented by AampRialtoPlayer.
 *
 * Decouples PlayerDirectRialtoCCManager (which needs no Rialto headers) from
 * AampRialtoPlayer (which owns the IMediaPipeline) so that neither class
 * needs to include the other's full header.
 */
class IDirectRialtoCC
{
public:
	virtual ~IDirectRialtoCC() = default;

	/**
	 * @brief Set the active CC text-track identifier on the pipeline.
	 * @param id  Track identifier string (e.g. "CC1", "SERVICE1").
	 * @return true on success.
	 */
	virtual bool setTextTrackIdentifier(const std::string &id) = 0;

	/**
	 * @brief Mute or un-mute CC rendering via the pipeline.
	 * @param muted  true to mute; false to un-mute.
	 * @return true on success.
	 */
	virtual bool setCCMute(bool muted) = 0;
};

#endif // IDIRECT_RIALTO_CC_H
