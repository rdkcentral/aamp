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

#include <stdexcept>

#include "GstSrc.h"
#include "GstTextTrackSinkFactory.h"
#include "RialtoServerLogging.h"
#include <MediaCommon.h>

static void gstRialtoSrcUriHandlerInit(gpointer gIface, gpointer ifaceData);

static GstStaticPadTemplate rialto_src_template =
    GST_STATIC_PAD_TEMPLATE("src_%u", GST_PAD_SRC, GST_PAD_SOMETIMES, GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY(rialto_gst_player_debug);
#define GST_CAT_DEFAULT rialto_gst_player_debug
#define gst_rialto_src_parent_class parent_class
#define RIALTO_SRC_CATEGORY_INIT                                                                                       \
    GST_DEBUG_CATEGORY_INIT(rialto_gst_player_debug, "rialtosrc", 0, "Rialto source element");
G_DEFINE_TYPE_WITH_CODE(GstRialtoSrc, gst_rialto_src, GST_TYPE_BIN,
                        G_ADD_PRIVATE(GstRialtoSrc)
                            G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, gstRialtoSrcUriHandlerInit)
                                RIALTO_SRC_CATEGORY_INIT)

static void gstRialtoSrcDispose(GObject *object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, dispose, (object));
}

static void gstRialtoSrcFinalize(GObject *object)
{
    GstRialtoSrc *src = GST_RIALTO_SRC(object);
    GstRialtoSrcPrivate *priv = src->priv;
    g_free(priv->uri);
    priv->~GstRialtoSrcPrivate();
    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void gstRialtoSrcSetProperty(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, set_property, (object, prop_id, value, pspec));
}

static void gstRialtoSrcGetProperty(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, get_property, (object, prop_id, value, pspec));
}

static GstURIType gstRialtoSrcUriGetType(GType)
{
    return GST_URI_SRC;
}

const gchar *const *gstRialtoSrcGetProtocols(GType)
{
    static const char *protocols[] = {"rialto", 0};
    return protocols;
}

static gchar *gstRialtoSrcGetUri(GstURIHandler *handler)
{
    GstRialtoSrc *src = GST_RIALTO_SRC(handler);
    gchar *ret;
    GST_OBJECT_LOCK(src);
    ret = g_strdup(src->priv->uri);
    GST_OBJECT_UNLOCK(src);
    return ret;
}

static gboolean gstRialtoSrcSetUri(GstURIHandler *handler, const gchar *uri, GError **error)
{
    GstRialtoSrc *src = GST_RIALTO_SRC(handler);
    GstRialtoSrcPrivate *priv = src->priv;
    if (GST_STATE(src) >= GST_STATE_PAUSED)
    {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }
    GST_OBJECT_LOCK(src);
    g_free(priv->uri);
    priv->uri = 0;
    if (!uri)
    {
        GST_OBJECT_UNLOCK(src);
        return TRUE;
    }
    priv->uri = g_strdup(uri);
    GST_OBJECT_UNLOCK(src);
    return TRUE;
}

static void gstRialtoSrcUriHandlerInit(gpointer gIface, gpointer)
{
    GstURIHandlerInterface *iface = reinterpret_cast<GstURIHandlerInterface *>(gIface);
    iface->get_type = gstRialtoSrcUriGetType;
    iface->get_protocols = gstRialtoSrcGetProtocols;
    iface->get_uri = gstRialtoSrcGetUri;
    iface->set_uri = gstRialtoSrcSetUri;
}

static gboolean gstRialtoSrcQueryWithParent(GstPad *pad, GstObject *parent, GstQuery *query)
{
    gboolean result = FALSE;
    switch (GST_QUERY_TYPE(query))
    {
    default:
    {
        GstPad *target = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad));
        // Forward the query to the proxy target pad.
        if (target)
            result = gst_pad_query(target, query);
        gst_object_unref(target);
        break;
    }
    }
    return result;
}

