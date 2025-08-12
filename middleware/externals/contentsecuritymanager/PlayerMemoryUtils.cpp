/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file PlayerMemoryUtils.cpp
 * @brief Helper functions for memory management
 */

#include "PlayerMemoryUtils.h"
#include "PlayerLogManager.h"
#include <assert.h>
#include <glib.h>
#include <errno.h>

/**
 * @brief Creates shared memory and provides the key
 */
void * player_CreateSharedMem( size_t shmLen, key_t & shmKey)
{
	void *shmPointer = NULL;
	if( shmLen > 0)
	{
		for( int retryCount=0; retryCount<SHMGET_RETRY_MAX; retryCount++ )
		{
			// generate pseudo-random value to use as a unique key
			shmKey = rand() + 1; // must be non-zero to allow access to non-child processes
			
			// allocate memory segment and identifier
			int shmId = shmget(shmKey, shmLen,
							   IPC_CREAT | // create new segment
							   IPC_EXCL | // fail if already exists
							   SHM_ACCESS_PERMISSION); // owner, group, other permissions
			if (shmId != -1 )
			{ // map newly allocated segment to shared memory pointer
				shmPointer = shmat(shmId, NULL, 0 );
				if( shmPointer != (void *)-1 )
				{ // success!
					MW_LOG_WARN("Shared memory shmId=%d ptr=%p created for the key, %u",
								 shmId, shmPointer, shmKey);
				}
				else
				{
					MW_LOG_ERR("shmat err=%d shmId=%d", errno, shmId );
					player_CleanUpSharedMem( shmPointer, shmKey, shmLen);
					shmPointer = NULL;
				}
				break;
			}
			else
			{
				MW_LOG_ERR("shmget err=%d", errno);
			}
		}
	}
	else
	{
		MW_LOG_ERR("invalid shmLen=%zu", shmLen);
	}
	return shmPointer;
}

/**
 * @brief detach and delete shared memory
 */
void player_CleanUpSharedMem(void* shmPointer, key_t shmKey, size_t shmLen)
{
	if( NULL != shmPointer && (void*)-1 != shmPointer)
	{ // detach shared memory
		if( -1 == shmdt(shmPointer) )
		{
			MW_LOG_ERR("shmdt failure %d for key %u", errno, shmKey);
		}
		int shmId = shmget(shmKey, shmLen, SHM_ACCESS_PERMISSION);
		if( shmId != -1 )
		{ // mark segment to be destroyed
			if( -1 == shmctl(shmId, IPC_RMID, NULL) )
			{
				MW_LOG_ERR("shmctl err=%d shmId=%d", errno, shmId );
			}
		}
		else
		{
			MW_LOG_ERR("bad shmKey=%u", shmKey);
		}
	}
	else
	{
		MW_LOG_ERR("bad shmPointer=%p", shmPointer );
	}
}

