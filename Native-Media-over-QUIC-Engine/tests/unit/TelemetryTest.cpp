#include "common/Stats.h"
#include "common/Telemetry.h"

#include <gtest/gtest.h>

using nmqe::Json;

TEST(Telemetry, EscapesStrings)
{
	EXPECT_EQ(Json::escape("a\"b\\c\nd\x01"), "a\\\"b\\\\c\\nd\\u0001");
}

TEST(Telemetry, BuildsObjects)
{
	Json json;
	json.add("n", static_cast<int64_t>(-3)).add("s", "x").add("b", true).raw("o", "{}");
	EXPECT_EQ(json.object(), R"({"n":-3,"s":"x","b":true,"o":{}})");
}

TEST(Distribution, Percentiles)
{
	nmqe::Distribution d;
	for (int i = 1; i <= 101; ++i) d.add(i);
	EXPECT_DOUBLE_EQ(d.percentile(50), 51);
	EXPECT_DOUBLE_EQ(d.percentile(0), 1);
	EXPECT_DOUBLE_EQ(d.percentile(100), 101);
}
