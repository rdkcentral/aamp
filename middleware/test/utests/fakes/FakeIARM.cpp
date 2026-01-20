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

#include "FakeIARM.h"
#include <iostream>

IARMTestHelper& IARMTestHelper::getInstance() {
    static IARMTestHelper instance;
    return instance;
}

void IARMTestHelper::registerHandler(const std::string& owner, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
    mHandlers[owner][eventId] = handler;
}

void IARMTestHelper::removeHandler(const std::string& owner, IARM_EventId_t eventId) {
    if (mHandlers.find(owner) != mHandlers.end()) {
        mHandlers[owner].erase(eventId);
    }
}

void IARMTestHelper::triggerEvent(const std::string& owner, IARM_EventId_t eventId, void* data, size_t len) {
    if (mHandlers.find(owner) != mHandlers.end() && 
        mHandlers[owner].find(eventId) != mHandlers[owner].end()) {
        mHandlers[owner][eventId](owner.c_str(), eventId, data, len);
    }
}

void IARMTestHelper::reset() {
    mHandlers.clear();
}

int IARMTestHelper::getHandlerCount(const std::string& owner, IARM_EventId_t eventId) {
    if (mHandlers.find(owner) != mHandlers.end() && 
        mHandlers[owner].find(eventId) != mHandlers[owner].end()) {
        return 1;
    }
    return 0;
}

extern "C" {

IARM_Result_t IARM_Bus_Init(const char *name) {
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Connect(void) {
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Disconnect(void) {
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Term(void) {
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RegisterEventHandler(const char *ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
    if (!ownerName || !handler) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    IARMTestHelper::getInstance().registerHandler(ownerName, eventId, handler);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_RemoveEventHandler(const char *ownerName, IARM_EventId_t eventId, IARM_EventHandler_t handler) {
    if (!ownerName) {
        return IARM_RESULT_INVALID_PARAM;
    }
    
    IARMTestHelper::getInstance().removeHandler(ownerName, eventId);
    return IARM_RESULT_SUCCESS;
}

IARM_Result_t IARM_Bus_Call(const char *ownerName, const char *methodName, void *arg, size_t argLen) {
    return IARM_RESULT_SUCCESS;
}

} // extern "C"