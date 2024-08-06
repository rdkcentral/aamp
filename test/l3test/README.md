# How to run L3 tests on HW platforms within a widget container. Alternatively these could be run using RDKShell.

## To run each test individually

```
pip3 install psutil
cd aamp/test/l3test
```

```
python3 run_l3_aamp.py --port <SSH Port of the connected RDK> --ip <IP address of the connected RDK device>
```

If you are doing automation - for example daily Jenkins job, it is recommended to run the command above with a timeout value.

# Results will be availabel in l3_report.json and l3_report.xml.
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

# Stop the widget and then run another test - eg 2001:
curl 'http://127.0.0.1:9001/as/apps/action/close?appId=com.aamp' -X POST -d '{}'
curl 'http://127.0.0.1:9001/as/apps/action/launch?appId=com.aamp' -X POST -d '{"url":"'"http://<IP_ADDR>:<PORT>/TST_2001_UVE_AampMainAndAdPlayback.html"'"}'


# TST_3000: CITestApp - can be run as follows from the test server:
curl 'http://127.0.0.1:9001/as/apps/action/launch?appId=com.aamp' -X POST -d '{"url":"'"https://cpetestutility.stb.r53.xcal.tv/aamptest/testApps/CITestApp/index.html"'"}'
