/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
* \file processProtection.cpp
*
* Process protection is for handling process protection of open cdm drm 
* for HSL Streaming  
* Functionalities are parse the file , get the drm type , PSSH data.
* Create DRM Session in thread 
* 
*/
#include "_base64.h"
#include "AampDRMSessionManager.h"
#include "AampDrmSession.h"
#include "fragmentcollector_hls.h"

#include <cstdlib>
#include <string>
using namespace std;

#ifdef AAMP_HLS_DRM

/**
 * Global aamp config data 
 */
extern AampConfig *gpGlobalConfig;
shared_ptr<AampDrmHelper> ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName);

/**
 * Local APIs declarations
 */
static int GetFieldValue(string &attrName, string keyName, string &valuePtr);
static int getPsshData(string attrName, string &psshData);
static shared_ptr<AampDrmHelper> getDrmHelper(string attrName , bool bPropagateUriParams, bool bDecryptClearSamplesRequired);

/**
 * @brief Return the string value, from the input KEY="value"
 * @param [in] attribute list to be searched 
 * @param [in] Key name to be checked to get the value
 * @param [out] value of the key
 * @return none
 */
static int GetFieldValue(string &attrName, string keyName, string &valuePtr){

	int valueStartPos = 0;
	int valueEndPos = 0;
	int keylen = (int)keyName.length();
	int status = DRM_API_FAILED;
	int found = 0, foundpos = 0;

	AAMPLOG_TRACE("Entering..");

	while ( (foundpos = (int)attrName.find(keyName,found)) != std::string::npos)
	{
		AAMPLOG_TRACE("keyName = %s",
		 keyName.c_str());

		valueStartPos = foundpos + keylen;
		if (attrName.at(valueStartPos) == '=')
		{
			string valueTempPtr = attrName.substr(valueStartPos+1);

			AAMPLOG_TRACE("valueTempPtr = %s",
			valueTempPtr.c_str());

			/* update start position based on substring */
			valueStartPos = 0;
			if (valueTempPtr.at(0) == '"')
			{
				valueTempPtr = valueTempPtr.substr(1);
				valueEndPos = (int)valueTempPtr.find('"');
			}
			else if ( (valueEndPos = (int)valueTempPtr.find(',')) == std::string::npos)
			{
				/*look like end string*/
				valueEndPos = (int)valueTempPtr.length();
			}

			valuePtr = valueTempPtr.substr(valueStartPos, valueEndPos);
			AAMPLOG_INFO("Value found : %s for Key : %s",
			valuePtr.c_str(), keyName.c_str());
			status = DRM_API_SUCCESS;
			break;
		}
		else
		{
			AAMPLOG_TRACE("Checking next occurrence of %s= in %s",
			keyName.c_str(), attrName.c_str());
			found = valueStartPos+1;
		}
	}

	if(DRM_API_SUCCESS != status)
	{
		AAMPLOG_ERR("Could not able to find %s in %s",
		keyName.c_str(), attrName.c_str());
	}

	return status;
}

/**
 * @brief API to get the PSSH Data from the manifest attribute list, getPsshData
 * @param [in] Attribute list
 * @param [out] pssData as reference 
 * @return status of the API
 */
static int getPsshData(string attrName, string &psshData){

	int status = GetFieldValue(attrName, "URI", psshData );
	if(DRM_API_SUCCESS != status){
		AAMPLOG_ERR("Could not able to get psshData from manifest"
		);
		return status;
	}
	/* Split string based on , and get the PSSH Data */
	psshData = psshData.substr(psshData.find(',')+1);

	return status;
}

/**
 * @brief API to get the DRM helper from the manifest attribute list, getDrmType
 * @param [in] Attribute list
 * 
 * @return AampDrmHelper - DRM Helper (nullptr in case of unexpected behavior)
 */
