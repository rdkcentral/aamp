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
# Use option: -t <seconds> to override the per-test CTest timeout (default: 60)
# The timeout can also be set via the CTEST_TIMEOUT environment variable;
# the -t option takes precedence over the environment variable.

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

# List tests that took longer than 1 second, ordered by duration (longest first)
list_slow_tests()
{
    local xml_file="$1"
    if [[ ! -f "$xml_file" ]]; then
        return
    fi
    python3 "${TESTDIR}/list_slow_tests.py" "$xml_file"
}

# "corrupt arc tag"
find . -name "*.gcda" -print0 | xargs -0 --no-run-if-empty rm

build_coverage=0
halt_on_error=0
rdke_build=0
# Default timeout: honour CTEST_TIMEOUT env var if set, otherwise 60 seconds.
CTEST_TIMEOUT=${CTEST_TIMEOUT:-60}

while getopts "ceht:" opt; do
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
    t ) CTEST_TIMEOUT=${OPTARG}
        echo "CTest timeout set to ${CTEST_TIMEOUT}s"
      ;;
    * )
      ;;
  esac
done

TESTDIR=$PWD
AAMPDIR=$(realpath ${TESTDIR}/../..)

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
    # Resolve Homebrew OpenSSL pkgconfig dir (openssl@3 is the repo default).
    # brew --prefix openssl resolves to openssl@3 on standard Homebrew setups.
    _ssl_prefix=$(brew --prefix openssl@3 2>/dev/null)
    if [[ ! -f "${_ssl_prefix}/lib/pkgconfig/openssl.pc" ]]; then
        echo "ERROR: Could not find a Homebrew OpenSSL pkgconfig file."
        echo "Please install it with:  brew install openssl@3"
        exit 1
    fi
    # Detect a stale cmake cache caused by a Homebrew OpenSSL upgrade.
    # pkg_check_modules(OPENSSL) caches the resolved Cellar path (e.g.
    # /opt/homebrew/Cellar/openssl@3/3.6.2/include).  After "brew upgrade
    # openssl@3 && brew cleanup", that Cellar directory may be incomplete or
    # missing headers entirely.  When make re-triggers cmake it runs without
    # PKG_CONFIG_PATH, so cmake reads the cached stale path and the build
    # fails with "openssl/sha.h file not found".  Clearing CMakeCache.txt
    # forces cmake to re-run pkg_check_modules with the correct PKG_CONFIG_PATH
    # set by this script, picking up the newly installed version.
    if [[ -f "CMakeCache.txt" ]]; then
        _cached_ssl_inc=$(grep "^OPENSSL_INCLUDE_DIRS:INTERNAL=" CMakeCache.txt | cut -d= -f2) || true
        if [[ -n "${_cached_ssl_inc}" && ! -f "${_cached_ssl_inc}/openssl/sha.h" ]]; then
            echo "WARNING: Cached OpenSSL include path is stale:"
            echo "         ${_cached_ssl_inc}/openssl/sha.h not found."
            echo "         This typically means 'brew upgrade openssl@3' ran since the last"
            echo "         cmake configure. Removing CMakeCache.txt so OpenSSL is re-detected."
            rm -f CMakeCache.txt
        fi
    fi
    # GStreamer: prefer the macOS framework installer; fall back to Homebrew.
    # Mirrors the detection logic added to scripts/install_aampcli.sh in
    # PR #1489 (VPAAMP-392).  The old code unconditionally prepended the
    # framework path, causing an opaque cmake failure on machines that have
    # GStreamer installed via Homebrew instead of the standalone framework.
    _GST_FRAMEWORK_PKG="/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig"
    if [[ -d "${_GST_FRAMEWORK_PKG}" ]]; then
        _GST_PKG_CONFIG="${_GST_FRAMEWORK_PKG}"
    else
        _GST_BREW_PREFIX=$(brew --prefix gstreamer 2>/dev/null) || true
        if [[ -n "${_GST_BREW_PREFIX}" && -d "${_GST_BREW_PREFIX}/lib/pkgconfig" ]]; then
            _GST_PKG_CONFIG="${_GST_BREW_PREFIX}/lib/pkgconfig"
            # gstreamer-app-1.0 lives in gst-plugins-base
            _GST_BASE_PREFIX=$(brew --prefix gst-plugins-base 2>/dev/null) || true
            if [[ -n "${_GST_BASE_PREFIX}" && -d "${_GST_BASE_PREFIX}/lib/pkgconfig" ]]; then
                _GST_PKG_CONFIG="${_GST_PKG_CONFIG}:${_GST_BASE_PREFIX}/lib/pkgconfig"
            fi
        else
            echo "ERROR: GStreamer not found. Please install one of:"
            echo "  GStreamer macOS framework: https://gstreamer.freedesktop.org/download/"
            echo "  OR via homebrew: brew install gstreamer gst-plugins-base"
            exit 1
        fi
    fi
    PKG_CONFIG_PATH=${_GST_PKG_CONFIG}:${AAMPDIR}/.libs/lib/pkgconfig:/usr/local/lib/pkgconfig:${_ssl_prefix}/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_RDKE_TEST_RUN=$rdke_build ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    PKG_CONFIG_PATH=${AAMPDIR}/.libs/lib/pkgconfig cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=${AAMPDIR}/.libs -DCMAKE_PLATFORM_UBUNTU=1 -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LIBRARY_PATH=${AAMPDIR}/.libs/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -DCMAKE_RDKE_TEST_RUN=$rdke_build -S../ -B$PWD -G "Unix Makefiles"
    export LD_LIBRARY_PATH=${AAMPDIR}/.libs/lib
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
  CT_OUTPUT_JUNIT="--output-junit ctest-results.xml"
else
  CT_TESTDIR="--testdir build"
  CT_OUTPUT_JUNIT=""
fi

if [ "$rdke_build" -eq "1" ]; then
	echo "RDKE build"

	export GTEST_OUTPUT="json"
  ctest -j 4 --timeout "${CTEST_TIMEOUT}" --output-on-failure --no-compress-output -T Test $CT_TESTDIR $CT_OUTPUT_JUNIT || true  # Don't exit script if a test fails

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
    ctest -j 4 --timeout "${CTEST_TIMEOUT}" --output-on-failure --no-compress-output -T Test $CT_TESTDIR $CT_OUTPUT_JUNIT
fi

if [[ -n "$CT_OUTPUT_JUNIT" ]]; then
    list_slow_tests ctest-results.xml
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
HTML_OUT=$(realpath ../CombinedCoverage)
XML_OUT=$(realpath ../coverage.xml)
$LCOV $COMBINE -a baseline.info --output-file all.info.1
$LCOV --remove all.info.1 --output-file all.info "*/aamp/tsb/test/*" "*/aamp/.libs/*" "*/aamp/test/*" "*/aamp/Linux/*" "*/aamp/subtec/subtecparser/*" "/usr/*"
genhtml --demangle-cpp -o ${HTML_OUT} all.info
# Generate coverage.xml
lcov_cobertura all.info -b ${AAMPDIR} --demangle -o ${XML_OUT} || true
echo "Coverage written to ${HTML_OUT}"
fi
