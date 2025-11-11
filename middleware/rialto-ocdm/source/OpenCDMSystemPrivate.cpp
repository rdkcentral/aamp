/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 Sky UK
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

#include "OpenCDMSystemPrivate.h"
#include "ActiveSessions.h"
#include "IMediaKeys.h"
#include <MediaCommon.h>

OpenCDMSystem *createSystem(const char system[], const std::string &metadata)
{
    const std::string kKeySystem{system};
    auto messageDispatcher{std::make_shared<MessageDispatcher>()};
    auto cdmBackend{std::make_shared<CdmBackend>(kKeySystem, messageDispatcher,
                                                 firebolt::rialto::IMediaKeysFactory::createFactory())};
    return new OpenCDMSystemPrivate(kKeySystem, metadata, messageDispatcher, cdmBackend);
}

OpenCDMSystemPrivate::OpenCDMSystemPrivate(const std::string &system, const std::string &metadata,
                                           const std::shared_ptr<MessageDispatcher> &messageDispatcher,
                                           const std::shared_ptr<CdmBackend> &cdmBackend)
    : m_log{"OpenCDMSystemPrivate"}, m_keySystem(system), m_metadata(metadata),
      m_control{firebolt::rialto::IControlFactory::createFactory()->createControl()},
      m_messageDispatcher{messageDispatcher}, m_cdmBackend{cdmBackend}
{
    m_log << debug << "constructed: " << static_cast<void *>(this);
    if (!m_control || !m_cdmBackend)
    {
        return;
    }
    firebolt::rialto::ApplicationState initialState{firebolt::rialto::ApplicationState::UNKNOWN};
    m_control->registerClient(m_cdmBackend, initialState);
    m_cdmBackend->initialize(initialState);
}

OpenCDMSystemPrivate::~OpenCDMSystemPrivate()
{
    m_log << debug << "destructed: " << static_cast<void *>(this);
}

const std::string &OpenCDMSystemPrivate::keySystem() const
{
    return m_keySystem;
}

const std::string &OpenCDMSystemPrivate::metadata() const
{
    return m_metadata;
}

OpenCDMSession *OpenCDMSystemPrivate::createSession(const LicenseType licenseType, OpenCDMSessionCallbacks *callbacks,
                                                    void *userData, const std::string &initDataType,
                                                    const std::vector<uint8_t> &initData) const
{
    return ActiveSessions::instance().create(m_cdmBackend, m_messageDispatcher, licenseType, callbacks, userData,
                                             initDataType, initData);
}

bool OpenCDMSystemPrivate::getDrmTime(uint64_t &drmTime) const
{
    if (!m_cdmBackend)
    {
        return false;
    }
    return m_cdmBackend->getDrmTime(drmTime);
}

bool OpenCDMSystemPrivate::getLdlSessionsLimit(uint32_t &ldlLimit) const
{
    if (!m_cdmBackend)
    {
        return false;
    }
    return m_cdmBackend->getLdlSessionsLimit(ldlLimit);
}

bool OpenCDMSystemPrivate::getKeyStoreHash(std::vector<unsigned char> &keyStoreHash) const
{
    if (!m_cdmBackend)
    {
        return false;
    }
    return m_cdmBackend->getKeyStoreHash(keyStoreHash);
}

bool OpenCDMSystemPrivate::getDrmStoreHash(std::vector<unsigned char> &drmStoreHash) const
{
    if (!m_cdmBackend)
    {
        return false;
    }
    return m_cdmBackend->getDrmStoreHash(drmStoreHash);
}

bool OpenCDMSystemPrivate::deleteKeyStore() const
{
    if (!m_cdmBackend)
    {
        return false;
    }
    return m_cdmBackend->deleteKeyStore();
}

bool OpenCDMSystemPrivate::deleteDrmStore() const
{
    if (!m_cdmBackend)
    {
        return false;
    }
    return m_cdmBackend->deleteDrmStore();
}

bool OpenCDMSystemPrivate::getMetricSystemData(std::vector<uint8_t> &buffer) const
{
    if (!m_cdmBackend)
    {
        return false;
    }

    return m_cdmBackend->getMetricSystemData(buffer);
}
