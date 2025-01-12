/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#ifndef AAMP_HANDLER_CONTROL_H
#define AAMP_HANDLER_CONTROL_H

#include <mutex>
#include <condition_variable>

#include "AampConfig.h"

/**
 * @brief AampHandlerControl boilerplate to include at the outer scope of the handler/callback to be managed by a corresponding AampHandlerControl
 * Note these macros are intentionally not wrapped in {} as the scopeHelper must remain at the outer scope of the handler/callback
 */
#define HANDLER_CONTROL_HELPER(HANDLER_CONTROL, RTN) auto scopeHelper = HANDLER_CONTROL.getScopeHelper(); \
								if(scopeHelper.returnStraightAway()) return RTN;
#define HANDLER_CONTROL_HELPER_CALLBACK_VOID() auto scopeHelper = _this->privateContext->callbackControl.getScopeHelper(); \
								if(scopeHelper.returnStraightAway()) return;

/**
 * @class AampHandlerControl
 * @brief Provides a thread safe way of disabling & confirming that handlers or callbacks are disabled (i.e. AampHandlerControl::done()) when
 * the corresponding handlers implement the boiler plate using AampHandlerControl::getScopeHelper() & AampHandlerControl::ScopeHelper::returnStraightAway()
 */
class AampHandlerControl{
	public:

	bool isEnabled() const;

	/**
	 * @class AampHandlerControl::ScopeHelper
	 * @brief An object that Indicates if the handler is permitted to perform any action (i.e. via AampHandlerControl::ScopeHelper::returnStraightAway()) and
	 * signals to its corresponging AampHandlerControl object when the handler is complete (so that AampHandlerControl can maintain an accurate count of the number of handlers running)
	 * ScopeHelper instances should only be accessed from one thread i.e. the corresponding handler.
	 * AampHandlerControl's methods are threadsafe.
	 * The lifetime of referenced AampHandlerControl objects must exceed the lifetime of referencing ScopeHelpers.
	 */
	class ScopeHelper
	{
		private:
		AampHandlerControl* mpController;

		public:
		ScopeHelper(): mpController(nullptr){/*empty*/}

		explicit ScopeHelper(AampHandlerControl* pController):mpController(pController){/*empty*/}

		/**
		 * @brief move constructor
		 * The new object takes on the responsibility for signaling any corresponding AampHandlerControl from 'other'.
		 * After this function 'other' will not reference or signal any AampHandlerControl.  It's scope is irrelevent.
		 */
		ScopeHelper(ScopeHelper&& other): mpController(nullptr)
		{
			mpController = other.mpController;
			other.mpController = nullptr;	//prevents ScopeHelper::handlerEnd() being called by other
		}

		/**
		 * @brief move assignment
		 * The assigned to object will signal any currently referened AampHandlerControl first.  Then
		 * the assigned to object will take on the responsibility any AampHandlerControl referenced by other.
		 * After this function 'other' will not reference or signal any AampHandlerControl.  It's scope is irrelevent.
		 */
		ScopeHelper& operator=(AampHandlerControl::ScopeHelper&& other);

		//not copyable this would invalidate the count held in AampHandlerControl
		ScopeHelper(const ScopeHelper& other)=delete;
		ScopeHelper& operator=(const ScopeHelper& other)=delete;
		~ScopeHelper()
		{
			if(mpController)
			{
				mpController->handlerEnd();
			}
		}

		/**
		 * @brief should be called at the start of each handler e.g. using HANDLER_CONTROL_HELPER
		 * @return returns true when the handler should exit straight away without performing any action
		 */
		bool returnStraightAway() const
		{
			auto pController = mpController;
			return !(pController && pController->isEnabled());
		}
	};

	private:
	bool mEnabled;
	int mInstanceCount;
	mutable std::mutex mSync;
	std::condition_variable mDoneCond;

	/**
	 * @brief Used by ScopeDetecter destructor to indicate that its associated handler has exited
	 */
	void handlerEnd();

	public:
	AampHandlerControl():mEnabled(true), mInstanceCount(0), mSync(), mDoneCond()
	{
		//empty
	}

	/**
	 * @brief Indicate to new instances of the handler to run i.e. new ScopeHelpers will return false from ScopeDetecter::returnStraightAway()
	 */
	void enable()
	{
		std::lock_guard<std::mutex> guard(mSync);
		mEnabled = true;
	}


	/**
	 * @brief Indicate that any future handlers should return without performing any action i.e. new ScopeHelpers will return true from ScopeDetecter::returnStraightAway()
	 */
	void disable()
	{
		std::lock_guard<std::mutex> guard(mSync);
		mEnabled = false;
	}

	/**
	 * @brief The number of instances of the handler currently running (+1 by getScopeHelper(), -1 when the returned ScopeHelper goes out of scope)
	 */
	int instancesRunning()
	{
		std::lock_guard<std::mutex> guard(mSync);
		return mInstanceCount;
	}
	/**
	 * @brief Call at the start of the handler, and store the return value in a local variable with handler scope.
	 * The returnStraightAway() method of the returned object should be called straight afterwards. e.g. using HANDLER_CONTROL_HELPER
	 * @return A AampHandlerControl::ScopeHelper linked to this object
	 */
	ScopeHelper getScopeHelper();

	/**
	 * @brief
	 * call disable(), & wait for up to
	 * MaximumDelayMilliseconds for handlers to exit.
	 * log an error message if handlers are still running after MaximumDelayMilliseconds
	 * @return true if no handlers are running when the function exits e.g. instancesRunning() ==0
	 */
	bool waitForDone(int MaximumDelayMilliseconds, std::string errormessage);
};

#endif /* AAMP_HANDLER_CONTROL_H  */
