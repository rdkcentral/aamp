---

# AAMP on Mac OS X

This document contains the instructions to setup and debug stand alone AAMP (aamp-cli) on Mac OS X.

## Install dependencies

**1. Install XCode from the Apple Store**

**2. Install XCode Command Line Tools**

This is required for MacOS version < 10.15

```
xcode-select --install
sudo installer -pkg /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_<version>.pkg -target /
```

For MacOS 10.15 & above, we can check the SDK install path as
```
xcrun --sdk macosx --show-sdk-path
/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk
```


##Build and execute aamp-cli
**1. Open aamp.xcodeproj in Xcode**

```
git clone "https://code.rdkcentral.com/r/rdk/components/generic/aamp" -b dev_sprint
cd aamp; bash install-aamp.sh
```

**2. Build the code**

```
	Start XCode, open build/AAMP.xcodeproj project file
	Product -> Build
```
If you see the error 'No CMAKE_C_COMPILER could be found.' when running osxbuild.sh, check that your installed cmake version matches the minimum required version shown earlier.
Even after updating the CMake version if you still see the above error, then run "sudo xcode-select --reset" and then execute the "osxbuild.sh" this fixes the issue.


**3. Select target to execute**

```
	Product -> Scheme -> Choose Scheme
	aamp-cli
```

**4. Execute**

While executing if you face the below MacOS warning then please follow the below steps to fix it.
"Example warning: Machine runs macOS 10.15.7, which is lower than aamp-cli's minimum deployment target of 11.1. Change your project's minimum deployment target or upgrade machine’s version of macOS."
Click "AAMP" project and lower the "macOS Deployment Target" version. For example: Change it to 10.11 then run the aamp-cli, it will work.


```
Product -> Run
```
