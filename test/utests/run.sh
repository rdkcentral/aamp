#!/bin/bash
# This script will build and run microtests.
# Use option: -c to additionally build coverage tests
# Use option: -h to halt coverage tests on error

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

#Function to build coverage tests. CWD should be test/utests/build. Pass in name of test build folder & name of test.
build_test () {
echo build $1 $2
cd ./tests/$1
make $2
if [ "$?" -ne "0" ] && [ "$halt_on_error" -eq "1" ]; then
  echo Halt on error $2 failed
  exit $?
fi
cd ../..
}

# Build and run microtests:
set -e

mkdir -p build

cd build
if [[ "$OSTYPE" == "darwin"* ]]; then
    PKG_CONFIG_PATH=/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH cmake -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug ../
elif [[ "$OSTYPE" == "linux"* ]]; then
    PKG_CONFIG_PATH=$PWD/../../../Linux/lib/pkgconfig /usr/bin/cmake --no-warn-unused-cli -DCMAKE_INSTALL_PREFIX=$PWD/../../../Linux -DCMAKE_PLATFORM_UBUNTU=1 -DCOVERAGE_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_LIBRARY_PATH=$PWD/../../../Linux/lib -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/gcc -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/g++ -S../ -B$PWD -G "Unix Makefiles"
    export LD_LIBRARY_PATH=$PWD/../../../Linux/lib
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
  build_test "AampCliSet" "AampCliSetTests_coverage"
  build_test "Base64AAMP" "Base64AAMPTests_coverage"
  build_test "AampUtilsTests" "AampUtilsTests_coverage"
  build_test "PlayerInstanceAAMP" "PlayerInstanceAAMPTests_coverage"
  build_test "PrivateInstanceAAMP" "PrivateInstanceAAMPTests_coverage"
  build_test "StreamAbstractionAAMP_HLS" "StreamAbstractionAAMP_HLS_coverage"
  build_test "StreamAbstractionAAMP_MPD" "StreamAbstractionAAMP_MPD_coverage"
  build_test "TextStyleAttributes" "TextStyleAttributesTests_coverage"
  build_test "UrlEncDecAAMP" "UrlEncDecAAMPTests_coverage"

  #Create combined test report
  lcov -a ./PlayerInstanceAAMPTests_coverage.info -a ./AampCliSetTests_coverage.info -a ./Base64AAMPTests_coverage.info -a ./AampUtilsTests_coverage.info -a ./UrlEncDecAAMPTests_coverage.info -a ./TextStyleAttributesTests_coverage.info -a ./StreamAbstractionAAMP_HLS_coverage.info -a ./StreamAbstractionAAMP_MPD_coverage.info -a ./PrivateInstanceAAMPTests_coverage.info -o combined.info
  genhtml combined.info -o ../CombinedCoverage
  echo Building coverage tests complete
fi

exit $?
