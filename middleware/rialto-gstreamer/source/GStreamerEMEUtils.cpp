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

#include <cstdint>

#include <stdio.h>

#include "GStreamerEMEUtils.h"
#include "GStreamerUtils.h"
#include "GstreamerCatLog.h"
#include "RialtoGStreamerEMEProtectionMetadata.h"

#define GST_CAT_DEFAULT rialtoGStreamerCat
void getEncryptedFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    gboolean encrypted = FALSE;
    gst_structure_get_boolean(protectionMeta->info, "encrypted", &encrypted);
    metadata.encrypted = encrypted;
}

void getMediaKeySessionIdFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta,
                                                BufferProtectionMetadata &metadata)
{
    gint mediaKeySessionId = 0;
    gst_structure_get_int(protectionMeta->info, "mks_id", &mediaKeySessionId);
    metadata.mediaKeySessionId = mediaKeySessionId;
}

void getKIDFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    const GValue *value = gst_structure_get_value(protectionMeta->info, "kid");
    if (value)
    {
        GstBuffer *keyIDBuffer = gst_value_get_buffer(value);
        if (keyIDBuffer)
        {
            GstMappedBuffer mappedKeyID(keyIDBuffer, GST_MAP_READ);
            if (mappedKeyID)
            {
                metadata.kid = std::vector<uint8_t>(mappedKeyID.data(), mappedKeyID.data() + mappedKeyID.size());
            }
        }
    }
}

void getIVFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    unsigned ivSize = 0;
    gst_structure_get_uint(protectionMeta->info, "iv_size", &ivSize);
    if (!ivSize && !g_strcmp0(gst_structure_get_string(protectionMeta->info, "cipher-mode"), "cbcs"))
    {
        // in case of cbcs, the same initialization vector is used to decrypt all the blocks
        gst_structure_get_uint(protectionMeta->info, "constant_iv_size", &ivSize);
    }
    const GValue *value = gst_structure_get_value(protectionMeta->info, "iv");
    if (value)
    {
        GstBuffer *ivBuffer = gst_value_get_buffer(value);
        if (ivBuffer)
        {
            GstMappedBuffer mappedIV(ivBuffer, GST_MAP_READ);
            if (mappedIV && (ivSize == mappedIV.size()))
            {
                metadata.iv = std::vector<uint8_t>(mappedIV.data(), mappedIV.data() + mappedIV.size());
            }
        }
    }
}

void getSubSamplesFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta, BufferProtectionMetadata &metadata)
{
    unsigned int subSampleCount = 0;
    gst_structure_get_uint(protectionMeta->info, "subsample_count", &subSampleCount);

    if (subSampleCount)
    {
        const GValue *value = gst_structure_get_value(protectionMeta->info, "subsamples");
        if (value)
        {
            GstBuffer *subSamplesBuffer = gst_value_get_buffer(value);
            if (subSamplesBuffer)
            {
                GstMappedBuffer mappedSubSamples(subSamplesBuffer, GST_MAP_READ);
                if (mappedSubSamples &&
                    ((mappedSubSamples.size() / (sizeof(int16_t) + sizeof(int32_t))) == subSampleCount))
                {
                    std::vector<uint8_t> subSamples(mappedSubSamples.data(),
                                                    mappedSubSamples.data() + mappedSubSamples.size());
                    //'senc' atom
                    // unsigned   int(16)      subsample_count;
                    //{
                    //  unsigned   int(16)      BytesOfClearData;
                    //  unsigned   int(32)      BytesOfEncryptedData;
                    //}[subsample_count]
                    size_t subSampleOffset = 0;
                    for (unsigned int subSampleIdx = 0; subSampleIdx < subSampleCount; ++subSampleIdx)
                    {
                        uint16_t bytesOfClearData = (uint16_t)subSamples[subSampleOffset] << 8 |
                                                    (uint16_t)subSamples[subSampleOffset + 1];
                        uint32_t bytesOfEncryptedData = (uint32_t)subSamples[subSampleOffset + 2] << 24 |
                                                        (uint32_t)subSamples[subSampleOffset + 3] << 16 |
                                                        (uint32_t)subSamples[subSampleOffset + 4] << 8 |
                                                        (uint32_t)subSamples[subSampleOffset + 5];
                        metadata.subsamples.push_back(
                            std::make_pair((uint32_t)bytesOfClearData, (uint32_t)bytesOfEncryptedData));
                        subSampleOffset += sizeof(int16_t) + sizeof(int32_t);
                    }
                }
            }
        }
    }
}

