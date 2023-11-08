/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2023 RDK Management
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

#include <gtest/gtest.h>
#include "AampEventManager.h"
#include "AampEventListener.h"

#include "AampEvent.h"
using namespace testing;
using namespace std;

AampConfig *gpGlobalConfig{nullptr};
        
class AampEventManagerTest : public Test {
protected:
    void SetUp() override {
    }
    void TearDown() override {
    delete handler;
    handler = nullptr;
    }
public:
	AampLogManager *mLogObj = new AampLogManager();
	AampEventManager *handler = new AampEventManager(mLogObj);
	EventListener* eventListener{nullptr};
};

TEST_F(AampEventManagerTest, IsEventListenerAvailableTest)
{	
	//Arrange: varible declaration
	for(int i= AAMP_EVENT_ALL_EVENTS ; i<= AAMP_MAX_NUM_EVENTS ;i++)
	{
	//Act: call the IsEventListenerAvailable function
	bool val = handler->IsEventListenerAvailable(AAMPEventType(i));
	// Assert:check for the value
	EXPECT_FALSE(val);
	}
}

TEST_F(AampEventManagerTest, SetFakeTuneFlagTest)
{
	//Arrange:variable declaration
	bool FakeTune=TRUE;
	// Act:call the SetFakeTuneFlag function
	handler->SetFakeTuneFlag(FakeTune);
}

TEST_F(AampEventManagerTest, SetAsyncTuneStateTest)
{
	// Act: call the SetAsyncTuneState function
	handler->SetAsyncTuneState(true);
	handler->SetAsyncTuneState(false);
}

TEST_F(AampEventManagerTest, SetPlayerStateTest)
{
	//Arrange: varible declaration
	for(int i= eSTATE_IDLE ; i<= eSTATE_BLOCKED ;i++)
	{
	//Act: call the SetPlayerState function
	handler->SetPlayerState(PrivAAMPState(i));
	// Assert:check for the equality
	EXPECT_EQ(i,i);
	}
}

TEST_F(AampEventManagerTest,AddListenerForAllEventsTest)
{
	//Arrange:declare the variable with size
	handler->AddListenerForAllEvents(eventListener);
}

TEST_F(AampEventManagerTest,AddEventListenerTest)
{
	handler->AddEventListener(AAMP_EVENT_ALL_EVENTS,eventListener);
}

TEST_F(AampEventManagerTest, RemoveListenerForAllEventsTest)
{
	//Act: call the removeEventlistener function
	handler->RemoveListenerForAllEvents(eventListener);
}

TEST_F(AampEventManagerTest,FlushPendingEventsTest)
{

    //Arrange:declare the variable with size
    handler->AddListenerForAllEvents(eventListener);

    for(int i= AAMP_EVENT_ALL_EVENTS ; i< AAMP_MAX_NUM_EVENTS ;i++)
    {
    handler->AddEventListener(AAMPEventType(i),eventListener);
    }
    // Act:call the FlushPendingEvents
    handler->FlushPendingEvents();
}


TEST_F(AampEventManagerTest,AddEventListenerTest_1)
{
    for(int i= AAMP_EVENT_ALL_EVENTS ; i< AAMP_MAX_NUM_EVENTS ;i++)
    {
    handler->AddEventListener(AAMPEventType(i),eventListener);
    }
}

TEST_F(AampEventManagerTest, SendEventTest_1)
{
    //Arrange:variable declaration
    AAMPEventPtr evntPtr= std::make_shared<AAMPEventObject>(AAMP_EVENT_EOS);
    for(int i= AAMP_EVENT_DEFAULT_MODE ; i<= AAMP_EVENT_ASYNC_MODE ;i++)
    {
    // Act: call the SendEvent function
    handler->SendEvent(evntPtr,AAMPEventMode(i));
    }
}

TEST_F(AampEventManagerTest, RemoveListenerForAllEventsTest_1)
{
    //Act: call the removeEventlistener function
    handler->RemoveListenerForAllEvents(eventListener);

    for(int i= AAMP_EVENT_ALL_EVENTS ; i< AAMP_MAX_NUM_EVENTS ;i++)
    {
    //Act: call the removeEventlistener function
    handler->RemoveEventListener(AAMPEventType(i),eventListener);
    }
}

TEST_F(AampEventManagerTest, IsSpecificEventListenerAvailableTest_1)
{
    //Arrange: varible declaration
    for(int i= AAMP_EVENT_ALL_EVENTS ; i<= AAMP_MAX_NUM_EVENTS ;i++)
    {
    //Act: call the IsSpecificEventListenerAvailable function
    bool val = handler->IsSpecificEventListenerAvailable(AAMPEventType(i));
    // Assert:check for the bool value
    EXPECT_FALSE(val);
    }
}

