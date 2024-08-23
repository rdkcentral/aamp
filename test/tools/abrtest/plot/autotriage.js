const ROWHEIGHT = 24;
const BITRATE_MARGIN = 2;
const TOP_MARGIN = 64;
const COLOR_TUNED = "#058840";
const COLOR_TUNE_FAILED = "#FF0000";
const COLOR_GOLD = "#FFD700";
const COLOR_LIGHT_GOLD = "#ffffcc";
const MARKER_FONT = "12px Arial";
const SMALL_FONT = "8px Arial";

var gAllLogLines;
var gBoxDownload;
var gTimeStampMin;

function findBox( e )
{
	const rect = canvas.getBoundingClientRect()
	var x = e.clientX - rect.left;
	var y = e.clientY - rect.top;
	
	let boxh = (ROWHEIGHT-4)/2;
	for (var i = gBoxDownload.length-1; i>=0; i-- )
	{ // reverse order, to match back-to-front draw order in case of overlapping bars
		const box = gBoxDownload[i];
		let boxy = box.y;
		if( box.fog )
		{
			boxy += boxh+1;
		}
		if( x >= box.x && x < box.x + box.w &&
			y >= boxy && y < boxy + boxh )
		{
			return box;
		}
	}
	return null;
}

function clickhandler(e)
{
	let box = findBox( e );
	if( box )
	{ // hack: adding newline makes text selectable in chrome
		//alert( box.line+"\n" );
		let e = document.getElementById("dialogtext");
		let json = ParseHttpRequestEnd(box.line);
		json.type = mediaTypes[json.type]
		e.innerHTML = JSON.stringify(json,null,2);
		e = document.getElementById("dialog");
		e.open = true;
	}
}

function MapMediaColor(mediaType)
{ // first color for box interior; second color for outline
	switch( mediaType )
	{
		case eMEDIATYPE_MANIFEST:
			return ['#00cccc','#006666']; // cyan;
		case eMEDIATYPE_PLAYLIST_VIDEO:
			return ['#00cc00','#006600']; // dark green
		case eMEDIATYPE_INIT_VIDEO:
			return ['#7fff7f','#3f7f3f']; // medium green
		case eMEDIATYPE_VIDEO:
			return ['#ccffcc','#667f66']; // light green
		case eMEDIATYPE_PLAYLIST_IFRAME:
			return ['#00cc00','#006600']; // dark green
		case eMEDIATYPE_INIT_IFRAME:
			return ['#7fff7f','#3f7f3f']; // medium green
		case eMEDIATYPE_IFRAME:
			return ['#ccffcc','#667f66']; // light green
		case eMEDIATYPE_PLAYLIST_AUDIO:
			return ['#0000cc','#000066']; // dark blue
		case eMEDIATYPE_INIT_AUDIO:
			return ['#7f7fff','#3f3f7f']; // medium blue
		case eMEDIATYPE_AUDIO:
			return ['#ccccff','#66667f']; // light blue
		case eMEDIATYPE_PLAYLIST_SUBTITLE:
			return ['#cccc00','#666600']; // dark yellow
		case eMEDIATYPE_INIT_SUBTITLE:
			return ['#ffff7f','#7f7f3f']; // medium yellow
		case eMEDIATYPE_SUBTITLE:
			return ['#ffffcc','#7f7f66']; // light yellow
		case eMEDIATYPE_LICENSE:
			return ['#ff7fff','#7f3f7f']; // medium magenta
		case -eMEDIATYPE_LICENSE: // pre/post overhead
			return ['#ffccff','#7f667f'];
		case eMEDIATYPE_MANIFEST_19:
			return ['#00cccc','#006666']; // cyan;
		default: // error
			return ['#ff2020','#7f3f3f'];
	}
}

function constructLabel(obj)
{
	let label = obj.url;
	if( label.endsWith(".mpd") )
	{
		label = "manifest";
	}
	else
	{
		let period = 0;
		{
			let idx1 = label.indexOf("-periodid-");
			if( idx1>=0 )
			{
				idx1 += 10;
				let idx2 = label.indexOf("-repid-",idx1);
				if( idx2>=0 )
				{
					period = label.substr(idx1,idx2-idx1);
				}
			}
			else
			{
				idx1 = label.indexOf("/abr/");
				if( idx1>=0 )
				{
					idx1 += 5;
					let idx2 = label.indexOf("-",idx1);
					if( idx2>=0 )
					{
						period = label.substr(idx1,idx2-idx1);
					}
				}
			}
			if( period )
			{
				period = "#"+(parseInt(period)%10);
			}
		}

		let sap =label.indexOf("-sap-")>=0;
		if( label.endsWith(".mp4") || label.endsWith(".seg") )
		{ // strip extension
			label = label.substr(0,label.length-4);
		}
		
		if( label.endsWith( "-init") )
		{ // strip/normalize suffix
			label = "init";
		}
		else if( label.endsWith("-header") )
		{ // strip/normalize
			label = "init";
		}
		else
		{
			let idx = label.lastIndexOf("-");
			let idx2 = label.lastIndexOf("/");
			if( idx<0 || idx2 > idx )
			{
				idx = idx2;
			}
			if( idx>=0 )
			{
				idx++;
				label = label.substr(idx,label.length-idx);
			}
		}
		label = obj.track+":"+label;
		if( sap )
		{
			label = "[sap]"+label;
		}
		label = label+period;
	}
	obj.label = label;
}

