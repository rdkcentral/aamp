/*
 * If not stated otherwise in this file or this component's Licenses.txt file
 * the following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
#include "gtest/gtest.h"
#include "AampSmokeTestPlayer.h"
#include "TuneSmokeTest.h"
#include "ScriptedSmokeTest.h"
char gtestReportPath[] = "xml:/tmp/Gtest_Report/aamp_gtest_report.xml";

#include <future>
#include <unistd.h>

// Render frames in video plane - default behavior
#ifdef __APPLE__
#import <cocoa_window.h>
#endif

void usage(char *execName)
{
	printf("\nusage: %s [-v] [-t testname] [-h]\n", execName);
	printf("-v display video in window [MacOS only]\n");
	printf("-t testname string filter for test to run\n");
	printf("   e.g.: \"smokeTest\"\n");
	printf("         \"scripts\"\n");
	printf("-h this message\n\n");
}

int runCommand(bool videoFlag,  void *testArg)
{
	char *testName = (char *)testArg;
	SmokeTest lSmokeTest;
	bool disableGTESTs = false;
	
	if( (testName == NULL) || ((testName != NULL) && (strncasecmp(testName,"smoketest",9) == 0)) )
	{
		lSmokeTest.aampTune();
		lSmokeTest.getTuneDetails();
	}
	else if (strncasecmp(testName,"scripts",7) == 0)
	{
		// Just run the smoke test scripts on their own
		testing::GTEST_FLAG(filter) = "SmokeTestScripts*";
		printf("\n[SMOKETEST] Running only scripted smoke test\n");
	}
	else if (strlen(testName) > 0)
	{
		// Can probably improve this but if we have a test name ('aamp_smoketest -t <testname>')
		// that isn't 'smoketest' or 'scripts' then we'll assume it is a specific script and
		// just try to run that.
		printf("\n[SMOKETEST] Attempting to run script: '%s'\n", testName);
		ScriptedSmokeTest::testScript(testName);
		// This is really for dev purposes so disable gtests and run it directly.
		disableGTESTs = true;
	}
	else
	{
		printf("\n[SMOKETEST] unrecognized test name specified: '%s'\n", testName);
		exit(0);
	}
	
#ifdef __APPLE__
	if (videoFlag == true)
	{
		terminateCocoaWindow(); // allow main thread to end
	}
#endif

	return disableGTESTs ? 0 : RUN_ALL_TESTS();
}


GTEST_API_ int main(int argc, char* argv[])
{
	int ch;
	bool videoFlag = false;
	char *testName = NULL;
	
	::testing::GTEST_FLAG(output) = gtestReportPath;
	::testing::InitGoogleTest(&argc, argv);

	
	while ((ch = getopt(argc, argv, "vht:")) != -1)
	{
		switch (ch)
		{
			case 'v':
				videoFlag = true;
				break;
			case 't':
				testName = optarg;
				printf("[SMOKETEST] Test name specified: '%s'\n", testName);
				break;
			case 'h':
			default:
				usage(argv[0]);
				exit(0);
		}
	}
	argc -= optind;
	argv += optind;


	AampPlayer lAampPlayer;
	lAampPlayer.initPlayerLoop(0,NULL);
	
	// launch the test as a separate thread as createAndRunCocoaWindow is blocking call
	auto future = std::async(runCommand, videoFlag, testName);
	
#ifdef __APPLE__
	// Ideally this would be located in various *Tune() routines as each tune creates a new window.
	// By the end of these tests there are several old video windows open, but all will be deleted.
	if (videoFlag == true)
	{
		createAndRunCocoaWindow();
	}
#endif

	return future.get();
}
