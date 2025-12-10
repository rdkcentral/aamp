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


/**
 * @file gstplayertaskpool.h
 * @brief PLAYER Gstreamer Task pool if needed
 */


#ifndef _GST_PLAYER_TASKPOOL_H_
#define _GST_PLAYER_TASKPOOL_H_

#include <gst/gst.h>

G_BEGIN_DECLS



#define GST_TYPE_PLAYER_TASKPOOL             (gst_player_taskpool_get_type ())
#define GST_PLAYER_TASKPOOL(pool)            (G_TYPE_CHECK_INSTANCE_CAST ((pool), GST_TYPE_PLAYER_TASKPOOL, GstPlayerTaskpool))
#define GST_IS_PLAYER_TASKPOOL(pool)         (G_TYPE_CHECK_INSTANCE_TYPE ((pool), GST_TYPE_PLAYER_TASKPOOL))
#define GST_PLAYER_TASKPOOL_CLASS(pclass)    (G_TYPE_CHECK_CLASS_CAST ((pclass), GST_TYPE_PLAYER_TASKPOOL, GstPlayerTaskpoolClass))
#define GST_IS_PLAYER_TASKPOOL_CLASS(pclass) (G_TYPE_CHECK_CLASS_TYPE ((pclass), GST_TYPE_PLAYER_TASKPOOL))
#define GST_PLAYER_TASKPOOL_GET_CLASS(pool)  (G_TYPE_INSTANCE_GET_CLASS ((pool), GST_TYPE_PLAYER_TASKPOOL, GstPlayerTaskpoolClass))
#define GST_PLAYER_TASKPOOL_CAST(pool)       ((GstPlayerTaskpool*)(pool))

typedef struct _GstPlayerTaskpool GstPlayerTaskpool;
typedef struct _GstPlayerTaskpoolClass GstPlayerTaskpoolClass;

struct _GstPlayerTaskpool {
  GstTaskPool    object;
};

struct _GstPlayerTaskpoolClass {
  GstTaskPoolClass parent_class;
};

GType           gst_player_taskpool_get_type    (void);


G_END_DECLS

#endif
