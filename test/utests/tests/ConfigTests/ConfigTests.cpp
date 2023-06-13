#include <iostream>
#include <string>
#include <string.h>
#include <limits>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <AampConfig.cpp>
#ifndef UTEST
#error 	UTEST should be defined, so aamp.cfg & aampcfg.json can be created in the build folder & can be overwritten.
#endif


// Fakes to allow linkage
AampConfig *gpGlobalConfig=NULL;
AampLogManager *mLogObj=NULL;

const double minDouble = std::numeric_limits<double>::lowest();
const double maxDouble = std::numeric_limits<double>::max();
static bool CodecSupport = true;

TEST(_Config, configStringToBool)
{
	EXPECT_TRUE(ConfigLookup::ConfigStringValueToBool("True"));
	EXPECT_TRUE(ConfigLookup::ConfigStringValueToBool("1"));
	EXPECT_FALSE(ConfigLookup::ConfigStringValueToBool("false"));
	EXPECT_FALSE(ConfigLookup::ConfigStringValueToBool("0"));
	EXPECT_FALSE(ConfigLookup::ConfigStringValueToBool(nullptr));
}

TEST(_Config, Process)
{
	AampConfig aampConfig;
	ConfigLookup lookUp;
	std::string key("key");
	std::string emptyValue;
	std::string value("true");
	lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, key, value);
	
	ConfigLookupEntryBool LookupTableBool = mConfigLookupTableBool[0];
	lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, LookupTableBool.cmdString, value);
	size_t size = ARRAY_SIZE(mConfigLookupTableBool);
	unsigned long i;
	for(i = 0; i< size; i++)
	{
		ConfigLookupEntryBool LookupTableBool = mConfigLookupTableBool[i];
		//Use GetConfigValue to get initial value, Send empty value & check with GetConfigValue that it's changed
		AAMPConfigSettingBool eCfg = mConfigLookupTableBool[i].configEnum;
		bool b1 = aampConfig.GetConfigValue(eCfg);
		lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, LookupTableBool.cmdString, emptyValue);
		bool b2 = aampConfig.GetConfigValue(eCfg);
		EXPECT_NE(b1, b2);
		lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, LookupTableBool.cmdString, value);
		b2 = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(b2, true);
		EXPECT_EQ(i, LookupTableBool.configEnum);
	}
	size = ARRAY_SIZE(mConfigLookupTableInt);
	for(i = 0; i< size; i++)
	{
		AAMPConfigSettingInt eCfg = mConfigLookupTableInt[i].configEnum;
		auto cfgInfo = mConfigLookupTableInt[eCfg];
		auto range = mConfigValueValidRange[cfgInfo.validRange];
		//Test Process command by using it to set a valid value (minValue) & checking that it gets set.
		value = std::to_string(range.minValue);
		ConfigLookupEntryInt LookupTableInt = mConfigLookupTableInt[i];
		lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, LookupTableInt.cmdString, value);
		int result = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(result, range.minValue);
	}
	size = ARRAY_SIZE(mConfigLookupTableFloat);
	for(i = 0; i< size; i++)
	{
		AAMPConfigSettingFloat eCfg = mConfigLookupTableFloat[i].configEnum;
		auto cfgInfo = mConfigLookupTableFloat[eCfg];
		auto range = mConfigValueValidRange[cfgInfo.validRange];
		value = std::to_string(range.minValue);
		ConfigLookupEntryFloat LookupTableFloat = mConfigLookupTableFloat[i];
		lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, LookupTableFloat.cmdString, value);
		double result = aampConfig.GetConfigValue(eCfg);
		EXPECT_DOUBLE_EQ(result, range.minValue);
	}
	size = ARRAY_SIZE(mConfigLookupTableString);
	for(i = 0; i< size; i++)
	{
		value = "Test Value";
		ConfigLookupEntryString LookupTableString = mConfigLookupTableString[i];
		lookUp.Process( &aampConfig, AAMP_DEFAULT_SETTING, LookupTableString.cmdString, value);
		AAMPConfigSettingString eCfg = mConfigLookupTableString[i].configEnum;
		std::string result = aampConfig.GetConfigValue(eCfg);
		EXPECT_STREQ(result.c_str(), value.c_str());
	}

	// Same as above tests but call Process with a value set in customJson struct & test that it gets set.
	customJson custom;
	custom.config = mConfigLookupTableString[0].cmdString;
	custom.configValue = "JSON Test";
	AAMPConfigSettingString eCfg = mConfigLookupTableString[0].configEnum;
	lookUp.Process(&aampConfig, custom);
	std::string result = aampConfig.GetConfigValue(eCfg);
	EXPECT_STREQ(result.c_str(), custom.configValue.c_str()) << "Process  custom json for cmdString: " << custom.config << " enum: " << mConfigLookupTableString[0].configEnum << " failed";
}


