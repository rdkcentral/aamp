#include "MediaPipeline.h"

namespace nmqe::media {

const char *toString(MediaSourceType type)
{
	switch (type)
	{
		case MediaSourceType::AUDIO: return "audio";
		case MediaSourceType::VIDEO: return "video";
		default: return "unknown";
	}
}

const char *toString(PlaybackState state)
{
	switch (state)
	{
		case PlaybackState::IDLE: return "idle";
		case PlaybackState::PLAYING: return "playing";
		case PlaybackState::PAUSED: return "paused";
		case PlaybackState::SEEKING: return "seeking";
		case PlaybackState::SEEK_DONE: return "seek_done";
		case PlaybackState::STOPPED: return "stopped";
		case PlaybackState::END_OF_STREAM: return "end_of_stream";
		case PlaybackState::FAILURE: return "failure";
		default: return "unknown";
	}
}

} // namespace nmqe::media
