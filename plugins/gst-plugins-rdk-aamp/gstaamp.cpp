/*
* Copyright 2018 RDK Management
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation, version 2.1
* of the license.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/

/**
 * @file gstaamp.cpp
 * @brief AAMP gstreamer plugin definitions
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include "gstaamp.h"
#include "main_aamp.h"
#include "priv_aamp.h"
#include "middleware/GstUtils.h"

GST_DEBUG_CATEGORY_STATIC (gst_aamp_debug_category);
#define GST_CAT_DEFAULT gst_aamp_debug_category

/* Workaround for Audio packets muxed late in the stream with
 * Gstreamer blocking the Video injection until the Audio packets are received.
 *
 * Since the buffer size is very less, only video packets get filled initially and
 * waiting for Gstreamer to consume the data.
 *
 * But Gstreamer is waiting for Audio Packets and not consuming the Video data.
 *
 * When we Increase the buffer size, it provides some space for adding more video packets
 * in the queue, when Gstreamer is blocking the push.
 *
 * Meanwhile Audio Packets will be received, and PLayback will start Smoothly.
 *
 * When we try to remove this Buffer size condition check, memory leak happened,
 * due to all the packets get added in the queue without any max threshold value.
 *
 * Temporary Fix
 *
 * Increasing the size of queue from 30 to 100.
 */
#define MAX_NUM_BUFFERS_IN_QUEUE 100

#define  GST_AAMP_LOG_TIMING(msg...) GST_FIXME_OBJECT(aamp, msg)
#define  STREAM_COUNT (sizeof(aamp->stream)/sizeof(aamp->stream[0]))

static const gchar *g_aamp_expose_hls_caps = NULL;

static GstStateChangeReturn
gst_aamp_change_state(GstElement * element, GstStateChange transition);
static void gst_aamp_finalize(GObject * object);
static gboolean gst_aamp_query(GstElement * element, GstQuery * query);

static GstFlowReturn gst_aamp_sink_chain(GstPad * pad, GstObject *parent, GstBuffer * buffer);
static gboolean gst_aamp_sink_event(GstPad * pad, GstObject *parent, GstEvent * event);

static gboolean gst_aamp_src_event(GstPad * pad, GstObject *parent, GstEvent * event);
static gboolean gst_aamp_src_query(GstPad * pad, GstObject *parent, GstQuery * query);

static void gst_aamp_configure(GstAamp * aamp, StreamOutputFormat format, StreamOutputFormat audioFormat);
static gboolean gst_aamp_ready(GstAamp *aamp);

/**
 * @enum GstAampProperties
 * @brief Placeholder for gstaamp  properties
 */
enum GstAampProperties
{
	PROP_0
};

gboolean gst_aamp_push(media_stream* stream, GstMiniObject *obj, gboolean *eosEvent = NULL)
{
	GST_TRACE_OBJECT(stream->parent, "Enter gst_aamp_push");
	gboolean retVal = TRUE;
	if (GST_IS_BUFFER(obj))
	{
		if (stream->isPaused)
		{
			GST_WARNING_OBJECT(stream->parent, "gst_pad_push[%s] paused\n", GST_PAD_NAME(stream->srcpad));
			return FALSE;
		}
		GstFlowReturn ret;
		ret = gst_pad_push(stream->srcpad, GST_BUFFER(obj));
		if (ret != GST_FLOW_OK)
		{
			GST_WARNING_OBJECT(stream->parent, "gst_pad_push[%s] error: %s \n", GST_PAD_NAME(stream->srcpad),
			        gst_flow_get_name(ret));
			retVal = gst_pad_pause_task(stream->srcpad);
			if (!retVal)
				GST_WARNING_OBJECT(stream->parent, "gst_pad_push[%s] pausing error \n", GST_PAD_NAME(stream->srcpad));
			stream->isPaused=TRUE;
			retVal = FALSE;
		}
	}
	else if (GST_IS_EVENT(obj))
	{
		GstEvent* event = GST_EVENT(obj);
		if (eosEvent)
		{
			if (GST_EVENT_TYPE(event) == GST_EVENT_EOS)
			{
				*eosEvent = TRUE;
			}
		}
		GST_INFO_OBJECT(stream->parent, "%s: send %s event\n", __FUNCTION__,
		        GST_EVENT_TYPE_NAME(event));
		if (!gst_pad_push_event(stream->srcpad, event))
		{
			GST_WARNING_OBJECT(stream->parent, "gst_pad_push_event[%s] error\n", GST_PAD_NAME(stream->srcpad));
		}
	}
	else
	{
		GST_ERROR_OBJECT(stream->parent, "%s: unexpected object type in queue\n", __FUNCTION__);
	}
	return retVal;
}

/**
 * @brief Enqueue buffer/event to queue
 * @param[in] stream Media stream object pointer
 * @param[in] item pointer to the object to be added to queue/pushed
 */
void gst_aamp_stream_add_item(media_stream* stream, gpointer item)
{
	GST_TRACE_OBJECT(stream->parent, "Enter gst_aamp_stream_add_item");
	GstAamp *aamp = GST_AAMP(stream->parent);
	g_assert(NULL != stream->srcpad);
	if (stream->parent->enable_src_tasks)
	{
		g_mutex_lock(&stream->mutex);
		while (g_queue_get_length(stream->queue) > MAX_NUM_BUFFERS_IN_QUEUE)
		{
			g_cond_wait(&stream->cond, &stream->mutex);
			if (aamp->flushing)
			{
				break;
			}
		}
		if (aamp->flushing)
		{
			GstMiniObject *obj = (GstMiniObject *) item;
			if (GST_IS_BUFFER(obj))
			{
				gst_buffer_unref(GST_BUFFER(obj));
			}
			else if (GST_IS_EVENT(obj))
			{
				gst_event_unref(GST_EVENT(obj));
			}
			else
			{
				GST_ERROR_OBJECT(aamp, "%s: unexpected object type in queue\n", __FUNCTION__);
			}
		}
		else
		{
			g_queue_push_tail(stream->queue, item);
		}
		g_cond_broadcast(&stream->cond);
		g_mutex_unlock(&stream->mutex);
	}
	else
	{
		gst_aamp_push(stream, (GstMiniObject *) item);
	}
}

/**
 * @brief Dequeue buffer/event and push it to srcpad
 * @param[in] stream Media stream object pointer
 */
void gst_aamp_stream_push_next_item(media_stream* stream)
{
	GST_TRACE_OBJECT(stream->parent, "Enter gst_aamp_stream_push_next_item \n");
	GstAamp *aamp = GST_AAMP(stream->parent);
	gboolean eosSent = FALSE;
	while (!eosSent)
	{
		g_mutex_lock(&stream->mutex);
		if (g_queue_is_empty(stream->queue) && !aamp->flushing)
		{
			g_cond_wait(&stream->cond, &stream->mutex);
		}
		if (aamp->flushing)
		{
			GST_INFO_OBJECT(aamp, "Flushing");
			g_mutex_unlock(&stream->mutex);
			gst_pad_pause_task(stream->srcpad);
			break;
		}
		gpointer item = g_queue_pop_head(stream->queue);
		g_cond_broadcast(&stream->cond);
		g_mutex_unlock(&stream->mutex);
		if (item)
		{
			if (!gst_aamp_push(stream, (GstMiniObject *)item, &eosSent))
			{
				break;
			}
		}
		else
		{
			GST_ERROR_OBJECT(aamp, "%s: object NULL\n", __FUNCTION__);
			break;
		}
	}
}

/**
 * @brief Start src pad task of stream
 * @param[in] stream Media stream object pointer
 */
