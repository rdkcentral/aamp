/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#include "GstLogForwarding.h"
#include "RialtoLogging.h"
#include <gst/gst.h>
#include <sstream>

namespace
{
void gstreamerLogFunction(GstDebugCategory *category, GstDebugLevel level, const gchar *file, const gchar *function,
                          gint line, GObject *object, GstDebugMessage *message, gpointer data)
{
    std::stringstream ss;
    switch (level)
    {
    case GST_LEVEL_NONE:
    case GST_LEVEL_ERROR:
    {
        ss << "ERR: ";
        break;
    }
    case GST_LEVEL_WARNING:
    {
        ss << "WRN: ";
        break;
    }
    case GST_LEVEL_FIXME:
    {
        ss << "FIXME: ";
        break;
    }
    case GST_LEVEL_INFO:
    {
        ss << "NFO: ";
        break;
    }
    case GST_LEVEL_DEBUG:
    {
        ss << "DBG: ";
        break;
    }
    case GST_LEVEL_LOG:
    {
        ss << "LOG: ";
        break;
    }
    case GST_LEVEL_TRACE:
    {
        ss << "TRACE: ";
        break;
    }
    case GST_LEVEL_MEMDUMP:
    {
        ss << "MEMDUMP: ";
        break;
    }
    case GST_LEVEL_COUNT:
    {
        ss << "COUNT: ";
        break;
    }
    default:
    {
        break;
    }
    }
    ss << "C:" << gst_debug_category_get_name(category) << " M:" << file << " F:" << function << ":" << line;
    if (object)
    {
        if (GST_IS_PAD(object) && GST_OBJECT_NAME(object) && GST_OBJECT_PARENT(object) &&
            GST_OBJECT_NAME(GST_OBJECT_PARENT(object)))
        {
            ss << " <" << GST_OBJECT_NAME(GST_OBJECT_PARENT(object)) << ":" << GST_OBJECT_NAME(object) << ">";
        }
        else if (GST_IS_OBJECT(object) && GST_OBJECT_NAME(object))
        {
            ss << " <" << GST_OBJECT_NAME(object) << ">";
        }
        else if (G_IS_OBJECT(object))
        {
            ss << " <" << G_OBJECT_TYPE_NAME(object) << "@" << static_cast<void *>(object) << ">";
        }
        else
        {
            ss << " <" << static_cast<void *>(object) << ">";
        }
    }
    ss << ": " << gst_debug_message_get(message);
    RIALTO_LOG_EXTERNAL("%s", ss.str().c_str());
}
} // namespace

namespace firebolt::rialto::server
{
void enableGstLogForwarding()
{
    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(gstreamerLogFunction, nullptr, nullptr);
}
} // namespace firebolt::rialto::server
