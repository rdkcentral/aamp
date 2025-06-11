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
 * @file ContentProtectionSession.h
 * @brief Class definitions for managing ContentProtection Sessions for secure playback 
 */

#ifndef __CONTENTPROTECTIONSESSION_H__
#define __CONTENTPROTECTIONSESSION_H__

#include <mutex>
#include <sys/time.h>
#include <atomic>
#include <memory>
#include <sstream>

#define CONTENT_PROTECTION_INVALID_SESSION_ID (-1)

class ContentProtectionBase;

/**
 * @class ContentProtectionSession
 * @brief Wrapper object to manage the lifetime and identity of a SecManager session
 *
 * When the last ContentProtectionSession referencing a valid session ID goes out of scope,
 * the session is automatically closed by its SessionManager.
 */
class ContentProtectionSession
{
	/* The coupling between ContentProtectionPriv & ContentProtectionSession is not ideal from an architecture standpoint but
	 * it minimises changes to existing ContentProtectionPriv code:
	 * ~SessionManager() calls ContentProtectionPriv::ReleaseSession()
	 * ContentProtectionPriv::acquireLicence() creates instances of ContentProtectionSession*/
	friend ContentProtectionBase;
	private:
	/**
	 * @brief Responsible for closing the corresponding sec manager sessions when it is no longer used
	 */
	class SessionManager
	{
		private:
			int64_t mID;	//set once undermutex in constructor
			std::atomic<std::size_t> mInputSummaryHash;	//can be changed by setInputSummaryHash
			/**
			 * @brief Constructs a SessionManager
			 * @param sessionID Unique identifier for the DRM session
			 * @param inputSummaryHash Hash of DRM metadata inputs
			 */
			SessionManager(int64_t sessionID, std::size_t inputSummaryHash);

		public:
			/**
			 * @brief Creates or reuses a SessionManager for a given session ID
			 * @param sessionID DRM session ID
			 * @param inputSummaryHash Metadata hash to detect session changes
			 * @return Shared pointer to a SessionManager instance
			 */
			static std::shared_ptr<ContentProtectionSession::SessionManager> getInstance(int64_t sessionID, std::size_t inputSummaryHash);
			/** @brief Returns the session ID */
			int64_t getID(){return mID;}
			/** @brief Returns the input metadata hash */
			std::size_t getInputSummaryHash()
			{
				return mInputSummaryHash.load();
			}
			/**
			 * @brief Updates the session's metadata hash (on license renewal)
			 * @param inputSummaryHash New hash to store
			 */
			void setInputSummaryHash(std::size_t inputSummaryHash);

			//calls ContentProtectionPriv::ReleaseSession() on mID
			~SessionManager();
	};

	std::shared_ptr<ContentProtectionSession::SessionManager> mpSessionManager;
	mutable std::mutex sessionIdMutex;

	/**
	 * @brief constructor for valid objects
	 * this will cause ContentProtectionPriv::ReleaseSession() to be called on sessionID
	 * when the last ContentProtectionSession, referencing is destroyed
	 * this is only intended to be used in ContentProtectionPriv::acquireLicence()
	 * it is the responsibility of ContentProtectionPriv::acquireLicence() to ensure sessionID is valid
	 */
	ContentProtectionSession(int64_t sessionID, std::size_t inputSummaryHash);
	public:
	/**
	 * @brief Default constructor for an invalid ContentProtectionSession
	 */
	ContentProtectionSession(): mpSessionManager(), sessionIdMutex() {};
	/**
	 * @brief Copy constructor
	 * allow copying, the secManager session will only be closed when all copies have gone out of scope
	 */
	ContentProtectionSession(const ContentProtectionSession& other): mpSessionManager(), sessionIdMutex()
	{
		std::lock(sessionIdMutex, other.sessionIdMutex);
		std::lock_guard<std::mutex> thisLock(sessionIdMutex, std::adopt_lock);
		std::lock_guard<std::mutex> otherLock(other.sessionIdMutex, std::adopt_lock);
		mpSessionManager=other.mpSessionManager;
	}
	/**
	 * @brief Copy assignment operator
	 */
	ContentProtectionSession& operator=(const ContentProtectionSession& other)
	{
		std::lock(sessionIdMutex, other.sessionIdMutex);
		std::lock_guard<std::mutex> thisLock(sessionIdMutex, std::adopt_lock);
		std::lock_guard<std::mutex> otherLock(other.sessionIdMutex, std::adopt_lock);
		mpSessionManager=other.mpSessionManager;
		return *this;
	}

	/**
	 * @fn getSessionID
	 * @brief  Gets the session ID (thread-safe)
	 * returns the session ID value for use with JSON API
	 * The returned value should not be used outside the lifetime of
	 * the ContentProtectionSession on which this method is called
	 * otherwise the session may be closed before the ID can be used
	 */
	int64_t getSessionID(void) const;
	/**
	 * @brief Gets the input metadata hash
	 * @return Hash value
	 */
	std::size_t getInputSummaryHash();

	/**
	 * @brief Checks whether this session is valid
	 * @return true if valid, false otherwise
	 */
	bool isSessionValid(void) const
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		return (mpSessionManager.use_count()!=0);
	}
	/**
	 * @brief Invalidates the session, dropping internal reference
	 */
	void setSessionInvalid(void)
	{
		std::lock_guard<std::mutex>lock(sessionIdMutex);
		mpSessionManager.reset();
	}

	/**
	 * @brief Returns string representation of session state
	 * @return Human-readable session ID and validity info
	 */
	std::string ToString()
	{
		std::stringstream ss;
		ss<<"Session ";
		auto id = getSessionID();	//ID retrieved under mutex
		if(id != CONTENT_PROTECTION_INVALID_SESSION_ID)
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

#endif /* __CONTENTPROTECTIONSESSION_H__ */