void gst_aamp_stream_start(media_stream* stream)
{
	GST_DEBUG_OBJECT(stream->parent, "Enter gst_aamp_stream_start");
	if(stream->srcpad)
	{
		GST_INFO_OBJECT(stream->parent, "start %s pad task", GST_PAD_NAME(stream->srcpad));
		gst_pad_start_task (stream->srcpad, (GstTaskFunction) gst_aamp_stream_push_next_item,
				stream, NULL);
	}
}

/**
 * @brief Flushes stream queue
 * @param[in] aamp Gstreamer aamp object pointer
 */
void gst_aamp_stream_flush(GstAamp *aamp)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_stream_flush");
	for (int i = 0; i < STREAM_COUNT; i++)
	{
		media_stream* stream = &aamp->stream[i];
		if (stream->srcpad)
		{
			if (aamp->enable_src_tasks)
			{
				g_mutex_lock(&stream->mutex);
				while (FALSE == g_queue_is_empty(stream->queue))
				{
					GstMiniObject *obj = (GstMiniObject *) g_queue_pop_head(stream->queue);
					if (GST_IS_BUFFER(obj))
					{
						gst_buffer_unref(GST_BUFFER(obj));
					}
					else if (GST_IS_EVENT(obj))
					{
						gst_event_unref(GST_EVENT(obj));
					}
					else
					{
						GST_ERROR_OBJECT(stream->parent, "%s: unexpected object type in queue\n", __FUNCTION__);
					}
				}
				g_cond_broadcast(&stream->cond);
				g_mutex_unlock(&stream->mutex);
			}
		}
	}
}

/**
 * @brief Flushes gstreamer elements and stop any running pad tasks
 * @param[in] aamp Gstreamer aamp object pointer
 */
void gst_aamp_stop_and_flush(GstAamp *aamp)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_stop_and_flush");
	aamp->flushing = TRUE;
	gst_aamp_stream_flush(aamp);
	for (int i = 0; i < STREAM_COUNT; i++)
	{
		media_stream* stream = &aamp->stream[i];
		if (stream->srcpad)
		{
			GST_INFO_OBJECT(aamp, "[%s]sending flush start", GST_PAD_NAME(stream->srcpad));
			gst_pad_push_event(stream->srcpad, gst_event_new_flush_start());
			if (aamp->enable_src_tasks)
			{
				gst_pad_stop_task(stream->srcpad);
			}
			GST_INFO_OBJECT(aamp, "[%s]sending flush stop", GST_PAD_NAME(stream->srcpad));
			gst_pad_push_event(stream->srcpad, gst_event_new_flush_stop(TRUE));
		}
	}
	aamp->flushing = FALSE;
}

/**
 * @class GstAampStreamer
 * @brief Handle media data/configuration/events from AAMP core
 */
class GstAampStreamer : public StreamSink, public AAMPEventObjectListener
{
private:
	/**
	 * @brief Inject stream buffer to gstreamer pipeline
	 * @param[in] mediaType stream type
	 * @param[in] ptr buffer pointer
	 * @param[in] len0 length of buffer
	 * @param[in] fpts PTS of buffer (in sec)
	 * @param[in] fdts DTS of buffer (in sec)
	 * @param[in] fDuration duration of buffer (in sec)
	 * @param[in] ptsOffset offset to be added to PTS/DTS
	 * @param[in] copy to map or transfer the buffer
	 */
	bool SendHelper(AampMediaType mediaType, const void *ptr, size_t len, double fpts, double fdts, double fDuration, double ptsOffset, bool copy)
	{
		const char* mediaTypeStr = (mediaType == eMEDIATYPE_AUDIO) ? "AUDIO" : "VIDEO";
		media_stream* stream = &aamp->stream[mediaType];
		gboolean discontinuity = FALSE;
		bool bPushBuffer = true;

		GST_DEBUG_OBJECT(aamp, "%s:%d MediaType(%s) len(%lu), fpts(%lf), fdts(%lf), fDuration(%lf), ptsOffset(%lf)\n", __FUNCTION__, __LINE__, mediaTypeStr, len, fpts, fdts, fDuration, ptsOffset);

#ifdef AAMP_DISCARD_AUDIO_TRACK
		if (mediaType == eMEDIATYPE_AUDIO)
		{
			GST_WARNING_OBJECT(aamp, "%s:%d Discard audio track- not sending data\n", __FUNCTION__, __LINE__);
			return false;
		}
#endif

		if (!readyToSend)
		{
			if (!gst_aamp_ready(aamp))
			{
				GST_WARNING_OBJECT(aamp, "%s:%d Not ready to consume data type(%s)\n", __FUNCTION__, __LINE__, mediaTypeStr);
				return false;
			}
			readyToSend = true;
		}

		if (stream->srcpad)
		{
			if (copy)
			{
				if (stream->isPaused)
				{
					for (int i = 0; i < STREAM_COUNT; i++)
					{
						aamp->stream[i].isPaused = TRUE;
					}
				}

				if (stream->resetPosition && aamp->player_aamp->aamp->seek_pos_seconds > 0)
				{
					aamp->spts = aamp->player_aamp->aamp->seek_pos_seconds;
					GST_DEBUG_OBJECT(aamp, "%s:%d Updating spts(%f) mediaType(%s)", __FUNCTION__, __LINE__, aamp->spts, mediaTypeStr);
				}

				if (aamp->spts > 0 && !aamp->isSkipSeekPosUpdate)
				{
					fpts += aamp->spts;
					fdts += aamp->spts;
				}

				GST_TRACE_OBJECT(aamp, "%s:%d MediaType(%d) Updated fpts(%lf)\n", __FUNCTION__, __LINE__, mediaType, fpts);

				bPushBuffer = !stream->isPaused;
			}
		}
		else
		{
			GST_WARNING_OBJECT(aamp, "%s:%d Pad NULL mediaType(%s) len(%d) fpts(%f)\n", __FUNCTION__, __LINE__, mediaTypeStr, (int)len, fpts);
			return false;
		}

		GstClockTime pts = (GstClockTime)(fpts * GST_SECOND);
		GstClockTime dts = (GstClockTime)(fdts * GST_SECOND);

		if(stream->eventsPending)
		{
			SendPendingEvents(stream, pts);
			discontinuity = TRUE;
		}

		if (aamp->player_aamp->aamp->DownloadsAreEnabled() && bPushBuffer)
		{
			GstBuffer *buffer;

			if (copy)
			{
				buffer = gst_buffer_new_allocate(NULL, (gsize)len, NULL);

				if (buffer)
				{
					GstMapInfo map;
					gst_buffer_map(buffer, &map, GST_MAP_WRITE);
					memcpy(map.data, ptr, len);
					gst_buffer_unmap(buffer, &map);
					GST_BUFFER_PTS(buffer) = pts;
					GST_BUFFER_DTS(buffer) = dts;
				}
				else
				{
					bPushBuffer = FALSE;
				}
			}
			else
			{
				buffer = gst_buffer_new_wrapped ((gpointer)ptr ,(gsize)len);

				if (buffer)
				{
					GST_BUFFER_PTS(buffer) = pts;
					GST_BUFFER_DTS(buffer) = dts;
				}
				else
				{
					bPushBuffer = FALSE;
				}
			}

			if (bPushBuffer)
			{
				if (discontinuity)
				{
					GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT);
				}

				gst_aamp_stream_add_item (stream, buffer);
			}
		}

		GST_TRACE_OBJECT(aamp, "%s:%d Exit", __FUNCTION__, __LINE__);
		return bPushBuffer;
	}

