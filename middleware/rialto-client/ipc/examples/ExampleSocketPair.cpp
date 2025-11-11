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

#include <IIpcChannel.h>
#include <IIpcController.h>
#include <IIpcControllerFactory.h>
#include <IIpcServer.h>
#include <IIpcServerFactory.h>
#include <RialtoLogging.h>

#include "example.pb.h"

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

class MyExampleService : public ::example::ExampleService
{
public:
    void exampleEcho(google::protobuf::RpcController *controller, const ::example::RequestEcho *request,
                     ::example::ResponseEcho *response, google::protobuf::Closure *done) override
    {
        // convert received string to upper case and send back
        std::string text = request->text();
        std::transform(text.begin(), text.end(), text.begin(), ::toupper);
        response->set_text(text);

        printf("[server] ExampleService::exampleEcho({%s}) -> {%s}\n", request->DebugString().c_str(),
               response->DebugString().c_str());

        // if you wanted to report an error you'd do this
        // controller->SetFailed("Failed for some reason ...");

        // complete
        done->Run();
    }
};

// Callback called when the RPC call completes with either a valid result or error
static void onComplete(bool *done)
{
    *done = true;
}

// Simple 'main' for the spawned example
static int childProcessMain(int sock)
{
    // create a channel to a socket created by the server
    auto factory = firebolt::rialto::ipc::IChannelFactory::createFactory();
    auto channel = factory->createChannel(sock);
    if (!channel)
    {
        fprintf(stderr, "[child] Failed to connect to socket fd '%d'\n", sock);
        return EXIT_FAILURE;
    }

    // create the stub for rpc calls
    ::example::ExampleService::Stub stub(channel.get());

    // make a basic call and get the result polling on the channel
    {
        printf("[child] Making IPC call ExampleService.exampleEcho()\n");

        ::example::RequestEcho request;
        ::example::ResponseEcho response;

        // populate the request message
        request.set_text("the significant owl hoots in the night");

        // create a controller which is used to check for any errors or cancel the rpc call
        auto controllerFactory = IControllerFactory::createFactory();
        auto controller = controllerFactory->create();

        // make the rpc call, onComplete will be called when it completes
        bool done = false;
        stub.exampleEcho(controller.get(), &request, &response, google::protobuf::NewCallback(onComplete, &done));

        // process the event loop until the RPC call completes
        while (channel->process() && !done)
        {
            channel->wait(-1);
        }

        // check if the call failed with an error
        if (controller->Failed())
        {
            fprintf(stderr, "[child] \texampleEcho IPC call failed with error '%s'\n", controller->ErrorText().c_str());
        }
        else
        {
            printf("[child] \treceived response '%s'\n", response.DebugString().c_str());
        }
    }
}

// Example of spawning a client and giving it the socket to talk back the server on
static pid_t spawnClient(int sock)
{
    // fork and exec the process
    pid_t pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork failed - %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0)
    {
        // within spawned process
        int ret = childProcessMain(sock);
        _exit(ret);
    }
    else
    {
        // still in parent, close the socket
        close(sock);
    }

    return pid;
}

// Example callback for a client disconnects
static void clientDisconnected(const std::shared_ptr<::firebolt::rialto::ipc::Client> &client)
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

    // create a socket pair, one for the server and one for the spawned client
    int socks[2] = {-1, -1};
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, socks) < 0)
    {
        fprintf(stderr, "socketpair failed - %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // give one socket to the server
    auto client = server->addClient(socks[0], &clientDisconnected);
    if (!client)
    {
        return EXIT_FAILURE;
    }

    // export the example service to the client
    client->exportService(std::make_shared<MyExampleService>());

    // spawn off a client process, giving it the socket fd
    pid_t childPid = spawnClient(socks[1]);
    if (childPid <= 0)
    {
        return EXIT_FAILURE;
    }

    // loop processing all event (alternatively you can call server->fd() to get a file descriptor to add to your
    // own poll loop and then when woken call server->process())
    while (server->process() && client->isConnected())
    {
        server->wait(-1);
    }

    // ensure we reap the child process
    int status;
    if (waitpid(childPid, &status, 0) < 0)
    {
        fprintf(stderr, "waitpid failed - %s\n", strerror(errno));
    }

    return EXIT_SUCCESS;
}
