/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#ifndef FIREBOLT_RIALTO_IPC_FILE_DESCRIPTOR_H_
#define FIREBOLT_RIALTO_IPC_FILE_DESCRIPTOR_H_

// -----------------------------------------------------------------------------
/*!
    \class FileDescriptor
    \brief Light wrapper around a file descriptor so it can be used safely.

    Why do we need this?  Because we want to safely pass a file descriptor
    around.

    Why not just use an integer? Because although it's obviously fine to pass
    an integer around there is no guarantee that the descriptor is still valid
    when it's used.  This class uses \c dup(2) to ensure that if the object was
    created with a valid file descriptor in the first place then it and all copy
    constructed objects will have a valid file descriptor.

*/

namespace firebolt::rialto::ipc
{
class FileDescriptor
{
public:
    FileDescriptor();
    explicit FileDescriptor(int fd);
    FileDescriptor(const FileDescriptor &other);
    FileDescriptor &operator=(FileDescriptor &&other) noexcept;
    FileDescriptor &operator=(const FileDescriptor &other);
    ~FileDescriptor();

public:
    bool isValid() const;
    int fd() const;

    void reset(int fd = -1);
    void clear();

    int release();

private:
    int m_fd;
};

} // namespace firebolt::rialto::ipc

#endif // FIREBOLT_RIALTO_IPC_FILE_DESCRIPTOR_H_