public:
	/**
	 * @brief GstAampStreamer Constructor
	 * @param[in] aamp Associated gstaamp pointer
	 */
	GstAampStreamer(GstAamp * aamp)
	{
		GST_DEBUG_OBJECT(aamp, "Enter GstAampStreamer");
		this->aamp = aamp;
		rate = AAMP_NORMAL_PLAY_RATE;
		srcPadCapsSent = true;
		format = FORMAT_INVALID;
		audioFormat = FORMAT_INVALID;
		readyToSend = false;
		for (int i = 0; i < STREAM_COUNT; i++)
			aamp->stream[i].isPaused = FALSE;
		aamp->seekFlush = FALSE;
		aamp->spts=0.0;
	}


	/**
	 * @brief Configures gstaamp with stream output formats
	 * @param[in] format Output format of main media
	 * @param[in] audioFormat Output format of audio if present
	 * @param[in] auxFormat Output format of aux audio if present
	 * @param[in] bESChangeStatus - To force configure the pipeline when audio codec changed (used for DASH)
	 * @param[in] forwardAudioToAux if audio buffers to be forwarded to aux pipeline
	 */
	void Configure(StreamOutputFormat format, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat, StreamOutputFormat subFormat, bool bESChangeStatus, bool forwardAudioToAux, bool /*setReadyAfterPipelineCreation*/)
	{
		GST_INFO_OBJECT(aamp, "Enter Configure() format = %d audioFormat = %d, auxFormat = %d, bESChangeStatus %d, forwardAudioToAux %d",
					format, audioFormat, auxFormat, bESChangeStatus, forwardAudioToAux);
		this->format = format;
		this->audioFormat = audioFormat;
		for (int i = 0; i < STREAM_COUNT; i++)
			aamp->stream[i].isPaused = FALSE;
		aamp->seekFlush = FALSE;
		aamp->spts = 0.0;
		gst_aamp_configure(aamp, format, audioFormat);
	}


	/**
	 * @brief Sends pending events to stream's src pad
	 * @param[in] stream Media stream to which events are sent
	 * @param[in] pts Presentation time-stamp
	 */
	void SendPendingEvents(media_stream* stream, GstClockTime pts)
	{
		GST_INFO_OBJECT(aamp, "Enter SendPendingEvents");
		if (stream->streamStart)
		{
			GST_INFO_OBJECT(aamp, "sending new_stream_start\n");
			gst_aamp_stream_add_item(stream, gst_event_new_stream_start(aamp->stream_id));

			GST_INFO_OBJECT(aamp, "%s: sending caps1\n", __FUNCTION__);
			gst_aamp_stream_add_item(stream, gst_event_new_caps(stream->caps));
			stream->streamStart = FALSE;
			GST_INFO_OBJECT(aamp, "%s: sent caps\n", __FUNCTION__);
			stream->isPaused=FALSE;
		}
		if (stream->flush)
		{
			GST_INFO_OBJECT(aamp, "%s: sending flush start\n", __FUNCTION__);
			gst_aamp_stream_add_item(stream, gst_event_new_flush_start());
			stream->isPaused=TRUE;

			GstEvent* event = gst_event_new_flush_stop(FALSE);
			GST_INFO_OBJECT(aamp, "%s: sending flush stop\n", __FUNCTION__);
			gst_aamp_stream_add_item(stream, event);
			stream->flush = FALSE;
		}
		if (stream->resetPosition)
		{
			stream->isPaused=FALSE;
			GstSegment segment;
			gst_segment_init(&segment, GST_FORMAT_TIME);
			segment.start = pts;
			segment.position = 0;
			segment.rate = AAMP_NORMAL_PLAY_RATE;
			segment.applied_rate = rate;
			GST_INFO_OBJECT(aamp, "Sending segment event. start %" G_GUINT64_FORMAT " stop %" G_GUINT64_FORMAT" rate %f\n", segment.start, segment.stop, segment.rate);
			GstEvent* event = gst_event_new_segment (&segment);
			gst_aamp_stream_add_item(stream, event);
			stream->resetPosition = FALSE;
		}



		stream->eventsPending = FALSE;
	}

	/**
	 * @brief inject HLS/ts elementary stream buffer to gstreamer pipeline
	 * @param[in] mediaType stream type
	 * @param[in] ptr buffer pointer
	 * @param[in] len0 length of buffer
	 * @param[in] fpts PTS of buffer (in sec)
	 * @param[in] fdts DTS of buffer (in sec)
	 * @param[in] fDuration duration of buffer (in sec)
	 * @note Caller owns ptr, may free on return
	 */
	bool SendCopy(AampMediaType mediaType, const void *ptr, size_t len0, double fpts, double fdts, double fDuration)
	{
		return SendHelper(mediaType, ptr, len0, fpts, fdts, fDuration, 0.0, true /*copy*/);
	}

	/**
	 * @brief inject mp4 segment to gstreamer pipeline
	 * @param[in] mediaType stream type
	 * @param[in] pBuffer buffer as GrowableBuffer pointer
	 * @param[in] fpts PTS of buffer (in sec)
	 * @param[in] fdts DTS of buffer (in sec)
	 * @param[in] fDuration duration of buffer (in sec)
	 * @param[in] ptsOffset offset to be added to PTS/DTS
	 * @param[in] initFragment flag to indicate init header
	 * @note Ownership of pBuffer is transferred
	 */
	bool SendTransfer(AampMediaType mediaType, void *ptr, size_t len, double fpts, double fdts, double fDuration, double ptsOffset, bool initFragment = false, bool discontinuity = false)
	{
		return SendHelper(mediaType, ptr, len, fpts, fdts, fDuration, ptsOffset, false /*transfer*/);
	}

	/**
	 * @brief Updates internal rate
	 * @param[in] rate Rate at which media is played back
	 */
	void UpdateRate(gdouble rate)
	{
		GST_INFO_OBJECT(aamp, "Enter UpdateRate rate = %lf", rate);
		if ( rate != this->rate)
		{
			this->rate = rate;
		}
	}

	/**
	 * @brief Handles stream specific EOS notification from aamp core
	 * @param[in] type Media type of the stream
	 */
	void EndOfStreamReached(AampMediaType type)
	{
		GST_WARNING_OBJECT(aamp, "MediaType %d", (int)type);
		media_stream* stream = &aamp->stream[type];
		if (stream->srcpad)
		{
			gst_aamp_stream_add_item( stream, gst_event_new_eos());
		}
	}


	/**
	 * @brief Handles Discontinuity of stream
	 * @param[in] mediaType type of stream
	 * @retval always false
	 */
	bool Discontinuity(AampMediaType mediaType)
	{
		GST_INFO_OBJECT(aamp, "Enter Discontinuity, mediaType = %d", mediaType);
		aamp->stream[mediaType].resetPosition = TRUE;
		aamp->stream[mediaType].eventsPending = TRUE;
		return false;
	}

	/**
	 * @brief Flush gstaamp streams
	 * @param[in] position seek position
	 * @param[in] rate playback rate
	 */
	void Flush(double position, int rate, bool shouldTearDown)
	{
		if (!aamp->seekFlush)
		{
		GST_INFO_OBJECT(aamp, "Enter Stream Flush position = %lf rate = %d shouldTearDown %d", position, rate, shouldTearDown);
		for (int i = 0; i < STREAM_COUNT; i++)
		{
			aamp->stream[i].flush = TRUE;
			aamp->stream[i].eventsPending = TRUE;
		}
		aamp->seekFlush = TRUE;
		}
	}

	void Stop(bool keepLastFrame)
	{
		GST_INFO_OBJECT(aamp, "Enter Stream stop keepLastFrame %d", keepLastFrame);
		for (int i = 0; i < STREAM_COUNT; i++)
		{
			aamp->stream[i].resetPosition = TRUE;
			aamp->stream[i].flush = TRUE;
			aamp->stream[i].eventsPending = TRUE;
		}
		aamp->seekFlush = TRUE;
	}

	unsigned long getCCDecoderHandle(void) override
	{
		GST_DEBUG_OBJECT(aamp, "Enter getCCDecoderHandle");
		GstStructure *structure;
		GstQuery *query;
		const GValue *val;
		gboolean ret;
		gpointer decoder_handle = NULL;

		if (!aamp->stream[eMEDIATYPE_VIDEO].srcpad)
		{
			GST_DEBUG_OBJECT(aamp, "No video src pad available for AAMP plugin to query decoder handle\n");
		}
		else
		{
			structure = gst_structure_new("get_video_handle", "video_handle", G_TYPE_POINTER, 0, NULL);
			query = gst_query_new_custom(GST_QUERY_CUSTOM, structure);
			ret = gst_pad_peer_query(aamp->stream[eMEDIATYPE_VIDEO].srcpad, query);
			if (ret)
			{
				GST_DEBUG_OBJECT(aamp, "Video decoder handle queried successfully\n");
				structure = (GstStructure *) gst_query_get_structure(query);
				val = gst_structure_get_value(structure, "video_handle");

				if (val == NULL)
				{
					GST_ERROR_OBJECT(aamp, "Unable to retrieve video decoder handle from structure\n");
				}
				else
				{
					decoder_handle = g_value_get_pointer(val);
					GST_DEBUG_OBJECT(aamp, "video decoder handle: %p\n", decoder_handle);
				}
			}
			else
			{
				GST_ERROR_OBJECT(aamp, "Video decoder handle query failed \n");
			}

			gst_query_unref(query);
		}
		return (unsigned long)decoder_handle;
	}

