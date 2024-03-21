// port of HybridABRManager.h

function CheckAbrThresholdSize( bufferlen, downloadTimeMs )
{
	let currentProfilebps = getBandwidthOfProfile(mAbrState.profile);
	let downloadbps = Math.floor(bufferlen*8000/downloadTimeMs);
	// extra coding to avoid picking lower profile
	// Avoid this reset for Low bandwidth timeout cases
	if( downloadbps < currentProfilebps && fragmentDurationMs && downloadTimeMs < fragmentDurationMs/2 )
	   //&& (abortReason != eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT)
	{
		downloadbps = currentProfilebps;
	}
	return downloadbps;
}

function UpdateABRBitrateDataBasedOnCacheLength( downloadbps )
{
	let tNow = ABRGetCurrentTimeMS();
	mAbrState.bitrateData.push( [tNow,downloadbps] );
	if(mAbrState.bitrateData.length > mAbrState.ABR_CACHE_LENGTH )
	{
		mAbrState.bitrateData.splice(0,1);
	}
}

function UpdateABRBitrateDataBasedOnCacheLife()
{
	let tNow = ABRGetCurrentTimeMS();
	let i=0;
	while( i<mAbrState.bitrateData.length )
	{
		let age = tNow - mAbrState.bitrateData[i][0];
		if( age > mAbrState.ABR_CACHE_LIFE )
		{
			mAbrState.bitrateData.splice(i,1);
		}
		else
		{
			i++;
		}
	}
}

function UpdateABRBitrateDataBasedOnCacheOutlier()
{
	let avg = 0;
	let n = 0;
	let medianbps=0;
	let bitrates = []
	for( var i=0; i<mAbrState.bitrateData.length; i++ )
	{
		bitrates.push( mAbrState.bitrateData[i][1] );
	}
	bitrates.sort(function(a,b){ return a-b });
	let idx = Math.floor(bitrates.length/2);
	if( bitrates.length%2 )
	{
		medianbps = bitrates[idx];
	}
	else
	{
		let m1 = bitrates[idx];
		let m2 = bitrates[idx+1];
		medianbps = (m1+m2)/2;
	}
	let abrOutlierDiffBytes = mAbrState.ABR_OUTLIER;
	for( var tmpDataIter=0; tmpDataIter<bitrates.length; tmpDataIter++ )
	{
		let br = bitrates[tmpDataIter];
		let diffOutlier = (br>medianbps)?(br-medianbps):(medianbps-br);
		if( diffOutlier > abrOutlierDiffBytes )
		{ // ignore this sample
			Log( "removing outlier" );
		}
		else
		{
			avg += br;
			n++;
		}
	}
	return Math.floor(avg/n);
}

function CheckProfileChange()
{
	let checkProfileChange = true;
	let currBW = getBandwidthOfProfile(mAbrState.profile);
	//Avoid doing ABR during initial buffering which will affect tune times adversely
	if( mAbrState.bufferedMs > 0 && mAbrState.bufferedMs < mAbrState.ABR_SKIP_DURATION)
	{
		//For initial fragment downloads, check available bw is less than default bw
		//If available BW is less than current selected one, we need ABR
		if ( mAbrState.networkBandwidth > 0 && mAbrState.networkBandwidth < currBW)
		{
		}
		else
		{
			checkProfileChange = false;
		}
	}
	return checkProfileChange;
}


function GetDesiredProfileOnBuffer( newProfileIndex )
{
	let currentBandwidth = getBandwidthOfProfile(mAbrState.profile);
	let newBandwidth     = getBandwidthOfProfile(newProfileIndex);
	if( mAbrState.bufferedMs > 0 )
	{
		if( newBandwidth > currentBandwidth )
		{
			// Rampup attempt. check if buffer availability is good before profile change
			// else retain current profile
			if( mAbrState.bufferedMs < mAbrState.AAMP_HIGH_BUFFER_BEFORE_RAMPUP )
			{
				newProfileIndex = mAbrState.profile;
			}
		}
		else
		{
			// Rampdown attempt. check if buffer availability is good before profile change
			// Also if delta of current profile to new profile is 1 , then ignore the change
			// if bigger rampdown , then adjust to new profile
			// else retain current profile
			if( mAbrState.bufferedMs > mAbrState.mABRMinBuffer && getRampedDownProfileIndex(mAbrState.profile) == newProfileIndex)
			{
				newProfileIndex = mAbrState.profile;
			}
		}
	}
	return newProfileIndex;
}

