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

#include "OpenCDMSessionPrivate.h"
#include "RialtoGStreamerEMEProtectionMetadata.h"
#include <gst/base/base.h>
#include <gst/gst.h>
#include <gst/gstprotection.h>

namespace
{
const KeyStatus convertKeyStatus(const firebolt::rialto::KeyStatus &keyStatus)
{
    switch (keyStatus)
    {
    case firebolt::rialto::KeyStatus::USABLE:
    {
        return Usable;
    }
    case firebolt::rialto::KeyStatus::EXPIRED:
    {
        return Expired;
    }
    case firebolt::rialto::KeyStatus::RELEASED:
    {
        return Released;
    }
    case firebolt::rialto::KeyStatus::OUTPUT_RESTRICTED:
    {
        return OutputRestricted;
    }
    case firebolt::rialto::KeyStatus::PENDING:
    {
        return StatusPending;
    }
    case firebolt::rialto::KeyStatus::INTERNAL_ERROR:
    default:
    {
        return InternalError;
    }
    }
}

const std::string kDefaultSessionId{"0"};
} // namespace

OpenCDMSessionPrivate::OpenCDMSessionPrivate(const std::shared_ptr<ICdmBackend> &cdm,
                                             const std::shared_ptr<IMessageDispatcher> &messageDispatcher,
                                             const LicenseType &sessionType, OpenCDMSessionCallbacks *callbacks,
                                             void *context, const std::string &initDataType,
                                             const std::vector<uint8_t> &initData)
    : m_log{"OpenCDMSessionPrivate"}, m_context(context), m_cdmBackend(cdm), m_messageDispatcher(messageDispatcher),
      m_rialtoSessionId(firebolt::rialto::kInvalidSessionId), m_callbacks(callbacks),
      m_sessionType(getRialtoSessionType(sessionType)), m_initDataType(getRialtoInitDataType(initDataType)),
      m_initData(initData), m_isInitialized{false}
{
    m_log << debug << "constructed: " << static_cast<void *>(this);
}

OpenCDMSessionPrivate::~OpenCDMSessionPrivate()
{
    m_log << debug << "destructed: " << static_cast<void *>(this);
    if (m_isInitialized)
    {
        releaseSession();
    }
}

bool OpenCDMSessionPrivate::initialize()
{
    return initialize(false);
}

bool OpenCDMSessionPrivate::initialize(bool isLDL)
{
    if (!m_cdmBackend || !m_messageDispatcher)
    {
        m_log << error << "Cdm/message dispatcher is NULL or not initialized";
        return false;
    }
    if (!m_isInitialized)
    {
        if (!m_cdmBackend->createKeySession(m_sessionType, isLDL, m_rialtoSessionId))
        {
            m_log << error << "Failed to create a session. Got drm error %u", getLastDrmError();
            return false;
        }
        m_messageDispatcherClient = m_messageDispatcher->createClient(this);
        m_isInitialized = true;
        m_log << info << "Successfully created a session";
    }
    return true;
}

bool OpenCDMSessionPrivate::generateRequest(const std::string &initDataType, const std::vector<uint8_t> &initData,
                                            const std::vector<uint8_t> &cdmData)
{
    firebolt::rialto::InitDataType dataType = getRialtoInitDataType(initDataType);
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if ((dataType != firebolt::rialto::InitDataType::UNKNOWN) && (-1 != m_rialtoSessionId))
    {
        if (m_cdmBackend->generateRequest(m_rialtoSessionId, dataType, initData))
        {
            m_log << info << "Successfully generated the request for the session";
            initializeCdmKeySessionId();
            return true;
        }
        else
        {
            m_log << error << "Failed to request for the session. Got drm error " << getLastDrmError();
        }
    }

    return false;
}

bool OpenCDMSessionPrivate::loadSession()
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if (-1 != m_rialtoSessionId)
    {
        if (m_cdmBackend->loadSession(m_rialtoSessionId))
        {
            m_log << info << "Successfully loaded the session";
            return true;
        }
        else
        {
            m_log << error << "Failed to load the session. Got drm error " << getLastDrmError();
        }
    }
    return false;
}

