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

#include "AampGstUtils.h"
#include "MockAampGstUtils.h"
#include <cstdarg>

MockAampGstUtils *g_mockAampGstUtils = nullptr;

extern "C" GstCaps *gst_caps_new_simple (const char* media_type, const char* fieldname, ...)
{
    GstCaps *return_ptr = NULL;

    if (g_mockAampGstUtils != nullptr)
    {
        va_list ap;
        va_start(ap, fieldname);
        GType var1 = va_arg(ap, GType);
        int var2 = va_arg(ap, int);
        void *ptr = va_arg(ap, void*);
        return_ptr = g_mockAampGstUtils->gst_caps_new_simple(media_type, fieldname, var1, var2, ptr);
        va_end(ap);
    }

    return return_ptr;
}
