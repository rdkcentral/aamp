#!/usr/bin/env python3
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

# Utility class to verify acceptance criteria of AAMP TSB in L2 tests.
###############################################################################

class AampTsbUtils:
	def __init__(self):
		self.seek_position = 0
		self.first_progress_event_position = 0.0
		self.POSITION_TOLERANCE_RW_SEEK = 0
		self.time_stamp_at_trick_play_secs = 0.0
		self.flag_time_stamp_at_trick_play_achieved = False
		self.flag_check_seek_position = False

	# Continously gets time stamp of log until flag_time_stamp_at_trick_play_achieved is set
	def get_time_stamp_at_trick_play(self, match, arg):
		if not self.flag_time_stamp_at_trick_play_achieved:
			self.time_stamp_at_trick_play_secs = float(match.group(1))

	# flag_time_stamp_at_trick_play_achieved is set at AAMP_EVENT_SPEED_CHANGED event
	def set_flag_time_stamp_at_trick_play_achieved(self, match, arg):
		self.flag_time_stamp_at_trick_play_achieved = True

	# Set Flag to check seek position, once the seek operation has done
	def set_flag_check_seek_position(self, match, arg):
		self.flag_check_seek_position = True

	# Extracts the play position after speed changes to x 1
	# If speed change is due to exiting from forward trick play, compares difference in play position to difference in time stamp between trickmode and play
	# If the speed change is due to existing from rewind trick play, the position now recorded must be as seen after tune time.
	# If the speed change is due to seek. current position must be equal to seek position + position at tune time
	def check_current_play_position(self, match, direction_arg):

		if isinstance(direction_arg, tuple):
			direction, ff_trick_mode_speed = direction_arg
		else:
			direction = direction_arg
			ff_trick_mode_speed = 0

		if direction == "ff":
			if self.flag_time_stamp_at_trick_play_achieved:
				self.flag_time_stamp_at_trick_play_achieved = False
				current_time_stamp_from_log_secs = float(match.group(1))
				pause_position = float(match.group(2))
				current_position = float(match.group(3))
				print(f"current_position: {current_position}, pause_position: {pause_position}")
				print(f"time_stamp_at_trick_play_secs: {self.time_stamp_at_trick_play_secs}, current_time_stamp_from_log_secs: {current_time_stamp_from_log_secs}")
				position_difference = current_position - pause_position
				print(f"position_difference: {position_difference}")
				expected_position_difference = (current_time_stamp_from_log_secs - self.time_stamp_at_trick_play_secs) * ff_trick_mode_speed
				print(f"expected_position_difference : {expected_position_difference }")
				assert position_difference <= expected_position_difference, \
				 	f"Difference in timestamp exceeds tolerance "
		elif direction == "rew":
			start_position = float(match.group(1))
			current_position = float(match.group(2))
			print(f"start_position: {start_position}, current_position: {current_position}")
			assert abs(start_position - current_position) < self.POSITION_TOLERANCE_RW_SEEK
		elif direction == "seek":
			if self.flag_check_seek_position:
				self.flag_check_seek_position = False
				start_position = float(match.group(1))
				seek_position = float(match.group(2))
				print(f"start_position: {start_position}, seek_position: {seek_position}")
				assert abs(seek_position - start_position - self.seek_position) < self.POSITION_TOLERANCE_RW_SEEK, \
					f"Playback position must be same as seek position"

	# Extracts the first progress even position, as a reference from which seek will be performed.
	def extract_first_progress_event_position(self, match, arg):
		self.first_progress_event_position = float(match.groups()[-1])
		print(f"first_progress_event_position: {self.first_progress_event_position}")

