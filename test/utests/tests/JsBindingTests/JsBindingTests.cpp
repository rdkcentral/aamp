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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

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

TEST_F(JsBindingTests, EventOnDetachedListenerIsNoOp)
{
	PrivAAMPStruct_JS *obj = new PrivAAMPStruct_JS();
	obj->_aamp = playerInstanceAAMP;
	obj->_ctx = reinterpret_cast<JSGlobalContextRef>(0x1234);

	JSObjectRef jsCallback = reinterpret_cast<JSObjectRef>(0x5678);
	AAMP_JSEventListener::AddEventListener(obj, AAMP_EVENT_STATE_CHANGED, jsCallback);
	ASSERT_EQ(obj->_listeners.size(), 1u);
	// Mirrors the shared_ptr copy SendEventSync() holds for the duration of dispatch.
	auto inFlightRef = std::static_pointer_cast<AAMP_JSEventListener>(obj->_listeners.begin()->second);

	// release()/GC finalization of the JS player object.
	AAMP_JSEventListener::RemoveAllEventListener(obj);
	delete obj;
	obj = nullptr;

	// An event already picked up for dispatch is delivered after teardown. It must be
	// dropped without calling into the freed JS context.
	EXPECT_CALL(*g_mockJavaScriptCore, JSObjectCallAsFunction(_,_,_,_,_,_)).Times(0);
	inFlightRef->Event(std::make_shared<StateChangedEvent>(eSTATE_PLAYING, ""));

	inFlightRef.reset();
}

TEST_F(JsBindingTests, TeardownDuringEventDispatchDoesNotTouchOwner)
{
	JSGlobalContextRef ctx = reinterpret_cast<JSGlobalContextRef>(0x1234);
	JSObjectRef jsCallback = reinterpret_cast<JSObjectRef>(0x5678);
	std::atomic<int> callbackProtectCount{0};
	std::atomic<int> callbackUnprotectCount{0};

	EXPECT_CALL(*g_mockJavaScriptCore, JSValueProtect(_, _)).Times(testing::AnyNumber());
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueUnprotect(_, _)).Times(testing::AnyNumber());
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueProtect(ctx, jsCallback))
		.WillRepeatedly(testing::InvokeWithoutArgs([&]() { callbackProtectCount++; }));
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueUnprotect(ctx, jsCallback))
		.WillRepeatedly(testing::InvokeWithoutArgs([&]() { callbackUnprotectCount++; }));

	PrivAAMPStruct_JS *obj = new PrivAAMPStruct_JS();
	obj->_aamp = playerInstanceAAMP;
	obj->_ctx = ctx;

	AAMP_JSEventListener::AddEventListener(obj, AAMP_EVENT_STATE_CHANGED, jsCallback);
	ASSERT_EQ(obj->_listeners.size(), 1u);
	EXPECT_EQ(callbackProtectCount, 1);
	auto inFlightRef = std::static_pointer_cast<AAMP_JSEventListener>(obj->_listeners.begin()->second);

	std::mutex callbackMutex;
	std::condition_variable callbackCondition;
	bool callbackEntered = false;
	bool releaseCallback = false;

	EXPECT_CALL(*g_mockJavaScriptCore, JSObjectCallAsFunction(_, jsCallback, _, _, _, _))
		.WillOnce(testing::InvokeWithoutArgs([&]() -> JSValueRef {
			std::unique_lock<std::mutex> lock(callbackMutex);
			callbackEntered = true;
			callbackCondition.notify_one();
			// Bounded so that teardown blocking behind dispatch fails instead of hanging.
			bool released = callbackCondition.wait_for(lock, std::chrono::seconds(5), [&]() {
				return releaseCallback;
			});
			EXPECT_TRUE(released) << "Teardown blocked behind in-flight event dispatch";
			return nullptr;
		}));

	std::thread dispatchThread([&]() {
		inFlightRef->Event(std::make_shared<StateChangedEvent>(eSTATE_PLAYING, ""));
	});

	{
		std::unique_lock<std::mutex> lock(callbackMutex);
		bool entered = callbackCondition.wait_for(lock, std::chrono::seconds(5), [&]() {
			return callbackEntered;
		});
		EXPECT_TRUE(entered);
		if (!entered)
		{
			releaseCallback = true;
			lock.unlock();
			callbackCondition.notify_one();
			dispatchThread.join();
			inFlightRef.reset();
			delete obj;
			return;
		}
	}

	// Remove and destroy the owner while the event callback is still in flight.
	AAMP_JSEventListener::RemoveAllEventListener(obj);
	delete obj;
	obj = nullptr;

	// The callback is still executing, so removal must not release it.
	EXPECT_EQ(callbackUnprotectCount, 0);

	{
		std::lock_guard<std::mutex> lock(callbackMutex);
		releaseCallback = true;
	}
	callbackCondition.notify_one();
	dispatchThread.join();

	// Finishing the in-flight dispatch releases the callback exactly once...
	EXPECT_EQ(callbackUnprotectCount, 1);

	// ...and dropping the last listener reference must not release it again.
	inFlightRef.reset();
	EXPECT_EQ(callbackUnprotectCount, 1);
	EXPECT_EQ(callbackProtectCount, 1);
}

