/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file jsevent.h
 * @brief JavaScript Event Impl for AAMP_JSController and AAMPMediaPlayer_JS
 */

#ifndef __AAMP_JSEVENT_H__
#define __AAMP_JSEVENT_H__

#include <JavaScriptCore/JavaScript.h>

/**
 * @brief To create a new JS event instance
 * @param[in] ctx JS execution context
 * @param[in] type event type
 * @param[in] bubbles denotes if event support bubbling
 * @param[in] cancelable denotes if event is cancelable
 * @retval JSObject of the new instance created
 */
JSObjectRef createNewAAMPJSEvent(JSGlobalContextRef ctx, const char *type, bool bubbles, bool cancelable);

#endif // __AAMP_JSEVENT_H__
