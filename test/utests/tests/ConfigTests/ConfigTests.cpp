#include <iostream>
#include <string>
#include <string.h>
#include <limits>

//include the google test dependencies
#include <gtest/gtest.h>

// unit under test
#include <AampConfig.cpp>

// Fakes to allow linkage
AampConfig *gpGlobalConfig=NULL;
AampLogManager *mLogObj=NULL;

const double minDouble = std::numeric_limits<double>::lowest();
const double maxDouble = std::numeric_limits<double>::max();

//Commented out until rdk-41036 changes are pushed.
static void testBoolSuccess(AampConfig& aampConfig, ConfigPriority owner)
{
	//AampConfig aampConfig;
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
	testBoolSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	testBoolSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	testBoolFail(aampConfig, AAMP_TUNE_SETTING);
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
	testIntSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	testIntSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	testIntFail(aampConfig, AAMP_TUNE_SETTING);
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
	testFloatSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	testFloatSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	testFloatFail(aampConfig, AAMP_TUNE_SETTING);
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
	testStringSuccess(aampConfig, AAMP_DEFAULT_SETTING);
	testStringSuccess(aampConfig, AAMP_CUSTOM_DEV_CFG_SETTING);
	testStringFail(aampConfig, AAMP_TUNE_SETTING);
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

//For linkage.
bool AAMPGstPlayer::IsCodecSupported(const std::string &codecName)
{
	return true;
}



