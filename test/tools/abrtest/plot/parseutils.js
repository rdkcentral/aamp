const months =
[
	"Jan","Feb","Mar",
	"Apr","May","Jun",
	"Jul","Aug","Sep",
	"Oct","Nov","Dec"
];

function ParseReceiverLogTimestamp( line )
{
	let rc = NaN;
	try
	{
		part = line.split(" ");
		if( part[0].endsWith(":") )
		{ // new simulator format: <sec>:<ms>:
			if( part[0].indexOf(".")>=0 )
			{
				part = part[0].split(".");
				var s = parseInt(part[0]);
				var ms = parseInt(part[1]);
				rc = s*1000+ms;
			}
			else
			{ // fog workaround for strings like:
				// 1722018482: 51 : {77361ba9} HttpRequestEnd:
				part = line.split(":");
				var s = parseInt(part[0].trim());
				var ms = parseInt(part[1].trim());
				rc = s*1000+ms;
			}
		}
		else if( part[1]==":" )
		{ // simulator format - <sec>:<ms>
			part = part[0].split(":");
			if( part.length>=2 )
			{
				var s = parseInt(part[0]);
				var ms = parseInt(part[1]);
				rc = s*1000+ms;
			}
		}
		else if( part[2]==":" )
		{
			var s = parseInt(part[0]);
			var ms = parseInt(part[1]);
			rc = s*1000+ms;
		}
		else
		{
			// format used on settop (GMT time)
			var idx = 0;
			var year = parseInt(part[idx++]);
			if( isNaN(year) )
			{ // year missing (fog logs)
				year = new Date().getFullYear();
				idx--;
			}
			var month = months.indexOf(part[idx++]);
			var day = parseInt(part[idx++]);
			var time = part[idx++].split(":");
			var hour = parseInt(time[0]);
			var minute = parseInt(time[1]);
			var seconds = parseFloat(time[2]);
			var s = Math.floor(seconds);
			var ms = Math.floor((seconds - s)*1000);
			rc = Date.UTC(year, month, day, hour, minute, s, ms );
		}
	}
	catch( e )
	{
	}
	return rc;
}

const httpCurlErrMap =
{
    0:"OK",
    7:"Couldn't Connect",
    18:"Partial File",
    23:"Interrupted Download",
    28:"Operation Timed Out",
    42:"Aborted by callback",
    56:"Failure with receiving network data",
    200:"OK",
    302:"Temporary Redirect",
    304:"Not Modified",
    204:"No Content",
    400:"Bad Request",
    401:"Unauthorized",
    403:"Forbidden",
    404:"Not Found",
    500:"Internal Server Error",
    502:"Bad Gateway",
    503:"Service Unavailable"
};

function mapError( code )
{
	code = parseInt(code);
    var desc = httpCurlErrMap[code];
    if( desc!=null )
    {
        desc = "("+desc+")";
    }
    else
    {
        desc = "";
    }
    return ((code<100)?"CURL":"HTTP") + code + desc;
}

const eMEDIATYPE_VIDEO = 0;
const eMEDIATYPE_AUDIO = 1;
const eMEDIATYPE_SUBTITLE = 2;
const eMEDIATYPE_AUX_AUDIO = 3;
const eMEDIATYPE_MANIFEST = 4;
const eMEDIATYPE_LICENSE = 5;
const eMEDIATYPE_IFRAME = 6;
const eMEDIATYPE_INIT_VIDEO = 7;
const eMEDIATYPE_INIT_AUDIO = 8;
const eMEDIATYPE_INIT_SUBTITLE = 9;
const eMEDIATYPE_INIT_AUX_AUDIO = 10;
const eMEDIATYPE_PLAYLIST_VIDEO = 11;
const eMEDIATYPE_PLAYLIST_AUDIO = 12;
const eMEDIATYPE_PLAYLIST_SUBTITLE = 13;
const eMEDIATYPE_PLAYLIST_AUX_AUDIO = 14;
const eMEDIATYPE_PLAYLIST_IFRAME = 15;
const eMEDIATYPE_INIT_IFRAME = 16;
const eMEDIATYPE_DSM_CC = 17;
const eMEDIATYPE_IMAGE = 18;
const eMEDIATYPE_MANIFEST_19 = 19;

const mediaTypes = [
"VIDEO",
"AUDIO",
"SUBTITLE",
"AUX_AUDIO",
"MANIFEST",
"LICENSE",
"IFRAME",
"INIT_VIDEO",
"INIT_AUDIO",
"INIT_SUBTITLE",
"INIT_AUX_AUDIO",
"PLAYLIST_VIDEO",
"PLAYLIST_AUDIO",
"PLAYLIST_SUBTITLE",
"PLAYLIST_AUX_AUDIO",
"PLAYLIST_IFRAME",
"PLAYLIST_INIT_IFRAME",
"DSM_CC",
"IMAGE",
"MANIFEST"];

