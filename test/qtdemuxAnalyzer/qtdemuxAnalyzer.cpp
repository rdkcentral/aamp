/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/*
 * qtDemuxAnalyzer.cpp - GStreamer Pipeline Test for MP4 Fragment Analysis
 * 
 * This program creates a GStreamer pipeline with appsrc -> qtdemux -> fakesink
 * to analyze MP4 initialization headers and fragments, with protection metadata probing.
 */

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasetransform.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WIDEVINE_PROTECTION_SYSTEM_ID "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"

// Dummy Widevine Decryptor Plugin
// Used to bypass the check in qtdemux that requires a decryptor to be present for protected streams
#define GST_TYPE_WIDEVINE_DECRYPT (gst_widevine_decrypt_get_type())
#define GST_WIDEVINE_DECRYPT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WIDEVINE_DECRYPT, GstWidevineDecrypt))
#define GST_WIDEVINE_DECRYPT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WIDEVINE_DECRYPT, GstWidevineDecryptClass))
#define GST_IS_WIDEVINE_DECRYPT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WIDEVINE_DECRYPT))
#define GST_IS_WIDEVINE_DECRYPT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WIDEVINE_DECRYPT))

typedef struct _GstWidevineDecrypt GstWidevineDecrypt;
typedef struct _GstWidevineDecryptClass GstWidevineDecryptClass;

struct _GstWidevineDecrypt {
	GstBaseTransform element;
};

struct _GstWidevineDecryptClass {
	GstBaseTransformClass parent_class;
};

GType gst_widevine_decrypt_get_type(void);

// Pad templates
static GstStaticPadTemplate sink_template =
		GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
		GST_STATIC_CAPS(
		"application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)video/x-h265, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)audio/x-eac3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)audio/x-ac3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)audio/x-opus, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)audio/x-ac4, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)audio/x-gst-fourcc-ec_3, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID "; "
		"application/x-cenc, original-media-type=(string)audio/mpeg, protection-system=(string)" WIDEVINE_PROTECTION_SYSTEM_ID)
	);


static GstStaticPadTemplate src_template =
		GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("video/x-h264;video/x-h264(memory:SecMem);audio/mpeg;video/x-h265;video/x-h265(memory:SecMem);audio/x-eac3;audio/x-gst-fourcc-ec_3;audio/x-ac3;audio/x-opus;audio/x-ac4")
	);
 

// Function declarations
static void gst_widevine_decrypt_class_init(GstWidevineDecryptClass *klass);
static void gst_widevine_decrypt_init(GstWidevineDecrypt *decrypt);
static GstFlowReturn gst_widevine_decrypt_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static gboolean gst_widevine_decrypt_sink_event(GstBaseTransform *trans, GstEvent *event);

G_DEFINE_TYPE(GstWidevineDecrypt, gst_widevine_decrypt, GST_TYPE_BASE_TRANSFORM);

static void gst_widevine_decrypt_class_init(GstWidevineDecryptClass *klass) {
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
	GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS(klass);

	gst_element_class_set_static_metadata(element_class,
		"Widevine Decryptor (Dummy)",
		"Codec/Decryptor",
		"Dummy Widevine decryptor for testing",
		"AAMP Team");

	gst_element_class_add_static_pad_template(element_class, &sink_template);
	gst_element_class_add_static_pad_template(element_class, &src_template);

	trans_class->transform_ip = gst_widevine_decrypt_transform_ip;
	trans_class->sink_event = gst_widevine_decrypt_sink_event;
	trans_class->passthrough_on_same_caps = FALSE;
}

static void gst_widevine_decrypt_init(GstWidevineDecrypt * /*decrypt*/) {
	g_print("Widevine decryptor element initialized\n");
}

static GstFlowReturn gst_widevine_decrypt_transform_ip(GstBaseTransform * /*trans*/, GstBuffer *buf) {
	g_print("Widevine decryptor: Processing buffer of size %" G_GSIZE_FORMAT "\n", gst_buffer_get_size(buf));

	// Check for protection metadata
	GstProtectionMeta *protection_meta = gst_buffer_get_protection_meta(buf);
	if (protection_meta) {
		g_print("Widevine decryptor: Found protection metadata, would decrypt here\n");
		// In a real implementation, this is where decryption would happen
		// For now, just pass through the buffer unchanged
	}

	return GST_FLOW_OK;
}