//createObject & deleteObject are helper functions for ProcessJson
static cJSON* createObject(const char* name, const char* valueStr, int numVal = 0)
{
	cJSON* obj = new cJSON;
	memset(obj,0, sizeof(*obj));
	obj->string = new char[strlen(name)+1];
	strcpy(obj->string, name);
	if(valueStr)
	{
		obj->valuestring = new char[strlen(valueStr)+1];
		strcpy(obj->valuestring, valueStr);
	}
	obj->type = numVal;
	obj->valueint = numVal;
	obj->valuedouble = numVal;
	return obj;
}

static void deleteObject(cJSON* obj)
{
	delete [] obj->string;
	delete [] obj->valuestring;
	delete obj;
}

TEST(_Config, ProcessJson)
{
	AampConfig aampConfig;
	ConfigLookup lookUp;
	
	cJSON* customVal1 = createObject("any text", "valueString1");
	cJSON* customValB = createObject(mConfigLookupTableBool[0].cmdString, "valueStringB");
	cJSON* customValNull = createObject(mConfigLookupTableBool[ARRAY_SIZE(mConfigLookupTableBool)-1].cmdString, NULL);
	cJSON* customValI = createObject(mConfigLookupTableInt[0].cmdString, "valueStringI");
	cJSON* customValF = createObject(mConfigLookupTableFloat[0].cmdString, "valueStringF");
	cJSON* customValS = createObject(mConfigLookupTableString[0].cmdString, "valueStringS");
	
	customJson customValues;
	std::vector<struct customJson> vCustom;

	//Process calls cJSON_GetObjectItem which looks along a list of child objects. If the "string" matches a value from the
	//mConfigLookupTable then configValue should be set to the value passed in and appended to a list. If valuestring is null the item should not be appended.
	customVal1->child = customValB;	
	lookUp.Process(&aampConfig, customVal1, customValues, vCustom);
	EXPECT_STREQ("valueStringB", vCustom[0].configValue.c_str());
	customVal1->child = customValI;
	lookUp.Process(&aampConfig, customVal1, customValues, vCustom);
	EXPECT_STREQ("valueStringI", vCustom[1].configValue.c_str());
	customVal1->child = customValF;
	lookUp.Process(&aampConfig, customVal1, customValues, vCustom);
	customVal1->child = customValS;
	lookUp.Process(&aampConfig, customVal1, customValues, vCustom);

	EXPECT_STREQ("valueStringB", vCustom[0].configValue.c_str());
	EXPECT_STREQ("valueStringI", vCustom[1].configValue.c_str());
	EXPECT_STREQ("valueStringF", vCustom[2].configValue.c_str());
	EXPECT_STREQ("valueStringS", vCustom[3].configValue.c_str());
	
	customVal1->child = customValNull;
	auto size = vCustom.size();
	lookUp.Process(&aampConfig, customVal1, customValues, vCustom);
	EXPECT_EQ(size, vCustom.size());
	
	deleteObject(customVal1);
	deleteObject(customValB);
	deleteObject(customValI);
	deleteObject(customValF);
	deleteObject(customValS);
	deleteObject(customValNull);
}


TEST(_Config, ProcessJsonSearch)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	ConfigLookup lookUp;
	//Create an object based on cmd string & look it up. Check that "Process" can set a value.
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableBool); i++)
	{
		//#define cJSON_True   (1 << 1)
		cJSON* searchObj = createObject(mConfigLookupTableBool[i].cmdString, "test text", 2);
		lookUp.Process(&aampConfig, AAMP_DEFAULT_SETTING, searchObj);
		AAMPConfigSettingBool cfgEnum = mConfigLookupTableBool[i].configEnum;
		bool result = aampConfig.GetConfigValue(cfgEnum);
		EXPECT_EQ(result, true);
		deleteObject(searchObj);
	}
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableInt); i++)
	{
		AAMPConfigSettingInt cfgEnum = mConfigLookupTableInt[i].configEnum;
		auto cfgInfo = mConfigLookupTableInt[cfgEnum];
		auto range = mConfigValueValidRange[cfgInfo.validRange];
		cJSON* searchObj = createObject(mConfigLookupTableInt[i].cmdString, "test text", range.minValue);
		//Use "Process" to set a valid value and validate 
		lookUp.Process(&aampConfig, AAMP_DEFAULT_SETTING, searchObj);
		int result = aampConfig.GetConfigValue(cfgEnum);
		EXPECT_EQ(result, range.minValue) << "Failed to set minValue for " << cfgEnum;
		deleteObject(searchObj);
	}
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableFloat); i++)
	{
		AAMPConfigSettingFloat cfgEnum = mConfigLookupTableFloat[i].configEnum;
		auto cfgInfo = mConfigLookupTableFloat[cfgEnum];
		auto range = mConfigValueValidRange[cfgInfo.validRange];
		cJSON* searchObj = createObject(mConfigLookupTableFloat[i].cmdString, "test text", range.minValue);
		lookUp.Process(&aampConfig, AAMP_DEFAULT_SETTING, searchObj);
		double result = aampConfig.GetConfigValue(cfgEnum);
		EXPECT_EQ(result, range.minValue) << "Failed to set minValue for " << cfgEnum;
		deleteObject(searchObj);
	}
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableString); i++)
	{
		cJSON* searchObj = createObject(mConfigLookupTableString[i].cmdString, "test text");
		lookUp.Process(&aampConfig, AAMP_DEFAULT_SETTING, searchObj);
		AAMPConfigSettingString cfgEnum = mConfigLookupTableString[i].configEnum;
		std::string result = aampConfig.GetConfigValue(cfgEnum);
		EXPECT_STREQ(result.c_str(), "test text") << "Failed to set string for " << cfgEnum;
		deleteObject(searchObj);
	}
}

