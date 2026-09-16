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
#include "jsbindings.h"
#include "jsevent.h"
#include "jseventlistener.h"
#include "jsutils.h"
#include "jsutils.h"
#include "PersistentWatermark.h"
#include "PersistentWatermarkDisplaySequencer.h"
#include "PersistentWatermarkEventHandler.h"
#include "PersistentWatermarkPluginAccess.h"
#include "PersistentWatermarkStorage.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockJavaScriptCore.h"

#include <algorithm>
#include <memory>

using ::testing::_;

std::shared_ptr<MockJavaScriptCore> g_mockJavaScriptCore;

class JsBindingTests : public ::testing::Test
{
protected:
	PlayerInstanceAAMP *playerInstanceAAMP;

	void SetUp() override
	{
		playerInstanceAAMP = new PlayerInstanceAAMP();
		g_mockJavaScriptCore = std::make_shared<MockJavaScriptCore>();
	}

	void TearDown() override
	{
		g_mockJavaScriptCore.reset();
		delete playerInstanceAAMP;
	}
public:
};

TEST_F(JsBindingTests, TestJsBindings)
{
	void *context = NULL;
	aamp_LoadJS(context, playerInstanceAAMP);

	aamp_UnloadJS( context );
}

TEST_F(JsBindingTests, TestJsonUtils )
{
	JSContextRef context = NULL;

	//EXPECT_CALL(*g_mockJavaScriptCore, JSObjectMake(_,_,_)).WillOnce(testing::Return( objRef ));
	JSObjectRef temp1 = aamp_CreateBodyResponseJSObject(context, "{}");
	JSObjectRef temp2 = aamp_CreateBodyResponseJSObject(context, "" );
	JSObjectRef temp3 = aamp_CreateBodyResponseJSObject(context, NULL );
	JSObjectRef temp4 = aamp_CreateBodyResponseJSObject(context, "{\"a\":1,\"b\":\"foo\"}" );
}

// Copies 'content' into the buffer supplied by aamp_JSValueToCString/aamp_JSValueToJSONCString,
// mirroring JSStringGetUTF8CString()'s real contract (bytes written including null terminator).
static size_t FakeWriteUTF8CString(const std::string &content, char *buffer, size_t bufferSize)
{
	size_t n = (bufferSize > 0) ? std::min(content.size(), bufferSize - 1) : 0;
	content.copy(buffer, n);
	buffer[n] = '\0';
	return n + 1;
}

TEST_F(JsBindingTests, JsValueToCStringConvertsNormalString)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x1);
	JSStringRef jsstr = reinterpret_cast<JSStringRef>(0x2);
	const std::string expected = "hello world";

	EXPECT_CALL(*g_mockJavaScriptCore, JSValueToStringCopy(context, value, nullptr))
		.WillOnce(testing::Return(jsstr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(jsstr))
		.WillOnce(testing::Return(expected.size() + 1));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(jsstr, testing::_, expected.size() + 1))
		.WillOnce(testing::Invoke([&expected](JSStringRef, char *buffer, size_t bufferSize) {
			return FakeWriteUTF8CString(expected, buffer, bufferSize);
		}));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(jsstr)).Times(1);

	std::string result = aamp_JSValueToCString(context, value, NULL);
	EXPECT_EQ(result, expected);
}

TEST_F(JsBindingTests, JsValueToCStringConvertsEmptyString)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x1);
	JSStringRef jsstr = reinterpret_cast<JSStringRef>(0x2);
	const std::string expected = "";

	EXPECT_CALL(*g_mockJavaScriptCore, JSValueToStringCopy(context, value, nullptr))
		.WillOnce(testing::Return(jsstr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(jsstr))
		.WillOnce(testing::Return(expected.size() + 1));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(jsstr, testing::_, expected.size() + 1))
		.WillOnce(testing::Invoke([&expected](JSStringRef, char *buffer, size_t bufferSize) {
			return FakeWriteUTF8CString(expected, buffer, bufferSize);
		}));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(jsstr)).Times(1);

	std::string result = aamp_JSValueToCString(context, value, NULL);
	EXPECT_TRUE(result.empty());
}

TEST_F(JsBindingTests, JsValueToCStringConvertsNonAsciiString)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x1);
	JSStringRef jsstr = reinterpret_cast<JSStringRef>(0x2);
	// UTF-8 bytes for "héllo wörld " followed by the U+1F600 emoji
	const std::string expected = "h\xC3\xA9llo w\xC3\xB6rld \xF0\x9F\x98\x80";

	EXPECT_CALL(*g_mockJavaScriptCore, JSValueToStringCopy(context, value, nullptr))
		.WillOnce(testing::Return(jsstr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(jsstr))
		.WillOnce(testing::Return(expected.size() + 1));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(jsstr, testing::_, expected.size() + 1))
		.WillOnce(testing::Invoke([&expected](JSStringRef, char *buffer, size_t bufferSize) {
			return FakeWriteUTF8CString(expected, buffer, bufferSize);
		}));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(jsstr)).Times(1);

	std::string result = aamp_JSValueToCString(context, value, NULL);
	EXPECT_EQ(result, expected);
}

