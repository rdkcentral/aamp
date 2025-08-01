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

/**
 *  @file PlayerCCManager.cpp
 *
 *  @brief Base class for ClosedCaption integration layer
 *
 */

#include "PlayerLogManager.h" // Included for MW_LOG
//TODO: Fix cyclic dependency btw GlobalConfig and PlayerLogManager

#include "PlayerJsonObject.h" // For JSON parsing
#include "PlayerUtils.h" // For player_StartsWith

#include "PlayerCCManager.h"
#include "PlayerSubtecCCManager.h"


#define CHAR_CODE_1 49
#define CHAR_CODE_6 54

int IsCCOnFlag = 0;

#if defined(SUBTITLE_SUPPORTED)
static mrcc_Error GetCapability(gsw_CcAttribType attribType, gsw_CcType ccType, void **values, unsigned int *size)
{
        return subtecConnector::ccMgrAPI::ccGetCapability(attribType, ccType, values, size);
}

static mrcc_Error GetAttributes(gsw_CcAttributes * attrib, gsw_CcType ccType)
{
        return subtecConnector::ccMgrAPI::ccGetAttributes(attrib, ccType);
}

static mrcc_Error SetAttributes(gsw_CcAttributes * attrib, short type, gsw_CcType ccType)
{
        return subtecConnector::ccMgrAPI::ccSetAttributes(attrib, type, ccType);
}
#else
// stubs for (simulator & RPI) builds without inband ccDataReader
static mrcc_Error GetCapability(gsw_CcAttribType attribType, gsw_CcType ccType, void **values, unsigned int *size)
{
        return 0;
}

static mrcc_Error GetAttributes(gsw_CcAttributes * attrib, gsw_CcType ccType)
{
        return 0;
}

static mrcc_Error SetAttributes(gsw_CcAttributes * attrib, short type, gsw_CcType ccType)
{
        return 0;
}
#endif
/**
 * @brief Get color option from input string
 *
 * @param[in] attributeIndex - CC attribute
 * @param[in] ccType - CC type
 * @param[in] input - input color style
 * @param[out] colorOut - color option for the input value
 * @return int - 0 for success, -1 for failure
 */
