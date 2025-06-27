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

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "gstcdmidecryptor.h"
#include <open_cdm.h>
#include <open_cdm_adapter.h>
#if defined(AMLOGIC)
#include <gst_svp_meta.h>
#endif
#include <dlfcn.h>
#include <stdio.h>
#include "DrmConstants.h"

GST_DEBUG_CATEGORY_STATIC ( gst_cdmidecryptor_debug_category);
#define GST_CAT_DEFAULT  gst_cdmidecryptor_debug_category
#define DECRYPT_FAILURE_THRESHOLD 5

enum
{
	PROP_0, PROP_PLAYER, PROP_DRM_SESSION_MANAGER
};

//#define FUNCTION_DEBUG 1
#ifdef FUNCTION_DEBUG
#define DEBUG_FUNC()    g_warning("####### %s : %d ####\n", __FUNCTION__, __LINE__);
#else
#define DEBUG_FUNC()
#endif

/**
 * @brief Replaces the Key ID in Widevine PSSH data with the Key ID from Clear Key PSSH data.
 *
 * This function modifies the Widevine PSSH data by replacing its Key ID with the Key ID
 * from the Clear Key PSSH data. It allocates memory for the modified PSSH data and returns it.
 *
 * @param[in] InputData Pointer to the input PSSH data.
 * @param[in] InputDataLength Length of the input PSSH data.
 * @param[out] OutputDataLength Reference to store the length of the modified PSSH data.
 * @return Pointer to the modified PSSH data. Returns NULL if the input data is invalid
 *         or memory allocation fails.
 *
 * @note The caller is responsible for freeing the memory allocated for the modified PSSH data.
 */
static unsigned char* ReplaceKIDPsshData(const unsigned char *InputData, const size_t InputDataLength,  size_t & OutputDataLength)
{
	unsigned char *outputData = NULL;
	unsigned int WIDEVINE_PSSH_KEYID_OFFSET = 36u;
	unsigned int CK_PSSH_KEYID_OFFSET = 32u;
	unsigned int COMMON_KEYID_SIZE = 16u;
	unsigned char WVSamplePSSH[] = {
		0x00, 0x00, 0x00, 0x3c,
		0x70, 0x73, 0x73, 0x68,
		0x00, 0x00, 0x00, 0x00,
		0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,
		0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed,
		0x00, 0x00, 0x00, 0x1c, 0x08, 0x01, 0x12, 0x10,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //dummy KeyId (16 byte)
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //dummy KeyId (16 byte)
		0x22, 0x06, 0x74, 0x65, 0x73, 0x74, 0x5f, 0x37
	};
	if (InputData)
	{
		MW_LOG_INFO("Converting system UUID of PSSH data size (%zu)", InputDataLength);
#ifdef ENABLE_DUMP
		MW_LOG_INFO("PSSH Data (%d) Before Modification : ", InputDataLength);
		DumpBlob(InputData, InputDataLength);
#endif
		/** Replace KeyID of WV PSSH Data with Key ID of CK PSSH Data **/
		int iWVpssh = WIDEVINE_PSSH_KEYID_OFFSET;
		int CKPssh = CK_PSSH_KEYID_OFFSET;
		int size = 0;
		if (CK_PSSH_KEYID_OFFSET+COMMON_KEYID_SIZE <=  InputDataLength)
		{
			for (; size < COMMON_KEYID_SIZE; ++size, ++iWVpssh, ++CKPssh  )
			{
				/** Transfer KeyID from CK PSSH data to WV PSSH Data **/
				WVSamplePSSH[iWVpssh] = InputData[CKPssh];
			}
			/** Allocate WV PSSH Data memory and transfer local data **/
			outputData = (unsigned char *)malloc(sizeof(WVSamplePSSH));
			if (outputData)
			{
				memcpy(outputData, WVSamplePSSH, sizeof(WVSamplePSSH));
				OutputDataLength = sizeof(WVSamplePSSH);
#ifdef ENABLE_DUMP
				MW_LOG_INFO("PSSH Data (%d) after Modification : ", OutputDataLength);													    DumpBlob(outputData, OutputDataLength);
#endif
				return outputData;
			}
			else
			{
				MW_LOG_ERR("PSSH Data Memory allocation failed ");
			}
		}
		else
		{
			//Invalid PSSH data
			MW_LOG_ERR("Invalid Clear Key PSSH data ");															    }
	}
	else
	{																					//Invalid argument - PSSH Data
		MW_LOG_ERR("Invalid Argument of PSSH data ");
	}
	return NULL;
}

