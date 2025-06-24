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

#include "MockPlayerJsonObject.h"
#include "PlayerJsonObject.h"

std::shared_ptr<MockPlayerJsonObject> g_mockPlayerJsonObject;

PlayerJsonObject::PlayerJsonObject() : mParent(NULL), mJsonObj()
{
}

PlayerJsonObject::PlayerJsonObject(const std::string& jsonStr) : mParent(NULL), mJsonObj()
{
}

PlayerJsonObject::PlayerJsonObject(const char* jsonStr) : mParent(NULL), mJsonObj()
{
}

PlayerJsonObject::~PlayerJsonObject()
{
}

bool PlayerJsonObject::add(const std::string& name, const std::string& value, const ENCODING encoding)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value, encoding);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, const char *value, const ENCODING encoding)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value, encoding);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, const std::vector<std::string>& values)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, values);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, const std::vector<uint8_t>& values, const ENCODING encoding)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, values, encoding);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, PlayerJsonObject& value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, std::vector<PlayerJsonObject*>& values)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, values);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, cJSON *value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, bool value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, int value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, double value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value);
	}
	return false;
}

bool PlayerJsonObject::add(const std::string& name, long value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->add(name, value);
	}
	return false;
}

bool PlayerJsonObject::set(PlayerJsonObject *parent, cJSON *object)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->set(parent, object);
	}
	return false;
}

bool PlayerJsonObject::get(const std::string& name, PlayerJsonObject &value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->get(name, value);
	}
	return false;
}

bool PlayerJsonObject::get(const std::string& name, std::string& value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->get(name, value);
	}
	return false;
}

bool PlayerJsonObject::get(const std::string& name, int& value)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->get(name, value);
	}
	return false;
}

bool PlayerJsonObject::get(const std::string& name, std::vector<std::string>& values)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->get(name, values);
	}
	return false;
}

bool PlayerJsonObject::get(const std::string& name, std::vector<uint8_t>& values, const ENCODING encoding)
{
	if (g_mockPlayerJsonObject != nullptr)
	{
		return g_mockPlayerJsonObject->get(name, values, encoding);
	}
	return false;
}

std::string PlayerJsonObject::print()
{
	return "";
}

std::string PlayerJsonObject::print_UnFormatted()
{
	return "";
}

void PlayerJsonObject::print(std::vector<uint8_t>& data)
{
}

bool PlayerJsonObject::isArray(const std::string& name)
{
	return false;
}

bool PlayerJsonObject::isString(const std::string& name)
{
	return false;
}

bool PlayerJsonObject::isNumber(const std::string& name)
{
	return false;
}

bool PlayerJsonObject::isObject(const std::string& name)
{
	return false;
}

