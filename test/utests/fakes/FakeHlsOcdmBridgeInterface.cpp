#include "HlsOcdmBridgeInterface.h"

/**
 * @class FakeHlsOcdmBridge
 * @brief OCDM bridge to handle DRM key
 */

class AampFakeHlsOcdmBridge : public HlsDrmBase
{
public:
	AampFakeHlsOcdmBridge(DrmSession * DrmSession){}

	virtual ~AampFakeHlsOcdmBridge(){}

	AampFakeHlsOcdmBridge(const AampFakeHlsOcdmBridge&) = delete;

	AampFakeHlsOcdmBridge& operator=(const AampFakeHlsOcdmBridge&) = delete;

	/*HlsDrmBase Methods*/

	virtual DrmReturn SetMetaData(void* metadata,int trackType) override {return DrmReturn::eDRM_ERROR;}

	virtual DrmReturn SetDecryptInfo(const struct DrmInfo *drmInfo, int acquireKeyWaitTime) override {return DrmReturn::eDRM_ERROR;}

	virtual DrmReturn Decrypt(int bucketType, void *encryptedDataPtr, size_t encryptedDataLen, int timeInMs = DECRYPT_WAIT_TIME_MS) override {return DrmReturn::eDRM_ERROR;}

	virtual void Release() override {}

	virtual void CancelKeyWait() override {}

	virtual void RestoreKeyState() override {}

	virtual void AcquireKey(void *metadata,int trackType) override {}

	virtual DRMState GetState() override {return DRMState::eDRM_KEY_FAILED;}

};

HlsDrmBase* HlsOcdmBridgeInterface::GetBridge(DrmSession * playerDrmSession)
{
   
    return new AampFakeHlsOcdmBridge(playerDrmSession);

}