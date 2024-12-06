#!/usr/bin/env bash
#
# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#!/bin/bash
# This script will build and run L1 tests.
# Use option: -c to additionally build coverage tests
# Use option: -h to halt coverage tests on error

# "corrupt arc tag"
find . -name "*.gcda" -print0 | xargs -0 rm

build_coverage=0
halt_on_error=0

while getopts "ch" opt; do
  case ${opt} in
    c ) echo Do build coverage
        build_coverage=1
      ;;
    h ) echo Halt on error
        halt_on_error=1
      ;;
    * )
      ;;
  esac
done

# Find all of the potential tests by directory search so they can be built individually
#Create a list of all folders in tests (in aamp/test/utests, not in build folder)
#(In development, to build just a single test, TESTLIST can be replaced with a single test folder, e.g. "AampCliSet)"
TESTLIST=`find ./tests -mindepth 1 -maxdepth 1 -type d | cut -c 9-`
TESTDIR=$PWD
echo "L1 test list: "$TESTLIST

#Function to build tests. CWD should be test/utests/build. Pass in name of test build folder & name of test.
build_test () {
echo "BUILDING "$1 $2
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

# Build and run L1 tests:
set -e

rm -rf build
mkdir -p build

cd build
# Run cmake
if [[ "$OSTYPE" == "darwin"* ]]; then
    PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    PKG_CONFIG_PATH=$PWD/../../../ cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=$PWD/../../../ -DCMAKE_PLATFORM_UBUNTU=1 -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LIBRARY_PATH=$PWD/../../../ -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S../ -B$PWD -G "Unix Makefiles"
    export LD_LIBRARY_PATH=$PWD/../../../
else
    #abort the script if its not macOS or linux
    echo "Aborting unsupported OS detected"
    echo $OSTYPE
    exit 1
fi

make

ctest -j 4 --output-on-failure --no-compress-output -T Test

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
