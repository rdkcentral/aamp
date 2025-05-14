/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <iostream>
#include <string>
#include <string.h>
#include<limits>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <AampJsonObject.cpp>

// Fakes to allow linkage
AampConfig *gpGlobalConfig=NULL;

TEST(_JsonObject, AampJsonObject_Test)
{
	const char *json = R"({"strarray":["de","en","es"],"string":"apple","int":123,"obj":{"A":65,"B":66},"raw":"chunky","double":4.2,"objarray":[{"idx":0},{"idx":1},{"idx":2}]})";
	
	AampJsonObject *obj = new AampJsonObject(json);
	bool rc;
	std::vector<std::string> list;
	rc = obj->get("strarray", list );
	EXPECT_EQ( list.size(), 3);
	EXPECT_EQ( list[0].compare("de"), 0);
	EXPECT_EQ( list[1].compare("en"), 0);
	EXPECT_EQ( list[2].compare("es"), 0);

	std::string sval;
	rc = obj->get("string",sval);
	EXPECT_EQ( sval.compare("apple"), 0);

	int ival;
	rc = obj->get("int",ival);
	EXPECT_EQ( ival, 123 );

	AampJsonObject obj2;
	rc = obj->get("obj", obj2 );
	obj2.get("A",ival);
	EXPECT_EQ( ival, 65 );
	obj2.get("B",ival);
	EXPECT_EQ( ival, 66 );

	std::vector<uint8_t> values;
	obj->get( "raw", values, AampJsonObject::ENCODING_STRING );
	EXPECT_EQ( values.size(), 6 );
	EXPECT_EQ( values[0], 'c' );
	EXPECT_EQ( values[1], 'h' );
	EXPECT_EQ( values[2], 'u' );
	EXPECT_EQ( values[3], 'n' );
	EXPECT_EQ( values[4], 'k' );
	EXPECT_EQ( values[5], 'y' );

	double dval = 0.0;
	EXPECT_TRUE(obj->get("double", dval));
	EXPECT_EQ(dval, 4.2);

	std::vector<AampJsonObject> array;
	int idx = -1;
	EXPECT_TRUE(obj->get("objarray", array));
	ASSERT_EQ(array.size(), 3);
	EXPECT_TRUE(array[0].get("idx", idx));
	EXPECT_EQ(idx, 0);
	EXPECT_TRUE(array[1].get("idx", idx));
	EXPECT_EQ(idx, 1);
	EXPECT_TRUE(array[2].get("idx", idx));
	EXPECT_EQ(idx, 2);

	delete obj;
}


static void CustomArrayRead(cJSON *customArray)
{
	std::string keyname;
	customJson customValues;
	cJSON *customVal=NULL;
	cJSON *searchVal=NULL;

	int length = cJSON_GetArraySize(customArray);
	if(customArray != NULL)
	{
		for(int i = 0; i < length ; i++)
		{
			customVal = cJSON_GetArrayItem(customArray,i);
			if((searchVal = cJSON_GetObjectItem(customVal,"url")) != NULL)
			{
				EXPECT_STREQ(searchVal->valuestring, "HBO");
			}
			else if((searchVal = cJSON_GetObjectItem(customVal,"playerId")) != NULL)
			{
				EXPECT_STREQ(searchVal->valuestring, "0");
			}
			else if((searchVal = cJSON_GetObjectItem(customVal,"appName")) != NULL)
			{
				EXPECT_STREQ(searchVal->valuestring, "Sample App");
			}
		}
	}
}



