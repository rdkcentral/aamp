/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * limitations under the License.m
*/

/**
 * @file ContentSecurityManagerSession.cpp
 * @brief Class impl for ContentSecurityManagerSession
 */

#include "ContentSecurityManager.h"
#include "PlayerLogManager.h"
std::shared_ptr<ContentSecurityManagerSession::SessionManager> ContentSecurityManagerSession::SessionManager::getInstance(int64_t sessionID, std::size_t inputSummaryHash)
{
        std::shared_ptr<SessionManager> returnValue;

        static std::mutex instancesMutex;
        std::lock_guard<std::mutex> lock{instancesMutex};
        static std::map<int64_t, std::weak_ptr<SessionManager>> instances;

        //Remove pointers to expired instances
        {
                std::vector<int64_t> keysToRemove;
                for (auto i : instances)
                {
                        if(i.second.expired())
                        {
                                keysToRemove.push_back(i.first);
                        }
                }
                if(keysToRemove.size())
                {
                        std::stringstream ss;
                        ss<<"ContentSecurityManagerSession: "<<keysToRemove.size()<<" expired (";
                        for(auto key:keysToRemove)
                        {
                                ss<<key<<",";
                                instances.erase(key);
                        }

                        ss<<"), instances remaining."<< instances.size();
                        MW_LOG_MIL("%s",ss.str().c_str());
                }
        }

        /* Only create or retrieve instances for valid sessionIDs.
         * <0 is used as an invalid value e.g. CONTENT_SECURITY_MGR_INVALID_SESSION_ID
         * 0 removes all sessions which is not the intended behaviour here*/
        if(sessionID>0)
        {
                if(instances.count(sessionID)>0)
                {
                        //get an existing pointer which may be no longer valid
                        returnValue = instances[sessionID].lock();

                        if(!returnValue)
                        {
                                //unexpected
                                MW_LOG_WARN("ContentSecurityManagerSession: session ID %" PRId64 " reused or session closed too early.",
                                sessionID);
                        }
                }

                if(returnValue)
                {
                        if(returnValue->getInputSummaryHash()!=inputSummaryHash)
                        {
                                //this should only occur after a successful updatePlaybackSession
                                MW_LOG_MIL("ContentSecurityManagerSession: session ID %" PRId64 " input data changed.", sessionID);
                                returnValue->setInputSummaryHash(inputSummaryHash);
                        }
                }
                else
                {
                        /* where an existing, valid instance is not available for sessionID
                        * create a new instance & save a pointer to it for possible future reuse*/
                        returnValue.reset(new SessionManager{sessionID, inputSummaryHash});
                        instances[sessionID] = returnValue;
                        MW_LOG_WARN("ContentSecurityManagerSession: new instance created for ID:%" PRId64 ", %zu instances total.",
                        sessionID,
                        instances.size());
                }
        }
        else
        {
                MW_LOG_WARN("ContentSecurityManagerSession: invalid ID:%" PRId64 ".", sessionID);
        }

        return returnValue;
}

ContentSecurityManagerSession::SessionManager::~SessionManager()
{
        if(mID>0)
        {
                ContentSecurityManager::GetInstance()->ReleaseSession(mID);
        }
}
void ContentSecurityManagerSession::SessionManager::setInputSummaryHash(std::size_t inputSummaryHash)
{
        mInputSummaryHash=inputSummaryHash;
        std::stringstream ss;
        ss<<"Input summary hash updated to: "<<inputSummaryHash << "for ID "<<mID;
        MW_LOG_MIL("%s", ss.str().c_str());
}


ContentSecurityManagerSession::SessionManager::SessionManager(int64_t sessionID, std::size_t inputSummaryHash):
mID(sessionID),
mInputSummaryHash(inputSummaryHash)
{};

ContentSecurityManagerSession::ContentSecurityManagerSession(int64_t sessionID, std::size_t inputSummaryHash):
mpSessionManager(ContentSecurityManagerSession::SessionManager::getInstance(sessionID, inputSummaryHash)),
sessionIdMutex()
{};

int64_t ContentSecurityManagerSession::getSessionID(void) const
{
        std::lock_guard<std::mutex>lock(sessionIdMutex);
        int64_t ID = CONTENT_SECURITY_MGR_INVALID_SESSION_ID;
        if(mpSessionManager)
        {
                ID = mpSessionManager->getID();
        }

        return ID;
}

std::size_t ContentSecurityManagerSession::getInputSummaryHash()
{
        std::lock_guard<std::mutex>lock(sessionIdMutex);
        std::size_t hash=0;
        if(mpSessionManager)
        {
                hash = mpSessionManager->getInputSummaryHash();
        }

        return hash;
}

