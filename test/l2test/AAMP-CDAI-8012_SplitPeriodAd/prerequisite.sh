
#!/bin/bash
set -x
do_extract=0
download_stream() {
    #Stop multiple fetching of data if we already have it
    file=$(basename ${1})
    if [ -f $file ]; then
        echo "$file exists, not fetching again"
        return 0
    fi
    if (curl -O  -X GET $1);then
        echo "Test referring :" $1
        do_extract=1
    else
        echo "Downloading the stream failed. Check that the URL '$1' can be reached.
        If the URL is incorrect, set the environmental variable TEST_8012_STREAM_PATH to the right value in .env file under l2test folder"
        return 1
    fi
    return 0
}
extract_stream() {
    for x in $(pwd)/*.tar.xz
    do
    echo "extracting $x"
    if ! tar -xf "$x" -C "$(pwd)/testdata"; then
        echo "Error: '$x' is corrupted or not a valid zip archive."
        rm -rf $x
        return 1
    else
        echo "extraction complete ----> $x"
    fi
    done
    return 0
}
if [ -z "$TEST_8012_STREAM_PATH" ]; then
    TEST_8012_STREAM_PATH="https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/streams/L2/AAMP-CDAI-8004_ShortAd/content.tar.xz" #reusing 8004 test content
fi
echo $(pwd)
try=1
download_stream "${TEST_8012_STREAM_PATH}"
while [ "$?" != 0 ] && [ "${try}" < 3 ]; do
    try=$((try + 1))
    download_stream "${TEST_8012_STREAM_PATH}"
done
mkdir -p $(pwd)/testdata
files=$(find "$(pwd)/testdata/content" -maxdepth 1 -type f)
if [ -z "$files" ] || [ ${do_extract} = 1 ]; then
    echo "No files found directly under directory '$directory'."
else
    exit 0
fi
try=1
extract_stream
while [ "$?" != 0 ] && [ "${try}" < 3 ]; do
    try=$((try + 1))
    download_stream "${TEST_8012_STREAM_PATH}"
    extract_stream
done

