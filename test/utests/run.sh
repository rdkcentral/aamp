#!/bin/bash -e
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

#Create a list of all folders in tests (in aamp/test/utests, not in build folder)
#(In development, to build just a single test, TESTLIST can be replaced with a single test folder, e.g. "AampCliSet)"
TESTLIST=`find ./tests/* -maxdepth 0 -type d | cut -c 9-`
TESTDIR=$PWD
AAMPDIR=$(realpath ${TESTDIR}/../..)

echo "Test list: "$TESTLIST
AAMP_BUILD_GCNO=""

if [ "$build_coverage" -eq "1" ]; then
    #Find where aamp .gcno files get put when aamp-cli is built via install-aamp.sh -c
    A_GCNO=$(find ${AAMPDIR}/build -name 'AampConfig*gcno' -print -quit)

    if [ -z "$A_GCNO" ]; then
        echo "ERROR need to run 'install-aamp.sh -c' first to get baseline list of aamp files for coverage"
        exit 1
    fi 
    AAMP_BUILD_GCNO=$(dirname $A_GCNO)
fi

# Build and run microtests:
set -e

mkdir -p build

cd build

if [[ "$OSTYPE" == "darwin"* ]]; then
    PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:${AAMPDIR}/.libs/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    PKG_CONFIG_PATH=${AAMPDIR}/.libs/lib/pkgconfig cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=${AAMPDIR}/.libs -DCMAKE_PLATFORM_UBUNTU=1 -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LIBRARY_PATH=${AAMPDIR}/.libs/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S../ -B$PWD -G "Unix Makefiles"
    export LD_LIBRARY_PATH=${AAMPDIR}/.libs/lib
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

if [ "$build_coverage" -eq "1" ]; then
#We are in utests/build 

LCOV=lcov

#Get initial baseline of files from aamp-cli build
$LCOV --initial $IGNORE --directory ${AAMP_BUILD_GCNO} -b $AAMPDIR --capture --output-file baseline.info

#Get a list of dirs which contain coverage data for aamp source files.
TEST_DIRS=$(find tests -name '*.dir' -type d | grep -v _coverage.dir )
COMBINE=""
for DIR in $TEST_DIRS; do
  info_file=$DIR/TEST.info
  cmd="$LCOV --directory $DIR -b $TESTDIR --capture --output-file ${info_file}"
  echo $cmd
  $cmd
  COMBINE=$COMBINE" -a $info_file"
done
$LCOV $COMBINE -a baseline.info --output-file all.info.1
$LCOV --remove all.info.1 --output-file all.info "*/aamp/test/*" "*/aamp/Linux/*" "*/aamp/subtec/subtecparser/*" "/usr/*"
genhtml --demangle-cpp -o ../CombinedCoverage all.info
echo "Coverage written to $PWD/CombinedCoverage"
fi