TEST(_Config, OperatorEqual)
{
	AampConfig aampConfigL, aampConfigR;
	aampConfigL.Initialize();
	aampConfigL = aampConfigR;
	//Now check L=R for all types

	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableBool); i++)
	{
		AAMPConfigSettingBool eCfg = mConfigLookupTableBool[i].configEnum;
		bool bResultL = aampConfigL.IsConfigSet(eCfg);
		bool bResultR = aampConfigR.IsConfigSet(eCfg);		
		EXPECT_EQ(bResultL, bResultR);
	}
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableString); i++)
	{
		AAMPConfigSettingString eCfg = mConfigLookupTableString[i].configEnum;
		std::string resultL = aampConfigL.GetConfigValue(eCfg);
		std::string resultR = aampConfigR.GetConfigValue(eCfg);		
		EXPECT_STREQ(resultL.c_str(), resultR.c_str());
	}
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableInt); i++)
	{
		AAMPConfigSettingInt eCfg = mConfigLookupTableInt[i].configEnum;
		int resultL = aampConfigL.GetConfigValue(eCfg);
		int resultR = aampConfigR.GetConfigValue(eCfg);		
		EXPECT_EQ(resultL, resultR);
	}
	for(auto i = 0; i < ARRAY_SIZE(mConfigLookupTableFloat); i++)
	{
		AAMPConfigSettingFloat eCfg = mConfigLookupTableFloat[i].configEnum;
		double resultL = aampConfigL.GetConfigValue(eCfg);
		double resultR = aampConfigR.GetConfigValue(eCfg);		
		EXPECT_DOUBLE_EQ(resultL, resultR);
	}
}

TEST(_Config, ReadDeviceCapability)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	//Cause IsCodecSupported to return true, so disabled config should be false
	CodecSupport = true;
	aampConfig.ReadDeviceCapability();
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableAC4),false);
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableAC3),false);
	CodecSupport = false;
	aampConfig.ReadDeviceCapability();
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableAC4),true);
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableAC3),true);
}

TEST(_Config, GetUserAgentString)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string str1 = aampConfig.GetUserAgentString();
	std::string str2 = aampConfig.GetConfigValue(eAAMPConfig_UserAgent);
	EXPECT_STREQ(str1.c_str(), str2.c_str());
}



