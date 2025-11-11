/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Sky UK
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

#include "LinuxUtils.h"
#include "RialtoCommonLogging.h"
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
constexpr uid_t kNoOwnerChange = -1; // -1 means chown() won't change the owner
constexpr gid_t kNoGroupChange = -1; // -1 means chown() won't change the group

uid_t getFileOwnerId(const std::string &fileOwner)
{
    uid_t ownerId = kNoOwnerChange;
    const size_t kBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (!fileOwner.empty() && kBufferSize > 0)
    {
        errno = 0;
        passwd passwordStruct{};
        passwd *passwordResult = nullptr;
        char buffer[kBufferSize];
        int result = getpwnam_r(fileOwner.c_str(), &passwordStruct, buffer, kBufferSize, &passwordResult);
        if (result == 0 && passwordResult)
        {
            ownerId = passwordResult->pw_uid;
        }
        else
        {
            RIALTO_COMMON_LOG_SYS_WARN(errno, "Failed to determine ownerId for '%s'", fileOwner.c_str());
        }
    }
    return ownerId;
}

gid_t getFileGroupId(const std::string &fileGroup)
{
    gid_t groupId = kNoGroupChange;
    const size_t kBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (!fileGroup.empty() && kBufferSize > 0)
    {
        errno = 0;
        group groupStruct{};
        group *groupResult = nullptr;
        char buffer[kBufferSize];
        int result = getgrnam_r(fileGroup.c_str(), &groupStruct, buffer, kBufferSize, &groupResult);
        if (result == 0 && groupResult)
        {
            groupId = groupResult->gr_gid;
        }
        else
        {
            RIALTO_COMMON_LOG_SYS_WARN(errno, "Failed to determine groupId for '%s'", fileGroup.c_str());
        }
    }
    return groupId;
}
} // namespace

namespace firebolt::rialto::common
{
bool setFilePermissions(const std::string &filePath, unsigned int filePermissions)
{
    errno = 0;
    if (chmod(filePath.c_str(), filePermissions) != 0)
    {
        RIALTO_COMMON_LOG_SYS_WARN(errno, "Failed to change the permissions on %s", filePath.c_str());
        return false;
    }
    return true;
}

bool setFileOwnership(const std::string &filePath, const std::string &fileOwner, const std::string &fileGroup)
{
    uid_t ownerId = getFileOwnerId(fileOwner);
    gid_t groupId = getFileGroupId(fileGroup);

    if (ownerId != kNoOwnerChange || groupId != kNoGroupChange)
    {
        errno = 0;
        if (chown(filePath.c_str(), ownerId, groupId) != 0)
        {
            RIALTO_COMMON_LOG_SYS_WARN(errno, "Failed to change the owner/group for %s", filePath.c_str());
        }
    }
    return true;
}
} // namespace firebolt::rialto::common
