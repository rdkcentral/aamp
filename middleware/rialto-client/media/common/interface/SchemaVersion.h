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

#ifndef FIREBOLT_RIALTO_COMMON_SCHEMA_VERSION_H_
#define FIREBOLT_RIALTO_COMMON_SCHEMA_VERSION_H_

#include <cstdint>
#include <string>

namespace firebolt::rialto::common
{
class SchemaVersion
{
public:
    SchemaVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t patch);
    ~SchemaVersion() = default;

    bool operator==(const SchemaVersion &other) const;
    bool isCompatible(const SchemaVersion &other) const;
    std::string str() const;
    std::uint32_t major() const;
    std::uint32_t minor() const;
    std::uint32_t patch() const;

private:
    std::uint32_t m_major;
    std::uint32_t m_minor;
    std::uint32_t m_patch;
};

// Current schema version, common for server and client. Taken from main CMakeLists.txt file
SchemaVersion getCurrentSchemaVersion();
} // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_SCHEMA_VERSION_H_
