/*
 * Copyright (C) 2022 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once
#include <MediaCommon.h>
#include <gst/gst.h>
#include <gst/gstprotection.h>

#include <stdint.h>
#include <vector>

struct BufferProtectionMetadata
{
    BufferProtectionMetadata() : encrypted(false) {}

    bool encrypted{false};
    int mediaKeySessionId{-1};
    std::vector<uint8_t> iv;
    std::vector<uint8_t> kid;
    // vector of bytesOfClearData, bytesOfEncryptedData
    std::vector<std::pair<uint32_t, uint32_t>> subsamples;
    uint32_t initWithLast15{0};

    // Encryption scheme
    firebolt::rialto::CipherMode cipherMode{firebolt::rialto::CipherMode::UNKNOWN};
    uint32_t cryptBlocks{0};
    uint32_t skipBlocks{0};
    bool encryptionPatternSet{false};
};

void ProcessProtectionMetadata(GstBuffer *buffer, BufferProtectionMetadata &metadata);