static int getColor(gsw_CcAttribType attributeIndex, gsw_CcType ccType, std::string input, gsw_CcColor *colorOut)
{
	static gsw_CcColor* ccColorCaps[GSW_CC_COLOR_MAX];
	static bool flagForMalloc = false;
	unsigned int ccSizeOfCaps = 0;
	MW_LOG_TRACE("input: %s", input.c_str());

	if (!input.empty() && colorOut)
	{
		if (!flagForMalloc)
		{
			flagForMalloc = true;
			for (int i = 0; i < GSW_CC_COLOR_MAX; i++)
			{
				ccColorCaps[i] = (gsw_CcColor*) malloc(sizeof(gsw_CcColor));
				memset(ccColorCaps[i], 0, sizeof(gsw_CcColor));
			}
		}
		GetCapability(attributeIndex, ccType, (void**) &ccColorCaps, &ccSizeOfCaps);

		bool found = false;
		const char *inputStr = input.c_str();
		for (unsigned int i = 0; i < ccSizeOfCaps; i++)
		{
			MW_LOG_TRACE("color caps: %s", ccColorCaps[i]->name);
			if (0 == strncasecmp(ccColorCaps[i]->name, inputStr, GSW_MAX_CC_COLOR_NAME_LENGTH))
			{
				MW_LOG_TRACE("found match %s", ccColorCaps[i]->name);
				memcpy(colorOut, ccColorCaps[i], sizeof (gsw_CcColor));
				found = true;
				break;
			}
		}
		if(!found)
		{
			MW_LOG_ERR("Unsupported color type %s", inputStr);
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL!");
		return -1;
	}
	return 0;
}

/**
 * @brief Get text style value from input string
 *
 * @param[in] input - input text style value
 * @param[out] fontSizeOut - text style option for the input value
 * @return int - 0 for success, -1 for failure
 */
static int getTextStyle(std::string input, gsw_CcTextStyle *textStyleOut)
{
	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && textStyleOut)
	{
		const char *inputStr = input.c_str();
		if (0 == strncasecmp(inputStr, "false", strlen("false")))
		{
			*textStyleOut = GSW_CC_TEXT_STYLE_FALSE;
		}
		else if (0 == strncasecmp(inputStr, "true", strlen("true")))
		{
			*textStyleOut = GSW_CC_TEXT_STYLE_TRUE;
		}
		else if (0 == strncasecmp(inputStr, "auto", strlen("auto")))
		{
			*textStyleOut = GSW_CC_TEXT_STYLE_EMBEDDED_TEXT;
		}
		else
		{
			MW_LOG_ERR("Unsupported text style  %s", inputStr);
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		return -1;
	}
	return 0;
}

/**
 * @brief Get edge type value from input string
 *
 * @param[in] input - input edge type value
 * @param[out] fontSizeOut - edge type option for the input value
 * @return int - 0 for success, -1 for failure
 */
static int getEdgeType(std::string input, gsw_CcEdgeType *edgeTypeOut)
{
	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && edgeTypeOut)
	{
		const char *inputStr = input.c_str();
		if (0 == strncasecmp(inputStr, "none", strlen("none")))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_NONE;
		}
		else if (0 == strncasecmp(inputStr, "raised", strlen("raised")))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_RAISED;
		}
		else if (0 == strncasecmp(inputStr, "depressed", strlen("depressed")))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_DEPRESSED;
		}
		else if (0 == strncasecmp(inputStr, "uniform", strlen("uniform")))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_UNIFORM;
		}
		else if ((0 == strncasecmp(inputStr, "drop_shadow_left", strlen("drop_shadow_left"))) ||
			(0 == strncasecmp(inputStr, "Left drop shadow", strlen("Left drop shadow"))))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_SHADOW_LEFT;
		}
		else if ((0 == strncasecmp(inputStr, "drop_shadow_right", strlen("drop_shadow_right"))) ||
			(0 == strncasecmp(inputStr, "Right drop shadow", strlen("Right drop shadow"))))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_SHADOW_RIGHT;
		}
		else if (0 == strncasecmp(inputStr, "auto", strlen("auto")))
		{
			*edgeTypeOut = GSW_CC_EDGE_TYPE_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported edge type %s", inputStr);
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		return -1;
	}
	return 0;
}

/**
 * @brief Get font style value from input string
 *
 * @param[in] input - input font style value
 * @param[out] fontSizeOut - font style option for the input value
 * @return int - 0 for success, -1 for failure
 */
