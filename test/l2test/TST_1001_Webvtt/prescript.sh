#!/bin/bash
if [[ "$OSTYPE" == "darwin"* ]]; then
pushd ../../../
#Modify aamp/CMakeLists.txt to disable address sanitizer, as the shim layers wrapping python cause the test to fail otherwise. The original file is stored as CmakeLists.txt.bak
sed -i'.bak' 's/PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER TRUE/PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER FALSE/;s/PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN TRUE/PROPERTY XCODE_SCHEME_ADDRESS_SANITIZER_USE_AFTER_RETURN FALSE/' CMakeLists.txt
popd
fi
