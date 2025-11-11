
#include "IControl.h"
#include "IMediaKeys.h"
#include <memory>
#include <string>
#include <vector>

namespace firebolt::rialto
{
// ---- IControl stub ----

class ControlFactoryStub : public IControlFactory
{
public:
    std::shared_ptr<IControl> createControl() const override
    {
        struct ControlStub : public IControl
        {
            bool registerClient(std::weak_ptr<IControlClient> /*client*/, ApplicationState &appState) override
            {
                // Leave application state UNKNOWN for stub
                appState = ApplicationState::UNKNOWN;
                return true; // pretend success
            }
        };
        return std::make_shared<ControlStub>();
    }
};

std::shared_ptr<IControlFactory> IControlFactory::createFactory()
{
    return std::make_shared<ControlFactoryStub>();
}

// ---- IMediaKeys stub ----
class MediaKeysFactoryStub : public IMediaKeysFactory
{
public:
    std::unique_ptr<IMediaKeys> createMediaKeys(const std::string & /*keySystem*/) const override
    {
        struct MediaKeysStub : public IMediaKeys
        {
            MediaKeyErrorStatus selectKeyId(int32_t /*keySessionId*/, const std::vector<uint8_t> &/*keyId*/) override { return MediaKeyErrorStatus::FAIL; }
            bool containsKey(int32_t /*keySessionId*/, const std::vector<uint8_t> &/*keyId*/) override { return false; }
            MediaKeyErrorStatus createKeySession(KeySessionType /*sessionType*/, std::weak_ptr<IMediaKeysClient> /*client*/, bool /*isLDL*/, int32_t &keySessionId) override { keySessionId = -1; return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus generateRequest(int32_t /*keySessionId*/, InitDataType /*initDataType*/, const std::vector<uint8_t> &/*initData*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus loadSession(int32_t /*keySessionId*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus updateSession(int32_t /*keySessionId*/, const std::vector<uint8_t> &/*responseData*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus setDrmHeader(int32_t /*keySessionId*/, const std::vector<uint8_t> &/*requestData*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus closeKeySession(int32_t /*keySessionId*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus removeKeySession(int32_t /*keySessionId*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus deleteDrmStore() override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus deleteKeyStore() override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getDrmStoreHash(std::vector<unsigned char> &/*drmStoreHash*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getKeyStoreHash(std::vector<unsigned char> &/*keyStoreHash*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getLdlSessionsLimit(uint32_t &ldlLimit) override { ldlLimit = 0; return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getLastDrmError(int32_t /*keySessionId*/, uint32_t &errorCode) override { errorCode = 0; return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getDrmTime(uint64_t &drmTime) override { drmTime = 0; return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getCdmKeySessionId(int32_t /*keySessionId*/, std::string &cdmKeySessionId) override { cdmKeySessionId.clear(); return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus releaseKeySession(int32_t /*keySessionId*/) override { return MediaKeyErrorStatus::FAIL; }
            MediaKeyErrorStatus getMetricSystemData(std::vector<uint8_t> &/*buffer*/) override { return MediaKeyErrorStatus::FAIL; }
        };
        return std::make_unique<MediaKeysStub>();
    }
};

std::shared_ptr<IMediaKeysFactory> IMediaKeysFactory::createFactory()
{
    return std::make_shared<MediaKeysFactoryStub>();
}

} // namespace firebolt::rialto