static int getFontStyle(std::string input, gsw_CcFontStyle *fontStyleOut)
{
	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && fontStyleOut)
	{
		const char *inputStr = input.c_str();
		if (0 == strncasecmp(inputStr, "default", strlen("default")))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_DEFAULT, sizeof(GSW_CC_FONT_STYLE_DEFAULT));
		}
		else if ((0 == strncasecmp(inputStr, "monospaced_serif", strlen("monospaced_serif"))) ||
			(0 == strncasecmp(inputStr, "Monospaced serif", strlen("Monospaced serif"))))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_MONOSPACED_SERIF, sizeof(GSW_CC_FONT_STYLE_MONOSPACED_SERIF));
		}
		else if ((0 == strncasecmp(inputStr, "proportional_serif", strlen("proportional_serif"))) ||
			(0 == strncasecmp(inputStr, "Proportional serif", strlen("Proportional serif"))))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_PROPORTIONAL_SERIF, sizeof(GSW_CC_FONT_STYLE_PROPORTIONAL_SERIF));
		}
		else if ((0 == strncasecmp(inputStr, "monospaced_sanserif", strlen("monospaced_sanserif"))) ||
			(0 == strncasecmp(inputStr, "Monospaced sans serif", strlen("Monospaced sans serif"))))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_MONOSPACED_SANSSERIF, sizeof(GSW_CC_FONT_STYLE_MONOSPACED_SANSSERIF));
		}
		else if ((0 == strncasecmp(inputStr, "proportional_sanserif", strlen("proportional_sanserif"))) ||
			(0 == strncasecmp(inputStr, "Proportional sans serif", strlen("Proportional sans serif"))))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_PROPORTIONAL_SANSSERIF, sizeof(GSW_CC_FONT_STYLE_PROPORTIONAL_SANSSERIF));
		}
		else if (0 == strncasecmp(inputStr, "casual", strlen("casual")))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_CASUAL, sizeof(GSW_CC_FONT_STYLE_CASUAL));
		}
		else if (0 == strncasecmp(inputStr, "cursive", strlen("cursive")))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_CURSIVE, sizeof(GSW_CC_FONT_STYLE_CURSIVE));
		}
		else if ((0 == strncasecmp(inputStr, "smallcaps", strlen("smallcaps"))) ||
			(0 == strncasecmp(inputStr, "small capital", strlen("small capital"))))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_SMALL_CAPITALS, sizeof(GSW_CC_FONT_STYLE_SMALL_CAPITALS));
		}
		else if (0 == strncasecmp(inputStr, "auto", strlen("auto")))
		{
			memcpy(fontStyleOut, GSW_CC_FONT_STYLE_EMBEDDED, sizeof(GSW_CC_FONT_STYLE_EMBEDDED));
		}
		else
		{
			MW_LOG_ERR("Unsupported font style type %s", inputStr);
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		return -1;
	}
	return 0;
}

/**
 * @brief Get font size value from input string
 *
 * @param[in] input - input font size value
 * @param[out] fontSizeOut - font size option for the input value
 * @return int - 0 for success, -1 for failure
 */
static int getFontSize(std::string input, gsw_CcFontSize *fontSizeOut)
{
	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && fontSizeOut)
	{
		const char *inputStr = input.c_str();
		if (0 == strncasecmp(inputStr, "small", strlen("small")))
		{
			*fontSizeOut = GSW_CC_FONT_SIZE_SMALL;
		}
		else if ((0 == strncasecmp(inputStr, "standard", strlen("standard"))) ||
			(0 == strncasecmp(inputStr, "medium", strlen("medium"))))
		{
			*fontSizeOut = GSW_CC_FONT_SIZE_STANDARD;
		}
		else if (0 == strncasecmp(inputStr, "large", strlen("large")))
		{
			*fontSizeOut = GSW_CC_FONT_SIZE_LARGE;
		}
		else if (0 == strncasecmp(inputStr, "extra_large", strlen("extra_large")))
		{
			*fontSizeOut = GSW_CC_FONT_SIZE_EXTRALARGE;
		}
		else if (0 == strncasecmp(inputStr, "auto", strlen("auto")))
		{
			*fontSizeOut = GSW_CC_FONT_SIZE_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported font size type %s", inputStr);
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		return -1;
	}
	return 0;
}

/**
 * @brief Get opacity value from input string
 *
 * @param[in] input - input opacity style
 * @param[out] opacityOut - opacity option for the input value
 * @return int - 0 for success, -1 for failure
 */
static int getOpacity(std::string input, gsw_CcOpacity *opacityOut)
{
	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && opacityOut)
	{
		const char *inputStr = input.c_str();
		if (0 == strncasecmp(inputStr, "solid", strlen("solid")))
		{
			*opacityOut = GSW_CC_OPACITY_SOLID;
		}
		else if (0 == strncasecmp(inputStr, "flash", strlen("flash")))
		{
			*opacityOut = GSW_CC_OPACITY_FLASHING;
		}
		else if (0 == strncasecmp(inputStr, "translucent", strlen("translucent")))
		{
			*opacityOut = GSW_CC_OPACITY_TRANSLUCENT;
		}
		else if (0 == strncasecmp(inputStr, "transparent", strlen("transparent")))
		{
			*opacityOut = GSW_CC_OPACITY_TRANSPARENT;
		}
		else if (0 == strncasecmp(inputStr, "auto", strlen("auto")))
		{
			*opacityOut = GSW_CC_OPACITY_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported opacity type %s", inputStr);
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		return -1;
	}
	return 0;
}