void Stream(void)
{
	GST_DEBUG_OBJECT(aamp, "Enter Stream()");
		for (int i = 0; i < STREAM_COUNT; i++)
                {
                        aamp->stream[i].resetPosition = TRUE;
                        aamp->stream[i].eventsPending = TRUE;
                }
}

void SeekStreamSink(double position, double rate)
{
	GST_DEBUG_OBJECT(aamp, "Enter SeekStreamSink()");
}

void StopBuffering(bool forceStop)
{
	GST_DEBUG_OBJECT(aamp, "Enter StopBuffering()");
}

	void Event(const AAMPEventPtr& event);
private:
	GstAamp * aamp;
	gdouble rate;
	bool srcPadCapsSent;
	StreamOutputFormat format;
	StreamOutputFormat audioFormat;
	bool readyToSend;
};

#define AAMP_TYPE_INIT_CODE { \
	GST_DEBUG_CATEGORY_INIT (gst_aamp_debug_category, "aamp", 0, \
		"debug category for aamp element"); \
	}
static GstStaticPadTemplate gst_aamp_sink_template_hls = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("application/x-hls;"
						"application/x-aamp;"));
static GstStaticPadTemplate gst_aamp_sink_template = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("application/x-aamp;"));

#define AAMP_SRC_CAPS_STR "video/mpegts, " \
        "  systemstream=(boolean)true, "\
        "  packetsize=(int)188;"
#define AAMP_SRC_AUDIO_CAPS_STR \
    "audio/mpeg, " \
      "mpegversion = (int) 1;" \
    "audio/mpeg, " \
      "mpegversion = (int) 2, " \
      "stream-format = (string) adts; " \
    "audio/x-ac3; audio/x-eac3;"

static GstStaticPadTemplate gst_aamp_src_template_video =
    GST_STATIC_PAD_TEMPLATE ("video_%02x",
        GST_PAD_SRC,
        GST_PAD_SOMETIMES,
		GST_STATIC_CAPS(AAMP_SRC_CAPS_STR));

static GstStaticPadTemplate gst_aamp_src_template_audio =
    GST_STATIC_PAD_TEMPLATE ("audio_%02x",
        GST_PAD_SRC,
        GST_PAD_SOMETIMES,
		GST_STATIC_CAPS(AAMP_SRC_AUDIO_CAPS_STR));

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstAamp, gst_aamp, GST_TYPE_ELEMENT, AAMP_TYPE_INIT_CODE);


/**
 * @brief Idle task to report decoder handle
 * @param[in] user_data  gstaamp pointer
 * @retval G_SOURCE_REMOVE if handle received, G_SOURCE_CONTINUE otherwise
 */
static gboolean gst_report_video_decode_handle(gpointer user_data)
{
	GstAamp *aamp = (GstAamp *) user_data;
	GST_DEBUG_OBJECT(aamp, "Enter gst_report_video_decode_handle");
	aamp->player_aamp->aamp->NotifyFirstFrameReceived(aamp->context->getCCDecoderHandle());
	aamp->decoder_idle_id = 0;
	return G_SOURCE_REMOVE;
}


/**
 * @brief Updates audio src pad states.
 * @param[in] aamp gstaamp pointer
 */
static void gst_aamp_update_audio_src_pad(GstAamp * aamp)
{
	GST_INFO_OBJECT(aamp, "Enter gst_aamp_update_audio_src_pad");
#ifndef AAMP_DISCARD_AUDIO_TRACK
	if (NULL != aamp->stream[eMEDIATYPE_AUDIO].srcpad)
	{
		gboolean enable_audio;
		if ( aamp->rate != AAMP_NORMAL_PLAY_RATE)
		{
			enable_audio = FALSE;
		}
		else
		{
			enable_audio = TRUE;
		}
		if (enable_audio && !aamp->audio_enabled)
		{
			GST_INFO_OBJECT(aamp, "Enable aud and add pad");
			if (FALSE == gst_pad_set_active (aamp->stream[eMEDIATYPE_AUDIO].srcpad, TRUE))
			{
				GST_WARNING_OBJECT(aamp, "gst_pad_set_active failed");
			}
			if (FALSE == gst_element_add_pad(GST_ELEMENT(aamp), aamp->stream[eMEDIATYPE_AUDIO].srcpad))
			{
				GST_WARNING_OBJECT(aamp, "gst_element_add_pad stream[eMEDIATYPE_AUDIO].srcpad failed");
			}
			if (aamp->enable_src_tasks)
			{
				gst_aamp_stream_start(&aamp->stream[eMEDIATYPE_AUDIO]);
			}
			aamp->stream[eMEDIATYPE_AUDIO].streamStart = TRUE;
			aamp->stream[eMEDIATYPE_AUDIO].eventsPending = TRUE;
			aamp->audio_enabled = TRUE;
		}
		else if (!enable_audio && aamp->audio_enabled)
		{
			GST_INFO_OBJECT(aamp, "Disable aud and remove pad");
			if (FALSE == gst_pad_set_active (aamp->stream[eMEDIATYPE_AUDIO].srcpad, FALSE))
			{
				GST_WARNING_OBJECT(aamp, "gst_pad_set_active FALSE failed");
			}
			if (FALSE == gst_element_remove_pad(GST_ELEMENT(aamp), aamp->stream[eMEDIATYPE_AUDIO].srcpad))
			{
				GST_WARNING_OBJECT(aamp, "gst_element_remove_pad stream[eMEDIATYPE_AUDIO].srcpad failed");
			}
			aamp->audio_enabled = FALSE;
		}
	}
#endif
}


/**
 * @brief Initialize a stream.
 * @param[in] parent pointer to gstaamp instance
 * @param[out] stream pointer to stream structure
 */
void gst_aamp_initialize_stream( GstAamp* parent, media_stream* stream)
{
	GST_DEBUG_OBJECT(parent, "Enter gst_aamp_initialize_stream");
	stream->queue = g_queue_new ();
	stream->parent = parent;
	g_mutex_init (&stream->mutex);
	g_cond_init (&stream->cond);
}

