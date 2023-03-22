To run the automated tests it is assumed the aamp-builder image is available, as it is already available in Jenkins.

The sample tests run a python expects script to verify the proxy behaviour regarding throttling the bandwidth and error injection.

To run the tests:
copy _dockerrun.sh into the aamp directory of the build to be tested as per the AAMP_PATH. This will be executed on starting the UUT container
docker compose -p abr-proxy-abr-automated-test  --env-file env_file.txt   up --abort-on-container-exit  --remove-orphans
