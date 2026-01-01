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

#include "hostIf_tr69ReqHandler.h"
#include <cstring>

extern "C" {

int hostIf_GetParams(const char* paramName, char* paramValue, size_t* paramLen) {
    // Stub implementation - returns empty value
    if (paramValue && paramLen) {
        paramValue[0] = '\0';
        *paramLen = 0;
    }
    return 0;
}

}
