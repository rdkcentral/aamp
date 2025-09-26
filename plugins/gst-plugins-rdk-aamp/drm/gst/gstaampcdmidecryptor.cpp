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
 * @file gstaampcdmidecryptor.cpp
 * @brief aamp cdmi decryptor plugin definitions
 */
#ifndef UBUNTU
// avoid ubuntu-specific segFault

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "gstaampcdmidecryptor.h"
#include <open_cdm.h>
#include <open_cdm_adapter.h>
#include "DrmConstants.h"
#if defined(AMLOGIC)
#include <gst_svp_meta.h>
#endif
#include <dlfcn.h>
#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC ( gst_aampcdmidecryptor_debug_category);
#define GST_CAT_DEFAULT  gst_aampcdmidecryptor_debug_category
#define DECRYPT_FAILURE_THRESHOLD 5

enum
{
	PROP_0, PROP_AAMP
};

//#define FUNCTION_DEBUG 1
#ifdef FUNCTION_DEBUG
#define DEBUG_FUNC()    g_warning("####### %s : %d ####\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC()
#endif

static const gchar *srcMimeTypes[] = { "video/x-h264", "video/x-h264(memory:SecMem)", "audio/mpeg", "video/x-h265", "video/x-h265(memory:SecMem)", "audio/x-eac3", "audio/x-gst-fourcc-ec_3", "audio/x-ac3","audio/x-opus", nullptr };

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstAampCDMIDecryptor, gst_aampcdmidecryptor, GST_TYPE_BASE_TRANSFORM,
		GST_DEBUG_CATEGORY_INIT (gst_aampcdmidecryptor_debug_category, "aampcdmidecryptor", 0,
				"debug category for aampcdmidecryptor element"));

/* prototypes */
static void gst_aampcdmidecryptor_dispose(GObject*);
static GstCaps *gst_aampcdmidecryptor_transform_caps(
		GstBaseTransform * trans, GstPadDirection direction, GstCaps * caps,
		GstCaps * filter);
static gboolean gst_aampcdmidecryptor_sink_event(GstBaseTransform * trans,
		GstEvent * event);
static GstFlowReturn gst_aampcdmidecryptor_transform_ip(
		GstBaseTransform * trans, GstBuffer * buf);
static GstStateChangeReturn gst_aampcdmidecryptor_changestate(
		GstElement* element, GstStateChange transition);
static void gst_aampcdmidecryptor_set_property(GObject * object,
		guint prop_id, const GValue * value, GParamSpec * pspec);
static gboolean gst_aampcdmidecryptor_accept_caps(GstBaseTransform * trans,
		GstPadDirection direction, GstCaps * caps);
static OpenCDMError(*OCDMGstTransformCaps)(GstCaps **);

static void gst_aampcdmidecryptor_class_init(
		GstAampCDMIDecryptorClass * klass)
{
	DEBUG_FUNC();

	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->set_property = gst_aampcdmidecryptor_set_property;
	gobject_class->dispose = gst_aampcdmidecryptor_dispose;

	g_object_class_install_property(gobject_class, PROP_AAMP,
			g_param_spec_pointer("aamp", "AAMP",
					"AAMP instance to do profiling", G_PARAM_WRITABLE));

	GST_ELEMENT_CLASS(klass)->change_state =
			gst_aampcdmidecryptor_changestate;

	base_transform_class->transform_caps = GST_DEBUG_FUNCPTR(
			gst_aampcdmidecryptor_transform_caps);
	base_transform_class->sink_event = GST_DEBUG_FUNCPTR(
			gst_aampcdmidecryptor_sink_event);
	base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(
			gst_aampcdmidecryptor_transform_ip);

#if !defined(AMLOGIC)
	base_transform_class->accept_caps = GST_DEBUG_FUNCPTR(
			gst_aampcdmidecryptor_accept_caps);
#endif
	base_transform_class->transform_ip_on_passthrough = FALSE;

	gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
			"Decrypt encrypted content with CDMi",
			GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
			"Decrypts streams encrypted using Encryption.",
			"Comcast");
	//GST_DEBUG_OBJECT(aampcdmidecryptor, "Inside custom plugin init\n");
}

