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
 * @file PlayerJsonObject.cpp
 * @brief File to handle Json format object
 */

#include <vector>

#include "PlayerJsonObject.h"
#include "_base64.h"

//unsigned char *player_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen);
char *player_Base64_URL_Encode(const unsigned char *src, size_t len);

PlayerJsonObject::PlayerJsonObject() : mParent(NULL), mJsonObj()
{
	mJsonObj = cJSON_CreateObject();
}

PlayerJsonObject::PlayerJsonObject(const std::string& jsonStr) : mParent(NULL), mJsonObj()
{
	mJsonObj = cJSON_Parse(jsonStr.c_str());

	if (!mJsonObj)
	{
		throw PlayerJsonParseException();
	}
}

PlayerJsonObject::PlayerJsonObject(const char* jsonStr) : mParent(NULL), mJsonObj()
{
	mJsonObj = cJSON_Parse(jsonStr);

	if (!mJsonObj)
	{
		throw PlayerJsonParseException();
	}
}

PlayerJsonObject::~PlayerJsonObject()
{
	if (!mParent)
	{
		cJSON_Delete(mJsonObj);
	}
}

/**
 *  @brief Add a string value
 */
bool PlayerJsonObject::add(const std::string& name, const std::string& value, const ENCODING encoding)
{
	bool res = false;

	if (encoding == ENCODING_STRING)
	{
		res = add(name, cJSON_CreateString(value.c_str()));
	}
	else
	{
		res = add(name, std::vector<uint8_t>(value.begin(), value.end()), encoding);
	}

	return res;
}

/**
 *  @brief Add a string value
 */
bool PlayerJsonObject::add(const std::string& name, const char *value, const ENCODING encoding)
{
	return add(name, std::string(value), encoding);
}

/**
 *  @brief Add a vector of string values as a JSON array
 */
bool PlayerJsonObject::add(const std::string& name, const std::vector<std::string>& values)
{
	bool res = false;
	cJSON* arr = cJSON_CreateArray();

	if (!arr)
	{
		// Failed to  create array
	}
	else
	{
		res = true;
		for (auto& value : values)
		{
			cJSON* string_item = cJSON_CreateString(value.c_str());

			if (!string_item)
			{
				// Create string failed
				res = false;
			}
			else if (!cJSON_AddItemToArray(arr, string_item))
			{
				cJSON_Delete(string_item);
				res = false;
			}
			else
			{
				// String added successfully
			}

			if (!res)
			{
				// Adding an item failed
				break;
			}
		}

		if (!res)
		{
			// Something failed
		}
		else if (!add(name, arr))
		{
			// Adding the array to the json object failed
			res = false;
		}
		else
		{
			// Array added successfully
		}

		if (!res)
		{
			// Something failed, so delete whole array
			cJSON_Delete(arr);
		}
	}

	return res;
}

/**
 *  @brief Add the provided bytes after encoding in the specified encoding
 */
bool PlayerJsonObject::add(const std::string& name, const std::vector<uint8_t>& values, const ENCODING encoding)
{
	bool res = false;

	switch (encoding)
	{
		case ENCODING_STRING:
		{
			std::string strValue(values.begin(), values.end());
			res = add(name, cJSON_CreateString(strValue.c_str()));
		}
		break;

		case ENCODING_BASE64:
		{
			const char *encodedResponse = base64_Encode( reinterpret_cast<const unsigned char*>(values.data()), values.size());
			if (encodedResponse != NULL)
			{
				res = add(name, cJSON_CreateString(encodedResponse));
				free((void*)encodedResponse);
			}
		}
		break;

		case ENCODING_BASE64_URL:
		{
			const char *encodedResponse = player_Base64_URL_Encode( reinterpret_cast<const unsigned char*>(values.data()), values.size());
			if (encodedResponse != NULL)
			{
				res = add(name, cJSON_CreateString(encodedResponse));
				free((void*)encodedResponse);
			}
		}
		break;

		default:
			/* Unsupported encoding format */
			break;
	}

	return res;
}

/**
 *  @brief Add a #PlayerJsonObject value
 */
bool PlayerJsonObject::add(const std::string& name, PlayerJsonObject& value)
{
	bool res = false;

	if (cJSON_AddItemToObject(mJsonObj, name.c_str(), value.mJsonObj))
	{
	value.mParent = this;
		res = true;
	}

	return res;
}

/**
 *  @brief Add a vector of #PlayerJsonObject values as a JSON array
 */
bool PlayerJsonObject::add(const std::string& name, std::vector<PlayerJsonObject*>& values)
{
	bool res = false;
	cJSON *arr = cJSON_CreateArray();

	if (!arr)
	{
		// Failed to  create array
	}
	else
	{
		res = true;
	for (auto& obj : values)
	{
			if (!cJSON_AddItemToArray(arr, obj->mJsonObj))
			{
				// Failed to add item to array
				res = false;
				break;
			}
			else
			{
		obj->mParent = this;
	}
		}

		if (!res)
		{
			// Something failed
		}
		else if (!add(name, arr))
		{
			// Adding the array to the json object failed
			res = false;
		}
		else
		{
			// Array added successfully
		}

		if (!res)
		{
			// Something failed, so delete whole array
			cJSON_Delete(arr);
		}
	}

	return res;
}