/**
 *  @brief Set CC styles for rendering
 */
int PlayerCCManagerBase::SetStyle(const std::string &options)
try
{
	EnsureRendererCommsInitialized();
	MW_LOG_WARN("PlayerCCManagerBase::");

	int ret = -1;
	/*
	 * Values received from JS PP:
	 *
	 * export interface IPlayerCCStyle {
	 *	fontStyle?: string;
	 *	textEdgeColor?: string;
	 *	textEdgeStyle?: string;
	 *	textForegroundColor?: string;
	 *	textForegroundOpacity?: string;
	 *	textBackgroundColor?: string;
	 *	textBackgroundOpacity?: string;
	 *	penItalicized?: string;
	 *	penSize?: string;
	 *	penUnderline?: string;
	 *	windowBorderEdgeColor?: string;
	 *	windowBorderEdgeStyle?: string;
	 *	windowFillColor?: string;
	 *	windowFillOpacity?: string;
	 *	bottomInset?: string;
	 *	}
	 *      FYI - bottomInset unsupported in RDK CC module
	 */

	if(mEnabled)
	{
		if (!options.empty())
		{
			PlayerJsonObject inputOptions(options);

			std::string optionValue;
			ret = 0;
			mOptions = options;

			gsw_CcAttributes attribute;
			/** Get the current attributes */
			GetAttributes (&attribute, GSW_CC_TYPE_DIGITAL);

			short attribsMask = 0;

			if (inputOptions.get("fontStyle", optionValue))
			{
				getFontStyle(optionValue, &(attribute.fontStyle));
				attribsMask |=GSW_CC_ATTRIB_FONT_STYLE;
			}

			if (inputOptions.get("textEdgeColor", optionValue))
			{
				getColor(GSW_CC_ATTRIB_EDGE_COLOR, GSW_CC_TYPE_DIGITAL, optionValue, &(attribute.edgeColor));
				attribsMask |=GSW_CC_ATTRIB_EDGE_COLOR;
			}

			if (inputOptions.get("textEdgeStyle", optionValue))
			{
				getEdgeType(optionValue, &(attribute.edgeType));
				attribsMask |=GSW_CC_ATTRIB_EDGE_TYPE;
			}

			if (inputOptions.get("textForegroundColor", optionValue))
			{
				getColor(GSW_CC_ATTRIB_FONT_COLOR, GSW_CC_TYPE_DIGITAL, optionValue, &(attribute.charFgColor));
				attribsMask |=GSW_CC_ATTRIB_FONT_COLOR;
			}

			if (inputOptions.get("textForegroundOpacity", optionValue))
			{
				getOpacity(optionValue, &(attribute.charFgOpacity));
				attribsMask |=GSW_CC_ATTRIB_FONT_OPACITY;
			}

			if (inputOptions.get("textBackgroundColor", optionValue))
			{
				getColor(GSW_CC_ATTRIB_BACKGROUND_COLOR, GSW_CC_TYPE_DIGITAL, optionValue, &(attribute.charBgColor));
				attribsMask |=GSW_CC_ATTRIB_BACKGROUND_COLOR;
			}

			if (inputOptions.get("textBackgroundOpacity", optionValue))
			{
				getOpacity(optionValue, &(attribute.charBgOpacity));
				attribsMask |=GSW_CC_ATTRIB_BACKGROUND_OPACITY;
			}

			if (inputOptions.get("penItalicized", optionValue))
			{
				getTextStyle(optionValue, &(attribute.fontItalic));
				attribsMask |=GSW_CC_ATTRIB_FONT_ITALIC;
			}

			if (inputOptions.get("penSize", optionValue))
			{
				getFontSize(optionValue, &(attribute.fontSize));
				attribsMask |=GSW_CC_ATTRIB_FONT_SIZE;
			}

			if (inputOptions.get("penUnderline", optionValue))
			{
				getTextStyle(optionValue, &(attribute.fontUnderline));
				attribsMask |=GSW_CC_ATTRIB_FONT_UNDERLINE;
			}

			if (inputOptions.get("windowBorderEdgeColor", optionValue))
			{
				getColor(GSW_CC_ATTRIB_BORDER_COLOR, GSW_CC_TYPE_DIGITAL, optionValue, &(attribute.borderColor));
				attribsMask |=GSW_CC_ATTRIB_BORDER_COLOR;
			}

			if (inputOptions.get("windowBorderEdgeStyle", optionValue))
			{
				getEdgeType(optionValue, (gsw_CcEdgeType*) &(attribute.borderType));
				attribsMask |=GSW_CC_ATTRIB_BORDER_TYPE;
			}

			if (inputOptions.get("windowFillColor", optionValue))
			{
				getColor(GSW_CC_ATTRIB_WIN_COLOR, GSW_CC_TYPE_DIGITAL, optionValue, &(attribute.winColor));
				attribsMask |=GSW_CC_ATTRIB_WIN_COLOR;
			}

			if (inputOptions.get("windowFillOpacity", optionValue))
			{
				getOpacity(optionValue, &(attribute.winOpacity));
				attribsMask |=GSW_CC_ATTRIB_WIN_OPACITY;
			}

			if(attribsMask != 0)
			{

				SetAttributes(&attribute, attribsMask, GSW_CC_TYPE_DIGITAL);
				SetAttributes(&attribute, attribsMask, GSW_CC_TYPE_ANALOG);
			}
			else
			{
				MW_LOG_WARN("PlayerCCManagerBase::received optionsJson but result attributeMask is 0");
			}

		}
	}
	else
	{
		MW_LOG_WARN("PlayerCCManagerBase::CC rendering not enabled");
	}
	return ret;
}
catch(const PlayerJsonParseException& e)
{
	MW_LOG_ERR("PlayerCCManagerBase: PlayerJsonParseException - %s", e.what());
	return -1;
}