function x2time( x )
{
	return x - (BITRATE_MARGIN)*10 + gTimeStampMin;
}

function time2x(t) {
	return BITRATE_MARGIN + (t - gTimeStampMin) * 0.1;
}

function drawTimelineBackdrop()
{
	ctx.textAlign = "left";
	ctx.font = MARKER_FONT;

	let shade = true;
	let t0 = gTimeStampMin;
	let x0 = time2x(t0);
	var axisLabel = 0;
	for(;;)
	{
		shade = !shade;
		let x1 = time2x(t0);
		if( x1<BITRATE_MARGIN )
		{
			t0+=1000;
			continue;
		}
		if( x1 >= canvas.width ) break;
		
		if( shade )
		{ // light grey band every other second
			ctx.fillStyle = '#f3f3f3';
			ctx.fillRect(x0, TOP_MARGIN - ROWHEIGHT/2, x1 - x0, canvas.height - TOP_MARGIN );
		}
		
		ctx.fillStyle = '#000000';
		ctx.fillText( axisLabel, x1, TOP_MARGIN-20 );
		x0 = x1;
		t0 += 1000;
		axisLabel++;
	}
}

function drawTrackAxes()
{
	var n = gBoxDownload.length;
	ctx.strokeStyle = '#dddddd';
	for (var i = 0; i < n; i++) {
		let y = i*ROWHEIGHT+64;
		ctx.strokeRect(BITRATE_MARGIN + 2, y+ROWHEIGHT/2-2, canvas.width, 1);
	}
}

function drawTrackLabels()
{
	ctx.font = MARKER_FONT;
	ctx.textAlign = "right";
	var first = true;

	const yAxisList = document.getElementById('yAxisList');
	const listItem = document.createElement('li');
	listItem.style.height = `${TOP_MARGIN + 2}px`;
	yAxisList.appendChild(listItem);

	for (var i = 0; i < gBoxDownload.length; i++) {
		let obj = gBoxDownload[i];
		let label = obj.label;

		/*let y = obj.y+ROWHEIGHT/2;
		ctx.fillStyle = "#ffffff";
		ctx.fillRect(0,y-ROWHEIGHT/2,BITRATE_MARGIN,ROWHEIGHT/2+6 );
		ctx.fillStyle = '#000000';
		ctx.fillText(label, BITRATE_MARGIN-6, y+2 );*/

		// Check if label already exists in yAxisList
		const existingLabels = Array.from(yAxisList.children).map(item => item.textContent);
		if (!existingLabels.includes(label)) {
			// Add axis label to the side bar list
			const listItem = document.createElement('li');
			listItem.textContent = obj.label;
			yAxisList.appendChild(listItem);
		}
	}
}

function drawDownloadActivity()
{
	ctx.font = SMALL_FONT;
	let h = (ROWHEIGHT-4)/2;
	
	for (var i = 0; i < gBoxDownload.length; i++)
	{
		const obj = gBoxDownload[i];
		let y = obj.y;
		if( obj.fog )
		{
			y += h+1;
		}
		ctx.fillStyle = obj.fillStyle[0];
		ctx.fillRect( obj.x, y, obj.w, h );
		ctx.strokeStyle = obj.fillStyle[1];
		ctx.strokeRect( obj.x, y, obj.w, h );
		if( obj.error!="" )
		{
			if( ctx.measureText(obj.error).width<obj.w )
			{
				ctx.fillStyle = "white";
				ctx.fillText(obj.error, obj.x+4, y+ROWHEIGHT/2-2-2 );
			}
			else
			{
				ctx.fillStyle = "black";
				ctx.fillText(obj.error, obj.x+obj.w+4, y+ROWHEIGHT/2-2-2 );
			}
		}
	}
}

function paint()
{
	ctx.clearRect(0, 0, canvas.width, canvas.height);
	drawTimelineBackdrop();
	drawTrackAxes();
	drawDownloadActivity();
	drawTrackLabels();
}

function ProcessLog( text )
{
	gAllLogLines = text.split("\n");
	for( var i=0; i<gAllLogLines.length; i++ )
	{
		ProcessLine(gAllLogLines[i]);
	}
	updateLayoutDownload();
	paint();
}

function dropHandler(e)
{
	e.preventDefault();
	var file = e.dataTransfer.files[0], reader = new FileReader();
	reader.onload = function(event)
	{
		ProcessLog( event.target.result );
	};
	reader.readAsText(file);
	return false;
}

function dragOverHandler(ev) {
	ev.preventDefault();
}

