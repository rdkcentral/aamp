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
#include "stream_utils.hpp"

namespace Math
{
	double min( double v0, double v1 )
	{
		return (v0<v1)?v0:v1;
	}
	double max( double v0, double v1 )
	{
		return (v0>v1)?v0:v1;
	}
	int floor( double v )
	{
		return (int)v;
	}
};

double ComputePeriodDurationSeconds( const AdaptationSet &adaptationSet )
{
	auto total = 0.0;
	if( adaptationSet.representation.size()>0 )
	{
		auto representation = adaptationSet.representation[0];
		if( representation.data.duration.size()>1 )
			//if( Array.isArray(representation.duration) )
		{
			for( auto d : representation.data.duration )
			{
				total += d;
			}
			total/=representation.data.timescale;
		}
	}
	return total;
}

double GetPeriodFirstPts( const Timeline &timeline, std::string contentType, PeriodObj &period )
{
	double pts = 0.0;
	auto adaptationSet = period.adaptationSet[contentType];
	if( adaptationSet.representation.size()>0 )
	{
		auto representation = adaptationSet.representation[0];
		if( representation.data.presentationTimeOffset )
		{
			pts = representation.data.presentationTimeOffset/(double)representation.data.timescale;
		}
		else if( timeline.type == "dynamic" )
		{
			auto baseTime = timeline.tuneUTC - timeline.availabilityStartTime - timeline.timeShiftBufferDepth;
			if( representation.data.duration.size()==0 )
			{
				assert(0);
			}
			else if( representation.data.duration.size()>1 )
			{
				auto dt = (baseTime - period.start);
				pts += dt;
			}
			else
			{
				auto d = representation.data.duration[0];
				int segment = Math::floor( baseTime/d );
				pts = segment*d/(double)representation.data.timescale;
			}
		}
	}
	return pts;
}

void ComputeTimestampOffsets( Timeline &timeline )
{
	double totalDurationSeconds = 0;
	for( int iPeriod=0; iPeriod<timeline.period.size(); iPeriod++ )
	{
		PeriodObj &period = timeline.period[iPeriod];
		if( iPeriod==0 )
		{
			if( period.start<0 )
			{
				period.start = 0;
			}
		}
		else
		{
			PeriodObj &prevPeriod = timeline.period[iPeriod-1];
			if( prevPeriod.start>=0 )
			{
				if( prevPeriod.duration<0 && period.start>=0  )
				{ // difference in start times
					prevPeriod.duration = period.start - prevPeriod.start;
				}
				if( period.start<0 && prevPeriod.duration>=0 )
				{ // start+duration
					period.start = prevPeriod.start + prevPeriod.duration;
				}
			}
		}
	}
	
	{
		PeriodObj &lastPeriod = timeline.period[timeline.period.size()-1];
		if( lastPeriod.duration<0 && lastPeriod.start >=0 )
		{ // use mediaPresentationDuration to infer final duration
			lastPeriod.duration = timeline.mediaPresentationDuration - lastPeriod.start;
		}
	}
	
	for( int iPeriod=0; iPeriod<timeline.period.size(); iPeriod++ )
	{
		PeriodObj &period = timeline.period[iPeriod];
		auto periodDuration = period.duration;
		auto videoDuration = ComputePeriodDurationSeconds( period.adaptationSet["video"]);
		auto audioDuration = ComputePeriodDurationSeconds( period.adaptationSet["audio"]);
		if( videoDuration || audioDuration )
		{
			if( videoDuration && audioDuration )
			{
				periodDuration = Math::min( videoDuration, audioDuration );
				// NOTE: if we use Math::max, one track can end early, leaving small gap
				// Chrome browser especially doesn't like this - causes split buffers and stalls
			}
			else
			{ // audio-only or video-only playback
				periodDuration = Math::max( videoDuration, audioDuration );
			}
			period.duration = periodDuration;
		}
		auto firstVideoPts = GetPeriodFirstPts(timeline,"video",period);
		auto firstAudioPts = GetPeriodFirstPts(timeline,"audio",period);
		period.firstPts = Math::max(firstVideoPts,firstAudioPts);
		//period.timestampOffset = totalDurationSeconds-period.firstPts;
		totalDurationSeconds += periodDuration;
	}
	//console.log( timeline );
}

#if 0
function Stream( timeline )
{
	// generate human-readable stream summary based on manifest characteristics
	this.type = "";
	if( timeline.minimumUpdatePeriod ) this.type += "updating ";
	if( timeline.type=="dynamic" )
	{
		this.type += "live ";
		timeline.tuneUTC = Date.now()/1000.0;
	}
	if( manifest_characteristics&FLAG_ENCRYPTED ) this.type += "encrypted ";
	if( timeline.length>1 )
	{
		this.type += "multiperiod("+timeline.length+") ";
	}
	if( manifest_characteristics&FLAG_SEGMENT_BASE ) this.type += "SegmentBase ";
	if( manifest_characteristics&FLAG_SEGMENT_LIST ) this.type += "SegmentList ";
	if( manifest_characteristics&FLAG_SEGMENT_TEMPLATE ) this.type += "SegmentTemplate ";
	if( manifest_characteristics&FLAG_SEGMENT_TIMELINE ) this.type += "with SegmentTimeline ";
	if( manifest_characteristics&FLAG_PTO ) this.type += "with PTO ";
	
	this.tracks = [];
	
	this.timeline = timeline;
	ComputeTimestampOffsets( timeline );
}

Stream.prototype.addTrack = function( track )
{
	this.tracks.push(track);
};

Stream.prototype.refresh = function()
{
	console.log( "refreshing manifest" );
	// FIXME - use Time for synchronization
	let stream = this;
	LoadManifest(this.timeline.url,function(timeline){
		mediaElement.pause();
		let position = mediaElement.currentTime;
		stream.timeline = timeline;
		ComputeTimestampOffsets( timeline );
		mediaElement.currentTime = position;
		mediaElement.play();
	})
};
#endif

