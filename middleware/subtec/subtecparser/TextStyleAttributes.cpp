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
 * @file TextStyleAttributes.cpp
 *
 * @brief This file provides implementation of class methods related to subtitle text attributes
 *
 */

#include <assert.h>
#include <cctype>
#include <algorithm>
#include <string>
#include "TextStyleAttributes.h"
#include "PlayerJsonObject.h"                     // For JSON parsing


TextStyleAttributes::TextStyleAttributes()
{
}

/**
 * @brief Get font size value from input string
 *
 * @param[in] input - input font size value
 * @param[out] fontSizeOut - font size option for the input value
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getFontSize(std::string input, FontSize *fontSizeOut)
{
	int retVal = 0;

	if (!input.empty() && fontSizeOut)
	{
		transform(input.begin(), input.end(), input.begin(), ::tolower);	/* Makes sure that string is in lower case before comparison */

		if (input == "small")
		{
			*fontSizeOut = FONT_SIZE_SMALL;
		}
		else if ((input == "standard") || (input =="medium"))
		{
			*fontSizeOut = FONT_SIZE_STANDARD;
		}
		else if (input == "large")
		{
			*fontSizeOut = FONT_SIZE_LARGE;
		}
		else if (input == "extra_large")
		{
			*fontSizeOut = FONT_SIZE_EXTRALARGE;
		}
		else if (input == "auto")
		{
			*fontSizeOut = FONT_SIZE_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported font size type %s", input.c_str());
			retVal = -1;
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		retVal = -1;
	}
	return retVal;
}

/**
 * @brief Get font style value from input string
 *
 * @param[in] input - input font style value
 * @param[out] fontSizeOut - font style option for the input value
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getFontStyle(std::string input, FontStyle *fontStyleOut)
{
	int retVal = 0;

	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && fontStyleOut)
        {
		transform(input.begin(), input.end(), input.begin(), ::tolower);	/* Makes sure that string is in lower case before comparison */
		if (input == "default")
		{
			*fontStyleOut = FONT_STYLE_DEFAULT;
		}
		else if ((input == "monospaced_serif") || (input == "monospaced serif"))
		{
			*fontStyleOut = FONT_STYLE_MONOSPACED_SERIF;
		}
		else if ((input == "proportional_serif") || (input == "proportional serif"))
		{
			*fontStyleOut = FONT_STYLE_PROPORTIONAL_SERIF;
		}
		else if ((input == "monospaced_sanserif") || (input == "monospaced sans serif"))
		{
			*fontStyleOut = FONT_STYLE_MONOSPACED_SANSSERIF;
		}
		else if ((input == "proportional_sanserif") || (input == "proportional sans serif"))
		{
			*fontStyleOut = FONT_STYLE_PROPORTIONAL_SANSSERIF;
		}
		else if (input == "casual")
		{
			*fontStyleOut = FONT_STYLE_CASUAL;
		}
		else if (input == "cursive")
		{
			*fontStyleOut = FONT_STYLE_CURSIVE;
		}
		else if ((input == "smallcaps") || (input == "small capital"))
		{
			*fontStyleOut = FONT_STYLE_SMALL_CAPITALS;
		}
		else if (input == "auto")
		{
			*fontStyleOut = FONT_STYLE_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported font style type %s", input.c_str());
			retVal = -1;
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		retVal = -1;
	}
	return retVal;
}

/**
 * @brief Get color value from input string
 *
 * @param[in] input - input color value
 * @param[out] colorOut - color option for the input value
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getColor(std::string input, SupportedColors *colorOut)
{
	int retVal = 0;

	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && colorOut)
	{
		transform(input.begin(), input.end(), input.begin(), ::tolower);        /* Makes sure that string is in lower case before comparison */

		auto color = ColorMapTable.find(input);
	        if (color != ColorMapTable.end())
		{
			*colorOut = color->second;
		}
		else
		{
			MW_LOG_ERR("Unsupported font color %s", input.c_str());
			retVal = -1;
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		retVal = -1;
	}
	return retVal;
}

