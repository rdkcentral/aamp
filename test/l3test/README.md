# How to run L3 tests on HW platforms within a widget container. Alternatively these could be run using RDKShell.

Make sure that the host device (PC/Laptop) and RDK device are on the same network and subnet.

## To run all the tests

```
pip3 install psutil
cd aamp/test/l3test
```

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device>
```

## Test options

### Run individual/group of tests

Add a -t option:

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device> -t <test numbers>
```

eg:

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device> -t 2000
```
This will only run the 2000 test suit

or 

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device> -t 2000 2005 3000
```
This will run 2000, 2005 and 3000 test suits

###Run AAMP on rialto
To run AAMP on rialto, use -r option
```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device> -r
```

### Run all tests in a testsuit despite one failure:
By default if one test fails inside a testsuit, the testsuit is not be further executed and ends.
To attempt to run subsequent tests despite one failure:

Add a -d flag:

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device> -d
```

eg to run 2007 and 3000 testsuites with -d flag:

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device> -t 2007 3000 -d
```


If you are doing automation - for example daily Jenkins job, it is recommended to run these commands with a timeout value.

# Results will be available in l3_report.json and l3_report.xml.
# AAMP logs for each test can be found in /l3test/AAMP_Logs/ directory.

## To run each test individually

# Start a webserver on an available port : eg. 8000 - on local windows laptop <IP_ADDR>:<PORT>
cd C:\aamp\test\l3test\
python -m http.server 8000 

# Download widget to /tmp on the STB
curl -o /tmp/com.aamp.wgt https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/public/aamptest/widgets/com.aamp.wgt

# Install the widget on the STB
appsservicectl set-test-preference forceallappslaunchable true
appsservicectl set-test-preference softcatdisableremoveapps true
curl -X POST --header "Content-Type:application/zip" --data-binary @/tmp/com.aamp.wgt 127.0.0.1:8090/pm/install?sign=auto

# Run an individual test inside the widget - eg, Run test 2000:
curl 'http://127.0.0.1:9001/as/apps/action/launch?appId=com.aamp' -X POST -d '{"url":"'"http://<IP_ADDR>:<PORT>/TST_2000_UVE_AampPlayback.html"'"}'

# Run an individual Simlinear test inside the widget - eg, Run test 2002:
- Download content, unzip and replace host ip, simlinear port for the test using script:
    ```
    ./simlinear.sh <content_url> <HOST_IP_ADDR> <simlinear_port> <TST_UVE_UTILS.js (or file containing related simlinearBaseUrl)>
    ```
    cd into content
    start simlinear

    Run rest of the test like any other test

# Stop the widget and then run another test - eg 2001:
curl 'http://127.0.0.1:9001/as/apps/action/close?appId=com.aamp' -X POST -d '{}'
curl 'http://127.0.0.1:9001/as/apps/action/launch?appId=com.aamp' -X POST -d '{"url":"'"http://<IP_ADDR>:<PORT>/TST_2001_UVE_AampMainAndAdPlayback.html"'"}'


# TST_3000: CITestApp - can be run as follows from the test server:
curl 'http://127.0.0.1:9001/as/apps/action/launch?appId=com.aamp' -X POST -d '{"url":"'"https://cpetestutility.stb.r53.xcal.tv/aamptest/testApps/CITestApp/index.html"'"}'

# How to create tests with Simlinear:
- Add required configurations in simlinearConfig.json
  "contentUrl" - URL of the content to download
  "contentType" - "hls" or "dash"
  "contentDirectory" - Directory name of content after download
  "testFile" - File containing simlinearContentUrl for the test. For example - for test TST_2002_UVE_SimlinearMainAndAdPlayBack.html, it would be TST_UVE_utils.js

- Test file should have either "simlinear" or "Simlinear" word in it for the python script to identify the file.