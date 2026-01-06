#ifndef MOCK_GST_UTILS_H
#define MOCK_GST_UTILS_H

#include <gmock/gmock.h>
#include "GstUtils.h"  // Includes the enums and function signatures

// Namespace or class-based mocks depending on usage
// If functions are global, we need to wrap them in a mockable interface.

class IGstUtils {
public:
    virtual ~IGstUtils() = default;

    virtual const char* gstGetMediaTypeName(GstMediaType mediaType) = 0;
    virtual void PlayerCliGstInit(int* argc, char*** argv) = 0;
    virtual void PlayerCliGstTerm() = 0;
    virtual GstCaps* GetCaps(GstStreamOutputFormat format) = 0;
    virtual long long GetCurrentTimeMS() = 0;
};

// Google Mock implementation
class MockGstUtils : public IGstUtils {
public:
    MOCK_METHOD(const char*, gstGetMediaTypeName, (GstMediaType mediaType), (override));
    MOCK_METHOD(void, PlayerCliGstInit, (int* argc, char*** argv), (override));
    MOCK_METHOD(void, PlayerCliGstTerm, (), (override));
    MOCK_METHOD(GstCaps*, GetCaps, (GstStreamOutputFormat format), (override));
    MOCK_METHOD(long long, GetCurrentTimeMS, (), (override));
};
extern MockGstUtils *g_mockGstUtils;
#endif // MOCK_GST_UTILS_H