/**
 * @brief Configures gstaamp with stream output formats
 * @param[in] aamp gstaamp pointer
 * @param[in] format Output format of main media
 * @param[in] audioFormat Output format of audio if present
 */
static void gst_aamp_configure(GstAamp * aamp, StreamOutputFormat format, StreamOutputFormat audioFormat)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_configure format %d, audioFormat %d", format, audioFormat);
	GstCaps *caps;
	gchar * padname = NULL;
	GST_DEBUG_OBJECT(aamp, "format %d audioFormat %d", format, audioFormat);

	g_mutex_lock (&aamp->mutex);
	if ( aamp->state >= GST_AAMP_CONFIGURED )
	{
		gst_aamp_update_audio_src_pad(aamp);
		g_mutex_unlock (&aamp->mutex);
		GST_INFO_OBJECT(aamp, "Already configured, sending streamStart");
		return;
	}
	g_mutex_unlock (&aamp->mutex);

	aamp->isSkipSeekPosUpdate = FALSE;

	if (aamp->player_aamp->aamp->IsMuxedStream())
	{
		GST_INFO_OBJECT(aamp, "Muxed stream, enable src pad tasks");
		aamp->enable_src_tasks = TRUE;
	}
	else
	{
		GST_INFO_OBJECT(aamp, "de-muxed stream, do not enable src pad tasks");
		aamp->enable_src_tasks = FALSE;

		if( aamp->player_aamp->aamp->IsAudioPlayContextCreationSkipped() )
		{
			aamp->isSkipSeekPosUpdate = TRUE;
			GST_INFO_OBJECT(aamp, "de-muxed stream and audio playcontext creation skipped, so seek pos should not update to the pts values");
		}
	}

	caps = GetCaps((GstStreamOutputFormat)format);

	if (caps)
	{
		media_stream* video = &aamp->stream[eMEDIATYPE_VIDEO];
		padname = g_strdup_printf ("video_%02x", 1);
		GstPad *srcpad = gst_pad_new_from_static_template(&gst_aamp_src_template_video, padname);
		gst_object_ref(srcpad);
		gst_pad_use_fixed_caps(srcpad);
		GST_OBJECT_FLAG_SET(srcpad, GST_PAD_FLAG_NEED_PARENT);
		gst_pad_set_query_function(srcpad, GST_DEBUG_FUNCPTR(gst_aamp_src_query));
		gst_pad_set_event_function(srcpad, GST_DEBUG_FUNCPTR(gst_aamp_src_event));
		video->caps = caps;
		video->srcpad = srcpad;
		gst_aamp_initialize_stream(aamp, video);
		if (padname)
		{
			GST_INFO_OBJECT(aamp, "Created pad %s", padname);
		    g_free (padname);
		}
		aamp->stream_id = gst_pad_create_stream_id(srcpad, GST_ELEMENT(aamp), NULL);
	}
	else
	{
		GST_WARNING_OBJECT(aamp, "Unsupported videoFormat %d", format);
		return;
	}

	caps = GetCaps((GstStreamOutputFormat)audioFormat);
	if (caps)
	{
		media_stream* audio = &aamp->stream[eMEDIATYPE_AUDIO];
		padname = g_strdup_printf ("audio_%02x", 1);
		GstPad *srcpad = gst_pad_new_from_static_template(&gst_aamp_src_template_audio, padname);
		g_free(padname);
		gst_object_ref(srcpad);
		gst_pad_use_fixed_caps(srcpad);
		GST_OBJECT_FLAG_SET(srcpad, GST_PAD_FLAG_NEED_PARENT);
		gst_pad_set_query_function(srcpad, GST_DEBUG_FUNCPTR(gst_aamp_src_query));
		gst_pad_set_event_function(srcpad, GST_DEBUG_FUNCPTR(gst_aamp_src_event));
		audio->srcpad = srcpad;
		audio->caps= caps;
		gst_aamp_initialize_stream(aamp, audio);
	}
	else
	{
		GST_WARNING_OBJECT(aamp, "Unsupported audioFormat %d", audioFormat);
		return;
	}

	if (caps)
	{
		GST_INFO_OBJECT(aamp, "Setting aamp->state to GST_AAMP_CONFIGURED");
		g_mutex_lock (&aamp->mutex);
		aamp->state = GST_AAMP_CONFIGURED;
		g_cond_signal(&aamp->state_changed);
		g_mutex_unlock (&aamp->mutex);
	}
}


/**
 * @brief Invoked by gstreamer core to initialize class.
 * @param[in] klass GstAampClass pointer
 */
static void gst_aamp_class_init(GstAampClass * klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	g_aamp_expose_hls_caps = g_getenv ("GST_AAMP_EXPOSE_HLS_CAPS");
	if (g_aamp_expose_hls_caps)
	{
		gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_aamp_sink_template_hls));
	}
	else
	{
		gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_aamp_sink_template));
	}
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_aamp_src_template_audio));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_aamp_src_template_video));

	gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass), "Advanced Adaptive Media Player", "Demux",
			"Advanced Adaptive Media Player", "Comcast");

	gobject_class->finalize = gst_aamp_finalize;
	element_class->change_state = GST_DEBUG_FUNCPTR(gst_aamp_change_state);
	element_class->query = GST_DEBUG_FUNCPTR(gst_aamp_query);
}

/**
 * @brief Invoked by gstreamer core to initialize element.
 * @param[in] aamp gstaamp pointer
 */
static void gst_aamp_init(GstAamp * aamp)
{
	GST_AAMP_LOG_TIMING("Enter gst_aamp_init");
	g_mutex_init (&aamp->mutex);
	g_cond_init (&aamp->state_changed);
	aamp->location = NULL;
	aamp->rate = AAMP_NORMAL_PLAY_RATE;
	aamp->audio_enabled = FALSE;
	// Lock access to state to keep Coverity happy
	g_mutex_lock (&aamp->mutex);
	aamp->state = GST_AAMP_NONE;
	g_mutex_unlock (&aamp->mutex);
	aamp->context = new GstAampStreamer(aamp);
	aamp->player_aamp = new PlayerInstanceAAMP(aamp->context);
	aamp->sinkpad = gst_pad_new_from_static_template(&gst_aamp_sink_template_hls, "sink");
	memset(&aamp->stream[0], 0 , sizeof(aamp->stream));
	aamp->stream_id = NULL;
	aamp->idle_id = 0;
	aamp->enable_src_tasks = FALSE;
	aamp->decoder_idle_id = 0;

	gst_pad_set_chain_function(aamp->sinkpad, GST_DEBUG_FUNCPTR(gst_aamp_sink_chain));
	gst_pad_set_event_function(aamp->sinkpad, GST_DEBUG_FUNCPTR(gst_aamp_sink_event));
	gst_element_add_pad(GST_ELEMENT(aamp), aamp->sinkpad);
	aamp->context->Discontinuity(eMEDIATYPE_VIDEO);
	aamp->context->Discontinuity(eMEDIATYPE_AUDIO);
}

/**
 * @brief Finalize a stream.
 * @param[in] stream pointer to stream structure
 */
void gst_aamp_finalize_stream( media_stream* stream)
{
	GST_DEBUG_OBJECT(stream->parent, "Enter gst_aamp_finalize_stream");
	if (stream->srcpad)
	{
		if (stream->caps)
		{
			gst_caps_unref(stream->caps);
		}

		if (stream->srcpad)
		{
			gst_object_unref(stream->srcpad);
		}

		if (stream->queue)
		{
			g_queue_free(stream->queue);
		}
		g_mutex_clear(&stream->mutex);
		g_cond_clear(&stream->cond);
	}
}

/**
 * @brief Invoked by gstreamer core to finalize element.
 * @param[in] object gstaamp pointer
 */
