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

/**
 * @file HlsOcdmBridgeInterface.cpp
 * @brief Handles OCDM bridge interface to validate DRM License
 */

#include "HlsOcdmBridgeInterface.h"

#ifdef USE_OPENCDM_ADAPTER
#include "HlsOcdmBridge.h"
#endif


HlsDrmBase* HlsOcdmBridgeInterface::GetBridge(DrmSession * playerDrmSession)
{
   
#ifdef USE_OPENCDM_ADAPTER
    return new HlsOcdmBridge(playerDrmSession);
#else
   return new FakeHlsOcdmBridge(playerDrmSession);
#endif

}
