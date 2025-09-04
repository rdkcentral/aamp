/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef _CURL_INTERFACE_H_
#define _CURL_INTERFACE_H_

/**
 * @file curl_interface.h
 * @brief Header file for interface between player and  middleware DRM
 */

#include <stddef.h>
#include <memory>
#include <priv_aamp.h>
#include <AampCurlDefine.h>
#include <AampUtils.h>

/**
 * @class CurlInterface
 */
class CurlInterface
{
public:
	CurlInterface();
	
	~CurlInterface();

	/**
	 * @fn HandleSSLWriteCallback
	 *
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param userdata CurlCallbackContext pointer
	 * @retval size consumed or 0 if interrupted
	 */
	size_t HandleSSLWriteCallback( const char *ptr, size_t size, size_t nmemb, void* userdata );

	/**
	 * @fn HandleSSLProgressCallback
	 *
	 * @param clientp app-specific as optionally set with CURLOPT_PROGRESSDATA
	 * @param dltotal total bytes expected to download
	 * @param dlnow downloaded bytes so far
	 * @param ultotal total bytes expected to upload
	 * @param ulnow uploaded bytes so far
	 * @retval negative value to abort, zero otherwise
	 */
	int HandleSSLProgressCallback( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow );

	/**
	 * @fn HandleSSLHeaderCallback
	 *
	 * @param ptr pointer to buffer containing the data
	 * @param size size of the buffer
	 * @param nmemb number of bytes
	 * @param user_data  CurlCallbackContext pointer
	 * @retval returns size * nmemb
	 */
	size_t HandleSSLHeaderCallback( const char *ptr, size_t size, size_t nmemb, void* userdata );
};

#endif // _CURL_INTERFACE_H_