/**
 *  @brief To stop CC rendering
 */
void PlayerCCManagerBase::Stop()
{
	EnsureRendererCommsInitialized();
	MW_LOG_WARN("PlayerCCManagerBase::mEnabled=%d",mEnabled);
	StopRendering();
}

/**
 *  @brief To start CC rendering
 */
void PlayerCCManagerBase::Start()
{
	EnsureInitialized();
	MW_LOG_WARN("PlayerCCManagerBase:: mEnabled=%d",  mEnabled);
	StartRendering();
}

/**
 *  @brief Initialize CC resource.
 */
int PlayerCCManagerBase::Init(void *handle)
{
	if (handle == NULL)
	{
		MW_LOG_WARN("PlayerCCManagerBase:: NULL handle");
		return -1;
	}

	if (Initialize(handle) != 0)
	{
		MW_LOG_WARN("PlayerCCManagerBase::Initialize failure");
		return -1;
	}

	MW_LOG_WARN("PlayerCCManagerBase:: Start CC with video dec handle: %p and mEnabled: %d", handle, mEnabled);

	if (mEnabled)
	{
		Start();
	}
	else
	{
		Stop();
	}

	return 0;
}

/**
 *  @brief To enable/disable CC when trickplay starts/ends
 */
void PlayerCCManagerBase::SetTrickplayStatus(bool on)
{
	MW_LOG_WARN("PlayerCCManagerBase::trickplay status(%d)", on);
	if (on)
	{
		// When trickplay starts, stop CC rendering
		Stop();
	}
	else if (mEnabled)
	{
		// When trickplay ends and CC rendering enabled by app
		Start();
	}
	mTrickplayStarted = on;
}

