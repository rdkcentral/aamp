#!/bin/bash -e
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
# This script will build and run microtests.
# Use option: -c to additionally build coverage tests
# Use option: -h to halt coverage tests on error

if [[ -z "${MAKEFLAGS}" ]]; then
    export MAKEFLAGS=-j$(nproc)
fi

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

# "corrupt arc tag"
(find . -name "*.gcda" -print0 | xargs -0 rm) || true

build_coverage=0
halt_on_error=0
rdke_build=0

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

TEST_DIR=$PWD
PLAYERDIR=$(realpath ${TEST_DIR}/../..)

PLAYER_BUILD_GCNO=""

if [ "$build_coverage" -eq "1" ]; then
    #Find where player .gcno files get put when player-cli is built via install-middleware.sh -c
    A_GCNO=$(find ${PLAYERDIR}/build -name 'AampConfig*gcno' -print -quit)

    if [ -z "$A_GCNO" ]; then
        echo "ERROR need to run 'install-middleware.sh -c' first to get baseline list of middleware files for coverage"
        exit 1
    fi
    PLAYER_BUILD_GCNO=$(dirname $A_GCNO)
fi

# Build and run microtests:
set -e

mkdir -p build

cd build

if [[ "$OSTYPE" == "darwin"* ]]; then
    PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:${PLAYERDIR}/.libs/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_RDKE_TEST_RUN=$rdke_build ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    echo "PLAYER DIR[${PLAYERDIR}]"
    PKG_CONFIG_PATH=${PLAYERDIR}/.libs/lib/pkgconfig cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=${PLAYERDIR}/.libs -DCMAKE_PLATFORM_UBUNTU=1 -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LIBRARY_PATH=${PLAYERDIR}/.libs/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -DCMAKE_RDKE_TEST_RUN=$rdke_build -S../ -B$PWD -G "Unix Makefiles"
    export LD_LIBRARY_PATH=${PLAYERDIR}/.libs/lib
else
    #abort the script if its not macOS or linux
    echo "Aborting unsupported OS detected"
    echo $OSTYPE
    exit 1
fi
make
# Work around for cmake deprecation of --testdir option for version 3.21.0 and above
cmake_version=$(cmake --version | head -n 1 | awk '{print $3}')
major_version=$(echo "$cmake_version" | cut -d. -f1)
minor_version=$(echo "$cmake_version" | cut -d. -f2)
if [[ "$major_version" -gt 3 ]] || [[ "$major_version" -eq 3 && "$minor_version" -ge 21 ]]; then
  CT_TESTDIR="" 
else
  CT_TESTDIR="--testdir build" 
    
fi

if [ "$rdke_build" -eq "1" ]; then
	echo "RDKE build"

	export GTEST_OUTPUT="json"
  ctest -j 4 --output-on-failure --no-compress-output -T Test $CT_TESTDIR || true  # Don't exit script if a test fails

  cd tests

  for test_dir in */; do
      if [ -d "$test_dir" ] && [ "$test_dir" != "CMakeFiles/" ] && [ "$test_dir" != "tsb/" ]; then
          if [ ! -f "$test_dir/test_detail.json" ]; then
              echo "Missing: $test_dir/test_detail.json"

              # Create a fallback test_detail.json
            cat <<EOF > "$test_dir/test_detail.json"
{
  "tests": 1,
  "failures": 1,
  "disabled": 0,
  "errors": 0,
  "time": "0.0s",
  "name": "${test_dir%/}",
  "testsuites": [
    {
      "name": "${test_dir%/}",
      "tests": 1,
      "failures": 1,
      "disabled": 0,
      "errors": 0,
      "time": "0.0s",
      "testsuite": [
        {
          "name": "${test_dir%/}",
          "status": "failed",
          "time": "0.0s",
          "classname": "${test_dir%/}",
          "failure": {
            "message": "Testing ended abruptly",
            "type": "SEGFAULT (probably)"
          }
        }
      ]
    }
  ]
}
EOF
          fi
      fi
  done

  cd ..

	find . -name test_detail\*.json | xargs cat |  jq -s '{test_cases_results: {tests: map(.tests) | add,failures: map(.failures) | add,disabled: map(.disabled) | add,errors: map(.errors) | add,time: ((map(.time | rtrimstr("s") | tonumber) | add) | tostring + "s"),name: .[0].name,testsuites: map(.testsuites[])}}' > L1Report.json

else
    ctest -j 4 --output-on-failure --no-compress-output -T Test $CT_TESTDIR --output-junit ctest-results.xml
fi

if [ "$build_coverage" -eq "1" ]; then
#We are in utests/build

LCOV=lcov

#Get initial baseline of files from player-cli build
$LCOV --initial $IGNORE --directory ${PLAYER_BUILD_GCNO} -b $PLAYERDIR --capture --output-file baseline.info

#Get a list of dirs which contain coverage data for player source files.
TEST_DIRS=$(find tests -name '*.dir' -type d | grep -v _coverage.dir )
COMBINE=""
for DIR in $TEST_DIRS; do
  info_file=$DIR/TEST.info
  cmd="$LCOV --directory $DIR -b $TESTDIR --capture --output-file ${info_file}"
  echo $cmd
  $cmd
  COMBINE=$COMBINE" -a $info_file"
done
HTML_OUT=$(realpath ../CombinedCoverage)
XML_OUT=$(realpath ../coverage.xml)
$LCOV $COMBINE -a baseline.info --output-file all.info.1
$LCOV --remove all.info.1 --output-file all.info "*/aamp/tsb/test/*" "*/aamp/.libs/*" "*/aamp/test/*" "*/aamp/Linux/*" "*/aamp/subtec/subtecparser/*" "/usr/*"
genhtml --demangle-cpp -o ${HTML_OUT} all.info
# Generate coverage.xml
lcov_cobertura all.info -b ${PLAYERDIR} --demangle -o ${XML_OUT} || true
echo "Coverage written to ${HTML_OUT}"
fi