static void testBoolSuccess(AampConfig& aampConfig, ConfigPriority owner)
{
	int bResult, bResult2;
	size_t MaxConfig = sizeof(mConfigLookupTableBool) / sizeof(mConfigLookupTableBool[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		AAMPConfigSettingBool eCfg = mConfigLookupTableBool[i].configEnum;
		//Get current setting & try & set to the opposite.
		bResult = aampConfig.IsConfigSet(eCfg);
		aampConfig.SetConfigValue(owner, eCfg, !bResult);
		bResult2 = aampConfig.IsConfigSet(eCfg);
		EXPECT_NE(bResult, bResult2) << mConfigLookupTableBool[i].cmdString << " failed to set to " << !bResult;
		//Set back to original state and verify it's what we set it to
		aampConfig.SetConfigValue(owner, eCfg, bResult);
		bResult2 = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(bResult, bResult2);
		aampConfig.GetConfigValue((AAMPConfigSettingBool)(MaxConfig+1)); //coverage
	}
}

static void testBoolFail(AampConfig& aampConfig, ConfigPriority owner)
{
	int bResult, bResult2;
	size_t MaxConfig = sizeof(mConfigLookupTableBool) / sizeof(mConfigLookupTableBool[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		AAMPConfigSettingBool eCfg = mConfigLookupTableBool[i].configEnum;
		//Get current setting & try & set to the opposite.
		bResult = aampConfig.IsConfigSet(eCfg);
		aampConfig.SetConfigValue(owner, eCfg, !bResult);
		bResult2 = aampConfig.IsConfigSet(eCfg);
		EXPECT_EQ(bResult, bResult2) << mConfigLookupTableBool[i].cmdString << " Was incorrectly able to set to " << !bResult;
	}
}

TEST(_Config, configSetGetBool)
{
	//Test with default owner then a higher priority owner which should succeed. Test with lower priority owner to ensure it isn't allowed to overwrite settings.
	AampConfig aampConfig;
	aampConfig.Initialize();
	AAMPConfigSettingBool cfg = mConfigLookupTableBool[0].configEnum;
	testBoolSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_DEFAULT_SETTING) << "Failed to set owner for: " << cfg;
	testBoolSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Failed to set owner for: " << cfg;
	testBoolFail(aampConfig, AAMP_TUNE_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Owner unexpectedly changed for: " << cfg;
}

static void testIntSuccess(AampConfig& aampConfig, ConfigPriority owner)
{
	size_t MaxConfig = sizeof(mConfigLookupTableInt) / sizeof(mConfigLookupTableInt[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		int iTest, iResult;
		AAMPConfigSettingInt eCfg = mConfigLookupTableInt[i].configEnum;

		ConfigValidRange validRange = mConfigLookupTableInt[i].validRange;
		size_t ranges = sizeof(mConfigValueValidRange)/sizeof(mConfigValueValidRange[0]);
		int minVal, maxVal;
		bool foundRange = false;
		for(int j=0; j<ranges; j++)
		{
			if(validRange == mConfigValueValidRange[j].type)
			{
				minVal = mConfigValueValidRange[j].minValue;
				maxVal = mConfigValueValidRange[j].maxValue;
				foundRange = true;
				break;
			}
		}
		EXPECT_EQ(foundRange, true);
		EXPECT_GE(maxVal, minVal);  //Max val should be >= min
		//Get current value before trying to set it.
		int initVal = aampConfig.GetConfigValue(eCfg);
		// Check it's between (or equal to) max & min values.
		EXPECT_GE(maxVal, initVal) << mConfigLookupTableInt[i].cmdString << " initVal: " << initVal << " should be <= Max: " << maxVal;
		EXPECT_LE(minVal, initVal) << mConfigLookupTableInt[i].cmdString << " initVal: " << initVal << " should be >= Min: " << minVal;
				
		//Try & set to a value > max.
		iTest = maxVal < INT_MAX ? maxVal + 1 : INT_MAX;
		aampConfig.SetConfigValue(owner, eCfg, iTest);
		iResult = aampConfig.GetConfigValue(eCfg);
		if(maxVal == INT_MAX)
		{
			EXPECT_EQ(iResult, INT_MAX)  << mConfigLookupTableInt[i].cmdString << " initVal: " << initVal; //Value should be INT_MAX
		}
		else
		{
			EXPECT_EQ(initVal, iResult)  << mConfigLookupTableInt[i].cmdString << " initVal: " << initVal; //Value should be unchanged from the start.
		}
				
		//Try & set to a value < min.
		initVal = aampConfig.GetConfigValue(eCfg);
		iTest = minVal > INT_MIN ? minVal - 1 : INT_MIN; 	
		aampConfig.SetConfigValue(owner, eCfg, iTest);
		iResult = aampConfig.GetConfigValue(eCfg);
		if(minVal == INT_MIN)
		{
			EXPECT_EQ(iResult, INT_MIN)  << mConfigLookupTableInt[i].cmdString << "initVal: " << initVal;
		}
		else
		{
			//Value should be unchanged from the start.
			EXPECT_EQ(initVal, iResult)  << mConfigLookupTableInt[i].cmdString << " initVal: " << initVal << "Tried to set to: " << iTest << "result: " << iResult;
		}

		//Verify we can set to a value between min and max.
		iTest = minVal == maxVal ? maxVal : minVal + 1;
		aampConfig.SetConfigValue(owner, eCfg, iTest);
		iResult = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(iTest, iResult) << mConfigLookupTableInt[i].cmdString << " result: " << iResult << " should be equal to test val: " << iTest;
		//Test return as a parameter.
		int iResult2 = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(iResult, iResult2) << mConfigLookupTableInt[i].cmdString << " result: " << iResult2 << " should be equal to test val: " << iTest;
		aampConfig.GetConfigValue((AAMPConfigSettingInt)(MaxConfig+1)); //coverage
	}
}

static void testIntFail(AampConfig& aampConfig, ConfigPriority owner)
{
	size_t MaxConfig = sizeof(mConfigLookupTableInt) / sizeof(mConfigLookupTableInt[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		int iTest, iResult;
		AAMPConfigSettingInt eCfg = mConfigLookupTableInt[i].configEnum;

		ConfigValidRange validRange = mConfigLookupTableInt[i].validRange;
		size_t ranges = sizeof(mConfigValueValidRange)/sizeof(mConfigValueValidRange[0]);
		int minVal, maxVal;
		bool foundRange = false;
		for(int j=0; j<ranges; j++)
		{
			if(validRange == mConfigValueValidRange[j].type)
			{
				minVal = mConfigValueValidRange[j].minValue;
				maxVal = mConfigValueValidRange[j].maxValue;
				foundRange = true;
				break;
			}
		}
		EXPECT_EQ(foundRange, true);
		//Get current value before trying to set it.
		int initVal = aampConfig.GetConfigValue(eCfg);
		iTest = minVal == maxVal ? maxVal : minVal + 1;
		aampConfig.SetConfigValue(owner, eCfg, iTest);
		//Check that we were unable to change the current value
		iResult = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(initVal, iResult) << mConfigLookupTableInt[i].cmdString << " result: " << iResult << " was incorrectly able to change: " << initVal;
	}
}

TEST(_Config, configSetGetInt)
{
	//Test with default owner then a higher priority owner which should succeed. Test with lower priority owner to ensure it isn't allowed to overwrite settings.
	AampConfig aampConfig;
	aampConfig.Initialize();
	AAMPConfigSettingInt cfg = mConfigLookupTableInt[0].configEnum;

	testIntSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_DEFAULT_SETTING) << "Failed to set owner for: " << cfg;
	testIntSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Failed to set owner for: " << cfg;
	testIntFail(aampConfig, AAMP_TUNE_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Owner unexpectedly changed for: " << cfg;
}

static void testFloatSuccess(AampConfig& aampConfig, ConfigPriority owner)
{
	size_t MaxConfig = sizeof(mConfigLookupTableFloat) / sizeof(mConfigLookupTableFloat[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		double dTest, dResult;
		AAMPConfigSettingFloat eCfg = mConfigLookupTableFloat[i].configEnum;
		
		ConfigValidRange validRange = mConfigLookupTableFloat[i].validRange;
		size_t ranges = sizeof(mConfigValueValidRange)/sizeof(mConfigValueValidRange[0]);
		double minVal, maxVal;
		bool foundRange = false;
		for(int j=0; j<ranges; j++)
		{
			if(validRange == mConfigValueValidRange[j].type)
			{
				minVal = mConfigValueValidRange[j].minValue;
				maxVal = mConfigValueValidRange[j].maxValue;
				foundRange = true;
				break;
			}
		}
		EXPECT_EQ(foundRange, true);
		EXPECT_GE(maxVal, minVal);  //Max val should be >= min
		//Get current value before trying to set it.
		double initVal = aampConfig.GetConfigValue(eCfg);
		// Check it's between (or equal to) max & min values.
		EXPECT_GE(maxVal, initVal) << mConfigLookupTableFloat[i].cmdString << " Init: " << initVal << " should be <= Max: " << maxVal;
		EXPECT_LE(minVal, initVal) << mConfigLookupTableFloat[i].cmdString << " Init: " << initVal << " should be >= Min: " << minVal;
				
		//Try & set to a value > max.
		initVal = aampConfig.GetConfigValue(eCfg);
		dTest = maxVal < maxDouble ? maxVal + 1 : maxDouble;
		aampConfig.SetConfigValue(owner, eCfg, dTest);
		dResult = aampConfig.GetConfigValue(eCfg);
		if(maxVal == maxDouble)
		{
			EXPECT_DOUBLE_EQ(dResult, maxDouble)  << mConfigLookupTableInt[i].cmdString << " Init: " << initVal;
		}
		else
		{
			//Value should be unchanged from the start.
			EXPECT_DOUBLE_EQ(initVal, dResult)  << mConfigLookupTableInt[i].cmdString << " Init: " << initVal << " should  be unchanged but is now: " << dResult; 
		}
				
		//Try & set to a value < min.
		initVal = aampConfig.GetConfigValue(eCfg);
		dTest = minVal > minDouble ? minVal - 1.0 : minDouble;
		aampConfig.SetConfigValue(owner, eCfg, dTest);
		dResult = aampConfig.GetConfigValue(eCfg);
		if(minVal == minDouble)
		{
			EXPECT_DOUBLE_EQ(dResult, minDouble)  << mConfigLookupTableFloat[i].cmdString << "Init: " << initVal;
		}
		else
		{
			//Value should be unchanged from the start.
			EXPECT_DOUBLE_EQ(initVal, dResult)  << mConfigLookupTableFloat[i].cmdString << " Init: " << initVal << "Tried to set to: " << dTest << "result: " << dResult << "min " << minVal;
		}

		//Verify we can set to a value between min and max.
		dTest = minVal/2 + maxVal/2;
		aampConfig.SetConfigValue(owner, eCfg, dTest);
		dResult = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(dTest, dResult) << mConfigLookupTableFloat[i].cmdString << " result: " << dResult << " should be equal to test val: " << dTest;
		//Test double returned as a parameter.
		double dResult2 = aampConfig.GetConfigValue(eCfg);
		EXPECT_DOUBLE_EQ(dTest, dResult2)  << mConfigLookupTableFloat[i].cmdString << " result: " << dResult2 << " should be equal to test val: " << dTest;
		aampConfig.GetConfigValue((AAMPConfigSettingFloat)(MaxConfig+1)); //coverage
	}
}

static void testFloatFail(AampConfig& aampConfig, ConfigPriority owner)
{
	size_t MaxConfig = sizeof(mConfigLookupTableFloat) / sizeof(mConfigLookupTableFloat[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		double dTest, dResult;
		AAMPConfigSettingFloat eCfg = mConfigLookupTableFloat[i].configEnum;
		
		ConfigValidRange validRange = mConfigLookupTableFloat[i].validRange;
		size_t ranges = sizeof(mConfigValueValidRange)/sizeof(mConfigValueValidRange[0]);
		double minVal, maxVal;
		bool foundRange = false;
		for(int j=0; j<ranges; j++)
		{
			if(validRange == mConfigValueValidRange[j].type)
			{
				minVal = mConfigValueValidRange[j].minValue;
				maxVal = mConfigValueValidRange[j].maxValue;
				foundRange = true;
				break;
			}
		}
		EXPECT_EQ(foundRange, true);

		//Check the result is still the initial value as we don't have permission to change the setting.
		double dInit = aampConfig.GetConfigValue(eCfg);
		//Choose a valid value, 1/4 of the way between min & max. Needs to be different to the value set by previous owner so we can tell if this owner is able to set it.
		dTest = minVal/2 + (minVal/2 + maxVal/2) / 2 ;
		aampConfig.SetConfigValue(owner, eCfg, dTest);
		dResult = aampConfig.GetConfigValue(eCfg);
		EXPECT_EQ(dInit, dResult) << mConfigLookupTableFloat[i].cmdString << " result: " << dResult << " should be equal to init val: " << dInit;
	}
}

TEST(_Config, configSetGetFloat)
{
	//Test with default owner then a higher priority owner which should succeed. Test with lower priority owner to ensure it isn't allowed to overwrite settings.
	AampConfig aampConfig;
	aampConfig.Initialize();
	AAMPConfigSettingFloat cfg = mConfigLookupTableFloat[0].configEnum;
	testFloatSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_DEFAULT_SETTING) << "Failed to set owner for: " << cfg;
	testFloatSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Failed to set owner for: " << cfg;
	testFloatFail(aampConfig, AAMP_TUNE_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Owner unexpectedly changed for: " << cfg;
}

static void testStringSuccess(AampConfig& aampConfig, ConfigPriority owner)
{
	size_t MaxConfig = sizeof(mConfigLookupTableString) / sizeof(mConfigLookupTableString[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		AAMPConfigSettingString eCfg = mConfigLookupTableString[i].configEnum;
		std::string testString("Test string");
		aampConfig.SetConfigValue(owner, eCfg, testString);
		//Test returned string
		std::string result = aampConfig.GetConfigValue(eCfg);
		EXPECT_STREQ(testString.c_str(), result.c_str()) << mConfigLookupTableString[i].cmdString << " result: " << result.c_str() << " should be equal to test val: " << testString.c_str();
		testString = "Different text";
		aampConfig.SetConfigValue(owner, eCfg, testString);
		result = aampConfig.GetConfigValue(eCfg);
		EXPECT_STREQ(testString.c_str(), result.c_str())  << mConfigLookupTableString[i].cmdString << " result: " << result.c_str() << " should be equal to test val: " << testString.c_str();
		aampConfig.GetConfigValue((AAMPConfigSettingString)(MaxConfig+1)); //coverage
	}
}

static void testStringFail(AampConfig& aampConfig, ConfigPriority owner)
{
	size_t MaxConfig = sizeof(mConfigLookupTableString) / sizeof(mConfigLookupTableString[0]);
	for(int i = 0; i < MaxConfig; i++)
	{
		AAMPConfigSettingString eCfg = mConfigLookupTableString[i].configEnum;
		std::string testString("Test fail string");
		aampConfig.SetConfigValue(owner, eCfg, testString);
		//Test returned string
		std::string result = aampConfig.GetConfigValue(eCfg);
		EXPECT_STRNE(testString.c_str(), result.c_str()) << mConfigLookupTableString[i].cmdString << " result: " << result.c_str() << " was incorrectly able to set: " << testString.c_str();
	}
}

TEST(_Config, configSetGetString)
{
	//Test with default owner then a higher priority owner which should succeed. Test with lower priority owner to ensure it isn't allowed to overwrite settings.
	AampConfig aampConfig;
	aampConfig.Initialize();
	AAMPConfigSettingString cfg = mConfigLookupTableString[0].configEnum;
	testStringSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_DEFAULT_SETTING) << "Failed to set owner for: " << cfg;
	testStringSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Failed to set owner for: " << cfg;
	testStringFail(aampConfig, AAMP_TUNE_SETTING);
	EXPECT_EQ(aampConfig.GetConfigOwner(cfg), AAMP_CUSTOM_DEV_CFG_SETTING) << "Owner unexpectedly changed for: " << cfg;
}

TEST(_Config, ProcessConfigTextBlankString)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr("");
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
}

//With no value supplied, the resulting value expected to be toggle/inverted of existing value
TEST(_Config, ProcessConfigTextNoValue)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr("debug= ");
	bool bResult = aampConfig.GetConfigValue(eAAMPConfig_DebugLogging);
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
	bool bResult2 = aampConfig.GetConfigValue(eAAMPConfig_DebugLogging);
	EXPECT_NE(bResult, bResult2);
}

TEST(_Config, ProcessConfigTextValidProperty)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr("debug=1");
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
	int configVal = aampConfig.GetConfigValue(eAAMPConfig_DebugLogging);
	EXPECT_EQ(configVal,1);
}

//With no value supplied, the resulting value expected to be toggle/inverted of existing value
TEST(_Config, ProcessConfigTextNoValue2)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr("debug");
	bool bResult = aampConfig.GetConfigValue(eAAMPConfig_DebugLogging);
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
	bool bResult2 = aampConfig.GetConfigValue(eAAMPConfig_DebugLogging);
	EXPECT_NE(bResult, bResult2);
}

TEST(_Config, ProcessConfigTextWhiteSpace)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr(" ");
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
}

TEST(_Config, ProcessConfigTextNewLine)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr("\n");
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
}