/**
 * @brief Get edge type value from input string
 *
 * @param[in] input - input edge type value
 * @param[out] edgeTypeOut - edge type option for the input value
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getEdgeType(std::string input, EdgeType *edgeTypeOut)
{
	int retVal = 0;

	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && edgeTypeOut)
	{
		transform(input.begin(), input.end(), input.begin(), ::tolower);	/* Makes sure that string is in lower case before comparison */
		if (input == "none")
		{
			*edgeTypeOut = EDGE_TYPE_NONE;
		}
		else if (input == "raised")
		{
			*edgeTypeOut = EDGE_TYPE_RAISED;
		}
		else if (input == "depressed")
		{
			*edgeTypeOut = EDGE_TYPE_DEPRESSED;
		}
		else if (input == "uniform")
		{
			*edgeTypeOut = EDGE_TYPE_UNIFORM;
		}
		else if ((input == "drop_shadow_left") || (input == "left drop shadow"))
		{
			*edgeTypeOut = EDGE_TYPE_SHADOW_LEFT;
		}
		else if ((input == "drop_shadow_right") || (input == "right drop shadow"))
		{
			*edgeTypeOut = EDGE_TYPE_SHADOW_RIGHT;
		}
		else if (input == "auto")
		{
			*edgeTypeOut = EDGE_TYPE_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported edge type %s", input.c_str());
			retVal = -1;
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		retVal = -1;
	}
	return retVal;
}

/**
 * @brief Get opacity value from input string
 *
 * @param[in] input - input opacity value
 * @param[out] opacityOut - opacity option for the input value
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getOpacity(std::string input, Opacity *opacityOut)
{
	int retVal = 0;

	MW_LOG_TRACE("input: %s", input.c_str());
	if (!input.empty() && opacityOut)
	{
		transform(input.begin(), input.end(), input.begin(), ::tolower);	/* Makes sure that string is in lower case before comparison */
		if (input == "solid")
		{
			*opacityOut = OPACITY_SOLID;
		}
		else if (input == "flash")
		{
			*opacityOut = OPACITY_FLASHING;
		}
		else if (input == "translucent")
		{
			*opacityOut = OPACITY_TRANSLUCENT;
		}
		else if (input == "transparent")
		{
			*opacityOut = OPACITY_TRANSPARENT;
		}
		else if (input == "auto")
		{
			*opacityOut = OPACITY_EMBEDDED;
		}
		else
		{
			MW_LOG_ERR("Unsupported opacity %s", input.c_str());
			retVal = -1;
		}
	}
	else
	{
		MW_LOG_ERR("Input is NULL");
		retVal = -1;
	}
	return retVal;
}

/**
 * @brief Gets Attributes of the subtitle
 *
 * @param[in] options - Json string containing the attributes
 * @param[out] attributesValues - Extracted Attribute values (for now they are font size and position)
 * @param[out] attributesMask - Mask corresponding to extracted attribute values
 * @return int - 0 for success, -1 for failure
 */
