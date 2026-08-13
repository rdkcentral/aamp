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
#include "jsbindings-main.h"
#include "jsutils.h"
#include "PersistentWatermark.h"
#include "PersistentWatermarkDisplaySequencer.h"
#include "PersistentWatermarkEventHandler.h"
#include "PersistentWatermarkPluginAccess.h"
#include "PersistentWatermarkStorage.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockJavaScriptCore.h"

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
	AAMPPlayer_LoadJS(context);

	AAMPPlayer_UnloadJS(context);
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
