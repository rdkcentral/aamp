#ifndef DEVICE_FIREBOLT_INTERFACE_H
#define DEVICE_FIREBOLT_INTERFACE_H

#include "DeviceInterfaceBase.h"
#include "FireboltInterface.h"

#include <string>
#include <vector>

class DeviceFireboltInterface : public DeviceInterfaceBase {

    
    public:

        DeviceFireboltInterface(const DeviceFireboltInterface&) = delete;
        
        DeviceFireboltInterface& operator=(const DeviceFireboltInterface&) = delete;

        void RegisterDsMgrEventHandler() override;

        void RemoveDsMgrEventHandler() override;

        bool IsActiveStreamingInterfaceWifi() override;

        char *GetTR181Config(const char * paramName, size_t & iConfigLen) override;

        static std::shared_ptr<DeviceFireboltInterface> GetInstance();

    private:

        bool mIsConnected = false;

        unsigned int mListenerId;

        std::shared_ptr<FireboltInterface> m_pFireboltInterface;

        std::vector<uint64_t> mDsMgrSubscriptionId;

        std::vector<uint64_t> mNtwMgrSubscriptionId;

        DeviceFireboltInterface();

        bool CreateFireboltInstance(const std::string &url);

        void ConnectionChanged(const bool connected, int error);

        void DestroyFireboltInstance();

};

#endif