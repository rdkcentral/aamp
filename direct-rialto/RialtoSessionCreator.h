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
 * @file RialtoSessionCreator.h
 * @brief Thin helper that produces a DrmSessionCreator for the Rialto path.
 *
 * Including this header keeps priv_aamp.cpp decoupled from the concrete
 * Rialto types (RialtoMediaKeySystem, RialtoMediaKeySessionAdapter).
 * Only the DrmSessionCreator std::function type is exposed.
 */

#ifndef RialtoSessionCreator_h
#define RialtoSessionCreator_h

#include "DrmSessionManager.h"

/**
 * @brief Build a DrmSessionCreator that constructs Rialto DRM sessions.
 *
 * Returns a callable that creates a RialtoMediaKeySessionAdapter wrapped
 * in a RialtoMediaKeySystem for any DRM system ID. Intended to be passed
 * to AampDRMLicenseManager's constructor on the Rialto path.
 *
 * @return DrmSessionCreator lambda; never null.
 */
DrmSessionCreator makeRialtoSessionCreator();

#endif // RialtoSessionCreator_h
