/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#include <JavaScriptCore/JavaScript.h>
#include <string>

#include <vector>
#include <map>

typedef enum { eJSON_TYPE_NULL, eJSON_TYPE_INT, eJSON_TYPE_STR, eJSON_TYPE_ARRAY, eJSON_TYPE_OBJ } JsonType;

class JsonValueInternal
{
private:
	JsonType type;
public:
	int iVal;
	std::string sVal;
	std::vector<class JsonValueInternal *> aVal;
	std::map<std::string,class JsonValueInternal *> oVal;
	
public:
	JsonValueInternal( JsonType type ) : type(type)
	{
	}
	
	~JsonValueInternal()
	{
	}
};

JSClassRef JSClassCreate(const JSClassDefinition* definition)
{
	return NULL;
}

void JSClassRelease(JSClassRef jsClass)
{
}

JSGlobalContextRef JSContextGetGlobalContext(JSContextRef)
{
	return NULL;
}

void* JSObjectGetPrivate(JSObjectRef object)
{
	return NULL;
}

void JSGarbageCollect(JSContextRef ctx)
{
}

JSObjectRef JSContextGetGlobalObject(JSContextRef ctx)
{
	return NULL;
}

JSValueRef JSObjectCallAsFunction(JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	return NULL;
}

JS_EXPORT bool JSObjectDeleteProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
	return false;
}

JSValueRef JSObjectGetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
	return NULL;
}

JSValueRef JSObjectGetPropertyAtIndex(JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception)
{
	return NULL;
}

bool JSObjectIsConstructor(JSContextRef ctx, JSObjectRef object)
{
	return false;
}
bool JSObjectIsFunction(JSContextRef ctx, JSObjectRef object)
{
	return false;
}

JSObjectRef JSObjectMake(JSContextRef ctx, JSClassRef jsClass, void* data)
{
	return (JSObjectRef)new JsonValueInternal(eJSON_TYPE_OBJ);
	//return NULL;
}

JSObjectRef JSObjectMakeArray(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	return (JSObjectRef)new JsonValueInternal(eJSON_TYPE_ARRAY);
//	return NULL;
}

JSObjectRef JSObjectMakeConstructor(JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor)
{
	return NULL;
}

JSObjectRef JSObjectMakeError(JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
	return NULL;
}

JSObjectRef JSObjectMakeFunction(JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception)
{
	return NULL;
}

bool JSValueIsNumber(JSContextRef ctx, JSValueRef value)
{
	return false;
}

bool JSValueIsString(JSContextRef ctx, JSValueRef value)
{
	return false;
}

bool JSValueIsBoolean(JSContextRef ctx, JSValueRef value)
{
	return false;
}

bool JSValueIsObject(JSContextRef ctx, JSValueRef value)
{
	return false;
}

bool JSObjectSetPrivate(JSObjectRef object, void* data)
{
	return false;
}
void JSObjectSetProperty(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception)
{
}

JSStringRef JSStringCreateWithUTF8CString(const char* string)
{
	auto obj = new JsonValueInternal(eJSON_TYPE_STR);
	obj->sVal = string;
	return (JSStringRef)obj;
//	return NULL;
}

size_t JSStringGetMaximumUTF8CStringSize(JSStringRef string)
{
	return 0;
}

size_t JSStringGetUTF8CString(JSStringRef string, char* buffer, size_t bufferSize)
{
	return 0;
}

void JSStringRelease(JSStringRef string)
{
}

JSStringRef JSValueCreateJSONString(JSContextRef ctx, JSValueRef value, unsigned indent, JSValueRef* exception)
{
	return NULL;
}

bool JSValueIsInstanceOfConstructor(JSContextRef ctx, JSValueRef value, JSObjectRef constructor, JSValueRef* exception)
{
	return false;
}

JSValueRef JSValueMakeBoolean(JSContextRef ctx, bool boolean)
{
	return NULL;
}

JS_EXPORT JSValueRef JSValueMakeNumber(JSContextRef ctx, double number)
{
	return NULL;
}

JSValueRef JSValueMakeString(JSContextRef ctx, JSStringRef string)
{
	return NULL;
}

JSValueRef JSValueMakeUndefined(JSContextRef ctx)
{
	return NULL;
}

void JSValueProtect(JSContextRef ctx, JSValueRef value)
{
}

bool JSValueToBoolean(JSContextRef ctx, JSValueRef value)
{
	return false;
}

double JSValueToNumber(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
	return 0.0;
}

JSObjectRef JSValueToObject(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
	return NULL;
}

JSStringRef JSValueToStringCopy(JSContextRef ctx, JSValueRef value, JSValueRef* exception)
{
	return NULL;
}

void JSValueUnprotect(JSContextRef ctx, JSValueRef value)
{
}

std::string GetBrowserUA(JSContextRef ctx)
{
	return "";
}
