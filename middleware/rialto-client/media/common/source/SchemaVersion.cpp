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

#include "SchemaVersion.h"

namespace firebolt::rialto::common
{
SchemaVersion::SchemaVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t patch)
    : m_major{major}, m_minor{minor}, m_patch{patch}
{
}

bool SchemaVersion::operator==(const SchemaVersion &other) const
{
    return this->m_major == other.m_major && this->m_minor == other.m_minor && this->m_patch == other.m_patch;
}

bool SchemaVersion::isCompatible(const SchemaVersion &other) const
{
    return this->m_major == other.m_major;
}

std::string SchemaVersion::str() const
{
    return std::to_string(m_major) + "." + std::to_string(m_minor) + "." + std::to_string(m_patch);
}

std::uint32_t SchemaVersion::major() const
{
    return m_major;
}

std::uint32_t SchemaVersion::minor() const
{
    return m_minor;
}

std::uint32_t SchemaVersion::patch() const
{
    return m_patch;
}

SchemaVersion getCurrentSchemaVersion()
try
{
    return SchemaVersion{static_cast<std::uint32_t>(std::stoul(PROJECT_VER_MAJOR)),
                         static_cast<std::uint32_t>(std::stoul(PROJECT_VER_MINOR)),
                         static_cast<std::uint32_t>(std::stoul(PROJECT_VER_PATCH))};
}
catch (const std::exception &e)
{
    // If conversion can't be performed, return default version
    return SchemaVersion{1, 0, 0};
}
} // namespace firebolt::rialto::common
