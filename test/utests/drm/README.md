# Contents

This directory contains headers, mocks/fakes and utilities used by the following tests:

- utests/tests/DrmOcdm
- utests/tests/DrmLegacy
- utests/tests/DrmSecureClient

# About the DRM tests

These tests test the AAMP DRM "sub-component" as a whole - they are not really unit tests
that test AAMP DRM classes individually, but are instead low-level integration tests,
testing how the AAMP DRM classes work together as a sub-system.

They are therefore best considered "L1.5" tests - sub-component tests, sitting somewhere
between the L1 unit (class) and L2 component tests.

The tests are part of the L1 test suite, and therefore run under utests/run.sh.

# Open CDM header dependency and patch

The ocdm/ headers used by these tests were taken from the ThunderClientLibraries
version R4.4.1:

- https://github.com/rdkcentral/ThunderClientLibraries/blob/R4.4.1/Source/ocdm/open_cdm.h
- https://github.com/rdkcentral/ThunderClientLibraries/blob/R4.4.1/Source/ocdm/adapter/open_cdm_adapter.h

The ocdm/open_cdm.h has been patched with the patch file ocdm/open_cdm.h.patch to avoid a
compilation error, since both the OCDM headers and AAMP itself define a AampMediaType enum.

# Secure Client header dependency

The secclient/ headers are Comcast proprietary code, and are only available from Comcast's source
repositories for the Secure Client.  If the headers are not present (i.e. the aamp/../secclient/ directory
is empty or does not exist), then the DrmSecureClient tests will be skipped.
