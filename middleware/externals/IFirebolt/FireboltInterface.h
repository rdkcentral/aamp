#pragma once

#include "fireboltaamp.h"

#include <memory>

class FireboltInterface{

    public:

        FireboltInterface(const FireboltInterface&) = delete;
        
        FireboltInterface& operator=(const FireboltInterface&) = delete;

        static std::shared_ptr<FireboltInterface> GetInstance();

        ~FireboltInterface();

    private:

        bool mIsConnected = false;

        unsigned int mListenerId;

        FireboltInterface();

        bool CreateFireboltInstance(const std::string &url);

        void ConnectionChanged(const bool connected, int error);

        void DestroyFireboltInstance();

};