TEST_F(JsBindingTests, JsValueToCStringReturnsEmptyOnConversionException)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x1);
	JSValueRef exception = NULL;

	// JSValueToStringCopy() returns NULL when the conversion raises an exception
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueToStringCopy(context, value, &exception))
		.WillOnce(testing::Return(nullptr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(testing::_)).Times(0);
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(testing::_)).Times(0);

	std::string result = aamp_JSValueToCString(context, value, &exception);
	EXPECT_TRUE(result.empty());
}

TEST_F(JsBindingTests, JsValueToJSONCStringConvertsNormalObject)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x3);
	JSStringRef jsstr = reinterpret_cast<JSStringRef>(0x4);
	const std::string expected = "{\"a\":1,\"b\":\"foo\"}";

	EXPECT_CALL(*g_mockJavaScriptCore, JSValueCreateJSONString(context, value, 0, nullptr))
		.WillOnce(testing::Return(jsstr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(jsstr))
		.WillOnce(testing::Return(expected.size() + 1));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(jsstr, testing::_, expected.size() + 1))
		.WillOnce(testing::Invoke([&expected](JSStringRef, char *buffer, size_t bufferSize) {
			return FakeWriteUTF8CString(expected, buffer, bufferSize);
		}));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(jsstr)).Times(1);

	std::string result = aamp_JSValueToJSONCString(context, value, NULL);
	EXPECT_EQ(result, expected);
}

TEST_F(JsBindingTests, JsValueToJSONCStringConvertsEmptyString)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x3);
	JSStringRef jsstr = reinterpret_cast<JSStringRef>(0x4);
	const std::string expected = "";

	EXPECT_CALL(*g_mockJavaScriptCore, JSValueCreateJSONString(context, value, 0, nullptr))
		.WillOnce(testing::Return(jsstr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(jsstr))
		.WillOnce(testing::Return(expected.size() + 1));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(jsstr, testing::_, expected.size() + 1))
		.WillOnce(testing::Invoke([&expected](JSStringRef, char *buffer, size_t bufferSize) {
			return FakeWriteUTF8CString(expected, buffer, bufferSize);
		}));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(jsstr)).Times(1);

	std::string result = aamp_JSValueToJSONCString(context, value, NULL);
	EXPECT_TRUE(result.empty());
}

TEST_F(JsBindingTests, JsValueToJSONCStringReturnsEmptyOnConversionException)
{
	JSContextRef context = NULL;
	JSValueRef value = reinterpret_cast<JSValueRef>(0x3);
	JSValueRef exception = NULL;

	// JSValueCreateJSONString() returns NULL when the conversion raises an exception
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueCreateJSONString(context, value, 0, &exception))
		.WillOnce(testing::Return(nullptr));
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetMaximumUTF8CStringSize(testing::_)).Times(0);
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringGetUTF8CString(testing::_, testing::_, testing::_)).Times(0);
	EXPECT_CALL(*g_mockJavaScriptCore, JSStringRelease(testing::_)).Times(0);

	std::string result = aamp_JSValueToJSONCString(context, value, &exception);
	EXPECT_TRUE(result.empty());
}

TEST_F(JsBindingTests, RemoveEventListenerDetachesInFlightListener)
{
	PrivAAMPStruct_JS *obj = new PrivAAMPStruct_JS();
	obj->_aamp = playerInstanceAAMP;
	obj->_ctx = reinterpret_cast<JSGlobalContextRef>(0x1234);

	JSObjectRef jsCallback = reinterpret_cast<JSObjectRef>(0x5678);
	AAMP_JSEventListener::AddEventListener(obj, AAMP_EVENT_STATE_CHANGED, jsCallback);
	ASSERT_EQ(obj->_listeners.size(), 1u);

	// Stand-in for the shared_ptr copy AampEventManager::SendEventSync() would be
	// holding in its local dispatch list while removeEventListener() runs concurrently.
	auto inFlightRef = std::static_pointer_cast<AAMP_JSEventListener>(obj->_listeners.begin()->second);

	AAMP_JSEventListener::RemoveEventListener(obj, AAMP_EVENT_STATE_CHANGED, jsCallback);

	// Listener must be detached from its (about to be freed) owner even though a stray
	// reference is still outstanding.
	EXPECT_EQ(inFlightRef->p_obj, nullptr);
	EXPECT_EQ(inFlightRef->p_jsCallback, nullptr);

	// The JS object is torn down next, exactly as release()/GC finalization would do.
	delete obj;
	obj = nullptr;

	// inFlightRef is now the last owner. Destroying it must not touch the freed
	// PrivAAMPStruct_JS - if p_obj/p_jsCallback weren't nulled above, this deref's it.
	inFlightRef.reset();
}
