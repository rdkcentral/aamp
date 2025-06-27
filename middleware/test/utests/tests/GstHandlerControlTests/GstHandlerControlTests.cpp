
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
#include <gtest/gtest.h>
#include "GstHandlerControl.h"

class FunctionalTests : public ::testing::Test {
public:
	GstHandlerControl* mControl;
	void SetUp() override
	{
		mControl = new GstHandlerControl;
	}

	void TearDown() override
	{
		delete mControl;
	}
};

TEST_F(FunctionalTests, instancesRunning)
{
	ASSERT_EQ(0, mControl->instancesRunning());

	std::vector<GstHandlerControl::ScopeHelper> helpers;
	constexpr size_t MAX_INSTANCES = 10;
	for(int i=1; i<=MAX_INSTANCES; i++)
	{
		helpers.push_back(mControl->getScopeHelper());
		ASSERT_EQ(i, mControl->instancesRunning());
	}

	while(helpers.size())
	{
	   helpers.pop_back();
	   ASSERT_EQ(helpers.size(), mControl->instancesRunning());
	}
}

TEST_F(FunctionalTests, returnStraightAway)
{
	ASSERT_FALSE(mControl->getScopeHelper().returnStraightAway());
	mControl->disable();
	ASSERT_TRUE(mControl->getScopeHelper().returnStraightAway());
	mControl->enable();
	ASSERT_FALSE(mControl->getScopeHelper().returnStraightAway());
	mControl->waitForDone(0, "test");
	ASSERT_TRUE(mControl->getScopeHelper().returnStraightAway());
	mControl->enable();
	ASSERT_FALSE(mControl->getScopeHelper().returnStraightAway());
}


TEST_F(FunctionalTests, waitForDoneSingle)
{
	{
		auto scopedHelper=mControl->getScopeHelper();
		ASSERT_FALSE(mControl->waitForDone(0, "test"));
	}

	ASSERT_TRUE(mControl->waitForDone(0, "test"));
}

TEST_F(FunctionalTests, waitForDoneMultiple)
{
	ASSERT_EQ(0, mControl->instancesRunning());
	ASSERT_TRUE(mControl->waitForDone(0, "test"));

	std::vector<GstHandlerControl::ScopeHelper> helpers;
	constexpr size_t MAX_INSTANCES = 10;
	for(int i=1; i<=MAX_INSTANCES; i++)
	{
		helpers.push_back(mControl->getScopeHelper());
		ASSERT_FALSE(mControl->waitForDone(0, "test"));
	}

	while(helpers.size())
	{
	   ASSERT_FALSE(mControl->waitForDone(0, "test"));
	   helpers.pop_back();
	}

	ASSERT_TRUE(mControl->waitForDone(0, "test"));
}

TEST_F(FunctionalTests, moveAssignToDefault)
{
	ASSERT_EQ(0, mControl->instancesRunning());
	{
		GstHandlerControl::ScopeHelper helperA = mControl->getScopeHelper();
		ASSERT_EQ(1, mControl->instancesRunning());
		{
			GstHandlerControl::ScopeHelper helperB;
			ASSERT_EQ(1, mControl->instancesRunning());
			ASSERT_TRUE(helperB.returnStraightAway());
			ASSERT_FALSE(helperA.returnStraightAway());

			helperB = std::move(helperA);
			ASSERT_EQ(1, mControl->instancesRunning());
			ASSERT_TRUE(helperA.returnStraightAway());
			ASSERT_FALSE(helperB.returnStraightAway());
		}
	}

	ASSERT_EQ(0, mControl->instancesRunning());
}

TEST_F(FunctionalTests, moveAssignOverwriteWithDefault)
{
	ASSERT_EQ(0, mControl->instancesRunning());

	GstHandlerControl::ScopeHelper helperC = mControl->getScopeHelper();
	ASSERT_EQ(1, mControl->instancesRunning());

	GstHandlerControl::ScopeHelper helperD;
	ASSERT_EQ(1, mControl->instancesRunning());
	ASSERT_TRUE(helperD.returnStraightAway());
	ASSERT_FALSE(helperC.returnStraightAway());

	helperC = std::move(helperD);
	ASSERT_EQ(0, mControl->instancesRunning());
	ASSERT_TRUE(helperD.returnStraightAway());
	ASSERT_TRUE(helperC.returnStraightAway());
}

TEST_F(FunctionalTests, moveAssignOverwriteValidWithValid)
{
	ASSERT_EQ(0, mControl->instancesRunning());
	{
		GstHandlerControl::ScopeHelper helperE = mControl->getScopeHelper();
		ASSERT_EQ(1, mControl->instancesRunning());
		ASSERT_FALSE(helperE.returnStraightAway());


		GstHandlerControl::ScopeHelper helperF = mControl->getScopeHelper();
		ASSERT_EQ(2, mControl->instancesRunning());
		ASSERT_FALSE(helperE.returnStraightAway());
		ASSERT_FALSE(helperF.returnStraightAway());

		helperE = std::move(helperF);
		ASSERT_EQ(1, mControl->instancesRunning());
		ASSERT_FALSE(helperE.returnStraightAway());
		ASSERT_TRUE(helperF.returnStraightAway());
	}

	ASSERT_EQ(0, mControl->instancesRunning());
}


TEST_F(FunctionalTests, moveConstruct)
{
	ASSERT_EQ(0, mControl->instancesRunning());

	GstHandlerControl::ScopeHelper helperA = mControl->getScopeHelper();
	ASSERT_EQ(1, mControl->instancesRunning());
	ASSERT_FALSE(helperA.returnStraightAway());

	{
		GstHandlerControl::ScopeHelper helperB{std::move(helperA)};
		ASSERT_EQ(1, mControl->instancesRunning());
		ASSERT_TRUE(helperA.returnStraightAway());
		ASSERT_FALSE(helperB.returnStraightAway());
	}

	ASSERT_EQ(0, mControl->instancesRunning());
	ASSERT_TRUE(helperA.returnStraightAway());
}

TEST_F(FunctionalTests, defaultConstruct)
{
	ASSERT_EQ(0, mControl->instancesRunning());

	{
		GstHandlerControl::ScopeHelper helper;
		ASSERT_EQ(0, mControl->instancesRunning());
	}

	ASSERT_EQ(0, mControl->instancesRunning());
}
