/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 
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

#ifndef FIREBOLT_RIALTO_CONTROL_COMMON_H_
#define FIREBOLT_RIALTO_CONTROL_COMMON_H_

/**
 * @file ControlCommon.h
 *
 * The definition of the Control Common types
 *
 */

#include <stddef.h>
#include <stdint.h>

namespace firebolt::rialto
{
/**
 * @brief The application state.
 */
enum class ApplicationState
{
    UNKNOWN,
    RUNNING,
    INACTIVE
};
} // namespace firebolt::rialto

#endif // FIREBOLT_RIALTO_CONTROL_COMMON_H_
