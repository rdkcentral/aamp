var captureIp = 'not_set';


// AAMP Player class
class VideoCapture {

    // Create an AAMP media player instance and register some listeners for monitoring purposes
    constructor() {

        // Get the Capture Device IP from the optional args when set
        var url = new URL(window.location.href);
        if (url.searchParams.has("vcap")){
            captureIp = url.searchParams.get("vcap");
            console.log("Using VACP = (" + captureIp + ")");
        }
    }


    // Indicates if the video capture device is active
    isActive()
    {
        return (captureIp != "not_set")
    }


    // Asynchronously calls a URL on the Video Capture Device
    async callUrl (url)
    {
        // Construct the complete URL
        const completeUrl = 'http://' + captureIp + url;
        console.log("completeUrl: " + completeUrl);

        const response = await fetch(completeUrl);
        const myJson = await response.json(); //extract JSON from the http response

        return myJson
    }


    // Initializes the Video Capture Device
    async Init()
    {
        console.log("VideoCapture Init ENTER");

        if (this.isActive()) {
            var statusJson = await this.callUrl('/init');

            status = statusJson['status'];
            if (status != 'ok'){
                TST_ASSERT_FAIL_FATAL("Error calling Capture: " + status + " : " + statusJson['errorCode'])
            }
        }
        else {
            console.log("VideoCapture Init: Not Active");
        }

        console.log("VideoCapture Init EXIT: (status = " + status + ")");
    }


    // Terminates the Video Capture Device
    async Term()
    {
        console.log("VideoCapture Term ENTER");

        if (this.isActive()) {
            var statusJson = await this.callUrl('/term');

            status = statusJson['status'];
            if (status != 'ok'){
                TST_ASSERT_FAIL_FATAL("Error calling Capture: " + status + " : " + statusJson['errorCode'])
            }
        }
        else {
            console.log("VideoCapture Term: Not Active");
        }

        console.log("VideoCapture Term EXIT: (status = " + status + ")");
    }


    // Start a video Capture Session
    async Capture(filename, duration)
    {
        console.log("VideoCapture Capture ENTER: filename=" + filename + ", duration=" + duration);

        if (this.isActive()) {
            var statusJson = await this.callUrl('/capture?filename=' + filename + '&duration=' + duration);

            status = statusJson['status'];
            if (status != 'ok'){
                TST_ASSERT_FAIL_FATAL("Error calling Capture: " + status + " : " + statusJson['errorCode'])
            }
        }
        else {
            console.log("VideoCapture Capture: Not Active");
        }

        console.log("VideoCapture Capture EXIT: (status = " + status + ")");
    }

    async TakeScreenGrab(filename)
    {

        console.log("VideoCapture Grab ENTER: filename=" + filename);

        if (this.isActive()) {
            var statusJson = await this.callUrl('/grab?filename=' + filename)

            status = statusJson['status'];
            if (status != 'ok'){
                TST_ASSERT_FAIL_FATAL("Error calling Capture: " + status + " : " + statusJson['errorCode'])
            }
        }
        else {
            console.log("VideoCapture Grab: Not Active");
        }

        console.log("VideoCapture Grab EXIT: (status = " + status + ")");
    }
    // Stop a video Capture Session
    async Stop()
    {
        console.log("VideoCapture Stop ENTER");

        if (this.isActive()) {
            var statusJson = await this.callUrl('/stop');

            status = statusJson['status'];
            if (status != 'ok'){
                TST_ASSERT_FAIL_FATAL("Error calling Stop: " + status + " : " + statusJson['errorCode'])
            }
        }
        else {
            console.log("VideoCapture Stop: Not Active");
        }

        console.log("VideoCapture Stop EXIT: (status = " + status + ")");
    }


    // Produce Debug Frames for a video Capture Session
    async Dump(filename, frameNo)
    {
        console.log("VideoCapture Dump ENTER: filename=" + filename + ", frameNo=" + frameNo);

        if (this.isActive()) {
            var statusJson = await this.callUrl('/dump?filename=' + filename + '&frameNo=' + frameNo);

            status = statusJson['status'];
            if (status != 'ok'){
                TST_ASSERT_FAIL_FATAL("Error calling Dump: " + status + " : " + statusJson['errorCode'])
            }
        }
        else {
            console.log("VideoCapture Dump: Not Active");
        }

        console.log("VideoCapture Dump EXIT: (status = " + status + ")");
    }


