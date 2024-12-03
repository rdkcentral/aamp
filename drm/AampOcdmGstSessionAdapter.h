/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

/**
 * @file AampOcdmGstSessionAdapter.h
 * @brief File holds operations on OCDM gst sessions
 */


#include <dlfcn.h>
#include <mutex>
#include <gst/gst.h>
#include "opencdmsessionadapter.h"


/**
 * @class AAMPOCDMGSTSessionAdapter
 * @brief OCDM Gstreamer session to decrypt
 */

class AAMPOCDMGSTSessionAdapter : public AAMPOCDMSessionAdapter
{
        void ExtractSEI( GstBuffer *buffer);
public:
	AAMPOCDMGSTSessionAdapter(std::shared_ptr<AampDrmHelper> drmHelper,  AampDrmCallbacks *drmCallbacks) : AAMPOCDMSessionAdapter(drmHelper, drmCallbacks)
, AAMPOCDMGSTSessionDecrypt(nullptr)
	{
                const char* ocdmgstsessiondecrypt = "opencdm_gstreamer_session_decrypt_buffer";
                AAMPOCDMGSTSessionDecrypt = (OpenCDMError(*)(struct OpenCDMSession*, GstBuffer*, GstCaps*))dlsym(RTLD_DEFAULT, ocdmgstsessiondecrypt);
                if (AAMPOCDMGSTSessionDecrypt)
                    AAMPLOG_WARN("Has opencdm_gstreamer_session_decrypt_buffer");
                else
                    AAMPLOG_WARN("No opencdm_gstreamer_session_decrypt_buffer found");
	};
	~AAMPOCDMGSTSessionAdapter() {};

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps);
	int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData);
private:
        OpenCDMError(*AAMPOCDMGSTSessionDecrypt)(struct OpenCDMSession*, GstBuffer*, GstCaps*);

};
