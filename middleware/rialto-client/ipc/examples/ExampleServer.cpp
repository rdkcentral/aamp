/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 Sky UK
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

#include <IIpcController.h>
#include <IIpcServer.h>
#include <IIpcServerFactory.h>
#include <RialtoLogging.h>

#include "example.pb.h"

#include <unistd.h>

class MyExampleServiceServer : public ::example::ExampleService
{
public:
    void exampleEcho(google::protobuf::RpcController *controller, const ::example::RequestEcho *request,
                     ::example::ResponseEcho *response, google::protobuf::Closure *done) override
    {
        // convert received string to upper case and send back
        std::string text = request->text();
        std::transform(text.begin(), text.end(), text.begin(), ::toupper);
        response->set_text(text);

        printf("ExampleService::exampleEcho({%s}) -> {%s}\n", request->DebugString().c_str(),
               response->DebugString().c_str());

        // if you wanted to report an error you'd do this
        // controller->SetFailed("Failed for some reason ...");

        // complete
        done->Run();
    }

    void exampleWithFd(google::protobuf::RpcController *controller, const ::example::RequestWithFd *request,
                       ::example::ResponseWithFd *response, google::protobuf::Closure *done) override
    {
        // expecting the request to contain an fd to stdout of the client, so just write a message to the client
        int fd = request->fd();
        if (dprintf(fd, "hello from the server\n") < 0)
            perror("dprintf failed\n");

        // reply with our own stdout fd
        response->set_fd(STDOUT_FILENO);

        printf("ExampleService::exampleWithFd({%s}) -> {%s}\n", request->DebugString().c_str(),
               response->DebugString().c_str());

        // complete
        done->Run();
    }

    void exampleWithNoReply(google::protobuf::RpcController *controller, const ::example::RequestWithNoReply *request,
                            ::example::EmptyResponse *response, google::protobuf::Closure *done) override
    {
        fprintf(stderr, "ExampleService::exampleWithNoReply({%s})\n", request->DebugString().c_str());

        // for 'no reply' calls both the response object and done closure are null
    }
};

// Example callback for when a client connects
static void clientConnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &client)
{
    printf("Client connected, pid:%d, uid:%d gid:%d\n", client->getClientPid(), client->getClientUserId(),
           client->getClientGroupId());

    // export the example server to the client
    client->exportService(std::make_shared<MyExampleService>());

    // example of sending an async event to the client
    auto someEvent = std::make_shared<::example::SomeEvent>();
    someEvent->set_id(1234);
    someEvent->set_text("some event");
    client->sendEvent(someEvent);
}

// Example callback for a client disconnects
static void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::IClient> &client)
{
    printf("Client disconnected, pid:%d, uid:%d gid:%d\n", client->getClientPid(), client->getClientUserId(),
           client->getClientGroupId());
}

int main(int argc, char *argv[])
{
    // verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // enable info level debugging
    firebolt::rialto::logging::setLogLevels(RIALTO_COMPONENT_IPC,
                                            RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_DEFAULT | RIALTO_DEBUG_LEVEL_INFO));

    auto factory = ::firebolt::rialto::ipc::IServerFactory::createFactory();
    auto server = factory->create();

    // add a listening socket for clients to connect to
    server->addSocket("/tmp/rialto.example.socket", &clientConnected, &clientDisconnected);

    // loop processing all event (alternatively you can call server->fd() to get a file descriptor to add to your
    // own poll loop and then when woken call server->process())
    while (server->process())
    {
        server->wait(-1);
    }

    return EXIT_SUCCESS;
}
