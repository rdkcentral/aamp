#!/bin/sh

# This is the RDK-E L1 test build and run script. It is expected to be run from a  
# rdke-builds/ci-container-image:latest docker container image.
if [ -n "$WORKSPACE" ]; then
    TESTHOME=$WORKSPACE
else
    TESTHOME="/home"
fi
echo "TESTHOME is $TESTHOME"

defaultlibdashversion="libdash = 3.0"
pkg-config --exists $defaultlibdashversion
if [ $? != 0 ]; then
  
   pushd $TESTHOME/aamp/test
   . $TESTHOME/aamp/install_libdash.sh
   popd
fi

#Build aamp components
echo "Building following aamp components"

#Build aampabr
echo "Building aampabr..."
cd $TESTHOME/aampabr
mkdir -p build
cd build
cmake ..
make
make install

#Build aampmetrics
echo "Building aampmetrics..."
cd $TESTHOME/aampmetrics
mkdir -p build
cd build
cmake ..
make
make install

apt update
echo "Installing gstreamer"
dpkg -s libgstreamer1.0-dev
if [ $? != 0 ]; then
    apt install -y libgstreamer1.0-dev
fi

dpkg -s libgstreamer-plugins-base1.0-dev
if [ $? != 0 ]; then
    apt install -y libgstreamer-plugins-base1.0-dev
fi

dpkg -s libreadline-dev
if [ $? != 0 ]; then
    apt install -y libreadline-dev
fi

dpkg -s sudo
if [ $? != 0 ]; then
    apt install -y sudo
fi

if ! command -v jq &> /dev/null; then
    apt install -y jq
fi

sudo apt remove -y meson
sudo pip3 install -y meson
sudo pip3 install -y lcov_cobertura
hash -r

mkdir -p "/tmp/Gtest_Report/"

echo "Running L1 tests"
cd $TESTHOME/aamp/test
cd utests
# apt install places gstreamer pc files as noted here
PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig ./run.sh -e

echo "Running L2 tests"
cd $TESTHOME/aamp/test
cd l2test

# Create virtual environment if it does not exists
if [ -d "./l2venv" ]; then
    echo "Virtual environment 'l2venv' exists."
else
    echo "Creating virtual environment."
    chmod +x l2framework_testenv.sh
    ./l2framework_testenv.sh
fi

echo "Activating virtual environment..."

#Activate virtual environment
. l2venv/bin/activate

if [ -n "$VIRTUAL_ENV" ] && [ "$(basename "$VIRTUAL_ENV")" = "l2venv" ]; then
    echo "Virtual environment 'l2venv' is activated."
else
    echo "Problem running Virtual environment."
    exit
fi

pip install pipreqs
# -s don't build subtec, we don't have credentials to clone that repo
python3 ./run_l2_aamp.py -b -m ci_test_set -s || true

deactivate || true

if [ -n "$VIRTUAL_ENV" ]; then
    echo "Virtual environment is still activated - exit failed."
else
    echo "Virtual environment successfully exited."
fi

#Create a minimal dummy JSON for L1 and L2 tests incase they fail to be built for some reason
if [ -e "$TESTHOME/aamp/test/utests/build/combinedReport.json" ]; then
    echo "L1 combinedReport.json was created successfully"
else
    echo "Cannot find L1 combinedReport.json, creating a minimal combinedReport.json"
    cat << EOF > "$TESTHOME/aamp/test/utests/build/combinedReport.json"
{
    "test_cases_results": 
    {
        "tests": 1,
        "failures": 0,
        "disabled": 0,
        "errors": 1,
        "timestamp": "$(date)",
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
}
EOF
fi

if [ -e "$TESTHOME/aamp/test/l2test/results.json" ]; then
    echo "L2 results.json was created suceessfully"
else
    echo "Cannot find L2 results.json, creating a minimal results.json"
    cat << EOF > "$TESTHOME/aamp/test/l2test/results.json"
{
    "test_cases_results": 
    {
        "tests": 1,
        "failures": 0,
        "disabled": 0,
        "errors": 1,
        "timestamp": "$(date)",
        "time": "0s",
        "name": "pytest",
        "hostname": "Linux (Generic)",
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
}
EOF
fi

#Create combined L1+L2 test report at /tmp/
jq -s '{ "test_cases_results": { tests: (map(.test_cases_results.tests) | add), failures: (map(.test_cases_results.failures) | add), disabled: (map(.test_cases_results.disabled) | add), errors: (map(.test_cases_results.errors) | add), time: ((map(.test_cases_results.time | rtrimstr("s") | tonumber) | add) | tostring + "s"), name: .[0].test_cases_results.name, testsuites: (map(.test_cases_results.testsuites[]))}}' $TESTHOME/aamp/test/utests/build/combinedReport.json $TESTHOME/aamp/test/l2test/results.json > /tmp/Gtest_Report/L1+L2Report.json

# Parse JSON file and extract relevant fields
failures=$(jq -r '.test_cases_results.failures' /tmp/Gtest_Report/L1+L2Report.json)
errors=$(jq -r '.test_cases_results.errors' /tmp/Gtest_Report/L1+L2Report.json)

# Check if failures and errors are all 0
if [ "$failures" -eq 0 ] && [ "$errors" -eq 0 ]; then
    echo "All test cases passed."
    exit 0
else
    echo "Some test cases failed or resulted in errors."
    exit 1
fi