static const gchar *srcMimeTypes[] = { "video/x-h264", "video/x-h264(memory:SecMem)", "audio/mpeg", "video/x-h265", "video/x-h265(memory:SecMem)", "audio/x-eac3", "audio/x-gst-fourcc-ec_3", "audio/x-ac3","audio/x-opus", nullptr };

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstCDMIDecryptor, gst_cdmidecryptor, GST_TYPE_BASE_TRANSFORM,
		GST_DEBUG_CATEGORY_INIT (gst_cdmidecryptor_debug_category, "cdmidecryptor", 0,
				"debug category for cdmidecryptor element"));

#if defined(UBUNTU)
// stubs to avoid strange ubuntu-specific SegFault while running L2 Plugin tests
static void gst_cdmidecryptor_class_init( GstCDMIDecryptorClass * klass)
{
	printf( "gst_cdmidecryptor_class_init\n" );
}
static void gst_cdmidecryptor_init( GstCDMIDecryptor *cdmidecryptor)
{
	printf( "gst_cdmidecryptor_init\n" );
}
#else
/* prototypes */
static void gst_cdmidecryptor_dispose(GObject*);
static GstCaps *gst_cdmidecryptor_transform_caps(
		GstBaseTransform * trans, GstPadDirection direction, GstCaps * caps,
		GstCaps * filter);
static gboolean gst_cdmidecryptor_sink_event(GstBaseTransform * trans,
		GstEvent * event);
static GstFlowReturn gst_cdmidecryptor_transform_ip(
		GstBaseTransform * trans, GstBuffer * buf);
static GstStateChangeReturn gst_cdmidecryptor_changestate(
		GstElement* element, GstStateChange transition);
static void gst_cdmidecryptor_set_property(GObject * object,
		guint prop_id, const GValue * value, GParamSpec * pspec);
static gboolean gst_cdmidecryptor_accept_caps(GstBaseTransform * trans,
		GstPadDirection direction, GstCaps * caps);
static OpenCDMError(*OCDMGstTransformCaps)(GstCaps **);

static void gst_cdmidecryptor_class_init(
		GstCDMIDecryptorClass *klass)
{
	DEBUG_FUNC();
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
	GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

	gobject_class->set_property = gst_cdmidecryptor_set_property;
	gobject_class->dispose = gst_cdmidecryptor_dispose;

	g_object_class_install_property(gobject_class, PROP_PLAYER,
			g_param_spec_pointer("aamp", "AAMP",
					"DrmSessionManager instance for DrmCallback", G_PARAM_WRITABLE));

	g_object_class_install_property(gobject_class, PROP_DRM_SESSION_MANAGER,
			g_param_spec_pointer("drm-session-manager", "DRM Session Manager",
				"Pointer to DRM session manager", G_PARAM_WRITABLE));

	GST_ELEMENT_CLASS(klass)->change_state =
			gst_cdmidecryptor_changestate;

	base_transform_class->transform_caps = GST_DEBUG_FUNCPTR(
			gst_cdmidecryptor_transform_caps);
	base_transform_class->sink_event = GST_DEBUG_FUNCPTR(
			gst_cdmidecryptor_sink_event);
	base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(
			gst_cdmidecryptor_transform_ip);

#if !defined(AMLOGIC)
	base_transform_class->accept_caps = GST_DEBUG_FUNCPTR(
			gst_cdmidecryptor_accept_caps);
#endif
	base_transform_class->transform_ip_on_passthrough = FALSE;

	gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
			"Decrypt encrypted content with CDMi",
			GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
			"Decrypts streams encrypted using Encryption.",
			"comcast");
	//GST_DEBUG_OBJECT(cdmidecryptor, "Inside custom plugin init\n");
}

