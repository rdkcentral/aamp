/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
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

#ifndef FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_DEEP_ELEMENT_ADDED_H_
#define FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_DEEP_ELEMENT_ADDED_H_

#include "GenericPlayerContext.h"
#include "IGlibWrapper.h"
#include "IGstGenericPlayerPrivate.h"
#include "IGstWrapper.h"
#include "IPlayerTask.h"
#include <gst/gst.h>
#include <memory>

namespace firebolt::rialto::server::tasks::generic
{
class DeepElementAdded : public IPlayerTask
{
public:
    DeepElementAdded(GenericPlayerContext &context, IGstGenericPlayerPrivate &player,
                     const std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> &gstWrapper,
                     const std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> &glibWrapper, GstBin *pipeline,
                     GstBin *bin, GstElement *element);
    ~DeepElementAdded() override;
    void execute() const override;

private:
    GenericPlayerContext &m_context;
    IGstGenericPlayerPrivate &m_player;
    std::shared_ptr<firebolt::rialto::wrappers::IGstWrapper> m_gstWrapper;
    std::shared_ptr<firebolt::rialto::wrappers::IGlibWrapper> m_glibWrapper;
    GstBin *m_pipeline;
    GstBin *m_bin;
    GstElement *m_element;
    gchar *m_elementName;
    bool m_callbackRegistered;
};
} // namespace firebolt::rialto::server::tasks::generic

#endif // FIREBOLT_RIALTO_SERVER_TASKS_GENERIC_DEEP_ELEMENT_ADDED_H_
