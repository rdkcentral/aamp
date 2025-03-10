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
 * @file gstaamptaskpool.cpp
 * @brief AAMP Gstreamer Task pool code
 */
#include "gstaamptaskpool.h"
#include <pthread.h>

/**
 * @brief AampGstTaskId to store the threadId
 */
typedef struct
{
  pthread_t thread;
} AampGstTaskId;

/** class initialization */
#define gst_aamp_taskpool_parent_class parent_class
G_DEFINE_TYPE(GstAampTaskpool, gst_aamp_taskpool, GST_TYPE_TASK_POOL);

GST_DEBUG_CATEGORY(gst_aamp_taskpool_debug_category);
#define GST_CAT_DEFAULT gst_aamp_taskpool_debug_category

/**
 * @brief Override for gst_task_pool_push
 *        Start the execution of a new thread from pool.
 *        In this case create and assign a thread for gst task
 *
 *        Not setting any specific policies or priorities for now
 *        as the pthread implementation takes thread attributes automatically
 *        from the calling thread.
 *
 *        For now we just need same priority for AAMP gst threads, and if enabled
 *        RT priority is set on AAMP thread which setup the pipeline thus the gst
 *        threads also get same priority
 *
 * @param pool GstTaskPool
 * @param func the function to call
 * @param data data to pass to func
 * @param error stores error
 * @return gpointer a pointer that should be used for the gst_task_pool_join function
 */
static gpointer gst_aamp_taskpool_push (GstTaskPool * pool, GstTaskPoolFunction func, gpointer data,
    GError ** error)
{
  AampGstTaskId *tid;
  gint res;

  tid = g_new0 (AampGstTaskId, 1);
  res = pthread_create (&tid->thread, NULL, (void *(*)(void *)) func, data);

  if (res != 0) {
    g_set_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN,
        "Error creating thread: %s", g_strerror (res));
    g_free (tid);
    tid = NULL;
  }
  return tid;
}

/**
 * @brief Override for gst_task_pool_join
 *        Used to join the created thread
 * @param pool GstTaskPool
 * @param id TaskId containing the thread info to join
 */
static void gst_aamp_taskpool_join (GstTaskPool * pool, gpointer id)
{
  AampGstTaskId *tid = (AampGstTaskId *) id;

  pthread_join (tid->thread, NULL);

  g_free (tid);
}


/**
 * @brief class_init function for gst_aamp_taskpool
 *
 * @param klass GstAampTaskpoolClass defined in header file
 */
static void gst_aamp_taskpool_class_init (GstAampTaskpoolClass * klass)
{
  GstTaskPoolClass *gsttaskpool_class;

  gsttaskpool_class = (GstTaskPoolClass *) klass;
  gsttaskpool_class->push = gst_aamp_taskpool_push;
  gsttaskpool_class->join = gst_aamp_taskpool_join;
}

/**
 * @brief gst_aamp_taskpool_init
 *
 * @param pool
 */
static void gst_aamp_taskpool_init (GstAampTaskpool * pool)
{
}