static void gst_cdmidecryptor_init(
		GstCDMIDecryptor *cdmidecryptor)
{
	DEBUG_FUNC();
	const char* ocdmgsttransformcaps = "opencdm_gstreamer_transform_caps";
	GstBaseTransform* base = GST_BASE_TRANSFORM(cdmidecryptor);

	gst_base_transform_set_in_place(base, TRUE);
	gst_base_transform_set_passthrough(base, FALSE);
	gst_base_transform_set_gap_aware(base, FALSE);

	g_mutex_init(&cdmidecryptor->mutex);
	//GST_DEBUG_OBJECT(cdmidecryptor, "\n Initialized plugin mutex\n");
	g_cond_init(&cdmidecryptor->condition);
	cdmidecryptor->streamReceived = false;
	// Lock access to canWait to keep Coverity happy
	g_mutex_lock(&cdmidecryptor->mutex);
	cdmidecryptor->canWait = false;
	g_mutex_unlock(&cdmidecryptor->mutex);
	cdmidecryptor->protectionEvent = NULL;
	cdmidecryptor->sessionManager = NULL;
	cdmidecryptor->drmSession = NULL;
	cdmidecryptor->player = NULL;
	cdmidecryptor->mediaType = eGST_MEDIATYPE_MANIFEST;
	cdmidecryptor->firstsegprocessed = false;
	cdmidecryptor->selectedProtection = NULL;
	cdmidecryptor->decryptFailCount = 0;
	cdmidecryptor->hdcpOpProtectionFailCount = 0;
	cdmidecryptor->notifyDecryptError = true;
	cdmidecryptor->streamEncrypted = false;
	cdmidecryptor->ignoreSVP = false;
	cdmidecryptor->sinkCaps = NULL;
	cdmidecryptor->svpCtx = NULL;

	OCDMGstTransformCaps = (OpenCDMError(*)(GstCaps**))dlsym(RTLD_DEFAULT, ocdmgsttransformcaps);
	if (OCDMGstTransformCaps)
	GST_INFO_OBJECT(cdmidecryptor, "Has opencdm_gstreamer_transform_caps support \n");
	else
	GST_INFO_OBJECT(cdmidecryptor, "No opencdm_gstreamer_transform_caps support \n");
	//GST_DEBUG_OBJECT(cdmidecryptor, "******************Init called**********************\n");
}

void gst_cdmidecryptor_dispose(GObject * object)
{
	DEBUG_FUNC();

	GstCDMIDecryptor *cdmidecryptor =
			GST_CDMI_DECRYPTOR(object);

	GST_DEBUG_OBJECT(cdmidecryptor, "dispose");

	if (cdmidecryptor->protectionEvent)
	{
		gst_event_unref(cdmidecryptor->protectionEvent);
		cdmidecryptor->protectionEvent = NULL;
	}
	if (cdmidecryptor->sinkCaps)
	{
		gst_caps_unref(cdmidecryptor->sinkCaps);
		cdmidecryptor->sinkCaps = NULL;
	}

	g_mutex_clear(&cdmidecryptor->mutex);
	g_cond_clear(&cdmidecryptor->condition);

	G_OBJECT_CLASS(gst_cdmidecryptor_parent_class)->dispose(object);
}

/*
 Append modified caps to dest, but only if it does not already exist in updated caps.
 */