bool OpenCDMSessionPrivate::updateSession(const std::vector<uint8_t> &license)
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if (-1 != m_rialtoSessionId)
    {
        if (m_cdmBackend->updateSession(m_rialtoSessionId, license))
        {
            m_log << info << "Successfully updated the session";
            return true;
        }
        else
        {
            m_log << error << "Failed to update the session. Got drm error " << getLastDrmError();
        }
    }

    return false;
}

bool OpenCDMSessionPrivate::getChallengeData(std::vector<uint8_t> &challengeData)
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }
    if ((m_initDataType != firebolt::rialto::InitDataType::UNKNOWN) && (-1 != m_rialtoSessionId))
    {
        if (m_cdmBackend->generateRequest(m_rialtoSessionId, m_initDataType, m_initData))
        {
            m_log << info << "Successfully generated the request for the session";
            initializeCdmKeySessionId();
        }
        else
        {
            m_log << error << "Failed to request for the session. Got drm error " << getLastDrmError();
            return false;
        }
    }
    else
    {
        return false;
    }
    std::unique_lock<std::mutex> lock{m_mutex};
    m_challengeCv.wait_for(lock, std::chrono::seconds{1}, [this]() { return !m_challengeData.empty(); });
    challengeData = m_challengeData;
    return !challengeData.empty();
}

void OpenCDMSessionPrivate::addProtectionMeta(GstBuffer *buffer, GstBuffer *subSample, const uint32_t subSampleCount,
                                              GstBuffer *IV, GstBuffer *keyID, uint32_t initWithLast15)
{
    // Set key for Playready
    GstBuffer *keyToApply = keyID;
    bool shouldReleaseKey{false};
    if (keyID && 0 == gst_buffer_get_size(keyID) && !m_playreadyKeyId.empty())
    {
        keyToApply = gst_buffer_new_allocate(nullptr, m_playreadyKeyId.size(), nullptr);
        gst_buffer_fill(keyToApply, 0, m_playreadyKeyId.data(), m_playreadyKeyId.size());
        shouldReleaseKey = true;
    }

    GstStructure *info = gst_structure_new("application/x-cenc", "encrypted", G_TYPE_BOOLEAN, TRUE, "mks_id",
                                           G_TYPE_INT, m_rialtoSessionId, "kid", GST_TYPE_BUFFER, keyToApply, "iv_size",
                                           G_TYPE_UINT, gst_buffer_get_size(IV), "iv", GST_TYPE_BUFFER, IV,
                                           "subsample_count", G_TYPE_UINT, subSampleCount, "subsamples", GST_TYPE_BUFFER,
                                           subSample, "encryption_scheme", G_TYPE_UINT, 0, // AES Counter
                                           "init_with_last_15", G_TYPE_UINT, initWithLast15, NULL);

    GstProtectionMeta *protectionMeta = reinterpret_cast<GstProtectionMeta *>(gst_buffer_get_protection_meta(buffer));
    if (protectionMeta && protectionMeta->info)
    {
        const char *cipherModeBuf = gst_structure_get_string(protectionMeta->info, "cipher-mode");
        if (cipherModeBuf)
        {
            GST_INFO("Copy cipher mode [%s] and crypt/skipt byte blocks to protection metadata.", cipherModeBuf);
            gst_structure_set(info, "cipher-mode", G_TYPE_STRING, cipherModeBuf, NULL);

            uint32_t patternCryptoBlocks = 0;
            uint32_t patternClearBlocks = 0;

            if (gst_structure_get_uint(protectionMeta->info, "crypt_byte_block", &patternCryptoBlocks))
            {
                gst_structure_set(info, "crypt_byte_block", G_TYPE_UINT, patternCryptoBlocks, NULL);
            }

            if (gst_structure_get_uint(protectionMeta->info, "skip_byte_block", &patternClearBlocks))
            {
                gst_structure_set(info, "skip_byte_block", G_TYPE_UINT, patternClearBlocks, NULL);
            }
        }
    }

    rialto_mse_add_protection_metadata(buffer, info);

    if (shouldReleaseKey)
    {
        gst_buffer_unref(keyToApply);
    }
}