int TextStyleAttributes::getAttributes(std::string options, attributesType &attributesValues, uint32_t &attributesMask)
{
	int retVal = 0;
	attributesMask = 0;

	MW_LOG_WARN("TextStyleAttributes::getAttributes");

	if (!options.empty())
	{
		std::string optionValue;
		Attributes attribute;
		try
		{
			PlayerJsonObject inputOptions(options);

			if (inputOptions.get("penSize", optionValue))
			{
				if(!getFontSize(optionValue, &(attribute.fontSize)))
				{
					attributesMask |= (1 << FONT_SIZE_ARR_POSITION);
					attributesValues[FONT_SIZE_ARR_POSITION] = attribute.fontSize;
					MW_LOG_INFO("The font size is %d", attributesValues[FONT_SIZE_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse penSize value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("fontStyle", optionValue))
			{
				if(!getFontStyle(optionValue, &(attribute.fontStyle)))
				{
					attributesMask |= (1 << FONT_STYLE_ARR_POSITION);
					attributesValues[FONT_STYLE_ARR_POSITION] = attribute.fontStyle;
					MW_LOG_INFO("The font style is %d", attributesValues[FONT_STYLE_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse fontStyle value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("textForegroundColor", optionValue))
			{
				if(!getColor(optionValue, &(attribute.fontColor)))
				{
					attributesMask |= (1 << FONT_COLOR_ARR_POSITION);
					attributesValues[FONT_COLOR_ARR_POSITION] = attribute.fontColor;
					MW_LOG_INFO("The font color is %d", attributesValues[FONT_COLOR_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse font color value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("textBackgroundColor", optionValue))
			{
				if(!getColor(optionValue, &(attribute.backgroundColor)))
				{
					attributesMask |= (1 << BACKGROUND_COLOR_ARR_POSITION);
					attributesValues[BACKGROUND_COLOR_ARR_POSITION] = attribute.backgroundColor;
					MW_LOG_INFO("The background color is %d", attributesValues[BACKGROUND_COLOR_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse background color value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("textEdgeStyle", optionValue))
			{
				if(!getEdgeType(optionValue, &(attribute.edgeType)))
				{
					attributesMask |= (1 << EDGE_TYPE_ARR_POSITION);
					attributesValues[EDGE_TYPE_ARR_POSITION] = attribute.edgeType;
					MW_LOG_INFO("The edge type is %d", attributesValues[EDGE_TYPE_ARR_POSITION]);
				}
			}
			if (inputOptions.get("textEdgeColor", optionValue))
			{
				if(!getColor(optionValue, &(attribute.edgeColor)))
				{
					attributesMask |= (1 << EDGE_COLOR_ARR_POSITION);
					attributesValues[EDGE_COLOR_ARR_POSITION] = attribute.edgeColor;
					MW_LOG_INFO("The edge color is %d", attributesValues[EDGE_COLOR_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse edge color value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("textBackgroundOpacity", optionValue))
			{
				if(!getOpacity(optionValue, &(attribute.backgroundOpacity)))
				{
					attributesMask |= (1 << BACKGROUND_OPACITY_ARR_POSITION);
					attributesValues[BACKGROUND_OPACITY_ARR_POSITION] = attribute.backgroundOpacity;
					MW_LOG_INFO("The background opacity is %d", attributesValues[BACKGROUND_OPACITY_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse background opacity value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("textForegroundOpacity", optionValue))
			{
				if(!getOpacity(optionValue, &(attribute.fontOpacity)))
				{
					attributesMask |= (1 << FONT_OPACITY_ARR_POSITION);
					attributesValues[FONT_OPACITY_ARR_POSITION] = attribute.fontOpacity;
					MW_LOG_INFO("The font opacity is %d", attributesValues[FONT_OPACITY_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse font opacity value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("windowFillColor", optionValue))
			{
				if(!getColor(optionValue, &(attribute.windowColor)))
				{
					attributesMask |= (1 << WIN_COLOR_ARR_POSITION);
					attributesValues[WIN_COLOR_ARR_POSITION] = attribute.windowColor;
					MW_LOG_INFO("The window color is %d", attributesValues[WIN_COLOR_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse window color value of %s", optionValue.c_str());
				}
			}
			if (inputOptions.get("windowFillOpacity", optionValue))
			{
				if(!getOpacity(optionValue, &(attribute.windowOpacity)))
				{
					attributesMask |= (1 << WIN_OPACITY_ARR_POSITION);
					attributesValues[WIN_OPACITY_ARR_POSITION] = attribute.windowOpacity;
					MW_LOG_INFO("The window opacity is %d", attributesValues[WIN_OPACITY_ARR_POSITION]);
				}
				else
				{
					MW_LOG_WARN("Cannot parse window opacity value of %s", optionValue.c_str());
				}
			}
		}
		catch(const PlayerJsonParseException& e)
		{
			MW_LOG_ERR("TextStyleAttributes: PlayerJsonParseException - %s", e.what());
			return -1;
		}
	}
	else
	{
		retVal = -1;
		MW_LOG_WARN("Empty input Json string");
	}
	return retVal;
}
