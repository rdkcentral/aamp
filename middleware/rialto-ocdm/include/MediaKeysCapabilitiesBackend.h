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

#ifndef MEDIA_KEYS_CAPABILITIES_BACKEND_H_
#define MEDIA_KEYS_CAPABILITIES_BACKEND_H_

#include <IMediaKeysCapabilities.h>
#include <memory>
#include <opencdm/open_cdm.h>
#include <string>
#include <vector>

class MediaKeysCapabilitiesBackend
{
public:
    static MediaKeysCapabilitiesBackend &instance();

    std::vector<std::string> getSupportedKeySystems();
    OpenCDMError supportsKeySystem(const std::string &keySystem);
    bool getSupportedKeySystemVersion(const std::string &keySystem, std::string &version);
    bool isServerCertificateSupported(const std::string &keySystem);

private:
    MediaKeysCapabilitiesBackend();
    ~MediaKeysCapabilitiesBackend();

private:
    std::shared_ptr<firebolt::rialto::IMediaKeysCapabilities> m_mediaKeysCapabilities;
};

#endif // MEDIA_KEYS_CAPABILITIES_BACKEND_H_
