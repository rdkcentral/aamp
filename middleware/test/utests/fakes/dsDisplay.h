#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    dsDISPLAY_EVENT_CONNECTED = 0,
    dsDISPLAY_EVENT_DISCONNECTED = 1
} dsDisplayEvent_t;

typedef enum {
    dsHDCP_STATUS_AUTHENTICATED = 0,
    dsHDCP_STATUS_AUTHENTICATION_FAILURE = 1,
    dsHDCP_STATUS_UNKNOWN = 2,
    dsHDCP_STATUS_UNAUTHENTICATED = 3
} dsHdcpStatus_t;

#ifdef __cplusplus
}
#endif
