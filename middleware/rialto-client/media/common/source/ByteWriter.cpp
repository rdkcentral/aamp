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

#include "ByteWriter.h"
#include <cstring>

namespace firebolt::rialto::common
{
size_t ByteWriter::writeByte(uint8_t *buf, size_t off, uint8_t byte) const
{
    buf[off++] = byte;
    return off;
}

size_t ByteWriter::writeBytes(uint8_t *buf, size_t off, const uint8_t *bytes, size_t count) const
{
    if (count)
        memcpy(buf + off, bytes, count);
    return off + count;
}

size_t ByteWriter::fillBytes(uint8_t *buf, size_t off, uint8_t byte, size_t count) const
{
    if (count)
        memset(buf + off, byte, count);
    return off + count;
}

size_t ByteWriter::writeUint32(uint8_t *buf, size_t off, uint32_t val) const
{
    buf[off++] = (uint8_t)((val & 0x000000ff) >> 0);
    buf[off++] = (uint8_t)((val & 0x0000ff00) >> 8);
    buf[off++] = (uint8_t)((val & 0x00ff0000) >> 16);
    buf[off++] = (uint8_t)((val & 0xff000000) >> 24);
    return off;
}

size_t ByteWriter::writeInt64(uint8_t *buf, size_t off, int64_t val) const
{
    buf[off++] = (uint8_t)((val & 0x00000000000000ff) >> 0);
    buf[off++] = (uint8_t)((val & 0x000000000000ff00) >> 8);
    buf[off++] = (uint8_t)((val & 0x0000000000ff0000) >> 16);
    buf[off++] = (uint8_t)((val & 0x00000000ff000000) >> 24);
    buf[off++] = (uint8_t)((val & 0x000000ff00000000) >> 32);
    buf[off++] = (uint8_t)((val & 0x0000ff0000000000) >> 40);
    buf[off++] = (uint8_t)((val & 0x00ff000000000000) >> 48);
    buf[off++] = (uint8_t)((val & 0xff00000000000000) >> 56);
    return off;
}

size_t ByteWriter::writeCString(uint8_t *buf, size_t off, const char *str) const
{
    while (*str)
    {
        buf[off++] = *str++;
    }
    buf[off++] = *str;
    return off;
}
} // namespace firebolt::rialto::common