/**
 *  @brief Add a cJSON value
 */
bool PlayerJsonObject::add(const std::string& name, cJSON *value)
{
	bool res = false;

	if (NULL == value)
	{
		// NULL parameter passed
	}
	else if (!cJSON_AddItemToObject(mJsonObj, name.c_str(), value))
	{
		// cJSON_AddItemToObject failed
	}
	else
	{
		res = true;
	}

	return res;
}


/**
 *  @brief Add a bool value
 */
bool PlayerJsonObject::add(const std::string& name, bool value)
{
	bool res = false;
	cJSON *bool_item = cJSON_CreateBool(value);

	if (!bool_item)
	{
		// Failed to create a bool
	}
	else if (!cJSON_AddItemToObject(mJsonObj, name.c_str(), bool_item))
	{
		cJSON_Delete(bool_item);
	}
	else
	{
		res = true;
	}

	return res;
}

/**
 *  @brief Add an int value
 */
bool PlayerJsonObject::add(const std::string& name, int value)
{
	bool res = false;
	cJSON *number_item = cJSON_CreateNumber(value);

	if (!number_item)
	{
		// Failed to create a number
	}
	else if (!cJSON_AddItemToObject(mJsonObj, name.c_str(), number_item))
	{
		cJSON_Delete(number_item);
	}
	else
	{
		res = true;
	}

	return res;
}

/**
 *  @brief Add a double value
 */
bool PlayerJsonObject::add(const std::string& name, double value)
{
	bool res = false;
	cJSON *number_item = cJSON_CreateNumber(value);

	if (!number_item)
	{
		// Failed to create a number
	}
	else if (!cJSON_AddItemToObject(mJsonObj, name.c_str(), number_item))
	{
		cJSON_Delete(number_item);
	}
	else
	{
		res = true;
	}

	return res;
}

/**
 *  @brief Add a long value
 */
bool PlayerJsonObject::add(const std::string& name, long value)
{
	bool res = false;
	cJSON *number_item = cJSON_CreateNumber(value);

	if (!number_item)
	{
		// Failed to create a number
	}
	else if (!cJSON_AddItemToObject(mJsonObj, name.c_str(), number_item))
	{
		cJSON_Delete(number_item);
	}
	else
	{
		res = true;
	}

	return res;
}

/**
 * @brief Set cJSON value
 */
bool PlayerJsonObject::set(PlayerJsonObject *parent, cJSON *object)
{
	this->mParent = parent;
	this->mJsonObj = object;
	/**< return true always to match the template */
	return true;
}

/**
 *  @brief Get the PlayerJson object from json data within the Json data
 */
bool PlayerJsonObject::get(const std::string& name, PlayerJsonObject &value)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retValue = false;
	if (strObj)
	{
		retValue = value.set(this, strObj);
	}
	return retValue;
}

/**
 *  @brief Get an array of objects from JSON data
 *  @param name name for the property
 *  @param[out] value JSON object array
 *  @return true if successfully retrieved, false otherwise
 */
bool PlayerJsonObject::get(const std::string& name, std::vector<PlayerJsonObject> &values)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	cJSON *object = NULL;
	bool retVal = true;
	int idx = 0;

	values.clear();
	cJSON_ArrayForEach(object, strObj)
	{
		values.emplace_back();
		values[idx++].set(this, object);
	}
	return retVal;
}

/**
 *  @brief Get a string value
 */
bool PlayerJsonObject::get(const std::string& name, std::string& value)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());

	if (strObj)
	{
		char *strValue = cJSON_GetStringValue(strObj);
		if (strValue)
		{
			value = strValue;
			return true;
		}
	}
	return false;
}

/**
 *  @brief Get a int value from a JSON data
 */
bool PlayerJsonObject::get(const std::string& name, int& value)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retValue = false;
	if (strObj)
	{
		// TODO: replace with cJSON_GetNumberValue(strObj);
		value =  (int)strObj->valuedouble;
		retValue = true;
	}
	return retValue;
}

/**
 *  @brief Get a double value from JSON data
 *  @param name name for the property
 *  @param[out] value double value
 *  @return true if successfully retrieved, false otherwise
 */
bool PlayerJsonObject::get(const std::string& name, double& value)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retValue = false;
	if (strObj && cJSON_IsNumber(strObj))
	{
		value = strObj->valuedouble;
		retValue = true;
	}
	return retValue;
}

/**
 * @brief Get a string value
 */
bool PlayerJsonObject::get(const std::string& name, std::vector<std::string>& values)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	cJSON *object = NULL;
	bool retVal = false;
	cJSON_ArrayForEach(object, strObj)
	{
		char *strValue = cJSON_GetStringValue(object);
		if (strValue)
		{
			values.push_back(std::string(strValue));
			retVal = true;
		}
	}
	return retVal;
}

