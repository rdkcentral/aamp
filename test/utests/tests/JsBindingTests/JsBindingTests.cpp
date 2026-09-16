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

using ::testing::_;

MockJavaScriptCore *g_mockJavaScriptCore;

class JsBindingTests : public ::testing::Test
{
protected:
	PlayerInstanceAAMP *playerInstanceAAMP;

	void SetUp() override
	{
		playerInstanceAAMP = new PlayerInstanceAAMP();
		g_mockJavaScriptCore = new MockJavaScriptCore();
	}

	void TearDown() override
	{
		g_mockJavaScriptCore = nullptr;
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