function ParseHttpRequestEnd( line )
{
	var rc = null;
	var prefix, offs;
	
	prefix = "HttpRequestEnd: Type: ";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{ // handle older HttpRequestEnd logging as used by FOG
	  // refer DELIA-57887 "[FOG] inconsistent HttpRequestEnd logging"
		// preliminary review shared which introduces "br" (bitrate) field in log - it's present for abrs_fragment#t:VIDEO
		// but not HttpRequestEnd
		var httpRequestEnd = {};
		line = "Type: "  + line.substr(offs+prefix.length);
		line = line.split(", ");
		for( var i=0; i<line.length; i++ )
		{
			var pat = line[i].split( ": " );
			httpRequestEnd[pat[0]] = pat[1];
		}
		if( httpRequestEnd.Type == "DASH-MANIFEST" )
		{
			httpRequestEnd.type = eMEDIATYPE_MANIFEST;
		}
		else if( httpRequestEnd.Type=="IFRAME" )
		{
			httpRequestEnd.type = eMEDIATYPE_IFRAME;
		}
		else if( httpRequestEnd.Type=="VIDEO" )
		{
			httpRequestEnd.type = eMEDIATYPE_VIDEO;
		}
		else if( httpRequestEnd.Type=="AUDIO" )
		{
			httpRequestEnd.type = eMEDIATYPE_AUDIO;
		}
		else if( httpRequestEnd.Type=="SUBTITLE" )
		{
		httpRequestEnd.type = eMEDIATYPE_SUBTITLE;
		}
		else
		{
			console.log( "unk type! '" + httpRequestEnd.Type + "'" );
		}
		httpRequestEnd.ulSz = httpRequestEnd.RequestedSize;
		httpRequestEnd.dlSz = httpRequestEnd.DownloadSize;
		httpRequestEnd.curlTime = httpRequestEnd.TotalTime;
		httpRequestEnd.responseCode = parseInt(httpRequestEnd.cerr)||parseInt(httpRequestEnd.hcode);
		httpRequestEnd.url = httpRequestEnd.Url;
		httpRequestEnd.Url = undefined;
		
		return httpRequestEnd;
	}
	
	prefix = "HttpRequestEnd: ";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{ // handle HttpRequestEnd as logged by aamp
		var json = line.substr(offs+prefix.length);
		try
		{
			rc = JSON.parse(json);
		}
		catch( err )
		{
			var isLicense = false;

			var URLPrefix = "";
			if(json.indexOf(",http") !== -1) {
				URLPrefix = "http";
			} else if(json.indexOf(",file") !== -1) {
				// Simulator logs have path as file://
				URLPrefix = "file";
			}
			// Parse the logs having comma in url
			var parseUrl = json.split("," + URLPrefix);

			var param = parseUrl[0].split(",");

			//insert url to param list
			param.push(URLPrefix + parseUrl[1]);

			var fields = "mediaType,type,responseCode,curlTime,total,connect,startTransfer,resolve,appConnect,preTransfer,redirect,dlSz,ulSz";
			if( param[0] != parseInt(param[0]) )
			{

				if( param.length == 16) {
					// use downloadbps value as br
					fields = "appName," + fields + ",br,url";
				} else if( param.length == 17) {
					fields = "appName," + fields + ",downloadbps,br,url";
				} else if( param.length == 15) {
					// If there is no bitrate field in license request, add bitrate as 0
					fields = "appName," + fields + ",downloadbps,br,url";
				    var licenseUrl = param[param.length - 1];
					param[param.length - 1] = 0; // hack to add dummy bitrate value 0
					param.push(0); // hack to add dummy bps value 0
					param.push(licenseUrl); // Push license url to the last
					isLicense = true;
				}
			} else {
				// Old log format where no appName in HttpRequestEnd log
				if( param.length == 15) {
					fields += ",br,url";
				} else if( param.length == 16) {
					fields += ",downloadbps,br,url";
				}
			}
			fields = fields.split(",");
			var httpRequestEnd = {};
			for( var i=0; i<fields.length; i++ )
			{
				httpRequestEnd[fields[i]] = param[i];
			};
			
			// Currently mediatype is send as audio for license in HTTPRequestEnd (RDKAAMP-745)
			// Remove the following hack once RDKAAMP-745 is fixed.
			if(isLicense) {
				httpRequestEnd.type = eMEDIATYPE_LICENSE;
			} else {
				httpRequestEnd.type = parseInt(httpRequestEnd.type);
			}

			httpRequestEnd.responseCode = parseInt(httpRequestEnd.responseCode);
			httpRequestEnd.curlTime = parseFloat(httpRequestEnd.curlTime);
			return httpRequestEnd;
		}
	}
	
	prefix = "HttpLicenseRequestEnd:";
	offs = line.indexOf(prefix);
	if( offs>=0 )
	{
		var json = line.substr(offs+prefix.length);
		try
		{
			rc = JSON.parse(json);
			rc.url = rc.license_url;
			rc.type = eMEDIATYPE_LICENSE;
		}
		catch( err )
		{
			console.log( "ignoring corrupt JSON from receiver log<JSON>" + json + "</JSON>" );
		}
	}
	return rc;
}
