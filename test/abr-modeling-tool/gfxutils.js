
//	downloadActivityCanvas
//	networkBandwidthCanvas
//	bufferHealthCanvas

// visualization utility functions
const MARGIN = 100;
var xx,yy;

function TimeMsToX( ms )
{ // map elapsed time (ms) to x axis
	return ms/25 + MARGIN;
}

function ProfileToY( profileIndex )
{ // map profile index to y axis; lowest bottom, highest at top
	return (getNumProfiles()-1-profileIndex)*48+24;
}

function BufferingToY( ms )
{ // map buffering (ms) to y axis
	return bufferHealthCanvas.height - ms/100;
}

function clearBackdropHelper( canvas )
{
	let ctx = canvas.getContext("2d");
	ctx.lineWidth = 2;
	ctx.clearRect(0,0,canvas.width,canvas.height);
	ctx.textAlign="right";
	ctx.font = "24px serif";
	// shade background bands for other second of timeline
	let x0 = MARGIN;
	var alt = true;
	var t = 0;
	for(;;)
	{
		let x1 = TimeMsToX(t);
		alt = !alt;
		if( alt )
		{
			ctx.fillStyle = "#eeeeee";
			ctx.fillRect(x0,0,x1-x0,canvas.height);
		}
		x0=x1;
		if( x0 >= canvas.width )
		{
			break;
		}
		t += 1000; // 1 second
	}
	return ctx;
}

function drawBackdrop()
{
	{
		let ctx = clearBackdropHelper( downloadActivityCanvas );
		ctx.setLineDash([2,2]);
		
		// horizontal line for each profile
		ctx.strokeStyle = "orange";
		for( var i=0; i<manifest.length; i++ )
		{
			let y = ProfileToY(i);
			ctx.beginPath();
			ctx.moveTo(MARGIN,y);
			ctx.lineTo(downloadActivityCanvas.width,y);
			ctx.stroke();
			
			ctx.fillStyle = "black";
			ctx.fillText(manifest[i].bandwidth, MARGIN-4, y+7 );
		}
	}
	
	{
		let ctx = clearBackdropHelper( bufferHealthCanvas );

		// target buffering
		ctx.strokeStyle = "green";
		let y = BufferingToY(mAbrState.targetBuffering);
		ctx.beginPath();
		ctx.moveTo(MARGIN,y);
		ctx.lineTo(bufferHealthCanvas.width,y);
		ctx.stroke();
		ctx.setLineDash([]);
		ctx.fillStyle = "black";
		ctx.fillText("buf:"+(mAbrState.targetBuffering/1000)+"s", MARGIN-4, y+7 );
	}
	
	{
		let ctx = clearBackdropHelper(networkBandwidthCanvas);
		xx = undefined;
		yy = undefined;
		ctx.fillStyle = "black";
		ctx.fillText("netBw", MARGIN-4, networkBandwidthCanvas.height/2+7 );
	}
}

/**
 * @brief update visualization with download activity
 * as side effect, updates ABR state: elapsedTimeMs, bufferedMs
 * @param ms duration in milliseconds
 * @param downloading if true, this is a download starting now, of duration ms.  Otherwise, this is a delay (no download activity) starting now of duration ms
 */
function networkDelay( ms, downloading, color )
{
	if( !color )
	{
		color = "yellow";
	}
	let x0 = TimeMsToX(mAbrState.elapsedTimeMs);
	let y0 = BufferingToY(mAbrState.bufferedMs);

	{
		let ctx = bufferHealthCanvas.getContext("2d");
		
		ctx.strokeStyle = "green";
		ctx.beginPath();
		ctx.moveTo(x0,y0);
		
		if( ms > mAbrState.bufferedMs )
		{ // underflow/buffering detected
			// red line to indicate places where buffer ran dry
			let x1 = TimeMsToX(mAbrState.elapsedTimeMs+mAbrState.bufferedMs);
			let y1 = BufferingToY(0);
			ctx.lineTo( x1,y1 );
			ctx.stroke();
			
			// draw red vertical dashed line on timeline to indicate underflow
			ctx.setLineDash([5,5]);
			mAbrState.bufferedMs = 0;
			ctx.strokeStyle = "red";
			ctx.beginPath();
			ctx.moveTo(x1,0);
			ctx.lineTo(x1,bufferHealthCanvas.height);
			ctx.stroke();
			ctx.setLineDash([]);
		}
		else
		{
			let x1 = TimeMsToX(mAbrState.elapsedTimeMs+ms);
			mAbrState.bufferedMs -= ms;
			let y1 = BufferingToY(mAbrState.bufferedMs);
			ctx.lineTo( x1,y1 );
			ctx.stroke();
		}
	}

	{
		let ctx = downloadActivityCanvas.getContext("2d");

		let x = TimeMsToX(mAbrState.elapsedTimeMs);
		mAbrState.elapsedTimeMs += ms;
		let w = TimeMsToX(mAbrState.elapsedTimeMs)-x;
		let h = 12*2;
		let y = ProfileToY(mAbrState.profile)-h/2;
		if( downloading )
		{ // show download activity
			ctx.fillStyle = color;
			ctx.fillRect(x,y,w,h );
			ctx.strokeStyle = '#7f3f00';
			ctx.strokeRect(x,y,w,h );
			mAbrState.bufferedMs += fragmentDurationMs;
		}
	}
	
	{ // show estimated network bandwidth
		let ctx = networkBandwidthCanvas.getContext("2d");
		ctx.fillStyle = "blue";
		ctx.beginPath();
		if( xx!=undefined && yy!=undefined )
		{
			ctx.moveTo(xx,yy);
		}
		xx = TimeMsToX(mAbrState.elapsedTimeMs);
		yy = networkBandwidthCanvas.height - mAbrState.networkBandwidth*16/500000;
		if( xx==undefined && yy==undefined )
		{
			ctx.moveTo(xx,yy);
		}
		ctx.lineTo(xx,yy);
		ctx.stroke();
	}
}