void gstRialtoSrcHandleMessage(GstBin *bin, GstMessage *message)
{
    GstRialtoSrc *src = GST_RIALTO_SRC(GST_ELEMENT(bin));
    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_EOS:
    {
        gboolean emit_eos = TRUE;
        GstPad *pad = gst_element_get_static_pad(GST_ELEMENT(GST_MESSAGE_SRC(message)), "src");
        GST_DEBUG_OBJECT(src, "EOS received from %s", GST_MESSAGE_SRC_NAME(message));
        g_object_set_data(G_OBJECT(pad), "is-eos", GINT_TO_POINTER(1));
        gst_object_unref(pad);
        for (guint i = 0; i < src->priv->appsrc_count; i++)
        {
            gchar *name = g_strdup_printf("src_%u", i);
            GstPad *src_pad = gst_element_get_static_pad(GST_ELEMENT(src), name);
            GstPad *target = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(src_pad));
            gint is_eos = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(target), "is-eos"));
            gst_object_unref(target);
            gst_object_unref(src_pad);
            g_free(name);
            if (!is_eos)
            {
                emit_eos = FALSE;
                break;
            }
        }
        gst_message_unref(message);
        if (emit_eos)
        {
            GST_DEBUG_OBJECT(src, "All appsrc elements are EOS, emitting event now.");
            gst_element_send_event(GST_ELEMENT(bin), gst_event_new_eos());
        }
        break;
    }
    default:
        GST_BIN_CLASS(parent_class)->handle_message(bin, message);
        break;
    }
}

static void gstRialtoSrcDoAsyncStart(GstRialtoSrc *rialtoSrc)
{
    GstRialtoSrcPrivate *privateData = rialtoSrc->priv;
    if (privateData->async_done)
    {
        GST_DEBUG_OBJECT(rialtoSrc, "Rialto src already done");
        return;
    }

    GstMessage *message = gst_message_new_async_start(GST_OBJECT(rialtoSrc));
    GstBin *bin = GST_BIN(rialtoSrc);

    privateData->async_start = TRUE;
    GST_BIN_CLASS(parent_class)->handle_message(bin, message);
}

static void gstRialtoSrcDoAsyncDone(GstRialtoSrc *rialtoSrc)
{
    GstRialtoSrcPrivate *privateData = rialtoSrc->priv;
    if (!privateData->async_start)
    {
        GST_DEBUG_OBJECT(rialtoSrc, "Rialto src not started");
        return;
    }

    GstMessage *message = gst_message_new_async_done(GST_OBJECT(rialtoSrc), GST_CLOCK_TIME_NONE);
    GstBin *bin = GST_BIN(rialtoSrc);

    GST_BIN_CLASS(parent_class)->handle_message(bin, message);

    privateData->async_done = TRUE;
    privateData->async_start = FALSE;
}

static GstStateChangeReturn gstRialtoSrcChangeState(GstElement *element, GstStateChange transition)
{
    GstRialtoSrc *rialtoSrc = GST_RIALTO_SRC(element);
    GstRialtoSrcPrivate *privateData = rialtoSrc->priv;
    GstStateChangeReturn status = GST_STATE_CHANGE_SUCCESS;

    if (transition == GST_STATE_CHANGE_READY_TO_PAUSED)
    {
        gstRialtoSrcDoAsyncStart(rialtoSrc);
    }

    status = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (G_UNLIKELY(status == GST_STATE_CHANGE_FAILURE))
    {
        gstRialtoSrcDoAsyncDone(rialtoSrc);
        return status;
    }

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        if (!privateData->async_done)
            status = GST_STATE_CHANGE_ASYNC;
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        gstRialtoSrcDoAsyncDone(rialtoSrc);
        break;
    }
    default:
        break;
    }

    return status;
}

static void gst_rialto_src_init(GstRialtoSrc *src) // NOLINT(build/function_format)
{
    GstRialtoSrcPrivate *priv = reinterpret_cast<GstRialtoSrcPrivate *>(gst_rialto_src_get_instance_private(src));
    src->priv = priv;
    src->priv->appsrc_count = 0;
    src->priv->async_start = FALSE;
    src->priv->async_done = FALSE;
    g_object_set(GST_BIN(src), "message-forward", TRUE, nullptr);
}

