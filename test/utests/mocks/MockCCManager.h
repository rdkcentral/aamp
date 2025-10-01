#ifndef MOCK_PLAYER_CC_MANAGER_H
#define MOCK_PLAYER_CC_MANAGER_H

#include <gmock/gmock.h>
#include "PlayerCCManager.h"

/**
 * @brief Mock class for PlayerCCManagerBase
 */
class MockPlayerCCManagerBase : public PlayerCCManagerBase
{
public:
    // Pure virtual methods that must be mocked
    MOCK_METHOD(void, Release, (int iID), (override));
    MOCK_METHOD(void, StartRendering, (), (override));
    MOCK_METHOD(void, StopRendering, (), (override));
    MOCK_METHOD(int, SetDigitalChannel, (unsigned int id), (override));
    MOCK_METHOD(int, SetAnalogChannel, (unsigned int id), (override));
    MOCK_METHOD(bool, CheckCCHandle, (), (const, override));

    // Virtual methods with default implementations
    MOCK_METHOD(int, GetId, (), (override));
    MOCK_METHOD(int, SetStatus, (bool enable), (override));
    MOCK_METHOD(int, SetTrack, (const std::string& track, const CCFormat format), (override));
    MOCK_METHOD(int, SetStyle, (const std::string& options), (override));
    MOCK_METHOD(void, SetTrickplayStatus, (bool enable), (override));
    MOCK_METHOD(void, SetParentalControlStatus, (bool locked), (override));
    MOCK_METHOD(void, updateLastTextTracks, (const std::vector<CCTrackInfo>& newTextTracks), (override));
    MOCK_METHOD(bool, IsOOBCCRenderingSupported, (), (override));

    // Non-virtual methods for additional testing flexibility
    MOCK_METHOD(void, EnsureInitialized, (), (override));
    MOCK_METHOD(void, EnsureHALInitialized, (), (override));
    MOCK_METHOD(void, EnsureRendererCommsInitialized, (), (override));
    MOCK_METHOD(int, Initialize, (void* handle), (override));
};

extern MockPlayerCCManagerBase *g_mockPlayerCCManagerBase;


#endif // MOCK_PLAYER_CC_MANAGER_H