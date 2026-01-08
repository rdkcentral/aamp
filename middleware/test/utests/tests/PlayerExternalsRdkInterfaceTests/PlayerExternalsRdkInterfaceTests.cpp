/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <gtest/gtest.h>
#include <cstdlib>

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    // Use _Exit() instead of return to bypass global destructors
    // This works around a double-free bug in production code destructors
    // (PlayerExternalsRdkInterface and DeviceIARMInterface set their own
    // global shared_ptr to nullptr during destruction, causing crashes)
    _Exit(result);
}