TEST(_Config, ProcessConfigTextNoKeyOnlyValue)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string trstr("=1");
	aampConfig.ProcessConfigText(trstr,AAMP_OPERATOR_SETTING);
}


//channel map test for ProcessConfigJson
const std::string chmap = 
R"(
{
   "info":true,
   "progress":true,
   "licenseServerUrl":"https://mds.ccp.xcal.tv/license",
   "chmap": 
    [
      {
        "name": "HBO",
        "url": "http://ccr.ipvod-t6.xcr.comcast.net/ipvod6/HCHD0000000001467873/movie/1619768135498/manifest.mpd"
        
      }
    
   ],
   "drmConfig":
      {
        "name": "customData",
        "url": "http://ccr.ipvod-t6.xcr.comcast.net/ipvod6/HCHD0000000001467873/movie/1619768135498/manifest.mpd"
      }
}
)";

//"Custom" test for ProcessConfigJson
const std::string custom = 
R"(
{
  "info":true,
  "progress":false,
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
        "appName":"Peacock",
        "playlistTimeout":"9",
        "progress":"true",
        "networkTimeout":"40"
     }

  ]
}
)";


TEST(_Config, ProcessConfigJson)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string url = "HBO";
	std::string appname = "appname";
	int playerId = 0;

	//Test Custom code
	//customFound should be false so this should fail
	EXPECT_EQ(aampConfig.CustomSearch(url, playerId, appname), false);
	cJSON *cfgdata = cJSON_Parse(custom.c_str());
	aampConfig.ProcessConfigJson(cfgdata, AAMP_DEV_CFG_SETTING);
	//This should have set customFound to true, so call CustomSearch here
	EXPECT_EQ(aampConfig.CustomSearch(url, playerId, appname), true);
	url = "";
	appname = "Peacock";
	EXPECT_EQ(aampConfig.CustomSearch(url, playerId, appname), true);
	playerId = 1;
	EXPECT_EQ(aampConfig.CustomSearch(url, playerId, appname), true);
		
	//Test channel map code
	cfgdata = cJSON_Parse(chmap.c_str());
	aampConfig.ProcessConfigJson(cfgdata, AAMP_DEV_CFG_SETTING);
}

