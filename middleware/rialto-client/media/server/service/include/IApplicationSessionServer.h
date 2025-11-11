/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBOLT_RIALTO_SERVER_I_APPLICATION_SESSION_SERVER_H_
#define FIREBOLT_RIALTO_SERVER_I_APPLICATION_SESSION_SERVER_H_

#include <memory>

namespace firebolt::rialto::server
{
class IApplicationSessionServer;
class IApplicationSessionServerFactory
{
public:
    virtual ~IApplicationSessionServerFactory() = default;
    static std::unique_ptr<IApplicationSessionServerFactory> getFactory();
    virtual std::unique_ptr<IApplicationSessionServer> createApplicationSessionServer() const = 0;
};

class IApplicationSessionServer
{
public:
    virtual ~IApplicationSessionServer() = default;
    virtual bool init(int argc, char *argv[]) = 0;
    virtual void startService() = 0;
};
} // namespace firebolt::rialto::server

#endif // FIREBOLT_RIALTO_SERVER_I_APPLICATION_SESSION_SERVER_H_