static void gst_aampcdmidecryptor_init(
		GstAampCDMIDecryptor *aampcdmidecryptor)
{
	DEBUG_FUNC();

	const char* ocdmgsttransformcaps = "opencdm_gstreamer_transform_caps";
	GstBaseTransform* base = GST_BASE_TRANSFORM(aampcdmidecryptor);

	gst_base_transform_set_in_place(base, TRUE);
	gst_base_transform_set_passthrough(base, FALSE);
	gst_base_transform_set_gap_aware(base, FALSE);

	g_mutex_init(&aampcdmidecryptor->mutex);
	//GST_DEBUG_OBJECT(aampcdmidecryptor, "\n Initialized plugin mutex\n");
	g_cond_init(&aampcdmidecryptor->condition);
	aampcdmidecryptor->streamReceived = false;
	// Lock access to canWait to keep Coverity happy
	g_mutex_lock(&aampcdmidecryptor->mutex);
	aampcdmidecryptor->canWait = false;
	g_mutex_unlock(&aampcdmidecryptor->mutex);
	aampcdmidecryptor->protectionEvent = NULL;
	aampcdmidecryptor->sessionManager = NULL;
	aampcdmidecryptor->licenseManager = NULL;
	aampcdmidecryptor->drmSession = NULL;
	aampcdmidecryptor->aamp = NULL;
	aampcdmidecryptor->streamtype = eMEDIATYPE_MANIFEST;
	aampcdmidecryptor->firstsegprocessed = false;
	aampcdmidecryptor->selectedProtection = NULL;
	aampcdmidecryptor->decryptFailCount = 0;
	aampcdmidecryptor->hdcpOpProtectionFailCount = 0;
	aampcdmidecryptor->notifyDecryptError = true;
	aampcdmidecryptor->streamEncrypted = false;
	aampcdmidecryptor->ignoreSVP = false;
	aampcdmidecryptor->sinkCaps = NULL;
	aampcdmidecryptor->svpCtx = NULL;

	OCDMGstTransformCaps = (OpenCDMError(*)(GstCaps**))dlsym(RTLD_DEFAULT, ocdmgsttransformcaps);
	if (OCDMGstTransformCaps)
	GST_INFO_OBJECT(aampcdmidecryptor, "Has opencdm_gstreamer_transform_caps support \n");
	else
	GST_INFO_OBJECT(aampcdmidecryptor, "No opencdm_gstreamer_transform_caps support \n");
	//GST_DEBUG_OBJECT(aampcdmidecryptor, "******************Init called**********************\n");
}

void gst_aampcdmidecryptor_dispose(GObject * object)
{
	DEBUG_FUNC();

	GstAampCDMIDecryptor *aampcdmidecryptor =
			GST_AAMP_CDMI_DECRYPTOR(object);

	GST_DEBUG_OBJECT(aampcdmidecryptor, "dispose");

	if (aampcdmidecryptor->protectionEvent)
	{
		gst_event_unref(aampcdmidecryptor->protectionEvent);
		aampcdmidecryptor->protectionEvent = NULL;
	}
	if (aampcdmidecryptor->sinkCaps)
	{
		gst_caps_unref(aampcdmidecryptor->sinkCaps);
		aampcdmidecryptor->sinkCaps = NULL;
	}

	g_mutex_clear(&aampcdmidecryptor->mutex);
	g_cond_clear(&aampcdmidecryptor->condition);

	G_OBJECT_CLASS(gst_aampcdmidecryptor_parent_class)->dispose(object);
}

/*
 Append modified caps to dest, but only if it does not already exist in updated caps.
 */
static void gst_aampcdmicapsappendifnotduplicate(GstCaps* destCaps,
		GstStructure* cap)
{
	DEBUG_FUNC();

	bool duplicate = false;
	unsigned size = gst_caps_get_size(destCaps);
	for (unsigned index = 0; !duplicate && index < size; ++index)
	{
		GstStructure* tempCap = gst_caps_get_structure(destCaps, index);
		if (gst_structure_is_equal(tempCap, cap))
			duplicate = true;
	}

	if (!duplicate)
		gst_caps_append_structure(destCaps, cap);
	else
		gst_structure_free(cap);
}

