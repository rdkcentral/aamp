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

#ifndef HOSTIF_TR69_REQ_HANDLER_H
#define HOSTIF_TR69_REQ_HANDLER_H

#include <cstddef>

// Stub definitions for TR-69 interface
#define MAX_TR69_PARAM_LEN 2048
#define TR69HOSTIFMGR_MAX_PARAM_LEN 256
#define IARM_BUS_TR69HOSTIFMGR_NAME "TR69Bus"
#define IARM_BUS_TR69HOSTIFMGR_API_GetParams "GetParams"

typedef enum {
    HOSTIF_GET = 0,
    HOSTIF_SET
} HostIf_ReqType_t;

typedef enum {
    fcNoFault = 0,
    fcInvalidParameterName,
    fcInvalidParameterType
} faultCode_t;

typedef enum {
    hostIf_StringType = 0,
    hostIf_IntegerType,
    hostIf_BooleanType
} HostIf_ParamType_t;

typedef struct {
    char paramName[TR69HOSTIFMGR_MAX_PARAM_LEN];
    char paramValue[MAX_TR69_PARAM_LEN];
    int paramLen;
    HostIf_ReqType_t reqType;
    HostIf_ParamType_t paramtype;
    faultCode_t faultCode;
} HOSTIF_MsgData_t;

typedef struct {
    char paramName[256];
    char paramValue[MAX_TR69_PARAM_LEN];
    int paramLen;
} HostIf_tr69ReqHandler_Params_t;

extern "C" {
    int hostIf_GetParams(const char* paramName, char* paramValue, size_t* paramLen);
}

#endif // HOSTIF_TR69_REQ_HANDLER_H
