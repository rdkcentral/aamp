#include "PlayerHlsDrmSessionInterface.h"

PlayerHlsDrmSessionInterface* pInstance = nullptr;

/**
 * @brief Constructor
 */
PlayerHlsDrmSessionInterface::PlayerHlsDrmSessionInterface()
{

}

/**
 * @brief getInstance Get DRM instance
 *        Get an instance of the Hls DRM Session Manager
 */
PlayerHlsDrmSessionInterface* PlayerHlsDrmSessionInterface::getInstance()
{
	if(pInstance == nullptr)
	{
		pInstance = new PlayerHlsDrmSessionInterface();
	}
	return pInstance;
}

/**
 * @brief Check stream is DRM supported
 */
bool PlayerHlsDrmSessionInterface::isDrmSupported(const struct DrmInfo& drmInfo) const
{
	return false;
}

/**
 * @brief createSession create session for DRM
 */
std::shared_ptr<HlsDrmBase> PlayerHlsDrmSessionInterface::createSession(const struct DrmInfo& drmInfo, int  streamTypeIn)
{
	return nullptr;
}

/**
 * @brief Registers GetAcessKey callback from application
 */
void PlayerHlsDrmSessionInterface::RegisterGetHlsDrmSessionCb(const GetHlsDrmSessionCallback Callback)
{
}