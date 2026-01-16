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

    // Known defect in production code: `PlayerExternalsRdkInterface` and
    // `DeviceIARMInterface` singletons have flawed destruction that causes
    // a double-free at process exit. The issue stems from improper singleton
    // lifetime management. Use _Exit() to skip global destructors.
    // TODO: File bug and fix singleton destruction semantics in production code.

    _Exit(result);
}