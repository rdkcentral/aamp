# AAMP WebVTT L2 test

This python3 L2 test verifies WebVTT functionality. In particular it was
introduced to verify the following feature:
<RDKAAMP-477> [UVE] support ability to override WebVTT caption styling -> cont'd to handle AAMP -> GStreamer - Subtec

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

GStreamer Subtec plugin built and installed, see:

gst-plugins-rdk-aamp/gst_subtec/Readme.md

This test plays the following stream:
https://storage.googleapis.com/shaka-demo-assets/angel-one-hls/hls.m3u8

If that stream is not available, the test will fail at the beginning with a
timeout waiting for the event AAMP_EVENT_TUNED. Any other stream with no
subtitles can be used to run this test.

## Run l2test using script:

From the *test/l2test/TST_1001_Webvtt* folder run:

./run_test.py

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/test/l2test/TST_1001_Webvtt
    ./run_test.py

## Note Running on Mac

Due to the shim layers wrapping python it is not easy to run with ADDRESS_SANITIZER enabled, and you will see an error like:

    ==61962==ERROR: Interceptors are not working. This may be because AddressSanitizer is loaded too late (e.g. via dlopen). Please launch the executable with:
    DYLD_INSERT_LIBRARIES=/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/14.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib
    "interceptors not installed" && 0

The simplest solution is to temporarily disable ADDRESS_SANITIZER in CMakeLists.txt, rebuild aamp and run the script
