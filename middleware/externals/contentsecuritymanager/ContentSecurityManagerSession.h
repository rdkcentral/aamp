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
 * limitations under the License.
*/

/**
 * @file ContentSecurityManagerSession.h
 * @brief Class to Represents an contentsecurity manager session
 */

#ifndef __CONTENT_SECURITY_MANAGER_SESSION_H__
#define __CONTENT_SECURITY_MANAGER_SESSION_H__

#include <mutex>
#include <sys/time.h>
#include <atomic>
#include <memory>
#include <sstream>

#define CONTENT_SECURITY_MGR_INVALID_SESSION_ID (-1)

class ContentSecurityManager;

/**
 * @brief Represents an player sec manager session, 
 * Sessions are automatically closed there are no ContentSecurityManagerSession objects that reference it*/
class ContentSecurityManagerSession
{
	/* The coupling between ContentSecurityManager & ContentSecurityManagerSession is not ideal from an architecture standpoint but
	 * it minimises changes to existing ContentSecurityManager code:
	 * ~SessionManager() calls ContentSecurityManager::ReleaseSession()
	 * ContentSecurityManager::acquireLicence() creates instances of ContentSecurityManagerSession*/
	friend ContentSecurityManager;
private:
	/**
	 * @brief Responsible for closing the corresponding sec manager sessions when it is no longer used
	 */
	class SessionManager
	{
		private:
		int64_t mID;	//set once undermutex in constructor
		std::atomic<std::size_t> mInputSummaryHash;	//can be changed by setInputSummaryHash
		SessionManager(int64_t sessionID, std::size_t inputSummaryHash);

		public:
		/**
		 * @fn getInstance
		 * @brief
		 * Get a shared pointer to an object corresponding to the sessionID, creating a new one if required
		*/
		static std::shared_ptr<ContentSecurityManagerSession::SessionManager> getInstance(int64_t sessionID, std::size_t inputSummaryHash);

		int64_t getID(){return mID;}
		std::size_t getInputSummaryHash(){return mInputSummaryHash.load();}
		void setInputSummaryHash(std::size_t inputSummaryHash);

		//calls ContentSecurityManager::ReleaseSession() on mID
		~SessionManager();
	};

	std::shared_ptr<ContentSecurityManagerSession::SessionManager> mpSessionManager;
	mutable std::mutex sessionIdMutex;

public:
	/**
 	* @brief constructor for valid objects
	* this will cause ContentSecurityManager::ReleaseSession() to be called on sessionID
	* when the last ContentSecurityManagerSession, referencing is destroyed
	* this is only intended to be used in ContentSecurityManager::acquireLicence()
	* it is the responsibility of ContentSecurityManager::acquireLicence() to ensure sessionID is valid
	*/
	ContentSecurityManagerSession(int64_t sessionID, std::size_t inputSummaryHash);

	/**
 	* @brief constructor for an invalid object*/
	ContentSecurityManagerSession(): mpSessionManager(), sessionIdMutex() {};

	//allow copying, the secManager session will only be closed when all copies have gone out of scope
	ContentSecurityManagerSession(const ContentSecurityManagerSession& other): mpSessionManager(), sessionIdMutex()
	{
		std::lock(sessionIdMutex, other.sessionIdMutex);
		std::lock_guard<std::mutex> thisLock(sessionIdMutex, std::adopt_lock);
		std::lock_guard<std::mutex> otherLock(other.sessionIdMutex, std::adopt_lock);
		mpSessionManager=other.mpSessionManager;
	}
	ContentSecurityManagerSession& operator=(const ContentSecurityManagerSession& other)
	{
		std::lock(sessionIdMutex, other.sessionIdMutex);
		std::lock_guard<std::mutex> thisLock(sessionIdMutex, std::adopt_lock);
		std::lock_guard<std::mutex> otherLock(other.sessionIdMutex, std::adopt_lock);
		mpSessionManager=other.mpSessionManager;
		return *this;
	}

	/**
	 * @fn getSessionID
	 * @brief
	 * returns the session ID value for use with JSON API
	 * The returned value should not be used outside the lifetime of
	 * the ContentSecurityManagerSession on which this method is called
	 * otherwise the session may be closed before the ID can be used
	 */
	int64_t getSessionID(void) const;
	std::size_t getInputSummaryHash();

	bool isSessionValid(void) const
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		return (mpSessionManager.use_count()!=0);
	}
	void setSessionInvalid(void)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		mpSessionManager.reset();
	}

	std::string ToString()
	{
		std::stringstream ss;
		ss<<"Session ";
		auto id = getSessionID();	//ID retrieved under mutex
		if(id != CONTENT_SECURITY_MGR_INVALID_SESSION_ID)
		{
			ss<<id<<" valid";
		}
		else
		{
			ss<<"invalid";
		}
		return ss.str();
	}
};

#endif /* __CONTENT_SECURITY_MANAGER_SESSION_H__ */
