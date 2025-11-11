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
#include <IIpcControllerFactory.h>
#include <RialtoLogging.h>

#include "example.pb.h"

#include <unistd.h>

#include <thread>

// Callback invoked when the given event is received from the server
static void onSomeEvent(const std::shared_ptr<::example::SomeEvent> &event)
{
    printf("received event from server : %s\n", event->DebugString().c_str());
}

// Callback called when the RPC call completes with either a valid result or error
static void onComplete(bool *done)
{
    *done = true;
}

// Example thread to show how you process the ipc channel event loop
static void runChannelEventLoop(const std::shared_ptr<firebolt::rialto::ipc::IChannel> &channel)
{
    printf("Starting thread to process ipc channel events\n");

    while (channel->wait(-1))
    {
        if (!channel->process())
            break;
    }

    printf("Stopping ipc process thread - channel disconnected\n");
}

int main(int argc, char *argv[])
{
    // verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // enable info level debugging
    firebolt::rialto::logging::setLogLevels(RIALTO_COMPONENT_IPC,
                                            RIALTO_DEBUG_LEVEL(RIALTO_DEBUG_LEVEL_DEFAULT | RIALTO_DEBUG_LEVEL_INFO));

    // connect to a socket created by the server
    auto factory = firebolt::rialto::ipc::IChannelFactory::createFactory();
    auto channel = factory->createChannel("/tmp/rialto.example.socket");
    if (!channel)
    {
        fprintf(stderr, "Failed to connect to socket '/tmp/rialto.example.socket', is ExampleServer running?\n");
        return EXIT_FAILURE;
    }

    // subscribe to an event from the server
    channel->subscribe<::example::SomeEvent>(&onSomeEvent);

    // create the stub for rpc calls
    ::example::ExampleService::Stub stub(channel.get());

    // make a basic call and get the result polling on the channel
    {
        printf("Making RPC call ExampleService.exampleEcho()\n");

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
            fprintf(stderr, "\texampleEcho IPC call failed with error '%s'\n", controller->ErrorText().c_str());
        }
        else
        {
            printf("\treceived response '%s'\n", response.DebugString().c_str());
        }
    }

    // example of spawning a thread to run the event loop
    std::thread loop = std::thread(runChannelEventLoop, channel);

    // example making a call with no reply expected
    {
        printf("Making RPC call ExampleService.exampleWithNoReply()\n");

        ::example::RequestWithNoReply request;

        // populate the request message
        request.set_text("request with no reply expected");

        // create a controller which is used to check for any errors or cancel the rpc call
        auto controllerFactory = IControllerFactory::createFactory();
        auto controller = controllerFactory->create();

        // make the rpc call, onComplete will be called when it completes
        stub.exampleWithNoReply(controller.get(), &request, nullptr, nullptr);

        // check if the call failed - this will only happen if there was an issue sending the request (ie. channel disconnected)
        if (controller->Failed())
        {
            fprintf(stderr, "\texampleWithNoReply IPC call failed with error '%s'\n", controller->ErrorText().c_str());
        }
        else
        {
            printf("\trequest send\n");
        }
    }

    // example making a call sending and receiving file descriptors
    {
        printf("Making RPC call ExampleService.exampleWithFd()\n");

        ::example::RequestWithFd request;
        ::example::ResponseWithFd response;

        // populate the request message
        request.set_fd(STDOUT_FILENO);
        request.set_text("some test");

        // create a controller which is used to check for any errors or cancel the rpc call
        auto controllerFactory = IControllerFactory::createFactory();
        auto controller = controllerFactory->create();

        // make the rpc call, onComplete will be called when it completes
        bool done = false;
        stub.exampleWithFd(controller.get(), &request, &response, google::protobuf::NewCallback(onComplete, &done));

        // the complete callback will now be called on the runChannelEventLoop thread, so we just poll on done
        // being set to true (this is not truely thread safe but provides a simple example)
        while (!done)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        // check if the call failed with an error
        if (controller->Failed())
        {
            fprintf(stderr, "\texampleWithFd IPC call failed with error '%s'\n", controller->ErrorText().c_str());
        }
        else
        {
            printf("\treceived response '%s'\n", response.DebugString().c_str());

            // it is the callers responsibility to close the received file descriptor
            if (close(response.fd()) != 0)
                fprintf(stderr, "\tfailed to close the fd received from the server\n");
        }
    }

    // disconnect from the server
    channel->disconnect();

    // this should terminate the processing thread
    loop.join();

    return EXIT_SUCCESS;
}