bool OpenCDMSessionPrivate::addProtectionMeta(GstBuffer *buffer)
{
    GstProtectionMeta *protectionMeta = reinterpret_cast<GstProtectionMeta *>(gst_buffer_get_protection_meta(buffer));
    if (!protectionMeta)
    {
        m_log << debug << "No protection meta added to the buffer";
        return false;
    }

    GstStructure *info = gst_structure_copy(protectionMeta->info);
    gst_structure_set(info, "mks_id", G_TYPE_INT, m_rialtoSessionId, NULL);

    if (!gst_structure_has_field_typed(info, "encrypted", G_TYPE_BOOLEAN))
    {
        // Set encrypted
        gst_structure_set(info, "encrypted", G_TYPE_BOOLEAN, TRUE, NULL);
    }

    if (gst_structure_has_field_typed(info, "iv", GST_TYPE_BUFFER) &&
        !gst_structure_has_field_typed(info, "iv_size", G_TYPE_UINT))
    {
        const GValue *value = gst_structure_get_value(info, "iv");
        if (value)
        {
            GstBuffer *ivBuffer = gst_value_get_buffer(value);
            // Set iv size
            gst_structure_set(info, "iv_size", G_TYPE_UINT, gst_buffer_get_size(ivBuffer), NULL);
        }
    }

    if (!gst_structure_has_field_typed(info, "encryption_scheme", G_TYPE_UINT))
    {
        // Not used but required
        gst_structure_set(info, "encryption_scheme", G_TYPE_UINT, 0, NULL);
    }

    // Set key for Playready
    if (!m_playreadyKeyId.empty())
    {
        GstBuffer *keyID = gst_buffer_new_allocate(nullptr, m_playreadyKeyId.size(), nullptr);
        gst_buffer_fill(keyID, 0, m_playreadyKeyId.data(), m_playreadyKeyId.size());
        gst_structure_set(info, "kid", GST_TYPE_BUFFER, keyID, NULL);
        gst_buffer_unref(keyID);
    }

    rialto_mse_add_protection_metadata(buffer, info);

    return true;
}

bool OpenCDMSessionPrivate::closeSession()
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if (-1 != m_rialtoSessionId)
    {
        if (m_cdmBackend->closeKeySession(m_rialtoSessionId))
        {
            m_log << info << "Successfully closed the session";
            m_messageDispatcherClient.reset();
            m_challengeData.clear();
            m_keyStatuses.clear();
            return true;
        }
        else
        {
            m_log << warn << "Failed to close the session.";
        }
    }

    return false;
}

bool OpenCDMSessionPrivate::removeSession()
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if (-1 != m_rialtoSessionId)
    {
        if (m_cdmBackend->removeKeySession(m_rialtoSessionId))
        {
            m_log << info << "Successfully removed the session";
            return true;
        }
        else
        {
            m_log << warn << "Failed to remove the session.";
        }
    }

    return false;
}

void OpenCDMSessionPrivate::releaseSession()
{
    if (-1 != m_rialtoSessionId)
    {
        if (m_cdmBackend->releaseKeySession(m_rialtoSessionId))
        {
            m_log << info << "Successfully released the session";
        }
        else
        {
            m_log << warn << "Failed to release the session.";
        }
    }
}

bool OpenCDMSessionPrivate::containsKey(const std::vector<uint8_t> &keyId)
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if (-1 != m_rialtoSessionId)
    {
        return m_cdmBackend->containsKey(m_rialtoSessionId, keyId);
    }
    return false;
}

bool OpenCDMSessionPrivate::setDrmHeader(const std::vector<uint8_t> &drmHeader)
{
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return false;
    }

    if (-1 != m_rialtoSessionId)
    {
        return m_cdmBackend->setDrmHeader(m_rialtoSessionId, drmHeader);
    }
    return false;
}

bool OpenCDMSessionPrivate::selectKeyId(const std::vector<uint8_t> &keyId)
{
    m_log << debug << "Playready key selected.";
    m_playreadyKeyId = keyId;
    return true;
}

void OpenCDMSessionPrivate::onLicenseRequest(int32_t keySessionId,
                                             const std::vector<unsigned char> &licenseRequestMessage,
                                             const std::string &url)
{
    if (keySessionId == m_rialtoSessionId)
    {
        updateChallenge(licenseRequestMessage);

        if ((m_callbacks) && (m_callbacks->process_challenge_callback))
        {
            m_callbacks->process_challenge_callback(this, m_context, url.c_str(), licenseRequestMessage.data(),
                                                    licenseRequestMessage.size());
        }
    }
}

