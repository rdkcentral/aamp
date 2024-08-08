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

    if [ "$TEST_2031_STREAM_PATH" == "" ]; then
        TEST_2031_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/VideoTestStream_HLS.tar.xz"
    fi

    #Stop multiple fetching of data if we already have it
    file=$(basename ${TEST_2031_STREAM_PATH})
    if [ -f $file ]; then
        echo "$file exists, not fetching again"
        # exit 0
    fi
    if (curl -O  -X GET $TEST_2031_STREAM_PATH);then
        echo "Test referring :" $TEST_2031_STREAM_PATH
        extract_stream
    else
        echo "Downloading the stream failed. Check that the URL '$TEST_2031_STREAM_PATH' can be reached.
        If the URL is incorrect, set the environmental variable TEST_2031_STREAM_PATH to the right value in .env file under l2test folder"
    fi

}


extract_stream
