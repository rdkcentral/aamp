#!/bin/bash
# This script will build and run microtests.
# Use option: -c to additionally build coverage tests
# Use option: -h to halt coverage tests on error

# If a test crashes or has AS trap, provide an error test report
error_report() 
{
cat << EOF > test_details.json
{
  "tests": 1,
  "failures": 0,
  "disabled": 0,
  "errors": 1,
  "timestamp": "`date`",
  "time": "0s",
  "name": "AllTests",
  "testsuites": [
    {
      "name": "$1",
      "tests": 1,
      "failures": 0,
      "disabled": 0,
      "errors": 1
    }
  ]
}
EOF
}

# RDKAAMP-884 "corrupt arc tag"
find . -name "*.gcda" -print0 | xargs -0 rm

build_coverage=0
halt_on_error=0

while getopts "ceh" opt; do
  case ${opt} in
    c ) echo Do build coverage
        build_coverage=1
      ;;
    e ) echo RDK-E build
        rdke_build=1
      ;;
    h ) echo Halt on error
        halt_on_error=1
      ;;
    * )
      ;;
  esac
done

#Create a list of all folders in tests (in aamp/test/utests, not in build folder)
#(In development, to build just a single test, TESTLIST can be replaced with a single test folder, e.g. "AampCliSet)"
TESTLIST=`find ./tests/* -maxdepth 0 -type d | cut -c 9-`
TESTDIR=$PWD
echo "Test list: "$TESTLIST

#Function to build coverage tests. CWD should be test/utests/build. Pass in name of test build folder & name of test.
build_test () {
echo build $1 $2
if [ -d "./tests/$1" ]; then
   cd ./tests/$1
   make $2
   if [ "$?" -ne "0" ] && [ "$halt_on_error" -eq "1" ]; then
     echo Halt on error $cov_name failed #$2 failed
     exit $?
   fi
   cd ../..
   true
else
   false
fi
}

# Build and run microtests:
set -e

mkdir -p build

cd build
if [[ "$OSTYPE" == "darwin"* ]]; then
    PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    PKG_CONFIG_PATH=$PWD/../../../Linux/lib/pkgconfig cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=$PWD/../../../Linux -DCMAKE_PLATFORM_UBUNTU=1 -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LIBRARY_PATH=$PWD/../../../Linux/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S../ -B$PWD -G "Unix Makefiles"
    export LD_LIBRARY_PATH=$PWD/../../../Linux/lib
else
    #abort the script if its not macOS or linux
    echo "Aborting unsupported OS detected"
    echo $OSTYPE
    exit 1
fi

make

if [ "$rdke_build" -eq "1" ]; then
	echo "RDKE build"

	for TEST in $TESTLIST ; do
      		echo "TEST IS $TEST in $PWD"
      		pushd tests/$TEST
      		./$TEST --gtest_output=json || true  # Don't exit script if a test crashes
                if [ ! -f test_detail.json ]; then
                    error_report $TEST
                fi
      		popd
    	done

	find . -name test_detail\*.json | xargs cat |  jq -s '{test_cases_results: {tests: map(.tests) | add,failures: map(.failures) | add,disabled: map(.disabled) | add,errors: map(.errors) | add,time: ((map(.time | rtrimstr("s") | tonumber) | add) | tostring + "s"),name: .[0].name,testsuites: map(.testsuites[])}}' > combinedReport.json

else
    ctest -j 4 --output-on-failure --no-compress-output -T Test --testdir build --output-junit ctest-results.xml
fi

# Build coverage tests if option selected

if [ "$build_coverage" -eq "1" ]; then
  echo Building coverage tests
  COMBINED="lcov "
  for TEST in $TESTLIST ; do
    #Find the test name (in case it doesn't match the test folder name) by searching for EXEC_NAME and stripping final character
    COVNAME=`cat $TESTDIR/tests/$TEST/CMakeLists.txt | grep "set(EXEC_NAME" | cut -c 15- | sed 's/.$//'`
    COVNAME=$COVNAME"_coverage"
    if build_test $TEST $COVNAME ; then
       #Build up the command to create the combined report by adding each test name
       COMBINED=$COMBINED" -a ./"$COVNAME".info"
    else
       COMBINED_MISSING+="${COVNAME//_coverage} "
    fi
  done

  #Create combined test report
  COMBINED=$COMBINED" -o combined.info"
  echo "Combined: "$COMBINED
  $COMBINED
  genhtml combined.info -o ../CombinedCoverage
  echo Building coverage tests complete
  if [ ! -z "$COMBINED_MISSING" ] ; then
     echo "!!!"
     echo "Following tests were skipped, check tests/CMakeLists.txt to see if present: \"$COMBINED_MISSING\""
     echo "!!!"
  fi
fi

exit $?