void getInitWithLast15FromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta,
                                             BufferProtectionMetadata &metadata)
{
    guint initWithLast15 = 0;
    gst_structure_get_uint(protectionMeta->info, "init_with_last_15", &initWithLast15);
    metadata.initWithLast15 = initWithLast15;
}

void getEncryptionSchemeFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta,
                                               BufferProtectionMetadata &metadata)
{
    const char *cipherModeBuf = gst_structure_get_string(protectionMeta->info, "cipher-mode");
    GST_INFO("Retrieved encryption scheme '%s' from protection metadata.", cipherModeBuf ? cipherModeBuf : "unknown");
    if (g_strcmp0(cipherModeBuf, "cbcs") == 0)
    {
        metadata.cipherMode = firebolt::rialto::CipherMode::CBCS;
    }
    else if (g_strcmp0(cipherModeBuf, "cenc") == 0)
    {
        metadata.cipherMode = firebolt::rialto::CipherMode::CENC;
    }
    else if (g_strcmp0(cipherModeBuf, "cbc1") == 0)
    {
        metadata.cipherMode = firebolt::rialto::CipherMode::CBC1;
    }
    else if (g_strcmp0(cipherModeBuf, "cens") == 0)
    {
        metadata.cipherMode = firebolt::rialto::CipherMode::CENS;
    }
    else
    {
        if (cipherModeBuf)
        {
            GST_ERROR("Unknown encryption scheme '%s'!", cipherModeBuf);
        }
        else
        {
            GST_ERROR("Missing encryption scheme!");
        }
        metadata.cipherMode = firebolt::rialto::CipherMode::UNKNOWN;
    }
}

void getEncryptionPatternFromProtectionMetadata(GstRialtoProtectionMetadata *protectionMeta,
                                                BufferProtectionMetadata &metadata)
{
    if (gst_structure_get_uint(protectionMeta->info, "crypt_byte_block", &metadata.cryptBlocks) == false)
    {
        GST_INFO("Failed to get crypt_byte_block value!");
        return;
    }
    if (gst_structure_get_uint(protectionMeta->info, "skip_byte_block", &metadata.skipBlocks) == false)
    {
        GST_INFO("Failed to get skip_byte_block value!");
        return;
    }

    GST_INFO("Successful retrieval of 'crypt_byte_block' and 'skip_byte_block'.");
    metadata.encryptionPatternSet = true;
}

void ProcessProtectionMetadata(GstBuffer *buffer, BufferProtectionMetadata &metadata)
{
    if (buffer == nullptr)
        return;

    GstRialtoProtectionMetadata *protectionMeta = reinterpret_cast<GstRialtoProtectionMetadata *>(
        gst_buffer_get_meta(buffer, GST_RIALTO_PROTECTION_METADATA_GET_TYPE));
    if (protectionMeta)
    {
        getEncryptedFromProtectionMetadata(protectionMeta, metadata);
        if (metadata.encrypted)
        {
            getMediaKeySessionIdFromProtectionMetadata(protectionMeta, metadata);
            getKIDFromProtectionMetadata(protectionMeta, metadata);
            getIVFromProtectionMetadata(protectionMeta, metadata);
            getSubSamplesFromProtectionMetadata(protectionMeta, metadata);
            getInitWithLast15FromProtectionMetadata(protectionMeta, metadata);
            getEncryptionSchemeFromProtectionMetadata(protectionMeta, metadata);
            getEncryptionPatternFromProtectionMetadata(protectionMeta, metadata);
        }
    }
}