/**
 * @brief convert blob of binary data to ascii base64-URL-encoded equivalent
 * @retval pointer to malloc'd cstring containing base64 URL encoded version of string
 * @retval NULL if insufficient memory to allocate base64-URL-encoded copy
 * @note caller responsible for freeing returned cstring
 */
char *player_Base64_URL_Encode(const unsigned char *src, size_t len)
{
	char *rc = base64_Encode(src,len);
	if( rc )
	{
		char *dst = rc;
		while( *dst )
		{
			switch( *dst )
			{
				case '+':
					*dst = '-';
					break;
				case '/':
					*dst = '_';
					break;
				case '=':
					*dst = '\0';
					break;
				default:
					break;
			}
			dst++;
		}
	}
	return rc;
}

/**
 * @brief decode base64 URL encoded data to binary equivalent
 * @retval pointer to malloc'd memory containing decoded binary data
 * @retval NULL if insufficient memory to allocate decoded data
 * @note caller responsible for freeing returned data
 */
unsigned char *player_Base64_URL_Decode(const char *src, size_t *len, size_t srcLen)
{
	unsigned char * rc = NULL;
	char *temp = (char *)malloc(srcLen+3);
	if( temp )
	{
		temp[srcLen+2] = '\0';
		temp[srcLen+1] = '=';
		temp[srcLen+0] = '=';
		for( int iter = 0; iter < srcLen; iter++ )
		{
			char c = src[iter];
			switch( c )
			{
				case '_':
					c = '/';
					break;
				case '-':
					c = '+';
					break;
				default:
					break;
			}
			temp[iter] = c;
		}
		rc = base64_Decode(temp, len );
		free(temp);
	}
	else
	{
		*len = 0;
	}
	return rc;
}


/**
 * @brief Get a string value as a vector of bytes
 */
bool PlayerJsonObject::get(const std::string& name, std::vector<uint8_t>& values, const ENCODING encoding)
{
	bool res = false;
	std::string strValue;

	if (get(name, strValue))
	{
		values.clear();

		switch (encoding)
		{
			case ENCODING_STRING:
			{
				values.insert(values.begin(), strValue.begin(), strValue.end());
			}
			break;

			case ENCODING_BASE64:
			{
				size_t decodedSize = 0;
				const unsigned char *decodedResponse = base64_Decode(strValue.c_str(), &decodedSize, strValue.length());
				if (decodedResponse != NULL)
				{
					values.insert(values.begin(), decodedResponse, decodedResponse + decodedSize);
					res = true;
					free((void *)decodedResponse);
				}
			}
			break;

			case ENCODING_BASE64_URL:
			{
				size_t decodedSize = 0;
				const unsigned char *decodedResponse = player_Base64_URL_Decode(strValue.c_str(), &decodedSize, strValue.length());
				if (decodedResponse != NULL)
				{
					values.insert(values.begin(), decodedResponse, decodedResponse + decodedSize);
					res = true;
					free((void *)decodedResponse);
				}
			}
			break;

			default:
				/* Unsupported encoding format */
				break;
		}
	}
	return res;
}

/**
 *  @brief Print the constructed JSON to a string
 */
std::string PlayerJsonObject::print()
{
	char *jsonString = cJSON_Print(mJsonObj);
	if (NULL != jsonString)
	{
		std::string retStr(jsonString);
		cJSON_free(jsonString);
		return retStr;
	}
	return "";
}

/**
 *  @brief Print the constructed JSON to a string
 */
std::string PlayerJsonObject::print_UnFormatted()
{
	char *jsonString = cJSON_PrintUnformatted(mJsonObj);
	if (NULL != jsonString)
	{
		std::string retStr(jsonString);
		cJSON_free(jsonString);
		return retStr;
	}
	return "";
}

/**
 *  @brief Print the constructed JSON into the provided vector
 */
void PlayerJsonObject::print(std::vector<uint8_t>& data)
{
	std::string jsonOutputStr = print();
	(void)data.insert(data.begin(), jsonOutputStr.begin(), jsonOutputStr.end());
}

/**
 *  @brief Check whether the value is Array or not
 */
bool PlayerJsonObject::isArray(const std::string& name)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retVal = false;
	if (strObj)
	{
		retVal =  cJSON_IsArray(strObj);
	}
	return retVal;
}

/**
 * @brief Check whether the value is String or not
 */
bool PlayerJsonObject::isString(const std::string& name)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retVal = false;
	if (strObj)
	{
		retVal =  cJSON_IsString(strObj);
	}
	return retVal;
}

/**
 * @brief Check whether the value is Number or not
 */
bool PlayerJsonObject::isNumber(const std::string& name)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retVal = false;
	if (strObj)
	{
		retVal =  cJSON_IsNumber(strObj);
	}
	return retVal;
}

/**
 * @brief Check whether the value is Object or not
 */
bool PlayerJsonObject::isObject(const std::string& name)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retVal = false;
	if (strObj)
	{
		retVal = cJSON_IsObject(strObj);
	}
	return retVal;
}

