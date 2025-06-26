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

#ifndef AAMP_MOCK_JAVASCRIPT_CORE_H
#define AAMP_MOCK_JAVASCRIPT_CORE_H

#include <gmock/gmock.h>
#include <JavaScriptCore/JavaScript.h>

class MockJavaScriptCore
{
public:
	MOCK_METHOD(JSClassRef, JSClassCreate, (const JSClassDefinition* definition));
	MOCK_METHOD(void, JSClassRelease, (JSClassRef jsClass));
	MOCK_METHOD(JSGlobalContextRef, JSContextGetGlobalContext, (JSContextRef ctx));
	MOCK_METHOD(void, JSGarbageCollect, (JSContextRef ctx));
	MOCK_METHOD(JSObjectRef, JSContextGetGlobalObject, (JSContextRef ctx));
	MOCK_METHOD(JSValueRef, JSObjectCallAsFunction, (JSContextRef ctx, JSObjectRef object, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception));
	MOCK_METHOD(bool, JSObjectDeleteProperty, (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception));
	MOCK_METHOD(JSValueRef, JSObjectGetProperty, (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception));
	MOCK_METHOD(JSValueRef, JSObjectGetPropertyAtIndex, (JSContextRef ctx, JSObjectRef object, unsigned propertyIndex, JSValueRef* exception));
	MOCK_METHOD(bool, JSObjectIsConstructor, (JSContextRef ctx, JSObjectRef object));
	MOCK_METHOD(bool, JSObjectIsFunction, (JSContextRef ctx, JSObjectRef object));
	MOCK_METHOD(JSObjectRef, JSObjectMake, (JSContextRef ctx, JSClassRef jsClass, void* data));
	MOCK_METHOD( JSObjectRef, JSObjectMakeArray, (JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception));
	MOCK_METHOD( JSObjectRef, JSObjectMakeConstructor, (JSContextRef ctx, JSClassRef jsClass, JSObjectCallAsConstructorCallback callAsConstructor));
	MOCK_METHOD( JSObjectRef, JSObjectMakeError, (JSContextRef ctx, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception));
	MOCK_METHOD( JSObjectRef, JSObjectMakeFunction, (JSContextRef ctx, JSStringRef name, unsigned parameterCount, const JSStringRef parameterNames[], JSStringRef body, JSStringRef sourceURL, int startingLineNumber, JSValueRef* exception));
	MOCK_METHOD( bool, JSValueIsNumber, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD( bool, JSValueIsString, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD( bool, JSValueIsBoolean, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD( bool, JSValueIsObject, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD( bool, JSObjectSetPrivate, (JSObjectRef object, void* data));
	MOCK_METHOD( void, JSObjectSetProperty, (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSPropertyAttributes attributes, JSValueRef* exception));
	MOCK_METHOD( JSStringRef, JSStringCreateWithUTF8CString, (const char* string));
	MOCK_METHOD( size_t, JSStringGetMaximumUTF8CStringSize, (JSStringRef string));
	MOCK_METHOD( size_t, JSStringGetUTF8CString, (JSStringRef string, char* buffer, size_t bufferSize));
	MOCK_METHOD( void, JSStringRelease, (JSStringRef string));
	MOCK_METHOD( JSStringRef, JSValueCreateJSONString, (JSContextRef ctx, JSValueRef value, unsigned indent, JSValueRef* exception));
	MOCK_METHOD( bool, JSValueIsInstanceOfConstructor, (JSContextRef ctx, JSValueRef value, JSObjectRef constructor, JSValueRef* exception));
	MOCK_METHOD( JSValueRef, JSValueMakeBoolean, (JSContextRef ctx, bool boolean));
	MOCK_METHOD( JSValueRef, JSValueMakeNumber, (JSContextRef ctx, double number));
	MOCK_METHOD( JSValueRef, JSValueMakeString, (JSContextRef ctx, JSStringRef string));
	MOCK_METHOD( JSValueRef, JSValueMakeUndefined, (JSContextRef ctx));
	MOCK_METHOD( void, JSValueProtect, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD( bool, JSValueToBoolean, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD(double, JSValueToNumber, (JSContextRef ctx, JSValueRef value, JSValueRef* exception));
	MOCK_METHOD( JSObjectRef, JSValueToObject, (JSContextRef ctx, JSValueRef value, JSValueRef* exception));
	MOCK_METHOD( JSStringRef, JSValueToStringCopy, (JSContextRef ctx, JSValueRef value, JSValueRef* exception));
	MOCK_METHOD( void, JSValueUnprotect, (JSContextRef ctx, JSValueRef value));
	MOCK_METHOD( std::string, GetBrowserUA, (JSContextRef ctx));

};

extern MockJavaScriptCore *g_mockJavaScriptCore;

#endif /* AAMP_MOCK_JAVASCRIPT_CORE_H */