void OpenCDMSessionPrivate::onLicenseRenewal(int32_t keySessionId, const std::vector<unsigned char> &licenseRenewalMessage)
{
    if (keySessionId == m_rialtoSessionId)
    {
        updateChallenge(licenseRenewalMessage);

        if ((m_callbacks) && (m_callbacks->process_challenge_callback))
        {
            m_callbacks->process_challenge_callback(this, m_context, "" /*URL*/, licenseRenewalMessage.data(),
                                                    licenseRenewalMessage.size());
        }
    }
}

void OpenCDMSessionPrivate::updateChallenge(const std::vector<unsigned char> &challenge)
{
    std::unique_lock<std::mutex> lock{m_mutex};
    m_challengeData = challenge;
    m_challengeCv.notify_one();
}

void OpenCDMSessionPrivate::onKeyStatusesChanged(int32_t keySessionId,
                                                 const firebolt::rialto::KeyStatusVector &keyStatuses)
{
    if ((keySessionId == m_rialtoSessionId) && (m_callbacks) && (m_callbacks->key_update_callback))
    {
        for (const std::pair<std::vector<uint8_t>, firebolt::rialto::KeyStatus> &keyStatus : keyStatuses)
        {
            // Update internal key statuses
            m_keyStatuses[keyStatus.first] = keyStatus.second;

            const std::vector<uint8_t> &key = keyStatus.first;
            m_callbacks->key_update_callback(this, m_context, key.data(), key.size());
        }

        if (m_callbacks->keys_updated_callback)
        {
            m_callbacks->keys_updated_callback(this, m_context);
        }
    }
}

KeyStatus OpenCDMSessionPrivate::status(const std::vector<uint8_t> &key) const
{
    auto it = m_keyStatuses.find(key);
    if (it != m_keyStatuses.end())
    {
        return convertKeyStatus(it->second);
    }
    return KeyStatus::InternalError;
}

const std::string &OpenCDMSessionPrivate::getSessionId() const
{
    return m_cdmKeySessionId;
}

void OpenCDMSessionPrivate::initializeCdmKeySessionId()
{
    bool result{false};

    if (-1 != m_rialtoSessionId)
    {
        result = m_cdmBackend->getCdmKeySessionId(m_rialtoSessionId, m_cdmKeySessionId);
    }
    if (!result)
    {
        m_cdmKeySessionId = kDefaultSessionId;
    }
}

uint32_t OpenCDMSessionPrivate::getLastDrmError() const
{
    uint32_t err = 0;
    if (!m_cdmBackend)
    {
        m_log << error << "Cdm is NULL or not initialized";
        return -1;
    }

    (void)m_cdmBackend->getLastDrmError(m_rialtoSessionId, err);

    return err;
}

firebolt::rialto::KeySessionType OpenCDMSessionPrivate::getRialtoSessionType(const LicenseType licenseType)
{
    switch (licenseType)
    {
    case Temporary:
        return firebolt::rialto::KeySessionType::TEMPORARY;
    case PersistentUsageRecord: /// TODO: Rialto's equivalent??
        return firebolt::rialto::KeySessionType::UNKNOWN;
    case PersistentLicense:
        return firebolt::rialto::KeySessionType::PERSISTENT_LICENCE;
    default:
        return firebolt::rialto::KeySessionType::UNKNOWN;
    }
}

firebolt::rialto::InitDataType OpenCDMSessionPrivate::getRialtoInitDataType(const std::string &type)
{
    firebolt::rialto::InitDataType initDataType = firebolt::rialto::InitDataType::UNKNOWN;

    if (type == "cenc")
    {
        initDataType = firebolt::rialto::InitDataType::CENC;
    }
    else if (type == "webm")
    {
        initDataType = firebolt::rialto::InitDataType::WEBM;
    }
    else if (type == "drmheader")
    {
        initDataType = firebolt::rialto::InitDataType::DRMHEADER;
    }

    return initDataType;
}
