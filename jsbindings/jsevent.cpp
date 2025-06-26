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
 * @file jsevent.cpp
 * @brief JavaScript Event Impl for AAMP_JSController and AAMPMediaPlayer_JS
 */


#include "jsevent.h"
#include "jsutils.h"
#include <stdio.h>

static JSClassRef AAMPJSEvent_class_ref();

/**
 * @brief Structure contains properties and callbacks of Event object of AAMP_JSController
 */
static const JSClassDefinition AAMPJSEvent_object_def =
{
	0,
	kJSClassAttributeNone,
	"__Event_AAMPJS",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};


JSObjectRef createNewAAMPJSEvent(JSGlobalContextRef ctx, const char *type, bool bubbles, bool cancelable)
{
	JSObjectRef eventObj = JSObjectMake(ctx, AAMPJSEvent_class_ref(), NULL);
	return eventObj;
}

static JSClassRef AAMPJSEvent_class_ref()
{
	static JSClassRef classDef = NULL;
	if (!classDef)
	{
		classDef = JSClassCreate(&AAMPJSEvent_object_def);
	}
	return classDef;
}
