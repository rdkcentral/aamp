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
 * @file AampJsonObject.cpp
 * @brief File to handle Json format object 
 */

#include <vector>

#include "AampJsonObject.h"
#include "AampUtils.h"
#include "_base64.h"



AampJsonObject::AampJsonObject() : mParent(NULL), mJsonObj()
{
	mJsonObj = cJSON_CreateObject();
}

AampJsonObject::AampJsonObject(const std::string& jsonStr) : mParent(NULL), mJsonObj()
{
	mJsonObj = cJSON_Parse(jsonStr.c_str());

	if (!mJsonObj)
	{
		throw AampJsonParseException();
	}
}

AampJsonObject::AampJsonObject(const char* jsonStr) : mParent(NULL), mJsonObj()
{
	mJsonObj = cJSON_Parse(jsonStr);

	if (!mJsonObj)
	{
		throw AampJsonParseException();
	}
}

AampJsonObject::~AampJsonObject()
{
	if (!mParent)
	{
		cJSON_Delete(mJsonObj);
	}
}

/**
 *  @brief Add a string value
 */
bool AampJsonObject::add(const std::string& name, const std::string& value, const ENCODING encoding)
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
bool AampJsonObject::add(const std::string& name, const char *value, const ENCODING encoding)
{
	return add(name, std::string(value), encoding);
}

/**
 *  @brief Add a vector of string values as a JSON array
 */
bool AampJsonObject::add(const std::string& name, const std::vector<std::string>& values)
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
bool AampJsonObject::add(const std::string& name, const std::vector<uint8_t>& values, const ENCODING encoding)
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
			const char *encodedResponse = aamp_Base64_URL_Encode( reinterpret_cast<const unsigned char*>(values.data()), values.size());
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
 *  @brief Add a #AampJsonObject value
 */
bool AampJsonObject::add(const std::string& name, AampJsonObject& value)
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
 *  @brief Add a vector of #AampJsonObject values as a JSON array
 */
bool AampJsonObject::add(const std::string& name, std::vector<AampJsonObject*>& values)
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
bool AampJsonObject::add(const std::string& name, cJSON *value)
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
bool AampJsonObject::add(const std::string& name, bool value)
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
bool AampJsonObject::add(const std::string& name, int value)
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
bool AampJsonObject::add(const std::string& name, double value)
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
bool AampJsonObject::add(const std::string& name, long value)
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
bool AampJsonObject::set(AampJsonObject *parent, cJSON *object)
{
	this->mParent = parent;
	this->mJsonObj = object;
	/**< return true always to match the template */
	return true;
}

/**
 *  @brief Get the AampJson object from json data within the Json data
 */
bool AampJsonObject::get(const std::string& name, AampJsonObject &value)
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
bool AampJsonObject::get(const std::string& name, std::vector<AampJsonObject> &values)
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
bool AampJsonObject::get(const std::string& name, std::string& value)
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
bool AampJsonObject::get(const std::string& name, int& value)
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
bool AampJsonObject::get(const std::string& name, double& value)
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
bool AampJsonObject::get(const std::string& name, std::vector<std::string>& values)
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
 * @brief Get a string value as a vector of bytes
 */
bool AampJsonObject::get(const std::string& name, std::vector<uint8_t>& values, const ENCODING encoding)
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
				const unsigned char *decodedResponse = aamp_Base64_URL_Decode(strValue.c_str(), &decodedSize, strValue.length());
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
std::string AampJsonObject::print()
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
std::string AampJsonObject::print_UnFormatted()
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
void AampJsonObject::print(std::vector<uint8_t>& data)
{
	std::string jsonOutputStr = print();
	(void)data.insert(data.begin(), jsonOutputStr.begin(), jsonOutputStr.end());
}

/**
 *  @brief Check whether the value is Array or not
 */
bool AampJsonObject::isArray(const std::string& name)
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
bool AampJsonObject::isString(const std::string& name)
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
bool AampJsonObject::isNumber(const std::string& name)
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
bool AampJsonObject::isObject(const std::string& name)
{
	cJSON *strObj = cJSON_GetObjectItem(mJsonObj, name.c_str());
	bool retVal = false;
	if (strObj)
	{
		retVal = cJSON_IsObject(strObj);
	}
	return retVal;
}
