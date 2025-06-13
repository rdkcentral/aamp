/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2015 RDK Management
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
 * @file PlayerRfc.cpp
 * @brief APIs to get RFC configured data
 */

#include <string>
#include <cstdio>
#include <cstring>
#ifdef PLAYER_RFC_ENABLED
#include "tr181api.h"
#endif
#include "PlayerRfc.h"
#include "PlayerExternalUtils.h"

#include "PlayerLogManager.h"

namespace RFCSettings
{
	/**
	* @brief   Fetch data from RFC
	* @param   CallerId and Parameter to be fetched
	* @retval  std::string host value
	*/
	std::string readRFCValue(const std::string& parameter,const char* playerName){
		std::string strhost="";
		#ifdef PLAYER_RFC_ENABLED
		if(!IsContainerEnvironment())
		{
            TR181_ParamData_t param = {0};

			//getParam() wrongly char* not const char*; cast to avoid compilation error
			tr181ErrorCode_t status = getParam((char *)playerName, parameter.c_str(), &param);

			if (tr181Success == status)
			{
				MW_LOG_INFO("RFC Parameter for %s is %s type = %d", parameter.c_str(), param.value, param.type);
				strhost = std::string(param.value);
			}
			else if (tr181ValueIsEmpty == status)
			{
				// NO RFC is set , which is success case
				MW_LOG_TRACE("RFC Parameter : %s is not set", parameter.c_str());
			}
			else
			{
				MW_LOG_ERR("get RFC Parameter for %s Failed : %s type = %d", parameter.c_str(), getTR181ErrorString(status), param.type);
			}
		}
		#endif
		return strhost;
	}
}
/**
 * EOF
 */
