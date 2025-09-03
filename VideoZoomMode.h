/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

#ifndef VIDEO_ZOOM_MODE_H
#define VIDEO_ZOOM_MODE_H

/**
 * @enum VideoZoomMode
 * @brief Video zoom mode
 */
enum VideoZoomMode
{
    VIDEO_ZOOM_NONE,    /**< Video Zoom None */
    VIDEO_ZOOM_DIRECT,  /**< Video Zoom Direct */
    VIDEO_ZOOM_NORMAL,  /**< Video Zoom Normal */
    VIDEO_ZOOM_16X9_STRETCH, /**< Video Zoom 16x9 stretch */
    VIDEO_ZOOM_4x3_PILLAR_BOX, /**< Video Zoom 4x3 pillar box */
    VIDEO_ZOOM_FULL, /**< Video Zoom Full */
    VIDEO_ZOOM_GLOBAL/**< Video Zoom Global */
};

#endif // VIDEO_ZOOM_MODE_H