static gboolean gst_widevine_decrypt_sink_event(GstBaseTransform *trans, GstEvent *event) {
	if (GST_EVENT_TYPE(event) == GST_EVENT_PROTECTION) {
		g_print("Widevine decryptor: Received protection event\n");
		const GstStructure *structure = gst_event_get_structure(event);
		if (structure) {
			gchar *struct_string = gst_structure_to_string(structure);
			g_print("Protection event structure: %s\n", struct_string);
			g_free(struct_string);
		}
	}

	return GST_BASE_TRANSFORM_CLASS(gst_widevine_decrypt_parent_class)->sink_event(trans, event);
}

// Plugin registration function
static gboolean register_widevine_decrypt_plugin(GstPlugin *plugin) {
	return gst_element_register(plugin, "widevine-decrypt", GST_RANK_PRIMARY, GST_TYPE_WIDEVINE_DECRYPT);
}

// Pipeline elements
typedef struct _PipelineData {
	GstElement *pipeline;
	GstElement *appsrc;
	GstElement *qtdemux;
	GstElement *fakesink;
	GMainLoop *loop;
	gboolean init_sent;
	gchar *init_file;
	gchar *fragment_file;
	guint bufferCount;
} PipelineData;

// Function to read file into buffer
static GstBuffer* read_file_to_buffer(const char* filename) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		g_print("Error: Could not open file %s\n", filename);
		return NULL;
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (file_size <= 0) {
		g_print("Error: Invalid file size for %s\n", filename);
		fclose(file);
		return NULL;
	}

	// Allocate buffer and read data
	gpointer data = g_malloc(file_size);
	size_t bytes_read = fread(data, 1, (size_t)file_size, file);
	fclose(file);

	if ((long)bytes_read != file_size) {
		g_print("Error: Could not read complete file %s\n", filename);
		g_free(data);
		return NULL;
	}

	// Create GstBuffer
	GstBuffer *buffer = gst_buffer_new_wrapped(data, file_size);
	g_print("Successfully loaded file %s (%ld bytes)\n", filename, file_size);

	return buffer;
}

