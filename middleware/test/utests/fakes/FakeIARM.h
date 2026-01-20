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

#ifndef FAKE_IARM_H
#define FAKE_IARM_H

#include <functional>
#include <map>
#include <string>

// IARM type definitions
typedef enum {
    IARM_RESULT_SUCCESS = 0,
    IARM_RESULT_INVALID_PARAM,
    IARM_RESULT_IPCCORE_FAIL
} IARM_Result_t;

typedef int IARM_EventId_t;
typedef void (*IARM_EventHandler_t)(const char *owner, IARM_EventId_t eventId, void *data, size_t len);

// IARM Bus definitions
#define IARM_BUS_DSMGR_NAME "DSMgr"
#define IARM_BUS_DSMGR_EVENT_HDMI_HOTPLUG 1
#define IARM_BUS_DSMGR_EVENT_HDCP_STATUS 2
#define IARM_BUS_DSMGR_EVENT_RES_PRECHANGE 3
#define IARM_BUS_DSMGR_EVENT_RES_POSTCHANGE 4

// IARM Event data structures
typedef enum {
    dsHDMI_EVENT_CONNECTED = 0,
    dsHDMI_EVENT_DISCONNECTED
} dsHDMI_EVENT_T;

typedef enum {
    dsHDCP_STATUS_UNPOWERED = 0,
    dsHDCP_STATUS_UNAUTHENTICATED,
    dsHDCP_STATUS_AUTHENTICATED,
    dsHDCP_STATUS_AUTHENTICATIONFAILURE
} dsHdcpStatus_t;

typedef enum {
    dsHDCP_VERSION_1X = 0,
    dsHDCP_VERSION_2X,
    dsHDCP_VERSION_MAX
} dsHdcpProtocolVersion_t;

typedef struct {
    int width;
    int height;
    union {
        struct {
            int event;
        } hdmi_hpd;
        struct {
            int hdcpStatus;
        } hdmi_hdcp;
        struct {
            int width;
            int height;
        } resn;
    } data;
} IARM_Bus_DSMgr_EventData_t;

// Test helper class to manage registered handlers
class IARMTestHelper {
public:
    static IARMTestHelper& getInstance();
    
    void registerHandler(const std::string& owner, IARM_EventId_t eventId, IARM_EventHandler_t handler);
    void removeHandler(const std::string& owner, IARM_EventId_t eventId);
    void triggerEvent(const std::string& owner, IARM_EventId_t eventId, void* data, size_t len);
    void reset();
    int getHandlerCount(const std::string& owner, IARM_EventId_t eventId);

private:
    std::map<std::string, std::map<IARM_EventId_t, IARM_EventHandler_t>> mHandlers;
};

// IARM API functions
extern "C" {
    IARM_Result_t IARM_Bus_Init(const char *name);
    IARM_Result_t IARM_Bus_Connect(void);
    IARM_Result_t IARM_Bus_Disconnect(void);
    IARM_Result_t IARM_Bus_Term(void);
    IARM_Result_t IARM_Bus_RegisterEventHandler(const char *ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler);
    IARM_Result_t IARM_Bus_RemoveEventHandler(const char *ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler);
    IARM_Result_t IARM_Bus_Call(const char *ownerName, const char *methodName, void *arg, size_t argLen);
}

#endif // FAKE_IARM_H