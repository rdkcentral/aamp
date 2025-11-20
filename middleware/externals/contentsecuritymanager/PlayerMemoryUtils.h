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
 * @file PlayerMemoryUtils.h
 * @brief Header file of helper functions for memory management
 */


#ifndef __PLAYER_MEMORY_UTILS_H__
#define __PLAYER_MEMORY_UTILS_H__

#include <stddef.h>
#include <sys/shm.h>
#define SHM_ACCESS_PERMISSION 0666
#define SHMGET_RETRY_MAX 10

/**
 * @fn player_CreateSharedMem
 * @param shmLen Length of the buffer to be created
 * @param shmKey shared memory key
 */
void * player_CreateSharedMem( size_t shmLen, key_t & shmKey);
/**
 * @fn player_CleanUpSharedMem
 * @param shmPointer Pointer to the created memory
 * @param shmKey shared memory key
 * @param shmLen Length of the buffer
 */
void player_CleanUpSharedMem(void* shmPointer, key_t shmKey, size_t shmLen);
#endif /* __PLAYER_MEMORY_UTILS_H__ */