static void gst_rialto_src_class_init(GstRialtoSrcClass *klass) // NOLINT(build/function_format)
{
    GObjectClass *oklass = G_OBJECT_CLASS(klass);
    GstElementClass *eklass = GST_ELEMENT_CLASS(klass);
    GstBinClass *bklass = GST_BIN_CLASS(klass);
    oklass->dispose = gstRialtoSrcDispose;
    oklass->finalize = gstRialtoSrcFinalize;
    oklass->set_property = gstRialtoSrcSetProperty;
    oklass->get_property = gstRialtoSrcGetProperty;
    GstPadTemplate *templ = gst_static_pad_template_get(&rialto_src_template);
    gst_element_class_add_pad_template(eklass, templ);
    gst_element_class_set_metadata(eklass, "Rialto source element", "Source",
                                   "Handles data incoming from the Rialto player", "POC <poc@sky.uk>");
    g_object_class_install_property(oklass, PROP_LOCATION,
                                    g_param_spec_string("location", "location", "Location to read from", 0,
                                                        (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    bklass->handle_message = GST_DEBUG_FUNCPTR(gstRialtoSrcHandleMessage);
    eklass->change_state = GST_DEBUG_FUNCPTR(gstRialtoSrcChangeState);
}

namespace firebolt::rialto::server
{
std::weak_ptr<IGstSrcFactory> GstSrcFactory::m_factory;
std::weak_ptr<IGstSrc> GstSrcFactory::m_gstSrc;
std::mutex GstSrcFactory::m_creationMutex;

std::shared_ptr<IGstSrcFactory> IGstSrcFactory::getFactory()
{
    std::shared_ptr<IGstSrcFactory> factory = GstSrcFactory::m_factory.lock();

    if (!factory)
    {
        try
        {
            factory = std::make_shared<GstSrcFactory>();
        }
        catch (const std::exception &e)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to create the gstreamer src factory, reason: %s", e.what());
        }

        GstSrcFactory::m_factory = factory;
    }

    return factory;
}

std::shared_ptr<IGstSrc> GstSrcFactory::getGstSrc()
{
    std::lock_guard<std::mutex> lock{m_creationMutex};

    std::shared_ptr<IGstSrc> gstSrc = GstSrcFactory::m_gstSrc.lock();

    if (!gstSrc)
    {
        try
        {
            gstSrc = std::make_shared<GstSrc>(firebolt::rialto::wrappers::IGstWrapperFactory::getFactory(),
                                              firebolt::rialto::wrappers::IGlibWrapperFactory::getFactory(),
                                              IGstDecryptorElementFactory::createFactory());
        }
        catch (const std::exception &e)
        {
            RIALTO_SERVER_LOG_ERROR("Failed to create the gstreamer src, reason: %s", e.what());
        }

        GstSrcFactory::m_gstSrc = gstSrc;
    }

    return gstSrc;
}

GstSrc::GstSrc(const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapperFactory> &gstWrapperFactory,
               const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapperFactory> &glibWrapperFactory,
               const std::shared_ptr<IGstDecryptorElementFactory> &decryptorFactory)
    : m_decryptorFactory(decryptorFactory)
{
    if ((!gstWrapperFactory) || (!(m_gstWrapper = gstWrapperFactory->getGstWrapper())))
    {
        throw std::runtime_error("Cannot create GstWrapper");
    }
    if ((!glibWrapperFactory) || (!(m_glibWrapper = glibWrapperFactory->getGlibWrapper())))
    {
        throw std::runtime_error("Cannot create GlibWrapper");
    }
    if (!m_decryptorFactory)
    {
        throw std::runtime_error("No decryptor factory provided");
    }
}

void GstSrc::initSrc()
{
    GstElementFactory *src_factory = m_gstWrapper->gstElementFactoryFind("rialtosrc");
    if (!src_factory)
    {
        m_gstWrapper->gstElementRegister(0, "rialtosrc", GST_RANK_PRIMARY + 100, GST_RIALTO_TYPE_SRC);
    }
    else
    {
        m_gstWrapper->gstObjectUnref(src_factory);
        src_factory = nullptr;
    }
}

void GstSrc::setDefaultStreamFormatIfNeeded(GstElement *appSrc)
{
    GstCaps *currentCaps = m_gstWrapper->gstAppSrcGetCaps(GST_APP_SRC(appSrc));
    if (currentCaps)
    {
        GstStructure *structure = m_gstWrapper->gstCapsGetStructure(currentCaps, 0);
        if (structure && (m_gstWrapper->gstStructureHasName(structure, "video/x-h264") ||
                          m_gstWrapper->gstStructureHasName(structure, "video/x-h265")))
        {
            bool hasStreamFormat = m_gstWrapper->gstStructureHasField(structure, "stream-format");
            bool hasCodecData = m_gstWrapper->gstStructureHasField(structure, "codec_data");

            if (!hasStreamFormat && !hasCodecData)
            {
                GstCaps *newCaps = m_gstWrapper->gstCapsCopy(currentCaps);
                if (newCaps)
                {
                    m_gstWrapper->gstCapsSetSimple(newCaps, "stream-format", G_TYPE_STRING, "byte-stream", nullptr);
                    m_gstWrapper->gstAppSrcSetCaps(GST_APP_SRC(appSrc), newCaps);
                    GST_INFO("Added stream-format=byte-stream to caps %" GST_PTR_FORMAT, newCaps);
                    m_gstWrapper->gstCapsUnref(newCaps);
                }
            }
        }
    }
    m_gstWrapper->gstCapsUnref(currentCaps);
}

void GstSrc::setupAndAddAppSrc(IDecryptionService *decryptionService, GstElement *source, StreamInfo &streamInfo,
                               GstAppSrcCallbacks *callbacks, gpointer userData, firebolt::rialto::MediaSourceType type)
{
    // Configure and add appsrc
    m_glibWrapper->gObjectSet(streamInfo.appSrc, "block", FALSE, "format", GST_FORMAT_TIME, "stream-type",
                              GST_APP_STREAM_TYPE_STREAM, "min-percent", 20, "handle-segment-change", TRUE, nullptr);
    m_gstWrapper->gstAppSrcSetCallbacks(GST_APP_SRC(streamInfo.appSrc), callbacks, userData, nullptr);

    const std::unordered_map<firebolt::rialto::MediaSourceType, uint32_t> queueSize =
        {{firebolt::rialto::MediaSourceType::VIDEO, 8 * 1024 * 1024},
         {firebolt::rialto::MediaSourceType::AUDIO, 512 * 1024},
         {firebolt::rialto::MediaSourceType::SUBTITLE, 256 * 1024}};

    auto sizeIt = queueSize.find(type);
    if (sizeIt != queueSize.end())
    {
        m_gstWrapper->gstAppSrcSetMaxBytes(GST_APP_SRC(streamInfo.appSrc), sizeIt->second);
    }
    else
    {
        GST_WARNING_OBJECT(source, "Could not find max-bytes value for appsrc");
    }

    m_gstWrapper->gstAppSrcSetStreamType(GST_APP_SRC(streamInfo.appSrc), GST_APP_STREAM_TYPE_SEEKABLE);

    GstRialtoSrc *src = GST_RIALTO_SRC(source);
    guint id = src->priv->appsrc_count;
    src->priv->appsrc_count++;
    gchar *name = m_glibWrapper->gStrdupPrintf("src_%u", id);
    m_gstWrapper->gstBinAdd(GST_BIN(source), streamInfo.appSrc);

    GstElement *src_elem = streamInfo.appSrc;

    // Configure and add decryptor
    if (streamInfo.hasDrm)
    {
        gchar *decryptorName =
            m_glibWrapper->gStrdupPrintf("rialtodecryptor%s_%u",
                                         (type == firebolt::rialto::MediaSourceType::VIDEO) ? "video" : "audio", id);
        GstElement *decryptor =
            m_decryptorFactory->createDecryptorElement(decryptorName, decryptionService, m_gstWrapper);
        m_glibWrapper->gFree(decryptorName);
        if (decryptor)
        {
            GST_DEBUG_OBJECT(src, "Injecting decryptor element %" GST_PTR_FORMAT, decryptor);

            m_gstWrapper->gstBinAdd(GST_BIN(source), decryptor);
            m_gstWrapper->gstElementSyncStateWithParent(decryptor);
            m_gstWrapper->gstElementLink(src_elem, decryptor);
            src_elem = decryptor;
        }
        else
        {
            GST_WARNING_OBJECT(src, "Could not create decryptor element");
        }

        if (type == firebolt::rialto::MediaSourceType::VIDEO)
        {
            // Configure and add payloader
            GstElement *payloader = createPayloader();
            if (payloader)
            {
                /*
                h264secparse and h265secparse have problems with parsing blank caps (with no stream-format nor
                codec_data defined). This is a workaround to set the stream-format to byte-stream if needed.
                */
                setDefaultStreamFormatIfNeeded(streamInfo.appSrc);

                if (GST_IS_BASE_TRANSFORM(payloader))
                {
                    m_gstWrapper->gstBaseTransformSetInPlace(GST_BASE_TRANSFORM(payloader), TRUE);
                }
                m_gstWrapper->gstBinAdd(GST_BIN(source), payloader);
                m_gstWrapper->gstElementSyncStateWithParent(payloader);
                m_gstWrapper->gstElementLink(src_elem, payloader);
                src_elem = payloader;
            }
            else
            {
                GST_WARNING_OBJECT(src, "Could not create payloader element");
            }
        }
    }

    // Configure and add buffer queue
    GstElement *queue = m_gstWrapper->gstElementFactoryMake("queue", nullptr);
    if (queue)
    {
        m_glibWrapper->gObjectSet(G_OBJECT(queue), "max-size-buffers", 10, "max-size-bytes", 0, "max-size-time",
                                  (gint64)0, "silent", TRUE, nullptr);
        m_gstWrapper->gstBinAdd(GST_BIN(source), queue);
        m_gstWrapper->gstElementSyncStateWithParent(queue);
        m_gstWrapper->gstElementLink(src_elem, queue);
        src_elem = queue;
    }
    else
    {
        GST_WARNING_OBJECT(src, "Could not create buffer queue element");
    }

    // Setup pad
    GstPad *target = m_gstWrapper->gstElementGetStaticPad(src_elem, "src");
    GstPad *pad = m_gstWrapper->gstGhostPadNew(name, target);
    m_gstWrapper->gstPadSetQueryFunction(pad, gstRialtoSrcQueryWithParent);
    m_gstWrapper->gstPadSetActive(pad, TRUE);

    m_gstWrapper->gstElementAddPad(source, pad);
    GST_OBJECT_FLAG_SET(pad, GST_PAD_FLAG_NEED_PARENT);

    m_gstWrapper->gstElementSyncStateWithParent(streamInfo.appSrc);

    m_glibWrapper->gFree(name);
    m_gstWrapper->gstObjectUnref(target);
}

void GstSrc::allAppSrcsAdded(GstElement *element)
{
    GstRialtoSrc *src = GST_RIALTO_SRC(element);
    m_gstWrapper->gstElementNoMorePads(element);
    gstRialtoSrcDoAsyncDone(src);
}

GstElement *GstSrc::createPayloader()
{
    static GstElementFactory *factory = nullptr;
    static gsize init = 0;
    if (m_glibWrapper->gOnceInitEnter(&init))
    {
        factory = m_gstWrapper->gstElementFactoryFind("svppay");
        m_glibWrapper->gOnceInitLeave(&init, 1);
    }
    if (!factory)
    {
        GST_WARNING("svppay not found");
        return nullptr;
    }
    return m_gstWrapper->gstElementFactoryCreate(factory, nullptr);
}

}; // namespace firebolt::rialto::server