    // Gets the video monitor session status
    async GetSessionStatus()
    {
        console.log("VideoCapture GetSessionStatus ENTER");

        var statusJson = await this.callUrl('/sessionStatus');

        // Convert to a string to aid debug
        const statusStr = JSON.stringify(statusJson);
        console.log("VideoCapture GetSessionStatus EXIT: (" + statusStr + ")");

        return statusJson
    }

    // Gets the video monitor capture status
    async GetCaptureStatus()
    {
        console.log("VideoCapture GetCaptureStatus ENTER");

        var statusJson = await this.callUrl('/captureStatus');

        // Convert to a string to aid debug
        const statusStr = JSON.stringify(statusJson);
        console.log("VideoCapture GetCaptureStatus EXIT: (" + statusStr + ")");

        return statusJson
    }

    // Signals if the VideoCapture is Init
    async IsInit()
    {
        var statusJson = await this.GetSessionStatus();
        var isInit = statusJson['init']

        console.log("VideoCapture IsInit EXIT: (" + isInit + ")");
        return isInit
    }

    // Blocking function which waits for the video monitor to be Init before returning
    async WaitForInit(timeout = 5)
    {
        console.log("WaitForInit");

        var isInit = await this.IsInit();
        let i = 0;

        while ((isInit == false) && (i <= timeout))
        {
            await new Promise(resolve => setTimeout(resolve, 1000));
            isInit = await this.IsInit();
            i += 1;
        }

        console.log("VideoCapture WaitForInit EXIT: (" + isInit + ")");
        return isInit
    }

    // Signals if the VideoCapture is receiving Video Frames
    async IsVideoPresent()
    {
        var statusJson = await this.GetSessionStatus();
        var isVideoPresent = statusJson['videoPresent']

        console.log("VideoCapture isVideoPresent EXIT: (" + isVideoPresent + ")");
        return isVideoPresent
    }

    // Blocking function which waits for the video monitor to be signal video is present before returning
    async WaitForVideoPresent(timeout = 5)
    {
        console.log("WaitForVideoPresent");

        var isVideoPresent = await this.IsVideoPresent();
        let i = 0;

        while ((isVideoPresent == false) && (i <= timeout))
        {
            await new Promise(resolve => setTimeout(resolve, 1000));
            isVideoPresent = await this.IsVideoPresent();
            i += 1;
        }

        console.log("VideoCapture WaitForVideoPresent EXIT: (" + isVideoPresent + ")");
        return isVideoPresent
    }

    // Signals if the VideoCapture is Capturing
    async IsCapturing()
    {
        var statusJson = await this.GetCaptureStatus();
        var isCapturing = statusJson['capturing']

        console.log("VideoCapture IsCapturing EXIT: (" + isCapturing + ")");
        return isCapturing
    }

    // Blocking function which waits for the video monitor to be Capturing before returning
    async WaitForCapturing(timeout = 5)
    {
        console.log("WaitForCapturing");

        var isCapturing = await this.IsCapturing();
        let i = 0;

        while ((isCapturing == false) && (i <= timeout))
        {
            await new Promise(resolve => setTimeout(resolve, 1000));
            isCapturing = await this.IsCapturing();
            i += 1;
        }

        console.log("VideoCapture WaitForInit EXIT: (" + isCapturing + ")");
        return isCapturing
    }

    async GetBlankingStatus()
    {
        var statusJson = await this.GetCaptureStatus();

        // Number of Blank periods
        var blankingNum = statusJson['blankingCount'].length

        // Last Blank period
        var blankingStart = statusJson['blankingStart'].slice(-1)
        var blankingEnd   = statusJson['blankingEnd'].slice(-1)
        var blankingCount = statusJson['blankingCount'].slice(-1)
        var blankingTime  = statusJson['blankingTime'].slice(-1)

        console.log("VideoCapture GetBlankingStatus EXIT: (" + blankingNum + ", " + blankingStart + ", " + blankingEnd + ", " + blankingCount + ", " + blankingTime + ")");
        return ([blankingNum, blankingTime, blankingStart, blankingEnd, blankingCount])
    }
};