// Building the JS event object allocates on the JS heap, which can run GC and finalize
// the owning player (RemoveAllEventListener() followed by delete). That must neither
// deadlock on listener state nor leave event construction touching the freed owner.
TEST_F(JsBindingTests, OwnerFinalizedDuringEventConstruction)
{
	PrivAAMPStruct_JS *obj = new PrivAAMPStruct_JS();
	obj->_aamp = playerInstanceAAMP;
	obj->_ctx = reinterpret_cast<JSGlobalContextRef>(0x1234);

	JSObjectRef jsCallback = reinterpret_cast<JSObjectRef>(0x5678);
	AAMP_JSEventListener::AddEventListener(obj, AAMP_EVENT_STATE_CHANGED, jsCallback);
	ASSERT_EQ(obj->_listeners.size(), 1u);
	auto inFlightRef = std::static_pointer_cast<AAMP_JSEventListener>(obj->_listeners.begin()->second);

	std::future<void> finalizer;
	EXPECT_CALL(*g_mockJavaScriptCore, JSObjectSetProperty(_, _, _, _, _, _))
		.WillOnce(testing::InvokeWithoutArgs([&]() {
			// Finalize from another thread so a listener lock held across event
			// construction shows up as a timeout rather than a self-deadlock.
			finalizer = std::async(std::launch::async, [obj]() {
				AAMP_JSEventListener::RemoveAllEventListener(obj);
			});
			bool finalized = (finalizer.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
			EXPECT_TRUE(finalized) << "Owner teardown blocked during event construction";
			if (finalized)
			{
				delete obj;
				obj = nullptr;
			}
		}))
		.WillRepeatedly(testing::Return());
	EXPECT_CALL(*g_mockJavaScriptCore, JSObjectCallAsFunction(_, jsCallback, _, _, _, _)).Times(testing::AtMost(1));

	inFlightRef->Event(std::make_shared<StateChangedEvent>(eSTATE_PLAYING, ""));

	ASSERT_TRUE(finalizer.valid());
	finalizer.wait();
	EXPECT_EQ(inFlightRef->p_obj, nullptr);
	delete obj;
	inFlightRef.reset();
}

// Owner that serves a promise callback for AAMP_EVENT_AD_RESOLVED, as the real
// AAMPMediaPlayer JS object does.
struct AdPromiseOwner : public PrivAAMPStruct_JS
{
	JSObjectRef promiseCallback = nullptr;
	int removeCallbackCount = 0;
	JSObjectRef getCallbackForAdId(std::string id) override { return promiseCallback; }
	void removeCallbackForAdId(std::string id) override { removeCallbackCount++; }
};

TEST_F(JsBindingTests, AdResolvedPromiseHandlerCanReleasePlayer)
{
	JSGlobalContextRef ctx = reinterpret_cast<JSGlobalContextRef>(0x1234);
	JSObjectRef promiseCallback = reinterpret_cast<JSObjectRef>(0x5678);
	AdPromiseOwner *obj = new AdPromiseOwner();
	obj->_aamp = playerInstanceAAMP;
	obj->_ctx = ctx;
	obj->promiseCallback = promiseCallback;

	// Promise-style listener: no JS callback, dispatched through getCallbackForAdId().
	AAMP_JSEventListener::AddEventListener(obj, AAMP_EVENT_AD_RESOLVED, NULL);
	ASSERT_EQ(obj->_listeners.size(), 1u);
	// Mirrors the shared_ptr copy SendEventSync() holds for the duration of dispatch.
	auto inFlightRef = std::static_pointer_cast<AAMP_JSEventListener>(obj->_listeners.begin()->second);

	// The owner drops its reference to the single-use promise callback before dispatch,
	// so the listener must keep it protected until the call has returned.
	testing::Sequence promiseLifetime;
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueProtect(_, _)).Times(testing::AnyNumber());
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueUnprotect(_, _)).Times(testing::AnyNumber());
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueProtect(ctx, promiseCallback))
		.Times(1)
		.InSequence(promiseLifetime);
	// The JS promise handler calls player.release(), which detaches every listener.
	EXPECT_CALL(*g_mockJavaScriptCore, JSObjectCallAsFunction(_, promiseCallback, _, _, _, _))
		.InSequence(promiseLifetime)
		.WillOnce(testing::InvokeWithoutArgs([obj]() -> JSValueRef {
			EXPECT_EQ(obj->removeCallbackCount, 1);
			AAMP_JSEventListener::RemoveAllEventListener(obj);
			return nullptr;
		}));
	EXPECT_CALL(*g_mockJavaScriptCore, JSValueUnprotect(ctx, promiseCallback))
		.Times(1)
		.InSequence(promiseLifetime);

	// Without the p_obj re-check in Event(), this dereferences NULL after the handler returns.
	inFlightRef->Event(std::make_shared<AdResolvedEvent>(true, "ad1", 0, 30000, "", "", ""));

	EXPECT_EQ(inFlightRef->p_obj, nullptr);
	EXPECT_EQ(obj->removeCallbackCount, 1);

	delete obj;
	inFlightRef.reset();
}
