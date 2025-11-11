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

#ifndef FIREBOLT_RIALTO_SERVER_GST_SRC_H_
#define FIREBOLT_RIALTO_SERVER_GST_SRC_H_

#include "IGlibWrapper.h"
#include "IGstDecryptorElementFactory.h"
#include "IGstSrc.h"
#include "IGstWrapper.h"

#include <gst/app/gstappsrc.h>
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <memory>
#include <mutex>

G_BEGIN_DECLS

#define GST_RIALTO_TYPE_SRC (gst_rialto_src_get_type())
#define GST_RIALTO_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_RIALTO_TYPE_SRC, GstRialtoSrc))
#define GST_RIALTO_SRC_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST((klass), GST_RIALTO_TYPE_SRC, GstRialtoSrcClass))
#define GST_IS_RIALTO_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_RIALTO_TYPE_SRC))
#define GST_IS_RIALTO_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_RIALTO_TYPE_SRC))

typedef struct _GstRialtoSrc GstRialtoSrc;
typedef struct _GstRialtoSrcClass GstRialtoSrcClass;
typedef struct _GstRialtoSrcPrivate GstRialtoSrcPrivate;

struct _GstRialtoSrc
{
    GstBin parent;
    GstRialtoSrcPrivate *priv;
};

struct _GstRialtoSrcClass
{
    GstBinClass parentClass;
};

GType gst_rialto_src_get_type(void); // NOLINT(build/function_format)

struct _GstRialtoSrcPrivate
{
    gchar *uri;
    guint appsrc_count;
    gboolean async_start;
    gboolean async_done;
};

enum
{
    PROP_0,
    PROP_LOCATION
};

G_END_DECLS

namespace firebolt::rialto::server
{
/**
 * @brief IGstSrc factory class definition.
 */
class GstSrcFactory : public IGstSrcFactory
{
public:
    /**
     * @brief Weak pointer to the singleton factroy object.
     */
    static std::weak_ptr<IGstSrcFactory> m_factory;

    /**
     * @brief Weak pointer to the singleton object.
     */
    static std::weak_ptr<IGstSrc> m_gstSrc;

    /**
     * @brief Mutex protection for creation of the GstSrc object.
     */
    static std::mutex m_creationMutex;

    std::shared_ptr<IGstSrc> getGstSrc() override;
};

/**
 * @brief The definition of the GstSrc.
 */
class GstSrc : public IGstSrc
{
public:
    /**
     * @brief The constructor.
     *
     * @param[in] gstWrapperFactory     : The gstreamer wrapper factory.
     * @param[in] glibWrapperFactory    : The glib wrapper factory.
     * @param[in] decryptorFactory      : The decryptor factory.
     */
    GstSrc(const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapperFactory> &gstWrapperFactory,
           const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapperFactory> &glibWrapperFactory,
           const std::shared_ptr<IGstDecryptorElementFactory> &decryptorFactory);

    /**
     * @brief Virtual destructor.
     */
    virtual ~GstSrc() {}

    void initSrc() override;

    void setupAndAddAppSrc(IDecryptionService *decryptionService, GstElement *source, StreamInfo &streamInfo,
                           GstAppSrcCallbacks *callbacks, gpointer userData,
                           firebolt::rialto::MediaSourceType type) override;

    void allAppSrcsAdded(GstElement *element) override;

protected:
    /*
     * @brief sets the default stream format if needed.
     */
    void setDefaultStreamFormatIfNeeded(GstElement *appSrc);

    /**
     * @brief The gstreamer wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;

    /**
     * @brief The glib wrapper object.
     */
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;

    /**
     * @brief The gst decryptor element factory object.
     */
    std::shared_ptr<IGstDecryptorElementFactory> m_decryptorFactory;

    /**
     * @brief Create a payloader element.
     *
     * @retval the payloader element.
     */
    GstElement *createPayloader();
};

}; // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_GST_SRC_H_