// Probe function to analyze protection metadata
static GstPadProbeReturn protection_metadata_probe(GstPad * /*pad*/, GstPadProbeInfo *info, gpointer user_data) {
	PipelineData *data = (PipelineData*)user_data;
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

	if (!buffer) {
		return GST_PAD_PROBE_OK;
	}

	data->bufferCount++;

	g_print("\n=== Protection Metadata Probe ===\n");
	g_print("Buffer size: %" G_GSIZE_FORMAT " bytes\n", gst_buffer_get_size(buffer));
	g_print("Buffer PTS: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(GST_BUFFER_PTS(buffer)));
	g_print("Buffer DTS: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(GST_BUFFER_DTS(buffer)));

	// Check for protection metadata
	GstProtectionMeta *protection_meta = gst_buffer_get_protection_meta(buffer);
	if (protection_meta) {
		g_print("*** PROTECTION METADATA FOUND ***\n");
		
		// Access protection info through the meta structure
		const GstStructure *info_struct = protection_meta->info;
		if (info_struct) {
			gchar *struct_string = gst_structure_to_string(info_struct);
			g_print("Protection info: %s\n", struct_string);
			g_free(struct_string);
		}
	} else {
		g_print("No protection metadata found\n");
	}

	g_print("=====================================\n\n");
	return GST_PAD_PROBE_OK;
}

// Callback for qtdemux pad-added signal
static void on_qtdemux_pad_added(GstElement * /*qtdemux*/, GstPad *new_pad, gpointer user_data) {
	PipelineData *data = (PipelineData*)user_data;
	GstCaps *caps = gst_pad_get_current_caps(new_pad);

	if (!caps) {
		caps = gst_pad_query_caps(new_pad, NULL);
	}

	if (caps) {
		const gchar *pad_name = gst_pad_get_name(new_pad);
		gchar* capsStr = gst_caps_to_string(caps);
		g_print("New pad created: %s with caps: %s\n", pad_name, capsStr);
		g_free(capsStr);

		// Add protection metadata probe to this pad
		gst_pad_add_probe(new_pad, GST_PAD_PROBE_TYPE_BUFFER, protection_metadata_probe, data, NULL);
		g_print("Added protection metadata probe to pad: %s\n", pad_name);

		// Link to fakesink (single sink for DASH fragments)
		GstPad *sink_pad = gst_element_get_static_pad(data->fakesink, "sink");
		if (sink_pad && !gst_pad_is_linked(sink_pad)) {
			GstPadLinkReturn link_result = gst_pad_link(new_pad, sink_pad);
			if (link_result == GST_PAD_LINK_OK) {
				g_print("Successfully linked pad %s to fakesink\n", pad_name);
			} else {
				g_print("Failed to link pad %s (error: %d)\n", pad_name, link_result);
			}
			gst_object_unref(sink_pad);
		} else {
			g_print("Fakesink already linked, skipping pad %s\n", pad_name);
		}

		gst_caps_unref(caps);
	}
}

// Synchronous bus handler for context messages
static GstBusSyncReply bus_sync_handler(GstBus * /*bus*/, GstMessage *message, gpointer user_data) {
	PipelineData *data = (PipelineData*)user_data;

	switch (GST_MESSAGE_TYPE(message)) {
		case GST_MESSAGE_NEED_CONTEXT: {
			const gchar *context_type;
			gst_message_parse_context_type(message, &context_type);
			g_print("GST_MESSAGE_NEED_CONTEXT: %s from %s\n", 
				   context_type, GST_OBJECT_NAME(GST_MESSAGE_SRC(message)));

			if (!g_strcmp0(context_type, "drm-preferred-decryption-system-id")) {
				g_print("Setting Widevine as preferred DRM system\n");
				GstContext *context = gst_context_new("drm-preferred-decryption-system-id", FALSE);
				GstStructure *context_structure = gst_context_writable_structure(context);
				gst_structure_set(context_structure, 
								"decryption-system-id", G_TYPE_STRING, WIDEVINE_PROTECTION_SYSTEM_ID, 
								NULL);
				gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);
				gst_context_unref(context);
				return GST_BUS_DROP;
			}
			break;
		}
		case GST_MESSAGE_STATE_CHANGED: {
			if (GST_MESSAGE_SRC(message) == GST_OBJECT(data->pipeline)) {
				GstState old_state, new_state;
				gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
				// Handle any sync-required state changes here if needed
				g_print("Pipeline state changed: %s -> %s\n",
					   gst_element_state_get_name(old_state),
					   gst_element_state_get_name(new_state));
			}
			break;
		}
		default:
			break;
	}

	return GST_BUS_PASS;
}

// Bus message handler
static gboolean bus_message_handler(GstBus * /*bus*/, GstMessage *message, gpointer user_data) {
	PipelineData *data = (PipelineData*)user_data;

	switch (GST_MESSAGE_TYPE(message)) {
		case GST_MESSAGE_ERROR: {
			GError *error;
			gchar *debug;
			gst_message_parse_error(message, &error, &debug);
			g_print("Error: %s\n", error->message);
			if (debug) {
				g_print("Debug: %s\n", debug);
			}
			g_error_free(error);
			g_free(debug);
			g_main_loop_quit(data->loop);
			break;
		}
		case GST_MESSAGE_EOS:
			g_print("End of stream reached\n");
			g_main_loop_quit(data->loop);
			break;
		case GST_MESSAGE_STATE_CHANGED: {
			if (GST_MESSAGE_SRC(message) == GST_OBJECT(data->pipeline)) {
				GstState old_state, new_state;
				gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
				g_print("Pipeline state changed: %s -> %s\n",
					   gst_element_state_get_name(old_state),
					   gst_element_state_get_name(new_state));
			}
			break;
		}
		case GST_MESSAGE_STREAM_START:
			g_print("Stream started\n");
			break;
		default:
			break;
	}

	return TRUE;
}

// Function to inject MP4 data
static gboolean inject_mp4_data(gpointer user_data) {
	PipelineData *data = (PipelineData*)user_data;

	if (!data->init_sent && data->init_file) {
		g_print("Injecting initialization segment...\n");
		GstBuffer *init_buffer = read_file_to_buffer(data->init_file);
		if (init_buffer) {
			GST_BUFFER_FLAG_SET(init_buffer, GST_BUFFER_FLAG_DISCONT);
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(data->appsrc), init_buffer);
			if (ret == GST_FLOW_OK) {
				g_print("Successfully injected initialization segment\n");
				data->init_sent = TRUE;
			} else {
				g_print("Failed to inject initialization segment: %d\n", ret);
				return FALSE;
			}
		} else {
			g_print("Failed to read initialization file\n");
			return FALSE;
		}
	}

	if (data->init_sent && data->fragment_file) {
		g_print("Injecting media fragment...\n");
		GstBuffer *fragment_buffer = read_file_to_buffer(data->fragment_file);
		if (fragment_buffer) {
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(data->appsrc), fragment_buffer);
			if (ret == GST_FLOW_OK) {
				g_print("Successfully injected media fragment\n");
				// Send EOS after fragment
				gst_app_src_end_of_stream(GST_APP_SRC(data->appsrc));
				return FALSE; // Remove timeout
			} else {
				g_print("Failed to inject media fragment: %d\n", ret);
			}
		} else {
			g_print("Failed to read fragment file\n");
		}
	}

	return TRUE; // Continue timeout
}

