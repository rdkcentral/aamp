// manifest abstraction
const fragmentDurationMs = 2000; // 2s segments

var manifest =
[ // bandwidth = bits per second
	{ "resolution":360,  "bytes":200000,  "bandwidth": 800000 },
	{ "resolution":480,  "bytes":350000,  "bandwidth":1400000 },
	{ "resolution":720,  "bytes":700000,  "bandwidth":2800000 },
	{ "resolution":1080, "bytes":1250000, "bandwidth":5000000 }
];

/**
for( var i=0; i<manifest.length; i++ )
{
	let numBytes = manifest[i].bandwidth*fragmentDurationMs/8000;
	console.log( numBytes );
	manifest[i].bytes = numBytes;
}
*/

function getNumProfiles()
{
	return manifest.length;
}

function getBandwidthOfProfile( idx )
{ // called by abrmanager
	return manifest[idx].bandwidth;
}

function getRampedDownProfileIndex( idx )
{ // called by abrmanager
	if( idx>0 )
	{
		idx--
	}
	return idx;
}

function getRampedUpProfileIndex( idx )
{
	if( idx<manifest.count-1 )
	{
		idx++;
	}
	return idx;
}

