
build_coverage=0
while getopts "ch" opt; do
  case ${opt} in
    c ) echo Do build coverage
        build_coverage=1
      ;;
    * ) echo -e "\nUsage: $0 [-c]"
        echo "Options:"
        echo "-c build for coverage (requires install-aamp.sh -c)"
        echo ""
        exit
      ;;
  esac
done

if [[ "$OSTYPE" == "darwin"* ]]; then
    pushd ../../build
    if [ "$build_coverage" -eq "1" ]; then
        xcodebuild -target aamp_smoketest_coverage build
        if [ $? -ne "0" ]; then
            echo -e "\nCould not build coverage target. Was install-aamp.sh -c run first?\n"
            exit $?
        fi
        popd
    else
        xcodebuild -target aamp_smoketest build
        if [ $? -ne "0" ]; then
            echo -e "\nCould not build. Was install-aamp.sh run first?\n"
            exit $?
        fi
        popd
        ../../build/test/smoketest/Debug/aamp_smoketest
    fi
elif [[ "$OSTYPE" == "linux"* ]]; then
    cd ../../build/test/smoketest
    if [ "$build_coverage" -eq "1" ]; then
        make aamp_smoketest_coverage
        if [ $? -ne "0" ]; then
            echo -e "\nCould not build coverage target. Was install-aamp.sh -c run first?\n"
            exit $?
        fi
    else
        make aamp_smoketest
        if [ $? -ne "0" ]; then
            echo -e "\nCould not build. Was install-aamp.sh run first?\n"
            exit $?
        fi
        ./aamp_smoketest
    fi
else    
    echo "WARNING - unsupported platform!"
fi 

exit $?

