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

#include "IpcClientControllerImpl.h"
#include "IpcControllerFactory.h"
#include "IpcLogging.h"
#include <memory>
#include <utility>

namespace firebolt::rialto::ipc
{
std::shared_ptr<IControllerFactory> IControllerFactory::createFactory()
{
    std::shared_ptr<IControllerFactory> factory;

    try
    {
        factory = std::make_shared<ControllerFactory>();
    }
    catch (const std::exception &e)
    {
        RIALTO_IPC_LOG_ERROR("Failed to create the controller factory, reason: %s", e.what());
    }

    return factory;
}

std::shared_ptr<google::protobuf::RpcController> ControllerFactory::create()
{
    return std::make_shared<ClientControllerImpl>();
}

// -----------------------------------------------------------------------------
/*!
    \overload

    \quote
        Resets the RpcController to its initial state so that it may be reused in
        a new call.  Must not be called while an RPC is in progress.


 */
void ClientControllerImpl::Reset() // NOLINT(build/function_format)
{
    std::lock_guard<std::mutex> locker(m_lock);
    m_failed = false;
    m_reason.clear();
}

// -----------------------------------------------------------------------------
/*!
    \overload

    \quote
        After a call has finished, returns true if the call failed.  The possible
        reasons for failure depend on the RPC implementation.  Failed() must not
        be called before a call has finished.  If Failed() returns true, the
        contents of the response message are undefined.

 */
bool ClientControllerImpl::Failed() const // NOLINT(build/function_format)
{
    std::lock_guard<std::mutex> locker(m_lock);
    return m_failed;
}

// -----------------------------------------------------------------------------
/*!
    \overload

    \quote
        If Failed() is true, returns a human-readable description of the error.

 */
std::string ClientControllerImpl::ErrorText() const // NOLINT(build/function_format)
{
    std::lock_guard<std::mutex> locker(m_lock);
    return m_reason;
}

// -----------------------------------------------------------------------------
/*!
    \overload
    \warning Not implemented

    \quote
        Advises the RPC system that the caller desires that the RPC call be
        canceled.  The RPC system may cancel it immediately, may wait awhile and
        then cancel it, or may not even cancel the call at all.  If the call is
        canceled, the "done" callback will still be called and the RpcController
        will indicate that the call failed at that time.

 */
void ClientControllerImpl::StartCancel() // NOLINT(build/function_format)
{
    RIALTO_IPC_LOG_WARN("Cancel not implemented");
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from RialtoIpcChannelImpl when a method call failed for any reason.

 */
void ClientControllerImpl::setMethodCallFailed(std::string reason)
{
    std::lock_guard<std::mutex> locker(m_lock);
    m_failed = true;
    m_reason = std::move(reason);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from RialtoIpcChannelImpl when a method call failed for any reason.

 */
void ClientControllerImpl::setMethodCallFailed(const char *format, va_list ap)
{
    char buf[512];
    vsnprintf(buf, sizeof(buf), format, ap);

    std::lock_guard<std::mutex> locker(m_lock);
    m_failed = true;
    m_reason.assign(buf);
}

} // namespace firebolt::rialto::ipc
