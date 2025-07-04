/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "AampConfig.h"
#include "MockAampConfig.h"

MockAampConfig *g_mockAampConfig = nullptr;

AampConfig::AampConfig()
{
}

AampConfig& AampConfig::operator=(const AampConfig& rhs) 
{
    return *this;
}

void AampConfig::Initialize()
{
}

void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettingBool cfg , const bool &value){}
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettingInt cfg , const int &value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->SetConfigValue(cfg,value);
    }
}
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettingFloat cfg , const double &value)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->SetConfigValue(cfg,value);
    }
}
void AampConfig::SetConfigValue(ConfigPriority owner, AAMPConfigSettingString cfg , const std::string &value){}


bool AampConfig::IsConfigSet(AAMPConfigSettingBool cfg)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->IsConfigSet(cfg);
    }
    else
    {
        return false;
    }
}

bool AampConfig::GetConfigValue(AAMPConfigSettingBool cfg)
{
	bool ret = false;
	if (g_mockAampConfig != nullptr)
	{
		ret = g_mockAampConfig->GetConfigValue(cfg);
	}
	return ret;
}

int AampConfig::GetConfigValue(AAMPConfigSettingInt cfg)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg);
    }
    else
    {
        return -1;
    }
}

double AampConfig::GetConfigValue(AAMPConfigSettingFloat cfg)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg);
    }
    else
    {
        return -1.0;
    }
}

std::string AampConfig::GetConfigValue(AAMPConfigSettingString cfg)
{
    if (g_mockAampConfig != nullptr)
    {
        return g_mockAampConfig->GetConfigValue(cfg);
    }
    else
    {
        return "";
    }
}

void AampConfig::ApplyDeviceCapabilities()
{
}

bool AampConfig::ReadAampCfgJsonFile()
{
    return false;
}

bool AampConfig::ReadAampCfgTxtFile()
{
    return false;
}

void AampConfig::ReadAampCfgFromEnv()
{
}

bool AampConfig::ProcessBase64AampCfg(const char * base64Config, size_t configLen,ConfigPriority cfgPriority)
{
	return false;
}

void AampConfig::ReadOperatorConfiguration()
{
}

void AampConfig::ShowOperatorSetConfiguration()
{
}

void AampConfig::ShowDevCfgConfiguration()
{
}

void AampConfig::RestoreConfiguration(ConfigPriority owner)
{
}

bool AampConfig::ProcessConfigJson(const cJSON *cfgdata, ConfigPriority owner )
{
    return false;
}

void AampConfig::DoCustomSetting(ConfigPriority owner)
{
}

ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingBool cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->GetConfigOwner(cfg);
	}
	else
	{
		return AAMP_DEFAULT_SETTING;
	}
}
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingInt cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->GetConfigOwner(cfg);
	}
	else
	{
		return AAMP_DEFAULT_SETTING;
	}
}
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingFloat cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->GetConfigOwner(cfg);
	}
	else
	{
		return AAMP_DEFAULT_SETTING;
	}
}
ConfigPriority AampConfig::GetConfigOwner(AAMPConfigSettingString cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->GetConfigOwner(cfg);
	}
	else
	{
		return AAMP_DEFAULT_SETTING;
	}
}

bool AampConfig::GetAampConfigJSONStr(std::string &str)
{
    return false;
}

std::string AampConfig::GetUserAgentString()
{
	return "";
}

const char * AampConfig::GetChannelOverride(const std::string manifestUrl)
{
    return nullptr;
}

const char * AampConfig::GetChannelLicenseOverride(const std::string manifestUrl)
{
    return nullptr;
}

bool AampConfig::CustomSearch( std::string url, int playerId , std::string appname)
{
    return false;
}

void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingBool cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->RestoreConfiguration(owner,cfg);
	}
}

void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingInt cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->RestoreConfiguration(owner,cfg);
	}
}

void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingFloat cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->RestoreConfiguration(owner,cfg);
	}
}

void AampConfig::RestoreConfiguration(ConfigPriority owner, AAMPConfigSettingString cfg)
{
	if (g_mockAampConfig != nullptr)
	{
		return g_mockAampConfig->RestoreConfiguration(owner,cfg);
	}
}
