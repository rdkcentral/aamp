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

#include "AampTime.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;

// Suite of microtests to validate operation of AampTime

const double oneNano = std::pow(10.0, -9);

class validateAampTimeOverloads : public ::testing::Test
{
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

TEST_F(validateAampTimeOverloads, testConstructor)
{
	AampTime a;
	AampTime b(100);
	AampTime c(1094.1);
	AampTicks dTicks(1094100, AampTime::TimeScale::milli);
	AampTime d(dTicks);

	// Verify that stored time is accurate to the expected time to within 1ns
	ASSERT_TRUE((fabs(a.inSeconds()) < oneNano));
	ASSERT_TRUE((fabs(b.inSeconds() - 100.0) < oneNano));
	ASSERT_TRUE((fabs(c.inSeconds() - 1094.1) < oneNano));
	ASSERT_TRUE((fabs(d.inSeconds() - 1094.1) < oneNano));
}

// The thud & blunder approach is because Google test does not support overloading operators cleanly.
// Attempte to pass them to the matchers end badly; could use delegation, but using ASSERT is less cumbersome

TEST_F(validateAampTimeOverloads, testAssignment)
{
	AampTime a(0.0);
	AampTime b(100.0);
	AampTime c(200.0);

	// Assign from object
	a = b;
	ASSERT_TRUE((a == b));

	// Assign from double
	a = 200.0;
	ASSERT_TRUE((a == c));

	ASSERT_TRUE((fabs(c.inSeconds() - 200.0) < oneNano));
}

TEST_F(validateAampTimeOverloads, testEquality)
{
	AampTime a;
	AampTime b(100);
	AampTime c(0.0);
	const AampTime d(0.0);
	const AampTime e(1.0);

	// Test equality between objects

	// Compare with self
	ASSERT_TRUE((a == a));
	// Compare with another object
	ASSERT_TRUE((a == c));
	// Compare with a const object
	ASSERT_TRUE((c == d));
	// Compare const object with another object
	ASSERT_TRUE((d == c));
	// Compare with another unequal object
	ASSERT_FALSE((a == b));
	// Verify != (inversion of ==)
	ASSERT_TRUE((a != b));
	// Compare const object with const object
	ASSERT_TRUE((d != e));

	// Test equality between object and double
	ASSERT_TRUE((a == 0.0));
	ASSERT_FALSE((a == 1.0));
	// Verify inverse operation
	ASSERT_TRUE((a != 1.0));
	ASSERT_FALSE((a != 0.0));
	// Test equality between const object and double
	ASSERT_TRUE((e == 1.0));

	// Test equality between double and object
	ASSERT_TRUE((0.0 == a));
	ASSERT_FALSE((1.0 == a));
	// Verify inverse operation
	ASSERT_TRUE((1.0 != a));
	ASSERT_FALSE((0.0 != a));
	// Test equality between double & const object
	ASSERT_TRUE((1.0 == e));
}

TEST_F(validateAampTimeOverloads, testNegation)
{
	AampTime a(1.0);
	AampTime b(-1.0);

	ASSERT_TRUE((-a == -1.0));
	ASSERT_TRUE((a == -b));

	a = 0.0;
	ASSERT_TRUE((-a == 0.0));
}

TEST_F(validateAampTimeOverloads, testComparisons)
{
	AampTime a(100.0);
	AampTime b(100.0);
	AampTime c(200.0);
	const AampTime d(300.0);
	AampTime e(0.0);

	// Verify self is neither > nor < self
	ASSERT_FALSE((a > a));
	ASSERT_FALSE((a < a));

	// Verify object > other object
	ASSERT_TRUE((c > a));
	// Verify inverse operation
	ASSERT_TRUE((a < c));
	// Verify const object > object
	ASSERT_TRUE((d > c));
	// Verify inverse operation (object < const object)
	ASSERT_TRUE((c < d));

	// Test object against double
	ASSERT_FALSE((a > 100.0));
	ASSERT_FALSE((a < 100.0));
	ASSERT_TRUE((a > 0.0));
	ASSERT_TRUE((a < 150.0));
	// Test const object
	ASSERT_TRUE((d > 0.0));
	// ASSERT_TRUE((0.0 < d));  // Not implemented

	// Compare objects with other objects
	ASSERT_TRUE((a <= b));
	ASSERT_TRUE((a >= b));
	ASSERT_TRUE((a <= c));
	ASSERT_TRUE((c >= a));
	ASSERT_FALSE((c <= a));
	ASSERT_FALSE((a >= c));
	// Test using const object as both lvalue and rvalue
	ASSERT_TRUE((a <= d));
	ASSERT_TRUE((d >= a));

	// Comparisons with 0
	ASSERT_FALSE((e > 0.0));
	ASSERT_FALSE((e < 0.0));
	ASSERT_TRUE((e >= 0.0));
	ASSERT_TRUE((e <= 0.0));
}


TEST_F(validateAampTimeOverloads, testAddition)
{
	AampTime a(0);
	AampTime b(10.0);
	const AampTime c(20.0);
	const double d(5.0);

	ASSERT_TRUE((a != b));

	// Object lvalue, double rvalue
	a = b + 10;
	ASSERT_TRUE((a == 20.0));

	a = 0.0;
	// Double rvalue
	a += 10.0;
	ASSERT_TRUE((a == 10.0));

	// Object lvalue, object rvalue
	a = a + b;
	ASSERT_TRUE((a == 20.0));

	// Object rvalue
	a += b;
	ASSERT_TRUE((a == 30.0));

	// Addition with const object as both lvalue and rvalue in overload
	a = c + 10.0;
	ASSERT_TRUE((a == 30.0));
	a = 10.0 + c;
	ASSERT_TRUE((a == 30.0));
	a += c;
	ASSERT_TRUE((a == 50.0));

	// Object lvalue, const double rvalue
	a = b + d;
	ASSERT_TRUE((a == 15.0));

	// Const object lvalue, const double rvalue
	a = c + d;
	ASSERT_TRUE((a == 25.0));
}

TEST_F(validateAampTimeOverloads, testSubtraction)
{
	AampTime a(10.0);
	AampTime b(30.0);
	const AampTime c(10);
	const double d(5.0);

	// Object lvalue, double rvalue
	a = b - 10.0;
	ASSERT_TRUE((a == 20.0));

	a = b;
	// Double rvalue
	a -= 10.0;
	ASSERT_TRUE((a == 20.0));

	// Const object lvalue, object rvalue
	a = c - b;
	ASSERT_TRUE((a == -20.0));

	// Object lvalue, const object rvalue
	a = b - c;
	ASSERT_TRUE((a == 20.0));

	// Double lvalue, const object rvalue
	a = 20.0 - c;
	ASSERT_TRUE((a == 10.0));

	// Object lvalue, const double rvalue
	a = b - d;
	ASSERT_TRUE((a == 25.0));

	// Const double lvalue, object rvalue
	a = d - b;
	ASSERT_TRUE((a == -25.0));

	// Const object lvalue, const double rvalue
	a = c - d;
	ASSERT_TRUE((a == 5.0));

	// Const double lvalue, const object rvalue
	a = d - c;
	ASSERT_TRUE((a == -5.0));

	// Const double rvalue
	a-=d;
	ASSERT_TRUE((a == -10.0));
}

TEST_F(validateAampTimeOverloads, testDivision)
{
	AampTime a(10.0);
	const AampTime b(20.0);
	const double c(2.0);

	// Object lvalue, double rvalue
	a = a / 2.0;
	ASSERT_TRUE((a == 5.0));

	// Const object lvalue, double rvalue
	a = b / 2.0;
	ASSERT_TRUE((a == 10.0));

	// Object lvalue, const double rvalue
	a = a / c;
	ASSERT_TRUE((a == 5.0));

	// Const object lvalue, const double rvalue
	a = b / c;
	ASSERT_TRUE((a == 10.0));

	// double lvalue, object rvalue not implemented
}

TEST_F(validateAampTimeOverloads, testMultiplication)
{
	AampTime a(10.0);
	const AampTime b(20.0);
	const double c(2.0);

	// Object lvalue, double rvalue
	a = a * 2.0;
	ASSERT_TRUE((a == 20.0));

	// Demonstrate type promotion works
	a = a * 3;
	ASSERT_TRUE((a == 60));

	// Const object lvalue, double rvalue
	a = b * 2.0;
	ASSERT_TRUE((a == 40.0));

	// Const object lvalue, const double rvalue
	a = b * c;
	ASSERT_TRUE((a == 40.0));

	// double lvalue, object rvalue not implemented
}

TEST_F(validateAampTimeOverloads, testIntegerHelpers)
{
	AampTime a(2.4);
	AampTime b(1.9999);
	AampTime c(0.1);
	AampTime d{0.0001};

	ASSERT_EQ(a.seconds(), 2);
	ASSERT_EQ(a.milliseconds(), 2400);
	ASSERT_EQ(b.seconds(), 1);
	ASSERT_EQ(b.milliseconds(), 1999);
	ASSERT_EQ(c.seconds(), 0);
	ASSERT_EQ(c.milliseconds(), 100);
	ASSERT_EQ(d.seconds(), 0);
	ASSERT_EQ(d.milliseconds(), 0);
	ASSERT_EQ(a.nearestSecond(), 2);
	ASSERT_EQ(b.nearestSecond(), 2);
	ASSERT_EQ(c.nearestSecond(), 0);
}


TEST_F(validateAampTimeOverloads, testCasting)
{
	AampTime a{2.4};

	ASSERT_DOUBLE_EQ((double)a, 2.4);
	ASSERT_EQ((int64_t)a, 2);
}

/**
 * @brief Test case for AampTicks::inMilli
 */
TEST_F(validateAampTimeOverloads, AampTicksInMilli)
{
	AampTicks ticks(5000, 1000); // 5000 ticks with a timescale of 1000
	EXPECT_EQ(ticks.inMilli(), 5000); // 5000 milliseconds
}
