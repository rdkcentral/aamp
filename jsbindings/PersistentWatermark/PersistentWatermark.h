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

#ifndef __PERSISTENT_WATERMARK_H__
#define __PERSISTENT_WATERMARK_H__
/**
 *  @brief
 * When enabled this function injects PersistentWatermark JS bindings.
 * When disabled this function only prints out a log message.
 * This function is enabled by defining USE_WATERMARK_JSBINDINGS.
 * When enabled USE_CPP_THUNDER_PLUGIN_ACCESS must also be defined.
 * PersistentWatermark JS bindings are an optional addition to the normal aamp js bindings.
 * PersistentWatermark JS bindings control the watermark plugin.*/
void PersistentWatermark_LoadJS(void* context);
#endif
