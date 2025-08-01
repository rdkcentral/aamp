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
 * @file playerExternalUtils.h
 * @brief Utilities for external's functionalities
 */

#ifndef PLAYER_EXTERNAL_UTILS
#define PLAYER_EXTERNAL_UTILS


/**
 * Hack to check if code is running in container environment.
 * @return True if running in container environment, false otherwise.
 */
bool IsContainerEnvironment(void);
/**
 * @brief Sleep for given milliseconds
 */
void ms_sleep(int milliseconds);
#endif