void gst_aamp_finalize(GObject * object)
{
	GstAamp *aamp = GST_AAMP(object);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_finalize");

	if (aamp->location)
	{
		g_free(aamp->location);
		aamp->location = NULL;
	}
	g_mutex_clear (&aamp->mutex);
	delete aamp->player_aamp;
	delete aamp->context;
	aamp->context=NULL;
	g_cond_clear (&aamp->state_changed);

	gst_aamp_finalize_stream( &aamp->stream[eMEDIATYPE_VIDEO]);
	gst_aamp_finalize_stream(&aamp->stream[eMEDIATYPE_AUDIO]);

	if (aamp->stream_id)
	{
		g_free(aamp->stream_id);
	}

	GST_AAMP_LOG_TIMING("Exit\n");
	G_OBJECT_CLASS(gst_aamp_parent_class)->finalize(object);
}

/**
 * @brief This function processes asynchronous events from aamp core
 * @param[in] e reference of event
 */
void GstAampStreamer::Event(const AAMPEventPtr &e)
{
	GST_TRACE_OBJECT(aamp, "Enter GstAampStreamer::Event");
		switch (e->getType())
		{
			case AAMP_EVENT_TUNED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_TUNED");
				break;

			case AAMP_EVENT_TUNE_FAILED:
				GST_WARNING_OBJECT(aamp, "Tune failed");
				g_mutex_lock (&aamp->mutex);
				aamp->state = GST_AAMP_STATE_ERROR;
				g_cond_signal(&aamp->state_changed);
				g_mutex_unlock (&aamp->mutex);
				break;

			case AAMP_EVENT_EOS:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_EOS");
				break;

			case AAMP_EVENT_PLAYLIST_INDEXED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_PLAYLIST_INDEXED");
				break;

			case AAMP_EVENT_PROGRESS:
				break;

			case AAMP_EVENT_TIMED_METADATA:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_TIMED_METADATA");
				break;

			case AAMP_EVENT_STATE_CHANGED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_STATE_CHANGED");
				break;

			case AAMP_EVENT_SEEKED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_SEEKED");
				break;

			case AAMP_EVENT_BITRATE_CHANGED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_BITRATE_CHANGED");
				break;

			case AAMP_EVENT_BUFFERING_CHANGED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_BUFFERING_CHANGED");
				break;

			case AAMP_EVENT_AUDIO_TRACKS_CHANGED:
				GST_INFO_OBJECT(aamp, "AAMP_EVENT_AUDIO_TRACKS_CHANGED");
				break;

			case AAMP_EVENT_REPORT_ANOMALY:
				GST_WARNING_OBJECT(aamp, "AAMP_EVENT_REPORT_ANOMALY");
				break;

			default:
				GST_DEBUG_OBJECT(aamp, "unknown event %d\n", e->getType());
				break;
		}
}


/**
 * @brief Queries URI to upstream element
 * @param[in] aamp gstaamp pointer
 * @retval
 */
static gboolean gst_aamp_query_uri(GstAamp *aamp)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_query_uri");
	gboolean ret = TRUE;
	GstQuery *query = gst_query_new_uri();

	ret = gst_pad_peer_query(aamp->sinkpad, query);
	if (ret)
	{
		gchar *uri;
		gst_query_parse_uri(query, &uri);
		GST_DEBUG_OBJECT(aamp, "uri %s\n", uri);
		if (aamp->location)
		{
			g_free(aamp->location);
		}
		aamp->location = g_strdup(uri);
		g_free(uri);
	}
	gst_query_unref(query);
	return ret;
}


/**
 * @brief Tune asynchronously
 * @param[in] aamp gstaamp pointer
 */
static void gst_aamp_tune_async(GstAamp *aamp)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_tune_async");
	g_mutex_lock(&aamp->mutex);
	aamp->state = GST_AAMP_TUNING;
	g_mutex_unlock(&aamp->mutex);
	GST_AAMP_LOG_TIMING("Calling aamp->Tune()\n");
	aamp->player_aamp->Tune(aamp->location);
}


/**
 * @brief Waits until aamp is configured
 * @param[in] aamp gstaamp pointer
 * @retval TRUE if configured, FALSE on error
 */
static gboolean gst_aamp_configured(GstAamp *aamp)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_configured");
	gboolean ret = FALSE;
	g_mutex_lock(&aamp->mutex);
	if ( aamp->state == GST_AAMP_TUNING )
	{
		g_cond_wait(&aamp->state_changed, &aamp->mutex);
	}
	ret = (aamp->state >= GST_AAMP_CONFIGURED);
	g_mutex_unlock(&aamp->mutex);
	return ret;
}


/**
 * @brief Waits until gstaamp state is ready
 * @param[in] aamp gstaamp pointer
 * @retval TRUE if ready, FALSE on error
 */
static gboolean gst_aamp_ready(GstAamp *aamp)
{
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_ready");
	gboolean ret = FALSE;
	g_mutex_lock(&aamp->mutex);
	while (aamp->state < GST_AAMP_READY)
	{
		g_cond_wait(&aamp->state_changed, &aamp->mutex);
	}
	ret = (aamp->state == GST_AAMP_READY);
	g_mutex_unlock(&aamp->mutex);
	return ret;
}


/**
 * @brief Idle task to report tune complete
 * @param[in] user_data  gstaamp pointer
 * @retval G_SOURCE_REMOVE if logging done, G_SOURCE_CONTINUE if state is not playing
 */
static gboolean gst_aamp_report_on_tune_done(gpointer user_data)
{
	GstAamp *aamp = (GstAamp *) user_data;
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_report_on_tune_done");
	GstElement *pbin = GST_ELEMENT(aamp);
	while (GST_ELEMENT_PARENT(pbin))
	{
		pbin = GST_ELEMENT_PARENT(pbin);
	}
	if (GST_STATE(pbin) == GST_STATE_PLAYING)
	{
		GST_AAMP_LOG_TIMING("LogTuneComplete()");
		aamp->player_aamp->aamp->LogTuneComplete();
		aamp->idle_id = 0;
		return G_SOURCE_REMOVE;
	}
	else
	{
		GST_DEBUG_OBJECT(aamp, "Pipeline Not yet playing");
		return G_SOURCE_CONTINUE;
	}
}


/**
 * @brief Invoked by gstreamer core to change element state.
 * @param[in] element gstaamp pointer
 * @param[in] trans state
 * @retval status of state change operation
 */
