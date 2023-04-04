
if [[ "$OSTYPE" == "darwin"* ]]; then
    cd ../../build
    xcodebuild -target aamp_smoketest_coverage build
elif [[ "$OSTYPE" == "linux"* ]]; then
    cd ../../build/test/smoketest
    make aamp_smoketest_coverage
else    
    echo "WARNING - unsupported platform!"
fi 