static GstCaps *
gst_aampcdmidecryptor_transform_caps(GstBaseTransform * trans,
		GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
	DEBUG_FUNC();
	GstAampCDMIDecryptor *aampcdmidecryptor = GST_AAMP_CDMI_DECRYPTOR(trans);
	g_return_val_if_fail(direction != GST_PAD_UNKNOWN, NULL);
	unsigned size = gst_caps_get_size(caps);
	GstCaps* transformedCaps = gst_caps_new_empty();

	GST_DEBUG_OBJECT(trans, "direction: %s, caps: %" GST_PTR_FORMAT " filter:"
			" %" GST_PTR_FORMAT, (direction == GST_PAD_SRC) ? "src" : "sink", caps, filter);

	if(!aampcdmidecryptor->selectedProtection)
	{
		GstStructure *capstruct = gst_caps_get_structure(caps, 0);
		const gchar* capsinfo = gst_structure_get_string(capstruct, "protection-system");
		if(capsinfo != NULL)
		{
			if(!g_strcmp0(capsinfo, PLAYREADY_UUID))
			{
				aampcdmidecryptor->selectedProtection = PLAYREADY_UUID;
			}
			else if(!g_strcmp0(capsinfo, WIDEVINE_UUID))
			{
				aampcdmidecryptor->selectedProtection = WIDEVINE_UUID;
			}
			else if(!g_strcmp0(capsinfo, CLEARKEY_UUID))
			{
				 aampcdmidecryptor->selectedProtection = CLEARKEY_UUID;
				 aampcdmidecryptor->ignoreSVP = true;
			}
			else if(!g_strcmp0(capsinfo, VERIMATRIX_UUID))
			{
				aampcdmidecryptor->selectedProtection = VERIMATRIX_UUID;
			}
		}
		else
		{
			GST_DEBUG_OBJECT(trans, "can't find protection-system field from caps: %" GST_PTR_FORMAT, caps);
		}
	}

	for (unsigned i = 0; i < size; ++i)
	{
		GstStructure* in = gst_caps_get_structure(caps, i);
		GstStructure* out = NULL;

		if (direction == GST_PAD_SRC)
		{

			out = gst_structure_copy(in);
			/* filter out the video related fields from the up-stream caps,
			 because they are not relevant to the input caps of this element and
			 can cause caps negotiation failures with adaptive bitrate streams */
			for (int index = gst_structure_n_fields(out) - 1; index >= 0;
					--index)
			{
				const gchar* fieldName = gst_structure_nth_field_name(out,
						index);

				if (g_strcmp0(fieldName, "base-profile")
						&& g_strcmp0(fieldName, "codec_data")
						&& g_strcmp0(fieldName, "height")
						&& g_strcmp0(fieldName, "framerate")
						&& g_strcmp0(fieldName, "level")
						&& g_strcmp0(fieldName, "pixel-aspect-ratio")
						&& g_strcmp0(fieldName, "profile")
						&& g_strcmp0(fieldName, "rate")
						&& g_strcmp0(fieldName, "width"))
				{
					continue;
				}
				else
				{
					gst_structure_remove_field(out, fieldName);
					GST_TRACE_OBJECT(aampcdmidecryptor, "Removing field %s", fieldName);
				}
			}

			gst_structure_set(out, "protection-system", G_TYPE_STRING,
					aampcdmidecryptor->selectedProtection, "original-media-type",
					G_TYPE_STRING, gst_structure_get_name(in), NULL);

			gst_structure_set_name(out, "application/x-cenc");

		}
		else
		{
			if (!gst_structure_has_field(in, "original-media-type"))
			{
				GST_DEBUG_OBJECT(trans, "No original-media-type field in caps: %" GST_PTR_FORMAT, out);

				// Check if these caps are present in supported src pad caps in case direction is GST_PAD_SINK,
				// we can allow caps in this case, since plugin will let the data passthrough
				gboolean found = false;
				for (int j = 0; srcMimeTypes[j]; j++)
				{
					if (gst_structure_has_name(in, srcMimeTypes[j]))
					{
						found = true;
						break;
					}
				}
				if (found)
				{
					//From supported src type format
					out = gst_structure_copy(in);
				}
				else
				{
					continue;
				}
			}
			else
			{

				out = gst_structure_copy(in);
				gst_structure_set_name(out,
				gst_structure_get_string(out, "original-media-type"));

				/* filter out the DRM related fields from the down-stream caps */
				for (int j = 0; j < gst_structure_n_fields(in); ++j)
				{
					const gchar* fieldName = gst_structure_nth_field_name(in, j);

					if (g_str_has_prefix(fieldName, "protection-system")
						|| g_str_has_prefix(fieldName, "original-media-type"))
					{
						gst_structure_remove_field(out, fieldName);
					}
				}
			}
		}

		gst_aampcdmicapsappendifnotduplicate(transformedCaps, out);

#if defined(AMLOGIC)
	if (direction == GST_PAD_SINK && !gst_caps_is_empty(transformedCaps) && OCDMGstTransformCaps)
		OCDMGstTransformCaps(&transformedCaps);
#endif
	}

	if (filter)
	{
		GstCaps* intersection;

		GST_LOG_OBJECT(trans, "Using filter caps %" GST_PTR_FORMAT, filter);
		intersection = gst_caps_intersect_full(transformedCaps, filter,
				GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref(transformedCaps);
		transformedCaps = intersection;
	}

	GST_LOG_OBJECT(trans, "returning %" GST_PTR_FORMAT, transformedCaps);
	if (direction == GST_PAD_SINK && !gst_caps_is_empty(transformedCaps))
	{
		g_mutex_lock(&aampcdmidecryptor->mutex);
		// clean up previous caps
		if (aampcdmidecryptor->sinkCaps)
		{
			gst_caps_unref(aampcdmidecryptor->sinkCaps);
			aampcdmidecryptor->sinkCaps = NULL;
		}
		aampcdmidecryptor->sinkCaps = gst_caps_copy(transformedCaps);
		g_mutex_unlock(&aampcdmidecryptor->mutex);
		GST_DEBUG_OBJECT(trans, "Set sinkCaps to %" GST_PTR_FORMAT, aampcdmidecryptor->sinkCaps);
	}
	return transformedCaps;
}

#ifdef USE_OPENCDM_ADAPTER

static GstFlowReturn gst_aampcdmidecryptor_transform_ip(
		GstBaseTransform * trans, GstBuffer * buffer)
{
	DEBUG_FUNC();

	GstAampCDMIDecryptor *aampcdmidecryptor =
			GST_AAMP_CDMI_DECRYPTOR(trans);

	GstFlowReturn result = GST_FLOW_OK;

	guint subSampleCount = 0;
	guint ivSize;
	gboolean encrypted;
	const GValue* value;
	GstBuffer* ivBuffer = NULL;
	GstBuffer* keyIDBuffer = NULL;
	GstBuffer* subsamplesBuffer = NULL;
	GstMapInfo subSamplesMap;
	GstProtectionMeta* protectionMeta = NULL;
	gboolean mutexLocked = FALSE;
	int errorCode;

	GST_DEBUG_OBJECT(aampcdmidecryptor, "Processing buffer");

	if (!buffer)
	{
		GST_ERROR_OBJECT(aampcdmidecryptor,"Failed to get writable buffer");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	protectionMeta =
			reinterpret_cast<GstProtectionMeta*>(gst_buffer_get_protection_meta(buffer));

	g_mutex_lock(&aampcdmidecryptor->mutex);
	mutexLocked = TRUE;
	if (!protectionMeta)
	{
		GST_DEBUG_OBJECT(aampcdmidecryptor,
				"Failed to get GstProtection metadata from buffer %p, could be clear buffer",buffer);
#if defined(AMLOGIC)
		// call decrypt even for clear samples in order to copy it to a secure buffer. If secure buffers are not supported
		// decrypt() call will return without doing anything
		if (aampcdmidecryptor->drmSession != NULL)
		   errorCode = aampcdmidecryptor->drmSession->decrypt(keyIDBuffer, ivBuffer, buffer, subSampleCount, subsamplesBuffer, aampcdmidecryptor->sinkCaps);
		else
		{ /* If drmSession creation failed, then the call will be aborted here */
			result = GST_FLOW_NOT_SUPPORTED;
			GST_ERROR_OBJECT(aampcdmidecryptor, "drmSession is **** NULL ****, returning GST_FLOW_NOT_SUPPORTED");
		}
#endif
		goto free_resources;
	}

	GST_TRACE_OBJECT(aampcdmidecryptor,
			"Mutex acquired, stream received: %s canWait: %d",
			aampcdmidecryptor->streamReceived ? "yes" : "no", aampcdmidecryptor->canWait);

	if (!aampcdmidecryptor->canWait
			&& !aampcdmidecryptor->streamReceived)
	{
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	if (!aampcdmidecryptor->firstsegprocessed)
	{
		GST_DEBUG_OBJECT(aampcdmidecryptor, "\n\nWaiting for key\n");
	}
	// The key might not have been received yet. Wait for it.
	if (!aampcdmidecryptor->streamReceived)
		g_cond_wait(&aampcdmidecryptor->condition,
				&aampcdmidecryptor->mutex);

	if (!aampcdmidecryptor->streamReceived)
	{
		GST_ERROR_OBJECT(aampcdmidecryptor,
				"Condition signaled from state change transition. Aborting.");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	/* If drmSession creation failed, then the call will be aborted here */
	if (aampcdmidecryptor->drmSession == NULL)
	{
		GST_ERROR_OBJECT(aampcdmidecryptor, "drmSession is invalid **** NULL ****. Aborting.");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	GST_TRACE_OBJECT(aampcdmidecryptor, "Got key event ; Proceeding with decryption");

	if (!gst_structure_get_uint(protectionMeta->info, "iv_size", &ivSize))
	{
		GST_ERROR_OBJECT(aampcdmidecryptor, "failed to get iv_size");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	if (!gst_structure_get_boolean(protectionMeta->info, "encrypted",
			&encrypted))
	{
		GST_ERROR_OBJECT(aampcdmidecryptor,
				"failed to get encrypted flag");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	// Unencrypted sample.
	if (!ivSize || !encrypted)
		goto free_resources;

	GST_TRACE_OBJECT(trans, "protection meta: %" GST_PTR_FORMAT, protectionMeta->info);
	if (!gst_structure_get_uint(protectionMeta->info, "subsample_count",
			&subSampleCount))
	{
		GST_ERROR_OBJECT(aampcdmidecryptor,
				"failed to get subsample_count");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	value = gst_structure_get_value(protectionMeta->info, "iv");
	if (!value)
	{
		GST_ERROR_OBJECT(aampcdmidecryptor, "Failed to get IV for sample");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	ivBuffer = gst_value_get_buffer(value);

	value = gst_structure_get_value(protectionMeta->info, "kid");
	if (!value) {
		GST_ERROR_OBJECT(aampcdmidecryptor, "Failed to get kid for sample");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	keyIDBuffer = gst_value_get_buffer(value);

	if (subSampleCount)
	{
		value = gst_structure_get_value(protectionMeta->info, "subsamples");
		if (!value)
		{
			GST_ERROR_OBJECT(aampcdmidecryptor,
					"Failed to get subsamples");
			result = GST_FLOW_NOT_SUPPORTED;
			goto free_resources;
		}
		subsamplesBuffer = gst_value_get_buffer(value);
		if (!gst_buffer_map(subsamplesBuffer, &subSamplesMap, GST_MAP_READ))
		{
			GST_ERROR_OBJECT(aampcdmidecryptor,
					"Failed to map subsample buffer");
			result = GST_FLOW_NOT_SUPPORTED;
			goto free_resources;
		}
	}

	errorCode = aampcdmidecryptor->drmSession->decrypt(keyIDBuffer, ivBuffer, buffer, subSampleCount, subsamplesBuffer, aampcdmidecryptor->sinkCaps);

	aampcdmidecryptor->streamEncrypted = true;
	if (errorCode != 0 || aampcdmidecryptor->hdcpOpProtectionFailCount)
	{
	if(errorCode == HDCP_OUTPUT_PROTECTION_FAILURE)
	{
		aampcdmidecryptor->hdcpOpProtectionFailCount++;
	}
	else if(aampcdmidecryptor->hdcpOpProtectionFailCount)
	{
		if(aampcdmidecryptor->hdcpOpProtectionFailCount >= DECRYPT_FAILURE_THRESHOLD) {
			GstStructure *newmsg = gst_structure_new("HDCPProtectionFailure", "message", G_TYPE_STRING,"HDCP Output Protection Error", NULL);
			gst_element_post_message(reinterpret_cast<GstElement*>(aampcdmidecryptor),gst_message_new_application (GST_OBJECT (aampcdmidecryptor), newmsg));
		}
		aampcdmidecryptor->hdcpOpProtectionFailCount = 0;
	}
	else
	{
		GST_ERROR_OBJECT(aampcdmidecryptor, "decryption failed; error code %d\n",errorCode);
		aampcdmidecryptor->decryptFailCount++;
		if(aampcdmidecryptor->decryptFailCount >= DECRYPT_FAILURE_THRESHOLD && aampcdmidecryptor->notifyDecryptError )
		{
			aampcdmidecryptor->notifyDecryptError = false;
			GError *error;
			if(errorCode == HDCP_COMPLIANCE_CHECK_FAILURE)
			{
				// Failure - 2.2 vs 1.4 HDCP
				error = g_error_new(GST_STREAM_ERROR , GST_STREAM_ERROR_FAILED, "HDCP Compliance Check Failure");
			}
			else
			{
				error = g_error_new(GST_STREAM_ERROR , GST_STREAM_ERROR_FAILED, "Decrypt Error: code %d", errorCode);
			}
			gst_element_post_message(reinterpret_cast<GstElement*>(aampcdmidecryptor), gst_message_new_error (GST_OBJECT (aampcdmidecryptor), error, "Decrypt Failed"));
			g_error_free(error);
			result = GST_FLOW_ERROR;
		}
		goto free_resources;
	}
	}
	else
	{
		aampcdmidecryptor->decryptFailCount = 0;
	aampcdmidecryptor->hdcpOpProtectionFailCount = 0;
		if (aampcdmidecryptor->streamtype == eMEDIATYPE_AUDIO)
		{
			GST_DEBUG_OBJECT(aampcdmidecryptor, "Decryption successful for Audio packets");
		}
		else
		{
			GST_DEBUG_OBJECT(aampcdmidecryptor, "Decryption successful for Video packets");
		}
	}

	if (!aampcdmidecryptor->firstsegprocessed
			&& aampcdmidecryptor->aamp)
	{
		if (aampcdmidecryptor->streamtype == eMEDIATYPE_VIDEO)
		{
			GST_INFO_OBJECT(aampcdmidecryptor,"profile end decrypt video");
			aampcdmidecryptor->aamp->profiler.ProfileEnd(
					PROFILE_BUCKET_DECRYPT_VIDEO);
		} else if (aampcdmidecryptor->streamtype == eMEDIATYPE_AUDIO)
		{
			GST_INFO_OBJECT(aampcdmidecryptor,"profile end decrypt audio");
			aampcdmidecryptor->aamp->profiler.ProfileEnd(
					PROFILE_BUCKET_DECRYPT_AUDIO);
		}
		aampcdmidecryptor->firstsegprocessed = true;
	}

	free_resources:

	if (!aampcdmidecryptor->firstsegprocessed
			&& aampcdmidecryptor->aamp)
	{
	if(!aampcdmidecryptor->streamEncrypted)
	{
		if (aampcdmidecryptor->streamtype == eMEDIATYPE_VIDEO)
		{
			GST_INFO_OBJECT(aampcdmidecryptor,"profile end decrypt video (clear)");
			aampcdmidecryptor->aamp->profiler.ProfileEnd(
					PROFILE_BUCKET_DECRYPT_VIDEO);
		} else if (aampcdmidecryptor->streamtype == eMEDIATYPE_AUDIO)
		{
			GST_INFO_OBJECT(aampcdmidecryptor,"profile end decrypt audio (clear)");
			aampcdmidecryptor->aamp->profiler.ProfileEnd(
					PROFILE_BUCKET_DECRYPT_AUDIO);
		}
	}
	else
	{
		if (aampcdmidecryptor->streamtype == eMEDIATYPE_VIDEO)
		{
			aampcdmidecryptor->aamp->profiler.ProfileError(PROFILE_BUCKET_DECRYPT_VIDEO, (int)result);
		} else if (aampcdmidecryptor->streamtype == eMEDIATYPE_AUDIO)
		{
			aampcdmidecryptor->aamp->profiler.ProfileError(PROFILE_BUCKET_DECRYPT_AUDIO, (int)result);
		}
	}
		aampcdmidecryptor->firstsegprocessed = true;
	}

	if (subsamplesBuffer)
		gst_buffer_unmap(subsamplesBuffer, &subSamplesMap);

	if (protectionMeta)
		gst_buffer_remove_meta(buffer,
				reinterpret_cast<GstMeta*>(protectionMeta));

	if (mutexLocked)
		g_mutex_unlock(&aampcdmidecryptor->mutex);
	return result;
}
#endif // USE_OPENCDM_ADAPTER


/* sink event handlers */
static gboolean gst_aampcdmidecryptor_sink_event(GstBaseTransform * trans,
		GstEvent * event)
{
	DEBUG_FUNC();

	GstAampCDMIDecryptor *aampcdmidecryptor =
			GST_AAMP_CDMI_DECRYPTOR(trans);
	gboolean result = FALSE;

	switch (GST_EVENT_TYPE(event))
	{

	//GST_EVENT_PROTECTION has information about encryption and contains initData for DRM library
	//This is the starting point of DRM activities.
	case GST_EVENT_PROTECTION:
	{
		const gchar* systemId;
		const gchar* origin;
		unsigned char *outData = NULL;
		size_t outDataLen = 0;
		GstBuffer* initdatabuffer;

	if(NULL == aampcdmidecryptor)
	{
		GST_ERROR_OBJECT(aampcdmidecryptor,
				"Invalid CDMI Decryptor Instance\n");
		result = FALSE;
		break;
	}

	//We need to get the sinkpad for sending upstream queries and
		//getting the current pad capability ie, VIDEO or AUDIO
		//in order to support tune time profiling
		GstPad * sinkpad = gst_element_get_static_pad(
				reinterpret_cast<GstElement*>(aampcdmidecryptor), "sink");
		//Query to get the aamp reference from gstaamp
		//this aamp instance is used for profiling
		if (NULL == aampcdmidecryptor->aamp)
		{
			const GValue *val;
			GstStructure * structure = gst_structure_new("get_aamp_instance",
					"aamp_instance", G_TYPE_POINTER, 0, NULL);
			GstQuery *query = gst_query_new_custom(GST_QUERY_CUSTOM, structure);
			gboolean res = gst_pad_peer_query(sinkpad, query);
			if (res)
			{
				structure = (GstStructure *) gst_query_get_structure(query);
				val = (gst_structure_get_value(structure, "aamp_instance"));
				aampcdmidecryptor->aamp =
						(PrivateInstanceAAMP*) g_value_get_pointer(val);
			}
			gst_query_unref(query);
		}

		if (aampcdmidecryptor->aamp == NULL)
		{
			GST_ERROR_OBJECT(aampcdmidecryptor,
					"aampcdmidecryptor unable to retrieve aamp instance\n");
			result = FALSE;
			break;
		}

		GST_DEBUG_OBJECT(aampcdmidecryptor,
				"Received encrypted event: Proceeding to parse initData\n");
		gst_event_parse_protection(event, &systemId, &initdatabuffer, &origin);
		GST_DEBUG_OBJECT(aampcdmidecryptor, "systemId: %s", systemId);
		GST_DEBUG_OBJECT(aampcdmidecryptor, "origin: %s", origin);
		/** If WideVine KeyID workaround is present check the systemId is clearKey **/
		if (aampcdmidecryptor->aamp->mIsWVKIDWorkaround){
			if(!g_str_equal(systemId, CLEARKEY_UUID) ){
				gst_event_unref(event);
				result = TRUE;
				break;
			}
			GST_DEBUG_OBJECT(aampcdmidecryptor, "\nWideVine KeyID workaround is present, Select KeyID from Clear Key\n");
			systemId = WIDEVINE_UUID ;

		}else{ /* else check the selected protection system */
			if (!g_str_equal(systemId, aampcdmidecryptor->selectedProtection))
			{
				gst_event_unref(event);
				result = TRUE;
				break;
			}
		}

		GstMapInfo mapInfo;
		if (!gst_buffer_map(initdatabuffer, &mapInfo, GST_MAP_READ))
			break;
		GST_DEBUG_OBJECT(aampcdmidecryptor, "scheduling keyNeeded event");
		
		if (eMEDIATYPE_MANIFEST == aampcdmidecryptor->streamtype)
		{
			GstCaps* caps = gst_pad_get_current_caps(sinkpad);
			GstStructure *capstruct = gst_caps_get_structure(caps, 0);
			const gchar* capsinfo = gst_structure_get_string(capstruct,
					"original-media-type");

			if (!g_strcmp0(capsinfo, "audio/mpeg"))
			{
				aampcdmidecryptor->streamtype = eMEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "audio/x-opus"))
			{
				aampcdmidecryptor->streamtype = eMEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "audio/x-eac3") || !g_strcmp0(capsinfo, "audio/x-ac3"))
			{
				aampcdmidecryptor->streamtype = eMEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "audio/x-gst-fourcc-ec_3"))
			{
				aampcdmidecryptor->streamtype = eMEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "video/x-h264"))
			{
				aampcdmidecryptor->streamtype = eMEDIATYPE_VIDEO;
			}
			else if (!g_strcmp0(capsinfo, "video/x-h265"))
			{
				aampcdmidecryptor->streamtype = eMEDIATYPE_VIDEO;
			}
			else
			{
				gst_caps_unref(caps);
				result = false;
				break;
			}
			gst_caps_unref(caps);
		}

		if (aampcdmidecryptor->aamp->mIsWVKIDWorkaround){
			GST_DEBUG_OBJECT(aampcdmidecryptor, "\nWideVine KeyID workaround is present, Applying WideVine KID workaround\n");
			outData = aampcdmidecryptor->aamp->ReplaceKeyIDPsshData(reinterpret_cast<unsigned char *>(mapInfo.data), mapInfo.size, outDataLen);
			if (NULL == outData){
				GST_ERROR_OBJECT(aampcdmidecryptor, "\nFailed to Apply WideVine KID workaround!\n");
				break;
			}
		}

		if(!aampcdmidecryptor->aamp->licenceFromManifest)
		{
			aampcdmidecryptor->aamp->profiler.ProfileBegin(
					PROFILE_BUCKET_LA_TOTAL);
		}
		g_mutex_lock(&aampcdmidecryptor->mutex);
		GST_DEBUG_OBJECT(aampcdmidecryptor, "\n acquired lock for mutex\n");
		aampcdmidecryptor->licenseManager  = aampcdmidecryptor->aamp->mDRMLicenseManager;
		DrmMetaDataEventPtr e = std::make_shared<DrmMetaDataEvent>(AAMP_TUNE_FAILURE_UNKNOWN, "", 0, 0, false, std::string{});
		if (aampcdmidecryptor->aamp->mIsWVKIDWorkaround){
			aampcdmidecryptor->drmSession =
				aampcdmidecryptor->licenseManager->createDrmSession(
						reinterpret_cast<const char *>(systemId), eMEDIAFORMAT_DASH,
						outData, outDataLen, aampcdmidecryptor->streamtype, aampcdmidecryptor->aamp, e, nullptr, false);
		}else{
			aampcdmidecryptor->drmSession =
				aampcdmidecryptor->licenseManager->createDrmSession(
						reinterpret_cast<const char *>(systemId), eMEDIAFORMAT_DASH,
						reinterpret_cast<const unsigned char *>(mapInfo.data),
						mapInfo.size, aampcdmidecryptor->streamtype, aampcdmidecryptor->aamp, e, nullptr, false);
		}
		if (NULL == aampcdmidecryptor->drmSession)
		{
/* For  Avoided setting 'streamReceived' as FALSE if createDrmSession() failed after a successful case.
 * Set to FALSE is already handled on gst_aampcdmidecryptor_init() as part of initialization.
 */
#if 0
			aampcdmidecryptor->streamReceived = FALSE;
#endif /* 0 */

			/* Need to reset canWait to skip conditional wait in "gst_aampcdmidecryptor_transform_ip to avoid deadlock
			 *		scenario on drm session failure
			 */
			aampcdmidecryptor->canWait = false;
		/* session manager fails to create session when state is inactive. Skip sending error event
		 * in this scenario. Later player will change it to active after processing SetLanguage(), or for the next Tune.
		 */
		if(SessionMgrState::eSESSIONMGR_ACTIVE == aampcdmidecryptor->licenseManager->getSessionMgrState())
		{
			if(!aampcdmidecryptor->aamp->licenceFromManifest)
			{
			AAMPTuneFailure failure = e->getFailure();
			if(AAMP_TUNE_FAILURE_UNKNOWN != failure)
				 {
				long responseCode = e->getResponseCode();
				bool selfAbort = (failure == AAMP_TUNE_LICENCE_REQUEST_FAILED && (responseCode == CURLE_ABORTED_BY_CALLBACK || responseCode == CURLE_WRITE_ERROR));
				if (!selfAbort)
				{
				aampcdmidecryptor->aamp->SendErrorEvent(failure);
				}
				aampcdmidecryptor->aamp->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL, (int)failure);
				aampcdmidecryptor->aamp->profiler.SetDrmErrorCode((int)failure);
			}
			else
			{
				aampcdmidecryptor->aamp->profiler.ProfileError(PROFILE_BUCKET_LA_TOTAL);
			}
			}
			GST_ERROR_OBJECT(aampcdmidecryptor,"Failed to create DRM Session\n");
		}
			result = TRUE;
		}
	else
		{
			aampcdmidecryptor->streamReceived = TRUE;
			if(!aampcdmidecryptor->aamp->licenceFromManifest)
			{
				aampcdmidecryptor->aamp->profiler.ProfileEnd(
						PROFILE_BUCKET_LA_TOTAL);
			}

			if (!aampcdmidecryptor->firstsegprocessed)
			{
				if (aampcdmidecryptor->streamtype == eMEDIATYPE_VIDEO)
				{
					GST_INFO_OBJECT(aampcdmidecryptor,"Starting decryption profiling for video");
					aampcdmidecryptor->aamp->profiler.ProfileBegin(
							PROFILE_BUCKET_DECRYPT_VIDEO);
				} else if (aampcdmidecryptor->streamtype == eMEDIATYPE_AUDIO)
				{
					GST_INFO_OBJECT(aampcdmidecryptor,"Starting decryption profiling for audio");
					aampcdmidecryptor->aamp->profiler.ProfileBegin(
							PROFILE_BUCKET_DECRYPT_AUDIO);
				}
			}

			result = TRUE;
		}
		g_cond_signal(&aampcdmidecryptor->condition);
		g_mutex_unlock(&aampcdmidecryptor->mutex);
		GST_DEBUG_OBJECT(aampcdmidecryptor, "\n releasing ...................... mutex\n");

		gst_object_unref(sinkpad);
		gst_buffer_unmap(initdatabuffer, &mapInfo);
		gst_event_unref(event);
		if(outData){
			free(outData);
			outData = NULL;
		}

		break;
	}
	default:
		result = GST_BASE_TRANSFORM_CLASS(
				gst_aampcdmidecryptor_parent_class)->sink_event(trans,
				event);
		break;
	}

	return result;
}

static GstStateChangeReturn gst_aampcdmidecryptor_changestate(
		GstElement* element, GstStateChange transition)
{
	DEBUG_FUNC();

	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstAampCDMIDecryptor* aampcdmidecryptor =
			GST_AAMP_CDMI_DECRYPTOR(element);

	switch (transition)
	{
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_DEBUG_OBJECT(aampcdmidecryptor, "READY->PAUSED");
		g_mutex_lock(&aampcdmidecryptor->mutex);
		aampcdmidecryptor->canWait = true;
		g_mutex_unlock(&aampcdmidecryptor->mutex);
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_DEBUG_OBJECT(aampcdmidecryptor, "PAUSED->READY");
		g_mutex_lock(&aampcdmidecryptor->mutex);
		aampcdmidecryptor->canWait = false;
		g_cond_signal(&aampcdmidecryptor->condition);
		g_mutex_unlock(&aampcdmidecryptor->mutex);
		break;
#if defined(AMLOGIC)
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_DEBUG_OBJECT(aampcdmidecryptor, "NULL->READY");
		if (aampcdmidecryptor->svpCtx == NULL)
		  gst_svp_ext_get_context(&aampcdmidecryptor->svpCtx, Server, 0);
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_DEBUG_OBJECT(aampcdmidecryptor, "READY->NULL");
		if (aampcdmidecryptor->svpCtx) {
			gst_svp_ext_free_context(aampcdmidecryptor->svpCtx);
			aampcdmidecryptor->svpCtx = NULL;
		}
		break;
#endif
	default:
		break;
	}

	ret =
			GST_ELEMENT_CLASS(gst_aampcdmidecryptor_parent_class)->change_state(
					element, transition);
	return ret;
}

static void gst_aampcdmidecryptor_set_property(GObject * object,
		guint prop_id, const GValue * value, GParamSpec * pspec)
{
	DEBUG_FUNC();

	GstAampCDMIDecryptor* aampcdmidecryptor =
			GST_AAMP_CDMI_DECRYPTOR(object);
	switch (prop_id)
	{
	case PROP_AAMP:
		GST_OBJECT_LOCK(aampcdmidecryptor);
		aampcdmidecryptor->aamp =
				(PrivateInstanceAAMP*) g_value_get_pointer(value);
		GST_DEBUG_OBJECT(aampcdmidecryptor,
				"Received aamp instance from appsrc\n");
		GST_OBJECT_UNLOCK(aampcdmidecryptor);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean gst_aampcdmidecryptor_accept_caps(GstBaseTransform * trans,
		GstPadDirection direction, GstCaps * caps)
{
	gboolean ret = TRUE;
	GST_DEBUG_OBJECT (trans, "received accept caps with direction: %s caps: %" GST_PTR_FORMAT, (direction == GST_PAD_SRC) ? "src" : "sink", caps);

	GstCaps *allowedCaps = NULL;

	if (direction == GST_PAD_SINK)
	{
		allowedCaps = gst_pad_query_caps(trans->sinkpad, caps);
	}
	else
	{
		allowedCaps = gst_pad_query_caps(trans->srcpad, caps);
	}

	if (!allowedCaps)
	{
		GST_ERROR_OBJECT(trans, "Error while query caps on %s pad of plugin with filter caps: %" GST_PTR_FORMAT, (direction == GST_PAD_SRC) ? "src" : "sink", caps);
		ret = FALSE;
	}
	else
	{
		GST_DEBUG_OBJECT(trans, "Allowed caps: %" GST_PTR_FORMAT, allowedCaps);
		ret = gst_caps_is_subset(caps, allowedCaps);
		gst_caps_unref(allowedCaps);
	}

	// Check if these are same as src pad caps in case direction is GST_PAD_SINK,
	// we can let it through in this case
	if (ret == FALSE && direction == GST_PAD_SINK)
	{
		guint size = gst_caps_get_size(caps);
		for (guint i = 0; i < size; i++)
		{
			GstStructure* inCaps = gst_caps_get_structure(caps, i);
			for (int j = 0; srcMimeTypes[j]; j++)
			{
				if (gst_structure_has_name(inCaps, srcMimeTypes[j]))
				{
					GST_DEBUG_OBJECT(trans, "found the requested caps in supported src mime types (type:%s), respond as supported!", srcMimeTypes[j]);
					ret = TRUE;
					break;
				}
			}
		}
	}
	GST_DEBUG_OBJECT(trans, "Return from accept_caps: %d", ret);
	return ret;
}
#endif // UBUNTU