static GstStateChangeReturn gst_aamp_change_state(GstElement * element, GstStateChange trans)
{
	GstAamp *aamp;
	GstStateChangeReturn ret;

	aamp = GST_AAMP(element);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_change_state");

	//reduced set of events to register
	constexpr std::array<AAMPEventType, 11> events {AAMP_EVENT_TUNED, AAMP_EVENT_EOS, AAMP_EVENT_PLAYLIST_INDEXED,
		AAMP_EVENT_PROGRESS, AAMP_EVENT_TIMED_METADATA, AAMP_EVENT_STATE_CHANGED, AAMP_EVENT_SEEKED,
		AAMP_EVENT_BITRATE_CHANGED, AAMP_EVENT_BUFFERING_CHANGED, AAMP_EVENT_AUDIO_TRACKS_CHANGED,
		AAMP_EVENT_REPORT_ANOMALY};

	switch (trans)
	{
		case GST_STATE_CHANGE_NULL_TO_READY:
			GST_AAMP_LOG_TIMING("GST_STATE_CHANGE_NULL_TO_READY");
			for (auto evt : events)
			{
				aamp->player_aamp->RegisterEvent(evt, aamp->context);
			}

			if ( FALSE == gst_aamp_query_uri( aamp) )
			{
				return GST_STATE_CHANGE_FAILURE;
			}
			gst_aamp_tune_async( aamp);
			aamp->report_tune = TRUE;
			aamp->report_decode_handle = TRUE;
			aamp->player_aamp->aamp->ResumeTrackDownloads(eMEDIATYPE_VIDEO);
			aamp->player_aamp->aamp->ResumeTrackDownloads(eMEDIATYPE_AUDIO);
			break;

		case GST_STATE_CHANGE_READY_TO_PAUSED:
			GST_AAMP_LOG_TIMING("GST_STATE_CHANGE_READY_TO_PAUSED\n");
			g_mutex_lock (&aamp->mutex);
			if (NULL != aamp->stream[eMEDIATYPE_VIDEO].srcpad)
			{
				if (FALSE == gst_pad_set_active (aamp->stream[eMEDIATYPE_VIDEO].srcpad, TRUE))
				{
					GST_WARNING_OBJECT(aamp, "gst_pad_set_active failed");
				}
				if (FALSE == gst_element_add_pad(GST_ELEMENT(aamp), aamp->stream[eMEDIATYPE_VIDEO].srcpad))
				{
					GST_WARNING_OBJECT(aamp, "gst_element_add_pad srcpad failed");
				}
				if (aamp->enable_src_tasks)
				{
					gst_aamp_stream_start(&aamp->stream[eMEDIATYPE_VIDEO]);
				}
				aamp->stream[eMEDIATYPE_VIDEO].streamStart = TRUE;
				aamp->stream[eMEDIATYPE_VIDEO].eventsPending = TRUE;
			}
			gst_aamp_update_audio_src_pad(aamp);
			aamp->state = GST_AAMP_READY;
			g_cond_signal(&aamp->state_changed);
			g_mutex_unlock (&aamp->mutex);
			gst_element_no_more_pads (element);
			break;
		case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
			GST_AAMP_LOG_TIMING("GST_STATE_CHANGE_PAUSED_TO_PLAYING\n");
//Invoke idle handler
			if (aamp->report_tune)
			{
				aamp->idle_id = g_timeout_add(50, gst_aamp_report_on_tune_done, aamp);
				aamp->report_tune = FALSE;
			}

			if (aamp->report_decode_handle)
			{
				aamp->decoder_idle_id = g_idle_add(gst_report_video_decode_handle, aamp);
				aamp->report_decode_handle = FALSE;
			}
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			if (aamp->enable_src_tasks)
			{
				gst_aamp_stop_and_flush(aamp);
			}
			break;

		default:
			break;
	}

	ret = GST_ELEMENT_CLASS(gst_aamp_parent_class)->change_state(element, trans);
	if (ret == GST_STATE_CHANGE_FAILURE)
	{
		GST_ERROR_OBJECT(aamp, "Parent state change failed\n");
		return ret;
	}
	else
	{
		GST_DEBUG_OBJECT(aamp, "Parent state change :  %s\n", gst_element_state_change_return_get_name(ret));
	}

	switch (trans)
	{
		case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
			if (aamp->idle_id)
			{
				g_source_remove(aamp->idle_id);
				aamp->idle_id = 0;
			}
			GST_DEBUG_OBJECT(aamp, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
			break;
		case GST_STATE_CHANGE_PAUSED_TO_READY:
			GST_DEBUG_OBJECT(aamp, "GST_STATE_CHANGE_PAUSED_TO_READY");
			if (aamp->decoder_idle_id)
			{
				g_source_remove(aamp->decoder_idle_id);
				aamp->decoder_idle_id = 0;
			}
			g_mutex_lock(&aamp->mutex);
			aamp->state = GST_AAMP_SHUTTING_DOWN;
			g_cond_signal(&aamp->state_changed);
			g_mutex_unlock(&aamp->mutex);
			aamp->player_aamp->Stop();
			break;
		case GST_STATE_CHANGE_READY_TO_NULL:
			GST_DEBUG_OBJECT(aamp, "GST_STATE_CHANGE_READY_TO_NULL");
			aamp->player_aamp->RegisterEvents(NULL);
			break;
		case GST_STATE_CHANGE_NULL_TO_READY:
			if (!gst_aamp_configured(aamp))
			{
				GST_ERROR_OBJECT(aamp, "Not configured");
				return GST_STATE_CHANGE_FAILURE;
			}
			else
			{
				GST_DEBUG_OBJECT(aamp, "GST_STATE_CHANGE_NULL_TO_READY Complete");
			}
			break;
		case GST_STATE_CHANGE_READY_TO_PAUSED:
			if (aamp->player_aamp->aamp->IsLive())
			{
				GST_INFO_OBJECT(aamp, "LIVE stream");
				ret = GST_STATE_CHANGE_NO_PREROLL;
			}
			GST_DEBUG_OBJECT(aamp, "GST_STATE_CHANGE_READY_TO_PAUSED");
			break;
		default:
			break;
	}
	GST_DEBUG_OBJECT(aamp, "Exit");

	return ret;
}


/**
 * @brief Element query override .invoked by gstreamer core
 * @param[in] element gstaamp pointer
 * @param[in] query gstreamer query
 * @retval TRUE if query is handled, FALSE if not handled
 */
static gboolean gst_aamp_query(GstElement * element, GstQuery * query)
{
	GstAamp *aamp = GST_AAMP(element);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_query");
	gboolean ret = FALSE;

	GST_INFO_OBJECT(aamp, " query %s\n", gst_query_type_get_name(GST_QUERY_TYPE(query)));

	switch (GST_QUERY_TYPE(query))
	{
		case GST_QUERY_POSITION:
		{
			GstFormat format;

			gst_query_parse_position(query, &format, NULL);
			if (format == GST_FORMAT_TIME)
			{
				gst_query_set_position(query, GST_FORMAT_TIME, (aamp->player_aamp->aamp->GetPositionMs()*GST_MSECOND));
				ret = TRUE;
			}
			break;
		}

		case GST_QUERY_DURATION:
		{
			GstFormat format;
			gst_query_parse_duration (query, &format, NULL);
			if (format == GST_FORMAT_TIME)
			{
				gint64 duration = aamp->player_aamp->aamp->DurationFromStartOfPlaybackMs()*GST_MSECOND;
				gst_query_set_duration (query, format, duration);
				GST_TRACE_OBJECT(aamp, "GST_QUERY_DURATION returning duration %" G_GUINT64_FORMAT "\n", duration);
				ret = TRUE;
			}
			else
			{
				const GstFormatDefinition* def =  gst_format_get_details(format);
				GST_WARNING_OBJECT(aamp, " GST_QUERY_DURATION format %s %s\n", def->nick, def->description);
			}
			break;
		}

		case GST_QUERY_SCHEDULING:
		{
			gst_query_set_scheduling (query, GST_SCHEDULING_FLAG_SEEKABLE, 1, -1, 0);
			ret = TRUE;
			break;
		}

		default:
			break;
	}

	if (FALSE == ret )
	{
		ret = GST_ELEMENT_CLASS(gst_aamp_parent_class)->query(element, query);
	}
	else
	{
		gst_query_unref(query);
	}
	return ret;
}


/**
 * @brief Chain function to handle buffer from source elements
 * @param[in] pad sink pad
 * @param[in] parent gstaamp pointer
 * @param[in] buffer Data from upstream
 * @retval status of operation
 */
static GstFlowReturn gst_aamp_sink_chain(GstPad * pad, GstObject *parent, GstBuffer * buffer)
{
	GstAamp *aamp = GST_AAMP(parent);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_sink_chain");
	GST_DEBUG_OBJECT(aamp, "chain");
	gst_buffer_unref(buffer);
	return GST_FLOW_OK;
}

/**
 * @brief Event listener on sink pad to be invoked by gstreamer core
 * @param[in] pad sink pad
 * @param[in] parent gstaamp pointer
 * @param[in] event gstreamer event
 * @retval TRUE if event is handled, FALSE if not handled
 */
static gboolean gst_aamp_sink_event(GstPad * pad, GstObject *parent, GstEvent * event)
{
	gboolean res = FALSE;
	GstAamp *aamp = GST_AAMP(parent);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_sink_event");

	GST_INFO_OBJECT(aamp, " EVENT %s\n", gst_event_type_get_name(GST_EVENT_TYPE(event)));

	switch (GST_EVENT_TYPE(event))
	{
		case GST_EVENT_EOS:
			GST_WARNING_OBJECT(aamp, "sink pad : Got EOS\n" );
			gst_event_unref(event);
			res = TRUE;
			break;
#ifdef AAMP_SEND_SEGMENT_EVENTS
		case GST_EVENT_SEGMENT:
			GST_INFO_OBJECT(aamp, "sink pad : Got Segment\n" );
			gst_event_unref(event);
			res = TRUE;
			break;
#endif
		default:
			res = gst_pad_event_default(pad, parent, event);
			break;
	}
	return res;
}

/**
 * @brief src pad query listener override to be invoked by gstreamer core
 * @param[in] pad src pad
 * @param[in] parent gstaamp pointer
 * @param[in] query gstreamer query
 * @retval TRUE if query is handled, FALSE if not query
 */
static gboolean gst_aamp_src_query(GstPad * pad, GstObject *parent, GstQuery * query)
{
	gboolean ret = FALSE;
	GstAamp *aamp = GST_AAMP(parent);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_src_query");

	GST_TRACE_OBJECT(aamp, " query %s\n", gst_query_type_get_name(GST_QUERY_TYPE(query)));

	switch (GST_QUERY_TYPE(query))
	{
		case GST_QUERY_CAPS:
		{
			GstCaps* caps = NULL;
			if(aamp->stream[eMEDIATYPE_VIDEO].srcpad == pad )
			{
				caps = aamp->stream[eMEDIATYPE_VIDEO].caps;
			}
			else if(aamp->stream[eMEDIATYPE_AUDIO].srcpad == pad )
			{
				caps = aamp->stream[eMEDIATYPE_AUDIO].caps;
			}
			else
			{
				GST_WARNING_OBJECT(aamp, "Unknown pad %p", pad);
			}
			gst_query_set_caps_result(query, caps);
			ret = TRUE;
			break;
		}
		case GST_QUERY_POSITION:
		{
			GstFormat format;

			gst_query_parse_position(query, &format, NULL);
			if (format == GST_FORMAT_TIME)
			{
				gint64 posMs = aamp->player_aamp->aamp->GetPositionMs();
				GST_TRACE_OBJECT(aamp, " GST_QUERY_POSITION position %" G_GUINT64_FORMAT " seconds\n", posMs/1000);
				gst_query_set_position(query, GST_FORMAT_TIME, (posMs*GST_MSECOND ));
				ret = TRUE;
			}
			break;
		}

		case GST_QUERY_DURATION:
		{
			GstFormat format;
			gst_query_parse_duration (query, &format, NULL);
			if (format == GST_FORMAT_TIME)
			{
				gint64 duration = aamp->player_aamp->aamp->DurationFromStartOfPlaybackMs()*GST_MSECOND;
				gst_query_set_duration (query, format, duration);
				GST_TRACE_OBJECT(aamp, " GST_QUERY_DURATION returning duration %" G_GUINT64_FORMAT "\n", duration);
				ret = TRUE;
			}
			else
			{
				const GstFormatDefinition* def =  gst_format_get_details(format);
				GST_WARNING_OBJECT(aamp, " GST_QUERY_DURATION format %s %s\n", def->nick, def->description);
			}
			break;
		}
		case GST_QUERY_SCHEDULING:
		{
			gst_query_set_scheduling (query, GST_SCHEDULING_FLAG_SEEKABLE, 1, -1, 0);
			ret = TRUE;
			break;
		}

		case GST_QUERY_CUSTOM:
		{
		//g_print("\n\n\nReceived custom event\n\n\n");
			GstStructure *structure = gst_query_writable_structure(query);
			if (structure && gst_structure_has_name(structure, "get_aamp_instance"))
			{
				GValue val = { 0, };
				g_value_init(&val, G_TYPE_POINTER);
				g_value_set_pointer(&val, (gpointer) aamp->player_aamp->aamp);
				gst_structure_set_value(structure, "aamp_instance", &val);
				ret = TRUE;
			} else
			{
				ret = FALSE;
			}
			break;
		}

		default:
			break;
	}
	if (FALSE == ret )
	{
		GST_DEBUG_OBJECT(aamp, "Execute default handler for query %s\n", gst_query_type_get_name(GST_QUERY_TYPE(query)));
		ret = gst_pad_query_default(pad, parent, query);
	}
	return ret;
}


/**
 * @brief Event listener on src pad to be invoked by gstreamer core
 * @param[in] pad src pad
 * @param[in] parent gstaamp pointer
 * @param[in] event gstreamer event
 * @retval TRUE if event is handled, FALSE if not handled
 */
static gboolean gst_aamp_src_event(GstPad * pad, GstObject *parent, GstEvent * event)
{
	gboolean res = FALSE;
	GstAamp *aamp = GST_AAMP(parent);
	GST_DEBUG_OBJECT(aamp, "Enter gst_aamp_src_event EVENT %s\n", gst_event_type_get_name(GST_EVENT_TYPE(event)));

	switch (GST_EVENT_TYPE(event))
	{
		case GST_EVENT_SEEK:
		{
			gdouble rate = 0;
			GstFormat format;
			GstSeekFlags flags;
			GstSeekType start_type;
			gint64 start = 0;
			GstSeekType stop_type;
			gint64 stop;
			gst_event_parse_seek(event, &rate, &format, &flags, &start_type, &start, &stop_type, &stop);
			if (format == GST_FORMAT_TIME)
			{
				GST_INFO_OBJECT(aamp, "sink pad : seek GST_FORMAT_TIME: rate %f, pos %" G_GINT64_FORMAT "\n", rate, start );
				if (flags & GST_SEEK_FLAG_FLUSH)
				{
					aamp->seekFlush = TRUE;
					gst_aamp_stop_and_flush(aamp);
					if (aamp->enable_src_tasks)
					{
						gst_aamp_stream_start(&aamp->stream[eMEDIATYPE_VIDEO]);
						if (aamp->audio_enabled)
						{
							gst_aamp_stream_start(&aamp->stream[eMEDIATYPE_AUDIO]);
						}
					}
				}
				if (rate != aamp->rate)
				{
					aamp->context->UpdateRate(rate);
					aamp->rate  = rate;
				}

				if (start_type == GST_SEEK_TYPE_NONE)
				{
					aamp->player_aamp->SetRate(rate);
				}
				else if (start_type == GST_SEEK_TYPE_SET)
				{
					double pos;
					if (rate < 0)
					{
						pos = stop / GST_SECOND;
					}
					else
					{
						pos = start / GST_SECOND;
					}
					aamp->player_aamp->SetRateAndSeek(rate, pos);
					aamp->stream[eMEDIATYPE_VIDEO].isPaused = FALSE;
					aamp->stream[eMEDIATYPE_AUDIO].isPaused = FALSE;
				}
				else
				{
					GST_WARNING_OBJECT(aamp, "Not supported");
				}
				res = TRUE;
			}
			break;
		}

		default:
			break;
	}
	if (FALSE == res )
	{
		res = gst_pad_event_default(pad, parent, event);
	}
	else
	{
		gst_event_unref(event);
	}

	return res;
}