static std::shared_ptr<AampDrmHelper> getDrmHelper(string attrName , bool bPropagateUriParams, bool bDecryptClearSamplesRequired){

	string systemId = "";
	
	if(DRM_API_SUCCESS != GetFieldValue(attrName, "KEYFORMAT", systemId )){
		AAMPLOG_ERR("Could not able to receive key id from manifest"
		);
		return nullptr;
	}

	/** Remove urn:uuid: from it */
	if (systemId.find("urn:uuid:") != std::string::npos){
		systemId = systemId.substr(strlen("urn:uuid:"));
	}
	DrmInfo drmInfo;
	drmInfo.mediaFormat = eMEDIAFORMAT_HLS_MP4;
	drmInfo.systemUUID = systemId;
	drmInfo.bPropagateUriParams = bPropagateUriParams;
	drmInfo.bDecryptClearSamplesRequired = bDecryptClearSamplesRequired;
	return AampDrmHelperEngine::getInstance().createHelper(drmInfo);
}

/**
 * @brief Process content protection of track
 * @param aamp PrivateInstanceAAMP instance 
 * @param attrName attribute value
 * @return shared_ptr to the AampDrmHelper instance
 */
shared_ptr<AampDrmHelper> ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName)
{
	/* StreamAbstractionAAMP_HLS* context; */
	/* Pseudo code for ProcessContentProtection in HLS is below
	 * Get Aamp instance as aamp
	 * 1. Create AampDrmHelper object based on attribute value
	 * 2. Get pssh data from manifest (extract URI value)
	 * 3. Set PSSH data to AampDrmHelper object
	 */
	shared_ptr<AampDrmHelper> finalDrmHelper = nullptr;
	unsigned char* data = NULL;
	size_t dataLength = 0;
	int status = DRM_API_FAILED;  
	string psshDataStr = "";
	char* psshData = NULL;

	do
	{
		shared_ptr<AampDrmHelper> drmHelper = getDrmHelper(attrName, ISCONFIGSET(eAAMPConfig_PropagateURIParam), aamp->isDecryptClearSamplesRequired());
		if (nullptr == drmHelper)
		{
			AAMPLOG_ERR("Failed to get DRM type/helper from manifest!");
			break;
		}

		status  = getPsshData(attrName, psshDataStr);
		if (DRM_API_SUCCESS != status)
		{
			AAMPLOG_ERR("Failed to get PSSH Data from manifest!");
			break;
		}
		psshData = (char*) malloc(psshDataStr.length() + 1);
		memset(psshData, 0x00 , psshDataStr.length() + 1);
		strncpy(psshData, psshDataStr.c_str(), psshDataStr.length());

		if(drmHelper->friendlyName().compare("Verimatrix") == 0)
		{
			AAMPLOG_WARN( "Verimatrix DRM" );
			data = (unsigned char *)psshData;
			dataLength = psshDataStr.length();
		}
		else
		{
			data = base64_Decode(psshData, &dataLength);
			/* No more use */
			free(psshData);
			psshData = NULL;
		}

		if (dataLength == 0)
		{
			AAMPLOG_ERR("Could not able to retrive DRM data from PSSH");
			break;
		}
		if (AampLogManager::isLogLevelAllowed(eLOGLEVEL_TRACE))
		{
			AAMPLOG_TRACE("content metadata from manifest; length %zu", dataLength);
			printf("*****************************************************************\n");
			for (int i = 0; i < dataLength; i++)
			{
				printf("%c", data[i]);
			}
			printf("\n*****************************************************************\n");
			for (int i = 0; i < dataLength; i++)
			{
				printf("%02x ", data[i]);
			}
			printf("\n*****************************************************************\n");

		}
		if (!drmHelper->parsePssh(data, (uint32_t)dataLength))
		{
			AAMPLOG_ERR("Failed to get key Id from manifest");
			break;
		}
		// After processing the PSSH information, return the drmHelper
		finalDrmHelper = drmHelper;
	} while(0);

	if(data)
	{
		free(data);  //CID:128617 - Resource leak
	}
	return finalDrmHelper;
}

#else

void* ProcessContentProtection(PrivateInstanceAAMP *aamp, std::string attrName)
{
	AAMPLOG_INFO("AAMP_HLS_DRM not enabled");
	return NULL;
}
#endif /** AAMP_HLS_DRM */

/**
 * EOF
 */