// Initialize pipeline
static gboolean initialize_pipeline(PipelineData *data) {
	// Create elements
	data->pipeline = gst_pipeline_new("mp4-analysis-pipeline");
	data->appsrc = gst_element_factory_make("appsrc", "source");
	data->qtdemux = gst_element_factory_make("qtdemux", "demux");
	data->fakesink = gst_element_factory_make("fakesink", "sink");

	if (!data->pipeline || !data->appsrc || !data->qtdemux || !data->fakesink) {
		g_print("Failed to create pipeline elements\n");
		return FALSE;
	}

	// Configure appsrc with typefind enabled
	g_object_set(G_OBJECT(data->appsrc),
				 "format", GST_FORMAT_TIME,
				 "typefind", TRUE,
				 NULL);

	// Configure fakesink for verbose output
	g_object_set(G_OBJECT(data->fakesink), "dump", FALSE, "silent", TRUE, NULL);

	// Add elements to pipeline
	gst_bin_add_many(GST_BIN(data->pipeline), 
					 data->appsrc, data->qtdemux, data->fakesink, NULL);

	// Link appsrc -> qtdemux
	if (!gst_element_link(data->appsrc, data->qtdemux)) {
		g_print("Failed to link pipeline elements\n");
		return FALSE;
	}

	// Connect signals
	g_signal_connect(data->qtdemux, "pad-added", G_CALLBACK(on_qtdemux_pad_added), data);

	// Set up bus with both sync and async handlers
	GstBus *bus = gst_element_get_bus(data->pipeline);
	gst_bus_set_sync_handler(bus, (GstBusSyncHandler)bus_sync_handler, data, NULL);
	gst_bus_add_watch(bus, bus_message_handler, data);
	gst_object_unref(bus);

	g_print("Pipeline initialized successfully\n");
	return TRUE;
}

// Cleanup function
static void cleanup_pipeline(PipelineData *data) {
	if (data->pipeline) {
		gst_element_set_state(data->pipeline, GST_STATE_NULL);
		gst_object_unref(data->pipeline);
	}
	if (data->loop) {
		g_main_loop_unref(data->loop);
	}
	g_free(data->init_file);
	g_free(data->fragment_file);
}

// Main function
int main(int argc, char *argv[]) {
	if (argc < 3) {
		g_print("Usage: %s <init_file.mp4> <fragment_file.mp4>\n", argv[0]);
		g_print("Example: %s audio_init.mp4 audio_frag.mp4\n", argv[0]);
		return -1;
	}

	// Initialize GStreamer
	gst_init(&argc, &argv);

	// Register dummy Widevine decryptor plugin
	gst_plugin_register_static(GST_VERSION_MAJOR, GST_VERSION_MINOR,
							  "widevine-decrypt",
							  "Dummy Widevine Decryptor Plugin",
							  register_widevine_decrypt_plugin,
							  "1.0",
							  "LGPL",
							  "qtdemuxAnalyzer",
							  "AAMP",
							  "http://www.qtdemuxAnalyzer.com/");
	g_print("Registered dummy Widevine decryptor plugin\n");

	// Initialize pipeline data
	PipelineData data = {};
	memset(&data, 0, sizeof(PipelineData));
	data.init_file = g_strdup(argv[1]);
	data.fragment_file = g_strdup(argv[2]);
	data.init_sent = FALSE;

	g_print("=== AAMP GStreamer MP4 Fragment Analysis ===\n");
	g_print("Initialization file: %s\n", data.init_file);
	g_print("Fragment file: %s\n", data.fragment_file);
	g_print("============================================\n\n");

	// Initialize pipeline
	if (!initialize_pipeline(&data)) {
		cleanup_pipeline(&data);
		return -1;
	}

	// Create main loop
	data.loop = g_main_loop_new(NULL, FALSE);

	// Start pipeline
	GstStateChangeReturn ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_print("Failed to start pipeline\n");
		cleanup_pipeline(&data);
		return -1;
	}

	// Schedule data injection
	g_timeout_add(1000, inject_mp4_data, &data); // Start after 1 second

	g_print("Starting pipeline and main loop...\n");
	g_main_loop_run(data.loop);

	g_print("Final buffer count processed: %u\n", data.bufferCount);
	g_print("Cleaning up...\n");
	cleanup_pipeline(&data);

	return 0;
}
