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
 * @file gstaamptaskpool.h
 * @brief AAMP Gstreamer Task pool if needed
 */


#ifndef _GST_AAMP_TASKPOOL_H_
#define _GST_AAMP_TASKPOOL_H_

#include <gst/gst.h>

G_BEGIN_DECLS



#define GST_TYPE_AAMP_TASKPOOL             (gst_aamp_taskpool_get_type ())
#define GST_AAMP_TASKPOOL(pool)            (G_TYPE_CHECK_INSTANCE_CAST ((pool), GST_TYPE_AAMP_TASKPOOL, GstAampTaskpool))
#define GST_IS_AAMP_TASKPOOL(pool)         (G_TYPE_CHECK_INSTANCE_TYPE ((pool), GST_TYPE_AAMP_TASKPOOL))
#define GST_AAMP_TASKPOOL_CLASS(pclass)    (G_TYPE_CHECK_CLASS_CAST ((pclass), GST_TYPE_AAMP_TASKPOOL, GstAampTaskpoolClass))
#define GST_IS_AAMP_TASKPOOL_CLASS(pclass) (G_TYPE_CHECK_CLASS_TYPE ((pclass), GST_TYPE_AAMP_TASKPOOL))
#define GST_AAMP_TASKPOOL_GET_CLASS(pool)  (G_TYPE_INSTANCE_GET_CLASS ((pool), GST_TYPE_AAMP_TASKPOOL, GstAampTaskpoolClass))
#define GST_AAMP_TASKPOOL_CAST(pool)       ((GstAampTaskpool*)(pool))

typedef struct _GstAampTaskpool GstAampTaskpool;
typedef struct _GstAampTaskpoolClass GstAampTaskpoolClass;

struct _GstAampTaskpool {
  GstTaskPool    object;
};

struct _GstAampTaskpoolClass {
  GstTaskPoolClass parent_class;
};

GType           gst_aamp_taskpool_get_type    (void);


G_END_DECLS

#endif