function CheckRampupFromSteadyState( newProfileIndex )
{
	let newBandwidth = getBandwidthOfProfile(newProfileIndex);
	let abrThreshold = (newBandwidth - mAbrState.networkBandwidth) * 100 / mAbrState.networkBandwidth;
	let nProfileIdx = getRampedUpProfileIndex(mAbrState.profile);
	// switch to new profile only on bitrate difference is less than 30 percentage
	if( abrThreshold >= 0 && abrThreshold <= 30 )
	{
		newProfileIndex = nProfileIdx;
	}
	if( newProfileIndex  != mAbrState.profile )
	{ // periodicly inch up to escape ABR Valley
		mAbrState.loop = (++mAbrState.loop >4)?1:mAbrState.loop;
		mAbrState.mMaxBufferCountCheck =  Math.pow(mAbrState.ABR_CACHE_LENGTH,mAbrState.loop);
		Log( "eAAMP_BITRATE_CHANGE_BY_BUFFER_FULL" );
	}
	return newProfileIndex;
}

function CheckRampdownFromSteadyState( newProfileIndex )
{
	if( mAbrState.mABRLowBufferCounter > mAbrState.ABR_CACHE_LENGTH )
	{
		newProfileIndex = getRampedDownProfileIndex(mAbrState.profile);
		if( newProfileIndex!=mAbrState.profile )
		{
			Log( "eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY" );
		}
	}
	return newProfileIndex;
}

function IsABRDataGoodToEstimate( time_diff )
{ // ?
	return time_diff >= mAbrState.DEFAULT_ABR_ELAPSED_MILLIS_FOR_ESTIMATE;
}

// ABRManager.cpp
function getProfileIndexByBitrateRampUpOrDown()
{
	let desiredProfileIndex = mAbrState.profile;
	if (mAbrState.networkBandwidth < 0 )
	{ // If the network bandwidth is not available, just reset the profile change up/down count.
		mAbrState.profileChangeUpCount = 0;
		mAbrState.profileChangeDownCount = 0;
		return desiredProfileIndex;
	}
	let numProfiles = getNumProfiles();
	let currentBandwidth = getBandwidthOfProfile(mAbrState.profile);
	if( mAbrState.networkBandwidth > currentBandwidth )
	{ // if networkBandwidth > is more than current bandwidth
		for( var i=0; i<numProfiles; i++ )
		{
			if( mAbrState.networkBandwidth >= getBandwidthOfProfile(i) )
			{
				desiredProfileIndex = i;
			}
			else
			{
				break;
			}
		}
		
		// No need to jump one profile for one network bw increase
		if( desiredProfileIndex == mAbrState.profile+1 )
		{
			mAbrState.profileChangeUpCount++;
			// if same profile holds good for next 3*2 fragments
			if( mAbrState.profileChangeUpCount < mAbrState.nwConsistencyCnt )
			{
				desiredProfileIndex = mAbrState.profile;
			}
			else
			{
				mAbrState.profileChangeUpCount = 0;
			}
		}
		else
		{
			mAbrState.profileChangeUpCount = 0;
		}
		mAbrState.profileChangeDownCount = 0;
	}
	else
	{
		// if networkBandwidth < than current bandwidth
		let i = mAbrState.profile;
		let foundSupported = false;
		while( i>0 )
		{
			i--;
			if( mAbrState.networkBandwidth >= getBandwidthOfProfile(i) )
			{
				desiredProfileIndex = i;
				foundSupported = true;
				break;
			}
		}
		
		// we didn't find a profile which can be supported in this bandwidth
		if( !foundSupported )
		{
			desiredProfileIndex = 0;
		}
		// No need to jump one profile for small  network change
		if( i==mAbrState.profile-1 )
		{
			mAbrState.profileChangeDownCount++;
			// if same profile holds good for next 3*2 fragments
			if( mAbrState.profileChangeDownCount < mAbrState.nwConsistencyCnt )
			{
				desiredProfileIndex = mAbrState.profile;
			}
			else
			{
				mAbrState.profileChangeDownCount = 0;
			}
		}
		else
		{
			mAbrState.profileChangeDownCount = 0;
		}
		mAbrState.profileChangeUpCount = 0;
	}
	return desiredProfileIndex;
}

