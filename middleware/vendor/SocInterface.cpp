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
#include <mutex>
#include "SocInterface.h"
#include "vendor/amlogic/AmlogicSocInterface.h"
#include "vendor/brcm/BrcmSocInterface.h"
#include "vendor/realtek/RealtekSocInterface.h"
#include "vendor/default/DefaultSocInterface.h"
// Private singleton storage
static std::shared_ptr<SocInterface> g_socInterface;
static std::mutex g_socMutex;

/**Initially re-sets the IsRialtoMode */
bool SocInterface::mIsRialtoMode = false;
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
SocPlatformType InferPlatformFromPluginScan()
{
	MW_LOG_MIL("Performing InterfacePluginScan");
	SocPlatformType platform = SOC_PLATFORM_DEFAULT;
	const char *savedScanner = getenv("GST_PLUGIN_SCANNER");
	setenv("GST_PLUGIN_SCANNER", "", 1);
	// Ensure GST is initialized
	if (!gst_init_check(nullptr, nullptr, nullptr)) {
		MW_LOG_ERR("gst_init_check() failed");
	}
	// Restore original env var so we do not affect the rest of the process
	if (savedScanner) { setenv("GST_PLUGIN_SCANNER", savedScanner, 1); }
	else              { unsetenv("GST_PLUGIN_SCANNER"); }
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
			platform = plugin.second;
			break;
		}
	}
	
	if( platform == SOC_PLATFORM_DEFAULT )
	{
		MW_LOG_WARN("InterfacePlayerRDK: None of the plugins found in registry");
	}
	return platform;
}

/**
 * @brief Infers SoC platform type from device.properties.
 * @return Inferred SoC platform type.
 */
SocPlatformType SocInterface::InferPlatformFromDeviceProperties( void )
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
 * @brief Helper to create the right subclass based on platform type.
 */
static std::shared_ptr<SocInterface> CreateForPlatform(SocPlatformType platformType)
{
	switch (platformType)
	{
		case SOC_PLATFORM_AMLOGIC:
			MW_LOG_MIL("Setting up SoC Interface for AMLOGIC");
			return std::make_shared<AmlogicSocInterface>();
		case SOC_PLATFORM_BROADCOM:
			MW_LOG_MIL("Setting up SoC Interface for BROADCOM");
			return std::make_shared<BrcmSocInterface>();
		case SOC_PLATFORM_REALTEK:
			MW_LOG_MIL("Setting up SoC Interface for REALTEK");
			return std::make_shared<RealtekSocInterface>();
		default:
			MW_LOG_MIL("Setting up SoC Interface for Default");
			return std::make_shared<DefaultSocInterface>();
	}
}


/**
 * @brief Loads the instance with rialto mode or not
 *
 * @return A pointer to the created SocInterface object, or nullptr on failure.
 */
std::shared_ptr<SocInterface> SocInterface::CreateSocInterface(bool isRialto)
{
	if(isRialto == true)
	{
	    MW_LOG_MIL("Rialto is enabled and creating default soc");
	}
	mIsRialtoMode = isRialto;
	return CreateSocInterface();
}

/**
 * @brief Phase 1: Creates an instance of the SoC-specific interface.
 *        Safe to call during dl_init — only reads /etc/device.properties, NO GStreamer calls.
 *
 * @return A pointer to the created SocInterface object.
 */
std::shared_ptr<SocInterface> SocInterface::CreateSocInterface()
{
	std::lock_guard<std::mutex> lock(g_socMutex);
	if (!g_socInterface)
	{
		// Only use device.properties at this stage — no GStreamer calls
		SocPlatformType platformType = InferPlatformFromDeviceProperties();
		g_socInterface = CreateForPlatform(platformType);
	}
	return g_socInterface;
}

/**
 * @brief Phase 2: Called after GStreamer is safely initialized to re-detect platform via plugin scan.
 *        Must NOT be called during dl_init / library constructor.
 *        Contains gst_init_check (via InferPlatformFromPluginScan).
 */
void SocInterface::InitializePlatformFromPlugins()
{
	std::lock_guard<std::mutex> lock(g_socMutex);
	if (mIsRialtoMode) return;

	// If device.properties already identified a specific platform, no need to scan plugins
	SocPlatformType currentType = InferPlatformFromDeviceProperties();
	if (currentType != SOC_PLATFORM_DEFAULT)
	{
		MW_LOG_MIL("Platform already identified from device.properties, skipping plugin scan");
		return;
	}

	// This calls gst_init_check internally — safe here because we are NOT in dl_init
	SocPlatformType platformType = InferPlatformFromPluginScan();
	if (platformType != SOC_PLATFORM_DEFAULT)
	{
		MW_LOG_MIL("Plugin scan detected platform, replacing SoC interface");
		g_socInterface = CreateForPlatform(platformType);
	}
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
 * @brief Configure Capability Acceptance for GStreamer Transform
 *
 * Sets up the accept_caps function pointer for a GStreamer base transform class.
 * This allows the transform element to decide whether it can accept a given set of capabilities (caps),
 * which is essential for negotiating media formats during pipeline setup.
 *
 * @param base_transform_class Pointer to the GStreamer base transform class to configure.
 * @param accept_caps_func Function used to determine if the transform accepts specific caps.
 */
void SocInterface::ConfigureAcceptCaps(GstBaseTransformClass* base_transform_class ,
													AcceptCapsFunc accept_caps_func) {
    if (accept_caps_func) {
        base_transform_class->accept_caps = GST_DEBUG_FUNCPTR(accept_caps_func);
    }
}