/**
 *  @brief To enable/disable CC when parental control locked/unlocked
 */
void PlayerCCManagerBase::SetParentalControlStatus(bool locked)
{
	MW_LOG_WARN("PlayerCCManagerBase:: lock status(%s)", (locked)?"true":"false");
	if (locked)
	{
		// When parental control locked, stop CC rendering
		Stop();
	}
	else
	{
		if (mEnabled)
		{
			// When parental control unlocked, start  CC rendering if already enabled by app
			Start();
		}
	}
	mParentalCtrlLocked = locked;
}

/**
 *  @brief Set CC track
 */
int PlayerCCManagerBase::SetTrack(const std::string &track, const CCFormat format)
{
	EnsureRendererCommsInitialized();
	int ret = -1;
	unsigned int trackNum = 0;
	CCFormat finalFormat = eCLOSEDCAPTION_FORMAT_DEFAULT;
	mTrack = track;

	// Check if track is CCx or SERVICEx or track number
	// Could be from 1 -> 63
	if (!track.empty() && track[0] >= CHAR_CODE_1 && track[0] <= CHAR_CODE_6)
	{
		trackNum = (unsigned int) std::stoul(track);
		// This is slightly confusing as we don't know if its 608/708
		// more info might be available in format argument
	}
	else if (track.size() > 2 && player_StartsWith(track.c_str(), "CC"))
	{
		// Value between 1 - 4
		// Set as analog channel
		finalFormat = eCLOSEDCAPTION_FORMAT_608;
		trackNum = ((int)track[2] - 48);
	}
	else if (track.size() > 3 && player_StartsWith(track.c_str(), "TXT"))
	{
		// Value between 5 - 8
		// Set as analog channel
		finalFormat = eCLOSEDCAPTION_FORMAT_608;
		trackNum = ((int)track[3] - 48) + 4;
	}
	else if (track.size() > 4 && player_StartsWith(track.c_str(), "TEXT"))
	{
		// Value between 5 - 8
		// Set as analog channel
		finalFormat = eCLOSEDCAPTION_FORMAT_608;
		trackNum = ((int)track[4] - 48) + 4;
	}
	else if (track.size() > 7 && player_StartsWith(track.c_str(), "SERVICE"))
	{
		// Value between 1 - 63
		// Set as digital channel
		finalFormat = eCLOSEDCAPTION_FORMAT_708;
		trackNum = (unsigned int) std::stoul(track.substr(7));
	}

	// Format argument overrides whatever we infer from track
	if (format != eCLOSEDCAPTION_FORMAT_DEFAULT && finalFormat != format)
	{
		if (format == eCLOSEDCAPTION_FORMAT_608)
		{
			//Force to 608
			finalFormat = eCLOSEDCAPTION_FORMAT_608;
			if (trackNum > 8)
			{
				//Hope this will not happen in realtime
				trackNum = 0;
			}
			MW_LOG_WARN("PlayerCCManagerBase:: Force CC track format to 608 and trackNum to %d!", trackNum);
		}
		else
		{
			//Force to 708
			finalFormat = eCLOSEDCAPTION_FORMAT_708;
			MW_LOG_WARN("PlayerCCManagerBase::Force CC track format to 708!");
		}
	}

	MW_LOG_WARN("PlayerCCManagerBase::Set CC InstreamId '%s' format(%d) trackNum(%d)", track.c_str(), finalFormat, trackNum);

	if (finalFormat == eCLOSEDCAPTION_FORMAT_708 && (trackNum > 0 && trackNum <= 63))
	{
		ret = SetDigitalChannel(trackNum);
	}
	else if (finalFormat == eCLOSEDCAPTION_FORMAT_608 && (trackNum > 0 && trackNum <= 8))
	{
		int analogChannel = GSW_CC_ANALOG_SERVICE_CC1 + (trackNum - 1);
		ret = SetAnalogChannel(analogChannel);
	}
	else
	{
		MW_LOG_WARN("PlayerCCManagerBase::Invalid track number or format, ignoring it!");
	}

	if(0 != ret)
	{
		MW_LOG_WARN("PlayerCCManagerBase::Failed to set trackNum(%d) and format(%d) with ret - %d", trackNum, finalFormat, ret);
	}
	return ret;
}

