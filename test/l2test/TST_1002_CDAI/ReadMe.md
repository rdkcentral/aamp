# AAMP VOD CDAI Single Pipeline L2 test

This python3 L2 test verifies CDAI Single Pipeline functionality, with config useSinglePipeline set to true.
In particular it was introduced to verify the following feature:

<RDK-1176> Optimization/Cosmetic fix for Sky VOD CDAI Transitions

Notably, it covers the following scenarios:

- Pre-roll, mid-roll and post-roll ads
- Transitions from main content to ad, ad to ad, and ad to main content
- Transitions from post-roll ad or old main content to new main content

The test mimics the pattern of calls that JSPP makes to AAMP at full stack.
In particular, the test creates/destroys ad players on the fly, before/after each ad.

## Pre-requisites to L2 tests:

AAMP installed using install-aamp.sh script.

This test plays the following streams:

https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad1/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad7/ed9e9eba-e818-413f-97ea-10cb3559ac31/1628085935274/AD/HD/manifest.mpd
https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad4/hsar1099-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad19/7b048ca3-6cf7-43c8-98a3-b91c09ed59bb/1628252309135/AD/HD/manifest.mpd
https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad2/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad1/7849033a-530a-43ce-ac01-fc4518674ed0/1628085609056/AD/HD/manifest.mpd
https://cpetestutility.stb.r53.xcal.tv/AAMP/tools/aamptest/ads/ad3/hsar1039-soip-ads-prd.cdn01.skycdp.com/ads-gb-s8-prd-ak.cdn01.skycdp.com/v1/frag/bmff/t/ipvodad17/dc004d50-30ea-4f46-add8-9a007fe7c8ec/1628085330949/AD/HD/manifest.mpd

## Run l2test using script:

From the *test/l2test/TST_1002_CDAI* folder run:

./run_test.py

## Example:

    cd aamp
    bash install-aamp.sh
    cd test/l2test/TST_1002_CDAI
    ./run_test.py
