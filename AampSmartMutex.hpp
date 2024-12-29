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

#ifndef AampSmartMutex_h
#define AampSmartMutex_h

#include "AampLogManager.h"
#include <time.h>
#include <sys/time.h>
#include <chrono>
#include <mutex>

/**
  * usage:
  * 1. AampSmartMutex mMutex("myMutex"); // declare named mutex
  * 2. to lock, use LOCK_SMARTMUTEX(mMutex) macro
  * 3. to unlock, call mMutex.unlock() method
  */
const int DEFAULT_MUTEX_WARN_THRESHOLD_MS = 50;

class AampSmartMutex
{
private:
	std::timed_mutex mutex;
	std::string owner;
	const char *name;
	int default_warn_threshold_ms;
	
	long long GetCurrentTimeMS(void)
	{
		struct timeval t;
		gettimeofday(&t, NULL);
		return (long long)(t.tv_sec*1e3 + t.tv_usec*1e-3);
	}
	
public:
	AampSmartMutex( const char *name, int default_warn_threshold_ms = DEFAULT_MUTEX_WARN_THRESHOLD_MS ) : name(name), default_warn_threshold_ms(default_warn_threshold_ms)
	{
	}
	
	~AampSmartMutex()
	{
	}
	
	void _lock( const char *function, size_t line )
	{
		bool slow = false;
		std::string caller = std::string(function)+":"+std::to_string(line);
		long long tStart = GetCurrentTimeMS();
		int attempt = 1;
		std::chrono::milliseconds timeout(default_warn_threshold_ms);
		for(;;)
		{
			if( mutex.try_lock_for(timeout) )
			{ // success
				owner = caller;
				if( slow )
				{
					AAMPLOG_WARN( "%s acquire %s from %s took %lld ms",
						caller.c_str(), name, owner.c_str(), GetCurrentTimeMS() - tStart );
				}
				return;
			}
			else
			{ // fail
				slow = true;
				AAMPLOG_WARN( "%s not yet able to acquire %s from %s (attempt#%d)",
					   caller.c_str(), name, owner.c_str(), attempt++ );
				timeout *= 2;
			}
		}
	}
	
	void unlock( void )
	{
		assert( owner.size()>0 );
		owner.clear();
		mutex.unlock();
	}
};

void LOCK_SMARTMUTEX( AampSmartMutex &m );
#define LOCK_SMARTMUTEX(MUTEX) MUTEX._lock( __FUNCTION__, __LINE__ )

#endif /* AampSmartMutex_h */
