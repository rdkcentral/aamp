The ABR proxy is designed to work in a container in the same docker compose project as a unit under test and simlinear (see [RDK-34251] < Automated ABR Testing>) , and as such it only supports a single client at a time.

For test purposes it is possible to bring the proxy up by itself, and use it as a proxy either routing external or simlinear traffic through it. When doing this the client may see "untrusted" certificates for HTTPS due to the way MITM attacks work. To work round this issue a .pem signing certificate can be copied into the docker image (see Dockerfile) which the client can then be told is trusted.



To build the ABR proxy docker image:
sudo docker build --build-arg MITMPROXY_WHEEL=mitmproxy-9.0.1-py3-none-any.whl . -t abr-test-proxy

The wheel installation distributions can be found under https://mitmproxy.org/downloads


To launch the docker compose abr proxy with simlinear container:
sudo docker compose -p abr-proxy-sl -f docker-compose-abr-proxy-sl.yml --env-file env_file.txt   up --abort-on-container-exit  --remove-orphans
NOTE:
The env_file.txt contains the environment definition of the path to the test stream to be served by simlinear
If the proxy is required without simlinear the simlinear service can be commented out of the yml file.


Commands can be sent to the proxy via the web console http://proxy_ip:8081, proxy_ctrl.py wrapper,
or by using curl. when using curl a session needs to be created to obtain the _xsrf token which is then used in
further requests for security reasons

Create a session and store the cookies:
curl -c cookies.txt -b cookies.txt -X GET http:/proxy_ip:8081

Sending a command to the proxy is done via the mitmproxy command interface with the arguments packaged in a json format
commands:
traffic.setrate dev client_ip rate
    Set the rate of requested content going to the client (specified by device (i.e eth0) and ip address) to a max rate specified in K Bytes / s
traffic.clearrate dev
    Remove any throttling currently active on the specified interface device
rule.errorreply token rule err_code ntimes
    Set a rule where if the request matches rule instead of servicing the request the response code is returned. The rules use the python re pattern matching.
rule.remove token
    Remove the rule that was added with token

examples...
Create an env var for the _xsrf token
export XSRFToken=$(grep _xsrf cookies.txt | awk '{ print $NF }')

Set the max rate of traffic going back to client whose ip address is 1.2.3.4 to 500 K bits / s
curl -c cookies.txt -b cookies.txt -X POST -d '{"arguments":["eth0","1.2.3.4","500"]}' http://proxy_ip:8081/commands/traffic.setrate -H 'Content-Type: application/json' -H "X-XSRFToken: $XSRFToken"

Add a rule to return 404 if the url requested matches the pattern (apply rule just once)
curl  -c cookies.txt -b cookies.txt -X POST -d '{"arguments":["token_1",".*video.*_29.ts","404","1"]}' http://proxy_ip:8081/commands/rules.errorreply -H 'Content-Type: application/json' -H "X-XSRFToken: $XSRFToken"

Remove the rule which was added with token_1
curl  -c cookies.txt -b cookies.txt -X POST -d '{"arguments":["token_1"]}' http://proxy_ip:8081/commands/rules.remove -H 'Content-Type: application/json' -H "X-XSRFToken: $XSRFToken"

