# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Native macOS player (AppKit) built on libaamp. Include paths, compile flags
# and AAMP_CLI_LD_FLAGS are inherited from the top-level CMakeLists.txt.
set(AAMP_PLAYER_SOURCES
	main.mm
	AppDelegate.mm
	PlayerEngine.mm
	VideoView.mm
	PlayerWindowController.mm
	DebugTerminalController.mm
	CommandInterpreter.mm
	SettingsStore.mm
	ContentCatalog.mm
	SettingsWindowController.mm
)

add_executable(aamp-player ${AAMP_PLAYER_SOURCES})
target_include_directories(aamp-player PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aamp-player
	aamp
	tsb
	${AAMP_CLI_LD_FLAGS}
	"-framework Cocoa"
	"-framework Accelerate"
	"-framework QuartzCore"
)
set_target_properties(aamp-player PROPERTIES
	COMPILE_FLAGS "${COMMON_COMPILE_FLAGS}"
	XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
)
xcode_define_schema(aamp-player)