window.onload = function() {
    const canvasContainer = document.getElementById('canvasContainer');
	const sidebarContent = document.getElementById('sidebarContent');
	// Sync scrolling veritically between canvas and y-axis nav bar list
    canvasContainer.addEventListener('scroll', (e) => {
		sidebarContent.style.transform = `translateY(-${canvasContainer.scrollTop}px)`;
    });
	
	gBoxDownload = [];
	canvas = document.getElementById("myCanvas");
	ctx = canvas.getContext("2d");
	
	window.addEventListener("keydown", function(e){
		if( e.ctrlKey || e.metaKey )
		{
			switch( e.which )
			{
				case 86: // Control+V Paste override - allow direct paste of raw log snippet as alternative to drag-drop
					e.preventDefault();
					navigator.clipboard.readText()
					.then(text => {
						ProcessLog( text );
					})
					.catch(err => {
						alert('failed to read clipboard contents: ' + err);
					});
					break;
					
				default:
					break;
			}
		}
	} );
}
	
function mapTrack( obj, httpRequestEnd )
{
	if( obj.type==eMEDIATYPE_LICENSE )
	{
		obj.track = "drm"; // AES-128
	}
	else if( obj.type==eMEDIATYPE_PLAYLIST_VIDEO )
	{
		obj.track = "vid-playlist";
	}
	else if( obj.type==eMEDIATYPE_VIDEO || obj.type == eMEDIATYPE_INIT_VIDEO )
	{
		// group by video bitrate
		if( httpRequestEnd.br )
		{
			obj.track = Math.floor((parseInt(httpRequestEnd.br)/1024))+"vid";
		}
		else
		{
			obj.track = 0;
		}
	}
	else if( obj.type == eMEDIATYPE_PLAYLIST_IFRAME || obj.type == eMEDIATYPE_IFRAME || obj.type == eMEDIATYPE_INIT_IFRAME )
	{
		obj.track = "iframe";
	}
	else if( obj.type == eMEDIATYPE_AUDIO || obj.type == eMEDIATYPE_INIT_AUDIO )
	{
		obj.track = "audio";
	}
	else if( obj.type == eMEDIATYPE_SUBTITLE || obj.type == eMEDIATYPE_INIT_SUBTITLE )
	{
		obj.track = "subtitle";
	}
	else if( obj.type == eMEDIATYPE_MANIFEST )
	{
		obj.track = "manifest";
	}
	else if( obj.type == eMEDIATYPE_MANIFEST_19 )
	{
		obj.track = "manifest";
	}
	else if( obj.type == eMEDIATYPE_PLAYLIST_AUDIO )
	{
		obj.track = "aud-playlist";
	}
	else
	{
		console.log( "UNK TYPE!" );
	}
}
	
function ProcessLine( line )
{
	let httpRequestEnd = ParseHttpRequestEnd(line);
	if( httpRequestEnd )
	{
		if( !httpRequestEnd.url ) return;
		//if( httpRequestEnd.type == eMEDIATYPE_MANIFEST ) return;
		
		if( httpRequestEnd.url.endsWith(" ") )
		{ // hack! trim whitespace
			httpRequestEnd.url = httpRequestEnd.url.substr(0,httpRequestEnd.url.length-1);
		}

		let obj = {};
		obj.line = line;
		obj.fog = (line.indexOf("[AAMP-PLAYER]")<0);
		//obj.instance = parsePlayerInstance(line);
		obj.error = mapError(httpRequestEnd.responseCode);
		obj.durationms = 1000*httpRequestEnd.curlTime;
		obj.type = httpRequestEnd.type;
		obj.ulSz = httpRequestEnd.ulSz;
		obj.bytes = httpRequestEnd.dlSz;
		obj.url = httpRequestEnd.url;
		var doneUtc = ParseReceiverLogTimestamp(line);
		obj.utcstart = doneUtc-obj.durationms;
		mapTrack( obj, httpRequestEnd );
		if( obj.error != "HTTP200(OK)" && obj.error != "HTTP206" ) {
			obj.fillStyle = MapMediaColor(-1);
		}
		else
		{
			obj.fillStyle = MapMediaColor(obj.type);
			obj.error = "";
		}
		constructLabel(obj);
		gBoxDownload.push(obj);
	} // httpRequestEnd
} // ProcessLine

function compareNumbers(a, b) {
	return a.utcstart - b.utcstart;
}

function updateLayoutDownload()
{
	gBoxDownload.sort( compareNumbers );
	if( !gTimeStampMin )
	{
		gTimeStampMin = gBoxDownload[0].utcstart;
	}
	let sy0 = 64-ROWHEIGHT/2;
	let sy = sy0+ROWHEIGHT;
	
	for (var i = 0; i < gBoxDownload.length; i++) {
		let box = gBoxDownload[i];
		var t0 = box.utcstart;
		var t1 = box.durationms + t0;
		box.x = time2x(t0)-1;
		box.w = time2x(t1) - box.x+2;
		var found = false;
		for( var j=0; j<gBoxDownload.length; j++ )
		{
			if( i!=j && gBoxDownload[j].y && box.label == gBoxDownload[j].label )
			{
				box.y = gBoxDownload[j].y;
				found = true;
				break;
			}
		}
		if( !found )
		{
			box.y = sy;
			sy += ROWHEIGHT;
		}
	}
}