TEST(_Config, GetAampConfigJSONStr)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	std::string str;
	aampConfig.GetAampConfigJSONStr(str);
}

TEST(_Config, ReadAampCfgJsonFile)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	FILE* fp = fopen("aampcfg.json", "w");
	if(fp)
	{
		fprintf(fp, "test\n");
		fclose(fp);
	}
	bool retVal = aampConfig.ReadAampCfgJsonFile();
	EXPECT_EQ(retVal, true);
}

TEST(_Config, ReadAampCfgTxtFile)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	FILE* fp = fopen("aamp.cfg", "w");
	if(fp)
	{
		fprintf(fp, "info\n");
		fprintf(fp, "progress\n");
		fprintf(fp, "trace=true\n");
		fprintf(fp, "export AAMP_DEBUG_FETCH_INJECT=true\n");
		fclose(fp);
	}	
	bool retVal = aampConfig.ReadAampCfgTxtFile();
	EXPECT_EQ(retVal, true);
}

TEST(_Config, ProcessConfigText)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	const char* result = aampConfig.GetChannelLicenseOverride("test");
	EXPECT_STREQ(result, NULL);
	std::string str = "*licenseServerUrl=test licence";
	aampConfig.ProcessConfigText(str, AAMP_DEV_CFG_SETTING);
	result = aampConfig.GetChannelOverride("HBOCM");
	EXPECT_STREQ(result, NULL);
	result = aampConfig.GetChannelLicenseOverride("HBOCM");
	EXPECT_STREQ(result, "test");
	// SERXIONE-2171
	std::string str2 = "donut=";
	aampConfig.ProcessConfigText(str2, AAMP_DEV_CFG_SETTING);
}