// StreamAbstractionAAMP
function GetDesiredProfileOnSteadyState( newProfileIndex )
{
	if( mAbrState.bufferedMs > 0 && mAbrState.profile == newProfileIndex )
	{
		if( mAbrState.bufferedMs > mAbrState.mABRMaxBuffer)
		{
			mAbrState.mABRHighBufferCounter++;
			mAbrState.mABRLowBufferCounter = 0;
			if(mAbrState.mABRHighBufferCounter > mAbrState.mMaxBufferCountCheck)
			{
				let nProfileIdx = getRampedUpProfileIndex(mAbrState.profile);
				let newBandwidth = getBandwidthOfProfile(nProfileIdx);
				CheckRampupFromSteadyState(newProfileIndex);
				mAbrState.mABRHighBufferCounter = 0;
			}
		}
		// steady state ,with no ABR cache available to determine actual bandwidth
		// this state can happen due to timeouts
		// Adding delta check: When bandwidth is higher than currentprofile bandwidth but insufficient to download both audio and video simultaneously, a delta less than 2000 kbps indicates a need for steady state rampdown.
		if( mAbrState.bufferedMs < mAbrState.mABRMinBuffer )
		{
			if(//aamp->GetLLDashServiceData()->lowLatencyMode ||
			   mAbrState.networkBandwidth == -1)
			{
				mAbrState.mABRLowBufferCounter++;
				mAbrState.mABRHighBufferCounter = 0;
				
				//HybridABRManager::BitrateChangeReason mhBitrateReason;
				//mhBitrateReason = (HybridABRManager::BitrateChangeReason) mBitrateReason;
				CheckRampdownFromSteadyState(newProfileIndex);
				//mBitrateReason = (BitrateChangeReason) mhBitrateReason;
				mAbrState.mABRLowBufferCounter = (mAbrState.mABRLowBufferCounter > mABRCacheLength)? 0 : mAbrState.mABRLowBufferCounter;
			}
		}
	}
	else
	{
		mAbrState.mABRLowBufferCounter = 0 ;
		mAbrState.mABRHighBufferCounter = 0;
	}
	return newProfileIndex;
}

function GetDesiredProfileBasedOnCache()
{
	let networkBandwidth = mAbrState.networkBandwidth;
	let desiredProfileIndex = mAbrState.profile;
	let currentBandwidth = getBandwidthOfProfile(mAbrState.profile);
	let mABRNwConsistency = mAbrState.ABR_NW_CONSISTENCY_CNT;
	let nwConsistencyCnt = (mAbrState.mNwConsistencyBypass)?1:mABRNwConsistency;
	
	// Ramp up/down (do ABR)
	desiredProfileIndex = getProfileIndexByBitrateRampUpOrDown();
	if (mAbrState.profile != desiredProfileIndex)
	{
		// There is a chance that desiredProfileIndex is reset in below GetDesiredProfileOnBuffer call
		// Since bitrate notification will not be triggered in this case, its fine
		Log( "eAAMP_BITRATE_CHANGE_BY_ABR" );
	}
	if(!mAbrState.mNwConsistencyBypass )//&& ISCONFIGSET(eAAMPConfig_ABRBufferCheckEnabled))
	{
		// Checking if frequent profile change happening
		if(mAbrState.profile != desiredProfileIndex)
		{
			GetDesiredProfileOnBuffer(desiredProfileIndex);
		}
		
		// Now check for Fixed BitRate for longer time(valley)
		GetDesiredProfileOnSteadyState( desiredProfileIndex );
		
		// After ABR is done , next configure the timeouts for next downloads based on buffer
		// ConfigureTimeoutOnBuffer();
	}
	// only for first call, consistency check is ignored
	mAbrState.mNwConsistencyBypass = false;
	return desiredProfileIndex;
}