TEST(_JsonObject, CustomTest)
{
const std::string custom = 
R"(
{
  "info":100,
  "progress":321,
  "Custom":
   [
      {
        "url":"HBO",
        "progress":"false",
        "networkTimeout":"25",
        "defaultBitrate":"2500000"

      },
      {
        "playerId":"0",
        "progress":"true",
        "info":"true",
        "networkTimeout":"32",
        "defaultBitrate":"320000"
      },
      {
        "appName":"Sample App",
        "playlistTimeout":"9",
        "progress":"true",
        "networkTimeout":"40"
     }

  ]
}
)";
	AampJsonObject obj(custom);
	obj.print_UnFormatted();
	std::string name = "name";
	std::string value = "value";
	EXPECT_EQ(obj.add(name, custom, AampJsonObject::ENCODING_BASE64), true);
	std::vector<uint8_t> values;
	EXPECT_EQ(obj.get( "name", values, AampJsonObject::ENCODING_BASE64), true);
	EXPECT_EQ( values.size(), custom.size());

	int ival;
	EXPECT_EQ(obj.get( "info", ival), true);
	EXPECT_EQ( ival, 100);
	EXPECT_EQ(obj.get( "progress", ival), true);
	EXPECT_EQ( ival, 321);
	cJSON *cfgdata = cJSON_Parse(custom.c_str());
	cJSON *customdata = cJSON_GetObjectItem(cfgdata, "Custom");
	CustomArrayRead(customdata);
}

TEST(_JsonObject, AddStrings)
{
	std::string nameString = R"({"name":"value"})";
	std::string expectedResult = R"({"name":"value","value list":["value 0","value 1","value 2"]})";
	AampJsonObject obj(nameString);
	std::vector<std::string> valueStrings;
	for(int i=0; i< 3; i++)
	{
		std::stringstream value;
		value << "value " << i;
		valueStrings.push_back(value.str());
	}
	EXPECT_EQ(obj.add("value list", valueStrings), true);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	EXPECT_STREQ(printStr.c_str(), expectedResult.c_str());
}

static void testAddUints(AampJsonObject::ENCODING encoding, const std::string expectedResult)
{
	std::string nameString = R"({"name": "value"})";
	AampJsonObject obj(nameString);
	std::vector<uint8_t> valueUints;
	for(int i=0; i < 5; i++)
	{
		valueUints.push_back('a'+ i);
	}
	EXPECT_EQ(obj.add("value list", valueUints, encoding), true);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	EXPECT_STREQ(printStr.c_str(), expectedResult.c_str());
}

TEST(_JsonObject, AddUints)
{
	testAddUints(AampJsonObject::ENCODING_STRING, R"({"name":"value","value list":"abcde"})");
	testAddUints(AampJsonObject::ENCODING_BASE64, R"({"name":"value","value list":"YWJjZGU="})");
	testAddUints(AampJsonObject::ENCODING_BASE64_URL, R"({"name":"value","value list":"YWJjZGU"})");
}

TEST(_JsonObject, AddString)
{
	AampJsonObject obj;
	std::string name = "name";
	std::string expectedResult = R"({"name":"test"})";
	std::vector<uint8_t> results;
	obj.add(name, "test");
	EXPECT_EQ(obj.get(name, results, AampJsonObject::ENCODING_BASE64), true);
	EXPECT_EQ(obj.isString(name), true);
	EXPECT_EQ(obj.get(name, results, AampJsonObject::ENCODING_BASE64_URL), true);
	std::string printStr = obj.print_UnFormatted();
	EXPECT_STREQ(printStr.c_str(), expectedResult.c_str());
	
	//Test failures
	EXPECT_EQ(obj.get(name, results, AampJsonObject::ENCODING(-1)), false);
	EXPECT_EQ(obj.isArray(name), false);
	EXPECT_EQ(obj.isNumber(name), false);
	EXPECT_EQ(obj.isObject(name), false);	
}


