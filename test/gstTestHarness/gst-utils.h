/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#ifndef GST_UTILS_H
#define GST_UTILS_H

#include <gst/app/gstappsrc.h>

typedef enum
{
	eMEDIATYPE_AUDIO,
	eMEDIATYPE_VIDEO
} MediaType;
#define NUM_MEDIA_TYPES 2

extern bool gstutils_quiet;

const char *gstutils_GetMediaTypeName( MediaType mediaType );
void gstutils_DumpFlags( GstElement * playbin );
void gstutils_element_setup_cb(GstElement * playbin, GstElement * element, class MediaStream *stream);
void gstutils_HandleGstMessageStateChanged( GstMessage *msg, const char *messageName );
void gstutils_HandleGstMessageStreamStatus( GstMessage *message, const char *messageName );
void gstutils_HandleGstMessageQOS( GstMessage * msg, const char *messageName );
void gstutils_HandleGstMessageTag( GstMessage *msg, const char *messageName );

#endif // GST_UTILS_H

