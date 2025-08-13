#pragma once

#include "fireboltaamp.h"

#include <memory>
#include <map>

/**
 * @brief FireboltCallbackName : names for firebolt callbacks
 */
enum FireboltCallbackName
{
    HDCP_CHANGED,
    NETWORK_CHANGED,
    VIDEO_RESOLUTION_CHANGED,
    WATERMARK_STATUS_CHANGED //keep in end to maintain count
};


class FireboltInterface{

    public:

        FireboltInterface(const FireboltInterface&) = delete;
        
        FireboltInterface& operator=(const FireboltInterface&) = delete;

        static std::shared_ptr<FireboltInterface> GetInstance();

        ~FireboltInterface();

    private:

        bool mIsConnected = false;

        unsigned int mListenerId;

        std::map<FireboltCallbackName, unsigned int> m_CallbackMap;

        FireboltInterface();

        bool CreateFireboltInstance(const std::string &url);

        void ConnectionChanged(const bool connected, int error);

        void DestroyFireboltInstance();

        void InitializeCallbackMap();

};