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

#include <assert.h>
#include "SocInterface.h"
#include "vendor/amlogic/AmlogicSocInterface.h"
#include "vendor/brcm/BrcmSocInterface.h"
#include "vendor/realtek/RealtekSocInterface.h"
#include "vendor/default/DefaultSocInterface.h"

/**
 * @brief Checks if the input string starts with the given prefix.
 *
 * @param inputStr The input string to check.
 * @param prefix The prefix to check for.
 *
 * @return True if the input string starts with the prefix, false otherwise.
 */
bool SocInterface::StartsWith( const char *inputStr, const char *prefix )
{
	bool rc = true;
	while( *prefix )
	{
		if( *inputStr++ != *prefix++ )
		{
			rc = false;
			break;
		}
	}
	return rc;
}

/**
 *  @brief To enable certain player configs based upon platform check
 */
SocPlatformType InferPlatformsFromPluginScan()
{
	// Ensure GST is initialized
	if (!gst_init_check(nullptr, nullptr, nullptr)) {
		MW_LOG_ERR("gst_init_check() failed");
	}
	static const std::pair<const char*, SocPlatformType> plugins[] = {
		{"amlhalasink", SOC_PLATFORM_AMLOGIC},
		{"omxeac3dec", SOC_PLATFORM_REALTEK},
		{"brcmaudiodecoder", SOC_PLATFORM_BROADCOM},
	};

	GstRegistry* registry = gst_registry_get();

	for (const auto& plugin : plugins)
	{
		GstPluginFeature* pluginFeature = gst_registry_lookup_feature(registry, plugin.first);
		if (pluginFeature)
		{
			gst_object_unref(pluginFeature);
			MW_LOG_MIL("InterfacePlayerRDK: %s plugin found in registry", plugin.first);
			return plugin.second;
		}
	}

	MW_LOG_WARN("InterfacePlayerRDK: None of the plugins found in registry");
	return SOC_PLATFORM_DEFAULT;
}

/**
 * @brief Infers SoC platform type from device.properties.
 * @return Inferred SoC platform type.
 */
SocPlatformType SocInterface::InferPlatformsFromDeviceProperties( void )
{
	SocPlatformType platform = SOC_PLATFORM_DEFAULT;
	FILE* fp = fopen("/etc/device.properties", "rb");
	if (fp)
	{
		MW_LOG_MIL("opened /etc/device.properties");
		char buf[4096];
		while( fgets(buf, sizeof(buf), fp) )
		{
			if (strncmp(buf, "SOC=", 4) == 0)
			{
				char* socName = buf + 4;  // Start after "SOC="
				for (int i = 0; socName[i] != '\0'; i++)
				{
					if (isspace(socName[i]))
					{
						socName[i] = '\0';  // Terminate at first whitespace
						break;
					}
				}
				if (*socName != '\0')  // If SOC name is not empty
				{
					MW_LOG_MIL("*** SOC %s ***", socName);
					if (strcmp(socName, "AMLOGIC") == 0)
					{
						platform = SOC_PLATFORM_AMLOGIC;
						break;
					}
					else if (strcmp(socName, "RTK") == 0)
					{
						platform = SOC_PLATFORM_REALTEK;
						break;
					}
					else if (strcmp(socName, "BRCM") == 0)
					{
						platform = SOC_PLATFORM_BROADCOM;
						break;
					}
				}
				else
				{
					MW_LOG_WARN("*** SOC not found ***");
				}
			}
		}
		fclose(fp);
	}
	else
	{
		MW_LOG_ERR("failed to open /etc/device.properties.");
	}
	return platform;
}


/**
 * @brief Creates an instance of the SoC-specific interface based on the detected platform.
 *
 * @return A pointer to the created SocInterface object, or nullptr on failure.
 */
std::shared_ptr<SocInterface> SocInterface::CreateSocInterface()
{
	SocPlatformType platformType = InferPlatformsFromDeviceProperties();

	if(platformType == SOC_PLATFORM_DEFAULT)
	{
		platformType = InferPlatformsFromPluginScan();
	}
	static std::shared_ptr<SocInterface> socInterface;
	if(socInterface)
	{
		return socInterface;
	}
	switch (platformType)
	{
		case SOC_PLATFORM_AMLOGIC:
			socInterface = std::make_shared<AmlogicSocInterface>();
			break;
		case SOC_PLATFORM_BROADCOM:
			socInterface = std::make_shared<BrcmSocInterface>();
			break;
		case SOC_PLATFORM_REALTEK:
			socInterface = std::make_shared<RealtekSocInterface>();
			break;
		default:
			socInterface = std::make_shared<DefaultSocInterface>();
			break;
	}
	if(!socInterface)
		MW_LOG_ERR("failed to create soc interface");

	return socInterface;
}

/**
 * @brief Get video PTS.
 *
 * Retrieves the current video presentation timestamp (PTS).
 *
 * @param video_sink The video sink element (unused)
 * @param video_dec The video decoder element.
 * @param isWesteros A flag for Westeros logic.
 *
 * @return Video PTS in nanoseconds, or -1 on error.
 */
long long SocInterface::GetVideoPts(GstElement *video_sink, GstElement *video_dec, bool isWesteros)
{
	gint64 currentPTS = 0;
	if(video_dec)
	{
		g_object_get(video_dec, "video-pts", &currentPTS, NULL);
		if(!isWesteros)
		{
			currentPTS *= 2;
		}
	}
	return (long long)currentPTS;
}

/**
 * @brief Set decode error on source.
 *
 * Sets a decode error flag on the given source object.
 *
 * @param src The source object.
 */
void SocInterface::SetDecodeError(GstObject* src)
{
	if(src)
	{
		g_object_set(src, "report_decode_errors", TRUE, NULL);
	}
}

/**
 * @brief Sets the state of Westeros Sink usage.
 *
 * This function updates the internal flag to indicate whether
 * Westeros Sink is being used. It does not enable or disable
 * Westeros Sink itself, but merely informs the SocInterface
 * about its status.
 *
 * @param status Set to `true` if Westeros Sink is enabled, `false` otherwise.
 */
void SocInterface::SetWesterosSinkState(bool status)
{
	mUsingWesterosSink = status;
}
/**
 *  @brief Dump a file to log
 */
void SocInterface::DumpFile(const char* fileName)
{
	int c;
	FILE *fp = fopen(fileName, "r");
	if (fp)
	{
		printf("\n************************Dump %s **************************\n", fileName);
		c = getc(fp);
		while (c != EOF)
		{
			printf("%c", c);
			c = getc(fp);
		}
		fclose(fp);
		printf("\n**********************Dump %s end *************************\n", fileName);
	}
	else
	{
		MW_LOG_WARN("Could not open %s", fileName);
	}
}