//Create a vector of AampJsonObjects and add this vector to another AampJsonObject
TEST(_JsonObject, AddJsonVector)
{
	std::string nameString = R"({"name": "value"})";
	std::string expectedResult =  R"({"name":"value","objects":[{"name0":"value0"},{"name1":"value1"},{"name2":"value2"},{"name3":"value3"},{"name4":"value4"}]})";
	AampJsonObject obj(nameString);
	std::vector<AampJsonObject*> valueJsonObjects;
	
	for(int i=0; i< 5; i++)
	{
		std::string nameString = R"({"nameX": "valueY"})";
		nameString[6]='0'+i;
		nameString[16]='0'+i;
		printf(":%s\n", nameString.c_str());
		AampJsonObject* newObj = new AampJsonObject(nameString);
		valueJsonObjects.push_back(newObj);
	}
	EXPECT_EQ(obj.add("objects", valueJsonObjects), true);
	std::string printString = obj.print_UnFormatted();
	printf("%s\n", printString.c_str());
	EXPECT_STREQ(printString.c_str(), expectedResult.c_str());
	AampJsonObject obj2;
	obj.get("name1", obj2);
	printString = obj2.print_UnFormatted();
	printf("%s\n", printString.c_str());
}

TEST(_JsonObject, AddJsonObject)
{
	std::string nameString = R"({"name": "value"})";
	std::string expectedResult = R"({"name":"value","name":{"nameJsonObj2":"val2"}})";
	AampJsonObject obj(nameString);
	AampJsonObject obj2;
	obj2.add("nameJsonObj2", "val2");
	obj.add("name", obj2);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	EXPECT_STREQ(printStr.c_str(), expectedResult.c_str());
}

TEST(_JsonObject, AddBool)
{
	AampJsonObject obj;
	obj.add("nameBool", true);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	std::string srch = "nameBool\":";
	size_t pos = printStr.find(srch);
	EXPECT_STREQ(printStr.substr(pos+srch.size()).c_str(), "true}");
}

TEST(_JsonObject, AddInt)
{
	AampJsonObject obj;
	obj.add("nameInt", 100);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	std::string srch = "nameInt\":";
	size_t pos = printStr.find(srch);
	EXPECT_STREQ(printStr.substr(pos+srch.size()).c_str(), "100}");
	int iVal;
	EXPECT_EQ(obj.get( "nameInt", iVal), true);
	EXPECT_EQ( iVal, 100);
}

TEST(_JsonObject, AddLong)
{
	AampJsonObject obj;
	obj.add("nameLong", 1000);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	std::string srch = "nameLong\":";
	size_t pos = printStr.find(srch);
	EXPECT_STREQ(printStr.substr(pos+srch.size()).c_str(), "1000}");
}

TEST(_JsonObject, AddFloat)
{
	AampJsonObject obj;
	obj.add("nameFloat", 123.45);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	std::string srch = "nameFloat\":";
	size_t pos = printStr.find(srch);
	EXPECT_STREQ(printStr.substr(pos+srch.size()).c_str(), "123.45}");
}

//Test adding multiple types to same JsonObject
TEST(_JsonObject, AddNumericTypes)
{
	AampJsonObject obj;
	
	obj.add("nameBool", true);
	obj.add("nameInt", 100);

	int ival;
	EXPECT_EQ(obj.get( "nameInt", ival), true);
	EXPECT_EQ( ival, 100);
	std::string printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
	
	double testDouble = std::numeric_limits<double>::max();
	obj.add("nameDouble", testDouble);
	
	long testLong = std::numeric_limits<long>::max();
	obj.add("nameLong", testLong);

	printStr = obj.print_UnFormatted();
	printf("%s\n", printStr.c_str());
}


TEST(_JsonObject, TestFailure)
{
	AampJsonObject obj;
	std::vector<uint8_t> results;
	std::string name;
	EXPECT_EQ(obj.get(name, results, AampJsonObject::ENCODING_BASE64), false);
	EXPECT_EQ(obj.get(name, results, AampJsonObject::ENCODING_BASE64_URL), false);
	EXPECT_EQ(obj.get(name, results, AampJsonObject::ENCODING(-1)), false);
	
	obj.print();
	std::vector<uint8_t> data;
	obj.print(data);
	EXPECT_EQ(obj.isArray(name), false);
	EXPECT_EQ(obj.isString(name), false);
	EXPECT_EQ(obj.isNumber(name), false);
	EXPECT_EQ(obj.isObject(name), false);
}