TEST(_Config, ReadOperatorConfiguration)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	setenv("AAMP_FORCE_AAC", "true", 1);
	setenv("AAMP_MIN_INIT_CACHE", "100", 1);
	setenv("CLIENT_SIDE_DAI", "true", 1);
	setenv("AAMP_ENABLE_WESTEROS_SINK", "1", 1);
	setenv("LOW_LATENCY_DASH", "1", 1);
	aampConfig.ReadOperatorConfiguration();
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_InitialBuffer),100);
}

TEST(_Config, ShowMiscConfiguration)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	aampConfig.ShowOperatorSetConfiguration();
	aampConfig.ShowAppSetConfiguration();
	aampConfig.ShowStreamSetConfiguration();
	aampConfig.ShowDefaultAampConfiguration();
	aampConfig.ShowDevCfgConfiguration();
	aampConfig.ShowAAMPConfiguration();
}

//Test RestoreConfiguration restores the value of previous owner for all different types
TEST(_Config, RestoreConfiguration)
{
	AampConfig aampConfig;
	aampConfig.Initialize();

	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_DisableEC3,true);
	aampConfig.SetConfigValue(AAMP_STREAM_SETTING, eAAMPConfig_DisableEC3, false); //- so last owner == AAMP_DEFAULT_SETTING
	aampConfig.RestoreConfiguration(AAMP_STREAM_SETTING, mLogObj); //current owner == AAMP_STREAM_SETTING, last owner != AAMP_STREAM_SETTING
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_DisableEC3), true) << "Failed to restore previous owner's setting";

	int iResult = aampConfig.GetConfigValue(eAAMPConfig_MaxBitrate);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_MaxBitrate, iResult-1);
	aampConfig.SetConfigValue(AAMP_STREAM_SETTING, eAAMPConfig_MaxBitrate, iResult-2);
	aampConfig.RestoreConfiguration(AAMP_STREAM_SETTING, mLogObj);
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_MaxBitrate), iResult-1) << "Failed to restore previous owner's setting";

	double dResult = aampConfig.GetConfigValue(eAAMPConfig_LiveOffset4K);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_LiveOffset4K, dResult-1);
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_LiveOffset4K), dResult-1);
	aampConfig.SetConfigValue(AAMP_STREAM_SETTING, eAAMPConfig_LiveOffset4K, dResult-2);
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_LiveOffset4K), dResult-2);
	aampConfig.RestoreConfiguration(AAMP_STREAM_SETTING, mLogObj);
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_LiveOffset4K), dResult-1) << "Failed to restore previous owner's setting";

	aampConfig.GetConfigValue(eAAMPConfig_LRHContentType);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_LRHContentType, "test1");
	aampConfig.SetConfigValue(AAMP_STREAM_SETTING, eAAMPConfig_LRHContentType, "test2");
	aampConfig.RestoreConfiguration(AAMP_STREAM_SETTING, mLogObj);
	EXPECT_EQ(aampConfig.GetConfigValue(eAAMPConfig_LRHContentType), "test1") << "Failed to restore previous owner's setting";

	//Coverage for ConfigureLogSettings
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_TraceLogging, false);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_DebugLogging, false);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_InfoLogging, false);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_WarnLogging, false);
	aampConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING, mLogObj);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_WarnLogging, true);
	aampConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING, mLogObj);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_InfoLogging, true);
	aampConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING, mLogObj);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_DebugLogging, true);
	aampConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING, mLogObj);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_TraceLogging, true);
	aampConfig.RestoreConfiguration(AAMP_CUSTOM_DEV_CFG_SETTING, mLogObj);
}

TEST(_Config, DoCustomSetting)
{
	AampConfig aampConfig;
	aampConfig.Initialize();
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_DisableEC3,true);
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableEC3), true);
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING,eAAMPConfig_DisableEC3, false);
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableEC3), false);
	//Make a config change that DoCustomSetting should ignore
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_StereoOnly,false);
	aampConfig.DoCustomSetting(AAMP_DEFAULT_SETTING);
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableEC3), false) << "DoCustomSetting changed a setting unexpectedly";
	//Make a config change that should cause DoCustomSetting to change a setting
	aampConfig.SetConfigValue(AAMP_DEFAULT_SETTING, eAAMPConfig_StereoOnly,true);
	aampConfig.DoCustomSetting(AAMP_DEFAULT_SETTING);
	EXPECT_EQ(aampConfig.IsConfigSet(eAAMPConfig_DisableEC3), true) << "DoCustomSetting failed to make a change";
	aampConfig.SetConfigValue(AAMP_APPLICATION_SETTING, eAAMPConfig_AuthToken, "test");
	aampConfig.DoCustomSetting(AAMP_DEFAULT_SETTING);
}

//For linkage. Also used by ReadDeviceCapability test.
bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	return CodecSupport;
}



