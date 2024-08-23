#!/bin/bash
set -x

echo $(pwd)
mkdir -p $(pwd)/testdata

extract_stream() {

    files=$(find "$(pwd)/testdata" -maxdepth 1 -type f)
    if [ -z "$files" ]; then
        echo "No files found directly under directory '$(pwd)/testdata'."
    else
        exit 0
    fi
    for x in $(pwd)/*.tar.xz
    do
    echo "extracting $x"
    if ! tar -xvf "$x" -C "$(pwd)/testdata"; then
        echo "Error: '$x' is corrupted or not a valid zip archive."
        rm -rf $x
        download_stream
    else
        echo "extraction complete ----> $x"
    fi
    done

}

download_stream() {

    if [ "$TEST_0002_STREAM_PATH" == "" ]; then
        TEST_0002_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/testApps/L2/AAMP-IFRAME-4007/VideoTestStream.tar.xz"
    fi

    #Stop multiple fetching of data if we already have it
    file=$(basename ${TEST_0002_STREAM_PATH})
    if [ -f $file ]; then
        echo "$file exists, not fetching again"
        # exit 0
    fi
    if (curl -O  -X GET $TEST_0002_STREAM_PATH);then
        echo "Test referring :" $TEST_0002_STREAM_PATH
        extract_stream
    else
        echo "Downloading the stream failed. Check that the URL '$TEST_0002_STREAM_PATH' can be reached.
        If the URL is incorrect, set the environmental variable TEST_0002_STREAM_PATH to the right value in .env file under l2test folder"
    fi

}


extract_stream