/**
 *  @brief To restore cc state after new tune
 */
void PlayerCCManagerBase::RestoreCC()
{
	MW_LOG_WARN("PlayerCCManagerBase::mEnabled: %d, mTrickplayStarted: %d, mParentalCtrlLocked: %d, mCCHandle: %s",
			mEnabled, mTrickplayStarted, mParentalCtrlLocked, (CheckCCHandle()) ? "set" : "not set");

	std::string trackId = GetTrack();

	const auto& textTracks = getLastTextTracks();
	bool matchFound = false;

	if(!textTracks.empty())
	{
		for(const auto& track : textTracks)
		{
			if(trackId == track.instreamId)
			{
				MW_LOG_WARN("PlayerCCManagerBase::matching id found in available tracks");
				matchFound = true;
				break;
			}
		}

		if(!matchFound)
		{
			std::string defaultTrack = textTracks.front().instreamId;
			trackId = defaultTrack.empty() ? "CC1" : defaultTrack;
			MW_LOG_WARN("PlayerCCManagerBase::matching id not found, selecting %s as default", trackId.c_str());
		}
	}
	else
	{
		MW_LOG_WARN("PlayerCCManagerBase::tracklist empty selecting CC1 as default");
		trackId = "CC1";
	}

	SetTrack(trackId);
}

/**
 *  @brief Enable/disable CC rendering
 */
int PlayerCCManagerBase::SetStatus(bool enable)
{
	int ret = 0;
	mEnabled = enable;
	MW_LOG_WARN("PlayerCCManagerBase::mEnabled: %d, mTrickplayStarted: %d, mParentalCtrlLocked: %d, mCCHandle: %s",
			mEnabled, mTrickplayStarted, mParentalCtrlLocked, (CheckCCHandle()) ? "set" : "not set");
	if (mEnabled)
		IsCCOnFlag = 1;
	else
		IsCCOnFlag = 0;

	if (!mTrickplayStarted && !mParentalCtrlLocked && CheckCCHandle())
	{
		// Setting CC rendering to true before media_closeCaptionStart is not honoured
		// by CC module. CC rendering status is saved in mEnabled and Start/Stop is
		// called when the required operations are completed
		if (mEnabled)
		{
			Start();
		}
		else
		{
			Stop();
		}
	}
	return ret;
}

/**
 * @brief To check whether Out of Band Closed caption/subtitle rendering supported or not. 
 */
bool PlayerCCManagerBase::IsOOBCCRenderingSupported()
{
	MW_LOG_TRACE("Subtec CC Mode Support: ON");
	return true;
}

/**
 * @brief Singleton instance
 */
PlayerCCManagerBase *PlayerCCManager::mInstance = NULL;

/**
 *  @brief Get the singleton instance
 */
PlayerCCManagerBase *PlayerCCManager::GetInstance()
{
	if (mInstance == NULL)
	{
#if defined(SUBTITLE_SUPPORTED)
		mInstance = new PlayerSubtecCCManager();
#else
		MW_LOG_WARN("No subtec support on simulators. Creating a dummy instance!");
		mInstance = new PlayerFakeCCManager();
#endif
	}
	return mInstance;
}

/**
 *  @brief Destroy instance
 */
void PlayerCCManager::DestroyInstance()
{
	if (mInstance)
	{
		delete mInstance;
		mInstance = NULL;
	}
}