static void gst_cdmicapsappendifnotduplicate(GstCaps* destCaps,
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
gst_cdmidecryptor_transform_caps(GstBaseTransform * trans,
		GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
	DEBUG_FUNC();
	GstCDMIDecryptor *cdmidecryptor = GST_CDMI_DECRYPTOR(trans);
	g_return_val_if_fail(direction != GST_PAD_UNKNOWN, NULL);
	unsigned size = gst_caps_get_size(caps);
	GstCaps* transformedCaps = gst_caps_new_empty();

	GST_DEBUG_OBJECT(trans, "direction: %s, caps: %" GST_PTR_FORMAT " filter:"
			" %" GST_PTR_FORMAT, (direction == GST_PAD_SRC) ? "src" : "sink", caps, filter);

	if(!cdmidecryptor->selectedProtection)
	{
		GstStructure *capstruct = gst_caps_get_structure(caps, 0);
		const gchar* capsinfo = gst_structure_get_string(capstruct, "protection-system");
		if(capsinfo != NULL)
		{
			if(!g_strcmp0(capsinfo, PLAYREADY_UUID))
			{
				cdmidecryptor->selectedProtection = PLAYREADY_UUID;
			}
			else if(!g_strcmp0(capsinfo, WIDEVINE_UUID))
			{
				cdmidecryptor->selectedProtection = WIDEVINE_UUID;
			}
			else if(!g_strcmp0(capsinfo, CLEARKEY_UUID))
			{
				 cdmidecryptor->selectedProtection = CLEARKEY_UUID;
				 cdmidecryptor->ignoreSVP = true;
			}
			else if(!g_strcmp0(capsinfo, VERIMATRIX_UUID))
			{
				cdmidecryptor->selectedProtection = VERIMATRIX_UUID;
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
					GST_TRACE_OBJECT(cdmidecryptor, "Removing field %s", fieldName);
				}
			}

			gst_structure_set(out, "protection-system", G_TYPE_STRING,
					cdmidecryptor->selectedProtection, "original-media-type",
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

		gst_cdmicapsappendifnotduplicate(transformedCaps, out);

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
		g_mutex_lock(&cdmidecryptor->mutex);
		// clean up previous caps
		if (cdmidecryptor->sinkCaps)
		{
			gst_caps_unref(cdmidecryptor->sinkCaps);
			cdmidecryptor->sinkCaps = NULL;
		}
		cdmidecryptor->sinkCaps = gst_caps_copy(transformedCaps);
		g_mutex_unlock(&cdmidecryptor->mutex);
		GST_DEBUG_OBJECT(trans, "Set sinkCaps to %" GST_PTR_FORMAT, cdmidecryptor->sinkCaps);
	}
	return transformedCaps;
}

#ifdef USE_OPENCDM_ADAPTER

static GstFlowReturn gst_cdmidecryptor_transform_ip(
		GstBaseTransform * trans, GstBuffer * buffer)
{
	DEBUG_FUNC();

	GstCDMIDecryptor *cdmidecryptor =
			GST_CDMI_DECRYPTOR(trans);

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

	GST_DEBUG_OBJECT(cdmidecryptor, "Processing buffer");

	if (!buffer)
	{
		GST_ERROR_OBJECT(cdmidecryptor,"Failed to get writable buffer");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	protectionMeta =
			reinterpret_cast<GstProtectionMeta*>(gst_buffer_get_protection_meta(buffer));

	g_mutex_lock(&cdmidecryptor->mutex);
	mutexLocked = TRUE;
	if (!protectionMeta)
	{
		GST_DEBUG_OBJECT(cdmidecryptor,
				"Failed to get GstProtection metadata from buffer %p, could be clear buffer",buffer);
#if defined(AMLOGIC)
		// call decrypt even for clear samples in order to copy it to a secure buffer. If secure buffers are not supported
		// decrypt() call will return without doing anything
		if (cdmidecryptor->drmSession != NULL)
		   errorCode = cdmidecryptor->drmSession->decrypt(keyIDBuffer, ivBuffer, buffer, subSampleCount, subsamplesBuffer, cdmidecryptor->sinkCaps);
		else
		{ /* If drmSession creation failed, then the call will be aborted here */
			result = GST_FLOW_NOT_SUPPORTED;
			GST_ERROR_OBJECT(cdmidecryptor, "drmSession is **** NULL ****, returning GST_FLOW_NOT_SUPPORTED");
		}
#endif
		goto free_resources;
	}

	GST_TRACE_OBJECT(cdmidecryptor,
			"Mutex acquired, stream received: %s canWait: %d",
			cdmidecryptor->streamReceived ? "yes" : "no", cdmidecryptor->canWait);

	if (!cdmidecryptor->canWait
			&& !cdmidecryptor->streamReceived)
	{
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	if (!cdmidecryptor->firstsegprocessed)
	{
		GST_DEBUG_OBJECT(cdmidecryptor, "\n\nWaiting for key\n");
	}
	// The key might not have been received yet. Wait for it.
	if (!cdmidecryptor->streamReceived)
		g_cond_wait(&cdmidecryptor->condition,
				&cdmidecryptor->mutex);

	if (!cdmidecryptor->streamReceived)
	{
		GST_ERROR_OBJECT(cdmidecryptor,
				"Condition signaled from state change transition. Aborting.");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	/* If drmSession creation failed, then the call will be aborted here */
	if (cdmidecryptor->drmSession == NULL)
	{
		GST_ERROR_OBJECT(cdmidecryptor, "drmSession is invalid **** NULL ****. Aborting.");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	GST_TRACE_OBJECT(cdmidecryptor, "Got key event ; Proceeding with decryption");

	if (!gst_structure_get_uint(protectionMeta->info, "iv_size", &ivSize))
	{
		GST_ERROR_OBJECT(cdmidecryptor, "failed to get iv_size");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	if (!gst_structure_get_boolean(protectionMeta->info, "encrypted",
			&encrypted))
	{
		GST_ERROR_OBJECT(cdmidecryptor,
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
		GST_ERROR_OBJECT(cdmidecryptor,
				"failed to get subsample_count");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	value = gst_structure_get_value(protectionMeta->info, "iv");
	if (!value)
	{
		GST_ERROR_OBJECT(cdmidecryptor, "Failed to get IV for sample");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	ivBuffer = gst_value_get_buffer(value);

	value = gst_structure_get_value(protectionMeta->info, "kid");
	if (!value) {
		GST_ERROR_OBJECT(cdmidecryptor, "Failed to get kid for sample");
		result = GST_FLOW_NOT_SUPPORTED;
		goto free_resources;
	}

	keyIDBuffer = gst_value_get_buffer(value);

	if (subSampleCount)
	{
		value = gst_structure_get_value(protectionMeta->info, "subsamples");
		if (!value)
		{
			GST_ERROR_OBJECT(cdmidecryptor,
					"Failed to get subsamples");
			result = GST_FLOW_NOT_SUPPORTED;
			goto free_resources;
		}
		subsamplesBuffer = gst_value_get_buffer(value);
		if (!gst_buffer_map(subsamplesBuffer, &subSamplesMap, GST_MAP_READ))
		{
			GST_ERROR_OBJECT(cdmidecryptor,
					"Failed to map subsample buffer");
			result = GST_FLOW_NOT_SUPPORTED;
			goto free_resources;
		}
	}

	errorCode = cdmidecryptor->drmSession->decrypt(keyIDBuffer, ivBuffer, buffer, subSampleCount, subsamplesBuffer, cdmidecryptor->sinkCaps);

	cdmidecryptor->streamEncrypted = true;
	if (errorCode != 0 || cdmidecryptor->hdcpOpProtectionFailCount)
	{
	if(errorCode == HDCP_OUTPUT_PROTECTION_FAILURE)
	{
		cdmidecryptor->hdcpOpProtectionFailCount++;
	}
	else if(cdmidecryptor->hdcpOpProtectionFailCount)
	{
		if(cdmidecryptor->hdcpOpProtectionFailCount >= DECRYPT_FAILURE_THRESHOLD) {
			GstStructure *newmsg = gst_structure_new("HDCPProtectionFailure", "message", G_TYPE_STRING,"HDCP Output Protection Error", NULL);
			gst_element_post_message(reinterpret_cast<GstElement*>(cdmidecryptor),gst_message_new_application (GST_OBJECT (cdmidecryptor), newmsg));
		}
		cdmidecryptor->hdcpOpProtectionFailCount = 0;
	}
	else
	{
		GST_ERROR_OBJECT(cdmidecryptor, "decryption failed; error code %d\n",errorCode);
		cdmidecryptor->decryptFailCount++;
		if(cdmidecryptor->decryptFailCount >= DECRYPT_FAILURE_THRESHOLD && cdmidecryptor->notifyDecryptError )
		{
			cdmidecryptor->notifyDecryptError = false;
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
			gst_element_post_message(reinterpret_cast<GstElement*>(cdmidecryptor), gst_message_new_error (GST_OBJECT (cdmidecryptor), error, "Decrypt Failed"));
			g_error_free(error);
			result = GST_FLOW_ERROR;
		}
		goto free_resources;
	}
	}
	else
	{
		cdmidecryptor->decryptFailCount = 0;
	cdmidecryptor->hdcpOpProtectionFailCount = 0;
		if (cdmidecryptor->mediaType == eGST_MEDIATYPE_AUDIO)
		{
			GST_DEBUG_OBJECT(cdmidecryptor, "Decryption successful for Audio packets");
		}
		else
		{
			GST_DEBUG_OBJECT(cdmidecryptor, "Decryption successful for Video packets");
		}
	}

	if (!cdmidecryptor->firstsegprocessed
			&& cdmidecryptor->sessionManager)
	{
		cdmidecryptor->sessionManager->profileBeginCb(cdmidecryptor->mediaType);
		cdmidecryptor->firstsegprocessed = true;
	}

	free_resources:

	if (!cdmidecryptor->firstsegprocessed
			&& cdmidecryptor->sessionManager)
	{
	if(!cdmidecryptor->streamEncrypted)
	{
		cdmidecryptor->sessionManager->profileEndCb(cdmidecryptor->mediaType);
	}
	else
	{
		cdmidecryptor->sessionManager->profileErrorCb(cdmidecryptor->mediaType, result);
	}
		cdmidecryptor->firstsegprocessed = true;
	}

	if (subsamplesBuffer)
		gst_buffer_unmap(subsamplesBuffer, &subSamplesMap);

	if (protectionMeta)
		gst_buffer_remove_meta(buffer,
				reinterpret_cast<GstMeta*>(protectionMeta));

	if (mutexLocked)
		g_mutex_unlock(&cdmidecryptor->mutex);
	return result;
}
#endif


/* sink event handlers */
static gboolean gst_cdmidecryptor_sink_event(GstBaseTransform * trans,
		GstEvent * event)
{
	DEBUG_FUNC();

	GstCDMIDecryptor *cdmidecryptor =
			GST_CDMI_DECRYPTOR(trans);
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

	if(NULL == cdmidecryptor)
	{
		GST_ERROR_OBJECT(cdmidecryptor,
				"Invalid CDMI Decryptor Instance\n");
		result = FALSE;
		break;
	}

	//We need to get the sinkpad for sending upstream queries and
		//getting the current pad capability ie, VIDEO or AUDIO
		//in order to support tune time profiling
		GstPad * sinkpad = gst_element_get_static_pad(
				reinterpret_cast<GstElement*>(cdmidecryptor), "sink");

		//if (cdmidecryptor->player == NULL) //cdmidecryptor->sessionManager
		if(cdmidecryptor->sessionManager == NULL)
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
				cdmidecryptor->sessionManager =
						(DrmSessionManager*) g_value_get_pointer(val);
			}
			gst_query_unref(query);
		}
		//if(cdmidecryptor->player == NULL)
		if(cdmidecryptor->sessionManager == NULL)
		{
		GST_ERROR_OBJECT(cdmidecryptor,
                "cdmidecryptor unable to retrieve player instance\n");
		result = FALSE;
		break;
		}


		GST_DEBUG_OBJECT(cdmidecryptor,
				"Received encrypted event: Proceeding to parse initData\n");
		gst_event_parse_protection(event, &systemId, &initdatabuffer, &origin);
		GST_DEBUG_OBJECT(cdmidecryptor, "systemId: %s", systemId);
		GST_DEBUG_OBJECT(cdmidecryptor, "origin: %s", origin);
		/** If WideVine KeyID workaround is present check the systemId is clearKey **/
		if (cdmidecryptor->sessionManager->m_drmConfigParam->mIsWVKIDWorkaround){	
			if(!g_str_equal(systemId, CLEARKEY_UUID) ){
				gst_event_unref(event);
				result = TRUE;
				break;
			}
			GST_DEBUG_OBJECT(cdmidecryptor, "\nWideVine KeyID workaround is present, Select KeyID from Clear Key\n");
			systemId = WIDEVINE_UUID ;

		}else{ /* else check the selected protection system */
			if (!g_str_equal(systemId, cdmidecryptor->selectedProtection))
			{
				gst_event_unref(event);
				result = TRUE;
				break;
			}
		}

		GstMapInfo mapInfo;
		if (!gst_buffer_map(initdatabuffer, &mapInfo, GST_MAP_READ))
			break;
		GST_DEBUG_OBJECT(cdmidecryptor, "scheduling keyNeeded event");
		
		if (eGST_MEDIATYPE_MANIFEST == cdmidecryptor->mediaType)
		{
			GstCaps* caps = gst_pad_get_current_caps(sinkpad);
			GstStructure *capstruct = gst_caps_get_structure(caps, 0);
			const gchar* capsinfo = gst_structure_get_string(capstruct,
					"original-media-type");

			if (!g_strcmp0(capsinfo, "audio/mpeg"))
			{
				cdmidecryptor->mediaType = eGST_MEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "audio/x-opus"))
			{
				cdmidecryptor->mediaType = eGST_MEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "audio/x-eac3") || !g_strcmp0(capsinfo, "audio/x-ac3"))
			{
				cdmidecryptor->mediaType = eGST_MEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "audio/x-gst-fourcc-ec_3"))
			{
				cdmidecryptor->mediaType = eGST_MEDIATYPE_AUDIO;
			}
			else if (!g_strcmp0(capsinfo, "video/x-h264"))
			{
				cdmidecryptor->mediaType = eGST_MEDIATYPE_VIDEO;
			}
			else if (!g_strcmp0(capsinfo, "video/x-h265"))
			{
				cdmidecryptor->mediaType = eGST_MEDIATYPE_VIDEO;
			}
			else
			{
				gst_caps_unref(caps);
				result = false;
				break;
			}
			gst_caps_unref(caps);
		}

		if (cdmidecryptor->sessionManager->m_drmConfigParam->mIsWVKIDWorkaround){
			GST_DEBUG_OBJECT(cdmidecryptor, "\nWideVine KeyID workaround is present, Applying WideVine KID workaround\n");
			outData = ReplaceKIDPsshData(reinterpret_cast<unsigned char *>(mapInfo.data), mapInfo.size, outDataLen);
			if (NULL == outData){
				GST_ERROR_OBJECT(cdmidecryptor, "\nFailed to Apply WideVine KID workaround!\n");
				break;
			}
		}

		cdmidecryptor->sessionManager->laprofileBeginCb(cdmidecryptor->mediaType);
		g_mutex_lock(&cdmidecryptor->mutex);
		GST_DEBUG_OBJECT(cdmidecryptor, "\n acquired lock for mutex\n");
		//cdmidecryptor->sessionManager  = cdmidecryptor->mDRMLicenseManager;
		//void *e = cdmidecryptor->sessionManager->DrmMetaDataCb();
		std::shared_ptr<void> e = cdmidecryptor->sessionManager->DrmMetaDataCb();
                int err = -1;
		if (cdmidecryptor->sessionManager->m_drmConfigParam->mIsWVKIDWorkaround){
			cdmidecryptor->drmSession =	cdmidecryptor->sessionManager->createDrmSession(err,
						reinterpret_cast<const char *>(systemId), eMEDIAFORMAT_DASH,
						outData, outDataLen, (int)cdmidecryptor->mediaType, cdmidecryptor->player, e.get(), nullptr, false);
		}else{
			cdmidecryptor->drmSession =
				cdmidecryptor->sessionManager->createDrmSession(err,
						reinterpret_cast<const char *>(systemId), eMEDIAFORMAT_DASH,
						reinterpret_cast<const unsigned char *>(mapInfo.data),
						mapInfo.size, (int)cdmidecryptor->mediaType, cdmidecryptor->player, e.get(), nullptr, false);
		}
		if(err != -1)
                {
                       cdmidecryptor->sessionManager->setfailureCb(e.get(),err);
                }
		if (NULL == cdmidecryptor->drmSession)
		{
/* For  Avoided setting 'streamReceived' as FALSE if createDrmSession() failed after a successful case.
 * Set to FALSE is already handled on gst_cdmidecryptor_init() as part of initialization.
 */
#if 0
			cdmidecryptor->streamReceived = FALSE;
#endif /* 0 */

			/* Need to reset canWait to skip conditional wait in "gst_cdmidecryptor_transform_ip to avoid deadlock
			 *		scenario on drm session failure
			 */
			cdmidecryptor->canWait = false;
		/* session manager fails to create session when state is inactive. Skip sending error event
		 * in this scenario. Later player will change it to active after processing SetLanguage(), or for the next Tune.
		 */
		if(SessionMgrState::eSESSIONMGR_ACTIVE == cdmidecryptor->sessionManager->getSessionMgrState())
		{
			cdmidecryptor->sessionManager->laprofileErrorCb(e.get());
			GST_ERROR_OBJECT(cdmidecryptor,"Failed to create DRM Session\n");
		}
			result = TRUE;
		}
	else
		{
			cdmidecryptor->streamReceived = TRUE;
			cdmidecryptor->sessionManager->laprofileEndCb(cdmidecryptor->mediaType);
			if (!cdmidecryptor->firstsegprocessed)
			{
				cdmidecryptor->sessionManager->profileBeginCb(cdmidecryptor->mediaType);
			}

			result = TRUE;
		}
		g_cond_signal(&cdmidecryptor->condition);
		g_mutex_unlock(&cdmidecryptor->mutex);
		GST_DEBUG_OBJECT(cdmidecryptor, "\n releasing ...................... mutex\n");

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
				gst_cdmidecryptor_parent_class)->sink_event(trans,
				event);
		break;
	}

	return result;
}

static GstStateChangeReturn gst_cdmidecryptor_changestate(
		GstElement* element, GstStateChange transition)
{
	
	DEBUG_FUNC();

	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstCDMIDecryptor* cdmidecryptor =
			GST_CDMI_DECRYPTOR(element);

	switch (transition)
	{
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_DEBUG_OBJECT(cdmidecryptor, "READY->PAUSED");
		g_mutex_lock(&cdmidecryptor->mutex);
		cdmidecryptor->canWait = true;
		g_mutex_unlock(&cdmidecryptor->mutex);
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_DEBUG_OBJECT(cdmidecryptor, "PAUSED->READY");
		g_mutex_lock(&cdmidecryptor->mutex);
		cdmidecryptor->canWait = false;
		g_cond_signal(&cdmidecryptor->condition);
		g_mutex_unlock(&cdmidecryptor->mutex);
		break;
#if defined(AMLOGIC)
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_DEBUG_OBJECT(cdmidecryptor, "NULL->READY");
		if (cdmidecryptor->svpCtx == NULL)
		 gst_svp_ext_get_context(&cdmidecryptor->svpCtx, Server, 0);
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_DEBUG_OBJECT(cdmidecryptor, "READY->NULL");
		if (cdmidecryptor->svpCtx) {
  	        	gst_svp_ext_free_context(cdmidecryptor->svpCtx);
			cdmidecryptor->svpCtx = NULL;	
		}
		break;
#endif
	default:
		break;
	}

	ret =
			GST_ELEMENT_CLASS(gst_cdmidecryptor_parent_class)->change_state(
					element, transition);
	return ret;
}

static void gst_cdmidecryptor_set_property(GObject * object,
		guint prop_id, const GValue * value, GParamSpec * pspec)
{
	DEBUG_FUNC();

	GstCDMIDecryptor* cdmidecryptor =
			GST_CDMI_DECRYPTOR(object);
	switch (prop_id)
	{
	case PROP_DRM_SESSION_MANAGER:
		GST_OBJECT_LOCK(cdmidecryptor);
		cdmidecryptor->player =
				(DrmCallbacks*) g_value_get_pointer(value);
		GST_DEBUG_OBJECT(cdmidecryptor,
				"Received player instance from appsrc\n");
		GST_OBJECT_UNLOCK(cdmidecryptor);
		break;
	case PROP_PLAYER:
		GST_OBJECT_LOCK(cdmidecryptor);
		cdmidecryptor->sessionManager = 
				(DrmSessionManager*)g_value_get_pointer(value);
		GST_DEBUG_OBJECT(cdmidecryptor,
				"Received DRM session manager");
		GST_OBJECT_UNLOCK(cdmidecryptor);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static gboolean gst_cdmidecryptor_accept_caps(GstBaseTransform * trans,
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
#endif
