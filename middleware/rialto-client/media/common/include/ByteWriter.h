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

#ifndef FIREBOLT_RIALTO_COMMON_BYTE_WRITER_H_
#define FIREBOLT_RIALTO_COMMON_BYTE_WRITER_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace firebolt::rialto::common
{
class ByteWriter
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~ByteWriter() {}
    /**
     * @brief Write a byte to the buffer
     *
     * @param[in] buf  : the buffer pointer.
     * @param[in] off  : the offset within the buffer.
     * @param[in] byte : the byte to write;
     *
     * @retval the new offset.
     */
    virtual size_t writeByte(uint8_t *buf, size_t off, uint8_t byte) const;

    /**
     * @brief Writes a number of bytes to the buffer
     *
     * @param[in] buf   : the buffer pointer.
     * @param[in] off   : the offset within the buffer.
     * @param[in] bytes : the bytes to write.
     * @param[in] count : the number of bytes to write.
     *
     * @retval the new offset.
     */
    virtual size_t writeBytes(uint8_t *buf, size_t off, const uint8_t *bytes, size_t count) const;
    /**
     * @brief Fills the buffer with a number of bytes.
     *
     * @param[in] buf   : the buffer pointer.
     * @param[in] off   : the offset within the buffer.
     * @param[in] byte  : the byte to write.
     * @param[in] count : the number of bytes to write.
     *
     * @retval the new offset.
     */
    virtual size_t fillBytes(uint8_t *buf, size_t off, uint8_t byte, size_t count) const;
    /**
     * @brief Write a 32 bit unsigned int to the buffer
     *
     * @param[in] buf : the buffer pointer.
     * @param[in] off : the offset within the buffer.
     * @param[in] val : the value to write;
     *
     * @retval the new offset.
     */
    virtual size_t writeUint32(uint8_t *buf, size_t off, uint32_t val) const;
    /**
     * @brief Write a 64 bit signed int to the buffer
     *
     * @param[in] buf : the buffer pointer.
     * @param[in] off : the offset within the buffer.
     * @param[in] val : the value to write;
     *
     * @retval the new offset.
     */
    virtual size_t writeInt64(uint8_t *buf, size_t off, int64_t val) const;
    /**
     * @brief Write a C string to the buffer including terminating null.
     *
     * @param[in] buf : the buffer pointer.
     * @param[in] off : the offset within the buffer.
     * @param[in] str : the str to write;
     *
     * @retval the new offset.
     */
    virtual size_t writeCString(uint8_t *buf, size_t off, const char *str) const;
};
} // namespace firebolt::rialto::common

#endif // FIREBOLT_RIALTO_COMMON_BYTE_WRITER_H_
