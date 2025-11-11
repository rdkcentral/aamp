/*
 * Copyright (C) 2024   
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once
#include <MediaCommon.h>

#include <cstdint>

constexpr double kDefaultVolume{1.0};
constexpr uint32_t kDefaultVolumeDuration{0};
constexpr firebolt::rialto::EaseType kDefaultEaseType{firebolt::rialto::EaseType::EASE_LINEAR};
constexpr bool kDefaultMute{false};
constexpr bool kDefaultLowLatency{false};
constexpr bool kDefaultSync{false};
constexpr bool kDefaultSyncOff{false};
constexpr int32_t kDefaultStreamSyncMode{0};
constexpr uint32_t kDefaultFadeVolume{0};
constexpr const char *kDefaultAudioFade = "100,0,L";
constexpr uint32_t kDefaultBufferingLimit{750};
constexpr bool kDefaultUseBuffering{false};
